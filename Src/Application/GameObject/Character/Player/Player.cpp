#include "../../../../Pch.h"
#include "Player.h"
#include "../../../Manager/ModelManager.h"
#include "../../../Manager/ManualGravityZoneManager.h"

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
    // 手動重力ゾーンチェック
    const bool canUseManualGravity = ManualGravityZoneManager::Instance().CanUseManualGravity(GetPos());

    // 重力切り替え（矢印キー）- ゾーン内でのみ有効、空中では1回まで
    if (canUseManualGravity)
    {
        // 空中制限チェック：地上 or 空中1回まで
        const bool canSwitch = m_isGround || CanSwitchGravityInAir();

        if (canSwitch && (GetAsyncKeyState(VK_DOWN) & 0x8000))  // ↓キー
        {
            if (GetManualGravity() != ManualGravityDir::Down)
            {
                SetManualGravity(ManualGravityDir::Down);
                if (!m_isGround) { ConsumeAirGravitySwitch(); }
            }
        }
        else if (canSwitch && (GetAsyncKeyState(VK_UP) & 0x8000))  // ↑キー
        {
            if (GetManualGravity() != ManualGravityDir::Up)
            {
                SetManualGravity(ManualGravityDir::Up);
                if (!m_isGround) { ConsumeAirGravitySwitch(); }
            }
        }
        else if (canSwitch && (GetAsyncKeyState(VK_LEFT) & 0x8000))  // ←キー
        {
            if (GetManualGravity() != ManualGravityDir::Left)
            {
                SetManualGravity(ManualGravityDir::Left);
                if (!m_isGround) { ConsumeAirGravitySwitch(); }
            }
        }
        else if (canSwitch && (GetAsyncKeyState(VK_RIGHT) & 0x8000))  // →キー
        {
            if (GetManualGravity() != ManualGravityDir::Right)
            {
                SetManualGravity(ManualGravityDir::Right);
                if (!m_isGround) { ConsumeAirGravitySwitch(); }
            }
        }
        // Rキーで自動モードに戻す（空中制限なし）
        else if (GetAsyncKeyState('R') & 0x8000)
        {
            SetManualGravity(ManualGravityDir::None);
        }
    }
    else
    {
        // ゾーン外では手動重力を無効化
        if (GetManualGravity() != ManualGravityDir::None)
        {
            SetManualGravity(ManualGravityDir::None);
        }
    }

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
    case State::Jump:
        // Jumpアニメがない場合は Idle で代用
        if (!ChangeAnimIfExist("Jump", false)) { ChangeAnim("Idle", true); }
        break;
    case State::Fall:
        // Fallアニメがない場合は Idle で代用
        if (!ChangeAnimIfExist("Fall", true)) { ChangeAnim("Idle", true); }
        break;
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
        const Math::Vector3 up    = GetUpDir();   // 惑星表面の法線（XY平面、Zは0）

        // ----- 正規直交基底を構築（行列式 = +1 を保証） -----
        // forward: モデルがデフォルトで向く軸（＋Z方向）に対応させる
        //   円周上の接線 = (-up.y, up.x, 0)。左向き（-X側）を正とし
        //   m_facingDir.x の符号で反転する
        const Math::Vector3 tangent = { -up.y, up.x, 0.0f };
        // m_facingSign は接線方向に対する左右符号（+1/-1）
        // up が反転しても tangent との関係で向きが決まるため歪みが起きない
        const float         fSign   = m_facingSign;

        // モデルのローカル Z（前方）→ ワールドの tangent 方向へ
        const Math::Vector3 modelFwd = tangent * fSign;           // forward 軸
        // モデルのローカル Y（上方）→ ワールドの up 方向へ
        const Math::Vector3 modelUp  = up;                        // up 軸
        // モデルのローカル X（右方）→ cross(up, forward) で確定
        //   det = +1 になるよう cross の順序を合わせる
        Math::Vector3 modelRight;
        modelUp.Cross(modelFwd, modelRight);
        modelRight.Normalize();

        // DirectX 行メジャー行列（行ベクトル用）:
        //   行0 = Right, 行1 = Up, 行2 = Forward
        const Math::Matrix rot(
            modelRight.x, modelRight.y, modelRight.z, 0.0f,
            modelUp.x,    modelUp.y,    modelUp.z,    0.0f,
            modelFwd.x,   modelFwd.y,   modelFwd.z,   0.0f,
            0.0f,         0.0f,         0.0f,          1.0f);

        const Math::Matrix scaleMat = DirectX::XMMatrixScaling(scale, scale, scale);
        const Math::Matrix transMat = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
        const Math::Matrix transDrawMat = DirectX::XMMatrixTranslation(
            pos.x + up.x * PlayerConst::ModelOffsetY,
            pos.y + up.y * PlayerConst::ModelOffsetY,
            pos.z + up.z * PlayerConst::ModelOffsetY);

        m_mWorld    = scaleMat * rot * transMat;
        m_drawWorld = scaleMat * rot * transDrawMat;
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

    // 矢の描画
    for (const auto& arrow : m_arrows)
    {
        arrow->DrawLit();
    }
}

