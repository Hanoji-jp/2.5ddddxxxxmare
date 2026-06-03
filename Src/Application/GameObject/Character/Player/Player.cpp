#include "../../../../Pch.h"
#include "Player.h"
#include "../../../Manager/ModelManager.h"
#include "../../../Manager/ManualGravityZoneManager.h"
#include "../../../Const/WarpHoleConst.h"
#include "../../../Const/JuiceConst.h"

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

    // AnimBlender にモデルワークを登録（以降は名前経由で安全取得）
    m_animBlender.Init(&m_modelWork);

    // アイテム取得用ヒットボックス初期化
    m_pickupHitBox.Init(ItemConst::PlayerPickupRadius, KdCollider::TypeEvent);

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
    // アイテム取得ヒットボックスをプレイヤー座標に合わせて更新
    m_pickupHitBox.Update(GetPos());

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
        // Rキーで自動モードに戻す（空中制限なし）
        else if (GetAsyncKeyState('R') & 0x8000)
        {
            SetManualGravity(ManualGravityDir::None);
        }
    }
    // ゾーン外でも手動重力はそのまま維持（ミスってゾーンを出たらふわーっと飛んでいく）

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
    m_animBlender.Update(m_modelWork, m_animSpeed);

    // ── 着地スクワッシュ検出 ────────────────────────────────
    if (m_isGround && !m_wasGround) { m_squashTimer = JuiceConst::SquashDuration; }
    if (m_squashTimer > 0.0f) { m_squashTimer -= 1.0f / 60.0f; }
    m_wasGround = m_isGround;
}

