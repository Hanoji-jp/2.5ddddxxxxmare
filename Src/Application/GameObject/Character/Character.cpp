#include "../../../Pch.h"
#include "Character.h"
#include "../Gimmick/MovingFloor.h"
#include "../Gimmick/WindBox.h"
#include "../Gimmick/SpikeBox.h"

void Character::Update()
{
    // フレーム開始時にキャッシュを無効化（このフレームの最初の ComputeGravityInfluence で計算）
    m_gravCacheDirty = true;
    ApplyGravity();
}

void Character::PostUpdate()
{
    // 重力切り替え時の押し出しをLerpで滑らかに適用
    bool isEjecting = false;
    if (m_ejectRemaining.LengthSquared() > 0.0001f)
    {
        isEjecting = true;
        const float kEjectLerp = std::min(0.3f * KdFPSController::GetDt() * 60.0f, 1.0f);
        const Math::Vector3 step = m_ejectRemaining * kEjectLerp;
        SetPos(GetPos() + step);
        m_ejectRemaining -= step;

        if (m_ejectRemaining.LengthSquared() < 0.001f)
        {
            m_ejectRemaining = { 0.0f, 0.0f, 0.0f };
            isEjecting = false;
        }
    }

    if (!isEjecting)
    {
        // フレームログ初期化
        m_currentFrameLog           = {};
        m_currentFrameLog.pos       = GetPos();
        m_currentFrameLog.velocity  = m_velocity;
        m_currentFrameLog.upDir     = m_upDir;

        // ---- 乗り床追従：前フレームに乗っていた床の移動分を「先に」反映する ----
        // 運んでから当たり判定を行うことで、床の移動で生じためり込みを
        // 同フレームの CheckWall / CheckGround の押し出しで解消できる。
        // （以前は判定後に運んでいたため、端で運ばれた分のめり込みが翌フレームまで残り、
        //   端ちょうどで止まるとめり込んで見えていた）
        if (m_pRidingFloor)
        {
            const auto spMF = m_pRidingFloor->lock();
            if (spMF)
            {
                SetPos(GetPos() + spMF->GetDeltaMove());
            }
            else
            {
                m_pRidingFloor = nullptr;
            }
        }

        // 分軸処理：横→壁判定→縦→床・天井判定
        m_preMovePos = GetPos();  // 水平移動前の位置を記録（移動床 XZ 判定に使う）

        ApplyVelocityHorizontal();
        CheckWall();
        ApplyVelocityVertical();
        CheckCeiling();
        CheckGround();   // ここで m_pRidingFloor を更新（次フレームの追従に使う）

        // 本体めり込み解決（最後の保証）：球カプセル vs 地形を最近接点で押し出す。
        // 角でも斜めに押し出されるため、レイで取りこぼした端めり込みもここで必ず解消される。
        ResolvePenetration();

        m_currentFrameLog.isGround = m_isGround;

        // リングバッファに記録
        if (m_debugLogEnabled)
        {
            if (static_cast<int>(m_collisionLog.size()) < kDebugLogFrames)
                m_collisionLog.resize(kDebugLogFrames);
            m_collisionLog[m_collisionLogIdx] = m_currentFrameLog;
            m_collisionLogIdx = (m_collisionLogIdx + 1) % kDebugLogFrames;
        }
    }
    else
    {
        ApplyVelocityHorizontal();
        ApplyVelocityVertical();
    }
}

void Character::DrawDebug()
{
    if (m_pDebugWire) { m_pDebugWire->Draw(); }
}

// 現在惑星をインデックスから取り直す（生ポインタをキャッシュしない＝再確保で壊れない）
const PlanetData* Character::CurrentPlanet() const
{
    return PlanetGravityManager::Instance().GetPlanet(m_currentPlanetIndex);
}

void Character::ApplyGravity()
{
    // ComputeGravityInfluence はフレームに1回だけ計算してキャッシュする
    if (m_gravCacheDirty)
    {
        m_gravCache      = PlanetGravityManager::Instance().ComputeGravityInfluence(GetPos(), -m_upDir);
        m_gravCacheDirty = false;
    }
    const GravityInfluenceResult gravResult = m_gravCache;
    const float dt60 = KdFPSController::GetDt() * 60.0f;
    // 重力回転（upDir）のSlerpをフレームレート非依存に：60fpsで UpDirSlerpSpeed に一致する
    // 指数補間係数。高FPSで回転が速くなるのを防ぐ。
    const float upSlerp = 1.0f - std::powf(1.0f - PlanetConst::UpDirSlerpSpeed, dt60);
    m_prevPlanetIndex = m_currentPlanetIndex;
    // 着地中は現在の惑星を固定。空中のときだけ支配惑星に乗り換える
    // （着地中に隣Boxへ乗り換えると、その下に地面が無く isGround を失うため）
    if (!m_isGround)
    {
        m_currentPlanetIndex = gravResult.dominantPlanetIdx;
    }

    // ── 2.5D: 現在(支配)BoxのZ範囲外へ出たら、惑星/ゾーンに関係なくワールド下向き(-Y)で落とす ──
    //   Z端を越えると接地は外れる(isGround=false)が、Boxの重力影響(hasInfluence)が残ると
    //   「下」がBox方向(Z成分入り)になり、さらに Move() が m_upDir 軸以外のY速度を毎フレーム
    //   捨てるため velY が0のまま＝落ちない。ここで up を +Y に固定して -Y 重力を加え、必ず落とす。
    if (!m_ignoreGravityZones && !m_isGround && m_manualGravityDir == ManualGravityDir::None
        && CurrentPlanet() && CurrentPlanet()->Shape == PlanetShape::Box
        && std::abs(GetPos().z - CurrentPlanet()->Position.z)
           > CurrentPlanet()->BoxHalfExtents.z + CollisionConst::GroundSampleRadius)
    {
        constexpr Math::Vector3 kFallGravDir = { 0.0f, -1.0f, 0.0f };
        constexpr Math::Vector3 kFallUp      = { 0.0f,  1.0f, 0.0f };

        m_upDir = kFallUp;
        const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
        const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, kFallUp);
        m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
            Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, upSlerp)));
        m_upDirVisual.Normalize();

        const float radialVel = m_velocity.Dot(kFallGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale * dt60,
                                         PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += kFallGravDir * (newRadial - radialVel); }
        return;
    }

    // 惑星重力の抑制は「実際にManualGravityゾーン内」のときだけ。
    // CanUseManualGravity はゾーン無し＝どこでもtrueを返すので、抑制判定には使わない
    const bool inManualZone = !m_ignoreGravityZones
        && ManualGravityZoneManager::Instance().IsInsideManualZone(GetPos());
    const bool hasManual    = (m_manualGravityDir != ManualGravityDir::None);

    // ── ケース① ゾーン外 + 手動あり + 惑星圏内 → 惑星に捕まる、手動リセット ──
    // ただし m_ignoreGravityZones（Enemy）の場合はリセットしない
    if (!m_ignoreGravityZones && !inManualZone && hasManual && gravResult.hasInfluence)
    {
        m_manualGravityDir = ManualGravityDir::None;
        // fall through → 惑星重力ブロックで処理
    }

    // ── ケース②③ 手動重力モード（ゾーン内 or ゾーン外惑星圏外）──
    if (m_manualGravityDir != ManualGravityDir::None)
    {
        Math::Vector3 gravDir  = { 0.0f, -1.0f, 0.0f };
        Math::Vector3 targetUp = { 0.0f,  1.0f, 0.0f };
        switch (m_manualGravityDir)
        {
        case ManualGravityDir::Down: gravDir = { 0.0f, -1.0f, 0.0f }; targetUp = { 0.0f,  1.0f, 0.0f }; break;
        case ManualGravityDir::Up:   gravDir = { 0.0f,  1.0f, 0.0f }; targetUp = { 0.0f, -1.0f, 0.0f }; break;
        default: break;
        }

        m_upDir = targetUp;
        const Math::Quaternion fromQ  = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
        const Math::Quaternion toQ    = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
        m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
            Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, upSlerp)));
        m_upDirVisual.Normalize();

        if (m_isGround) { return; }  // velocity 加算のみスキップ、Slerp は上で完了済み

        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale * dt60, PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += gravDir * (newRadial - radialVel); }
        return;
    }
    Math::Vector3 zoneGravDir;
    const bool inNormalZone = !m_ignoreGravityZones
        && ManualGravityZoneManager::Instance().IsInNormalGravityZone(GetPos(), zoneGravDir);
    // 惑星引力を無効にするのはどちらのゾーン内でも常にオフ
    const bool suppressPlanet = inNormalZone || inManualZone;

    // NormalBox着地中は外部惑星重力を一切受け付けない（非NormalはComputeGravityInfluenceに任せる）
    const bool onNormalBoxGround = m_isGround
        && CurrentPlanet()
        && CurrentPlanet()->Shape == PlanetShape::Box
        && CurrentPlanet()->bNormalGravity;

    if (gravResult.hasInfluence && !suppressPlanet)
    {
        if (onNormalBoxGround)
        {
            // NormalBox着地中：upDir を Y+ に固定、velocity は加算しない
            // Slerp は着地中も継続（途中着地で傾いたまま止まるのを防ぐ）
            constexpr Math::Vector3 kBoxUp = { 0.0f, 1.0f, 0.0f };
            m_upDir = kBoxUp;
            const Math::Quaternion fromQv = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQv   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQv, toQv, upSlerp)));
            m_upDirVisual.Normalize();
            return;
        }

        // 着地中に「今立っている惑星」以外が支配的でも、向き・地面を維持する。
        // （L字入隅で隣の壁Boxが支配的になっても、床に立っている間は傾けず・落とさない）
        const bool onDominantPlanet = (gravResult.dominantPlanetIdx == m_currentPlanetIndex);
        if (m_isGround && !onDominantPlanet) { return; }

        // 支配惑星の上方向を使う（合成だと L字入隅で対角45°に挟まるため）。
        const Math::Vector3 targetUp = gravResult.dominantUpDir;
        const Math::Vector3 gravDir  = -targetUp;

        // m_upDir は着地中も常に更新（球面上で向きを合わせ続ける必要がある）
        m_upDir = targetUp;
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, upSlerp)));
            m_upDirVisual.Normalize();
        }

        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * dt60, PlanetConst::MaxFallSpeed);
        if (newRadial > radialVel) { m_velocity += gravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑤
    if (inNormalZone)
    {
        const Math::Vector3 targetUp = -zoneGravDir;

        // m_upDir は着地中も常に更新
        m_upDir = targetUp;
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, upSlerp)));
            m_upDirVisual.Normalize();
        }

        if (m_isGround) { return; }  // velocity 加算のみスキップ、Slerp は上で完了済み
        const float radialVel = m_velocity.Dot(zoneGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale * dt60, PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += zoneGravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑥
    if (inManualZone)
    {
        constexpr Math::Vector3 kDefaultGravDir = { 0.0f, -1.0f, 0.0f };
        constexpr Math::Vector3 kDefaultUp      = { 0.0f,  1.0f, 0.0f };
        m_upDir = kDefaultUp;
        const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
        const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, kDefaultUp);
        m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
            Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, upSlerp)));
        m_upDirVisual.Normalize();
        if (m_isGround) { return; }  // velocity 加算のみスキップ、Slerp は上で完了済み
        const float radialVel = m_velocity.Dot(kDefaultGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale * dt60, PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += kDefaultGravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑦：重力源が一切ない場所（惑星圏外＋ゾーン外）。
    //   従来は無重力で浮いてしまっていた（箱の端からはみ出すと落ちない原因）。
    //   ワールド下向きの既定重力を加算して、ちゃんと落下させる。
    {
        constexpr Math::Vector3 kFallGravDir = { 0.0f, -1.0f, 0.0f };
        constexpr Math::Vector3 kFallUp      = { 0.0f,  1.0f, 0.0f };

        m_upDir = kFallUp;
        const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
        const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, kFallUp);
        m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
            Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, upSlerp)));
        m_upDirVisual.Normalize();

        if (m_isGround) { return; }   // 念のため：接地中は加算しない

        const float radialVel = m_velocity.Dot(kFallGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale * dt60,
                                         PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += kFallGravDir * (newRadial - radialVel); }
    }
}

