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

void Enemy::CheckGround()
{
    Math::Vector3 pos = GetPos();
    if (pos.y <= 0.0f)
    {
        pos.y        = 0.0f;
        m_velocity.y = 0.0f;
        m_isGround   = true;

        // 巡回の折り返し
        if (m_aiState == AIState::Patrol)
        {
            constexpr float PatrolRange = 5.0f;
            constexpr float PatrolOriginX = 0.0f;
            if (GetPos().x >  PatrolOriginX + PatrolRange) { m_patrolRight = false; }
            if (GetPos().x <  PatrolOriginX - PatrolRange) { m_patrolRight = true;  }
        }

        SetPos(pos);
    }
    else
    {
        m_isGround = false;
    }
}
