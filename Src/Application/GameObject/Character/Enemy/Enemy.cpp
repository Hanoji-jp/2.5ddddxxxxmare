#include "../../../../Pch.h"
#include "Enemy.h"

void Enemy::Init()
{
    m_hp    = EnemyConst::MaxHp;
    m_state = State::Idle;
    SetScale(Math::Vector3(EnemyConst::ModelScale, EnemyConst::ModelScale, EnemyConst::ModelScale));

    m_drawType = eDrawTypeLit;
}

void Enemy::Update()
{
    auto spTarget = m_wpTarget.lock();

    if (spTarget)
    {
        const Math::Vector3 toTarget = spTarget->GetPos() - GetPos();
        const float distSq = toTarget.LengthSquared();
        const float searchSq = EnemyConst::SearchRadius * EnemyConst::SearchRadius;
        const float attackSq = EnemyConst::AttackRange  * EnemyConst::AttackRange;

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
    case AIState::Patrol: Patrol(); break;
    case AIState::Chase:  Chase();  break;
    default:                        break;
    }

    Character::Update();
    CheckGround();
}

void Enemy::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}

void Enemy::Patrol()
{
    if (m_patrolRight)
    {
        m_velocity.x = EnemyConst::MoveSpeed;
    }
    else
    {
        m_velocity.x = -EnemyConst::MoveSpeed;
    }
}

void Enemy::Chase()
{
    auto spTarget = m_wpTarget.lock();
    if (!spTarget) { return; }

    const float dx = spTarget->GetPos().x - GetPos().x;
    m_velocity.x = (dx > 0.0f) ? EnemyConst::MoveSpeed : -EnemyConst::MoveSpeed;
}

