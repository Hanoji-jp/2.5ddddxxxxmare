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
    if (m_isGround) { return; }

    m_velocity.y += GameConst::Gravity;

    if (m_velocity.y < GameConst::MaxFallSpeed)
    {
        m_velocity.y = GameConst::MaxFallSpeed;
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

    const auto spMap = m_wpMap.lock();
    if (!spMap) { return; }

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
            // 着地Y座標にスナップ
            pos.y = pBest->m_hitPos.y;
            SetPos(pos);
            m_velocity.y = 0.0f;
            m_isGround   = true;
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

