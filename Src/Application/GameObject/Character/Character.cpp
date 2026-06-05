#include "../../../Pch.h"
#include "Character.h"
#include "../Gimmick/MovingFloor.h"

void Character::Update()
{
    ApplyGravity();
}

void Character::PostUpdate()
{
    // 重力切り替え時の押し出しをLerpで滑らかに適用
    bool isEjecting = false;
    if (m_ejectRemaining.LengthSquared() > 0.0001f)
    {
        isEjecting = true;
        constexpr float kEjectLerp = 0.3f;
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

        // 分軸処理：横→壁判定→縦→床・天井判定
        m_preMovePos = GetPos();  // 水平移動前の位置を記録（移動床 XZ 判定に使う）
        ApplyVelocityHorizontal();
        CheckWall();
        ApplyVelocityVertical();
        CheckCeiling();
        CheckGround();

        // ---- 乗り床追従：CheckGround で今フレーム乗っていると判定された床のみ追従 ----
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

void Character::ApplyGravity()
{
    const GravityInfluenceResult gravResult = PlanetGravityManager::Instance().ComputeGravityInfluence(GetPos(), -m_upDir);
    m_prevPlanetIndex = m_currentPlanetIndex;
    // 着地中は現在の惑星を固定。空中のときだけ支配惑星に乗り換える
    if (!m_isGround)
    {
        m_currentPlanetIndex = gravResult.dominantPlanetIdx;
        m_pCurrentPlanet     = PlanetGravityManager::Instance().GetPlanet(m_currentPlanetIndex);
    }

    const bool inManualZone = ManualGravityZoneManager::Instance().CanUseManualGravity(GetPos());
    const bool hasManual    = (m_manualGravityDir != ManualGravityDir::None);

    // ── ケース① ゾーン外 + 手動あり + 惑星圏内 → 惑星に捕まる、手動リセット ──
    if (!inManualZone && hasManual && gravResult.hasInfluence)
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
            Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed)));
        m_upDirVisual.Normalize();

        if (m_isGround) { return; }  // velocity 加算のみスキップ、Slerp は上で完了済み

        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale, PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += gravDir * (newRadial - radialVel); }
        return;
    }
    Math::Vector3 zoneGravDir;
    const bool inNormalZone = ManualGravityZoneManager::Instance().IsInNormalGravityZone(GetPos(), zoneGravDir);
    // 惑星引力を無効にするのはどちらのゾーン内でも常にオフ
    const bool suppressPlanet = inNormalZone || inManualZone;

    // NormalBox着地中は外部惑星重力を一切受け付けない（非NormalはComputeGravityInfluenceに任せる）
    const bool onNormalBoxGround = m_isGround
        && m_pCurrentPlanet
        && m_pCurrentPlanet->Shape == PlanetShape::Box
        && m_pCurrentPlanet->bNormalGravity;

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
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQv, toQv, PlanetConst::UpDirSlerpSpeed)));
            m_upDirVisual.Normalize();
            return;
        }

        const Math::Vector3 gravDir  = gravResult.totalGravityDir;
        const Math::Vector3 targetUp = -gravDir;

        // m_upDir は着地中も常に更新（球面上で向きを合わせ続ける必要がある）
        m_upDir = targetUp;
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed)));
            m_upDirVisual.Normalize();
        }

        if (m_isGround)
        {
            // 着地中も visual upDir の Slerp は継続（途中で止めると傾いたまま固定される）
            const bool onDominantPlanet = (gravResult.dominantPlanetIdx == m_currentPlanetIndex);
            if (!onDominantPlanet) { return; }
        }

        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
        if (newRadial > radialVel) { m_velocity += gravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑤ NormalGravityゾーン ──
    if (inNormalZone)
    {
        const Math::Vector3 targetUp = -zoneGravDir;

        // m_upDir は着地中も常に更新
        m_upDir = targetUp;
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed)));
            m_upDirVisual.Normalize();
        }

        if (m_isGround) { return; }  // velocity 加算のみスキップ、Slerp は上で完了済み
        const float radialVel = m_velocity.Dot(zoneGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale, PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += zoneGravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑥ ManualGravityゾーン内で手動入力なし → 通常下向き重力 ──
    if (inManualZone)
    {
        constexpr Math::Vector3 kDefaultGravDir = { 0.0f, -1.0f, 0.0f };
        constexpr Math::Vector3 kDefaultUp      = { 0.0f,  1.0f, 0.0f };
        m_upDir = kDefaultUp;
        const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
        const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, kDefaultUp);
        m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
            Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed)));
        m_upDirVisual.Normalize();
        if (m_isGround) { return; }  // velocity 加算のみスキップ、Slerp は上で完了済み
        const float radialVel = m_velocity.Dot(kDefaultGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel * m_gravityScale, PlanetConst::MaxFallSpeed * m_gravityScale);
        if (newRadial > radialVel) { m_velocity += kDefaultGravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑦ 無重力 ──
}