void Character::ApplyVelocity()
{
    Math::Vector3 pos = GetPos();
    pos += m_velocity;
    SetPos(pos);
}

void Character::ApplyVelocityHorizontal()
{
    const float dt60 = KdFPSController::GetDt() * 60.0f;
    // m_upDir軸垂直面への投影（水平成分のみ適用）
    const Math::Vector3 vertComponent = m_upDir * m_velocity.Dot(m_upDir);
    const Math::Vector3 horizVelocity = m_velocity - vertComponent;
    Math::Vector3 pos = GetPos();
    pos += horizVelocity * dt60;
    SetPos(pos);
}

void Character::ApplyVelocityVertical()
{
    const float dt60 = KdFPSController::GetDt() * 60.0f;
    // m_upDir軸方向の成分のみ適用
    const Math::Vector3 vertVelocity = m_upDir * m_velocity.Dot(m_upDir);
    Math::Vector3 pos = GetPos();
    pos += vertVelocity * dt60;
    SetPos(pos);
}

void Character::CheckCeiling()
{
    // 上方向（m_upDir）へレイを飛ばし、Box底面（下向き法線）にヒットしたら押し下げる
    // ジャンプ中（上向き速度あり）のみ判定
    if (m_velocity.Dot(m_upDir) <= 0.001f) { return; }

    const Math::Vector3 pos      = GetPos();
    const float         rayLen   = CollisionConst::CeilingRayOffset + CollisionConst::CeilingSnapDist;
    const Math::Vector3 rayStart = pos + m_upDir * CollisionConst::CeilingRayOffset;
    const Math::Vector3 rayDir   = m_upDir;

    const KdCollider::RayInfo ray(KdCollider::TypeBump, rayStart, rayDir, rayLen);

    // ---- Box 惑星
    const auto& planets = PlanetGravityManager::Instance().GetPlanets();
    for (const auto& p : planets)
    {
        if (p.Shape != PlanetShape::Box || !p.pCollider) { continue; }

        std::list<KdCollider::CollisionResult> results;
        if (!p.pCollider->Intersects(ray, p.mWorld, &results)) { continue; }

        for (const auto& r : results)
        {
            // 下向き法線（天井・底面）だけを対象にする
            if (r.m_hitNDir.Dot(m_upDir) > -0.7f) { continue; }

            // 当たった距離 = rayLen - overlapDistance
            const float hitDist = rayLen - r.m_overlapDistance;

            // 天井にめり込んでいる分だけ押し下げる
            const float penetration = (CollisionConst::CeilingRayOffset - hitDist);
            if (penetration > 0.0f)
            {
                SetPos(GetPos() - m_upDir * penetration);
            }

            // 上向き速度をカット
            const float velUp = m_velocity.Dot(m_upDir);
            if (velUp > 0.0f)
                m_velocity -= m_upDir * velUp;

            return; // 最初にヒットした天井で処理終了
        }
    }

    // ---- 通常マップ
    const auto spMap = m_wpMap.lock();
    if (spMap)
    {
        std::list<KdCollider::CollisionResult> results;
        if (spMap->Intersects(ray, &results))
        {
            for (const auto& r : results)
            {
                if (r.m_hitNDir.Dot(m_upDir) > -0.7f) { continue; }

                const float hitDist     = rayLen - r.m_overlapDistance;
                const float penetration = CollisionConst::CeilingRayOffset - hitDist;
                if (penetration > 0.0f)
                    SetPos(GetPos() - m_upDir * penetration);

                const float velUp = m_velocity.Dot(m_upDir);
                if (velUp > 0.0f)
                    m_velocity -= m_upDir * velUp;

                return;
            }
        }
    }

    // ---- WindBox 底面判定（下から飛び込んだ時の天井扱い）
    for (auto& wpWB : m_windBoxColliders)
    {
        const auto spWB = wpWB.lock();
        if (!spWB) { continue; }
        const auto spWBBox = std::dynamic_pointer_cast<WindBox>(spWB);
        if (!spWBBox || !spWBBox->IsEnabled()) { continue; }
        const KdCollider* col = spWBBox->GetCollider();
        if (!col) { continue; }

        std::list<KdCollider::CollisionResult> wbResults;
        if (!col->Intersects(ray, spWBBox->GetWorldMatrix(), &wbResults)) { continue; }

        for (const auto& r : wbResults)
        {
            if (r.m_hitNDir.Dot(m_upDir) > -0.7f) { continue; }

            const float hitDist     = rayLen - r.m_overlapDistance;
            const float penetration = CollisionConst::CeilingRayOffset - hitDist;
            if (penetration > 0.0f)
                SetPos(GetPos() - m_upDir * penetration);

            const float velUp = m_velocity.Dot(m_upDir);
            if (velUp > 0.0f)
                m_velocity -= m_upDir * velUp;

            return;
        }
    }
}

