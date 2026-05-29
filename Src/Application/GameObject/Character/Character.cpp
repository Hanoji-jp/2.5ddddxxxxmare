#include "../../../Pch.h"
#include "Character.h"

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

    ApplyVelocity();

    if (!isEjecting)
    {
        CheckWall();   // 水平押し出しを先に → 箱に潜り込む前に外へ出す
        CheckGround();
    }
}

void Character::DrawDebug()
{
    if (m_pDebugWire) { m_pDebugWire->Draw(); }
}

void Character::ApplyGravity()
{
    const GravityInfluenceResult gravResult = PlanetGravityManager::Instance().ComputeGravityInfluence(GetPos(), -m_upDir);
    m_prevPlanetIndex    = m_currentPlanetIndex;
    m_currentPlanetIndex = gravResult.dominantPlanetIdx;
    m_pCurrentPlanet     = PlanetGravityManager::Instance().GetPlanet(m_currentPlanetIndex);

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

        if (m_isGround) { return; }
        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
        if (newRadial > radialVel) { m_velocity += gravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース④ 惑星重力（NormalGravityゾーン内のみ惑星引力オフ、ManualGravityゾーンは手動入力がある場合のみオフ）──
    Math::Vector3 zoneGravDir;
    const bool inNormalZone = ManualGravityZoneManager::Instance().IsInNormalGravityZone(GetPos(), zoneGravDir);
    // 惑星引力を無効にするのはどちらのゾーン内でも常にオフ
    const bool suppressPlanet = inNormalZone || inManualZone;

    if (gravResult.hasInfluence && !suppressPlanet)
    {
        const Math::Vector3 targetUp = -gravResult.totalGravityDir;

        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
            m_upDir = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, 1.0f)));
            m_upDir.Normalize();
        }
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed)));
            m_upDirVisual.Normalize();
        }

        if (m_isGround) { return; }
        const Math::Vector3 gravDir = gravResult.totalGravityDir;
        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
        if (newRadial > radialVel) { m_velocity += gravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑤ NormalGravityゾーン ──
    if (inNormalZone)
    {
        const Math::Vector3 targetUp = -zoneGravDir;

        // m_upDir を slerp で滑らかに更新
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
            m_upDir = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, 1.0f)));
            m_upDir.Normalize();
        }
        // m_upDirVisual も slerp で更新（これがないとキャラクターの見た目が狂う）
        {
            const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed)));
            m_upDirVisual.Normalize();
        }

        if (m_isGround) { return; }
        const float radialVel = m_velocity.Dot(zoneGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
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
        if (m_isGround) { return; }
        const float radialVel = m_velocity.Dot(kDefaultGravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
        if (newRadial > radialVel) { m_velocity += kDefaultGravDir * (newRadial - radialVel); }
        return;
    }

    // ── ケース⑦ 無重力 ──
}

void Character::ApplyVelocity()
{
    Math::Vector3 pos = GetPos();
    pos += m_velocity;

    // 床スナップ：lerpで滑らかに着地（瞬間テレポート防止）
    if (m_snapActive)
    {
        pos.y = std::lerp(pos.y, m_snapTargetY, CollisionConst::GroundSnapLerpSpeed);
        if (std::abs(pos.y - m_snapTargetY) < 0.001f)
        {
            pos.y        = m_snapTargetY;
            m_snapActive = false;
        }
    }

    SetPos(pos);
}

