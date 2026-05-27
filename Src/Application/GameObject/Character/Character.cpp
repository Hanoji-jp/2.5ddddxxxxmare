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
        CheckGround();
    }

    CheckWall();
}

void Character::DrawDebug()
{
    if (m_pDebugWire) { m_pDebugWire->Draw(); }
}

void Character::ApplyGravity()
{
    // 手動重力モードの処理
    if (m_manualGravityDir != ManualGravityDir::None)
    {
        // 手動重力モード中でも惑星情報を取得（着地判定用）
        const GravityInfluenceResult gravResult = PlanetGravityManager::Instance().ComputeGravityInfluence(GetPos());
        m_currentPlanetIndex = gravResult.dominantPlanetIdx;
        m_pCurrentPlanet     = PlanetGravityManager::Instance().GetPlanet(m_currentPlanetIndex);

        // Sphere惑星の重力圏に入ったら、自動的に惑星重力モードに切り替え
        if (m_pCurrentPlanet && m_pCurrentPlanet->Shape == PlanetShape::Sphere && gravResult.hasInfluence)
        {
            m_manualGravityDir = ManualGravityDir::None;
            // 以下、通常の惑星重力処理を実行（fall through）
        }
        else
        {
            // 手動重力方向を決定
            Math::Vector3 gravDir = { 0.0f, -1.0f, 0.0f };
            Math::Vector3 targetUp = { 0.0f, 1.0f, 0.0f };

            switch (m_manualGravityDir)
            {
            case ManualGravityDir::Down:
                gravDir = { 0.0f, -1.0f, 0.0f };
                targetUp = { 0.0f, 1.0f, 0.0f };
                break;
            case ManualGravityDir::Up:
                gravDir = { 0.0f, 1.0f, 0.0f };
                targetUp = { 0.0f, -1.0f, 0.0f };
                break;
            case ManualGravityDir::Left:
                gravDir = { -1.0f, 0.0f, 0.0f };
                targetUp = { 1.0f, 0.0f, 0.0f };
                break;
            case ManualGravityDir::Right:
                gravDir = { 1.0f, 0.0f, 0.0f };
                targetUp = { -1.0f, 0.0f, 0.0f };
                break;
            }

            // 物理用upDirは即スナップ（壁判定・重力計算に使用）
            m_upDir = targetUp;

            // 見た目用upDirVisualはSlerpで補間（モデル回転に使用）
            const Math::Quaternion fromQ  = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
            const Math::Quaternion toQ    = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
            const Math::Quaternion blendQ = Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed);
            m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f }, Math::Matrix::CreateFromQuaternion(blendQ));
            m_upDirVisual.Normalize();

            // 地面に接地している場合は重力加速を適用しない
            if (m_isGround) { return; }

            // 手動重力方向に加速
            const float radialVel = m_velocity.Dot(gravDir);
            const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
            if (newRadial > radialVel)
            {
                m_velocity += gravDir * (newRadial - radialVel);
            }

            return; // 手動モードでは惑星重力を無視
        }
    }

    // 以下、通常の惑星重力モード
    // 重力合成結果を取得
    const GravityInfluenceResult gravResult = PlanetGravityManager::Instance().ComputeGravityInfluence(GetPos());

    // 最も影響力が強い惑星を保持（着地判定用）
    m_currentPlanetIndex = gravResult.dominantPlanetIdx;
    m_pCurrentPlanet     = PlanetGravityManager::Instance().GetPlanet(m_currentPlanetIndex);

    // m_upDir（物理用）は即スナップ、m_upDirVisual（見た目用）はSlerp
    Math::Vector3 targetUp = gravResult.dominantUpDir;
    {
        m_upDir = targetUp;  // 物理用は即確定

        const Math::Quaternion fromQ  = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, m_upDirVisual);
        const Math::Quaternion toQ    = Math::Quaternion::FromToRotation({ 0.0f, 1.0f, 0.0f }, targetUp);
        const Math::Quaternion blendQ = Math::Quaternion::Slerp(fromQ, toQ, PlanetConst::UpDirSlerpSpeed);
        m_upDirVisual = Math::Vector3::Transform({ 0.0f, 1.0f, 0.0f }, Math::Matrix::CreateFromQuaternion(blendQ));
        m_upDirVisual.Normalize();
    }

    // 地面に接地している場合は重力加速を適用しない
    if (m_isGround) { return; }

    // 合成重力方向に加速
    if (gravResult.hasInfluence)
    {
        const Math::Vector3 gravDir = gravResult.totalGravityDir;
        const float radialVel = m_velocity.Dot(gravDir);
        const float newRadial = std::min(radialVel + PlanetConst::GravityAccel, PlanetConst::MaxFallSpeed);
        if (newRadial > radialVel)
        {
            m_velocity += gravDir * (newRadial - radialVel);
        }
    }
    else
    {
        // 惑星の影響がない場合は通常の下方向重力
        m_velocity.y += GameConst::Gravity;
        if (m_velocity.y < GameConst::MaxFallSpeed)
        {
            m_velocity.y = GameConst::MaxFallSpeed;
        }
    }
}