void Character::CheckGround()
{
    m_isGround     = false;
    m_pRidingFloor = nullptr;

    const Math::Vector3 pos = GetPos();

    // 手動重力 Up（天井歩き）の場合、ワールド下向き固定の地面スナップ
    // （移動床の上面 Y 判定 / 球 NormalGravity の下向きレイ / 通常マップの下向きレイ）は
    // キャラを下の床に毎フレーム張り付けて上昇を妨げてしまう。
    // 天井（Box 下面）への着地は下の Box 惑星セクションが m_upDir 基準で正しく処理するため、
    // 下向き固定スナップ経路はスキップする。
    // Player はキック付き SetManualGravity で即スナップ範囲を脱出するので影響を受けないが、
    // 念のため敵（m_ignoreGravityZones）専用に限定して Player の挙動を保護する。
    const bool skipDownwardSnap =
        (m_manualGravityDir == ManualGravityDir::Up) && m_ignoreGravityZones;

    // ---- ★ 移動床（Box惑星と同じレイキャスト方式）----
    // AABB式の範囲チェックは使わず、-m_upDir 方向へ TypeGround レイを撃って上面へスナップ。
    // 早期 return より前に判定することで惑星上でも必ず拾う。
    if (!skipDownwardSnap && m_velocity.Dot(m_upDir) <= 0.1f)
    {
        // 体の幅ぶん外側にもレイを撒く（端で中心レイが外れて沈むのを防ぐ）
        const Math::Vector3 worldFwdMF = (std::abs(m_upDir.z) < 0.9f)
            ? Math::Vector3{ 0.0f, 0.0f, 1.0f } : Math::Vector3{ 1.0f, 0.0f, 0.0f };
        Math::Vector3 rightAxisMF; m_upDir.Cross(worldFwdMF, rightAxisMF); rightAxisMF.Normalize();

        // ★ 奥行き(Z/fwd)方向のサンプルは撒かない＝プレイヤーの実Zで接地判定する。
        //   これにより移動床のZ端からはみ出すと接地が外れて落下する（運ばれている間は乗る）。
        //   横(right)方向だけは端で沈まないよう体幅ぶんサンプルを残す。
        const float srMF = CollisionConst::GroundSampleRadius;
        const Math::Vector3 sampleOffsetsMF[3] = {
            { 0.0f, 0.0f, 0.0f },
            rightAxisMF * srMF, rightAxisMF * -srMF,
        };

        for (auto& wpMF : m_movingFloors)
        {
            const auto spMF = wpMF.lock();
            if (!spMF) { continue; }

            const KdCollider* col = spMF->GetCollider();
            if (!col) { continue; }

            // ★ 2.5Dの奥行き(Z)ゲート：床のZ範囲外なら接地しない＝落下（Box惑星と同じ）。
            if (std::abs(pos.z - spMF->GetPos().z) > spMF->GetHalfExtents().z + srMF)
            {
                continue;
            }

            for (const Math::Vector3& off : sampleOffsetsMF)
            {
                const Math::Vector3 rayStart = pos + off + m_upDir * CollisionConst::GroundRayOffset;
                const KdCollider::RayInfo ray(
                    KdCollider::TypeGround, rayStart, -m_upDir, CollisionConst::GroundRayLength);

                std::list<KdCollider::CollisionResult> results;
                if (!col->Intersects(ray, spMF->GetWorldMatrix(), &results)) { continue; }

                // 立っている面（法線が m_upDir と揃う面）の最も手前のヒットを採用
                const KdCollider::CollisionResult* pBest = nullptr;
                for (const auto& r : results)
                {
                    if (r.m_hitNDir.Dot(m_upDir) < 0.7f) { continue; }
                    if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance) { pBest = &r; }
                }
                if (!pBest) { continue; }

                // 上面はキャップ外面（Box面 + capThickness*2）を着地面とする
                const float capSurface    = PlanetConst::GrassCapThickness * 2.0f;
                const float distToSurface = (pos - pBest->m_hitPos).Dot(m_upDir) - capSurface;

                if (distToSurface >  CollisionConst::GroundSnapDist) { continue; }
                if (distToSurface < -CollisionConst::GroundSnapDist) { continue; }

                SetPos(pos - m_upDir * distToSurface);

                // 沈み込み方向の速度成分を除去
                const float normalComp = m_velocity.Dot(m_upDir);
                if (normalComp < 0.0f) { m_velocity -= m_upDir * normalComp; }

                m_isGround = true;
                m_airGravitySwitchCount = 0;
                m_pRidingFloor = &wpMF;
                return;
            }
        }
    }

    // ---- ★ 風ボックス上面（TypeGround レイキャスト）----
    // 天井歩き（ManualUp）は上面に張り付いてしまうのでスキップ
    if (!skipDownwardSnap && m_velocity.Dot(m_upDir) <= 0.1f)
    {
        const Math::Vector3 rayStart = pos + m_upDir * CollisionConst::GroundRayOffset;
        const KdCollider::RayInfo ray(
            KdCollider::TypeGround,
            rayStart,
            -m_upDir,
            CollisionConst::GroundRayLength);

        for (auto& wpWB : m_windBoxColliders)
        {
            const auto spWB = wpWB.lock();
            if (!spWB) { continue; }

            // WindBox 固有の Collider / WorldMatrix を使って判定
            const auto spWBBox = std::dynamic_pointer_cast<WindBox>(spWB);
            if (!spWBBox || !spWBBox->IsEnabled()) { continue; }
            const KdCollider* col = spWBBox->GetCollider();
            if (!col) { continue; }

            std::list<KdCollider::CollisionResult> results;
            if (!col->Intersects(ray, spWBBox->GetWorldMatrix(), &results)) { continue; }

            const KdCollider::CollisionResult* pBest = nullptr;
            for (const auto& r : results)
            {
                if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance)
                    pBest = &r;
            }
            if (!pBest) { continue; }

            // 上面にスナップ
            const float penetration = pBest->m_hitPos.Dot(m_upDir) - pos.Dot(m_upDir);
            if (penetration > -CollisionConst::GroundSnapDist && penetration <= CollisionConst::GroundRayOffset)
            {
                Math::Vector3 corrected = GetPos();
                corrected += m_upDir * penetration;
                SetPos(corrected);

                const float downComp = m_velocity.Dot(-m_upDir);
                if (downComp > 0.0f) { m_velocity += m_upDir * downComp; }

                m_isGround = true;
                m_airGravitySwitchCount = 0;
                // 移動床にアタッチされた風ボックスなら、その床に乗っている扱いで一緒に運ぶ
                if (!spWBBox->GetAttachFloor().expired()) { m_pRidingFloor = &spWBBox->GetAttachFloor(); }
                return;
            }
        }
    }

    // ---- ★ 棘ボックス上面（移動床アタッチ時に一緒に運ぶ）----
    // COL を TypeGround レイで判定し、上面に乗っていれば接地＋riding設定。
    if (!skipDownwardSnap && m_velocity.Dot(m_upDir) <= 0.1f)
    {
        const Math::Vector3 rayStart = pos + m_upDir * CollisionConst::GroundRayOffset;
        const KdCollider::RayInfo ray(
            KdCollider::TypeGround,
            rayStart,
            -m_upDir,
            CollisionConst::GroundRayLength);

        for (auto& wpSB : m_spikeBoxColliders)
        {
            const auto spSB = wpSB.lock();
            if (!spSB) { continue; }
            const auto spSpike = std::dynamic_pointer_cast<SpikeBox>(spSB);
            if (!spSpike || !spSpike->IsEnabled()) { continue; }
            const KdCollider* col = spSpike->GetCollider();
            if (!col) { continue; }

            std::list<KdCollider::CollisionResult> results;
            if (!col->Intersects(ray, spSpike->GetWorldMatrix(), &results)) { continue; }

            const KdCollider::CollisionResult* pBest = nullptr;
            for (const auto& r : results)
            {
                if (r.m_hitNDir.Dot(m_upDir) < 0.7f) { continue; }   // 上向き面のみ
                if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance) { pBest = &r; }
            }
            if (!pBest) { continue; }

            // 上面にスナップ
            const float penetration = pBest->m_hitPos.Dot(m_upDir) - pos.Dot(m_upDir);
            if (penetration > -CollisionConst::GroundSnapDist && penetration <= CollisionConst::GroundRayOffset)
            {
                Math::Vector3 corrected = GetPos();
                corrected += m_upDir * penetration;
                SetPos(corrected);

                const float downComp = m_velocity.Dot(-m_upDir);
                if (downComp > 0.0f) { m_velocity += m_upDir * downComp; }

                m_isGround = true;
                m_airGravitySwitchCount = 0;
                // 移動床にアタッチされた棘なら、その床に乗っている扱いで一緒に運ぶ
                if (!spSpike->GetAttachFloor().expired()) { m_pRidingFloor = &spSpike->GetAttachFloor(); }
                return;
            }
        }
    }

    // ---- ① Box惑星（Normal・非Normal共通）── レイキャスト方式 ----
    // AABB式の範囲チェック（abs(lp.x) > half など）は使わない。立っている面の
    // 法線方向(-m_upDir)へ TypeGround レイを撃ち、ヒット面へスナップする。
    // これにより地面と壁(CheckWall)が同じ箱コライダーの縁を共有するため、
    // 端での「持ち上げ⇄押し出し」の食い違い（端めり込み）が起きない。
    if (CurrentPlanet()
        && CurrentPlanet()->Shape == PlanetShape::Box)
    {
        // ジャンプ直後（m_upDir方向に速度あり）はスキップ
        if (m_velocity.Dot(m_upDir) > 0.1f) { return; }

        // 体の幅ぶん外側にもレイを撒く（中心1本だと端で外れて沈むため）
        const Math::Vector3 worldFwd = (std::abs(m_upDir.z) < 0.9f)
            ? Math::Vector3{ 0.0f, 0.0f, 1.0f } : Math::Vector3{ 1.0f, 0.0f, 0.0f };
        Math::Vector3 rightAxis; m_upDir.Cross(worldFwd, rightAxis); rightAxis.Normalize();
        Math::Vector3 fwdAxis;   rightAxis.Cross(m_upDir, fwdAxis);  fwdAxis.Normalize();

        const float sr = CollisionConst::GroundSampleRadius;
        const Math::Vector3 sampleOffsets[5] = {
            { 0.0f, 0.0f, 0.0f },
            rightAxis * sr, rightAxis * -sr,
            fwdAxis   * sr, fwdAxis   * -sr,
        };

        // 足元のどの Box 惑星でも接地できるよう、全 Box 惑星を調べる。
        // （壁際で隣の壁Boxが dominant になっても、立っている床Boxを見落とさない）
        const auto& planets = PlanetGravityManager::Instance().GetPlanets();
        for (const auto& p : planets)
        {
            if (p.Shape != PlanetShape::Box || !p.pCollider) { continue; }

            // ★ 2.5Dの奥行き(Z)ゲート：プレイヤーがこのBoxのZ範囲外なら接地しない＝落下。
            //   接地レイ(メッシュ)のZ範囲が見た目(BoxHalfExtents.z)と食い違っても、
            //   ここを権威にしてZ端からのはみ出しで必ず落とす（横の端と同じ感覚）。
            if (std::abs(pos.z - p.Position.z) > p.BoxHalfExtents.z + CollisionConst::GroundSampleRadius)
            {
                continue;
            }

            for (const Math::Vector3& off : sampleOffsets)
            {
                const Math::Vector3 rayStart = pos + off + m_upDir * CollisionConst::GroundRayOffset;
                const KdCollider::RayInfo ray(
                    KdCollider::TypeGround, rayStart, -m_upDir, CollisionConst::GroundRayLength);

                std::list<KdCollider::CollisionResult> results;
                if (!p.pCollider->Intersects(ray, p.mWorld, &results)) { continue; }

                // 立っている面（法線が m_upDir と揃う面）の最も手前のヒットを採用。
                const KdCollider::CollisionResult* pBest = nullptr;
                for (const auto& r : results)
                {
                    if (r.m_hitNDir.Dot(m_upDir) < 0.7f) { continue; }
                    if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance) { pBest = &r; }
                }
                if (!pBest) { continue; }

                // ヒット面の種別（±X / ±Y）からキャップ有無を判定
                const Math::Vector3& n = pBest->m_hitNDir;
                bool hasCap = false;
                if      (n.y >  0.5f) { hasCap = true; }  // 上面：常にキャップ
                else if (n.y < -0.5f) { hasCap = !p.bNormalGravity && p.BoxFaceGravityBottom == BoxFaceGravityMode::Inward; }
                else if (n.x >  0.5f) { hasCap = !p.bNormalGravity && p.BoxFaceGravityRight  == BoxFaceGravityMode::Inward; }
                else if (n.x < -0.5f) { hasCap = !p.bNormalGravity && p.BoxFaceGravityLeft   == BoxFaceGravityMode::Inward; }

                const float capSurface = hasCap ? (PlanetConst::GrassCapThickness * 2.0f) : 0.0f;
                const float distToSurface = (pos - pBest->m_hitPos).Dot(m_upDir) - capSurface;

                if (distToSurface <=  CollisionConst::GroundSnapDist &&
                    distToSurface >= -CollisionConst::GroundSnapDist)
                {
                    SetPos(pos - m_upDir * distToSurface);

                    // 面法線（沈み込み）方向の速度成分を除去
                    const float normalComp = m_velocity.Dot(m_upDir);
                    if (normalComp < 0.0f) { m_velocity -= m_upDir * normalComp; }

                    // 実際に立っている Box を現在惑星にする（dominant が隣Boxでも上書き）
                    m_currentPlanetIndex = static_cast<int>(&p - &planets[0]);

                    m_isGround = true;
                    m_airGravitySwitchCount = 0;
                    return;
                }
            }
        }

        // 通常は Box 惑星が担当するので Sphere/マップには落ちない
        // 天井歩き（skipDownwardSnap）の場合は、マップ天井検出のために落ちる
        if (!skipDownwardSnap) { return; }
    }

    // ---- ②-A 球惑星（放射状）─ 中心距離ベースの解析判定（すり抜け防止）----
    // レイ＆メッシュに頼らず、中心からの距離で必ず押し出すのでトンネリングしない。
    if (!skipDownwardSnap
        && CurrentPlanet()
        && CurrentPlanet()->Shape == PlanetShape::Sphere
        && !CurrentPlanet()->bNormalGravity)
    {
        const PlanetData& p = *CurrentPlanet();
        const float dx = pos.x - p.Position.x;
        const float dy = pos.y - p.Position.y;
        const float d  = std::sqrtf(dx * dx + dy * dy);
        if (d < 0.001f) { return; }
        const Math::Vector3 outDir = { dx / d, dy / d, 0.0f };

        // ジャンプ直後（外向き速度）はスキップ
        if (m_velocity.Dot(outDir) > 0.001f) { return; }

        // 立つ半径（GroundRadius）に足を合わせる。表面より内側＝めり込みは必ず押し出す
        const float standDist = p.GroundRadius;
        if (d <= standDist + CollisionConst::GroundSnapDist)
        {
            Math::Vector3 corrected = p.Position + outDir * standDist;
            corrected.z = pos.z;   // 2.5D：Zは維持
            SetPos(corrected);

            // 内向き（落下）速度を消す
            const float inwardVel = m_velocity.Dot(-outDir);
            if (inwardVel > 0.0f) { m_velocity += outDir * inwardVel; }

            if (m_prevPlanetIndex != m_currentPlanetIndex)
            {
                const float radial = m_velocity.Dot(outDir);
                m_velocity = outDir * radial;
            }
            m_isGround = true;
            m_airGravitySwitchCount = 0;
        }
        return;
    }

    // ---- ② Sphere等の惑星 ─ レイキャスト方式 ----
    // 天井歩き（手動 Up）は下向きレイで床に張り付くためスキップ
    if (!skipDownwardSnap && CurrentPlanet() && CurrentPlanet()->pCollider)
    {
        // ジャンプ直後はスキップ
        if (m_velocity.Dot(m_upDir) > 0.001f) { return; }

        const Math::Vector3 center = CurrentPlanet()->Position;
        Math::Vector3 outDir;
        if (CurrentPlanet()->bNormalGravity)
        {
            outDir = { 0.0f, 1.0f, 0.0f };
        }
        else
        {
            const float dx = pos.x - center.x;
            const float dy = pos.y - center.y;
            const float d  = std::sqrtf(dx * dx + dy * dy);
            if (d < 0.001f) { return; }
            outDir = { dx / d, dy / d, 0.0f };
        }

        const float         rayLen   = CurrentPlanet()->GravityRadius + PlanetConst::PlanetRayOffset;
        const Math::Vector3 rayStart = pos + outDir * PlanetConst::PlanetRayOffset;
        const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, -outDir, rayLen);

        std::list<KdCollider::CollisionResult> results;
        if (!CurrentPlanet()->pCollider->Intersects(ray, CurrentPlanet()->mWorld, &results)) { return; }

        const KdCollider::CollisionResult* pBest = nullptr;
        for (const auto& r : results)
        {
            if (r.m_hitNDir.Dot(outDir) < 0.7f) { continue; }
            if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance) { pBest = &r; }
        }
        if (!pBest) { return; }

        const float diff = (pos - pBest->m_hitPos).Dot(outDir);
        if (diff < CollisionConst::GroundSnapDist)
        {
            Math::Vector3 corrected = pos;
            corrected.x = pBest->m_hitPos.x * std::abs(outDir.x) + pos.x * (1.0f - std::abs(outDir.x));
            corrected.y = pBest->m_hitPos.y * std::abs(outDir.y) + pos.y * (1.0f - std::abs(outDir.y));
            SetPos(corrected);

            const float inwardVel = m_velocity.Dot(-outDir);
            if (inwardVel > 0.0f) { m_velocity += outDir * inwardVel * GameConst::LandingDamping; }

            if (m_prevPlanetIndex != m_currentPlanetIndex)
            {
                const float radial = m_velocity.Dot(outDir);
                m_velocity = outDir * radial;
            }
            m_isGround = true;
            m_airGravitySwitchCount = 0;
        }
        return;
    }

    // ---- ★ 天井歩き専用（ManualUp + m_ignoreGravityZones）----
    // CurrentPlanet() 以外のすべての Box 惑星底面 + マップ天井を検出
    if (skipDownwardSnap && !m_isGround)
    {
        // 天井から離れる方向（ジャンプ直後）はスキップ
        if (m_velocity.Dot(m_upDir) <= 0.1f)
        {
            // ---- Box 惑星の底面を全走査 ----
            const auto& planets = PlanetGravityManager::Instance().GetPlanets();
            for (const auto& p : planets)
            {
                if (p.Shape != PlanetShape::Box) { continue; }

                const Math::Vector3 lp   = pos - p.Position;
                const Math::Vector3 half = p.BoxHalfExtents;

                // XZ 範囲チェック
                if (std::abs(lp.x) > half.x) { continue; }

                // 底面：dist = -lp.y - half.y（正=底面より下=外側）
                const float capExt   = 0.0f;  // NormalBox 底面にキャップなし
                const float dist     = -lp.y - half.y;
                const float dToSurf  = dist - capExt;

                if (dToSurf >  CollisionConst::GroundSnapDist) { continue; }
                if (dToSurf < -CollisionConst::GroundSnapDist) { continue; }

                // 底面にスナップ（f.normal={0,-1,0} → corrected.y += dToSurf）
                Math::Vector3 corrected = pos;
                corrected.y += dToSurf;
                SetPos(corrected);

                // 底面に向かう速度成分（+Y）を除去
                if (m_velocity.y > 0.0f)
                {
                    m_velocity.y *= GameConst::LandingDamping;
                    if (std::abs(m_velocity.y) < 0.001f) { m_velocity.y = 0.0f; }
                }

                m_isGround = true;
                m_airGravitySwitchCount = 0;
                return;
            }

            // ---- マップ天井（上方向レイ、下向き法線面のみ）----
            const auto spMap = m_wpMap.lock();
            if (spMap)
            {
                const Math::Vector3 rayStart = pos;
                const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart,
                    Math::Vector3(0.0f, 1.0f, 0.0f), CollisionConst::GroundRayLength);

                std::list<KdCollider::CollisionResult> results;
                if (spMap->Intersects(ray, &results))
                {
                    const KdCollider::CollisionResult* pBest = nullptr;
                    for (auto& r : results)
                    {
                        // 下向き法線（天井面）のみ受け付ける
                        if (r.m_hitNDir.y > -0.7f) { continue; }
                        // 自分より上にある面のみ
                        if (r.m_hitPos.y < pos.y - 0.01f) { continue; }
                        if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance) { pBest = &r; }
                    }

                    if (pBest)
                    {
                        const float dist = pBest->m_hitPos.y - pos.y;  // 正=天井は上
                        if (dist >= 0.0f && dist < CollisionConst::GroundSnapDist)
                        {
                            Math::Vector3 corrected = GetPos();
                            corrected.y = pBest->m_hitPos.y;
                            SetPos(corrected);

                            if (m_velocity.y > 0.0f)
                            {
                                m_velocity.y *= GameConst::LandingDamping;
                                if (std::abs(m_velocity.y) < 0.001f) { m_velocity.y = 0.0f; }
                            }
                            m_isGround = true;
                            m_airGravitySwitchCount = 0;
                        }
                    }
                }
            }
        }
        return;
    }

    // ---- ③ 通常マップ ----
    if (m_velocity.y > 0.001f) { return; }

    const auto spMap = m_wpMap.lock();
    if (!spMap) { return; }

    const Math::Vector3 rayStart = pos + Math::Vector3(0.0f, CollisionConst::GroundRayOffset, 0.0f);
    const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart,
        Math::Vector3(0.0f, -1.0f, 0.0f), CollisionConst::GroundRayLength);

    std::list<KdCollider::CollisionResult> results;
    if (!spMap->Intersects(ray, &results)) { return; }

    const KdCollider::CollisionResult* pBest = nullptr;
    for (auto& r : results)
    {
        if (r.m_hitPos.y > pos.y + 0.01f) { continue; }
        const float dx = r.m_hitPos.x - pos.x;
        const float dz = r.m_hitPos.z - pos.z;
        if (dx * dx + dz * dz > CollisionConst::GroundHitHorizontalMax * CollisionConst::GroundHitHorizontalMax) { continue; }
        if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance) { pBest = &r; }
    }

    if (pBest)
    {
        Math::Vector3 correctedPos = pos;
        const float penetration    = pBest->m_hitPos.y - pos.y;
        if (penetration > -CollisionConst::GroundSnapDist)
        {
            correctedPos.y = pBest->m_hitPos.y;
            SetPos(correctedPos);
            if (m_velocity.y < 0.0f)
            {
                m_velocity.y *= GameConst::LandingDamping;
                if (std::abs(m_velocity.y) < 0.001f) { m_velocity.y = 0.0f; }
            }
            m_isGround = true;
            m_airGravitySwitchCount = 0;
        }
    }
}

