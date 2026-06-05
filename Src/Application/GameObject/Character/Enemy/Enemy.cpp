#include "../../../../Pch.h"
#include "Enemy.h"
#include "../../../../Application/Manager/ModelManager.h"
#include "../../../../Application/Manager/PlanetGravityManager.h"
#include "../../../../Application/Const/CollisionConst.h"

void Enemy::InitModel(const char* _path)
{
    m_hp    = EnemyConst::MaxHp;
    m_state = State::Idle;

    const auto spData = ModelManager::Instance().GetModel(_path);
    if (spData)
    {
        m_modelWork.SetModelData(spData);

        // AnimBlender にモデルワークを登録
        m_animBlender.Init(&m_modelWork);
        m_animBlender.ChangeAnimation(std::string("Idle"), true, 0);
    }
}

void Enemy::ChangeAnim(const std::string& _name, bool _loop)
{
    // 同じアニメーションなら再セットしない（リセット防止）
    if (m_currentAnimName == _name) { return; }

    if (!m_animBlender.ChangeAnimation(_name, _loop, EnemyConst::AnimBlendFrames)) { return; }
    m_currentAnimName = _name;
}

void Enemy::FaceTarget()
{
    const auto spTarget = m_wpTarget.lock();
    if (!spTarget) { return; }

    Math::Vector3 toTarget = spTarget->GetPos() - GetPos();
    toTarget.y = 0.0f;
    if (toTarget.LengthSquared() < 1e-4f) { return; }
    toTarget.Normalize();
    m_facingDir = toTarget;
}

void Enemy::Update()
{
    if (m_attackCool > 0) { --m_attackCool; }

    const auto spTarget = m_wpTarget.lock();
    if (spTarget)
    {
        const Math::Vector3 toTarget = spTarget->GetPos() - GetPos();
        const float distSq    = toTarget.LengthSquared();
        const float searchSq  = EnemyConst::SearchRadius    * EnemyConst::SearchRadius;
        const float attackSq  = GetAttackRange()            * GetAttackRange();

        if (distSq <= attackSq)
        {
            m_aiState = AIState::Attack;
        }
        else if (distSq <= searchSq)
        {
            m_aiState = AIState::Chase;
        }
        else
        {
            m_aiState = AIState::Patrol;
        }
    }

    switch (m_aiState)
    {
    case AIState::Patrol:  Patrol();   break;
    case AIState::Chase:   Chase();    break;
    case AIState::Attack:
        // 攻撃中は減速して停止
        m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, Math::Vector3::Zero,
            EnemyConst::Deceleration / EnemyConst::MoveSpeed);
        if (m_moveVelocity.LengthSquared() < 0.0001f) { m_moveVelocity = Math::Vector3::Zero; }
        m_velocity.x = m_moveVelocity.x;
        m_velocity.z = m_moveVelocity.z;
        DoAttack();
        break;
    default: break;
    }

    // アニメーション更新
    // 移動していなければ Idle にフォールバック
    if (m_moveVelocity.LengthSquared() < 0.0001f && m_aiState != AIState::Attack)
    {
        ChangeAnim("Idle");
    }
    m_animBlender.Update(m_modelWork);

    Character::Update();

    // ── 撃破バウンス開始 ─────────────────────────────────────
    if (IsDead() && !m_deathBounceActive)
    {
        m_deathBounceActive = true;
        m_deathBounceVelY   = JuiceConst::DeathBounceVel;
        m_deathFadeAlpha    = 1.0f;
    }

    // バウンス中の位置・フェード更新
    if (m_deathBounceActive)
    {
        constexpr float kDt      = 1.0f / 60.0f;
        constexpr float kGravity = 14.0f;  // バウンス用擬似重力
        m_deathBounceVelY -= kGravity * kDt;
        Math::Vector3 pos = GetPos();
        pos.y += m_deathBounceVelY * kDt;
        SetPos(pos);

        m_deathFadeAlpha -= JuiceConst::DeathFadeSpeed * kDt;
        if (m_deathFadeAlpha <= 0.0f)
        {
            m_isExpired = true;
        }
    }
}

