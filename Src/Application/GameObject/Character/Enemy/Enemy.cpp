#include "../../../../Pch.h"
#include "Enemy.h"
#include "../../../../Application/Manager/ModelManager.h"

void Enemy::InitModel(const char* _path)
{
    m_hp    = EnemyConst::MaxHp;
    m_state = State::Idle;

    const auto spData = ModelManager::Instance().GetModel(_path);
    if (spData)
    {
        m_modelWork.SetModelData(spData);

        const auto idleData = m_modelWork.GetAnimation("Idle");
        if (idleData) { m_animBlender.ChangeAnimation(idleData, true, 0); }
    }
}

void Enemy::ChangeAnim(const std::string& _name, bool _loop)
{
    // 同じアニメーションなら再セットしない（リセット防止）
    if (m_currentAnimName == _name) { return; }

    const auto spAnim = m_modelWork.GetAnimation(_name);
    if (!spAnim) { return; }

    m_animBlender.ChangeAnimation(spAnim, _loop, EnemyConst::AnimBlendFrames);
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
}

void Enemy::PostUpdate()
{
    Character::PostUpdate();

    // ワールド行列を更新
    const Math::Vector3 pos   = GetPos();
    const float         scale = EnemyConst::ModelScale;
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

void Enemy::Patrol()
{
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