void Player::PostUpdate()
{
    // 着地判定は Character::PostUpdate() 内の CheckGround() で確定するので
    // 呼び出し前後で prevGround を取得してトリガーを判定する
    const bool prevGround = m_isGround;
    const int  prevPlanet = m_currentPlanetIndex;

    // 親クラスのPostUpdate（位置反映）を先に実行
    Character::PostUpdate();

    // ① 空中→着地 かつ 惑星上
    const bool justLanded     = (!prevGround && m_isGround && m_currentPlanetIndex >= 0);
    // ② 空中で別惑星圏へ移動
    const bool planetSwitched = (m_currentPlanetIndex != prevPlanet
                                 && m_currentPlanetIndex >= 0
                                 && prevPlanet >= 0);
    if (justLanded || planetSwitched)
    {
        m_planetChangedThisFrame = true;
    }

    // 位置確定後にワールド行列を再構築
    {
        const Math::Vector3 pos        = GetPos();
        const float         scale      = PlayerConst::ModelScale;
        // ワープ中はワープ方向（Slerp済み）でモデルを向かせる
        const Math::Vector3 up         = GetUpDir();

        // ----- 正規直交基底を構築 -----
        Math::Vector3 modelRight, modelFwd;

        if (m_warpUpOverrideActive)
        {
            // ワープ中：up が任意方向になるので「up と最も平行でないワールド軸」で安定構築
            const Math::Vector3 worldX{ 1.0f, 0.0f, 0.0f };
            const Math::Vector3 worldY{ 0.0f, 1.0f, 0.0f };
            const Math::Vector3 worldZ{ 0.0f, 0.0f, 1.0f };
            const float absDotX = std::abs(up.Dot(worldX));
            const float absDotY = std::abs(up.Dot(worldY));
            const float absDotZ = std::abs(up.Dot(worldZ));
            const Math::Vector3 ref = (absDotX <= absDotY && absDotX <= absDotZ) ? worldX
                                    : (absDotY <= absDotZ)                        ? worldY
                                    : worldZ;
            up.Cross(ref, modelFwd);
            modelFwd.Normalize();
            // 2.5D なので奥行き軸(Z)は重力方向に関係なく常に固定
            modelRight = Math::Vector3{ 0.0f, 0.0f, 1.0f };
        }
        else
        {
            const bool onSphere = (m_pCurrentPlanet && m_pCurrentPlanet->Shape == PlanetShape::Sphere);
            Math::Vector3 tangentBase;
            if (onSphere)
            {
                tangentBase = { -up.y, up.x, 0.0f };
                if (tangentBase.LengthSquared() < 0.0001f) { tangentBase = { 0.0f, 0.0f, 1.0f }; }
                tangentBase.Normalize();
            }
            else
            {
                // 移動計算の tangent と同じ式で統一（TOP/BOTTOM/LEFT/RIGHT すべて一致）
                tangentBase = { -up.y, up.x, 0.0f };
                if (tangentBase.LengthSquared() < 0.0001f) { tangentBase = { 1.0f, 0.0f, 0.0f }; }
                tangentBase.Normalize();
            }

            modelFwd = tangentBase * m_facingSign;
            modelFwd -= up * modelFwd.Dot(up);
            if (modelFwd.LengthSquared() > 0.0001f)
                modelFwd.Normalize();
            else
                modelFwd = tangentBase * m_facingSign;

            // up × modelFwd で正規直交右手系を維持（全重力方向で det=+1）
            up.Cross(modelFwd, modelRight);
            if (modelRight.LengthSquared() > 0.0001f)
                modelRight.Normalize();
            else
                modelRight = Math::Vector3{ 0.0f, 0.0f, 1.0f };
        }

        const Math::Matrix rot(
            modelRight.x, modelRight.y, modelRight.z, 0.0f,
            up.x,         up.y,         up.z,         0.0f,
            modelFwd.x,   modelFwd.y,   modelFwd.z,   0.0f,
            0.0f,         0.0f,         0.0f,          1.0f);

        // 等方スケール行列（Traveling 中のみ縦伸び、着地時はスクワッシュ）
        const float stretchY = GetWarpStretch() ? WarpHoleConst::WarpStretchScale : 1.0f;

        // 着地スクワッシュ：タイマーが正の間は XZ 広がり・Y 縮み
        float squashX = 1.0f;
        float squashY = 1.0f;
        if (m_squashTimer > 0.0f)
        {
            const float t = m_squashTimer / JuiceConst::SquashDuration;  // 1→0
            squashX = 1.0f + (JuiceConst::SquashScaleX - 1.0f) * t;
            squashY = 1.0f + (JuiceConst::SquashScaleY - 1.0f) * t;
        }

        const Math::Matrix scaleMat(
            scale * squashX,             0.0f,             0.0f, 0.0f,
            0.0f,  scale * stretchY * squashY,             0.0f, 0.0f,
            0.0f,             0.0f, scale * squashX,             0.0f,
            0.0f,             0.0f,             0.0f,             1.0f);

        const Math::Matrix transMat = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
        const Math::Matrix transDrawMat = DirectX::XMMatrixTranslation(
            pos.x + up.x * PlayerConst::ModelOffsetY,
            pos.y + up.y * PlayerConst::ModelOffsetY,
            pos.z + up.z * PlayerConst::ModelOffsetY);

        // scaleMat（ローカル） → rot（姿勢） → trans（位置）
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
    // ダッシュ判定（地上かつ Shift 長押し）
    m_isDashing = m_isGround && (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    m_animSpeed = m_isDashing ? PlayerConst::DashAnimSpeedMul : 1.0f;

    const float speed = m_isDashing
        ? PlayerConst::MoveSpeed    * PlayerConst::DashSpeedMul
        : PlayerConst::MoveSpeed;
    const float accel = m_isDashing
        ? PlayerConst::Acceleration * PlayerConst::DashAccelMul
        : PlayerConst::Acceleration;

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

        // up に直交する接線を計算（球面・Box・Zone すべて同じ式で統一）
        // top(up=+Y)→tangent={-1,0,0}, left(up=-X)→tangent={0,-1,0}
        // bottom(up=-Y)→tangent={+1,0,0}, right(up=+X)→tangent={0,+1,0}
        Math::Vector3 tangent = { -up.y, up.x, 0.0f };
        if (tangent.LengthSquared() < 0.0001f) { tangent = { 1.0f, 0.0f, 0.0f }; }
        tangent.Normalize();

        // 入力の XY を接線・法線ベースに変換
        Math::Vector3 worldInput = tangent * input.x + up * input.y;
        // up 成分（radial 方向）を除去して接線のみにする
        worldInput -= up * worldInput.Dot(up);

        if (worldInput.LengthSquared() > 0.0001f)
        {
            worldInput = DirectX::XMVector3Normalize(worldInput);

            const Math::Vector3 targetVel = worldInput * speed;
            m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, targetVel,
                accel / speed);

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
    // 物理用upDir（即切り替え）を使うことで重力切り替え直後も床方向への速度混入を防ぐ
    const Math::Vector3 up = GetPhysicsUpDir();
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

    // AnimBlender 経由でワンクッション（存在しない名前は無視）
    if (!m_animBlender.ChangeAnimation(_animName, _isLoop, PlayerConst::AnimBlendFrames)) { return; }
    m_currentAnimName = _animName;
}

bool Player::ChangeAnimIfExist(const std::string& _animName, bool _isLoop)
{
    if (m_currentAnimName == _animName) { return true; }
    if (!m_animBlender.ChangeAnimation(_animName, _isLoop, PlayerConst::AnimBlendFrames)) { return false; }
    m_currentAnimName = _animName;
    return true;
}

void Player::TakeDamage(int _damage)
{
    // 基底クラスのダメージ処理
    Character::TakeDamage(_damage);

    // 被ダメ赤フラッシュをトリガー
    KdShaderManager::Instance().m_postProcessShader.TriggerDamageFlash();
}
