#include "../../../Pch.h"
#include "Character.h"

void Character::Update()
{
    ApplyGravity();
}

void Character::PostUpdate()
{
    CheckWall();
    ApplyVelocity();
    CheckGround();
}

void Character::DrawDebug()
{
    if (m_pDebugWire) { m_pDebugWire->Draw(); }
}

void Character::ApplyGravity()
{
    // 惑星影響圏内かチェック（m_isGround に関係なく毎フレーム更新する）
    m_pCurrentPlanet = PlanetGravityManager::Instance().FindNearestPlanet(GetPos());

    // m_upDir は着地中も毎フレーム更新（傾き表示に使うため）
    // シリンダー重力: XY平面の輸軸から外向きになる（Z成分は無視）
    if (m_pCurrentPlanet)
    {
        const Math::Vector3 pos     = GetPos();
        const Math::Vector3 center  = m_pCurrentPlanet->Position;
        const Math::Vector2 toXY    = { pos.x - center.x, pos.y - center.y };
        const float xyDist = toXY.Length();
        if (xyDist > 0.001f)
        {
            m_upDir = { toXY.x / xyDist, toXY.y / xyDist, 0.0f };
        }
    }
    else
    {
        m_upDir = { 0.0f, 1.0f, 0.0f };
    }

    if (m_isGround) { return; }

    if (m_pCurrentPlanet)
    {
        // シリンダー軸方向への重力（XY平面のみ、Zは変えない）
        const Math::Vector3 pos     = GetPos();
        const Math::Vector3 center  = m_pCurrentPlanet->Position;
        const Math::Vector2 toXY    = { center.x - pos.x, center.y - pos.y };
        const float xyDist = toXY.Length();
        if (xyDist > 0.001f)
        {
            const Math::Vector3 gravDir = { toXY.x / xyDist, toXY.y / xyDist, 0.0f };

            const float radialVel = m_velocity.Dot(gravDir);
            const float newRadial = std::min(radialVel + PlanetConst::GravityAccel,
                                             PlanetConst::MaxFallSpeed);
            m_velocity += gravDir * (newRadial - radialVel);
        }
        // Z成分は変えない（横スクロールのZ固定を尊重）
    }
    else
    {
        // 通常重力
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

    // シリンダー重力上にいる場合はXY距離ベースで着地判定
    if (m_pCurrentPlanet)
    {
        const Math::Vector3 pos    = GetPos();
        const Math::Vector3 center = m_pCurrentPlanet->Position;
        const Math::Vector2 toXY   = { pos.x - center.x, pos.y - center.y };
        const float xyDist = toXY.Length();
        const float diff   = xyDist - m_pCurrentPlanet->SurfaceRadius;

        // ジャンプ直後（外向きに速度がある）は着地判定をスキップして浮き上がらせる
        const Math::Vector3 outDir = (xyDist > 0.001f)
            ? Math::Vector3{ toXY.x / xyDist, toXY.y / xyDist, 0.0f }
            : Math::Vector3{ 0.0f, 1.0f, 0.0f };
        const float outwardVel = m_velocity.Dot(outDir);
        if (outwardVel > 0.001f) { return; }   // 外向きに動いている = 上昇中

        if (diff <= PlanetConst::GroundSnapTolerance)
        {
            // めり込み時のみ表面にスナップ
            if (diff < 0.0f && xyDist > 0.001f)
            {
                SetPos({ center.x + outDir.x * m_pCurrentPlanet->SurfaceRadius,
                         center.y + outDir.y * m_pCurrentPlanet->SurfaceRadius,
                         pos.z });
            }

            // 中心方向（落下方向）の速度成分を減衰させる（即ゼロにせず滑らかに着地）
            const float radialVel = m_velocity.Dot(-outDir);
            if (radialVel > 0.0f)
            {
                m_velocity -= (-outDir) * radialVel * (1.0f - GameConst::LandingDamping);
            }
            m_isGround = true;
        }
        return;
    }

    // 通常マップ上のレイキャスト着地判定
    const auto spMap = m_wpMap.lock();
    if (!spMap) { return; }

    // 上方向に速度がある（ジャンプ直後）は着地判定をスキップ
    if (m_velocity.y > 0.001f) { return; }

    Math::Vector3 pos = GetPos();

    // 足元より少し上からレイを下に飛ばす
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
            }
        }
    }
}

void Character::CheckWall()
{
    const auto spMap = m_wpMap.lock();
    if (!spMap) { return; }

    Math::Vector3 pos = GetPos();

    // デバッグ：スフィアを可視化
    const Math::Vector3 sphereCenter = pos + Math::Vector3(0.0f, CollisionConst::WallSphereOffsetY, 0.0f);
    if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
    m_pDebugWire->AddDebugSphere(sphereCenter, CollisionConst::WallSphereRadius, { 1,1,0,1 });

    // 8方向 × 3高さ でレイを飛ばして壁押し返し
    static const float angles[CollisionConst::WallRayDirCount] = {
        0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f
    };
    static const float heights[3] = {
        CollisionConst::WallRayOffsetY0,
        CollisionConst::WallRayOffsetY1,
        CollisionConst::WallRayOffsetY2
    };

    // 各軸の最大押し返し量を正負それぞれで記録（反対向きレイの相殺を防ぐ）
    float pushXPos = 0.0f, pushXNeg = 0.0f;
    float pushZPos = 0.0f, pushZNeg = 0.0f;

    for (float h : heights)
    {
        const Math::Vector3 rayOrigin = pos + Math::Vector3(0.0f, h, 0.0f);

        for (float deg : angles)
        {
            const float rad = DirectX::XMConvertToRadians(deg);
            const Math::Vector3 rayDir = { std::sinf(rad), 0.0f, std::cosf(rad) };

            const KdCollider::RayInfo ray(KdCollider::TypeBump, rayOrigin, rayDir, CollisionConst::WallRayLength);

            std::list<KdCollider::CollisionResult> results;
            if (!spMap->Intersects(ray, &results)) { continue; }

            float maxOverlap = 0.0f;
            for (auto& r : results)
            {
                if (std::isfinite(r.m_overlapDistance) && r.m_overlapDistance > maxOverlap)
                    maxOverlap = r.m_overlapDistance;
            }
            if (maxOverlap <= 0.0f) { continue; }

            // レイの逆方向で押し返す
            const Math::Vector3 push = -rayDir * maxOverlap;

            // 正負を分けて最大値のみ蓄積（逆方向レイの相殺防止）
            if (push.x > 0.0f) pushXPos = std::max(pushXPos,  push.x);
            else                pushXNeg = std::min(pushXNeg,  push.x);
            if (push.z > 0.0f) pushZPos = std::max(pushZPos,  push.z);
            else                pushZNeg = std::min(pushZNeg,  push.z);
        }
    }

    const Math::Vector3 totalPush = {
        pushXPos + pushXNeg,
        0.0f,
        pushZPos + pushZNeg
    };

    if (totalPush.LengthSquared() > 0.0f)
    {
        pos += totalPush;
        SetPos(pos);
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

