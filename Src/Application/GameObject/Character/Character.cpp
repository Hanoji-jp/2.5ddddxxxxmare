#include "../../../Pch.h"
#include "Character.h"

void Character::Update()
{
    ApplyGravity();
}

void Character::PostUpdate()
{
    ApplyVelocity();
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
    pos.z = GameConst::FixedZ;
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