void Character::ApplyVelocity()
{
    Math::Vector3 pos = GetPos();
    pos += m_velocity;
    SetPos(pos);
}

void Character::ApplyVelocityHorizontal()
{
    // m_upDir軸垂直面への投影（水平成分のみ適用）
    const Math::Vector3 vertComponent = m_upDir * m_velocity.Dot(m_upDir);
    const Math::Vector3 horizVelocity = m_velocity - vertComponent;
    Math::Vector3 pos = GetPos();
    pos += horizVelocity;
    SetPos(pos);
}

void Character::ApplyVelocityVertical()
{
    // m_upDir軸方向の成分のみ適用
    const Math::Vector3 vertVelocity = m_upDir * m_velocity.Dot(m_upDir);
    Math::Vector3 pos = GetPos();
    pos += vertVelocity;
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
    if (!spMap) { return; }

    std::list<KdCollider::CollisionResult> results;
    if (!spMap->Intersects(ray, &results)) { return; }

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

void Character::CheckGround()
{
    m_isGround     = false;
    m_pRidingFloor = nullptr;

    const Math::Vector3 pos = GetPos();

    // ---- ★ 移動床（Planet NormalGravity Box と同一の幾何距離方式）----
    // 早期 return より前に判定することで惑星上でも必ず拾う
    if (m_velocity.Dot(m_upDir) <= 0.1f)
    {
        for (auto& wpMF : m_movingFloors)
        {
            const auto spMF = wpMF.lock();
            if (!spMF) { continue; }

            const Math::Vector3  mfPos  = spMF->GetPos();
            const Math::Vector3& half   = spMF->GetHalfExtents();
            const Math::Vector3  lp     = pos - mfPos;

            // XZ 範囲チェックは「水平移動前の位置」で行う
            // CheckWall で押し返されて範囲内に戻った場合を弾くため
            const Math::Vector3  lpPre  = m_preMovePos - mfPos;
            if (std::abs(lpPre.x) > half.x) { continue; }
            if (std::abs(lpPre.z) > half.z) { continue; }

            // 上面 + キャップ外面からの Y 距離
            const float capSurface = PlanetConst::GrassCapThickness * 2.0f;
            const float surfaceY   = mfPos.y + half.y + capSurface;  // 絶対Y座標
            const float distToSurf = pos.y - surfaceY;               // 正=上方, 負=めり込み

            // 上すぎ → スキップ
            if (distToSurf >  CollisionConst::GroundSnapDist) { continue; }
            // 下すぎ → スキップ（側面から押し返されて床下に入った場合を弾く）
            // 上方向からの着地のみを意図しているため、食い込みは極小値のみ許容
            if (distToSurf < -CollisionConst::GroundHitDistMin) { continue; }

            // 上面にスナップ（Y のみ補正）
            Math::Vector3 corrected = GetPos();
            corrected.y -= distToSurf;
            SetPos(corrected);

            // 下方向速度をカット
            const float normalComp = m_velocity.Dot(m_upDir);
            if (normalComp < 0.0f) { m_velocity -= m_upDir * normalComp; }

            m_isGround = true;
            m_airGravitySwitchCount = 0;
            m_pRidingFloor = &wpMF;
            return;
        }
    }

    // ---- ① Box惑星（Normal・非Normal共通）── 外側距離方式 ----
    if (m_pCurrentPlanet
        && m_pCurrentPlanet->Shape == PlanetShape::Box)
    {
        // ジャンプ直後（m_upDir方向に速度あり）はスキップ
        if (m_velocity.Dot(m_upDir) > 0.1f) { return; }

        const PlanetData& p    = *m_pCurrentPlanet;
        const Math::Vector3 lp = pos - p.Position;
        const Math::Vector3& half = p.BoxHalfExtents;

        // NormalBox は Top面(Y+)のみ、非NormalBox は 4面すべて判定
        // キャップがある面はキャップ厚み分だけ外側距離を拡張する
        const float capExt = PlanetConst::GrassCapThickness * 2.0f;

        struct FaceInfo { float dist; Math::Vector3 normal; bool hasCap; };
        const FaceInfo faces[4] = {
            // 上面：常にキャップあり
            {  lp.y - half.y, { 0.0f,  1.0f, 0.0f }, true  },
            // 下面
            { -lp.y - half.y, { 0.0f, -1.0f, 0.0f },
              !p.bNormalGravity && p.BoxFaceGravityBottom == BoxFaceGravityMode::Inward },
            // 右面
            {  lp.x - half.x, {  1.0f, 0.0f, 0.0f },
              !p.bNormalGravity && p.BoxFaceGravityRight == BoxFaceGravityMode::Inward },
            // 左面
            { -lp.x - half.x, { -1.0f, 0.0f, 0.0f },
              !p.bNormalGravity && p.BoxFaceGravityLeft  == BoxFaceGravityMode::Inward },
        };

        // Left/Right にキャップがある場合、Top の X 判定範囲を広げる（描画側と同じ計算）
        const bool leftHasCap  = !p.bNormalGravity && p.BoxFaceGravityLeft  == BoxFaceGravityMode::Inward;
        const bool rightHasCap = !p.bNormalGravity && p.BoxFaceGravityRight == BoxFaceGravityMode::Inward;
        const float topCapExtX = PlanetConst::GrassCapMinOverhang
                               + (leftHasCap  ? PlanetConst::GrassCapThickness : 0.0f)
                               + (rightHasCap ? PlanetConst::GrassCapThickness : 0.0f);

        for (int fi = 0; fi < 4; ++fi)
        {
            // NormalBox は上面(fi==0)のみ
            if (p.bNormalGravity && fi != 0) { continue; }

            const auto& f = faces[fi];

            // 現在の m_upDir がこの面の法線方向と揃っているか
            if (f.normal.Dot(m_upDir) < 0.7f) { continue; }

            // ---- この面の有効矩形範囲チェック ----
            if (std::abs(f.normal.y) > 0.5f) // 上面 or 下面
            {
                // Top面はLeft/Rightキャップがある分だけX範囲を拡張
                const float xLimit = (fi == 0) ? half.x + topCapExtX : half.x;
                if (std::abs(lp.x) > xLimit) { continue; }
            }
            else // 左面 or 右面
            {
                if (std::abs(lp.y) > half.y) { continue; }
            }

            // キャップがある面はキャップ外面（Box面 + capThickness*2）を着地面とする
            const float capSurface = f.hasCap ? (PlanetConst::GrassCapThickness * 2.0f) : 0.0f;
            // dist to cap surface: 正=まだ外側、負=めり込み
            const float distToSurface = f.dist - capSurface;

            if (distToSurface >  CollisionConst::GroundSnapDist) { continue; }
            if (distToSurface < -CollisionConst::GroundSnapDist) { continue; }

            // キャップ外面にスナップ
            Math::Vector3 corrected = pos;
            corrected.x -= f.normal.x * distToSurface;
            corrected.y -= f.normal.y * distToSurface;
            SetPos(corrected);

            // 面法線方向の速度成分を除去
            const float normalComp = m_velocity.Dot(f.normal);
            if (normalComp < 0.0f) { m_velocity -= f.normal * normalComp; }

            m_isGround = true;
            m_airGravitySwitchCount = 0;
            return;
        }
        return; // Box惑星担当なのでSphere/マップには落ちない
    }

    // ---- ② Sphere等の惑星 ─ レイキャスト方式 ----
    if (m_pCurrentPlanet && m_pCurrentPlanet->pCollider)
    {
        // ジャンプ直後はスキップ
        if (m_velocity.Dot(m_upDir) > 0.001f) { return; }

        const Math::Vector3 center = m_pCurrentPlanet->Position;
        Math::Vector3 outDir;
        if (m_pCurrentPlanet->bNormalGravity)
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

        const float         rayLen   = m_pCurrentPlanet->GravityRadius + PlanetConst::PlanetRayOffset;
        const Math::Vector3 rayStart = pos + outDir * PlanetConst::PlanetRayOffset;
        const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, -outDir, rayLen);

        std::list<KdCollider::CollisionResult> results;
        if (!m_pCurrentPlanet->pCollider->Intersects(ray, m_pCurrentPlanet->mWorld, &results)) { return; }

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

    // 高速移動時のトンネリング防止：水平速度分レイ長を延ばす
    const Math::Vector3 horizVel  = m_velocity - m_upDir * m_velocity.Dot(m_upDir);
    const float         velExtra  = horizVel.Length();

    auto doPlanetRayPush = [&](const KdCollider& collider, const Math::Matrix& mat, bool isNormal)
    {
        float pushRightMax = 0.0f, pushRightMin = 0.0f;
        float pushFwdMax   = 0.0f, pushFwdMin   = 0.0f;

        const float* useHeights   = isNormal ? heights_Normal : heights;
        const int    useHeightCnt = isNormal ? 4 : 4;
        const float  useRayLength = (isNormal ? CollisionConst::WallRayLength_Normal : CollisionConst::WallRayLength) + velExtra;

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
                    if (std::isfinite(r.m_overlapDistance) && r.m_overlapDistance > maxOverlap)
                        maxOverlap = r.m_overlapDistance;
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

        for (int hi = 0; hi < 4; ++hi)
        {
            const Math::Vector3 rayOrigin = pos + m_upDir * heights[hi];
            for (const Math::Vector2& d : dirs)
            {
                const Math::Vector3 rayDir = rightAxis * d.x + fwdAxis * d.y;
                const KdCollider::RayInfo ray(KdCollider::TypeBump, rayOrigin, rayDir, CollisionConst::WallRayLength + velExtra);

                std::list<KdCollider::CollisionResult> results;
                if (!mapObj->Intersects(ray, &results)) { continue; }

                float maxOverlap = 0.0f;
                for (const auto& r : results)
                {
                    if (r.m_hitNDir.Dot(rayDir) > 0.0f) { continue; }
                    if (std::abs(r.m_hitNDir.Dot(m_upDir)) > CollisionConst::WallNormalUpThreshold) { continue; }
                    if (std::isfinite(r.m_overlapDistance) && r.m_overlapDistance > maxOverlap)
                        maxOverlap = r.m_overlapDistance;
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

    SetPos(pos);
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
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    if (!ImGui::Begin("Collision Debug"))
    {
        ImGui::End();
        return;
    }

    // ---- ログ ON/OFF ----
    ImGui::Checkbox("Enable Log (120 frames)", &m_debugLogEnabled);
    if (ImGui::Button("Clear Log"))
    {
        m_collisionLog.clear();
        m_collisionLogIdx = 0;
    }
    ImGui::Separator();

    // ---- 今フレームのリアルタイム情報 ----
    ImGui::TextColored(ImVec4(1,1,0,1), "=== Current Frame ===");
    {
        const auto& f = m_currentFrameLog;
        ImGui::Text("Pos      : (%.3f, %.3f, %.3f)", f.pos.x, f.pos.y, f.pos.z);
        ImGui::Text("Velocity : (%.3f, %.3f, %.3f)", f.velocity.x, f.velocity.y, f.velocity.z);
        ImGui::Text("UpDir    : (%.3f, %.3f, %.3f)", f.upDir.x, f.upDir.y, f.upDir.z);
        ImGui::Text("isGround : %s", f.isGround ? "TRUE" : "false");

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
            ImGui::Text("  TotalPush: (%.4f, %.4f, %.4f)",
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
            ImGui::Text("  Snapped: %s", f.groundSnapped ? "YES" : "no");
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