void Player::Move()
{
    // 入力取得（Z軸移動なし）
    Math::Vector3 input = { 0.0f, 0.0f, 0.0f };

    if (GetAsyncKeyState('D') & 0x8000) { input.x -= 1.0f; }  // D = 右
    if (GetAsyncKeyState('A') & 0x8000) { input.x += 1.0f; }  // A = 左
    if (GetAsyncKeyState('W') & 0x8000) { input.y += 1.0f; }  // W = 上
    if (GetAsyncKeyState('S') & 0x8000) { input.y -= 1.0f; }  // S = 下

    if (input.LengthSquared() > 0.0f)
    {
        input = DirectX::XMVector3Normalize(input);

        const Math::Vector3 up = GetUpDir();   // XY平面内の法線（Z=0）

        // up に直交する接線方向を求める（XY平面内、Z固定）
        // up = (ux, uy, 0) なら tangent = (-uy, ux, 0)
        const Math::Vector3 tangent = { -up.y, up.x, 0.0f };

        // 入力の XY を接線・法線ベースに変換
        Math::Vector3 worldInput = tangent * input.x + up * input.y;
        // up 成分（radial 方向）を除去して接線のみにする
        worldInput -= up * worldInput.Dot(up);

        if (worldInput.LengthSquared() > 0.0001f)
        {
            worldInput = DirectX::XMVector3Normalize(worldInput);

            const Math::Vector3 targetVel = worldInput * PlayerConst::MoveSpeed;
            m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, targetVel,
                PlayerConst::Acceleration / PlayerConst::MoveSpeed);

            // 向き更新: tangent とのドット積で左右符号を即時決定する
            // Lerp+スナップは初期値(±1)から抜け出せなくなるバグがあるため使わない
            m_facingSign = (tangent.Dot(worldInput) >= 0.0f) ? 1.0f : -1.0f;
        }

        m_state = State::Walk;
    }
    else
    {
        m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, Math::Vector3::Zero,
            PlayerConst::Deceleration / PlayerConst::MoveSpeed);
        if (m_moveVelocity.LengthSquared() < 0.0001f)
        {
            m_moveVelocity = Math::Vector3::Zero;
        }
        if (m_isGround) { m_state = State::Idle; }
        else if (m_state != State::Jump) { m_state = State::Fall; }
    }

    // velocity に反映（radial 成分は保持、接線成分だけ置き換え）
    const Math::Vector3 up = GetUpDir();
    Math::Vector3 surfaceVel = m_moveVelocity;
    surfaceVel -= up * surfaceVel.Dot(up);

    const float radialVel = m_velocity.Dot(up);
    m_velocity = surfaceVel + up * radialVel;
    m_velocity.z = 0.0f;   // Z は常に固定
}

void Player::Jump()
{
    if (m_isGround && (GetAsyncKeyState(VK_SPACE) & 0x8000))
    {
        // 惑星上なら法線方向（上方向）へジャンプ
        const Math::Vector3 jumpVec = GetUpDir() * PlayerConst::JumpPower;
        m_velocity += jumpVec;
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
        // 矢の発射方向: 現在の up から接線を求め m_facingSign で向きを決める
        const Math::Vector3 arrowUp      = GetUpDir();
        const Math::Vector3 arrowTangent = { -arrowUp.y, arrowUp.x, 0.0f };
        const Math::Vector3 arrowDir     = arrowTangent * m_facingSign;
        arrow->Launch(spawnPos, arrowDir);
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

bool Player::ChangeAnimIfExist(const std::string& _animName, bool _isLoop)
{
    const auto& spData = m_modelWork.GetData();
    if (!spData) { return false; }
    const auto spAnim = spData->GetAnimation(_animName);
    if (!spAnim) { return false; }
    ChangeAnim(_animName, _isLoop);
    return true;
}