void Enemy::PostUpdate()
{
    Character::PostUpdate();

    // ワールド行列を更新
    const Math::Vector3 pos   = GetPos();
    const float         scale = EnemyConst::ModelScale
                              * (m_deathBounceActive ? std::max(0.0f, m_deathFadeAlpha) : 1.0f);
    const float         yaw   = std::atan2f(m_facingDir.x, m_facingDir.z);

    m_mWorld = DirectX::XMMatrixScaling(scale, scale, scale)
             * DirectX::XMMatrixRotationY(yaw)
             * DirectX::XMMatrixTranslation(pos.x, pos.y + EnemyConst::ModelOffsetY, pos.z);
}

void Enemy::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}

bool Enemy::CheckEdgeAheadDir(const Math::Vector3& fwdDir) const
{
    if (!m_isGround) { return false; }

    const Math::Vector3 pos = GetPos();

    // ── 壁チェック ─────────────────────────────────────────────
    // 前方にレイを飛ばして壁に当たったら反転
    {
        const Math::Vector3 rayOrigin = pos + m_upDir * EnemyConst::WallCheckRayUpOffset;
        const KdCollider::RayInfo wallRay(KdCollider::TypeBump, rayOrigin, fwdDir,
            EnemyConst::WallCheckRayLen);

        const auto& planets = PlanetGravityManager::Instance().GetPlanets();
        for (const auto& p : planets)
        {
            if (!p.pCollider) { continue; }
            std::list<KdCollider::CollisionResult> results;
            if (!p.pCollider->Intersects(wallRay, p.mWorld, &results)) { continue; }
            for (const auto& r : results)
            {
                if (r.m_hitNDir.Dot(fwdDir) >= 0.0f) { continue; }
                if (std::abs(r.m_hitNDir.Dot(m_upDir)) > CollisionConst::WallNormalUpThreshold) { continue; }
                return true; // 壁あり
            }
        }

        const auto spMap = m_wpMap.lock();
        if (spMap)
        {
            std::list<KdCollider::CollisionResult> results;
            if (spMap->Intersects(wallRay, &results))
            {
                for (const auto& r : results)
                {
                    if (r.m_hitNDir.Dot(fwdDir) >= 0.0f) { continue; }
                    if (std::abs(r.m_hitNDir.Dot(m_upDir)) > CollisionConst::WallNormalUpThreshold) { continue; }
                    return true; // 壁あり
                }
            }
        }
    }

    // ── 崖チェック ─────────────────────────────────────────────
    // 一歩先の足元に地面がなければ崖
    {
        const Math::Vector3 probeBase = pos + fwdDir * EnemyConst::EdgeCheckStepDist
                                            + m_upDir * EnemyConst::EdgeCheckRayUpOffset;
        const KdCollider::RayInfo groundRay(KdCollider::TypeGround, probeBase, -m_upDir,
            EnemyConst::EdgeCheckGroundLen);

        bool foundGround = false;

        const auto& planets = PlanetGravityManager::Instance().GetPlanets();
        for (const auto& p : planets)
        {
            if (!p.pCollider) { continue; }
            std::list<KdCollider::CollisionResult> results;
            if (p.pCollider->Intersects(groundRay, p.mWorld, &results))
            {
                if (!results.empty()) { foundGround = true; break; }
            }
        }

        if (!foundGround)
        {
            const auto spMap = m_wpMap.lock();
            if (spMap)
            {
                std::list<KdCollider::CollisionResult> results;
                if (spMap->Intersects(groundRay, &results) && !results.empty())
                    foundGround = true;
            }
        }

        if (!foundGround) { return true; } // 崖あり
    }

    return false;
}

bool Enemy::CheckEdgeAhead() const
{
    const Math::Vector3 fwdDir = { m_patrolRight ? 1.0f : -1.0f, 0.0f, 0.0f };
    return CheckEdgeAheadDir(fwdDir);
}