void Character::ApplyVelocity()
{
    Math::Vector3 pos = GetPos();
    pos += m_velocity;
    SetPos(pos);
}

void Character::CheckGround()
{
    m_isGround = false;

    // 惑星重力下にいる場合は着地判定
    if (m_pCurrentPlanet)
    {
        // ---- Box 惑星の着地判定（AABB 面スナップ） ----
        if (m_pCurrentPlanet->Shape == PlanetShape::Box)
        {
            const Math::Vector3 pos   = GetPos();
            const Math::Vector3 lp    = pos - m_pCurrentPlanet->Position;
            const Math::Vector3& half = m_pCurrentPlanet->BoxHalfExtents;
            const float penX = half.x - std::abs(lp.x);
            const float penY = half.y - std::abs(lp.y);

            // 最近傍面の外向き法線
            Math::Vector3 outDir;
            if (penX < penY)
                outDir = { (lp.x >= 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f };
            else
                outDir = { 0.0f, (lp.y >= 0.0f ? 1.0f : -1.0f), 0.0f };

            // outDir が m_upDir と向きが一致しない面は「壁」→ CheckWall に任せてスキップ
            // 内積 < 0.7 なら地面ではなく壁とみなす
            constexpr float kGroundDotThreshold = 0.7f;
            if (outDir.Dot(m_upDir) < kGroundDotThreshold) { return; }

            // ジャンプ直後はスキップ
            if (m_velocity.Dot(outDir) > 0.001f) { return; }

            // プレイヤーが面にどれだけ近いか（内部なら正、外部なら負）
            const float distToFace = (outDir.x != 0.0f)
                ? (half.x - std::abs(lp.x))
                : (half.y - std::abs(lp.y));

            // 面の外側～内側 GroundSnapDist の範囲なら着地
            // 外側: distToFace < 0 で abs(distToFace) < GroundSnapDist
            // 内側: distToFace > 0 で常に着地可能とする
            const bool nearFace = (distToFace >= 0.0f) || (distToFace > -CollisionConst::GroundSnapDist);

            if (nearFace)
            {
                // 面上にスナップ
                Math::Vector3 corrected = pos;
                if (outDir.x != 0.0f)
                    corrected.x = m_pCurrentPlanet->Position.x + outDir.x * (half.x + 0.001f);
                else
                    corrected.y = m_pCurrentPlanet->Position.y + outDir.y * (half.y + 0.001f);
                SetPos(corrected);

                const float inwardVel = m_velocity.Dot(-outDir);
                if (inwardVel > 0.0f)
                {
                    m_velocity += outDir * inwardVel * GameConst::LandingDamping;
                }
                m_isGround = true;
                m_airGravitySwitchCount = 0;  // 着地で空中切り替え回数リセット
            }
            return;
        }

        // ---- Sphere 惑星の着地判定（既存レイキャスト） ----
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
            m_isGround = true;
            m_airGravitySwitchCount = 0;  // 着地で空中切り替え回数リセット
        }

        return;
    }

    // 通常マップ上のレイキャスト着地判定
    const auto spMap = m_wpMap.lock();
    if (!spMap) { return; }

    if (m_velocity.y > 0.001f) { return; }

    Math::Vector3 pos = GetPos();

    const Math::Vector3 rayStart = pos + Math::Vector3(0.0f, CollisionConst::GroundRayOffset, 0.0f);
    const Math::Vector3 rayDir   = Math::Vector3(0.0f, -1.0f, 0.0f);

    const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, rayDir, CollisionConst::GroundRayLength);

    std::list<KdCollider::CollisionResult> results;
    if (spMap->Intersects(ray, &results))
    {
        // 最も近い床を選ぶ
        const KdCollider::CollisionResult* pBest = nullptr;
        for (auto& r : results)
        {
            if (!pBest || r.m_overlapDistance > pBest->m_overlapDistance)
            {
                pBest = &r;
            }
        }

        if (pBest)
        {
            const float floorY      = pBest->m_hitPos.y;
            const float penetration = floorY - pos.y;   // 正 = めり込み、負 = 床より上

            // 着地許容範囲内（めり込み〜SnapDist 浮いている）
            if (penetration > -CollisionConst::GroundSnapDist)
            {
                // 常に床にスナップ（浮いたまま着地フラグだけ立つのを防ぐ）
                pos.y = floorY;
                SetPos(pos);
                // 落下速度を減衰させる
                if (m_velocity.y < 0.0f)
                {
                    m_velocity.y *= GameConst::LandingDamping;
                    if (std::abs(m_velocity.y) < 0.001f) { m_velocity.y = 0.0f; }
                }
                m_isGround = true;
                m_airGravitySwitchCount = 0;  // 着地で空中切り替え回数リセット
            }
        }
    }
}