void Character::CheckGround()
{
    m_isGround   = false;
    m_snapActive = false;   // 毎フレームリセット（今フレームで着地しなければ非アクティブ）

    // ---- Box 惑星の着地判定：レイキャストで面を検出 ----
    {
        const Math::Vector3 pos    = GetPos();
        const Math::Vector3 rayDir = -m_upDir; // 重力方向（下）へレイを飛ばす

        // 足元より少し上からレイを開始し、SnapDist 先まで飛ばす
        const float           rayLen   = CollisionConst::GroundRayOffset + CollisionConst::GroundSnapDist;
        const Math::Vector3   rayStart = pos + m_upDir * CollisionConst::GroundRayOffset;
        const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, rayDir, rayLen);

        // ジャンプ直後（上方向に速度あり）はスキップ
        if (m_velocity.Dot(m_upDir) <= 0.001f)
        {
            const auto& planets = PlanetGravityManager::Instance().GetPlanets();
            int   bestIdx  = -1;
            float bestDist = FLT_MAX; // レイ起点からヒットまでの距離（小さいほど近い）
            Math::Vector3 bestNormal = m_upDir;
            Math::Vector3 bestHitPos = pos;

            for (int i = 0; i < static_cast<int>(planets.size()); ++i)
            {
                const PlanetData& p = planets[i];
                if (p.Shape != PlanetShape::Box || !p.pCollider) { continue; }

                std::list<KdCollider::CollisionResult> results;
                if (!p.pCollider->Intersects(ray, p.mWorld, &results)) { continue; }

                for (const auto& r : results)
                {
                    // ヒット点がプレイヤー足元より十分上にある（壁の天面）は無視
                    // レイ起点は pos + upDir * GroundRayOffset なので
                    // ヒット点が pos + upDir * GroundRayOffset より upDir 方向に近い = 足元より上
                    const Math::Vector3 toHit = r.m_hitPos - pos;
                    const float upComp = toHit.Dot(m_upDir);
                    // upComp > GroundRayOffset の場合、ヒット点が足元より上 → 壁天面なので無視
                    if (upComp > CollisionConst::GroundRayOffset + 0.05f) { continue; }

                    // ヒット点の水平距離がキャラ半径より大きければ壁の天面なので無視
                    const Math::Vector3 horizontal = toHit - m_upDir * upComp;
                    if (horizontal.LengthSquared() > CollisionConst::GroundHitHorizontalMax * CollisionConst::GroundHitHorizontalMax) { continue; }

                    // overlapDistance = rayLen - hitDist なので hitDist = rayLen - overlapDistance
                    const float hitDist = rayLen - r.m_overlapDistance;

                    if (hitDist < bestDist)
                    {
                        bestDist   = hitDist;
                        bestIdx    = i;
                        bestNormal = r.m_hitNDir;
                        bestHitPos = r.m_hitPos;
                    }
                }
            }

            if (bestIdx >= 0)
            {
                // ヒット面上にスナップ（瞬時）
                Math::Vector3 corrected = pos;
                corrected += m_upDir * (CollisionConst::GroundRayOffset - bestDist + 0.001f);
                SetPos(corrected);

                // m_upDir を bestNormal へ lerp で滑らかに近づける（床から落ちる瞬間のカクつき防止）
                const Math::Quaternion fromQ = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDir);
                const Math::Quaternion toQ   = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, bestNormal);
                m_upDir = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f },
                    Math::Matrix::CreateFromQuaternion(Math::Quaternion::Slerp(fromQ, toQ, CollisionConst::GroundUpDirSlerpSpeed)));
                m_upDir.Normalize();

                const float inwardVel = m_velocity.Dot(-bestNormal);
                if (inwardVel > 0.0f)
                    m_velocity += bestNormal * inwardVel * GameConst::LandingDamping;

                if (m_prevPlanetIndex != bestIdx)
                {
                    const float radial = m_velocity.Dot(bestNormal);
                    m_velocity = bestNormal * radial;
                }
                m_isGround = true;
                m_airGravitySwitchCount = 0;
                return;
            }
        }
    }

    // 惑星重力下にいる場合は着地判定（Sphere等）
    if (m_pCurrentPlanet)
    {
        if (!m_pCurrentPlanet->pCollider) { return; }
        const Math::Vector3 pos    = GetPos();
        const Math::Vector3 center = m_pCurrentPlanet->Position;

        // 惑星中心→プレイヤー方向（＝「上」方向）
        Math::Vector3 outDir;
        if (m_pCurrentPlanet->bNormalGravity)
        {
            outDir = { 0.0f, 1.0f, 0.0f };
        }
        else
        {
            // XY 平面で計算（Z は固定）
            const float dx = pos.x - center.x;
            const float dy = pos.y - center.y;
            const float d  = std::sqrtf(dx * dx + dy * dy);
            if (d < 0.001f) { return; }
            outDir = { dx / d, dy / d, 0.0f };
        }

        // ジャンプ直後（外向きに速度がある）はスキップ
        if (m_velocity.Dot(outDir) > 0.001f) { return; }

        // レイ：プレイヤーより少し外側 → 中心方向へ
        const float         rayLen   = m_pCurrentPlanet->GravityRadius + PlanetConst::PlanetRayOffset;
        const Math::Vector3 rayStart = pos + outDir * PlanetConst::PlanetRayOffset;
        const Math::Vector3 rayDir   = -outDir;

        const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, rayDir, rayLen);
        std::list<KdCollider::CollisionResult> results;

        if (!m_pCurrentPlanet->pCollider->Intersects(ray, m_pCurrentPlanet->mWorld, &results))
        {
            return;
        }

        // レイ起点から最も近いヒット点を選ぶ（overlapDistance が大きいほど起点に近い）
        const KdCollider::CollisionResult* pBest = nullptr;
        for (const auto& r : results)
        {
            if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance)
            {
                pBest = &r;
            }
        }
        if (!pBest) { return; }

        // ヒット面の座標とプレイヤー位置を outDir 軸で比較
        // hitPos は面上の点、pos は現在位置
        // outDir 方向成分の差 = (pos - hitPos)・outDir
        // 正 → プレイヤーが面より外側（正常、着地手前）
        // 負 → プレイヤーが面より内側（めり込み）
        const float diff = (pos - pBest->m_hitPos).Dot(outDir);

        // SnapDist の範囲内（少し外側〜めり込み）なら着地とみなす
        if (diff < CollisionConst::GroundSnapDist)
        {
            // 面の上にスナップ（hitPos + outDir 方向の少しのオフセット）
            const Math::Vector3 snapPos = pBest->m_hitPos + outDir * 0.001f;
            // X,Y,Z のうち outDir 成分だけ補正する
            Math::Vector3 corrected = pos;
            corrected.x = snapPos.x * std::abs(outDir.x) + pos.x * (1.0f - std::abs(outDir.x));
            corrected.y = snapPos.y * std::abs(outDir.y) + pos.y * (1.0f - std::abs(outDir.y));
            SetPos(corrected);

            // 中心方向への速度成分を消す（着地）
            const float inwardVel = m_velocity.Dot(-outDir);
            if (inwardVel > 0.0f)
            {
                m_velocity += outDir * inwardVel * GameConst::LandingDamping;
            }
            // 惑星輸送時（異なる惑星に移動）は接線方向の慣性もクリア
            if (m_prevPlanetIndex != m_currentPlanetIndex)
            {
                const float radial = m_velocity.Dot(outDir);
                m_velocity = outDir * radial;
            }
            m_isGround = true;
            m_airGravitySwitchCount = 0;  // 着地で空中切り替え回数リセット
        }

        return;
    }

    // 通常マップ上のレイキャスト着地判定
    const auto spMap = m_wpMap.lock();
    if (!spMap) { return; }

    // ジャンプ直後（上方向に速度あり）はスキップ
    if (m_velocity.y > 0.001f) { return; }

    Math::Vector3 pos = GetPos();

    const Math::Vector3 rayStart = pos + Math::Vector3(0.0f, CollisionConst::GroundRayOffset, 0.0f);
    const Math::Vector3 rayDir   = Math::Vector3(0.0f, -1.0f, 0.0f);
    const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, rayDir, CollisionConst::GroundRayLength);

    std::list<KdCollider::CollisionResult> results;
    if (!spMap->Intersects(ray, &results)) { return; }

    const KdCollider::CollisionResult* pBest = nullptr;
    for (auto& r : results)
    {
        // ヒット面がプレイヤー足元より上にある（壁の天面）は無視
        if (r.m_hitPos.y > pos.y + 0.01f) { continue; }
        // ヒット点がプレイヤーの真下でない（横のボックス天面に当たっている）は無視
        const float dx = r.m_hitPos.x - pos.x;
        const float dz = r.m_hitPos.z - pos.z;
        if (dx * dx + dz * dz > CollisionConst::GroundHitHorizontalMax * CollisionConst::GroundHitHorizontalMax) { continue; }
        if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance)
            pBest = &r;
    }

    if (pBest)
    {
        const float floorY      = pBest->m_hitPos.y;
        const float penetration = floorY - pos.y;
        if (penetration > -CollisionConst::GroundSnapDist)
        {
            if (penetration > 0.0f)
            {
                // 既にめり込んでいる場合は即座に押し出す
                pos.y = floorY;
                SetPos(pos);
            }
            else
            {
                // 近接している場合はlerpスナップ
                m_snapTargetY = floorY;
                m_snapActive  = true;
            }
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

    const float r = CollisionConst::WallSphereRadius;

    // 1球あたりの押し出し処理（pos 更新のたびに球位置を再計算）
    auto doSpherePush = [&](auto intersectFn)
    {
        for (int si = 0; si < 2; ++si)
        {
            // pos が更新されるたびに球の位置を再計算する
            const Math::Vector3 center = pos + m_upDir * (si == 0
                ? CollisionConst::WallSphereOffsetY
                : CollisionConst::WallSphereOffsetY + r * 1.5f);

            const KdCollider::SphereInfo sphere(KdCollider::TypeBump,
                DirectX::BoundingSphere{ center, r });

            std::list<KdCollider::CollisionResult> results;
            if (!intersectFn(sphere, &results)) { continue; }

            for (const auto& res : results)
            {
                if (res.m_overlapDistance <= 0.0f) { continue; }

                Math::Vector3 pushDir = res.m_hitDir;
                if (pushDir.LengthSquared() < 0.0001f) { continue; }
                pushDir.Normalize();

                // 上方向成分を除去して水平成分だけにする
                const float upComp = pushDir.Dot(m_upDir);
                const Math::Vector3 horizontalDir = pushDir - m_upDir * upComp;
                const float horizontalLen = horizontalDir.Length();

                // 水平方向成分がほぼない（真上・真下）→ 床判定に任せる
                if (horizontalLen < 0.01f) { continue; }

                const Math::Vector3 finalPushDir = horizontalDir / horizontalLen;
                // 水平方向の実際の押し出し量
                const float finalPushDist = res.m_overlapDistance * horizontalLen;

                pos += finalPushDir * finalPushDist;

                // 壁方向への速度成分を消す
                const float velInto = m_velocity.Dot(-finalPushDir);
                if (velInto > 0.0f)
                    m_velocity += finalPushDir * velInto;
            }
        }
    };

    // 2イテレーションで深いめり込みも1フレームで解消
    constexpr int kIterations = 2;
    for (int iter = 0; iter < kIterations; ++iter)
    {
        // ---- Box 惑星
        const auto& planets = PlanetGravityManager::Instance().GetPlanets();
        for (const auto& p : planets)
        {
            if (p.Shape != PlanetShape::Box || !p.pCollider) { continue; }
            doSpherePush([&](const KdCollider::SphereInfo& s, std::list<KdCollider::CollisionResult>* out)
            {
                return p.pCollider->Intersects(s, p.mWorld, out);
            });
        }

        // ---- 通常マップ
        const auto spMap = m_wpMap.lock();
        if (spMap)
        {
            doSpherePush([&](const KdCollider::SphereInfo& s, std::list<KdCollider::CollisionResult>* out)
            {
                return spMap->Intersects(s, out);
            });
        }
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