void Enemy::Patrol()
{
    // 崖・壁検知：着地中かつ前方に障害がある場合は即反転
    if (m_useEdgeDetection && CheckEdgeAhead())
    {
        m_patrolRight = !m_patrolRight;
    }

    // スポーン地点から PatrolRange を超えたら折り返す
    const float dx = GetPos().x - m_spawnPos.x;
    if (dx >  EnemyConst::PatrolRange) { m_patrolRight = false; }
    if (dx < -EnemyConst::PatrolRange) { m_patrolRight = true;  }

    // 目標速度を決めて Lerp で加速
    const float targetX = m_patrolRight ? EnemyConst::MoveSpeed : -EnemyConst::MoveSpeed;
    const Math::Vector3 targetVel = { targetX, 0.0f, 0.0f };
    m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, targetVel,
        EnemyConst::Acceleration / EnemyConst::MoveSpeed);

    // Slerp で向きを補間
    const Math::Vector3 targetDir = { targetX > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
    const Math::Quaternion fromRot = Math::Quaternion::CreateFromYawPitchRoll(
        std::atan2f(m_facingDir.x, m_facingDir.z), 0.0f, 0.0f);
    const Math::Quaternion toRot = Math::Quaternion::CreateFromYawPitchRoll(
        std::atan2f(targetDir.x, targetDir.z), 0.0f, 0.0f);
    const Math::Quaternion blendRot = Math::Quaternion::Slerp(fromRot, toRot, EnemyConst::RotationSpeed);
    m_facingDir = Math::Vector3::Transform({ 0.0f, 0.0f, 1.0f },
        Math::Matrix::CreateFromQuaternion(blendRot));
    m_facingDir.y = 0.0f;

    m_velocity.x = m_moveVelocity.x;
    m_velocity.z = m_moveVelocity.z;

    ChangeAnim("Walk");
}

void Enemy::Chase()
{
    const auto spTarget = m_wpTarget.lock();
    if (!spTarget) { return; }

    Math::Vector3 toTarget = spTarget->GetPos() - GetPos();
    toTarget.y = 0.0f;
    if (toTarget.LengthSquared() < 1e-4f) { return; }
    toTarget.Normalize();

    // 崖・壁チェック：追跡方向の前方に障害があれば追跡を諦めて停止
    if (m_useEdgeDetection && CheckEdgeAheadDir(toTarget))
    {
        // 速度を減速させて止まる（崖に落ちない）
        m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, Math::Vector3::Zero,
            EnemyConst::Deceleration / EnemyConst::MoveSpeed);
        if (m_moveVelocity.LengthSquared() < 0.0001f) { m_moveVelocity = Math::Vector3::Zero; }
        m_velocity.x = m_moveVelocity.x;
        m_velocity.z = m_moveVelocity.z;
        ChangeAnim("Idle");
        return;
    }

    // 目標速度へ Lerp で加速
    const Math::Vector3 targetVel = toTarget * EnemyConst::MoveSpeed;
    m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, targetVel,
        EnemyConst::Acceleration / EnemyConst::MoveSpeed);

    m_velocity.x = m_moveVelocity.x;
    m_velocity.z = m_moveVelocity.z;

    // Slerp で向きを補間
    const Math::Quaternion fromRot = Math::Quaternion::CreateFromYawPitchRoll(
        std::atan2f(m_facingDir.x, m_facingDir.z), 0.0f, 0.0f);
    const Math::Quaternion toRot = Math::Quaternion::CreateFromYawPitchRoll(
        std::atan2f(toTarget.x, toTarget.z), 0.0f, 0.0f);
    const Math::Quaternion blendRot = Math::Quaternion::Slerp(fromRot, toRot, EnemyConst::RotationSpeed);
    m_facingDir = Math::Vector3::Transform({ 0.0f, 0.0f, 1.0f },
        Math::Matrix::CreateFromQuaternion(blendRot));
    m_facingDir.y = 0.0f;

    ChangeAnim("Walk");
}