void Character::CheckWall()
{
    Math::Vector3 pos = GetPos();

    // upDir に垂直な2軸（right, forward）を構成
    const Math::Vector3 worldFwd = (std::abs(m_upDir.z) < 0.9f)
        ? Math::Vector3{ 0.0f, 0.0f, 1.0f }
        : Math::Vector3{ 1.0f, 0.0f, 0.0f };
    Math::Vector3 rightAxis;
    m_upDir.Cross(worldFwd, rightAxis);
    rightAxis.Normalize();
    Math::Vector3 fwdAxis;
    rightAxis.Cross(m_upDir, fwdAxis);
    fwdAxis.Normalize();

    // 4軸方向のみ（斜めは不要：合成で補正される）
    static const Math::Vector2 dirs[4] = {
        { 1.0f,  0.0f},
        {-1.0f,  0.0f},
        { 0.0f,  1.0f},
        { 0.0f, -1.0f},
    };
    static const float heights[3] = {
        CollisionConst::WallRayOffsetY0,
        CollisionConst::WallRayOffsetY1,
        CollisionConst::WallRayOffsetY2,
    };

    // レイ対象：Box惑星コライダー or 通常マップ
    // ※手動重力時は m_pCurrentPlanet が更新されていない場合があるので直接取得する
    const PlanetData* pWallPlanet = m_pCurrentPlanet;
    if (!pWallPlanet)
    {
        const GravityInfluenceResult r = PlanetGravityManager::Instance().ComputeGravityInfluence(GetPos());
        pWallPlanet = PlanetGravityManager::Instance().GetPlanet(r.dominantPlanetIdx);
    }

    const bool hasPlanetCol = pWallPlanet
        && pWallPlanet->Shape == PlanetShape::Box
        && pWallPlanet->pCollider;
    const auto spMap = m_wpMap.lock();

    if (!hasPlanetCol && !spMap) { return; }

    // デバッグ
    if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
    m_pDebugWire->AddDebugSphere(pos + m_upDir * CollisionConst::WallSphereOffsetY,
                                 CollisionConst::WallSphereRadius, { 1,1,0,1 });

    float pushRightMax = 0.0f, pushRightMin = 0.0f;
    float pushFwdMax   = 0.0f, pushFwdMin   = 0.0f;

    for (float h : heights)
    {
        const Math::Vector3 rayOrigin = pos + m_upDir * h;

        for (const Math::Vector2& d : dirs)
        {
            const Math::Vector3 rayDir = rightAxis * d.x + fwdAxis * d.y;
            const KdCollider::RayInfo ray(KdCollider::TypeBump, rayOrigin, rayDir, CollisionConst::WallRayLength);

            std::list<KdCollider::CollisionResult> results;
            bool hit = false;

            if (hasPlanetCol)
                hit = pWallPlanet->pCollider->Intersects(ray, pWallPlanet->mWorld, &results);
            else if (spMap)
                hit = spMap->Intersects(ray, &results);

            if (!hit) { continue; }

            float maxOverlap = 0.0f;
            for (auto& r : results)
            {
                if (!std::isfinite(r.m_overlapDistance) || r.m_overlapDistance <= 0.0f) { continue; }
                if (r.m_overlapDistance > maxOverlap)
                    maxOverlap = r.m_overlapDistance;
            }
            if (maxOverlap <= 0.0f) { continue; }

            // m_overlapDistance = WallRayLength - ヒット距離 = そのまま押し出し量
            const float pushDist = maxOverlap;
            if (pushDist <= 0.0f) { continue; }

            const Math::Vector3 push = -rayDir * pushDist;
            const float dr = push.Dot(rightAxis);
            const float df = push.Dot(fwdAxis);

            if (dr > 0.0f) { pushRightMax = std::max(pushRightMax, dr); }
            else           { pushRightMin = std::min(pushRightMin, dr); }
            if (df > 0.0f) { pushFwdMax   = std::max(pushFwdMax,   df); }
            else           { pushFwdMin   = std::min(pushFwdMin,   df); }
        }
    }

    const Math::Vector3 totalPush =
        rightAxis * (pushRightMax + pushRightMin) +
        fwdAxis   * (pushFwdMax   + pushFwdMin);

    if (totalPush.LengthSquared() > 0.0f)
    {
        // 完全に押し出す
        pos += totalPush;
        SetPos(pos);

        // 壁方向へのvelocityを完全にカット
        const float pushLen = totalPush.Length();
        if (pushLen > 0.0001f)
        {
            const Math::Vector3 pushNorm = totalPush / pushLen;
            const float velIntoWall = m_velocity.Dot(-pushNorm);
            if (velIntoWall > 0.0f)
            {
                m_velocity += pushNorm * velIntoWall;
            }
        }
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

