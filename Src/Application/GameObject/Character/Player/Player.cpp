#include "../../../../Pch.h"
#include "Player.h"
#include "../../../Manager/ModelManager.h"

void Player::Init()
{
    m_hp    = PlayerConst::MaxHp;
    m_state = State::Idle;

    m_drawType = eDrawTypeLit;

    // プレイヤー本体モデル
    const auto spPlayerData = ModelManager::Instance().GetModel(PlayerConst::ModelPath);
    if (spPlayerData)
    {
        m_modelWork.SetModelData(spPlayerData);
    }

    // 初期アニメーション
    ChangeAnim("Idle");

    // 剣
    m_sword = std::make_shared<Sword>();
    m_sword->Init();

    // 弓
    m_bow = std::make_shared<Bow>();
    m_bow->Init();
}

void Player::Update()
{
    Move();
    Jump();
    AttackMelee();
    AttackRanged();
    Character::Update();

    // プレイヤー座標をログ出力
    const Math::Vector3 pos = GetPos();
    KdDebugGUI::Instance().AddLog("Player: x=%.2f  y=%.2f  z=%.2f\n", pos.x, pos.y, pos.z);

    // クールダウン更新
    if (m_meleeCooldown  > 0) { --m_meleeCooldown; }
    if (m_rangedCooldown > 0) { --m_rangedCooldown; }

    // 状態に応じてアニメーション切り替え
    switch (m_state)
    {
    case State::Idle:   ChangeAnim("Idle", true);  break;
    case State::Walk:
        // 移動速度が十分あるときだけWalk再生（歩き始め・終わりを滑らかに）
        if (m_moveVelocity.LengthSquared() > 0.0001f)
            ChangeAnim("Walk", true);
        else
            ChangeAnim("Idle", true);
        break;
    case State::Jump:   ChangeAnim("Jump",   false); break;
    case State::Fall:   ChangeAnim("Fall",   true);  break;
    case State::Attack: ChangeAnim("Attack", false); break;
    case State::Dead:   ChangeAnim("Dead",   false); break;
    default: break;
    }

    // アニメーション更新
    m_animBlender.Update(m_modelWork);
}

void Player::PostUpdate()
{
    // 親クラスのPostUpdate（位置反映）を先に実行
    Character::PostUpdate();

    // 位置確定後にワールド行列を再構築
    {
        const Math::Vector3 pos   = GetPos();
        const float         scale = PlayerConst::ModelScale;
        const float         yaw   = std::atan2f(m_facingDir.x, m_facingDir.z);

        // 論理位置（コリジョン基準）はオフセットなし
        m_mWorld = DirectX::XMMatrixScaling(scale, scale, scale)
                 * DirectX::XMMatrixRotationY(yaw)
                 * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

        // 描画専用行列にだけピボット補正オフセットを乗せる
        m_drawWorld = DirectX::XMMatrixScaling(scale, scale, scale)
                    * DirectX::XMMatrixRotationY(yaw)
                    * DirectX::XMMatrixTranslation(pos.x, pos.y + PlayerConst::ModelOffsetY, pos.z);
    }

    // 消滅した矢を除去
    m_arrows.erase(
        std::remove_if(m_arrows.begin(), m_arrows.end(),
            [](const std::shared_ptr<Arrow>& a) { return a->IsExpired(); }),
        m_arrows.end());

    // 飛翔中の矢を更新
    for (const auto& arrow : m_arrows)
    {
        arrow->Update();
    }
}

void Player::DrawLit()
{
    if (m_modelWork.IsEnable())
    {
        KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_drawWorld);
    }
}

void Player::Move()
{
    // 入力取得
    Math::Vector3 input = { 0.0f, 0.0f, 0.0f };

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { input.x += 1.0f; }
    if (GetAsyncKeyState(VK_LEFT)  & 0x8000) { input.x -= 1.0f; }
    if (GetAsyncKeyState(VK_UP)    & 0x8000) { input.z += 1.0f; }
    if (GetAsyncKeyState(VK_DOWN)  & 0x8000) { input.z -= 1.0f; }

    if (input.LengthSquared() > 0.0f)
    {
        // 入力方向に向かって加速
        input = DirectX::XMVector3Normalize(input);
        const Math::Vector3 targetVel = input * PlayerConst::MoveSpeed;

        m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, targetVel, PlayerConst::Acceleration / PlayerConst::MoveSpeed);

        // 向きをSlerpで滑らかに補間
        const Math::Quaternion fromRot = Math::Quaternion::CreateFromYawPitchRoll(
            std::atan2f(m_facingDir.x, m_facingDir.z), 0.0f, 0.0f);
        const Math::Quaternion toRot = Math::Quaternion::CreateFromYawPitchRoll(
            std::atan2f(input.x, input.z), 0.0f, 0.0f);
        const Math::Quaternion blendRot = Math::Quaternion::Slerp(fromRot, toRot, PlayerConst::RotationSpeed);

        // 補間した向きをfacingDirに反映
        m_facingDir = Math::Vector3::Transform({ 0.0f, 0.0f, 1.0f },
            Math::Matrix::CreateFromQuaternion(blendRot));
        m_facingDir.y = 0.0f;
        m_state     = State::Walk;
    }
    else
    {
        // 入力なし：減速して止まる
        m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, Math::Vector3::Zero, PlayerConst::Deceleration / PlayerConst::MoveSpeed);

        // 十分遅くなったら完全停止
        if (m_moveVelocity.LengthSquared() < 0.0001f)
        {
            m_moveVelocity = Math::Vector3::Zero;
        }

        if (m_isGround) { m_state = State::Idle; }
    }

    // 慣性速度をvelocityのXZに反映
    m_velocity.x = m_moveVelocity.x;
    m_velocity.z = m_moveVelocity.z;
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

void Player::AttackMelee()
{
    if (m_meleeCooldown > 0) { return; }

    // Zキーで近接攻撃
    if (GetAsyncKeyState('Z') & 0x8000)
    {
        m_meleeCooldown = PlayerConst::MeleeCooldown;
        m_state         = State::Attack;
    }
}

void Player::AttackRanged()
{
    if (m_rangedCooldown > 0) { return; }

    // Xキーで矢を発射
    if (GetAsyncKeyState('X') & 0x8000)
    {
        m_rangedCooldown = PlayerConst::RangedCooldown;

        const Math::Vector3 spawnPos = GetPos() + Math::Vector3(0.0f, PlayerConst::ArrowOffsetY, 0.0f);

        const auto arrow = std::make_shared<Arrow>();
        arrow->Init();
        arrow->Launch(spawnPos, m_facingDir);
        m_arrows.push_back(arrow);
    }
}

void Player::ChangeAnim(const std::string& _animName, bool _isLoop)
{
    // 同じアニメーションなら再セットしない
    if (m_currentAnimName == _animName) { return; }

    const auto& spData = m_modelWork.GetData();
    if (!spData) { return; }

    const auto spAnim = spData->GetAnimation(_animName);
    if (!spAnim) { return; }

    m_animBlender.ChangeAnimation(spAnim, _isLoop, PlayerConst::AnimBlendFrames);
    m_currentAnimName = _animName;
}