void Character::CheckWall()
{
    Math::Vector3 pos = GetPos();

    const Math::Vector3 worldFwd = (std::abs(m_upDir.z) < 0.9f)
        ? Math::Vector3{ 0.0f, 0.0f, 1.0f }
        : Math::Vector3{ 1.0f, 0.0f, 0.0f };
    Math::Vector3 rightAxis;
    m_upDir.Cross(worldFwd, rightAxis);
    rightAxis.Normalize();
    Math::Vector3 fwdAxis;
    rightAxis.Cross(m_upDir, fwdAxis);
    fwdAxis.Normalize();

    static const Math::Vector2 dirs[4] = {
        { 1.0f,  0.0f}, {-1.0f,  0.0f},
        { 0.0f,  1.0f}, { 0.0f, -1.0f},
    };
    static const float heights[4] = {
        CollisionConst::WallRayOffsetYNeg,
        CollisionConst::WallRayOffsetY0,
        CollisionConst::WallRayOffsetY1,
        CollisionConst::WallRayOffsetY2,
    };
    static const float heights_Normal[4] = {
        CollisionConst::WallRayOffsetYNeg_Normal,
        CollisionConst::WallRayOffsetY0_Normal,
        CollisionConst::WallRayOffsetY1_Normal,
        CollisionConst::WallRayOffsetY2_Normal,
    };

    // 高速移動時のトンネリング防止：このフレームの実移動量分だけレイ長を延ばす
    const float         dt60_wall = KdFPSController::GetDt() * 60.0f;
    const Math::Vector3 horizVel  = m_velocity - m_upDir * m_velocity.Dot(m_upDir);
    const float         velExtra  = horizVel.Length() * dt60_wall;

    auto doPlanetRayPush = [&](const KdCollider& collider, const Math::Matrix& mat, bool isNormal)
    {
        float pushRightMax = 0.0f, pushRightMin = 0.0f;
        float pushFwdMax   = 0.0f, pushFwdMin   = 0.0f;

        const float* useHeights   = isNormal ? heights_Normal : heights;
        const int    useHeightCnt = isNormal ? 4 : 4;
        const float  baseRayLen   = isNormal ? CollisionConst::WallRayLength_Normal : CollisionConst::WallRayLength;
        const float  useRayLength = baseRayLen + velExtra;

        for (int hi = 0; hi < useHeightCnt; ++hi)
        {
            const Math::Vector3 rayOrigin = pos + m_upDir * useHeights[hi];
            for (const Math::Vector2& d : dirs)
            {
                const Math::Vector3 rayDir = rightAxis * d.x + fwdAxis * d.y;
                const KdCollider::RayInfo ray(KdCollider::TypeBump, rayOrigin, rayDir, useRayLength);

                std::list<KdCollider::CollisionResult> results;
                if (!collider.Intersects(ray, mat, &results)) { continue; }

                float maxOverlap = 0.0f;
                for (const auto& r : results)
                {
                    if (r.m_hitNDir.Dot(rayDir) > 0.0f) { continue; }
                    // 上下方向成分が大きい面（床・天面）を壁として誤判定しないようにスキップ
                    if (std::abs(r.m_hitNDir.Dot(m_upDir)) > CollisionConst::WallNormalUpThreshold) { continue; }
                    // velExtra は検出用のわたしなので押し出し量はベースレイ長以内にクランプ
                    const float clampedOverlap = std::min(r.m_overlapDistance, baseRayLen);
                    if (std::isfinite(clampedOverlap) && clampedOverlap > maxOverlap)
                        maxOverlap = clampedOverlap;
                }
                if (maxOverlap <= 0.0f) { continue; }

                const Math::Vector3 push = -rayDir * maxOverlap;
                const float dr = push.Dot(rightAxis);
                const float df = push.Dot(fwdAxis);
                if (dr > 0.0f) pushRightMax = std::max(pushRightMax, dr);
                else           pushRightMin = std::min(pushRightMin, dr);
                if (df > 0.0f) pushFwdMax   = std::max(pushFwdMax,   df);
                else           pushFwdMin   = std::min(pushFwdMin,   df);
            }
        }

        const Math::Vector3 totalPush =
            rightAxis * (pushRightMax + pushRightMin) +
            fwdAxis   * (pushFwdMax   + pushFwdMin);

        if (totalPush.LengthSquared() > 0.0f)
        {
            pos += totalPush;
            const float pushLen = totalPush.Length();
            if (pushLen > 0.0001f)
            {
                const Math::Vector3 pushNorm = totalPush / pushLen;
                // 壁方向への速度成分を完全にカット（入力で押し続けても次フレームに刺さらないように）
                const float velIntoWall = m_velocity.Dot(-pushNorm);
                if (velIntoWall > 0.0f)
                    m_velocity -= (-pushNorm) * velIntoWall;
            }
        }
    };

    auto doMapRayPush = [&](const std::shared_ptr<KdGameObject>& mapObj)
    {
        float pushRightMax = 0.0f, pushRightMin = 0.0f;
        float pushFwdMax   = 0.0f, pushFwdMin   = 0.0f;
        constexpr float baseRayLen = CollisionConst::WallRayLength;

        for (int hi = 0; hi < 4; ++hi)
        {
            const Math::Vector3 rayOrigin = pos + m_upDir * heights[hi];
            for (const Math::Vector2& d : dirs)
            {
                const Math::Vector3 rayDir = rightAxis * d.x + fwdAxis * d.y;
                const KdCollider::RayInfo ray(KdCollider::TypeBump, rayOrigin, rayDir, baseRayLen + velExtra);

                std::list<KdCollider::CollisionResult> results;
                if (!mapObj->Intersects(ray, &results)) { continue; }

                float maxOverlap = 0.0f;
                for (const auto& r : results)
                {
                    if (r.m_hitNDir.Dot(rayDir) > 0.0f) { continue; }
                    if (std::abs(r.m_hitNDir.Dot(m_upDir)) > CollisionConst::WallNormalUpThreshold) { continue; }
                    const float clampedOverlap = std::min(r.m_overlapDistance, baseRayLen);
                    if (std::isfinite(clampedOverlap) && clampedOverlap > maxOverlap)
                        maxOverlap = clampedOverlap;
                }
                if (maxOverlap <= 0.0f) { continue; }

                const Math::Vector3 push = -rayDir * maxOverlap;
                const float dr = push.Dot(rightAxis);
                const float df = push.Dot(fwdAxis);
                if (dr > 0.0f) pushRightMax = std::max(pushRightMax, dr);
                else           pushRightMin = std::min(pushRightMin, dr);
                if (df > 0.0f) pushFwdMax   = std::max(pushFwdMax,   df);
                else           pushFwdMin   = std::min(pushFwdMin,   df);
            }
        }

        const Math::Vector3 totalPush =
            rightAxis * (pushRightMax + pushRightMin) +
            fwdAxis   * (pushFwdMax   + pushFwdMin);

        if (totalPush.LengthSquared() > 0.0f)
        {
            pos += totalPush;
            const float pushLen = totalPush.Length();
            if (pushLen > 0.0001f)
            {
                const Math::Vector3 pushNorm = totalPush / pushLen;
                // 壁方向への速度成分を完全にカット
                const float velIntoWall = m_velocity.Dot(-pushNorm);
                if (velIntoWall > 0.0f)
                    m_velocity -= (-pushNorm) * velIntoWall;
            }
        }
    };

    // ---- Box 惑星
    const auto& planets = PlanetGravityManager::Instance().GetPlanets();
    for (const auto& p : planets)
    {
        if (p.Shape != PlanetShape::Box || !p.pCollider) { continue; }
        doPlanetRayPush(*p.pCollider, p.mWorld, p.bNormalGravity);

        // ---- キャップ面の押し出し（描画側と同じ cap 有無・範囲で直接計算）----
        // キャップがある面に対して、キャップ外面を壁としてキャラを押し出す
        const float capT = PlanetConst::GrassCapThickness;
        const float minOH = PlanetConst::GrassCapMinOverhang;

        const bool capTop    = p.bNormalGravity
            ? true
            : (p.BoxFaceGravityTop    == BoxFaceGravityMode::Inward);
        const bool capBottom = !p.bNormalGravity && p.BoxFaceGravityBottom == BoxFaceGravityMode::Inward;
        const bool capRight  = !p.bNormalGravity && p.BoxFaceGravityRight  == BoxFaceGravityMode::Inward;
        const bool capLeft   = !p.bNormalGravity && p.BoxFaceGravityLeft   == BoxFaceGravityMode::Inward;

        // 各面の「キャップ外面距離」を計算し、埋まっていれば押し出す
        // lp = ローカル座標（Planet相対）
        const Math::Vector3 lp = pos - p.Position;
        const Math::Vector3& half = p.BoxHalfExtents;

        struct CapWallFace
        {
            bool        hasCap;
            float       outerDist; // 正=外側、負=めり込み量
            Math::Vector3 normal;
            // この面の横方向の有効範囲チェック（面上にいるか）
            float       sideAbs;
            float       sideLimit;
        };

        const float extLR_top    = minOH + (capLeft  ? capT : 0.0f);
        const float extLR_top2   = minOH + (capRight ? capT : 0.0f);
        const float extTB_side   = minOH + (capTop    ? capT : 0.0f);
        const float extTB_side2  = minOH + (capBottom ? capT : 0.0f);

        const CapWallFace capFaces[4] =
        {
            // 上面キャップ外面：y = half.y + capT*2
            { capTop,    lp.y - (half.y + capT * 2.0f), { 0.0f,  1.0f, 0.0f },
              std::abs(lp.x), half.x + (extLR_top + extLR_top2) * 0.5f },
            // 下面
            { capBottom, -(lp.y + half.y + capT * 2.0f), { 0.0f, -1.0f, 0.0f },
              std::abs(lp.x), half.x + (extLR_top + extLR_top2) * 0.5f },
            // 右面
            { capRight,  lp.x - (half.x + capT * 2.0f), {  1.0f, 0.0f, 0.0f },
              std::abs(lp.y), half.y + (extTB_side + extTB_side2) * 0.5f },
            // 左面
            { capLeft,  -(lp.x + half.x + capT * 2.0f), { -1.0f, 0.0f, 0.0f },
              std::abs(lp.y), half.y + (extTB_side + extTB_side2) * 0.5f },
        };

        // キャップ押し出しはプレイヤーがBox外にいる面に対してのみ有効
        // Box本体各面の外面距離（正=その面の外側にいる）
        const float faceDistArr[4] = {
             lp.y - half.y,   // 上面
            -lp.y - half.y,   // 下面
             lp.x - half.x,   // 右面
            -lp.x - half.x,   // 左面
        };

        // ★ 2.5D: BoxのZ範囲外（奥行きはみ出し）ではキャップ壁を当てない。
        //   キャップ面の横範囲チェックが lp.x / lp.y のみで lp.z を見ないため、
        //   Z外に出ても上面キャップが壁として発火し、落下速度を打ち消していた（Z落下不能の真因）。
        if (std::abs(lp.z) > half.z + CollisionConst::GroundSampleRadius)
        {
            continue;   // このBoxのキャップ押し出しをスキップ（Z端から落とす）
        }

        for (int ci = 0; ci < 4; ++ci)
        {
            const auto& cf = capFaces[ci];
            if (!cf.hasCap) { continue; }
            // 面の横範囲外ならスキップ
            if (cf.sideAbs > cf.sideLimit + 0.01f) { continue; }
            // 外面よりも外側（まだ当たっていない）ならスキップ
            if (cf.outerDist > 0.0f) { continue; }
            // Box本体外面距離が正（この面側にプレイヤーがいる）ときだけ有効
            // 反対面側・Box内部のプレイヤーには発火しない
            if (faceDistArr[ci] < 0.0f) { continue; }
            // めり込み量だけ押し出す
            const float penetration = -cf.outerDist;
            pos += cf.normal * penetration;
            // 法線方向への速度成分をカット
            const float velInto = m_velocity.Dot(-cf.normal);
            if (velInto > 0.0f) { m_velocity += cf.normal * velInto; }
        }
    }

    // ---- 通常マップ
    const auto spMap = m_wpMap.lock();
    if (spMap)
    {
        doMapRayPush(spMap);
    }

    // ---- 移動床 側面押し出し（レイキャスト方式）
    for (auto& wpMF : m_movingFloors)
    {
        const auto spMF = wpMF.lock();
        if (!spMF) { continue; }
        const KdCollider* col = spMF->GetCollider();
        if (!col) { continue; }
        doPlanetRayPush(*col, spMF->GetWorldMatrix(), true);
    }

    // ---- WindBox 側面・底面押し出し（レイキャスト方式）
    for (auto& wpWB : m_windBoxColliders)
    {
        const auto spWB = wpWB.lock();
        if (!spWB) { continue; }
        // KdGameObject 経由で WindBox にダウンキャスト
        const auto spWBBox = std::dynamic_pointer_cast<WindBox>(spWB);
        if (!spWBBox || !spWBBox->IsEnabled()) { continue; }
        const KdCollider* col = spWBBox->GetCollider();
        if (!col) { continue; }
        doPlanetRayPush(*col, spWBBox->GetWorldMatrix(), true);
    }

    // ---- 敵障害物 押し出し（TypeBump レイキャスト）
    // 敵の m_mWorld はモデルスケール込みなので、GetPos() で平行移動のみの行列を組む
    for (auto& wpEnemy : m_enemyObstacles)
    {
        const auto spEnemy = wpEnemy.lock();
        if (!spEnemy || spEnemy->IsExpired()) { continue; }
        const KdCollider* col = spEnemy->GetCollider();
        if (!col) { continue; }
        const Math::Matrix colMat = Math::Matrix::CreateTranslation(spEnemy->GetPos());
        doPlanetRayPush(*col, colMat, true);
    }

    SetPos(pos);
}

// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 本体めり込み解決：球カプセル vs 地形(Box=AABB) を最近接点で押し出す
//  - レイではなく「形状が重なったら法線方向へ押し戻す」方式。
//  - 角(コーナー)では最近接点が頂点/辺になるので、斜め方向に正しく押し出される。
//    → 真四角の箱の端でも、レイでは取りこぼす端めり込みが原理的に起きない。
//  - Box は回転しない前提（軸並行=AABB）。将来回転させる場合は地形ローカル空間へ
//    変換して同じ「球 vs AABB」を回せば OBB として動く。
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
void Character::ResolvePenetration()
{
    const float r = CollisionConst::BodyRadius;
    const float offsets[2] = { CollisionConst::BodyLowerOffset, CollisionConst::BodyUpperOffset };

    // 1つのワールドAABBに対して、カプセル各球を押し出す。押し出したら true。
    auto resolveAABB = [&](const Math::Vector3& bmin, const Math::Vector3& bmax) -> bool
    {
        bool pushed = false;
        for (float off : offsets)
        {
            const Math::Vector3 c = GetPos() + m_upDir * off;  // 球の中心（ワールド）

            // AABB 上の最近接点（各軸を範囲内へクランプ）。これは範囲フラグ判定ではなく
            // 押し出しベクトルを得るための最近接点計算。
            const Math::Vector3 closest = {
                std::max(bmin.x, std::min(c.x, bmax.x)),
                std::max(bmin.y, std::min(c.y, bmax.y)),
                std::max(bmin.z, std::min(c.z, bmax.z)),
            };

            const Math::Vector3 d = c - closest;
            const float dist2 = d.LengthSquared();
            if (dist2 > r * r) { continue; }  // 重なっていない

            if (dist2 > 1e-8f)
            {
                // 表面の外側：最近接点→中心方向へ (r - dist) 押し出す（角は斜めになる）
                const float dist = std::sqrtf(dist2);
                const Math::Vector3 n = d / dist;
                SetPos(GetPos() + n * (r - dist));
            }
            else
            {
                // 中心がボックス内部：最も浅い面の外向きへ押し出す
                const float faceDist[6] = {
                    bmax.x - c.x, c.x - bmin.x,
                    bmax.y - c.y, c.y - bmin.y,
                    bmax.z - c.z, c.z - bmin.z,
                };
                const Math::Vector3 faceN[6] = {
                    {  1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
                    {  0.0f, 1.0f, 0.0f }, {  0.0f,-1.0f, 0.0f },
                    {  0.0f, 0.0f, 1.0f }, {  0.0f, 0.0f,-1.0f },
                };
                int best = 0;
                for (int i = 1; i < 6; ++i) { if (faceDist[i] < faceDist[best]) { best = i; } }
                SetPos(GetPos() + faceN[best] * (faceDist[best] + r));
            }
            pushed = true;
        }
        return pushed;
    };

    // 箱を「素の半幅 + キャップがある面だけ capThickness*2 膨らませた」ワールドAABBにして押し出す。
    // プレイヤーはグラスキャップ表面に立つ（CheckGround も同じ面にスナップする）ので、
    // 押し出しもこの膨張面を相手にしないと、キャップ領域の角でレイ隙間の貫通が残る。
    const float capT2 = PlanetConst::GrassCapThickness * 2.0f;
    auto resolveBox = [&](const Math::Vector3& center, const Math::Vector3& half,
                          bool isNormal,
                          BoxFaceGravityMode top,  BoxFaceGravityMode bottom,
                          BoxFaceGravityMode left, BoxFaceGravityMode right) -> bool
    {
        Math::Vector3 bmin = center - half;
        Math::Vector3 bmax = center + half;
        const bool capTop    = isNormal ? true : (top    == BoxFaceGravityMode::Inward);
        const bool capBottom = !isNormal && (bottom == BoxFaceGravityMode::Inward);
        const bool capRight  = !isNormal && (right  == BoxFaceGravityMode::Inward);
        const bool capLeft   = !isNormal && (left   == BoxFaceGravityMode::Inward);
        if (capTop)    { bmax.y += capT2; }
        if (capBottom) { bmin.y -= capT2; }
        if (capRight)  { bmax.x += capT2; }
        if (capLeft)   { bmin.x -= capT2; }
        return resolveAABB(bmin, bmax);
    };

    // 球 vs OBB（中心C・直交軸u,v,w・半幅half）の最近接点押し出し。
    // 地形のローカル空間（軸並行）で解いて戻すので、回転・横向きでも正しく押し出す。
    auto resolveOBB = [&](const Math::Vector3& C,
                          const Math::Vector3& u, const Math::Vector3& v, const Math::Vector3& w,
                          const Math::Vector3& half) -> bool
    {
        bool pushed = false;
        for (float off : offsets)
        {
            const Math::Vector3 cc = GetPos() + m_upDir * off;
            const Math::Vector3 d  = cc - C;
            // ローカル座標（各軸への射影）
            const float lx = d.Dot(u), ly = d.Dot(v), lz = d.Dot(w);
            const float clx = std::clamp(lx, -half.x, half.x);
            const float cly = std::clamp(ly, -half.y, half.y);
            const float clz = std::clamp(lz, -half.z, half.z);
            const float dx = lx - clx, dy = ly - cly, dz = lz - clz;
            const float dist2 = dx * dx + dy * dy + dz * dz;
            if (dist2 > r * r) { continue; }   // 重なっていない

            if (dist2 > 1e-8f)
            {
                // 表面の外側：最近接点→中心方向へ押し出す（ローカル→ワールド）
                const float dist = std::sqrtf(dist2);
                const Math::Vector3 nWorld = (u * dx + v * dy + w * dz) / dist;
                SetPos(GetPos() + nWorld * (r - dist));
            }
            else
            {
                // 中心が内部：最も浅い面の外向きへ
                const float fd[6] = {
                    half.x - lx, lx + half.x,
                    half.y - ly, ly + half.y,
                    half.z - lz, lz + half.z,
                };
                int best = 0;
                for (int i = 1; i < 6; ++i) { if (fd[i] < fd[best]) { best = i; } }
                Math::Vector3 axis;
                switch (best)
                {
                case 0: axis =  u; break;  case 1: axis = -u; break;
                case 2: axis =  v; break;  case 3: axis = -v; break;
                case 4: axis =  w; break;  default: axis = -w; break;
                }
                SetPos(GetPos() + axis * (fd[best] + r));
            }
            pushed = true;
        }
        return pushed;
    };

    // 複数接触（角など）に収束させるため数回反復する
    for (int iter = 0; iter < CollisionConst::PenetrationIterations; ++iter)
    {
        bool any = false;

        // Box 惑星
        for (const auto& p : PlanetGravityManager::Instance().GetPlanets())
        {
            if (p.Shape != PlanetShape::Box) { continue; }
            any |= resolveBox(p.Position, p.BoxHalfExtents, p.bNormalGravity,
                p.BoxFaceGravityTop, p.BoxFaceGravityBottom,
                p.BoxFaceGravityLeft, p.BoxFaceGravityRight);
        }

        // 移動床（GetPos=中心, GetHalfExtents=半幅）
        for (auto& wpMF : m_movingFloors)
        {
            const auto spMF = wpMF.lock();
            if (!spMF) { continue; }
            any |= resolveBox(spMF->GetPos(), spMF->GetHalfExtents(), spMF->IsNormalGravity(),
                spMF->GetFaceTop(), spMF->GetFaceBottom(),
                spMF->GetFaceLeft(), spMF->GetFaceRight());
        }

        // WindBox（球 vs OBB：横向き・回転でも見た目の箱どおりに押し出す）
        for (auto& wpWB : m_windBoxColliders)
        {
            const auto spWB = wpWB.lock();
            if (!spWB) { continue; }
            const auto spWBBox = std::dynamic_pointer_cast<WindBox>(spWB);
            if (!spWBBox || !spWBBox->IsEnabled()) { continue; }
            any |= resolveOBB(spWBBox->GetCenter(),
                spWBBox->GetWindRight(), spWBBox->GetWindDir(), spWBBox->GetWindUp(),
                spWBBox->GetObbHalf());
        }

        // SpikeBox（COL のワールドAABB に対し、惑星と同じ最近接点押し出し＝robust）
        for (auto& wpSB : m_spikeBoxColliders)
        {
            const auto spSB = wpSB.lock();
            if (!spSB) { continue; }
            const auto spSpike = std::dynamic_pointer_cast<SpikeBox>(spSB);
            if (!spSpike || !spSpike->IsEnabled() || !spSpike->HasColAabb()) { continue; }
            const Math::Vector3 c = spSpike->GetColCenter();
            const Math::Vector3 h = spSpike->GetColHalf();
            any |= resolveAABB(c - h, c + h);
        }

        if (!any) { break; }
    }
}

void Character::TakeDamage(int _damage)
{
    m_hp -= _damage;
    if (m_hp <= 0)
    {
        m_hp    = 0;
        m_state = State::Dead;
        m_isExpired = true;
    }
}

void Character::DrawCollisionDebugGui()
{
    // FirstUseEver：imgui.ini に配置/ドッキング情報があればそちらを優先（Onceだと毎起動で上書きされ外れる）
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(U8("当たり判定デバッグ")))
    {
        ImGui::End();
        return;
    }

    // ---- ログ ON/OFF ----
    ImGui::Checkbox(U8("ログ記録 (120フレーム)"), &m_debugLogEnabled);
    if (ImGui::Button(U8("ログ消去")))
    {
        m_collisionLog.clear();
        m_collisionLogIdx = 0;
    }
    ImGui::Separator();

    // ---- 今フレームのリアルタイム情報 ----
    ImGui::TextColored(ImVec4(1,1,0,1), "=== Current Frame ===");
    {
        const auto& f = m_currentFrameLog;
        ImGui::Text(U8("位置     : (%.3f, %.3f, %.3f)"), f.pos.x, f.pos.y, f.pos.z);
        ImGui::Text(U8("速度     : (%.3f, %.3f, %.3f)"), f.velocity.x, f.velocity.y, f.velocity.z);
        ImGui::Text(U8("上方向   : (%.3f, %.3f, %.3f)"), f.upDir.x, f.upDir.y, f.upDir.z);
        ImGui::Text(U8("接地     : %s"), f.isGround ? "TRUE" : "false");

        if (!f.wallHits.empty())
        {
            ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "Wall Hits (%d)", (int)f.wallHits.size());
            for (const auto& w : f.wallHits)
            {
                const ImVec4 col = w.filtered ? ImVec4(0.5f,0.5f,0.5f,1) : ImVec4(1,0.6f,0.2f,1);
                ImGui::TextColored(col,
                    "  h=%.2f rayDir(%.2f,%.2f,%.2f) nDir(%.2f,%.2f,%.2f) ov=%.4f %s%s",
                    w.height,
                    w.rayDir.x, w.rayDir.y, w.rayDir.z,
                    w.hitNDir.x, w.hitNDir.y, w.hitNDir.z,
                    w.overlap,
                    w.isNormal ? "[N]" : "   ",
                    w.filtered ? "[FILTERED]" : "");
            }
            ImGui::Text(U8("  押し戻し合計: (%.4f, %.4f, %.4f)"),
                f.wallTotalPush.x, f.wallTotalPush.y, f.wallTotalPush.z);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Wall Hits: none");
        }

        if (!f.groundHits.empty())
        {
            ImGui::TextColored(ImVec4(0.4f,0.8f,1,1), "Ground Hits (%d)", (int)f.groundHits.size());
            for (const auto& g : f.groundHits)
            {
                const ImVec4 col = g.filtered ? ImVec4(0.5f,0.5f,0.5f,1) : ImVec4(0.2f,1,1,1);
                ImGui::TextColored(col,
                    "  pos(%.2f,%.2f,%.2f) n(%.2f,%.2f,%.2f) dist=%.4f %s",
                    g.hitPos.x, g.hitPos.y, g.hitPos.z,
                    g.hitNDir.x, g.hitNDir.y, g.hitNDir.z,
                    g.hitDist,
                    g.filtered ? "[FILTERED]" : "");
            }
            ImGui::Text(U8("  スナップ: %s"), f.groundSnapped ? "YES" : "no");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Ground Hits: none");
        }
    }

    ImGui::Separator();

    // ---- 過去ログ（リングバッファ）----
    if (!m_collisionLog.empty())
    {
        ImGui::TextColored(ImVec4(1,1,0,1), "=== Frame Log (newest first) ===");
        const int total = static_cast<int>(m_collisionLog.size());
        // 最新フレームから遡って表示
        for (int i = 1; i <= total; ++i)
        {
            const int idx = (m_collisionLogIdx - i + total) % total;
            const auto& f = m_collisionLog[idx];
            // 空エントリはスキップ（velocity が全0 で pos も 0）
            if (f.velocity.LengthSquared() < 0.000001f && f.pos.LengthSquared() < 0.000001f) { continue; }

            const bool hasWall   = !f.wallHits.empty();
            const bool hasGround = !f.groundHits.empty();
            ImVec4 rowCol = ImVec4(0.8f,0.8f,0.8f,1);
            if (!hasWall && !hasGround) rowCol = ImVec4(1,0.3f,0.3f,1); // 全スカ = 赤
            else if (hasWall)           rowCol = ImVec4(1,0.7f,0.2f,1); // 壁ヒット = 橙
            else                        rowCol = ImVec4(0.4f,1,0.4f,1); // 床のみ  = 緑

            char label[64];
            snprintf(label, sizeof(label), "[-%d] pos(%.2f,%.2f,%.2f) v(%.2f,%.2f,%.2f) G=%s W=%d Gnd=%d",
                i,
                f.pos.x, f.pos.y, f.pos.z,
                f.velocity.x, f.velocity.y, f.velocity.z,
                f.isGround ? "Y" : "N",
                (int)f.wallHits.size(), (int)f.groundHits.size());

            ImGui::TextColored(rowCol, "%s", label);
        }
    }

    ImGui::End();
}

