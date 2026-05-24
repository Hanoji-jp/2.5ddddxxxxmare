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
    const auto spAnim = m_modelWork.GetAnimation(_name);
    if (spAnim) { m_animBlender.ChangeAnimation(spAnim, _loop, EnemyConst::AnimBlendFrames); }
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
    case AIState::Attack:  DoAttack(); break;
    default:                           break;
    }

    // アニメーション更新
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
    m_velocity.x = m_patrolRight ? EnemyConst::MoveSpeed : -EnemyConst::MoveSpeed;

    // スポーン地点から PatrolRange を超えたら折り返す
    const float dx = GetPos().x - m_spawnPos.x;
    if (dx >  EnemyConst::PatrolRange) { m_patrolRight = false; }
    if (dx < -EnemyConst::PatrolRange) { m_patrolRight = true;  }

    // 向きをセット
    m_facingDir.x = m_patrolRight ? 1.0f : -1.0f;
    m_facingDir.z = 0.0f;

    ChangeAnim("Walk");
}

void Enemy::Chase()
{
    const auto spTarget = m_wpTarget.lock();
    if (!spTarget) { return; }

    const float dx = spTarget->GetPos().x - GetPos().x;
    const float dz = spTarget->GetPos().z - GetPos().z;
    m_velocity.x = (dx > 0.0f) ?  EnemyConst::MoveSpeed : -EnemyConst::MoveSpeed;
    m_velocity.z = (dz > 0.0f) ?  EnemyConst::MoveSpeed : -EnemyConst::MoveSpeed;

    FaceTarget();
    ChangeAnim("Walk");
}
