#include "../../../../Pch.h"
#include "Player.h"

void Player::Init()
{
    m_hp    = PlayerConst::MaxHp;
    m_state = State::Idle;
    SetScale(Math::Vector3(PlayerConst::ModelScale, PlayerConst::ModelScale, PlayerConst::ModelScale));

    m_drawType = eDrawTypeLit;
}

void Player::Update()
{
    Move();
    Jump();
    Character::Update();
    CheckGround();
}

void Player::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}

void Player::Move()
{
    m_velocity.x = 0.0f;

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        m_velocity.x = PlayerConst::MoveSpeed;
        m_state = State::Walk;
    }
    else if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        m_velocity.x = -PlayerConst::MoveSpeed;
        m_state = State::Walk;
    }
    else
    {
        if (m_isGround) { m_state = State::Idle; }
    }
}

void Player::Jump()
{
    if (m_isGround && (GetAsyncKeyState(VK_SPACE) & 0x8000))
    {
        m_velocity.y = PlayerConst::JumpPower;
        m_isGround   = false;
        m_state      = State::Jump;
    }
}

void Player::CheckGround()
{
    // 簡易着地判定：Y座標が0以下になったら地面とみなす
    Math::Vector3 pos = GetPos();
    if (pos.y <= 0.0f)
    {
        pos.y      = 0.0f;
        m_velocity.y = 0.0f;
        m_isGround = true;
        SetPos(pos);
    }
    else
    {
        m_isGround = false;
    }
}
