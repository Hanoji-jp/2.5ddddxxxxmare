#include "../../../../Pch.h"
#include "Player.h"
#include "../../../Manager/ModelManager.h"
#include "../../../Manager/ManualGravityZoneManager.h"
#include "../../../Const/WarpHoleConst.h"
#include "../../../Const/JuiceConst.h"
#include "../../../Const/WindBoxConst.h"
#include "../../../Const/SparkleConst.h"

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
	// 死亡中は通常の入力・移動・状態遷移を止める。
	// （Move() が毎フレーム m_state を上書きするため、放置すると Dead アニメが
	//   他アニメと毎フレーム切り替わってガクガクする）
	if (IsDead())
	{
		m_state    = State::Dead;
		m_animSpeed = 1.0f;
		ChangeAnim("Dead", false);
		m_velocity     = Math::Vector3::Zero;   // その場で停止
		m_moveVelocity = Math::Vector3::Zero;
		m_animBlender.Update(m_modelWork, m_animSpeed);
		return;
	}

	// 取得演出の発光を進める（ヒットストップ解除後はここで継続）
	UpdatePickupGlow(KdFPSController::GetDt());

	// アイテム取得ヒットボックスをプレイヤー座標に合わせて更新
	m_pickupHitBox.Update(GetPos());

	// ── 演出用：操作無効中は入力を受けず、重力落下のみ ──
	// （投げ出され→不時着などのカットシーン用。velocity は外部設定を維持）
	if (!m_controlEnabled)
	{
		m_moveVelocity = Math::Vector3::Zero;
		m_isDashing    = false;
		m_state        = m_isGround ? State::Idle : State::Fall;

		Character::Update();   // 重力（移動・当たり判定は PostUpdate 側）

		// 状態に応じてアニメ（Fall / Idle）を設定してから更新
		if (m_state == State::Fall) { if (!ChangeAnimIfExist("Fall", true)) { ChangeAnim("Idle", true); } }
		else                        { ChangeAnim("Idle", true); }
		m_animBlender.Update(m_modelWork, 1.0f);
		return;
	}

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
	//KdDebugGUI::Instance().AddLog("Player: x=%.2f  y=%.2f  z=%.2f\n", pos.x, pos.y, pos.z);

	// クールダウン更新
	if (m_meleeCooldown > 0) { --m_meleeCooldown; }
	if (m_rangedCooldown > 0) { --m_rangedCooldown; }
	if (m_invincibleTimer > 0) { --m_invincibleTimer; }

	// Jump 状態で下降局面に入ったら Fall に自動遷移してパラソルを開けるようにする
	if (m_state == State::Jump && !m_isGround)
	{
		const float upVel = m_velocity.Dot(GetUpDir());
		if (upVel <= 0.0f) { m_state = State::Fall; }
	}

	// ---- 僘入力受付（空中かつ僘所持中なら常に受付） ----
	if (!m_isGround && m_hasParasol)// 空中で傘所持中、または風の中にいる場合は常に僘入力を受け付ける
    {
		//ジャンプ系の捜査はすべてspaceで行うことにする
		//const bool wantOpen = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
		//if (wantOpen && !m_isParasolOpen)
		//{
		//	const float maxFall = PlanetConst::MaxFallSpeed * PlayerConst::ParasolGravityScale;
		//	const float downVel = m_velocity.Dot(-GetUpDir());
		//	if (downVel > maxFall)
		//	{
		//		m_velocity += GetUpDir() * (downVel - maxFall);
		//	}
		//	m_isParasolOpen = wantOpen;
		//}
		
        if (GetAsyncKeyState('E') & 0x8000)
        {
            m_isParasolOpen = true;
        }
        if (GetAsyncKeyState('Q') & 0x8000)
        {
            m_isParasolOpen = false;
        }
    }

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
        if (!ChangeAnimIfExist("Jump", false)) { ChangeAnim("Idle", true); }
        // ジャンプ中にパラソルを開いている場合は上半身だけ ParasolFall で上書き
        if (m_isParasolOpen)
        {
            const auto spParasolAnim = m_modelWork.GetAnimation("ParasolFall");
            if (spParasolAnim && m_animBlender.IsUpperBodyAnimEnd())
            {
                m_animBlender.SetUpperBodyAnim(spParasolAnim, PlayerConst::UpperBodyNodes(), true);
            }
        }
        else
        {
            // 傘を閉じたら上書き解除
            m_animBlender.ClearUpperBodyAnim();
        }
        break;
    case State::Fall:
        // 傘所持中: E で開く→ParasolFall / Q or デフォルト→Fall
        if (m_hasParasol)
        {
            if (GetAsyncKeyState('E') & 0x8000)
            {
                if (!m_isParasolOpen)
                {
                    // 僘を開いた瞬間：既存の落下速度を即座にクランプ
                    const float maxFall = PlanetConst::MaxFallSpeed * PlayerConst::ParasolGravityScale;
                    const float downVel = m_velocity.Dot(-GetUpDir());
                    if (downVel > maxFall)
                    {
                        m_velocity += GetUpDir() * (downVel - maxFall);
                    }
                }
                m_isParasolOpen = true;
            }
            if (GetAsyncKeyState('Q') & 0x8000)
            {
                m_isParasolOpen = false;
            }

            if (m_isParasolOpen)
            {
                if (!ChangeAnimIfExist("ParasolFall", true)) { ChangeAnim("Fall", true); }
            }
            else
            {
                if (!ChangeAnimIfExist("Fall", true)) { ChangeAnim("Idle", true); }
            }
        }
        else
        {
            m_isParasolOpen = false;
            if (!ChangeAnimIfExist("Fall", true)) { ChangeAnim("Idle", true); }
        }
        break;
    case State::Attack: ChangeAnim("Attack", false); break;
    case State::Dead:   ChangeAnim("Dead",   false); break;
    default: break;
    }

    // ---- Walk / Fall / Jump 中 Attack: 上半身 or 左腕だけ Attack で上書き ----
    if (m_isAttacking)
    {
        const bool isUpperBodyActive =
            (m_state == State::Walk) ||
            (m_state == State::Fall) ||
            (m_state == State::Jump);

        if (isUpperBodyActive)
        {
            // upper body アニメを AttackMelee でセット済み。終了したらリセット
            if (m_animBlender.IsUpperBodyAnimEnd())
            {
                m_isAttacking = false;
                m_animBlender.ClearUpperBodyAnim();
            }
        }
        else
        {
            // 全身 Attack の場合は upper body は不要
            m_animBlender.ClearUpperBodyAnim();
        }
    }

    // Attack フラグ終了チェック（全身 Attack の場合）
    if (m_isAttacking && m_state == State::Attack && m_animBlender.IsAnimationEnd())
    {
        m_isAttacking = false;
        m_state = m_isGround ? State::Idle : State::Fall;
    }

    // 着地したら傘を閉じる
    // 上半身アニメのクリアは Attack 中でない場合のみ（Walk中攻撃を維持するため）
    if (m_isGround)
    {
        m_isParasolOpen = false;
        if (!m_isAttacking)
        {
            m_animBlender.ClearUpperBodyAnim();
        }
    }

    // 重力スケール: 風の中 > 傘落下 > 通常 の優先順で決定
    {
        const float downVel = m_velocity.Dot(-GetUpDir());
        if (m_wasInWind && m_windIsHorizontal)
        {
            // 左右風の中：ほぼ落下しない
            m_gravityScale = WindBoxConst::WindGravityScale;
        }
        else if (m_isParasolOpen && downVel > 0.0f)
        {
            m_gravityScale = PlayerConst::ParasolGravityScale;
        }
        else
        {
            m_gravityScale = 1.0f;
        }
    }

    // フラグ読み取り前にローカルへコピー
    const bool inHorizWind = m_wasInWind && m_windIsHorizontal;

    // 風フラグを読み終えたのでリセット（次フレームの ApplyWind で再セットされる）
    m_wasInWind        = false;
    m_windIsHorizontal = false;

    // 横風の中：既存の落下速度もクランプ（重力スケールだけでは入った瞬間の速度が残る）
    if (inHorizWind)
    {
        const float downVel = m_velocity.Dot(-GetUpDir());
        if (downVel > WindBoxConst::WindHorizMaxFallSpeed)
        {
            m_velocity += GetUpDir() * (downVel - WindBoxConst::WindHorizMaxFallSpeed);
        }
    }

    // パラソル落下中は毎フレーム落下速度をクランプ（壁衝突・重力切り替え後も維持）
    if (m_isParasolOpen && !inHorizWind)
    {
        const float maxFall = PlanetConst::MaxFallSpeed * PlayerConst::ParasolGravityScale;
        const float downVel = m_velocity.Dot(-GetUpDir());
        if (downVel > maxFall)
        {
            m_velocity += GetUpDir() * (downVel - maxFall);
        }
    }

    // ---- 傘デバッグ: P キーで傘アイテム取得トグル ----
    if (GetAsyncKeyState('P') & 1)
    {
        m_hasParasol = !m_hasParasol;
    }

    // ---- ノード可視制御 ----
    // OpenedParasol: 傘所持 かつ 開いているときのみ表示
    m_modelWork.SetNodeVisible("OpenedParasol",  m_hasParasol && m_isParasolOpen);
    // ClosedParasol: 傘所持 かつ 閉じているときのみ表示
    m_modelWork.SetNodeVisible("ClosedParasol",  m_hasParasol && !m_isParasolOpen);

    // 剣: Attack 中は HandledSword ON / BackSword OFF
    m_modelWork.SetNodeVisible("HandledSword",  m_isAttacking);
    m_modelWork.SetNodeVisible("BackSword",     !m_isAttacking);

    // アニメーション更新
    m_animBlender.Update(m_modelWork, m_animSpeed);

    // ── 着地スクワッシュ検出 ────────────────────────────────
    if (m_isGround && !m_wasGround) { m_squashTimer = JuiceConst::SquashDuration; }
    if (m_squashTimer > 0.0f) { m_squashTimer -= KdFPSController::GetDt(); }
    m_wasGround = m_isGround;
}

void Player::PostUpdate()
{
    // 着地判定は Character::PostUpdate() 内の CheckGround() で確定するので
    // 呼び出し前後で prevGround を取得してトリガーを判定する
    const bool prevGround = m_isGround;
    const int  prevPlanet = m_currentPlanetIndex;

    // 死亡中は物理・当たり判定（CheckWall の敵押し出し含む）をスキップして位置を固定。
    // これをしないと、踏みつぶし時に Cubun 本体のめり込み防止押し出しで死体が外へ滑り出る。
    if (!IsDead())
    {
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

        // ── 風による傾き（drawWorld のみ。コリジョン用 m_mWorld は変えない）──
        if (std::abs(m_windTiltAngle) > 0.01f)
        {
            // 傾き軸：プレイヤーの前後方向（Z軸 = modelFwd）に対して垂直な軸 = modelRight（= up × modelFwd）
            // 風の左右成分が大きいほど右か左に傾く
            // 傾き回転：drawWorld の平行移動を除いた部分を中心に回転させる
            const Math::Vector3 drawPos = { m_drawWorld._41, m_drawWorld._42, m_drawWorld._43 };
            const Math::Matrix  tiltRot = Math::Matrix::CreateFromAxisAngle(
                Math::Vector3{ 0.0f, 0.0f, 1.0f },  // ワールドZ軸固定（プレイヤーの向きに依存しない）
                -m_windTiltAngle * (3.14159265f / 180.0f));  // 負符号でRightが右傾き、Leftが左傾き
            // 平行移動を一時除去→回転→戻す
            Math::Matrix drawNoTrans = m_drawWorld;
            drawNoTrans._41 = 0.0f; drawNoTrans._42 = 0.0f; drawNoTrans._43 = 0.0f;
            drawNoTrans = drawNoTrans * tiltRot;
            drawNoTrans._41 = drawPos.x; drawNoTrans._42 = drawPos.y; drawNoTrans._43 = drawPos.z;
            m_drawWorld = drawNoTrans;
        }

        // ── 演出用タンブル（描画のみ。ワールドZ軸まわりにロール）──
        if (std::abs(m_cutsceneSpin) > 0.0001f)
        {
            const Math::Vector3 drawPos = { m_drawWorld._41, m_drawWorld._42, m_drawWorld._43 };
            const Math::Matrix  spinRot = Math::Matrix::CreateFromAxisAngle(
                Math::Vector3{ 0.0f, 0.0f, 1.0f }, m_cutsceneSpin);
            Math::Matrix drawNoTrans = m_drawWorld;
            drawNoTrans._41 = 0.0f; drawNoTrans._42 = 0.0f; drawNoTrans._43 = 0.0f;
            drawNoTrans = drawNoTrans * spinRot;
            drawNoTrans._41 = drawPos.x; drawNoTrans._42 = drawPos.y; drawNoTrans._43 = drawPos.z;
            m_drawWorld = drawNoTrans;
        }

        // 風の積算を消費してタイルトを更新し、次フレームのためにリセット
        {
            const float dt60 = KdFPSController::GetDt() * 60.0f;
            // 風の右方向（tangent）成分でタイルト目標を決める
            // Right風 (windAccum.x > 0) → 正の角度 → 右傾き
            // Left風  (windAccum.x < 0) → 負の角度 → 左傾き
            // プレイヤーの向きに関係なくワールドX成分で傾き方向を決定する
            const float targetTilt = std::clamp(
                m_windAccum.x * (WindBoxConst::MaxTiltDeg / WindBoxConst::DefaultPower),
                -WindBoxConst::MaxTiltDeg, WindBoxConst::MaxTiltDeg);

            const float lerpSpeed = (m_windAccum.LengthSquared() > 0.0f)
                ? WindBoxConst::TiltLerpSpeed
                : WindBoxConst::TiltResetSpeed;
            m_windTiltAngle = m_windTiltAngle + (targetTilt - m_windTiltAngle)
                              * std::min(lerpSpeed * dt60, 1.0f);

            m_windAccum = {};  // 毎フレーム消費
        }
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

// 取得演出中：プレイヤーを加算ブルームバッファへ再描画して光らせる
void Player::DrawBright()
{
    if (m_pickupGlowTimer <= 0.0f) { return; }
    if (!m_modelWork.IsEnable())   { return; }

    // 残量 1->0 でフェード、+ 細かいシマー
    const float t       = m_pickupGlowTimer / SparkleConst::PickupGlowDuration;
    const float shimmer = 1.0f + SparkleConst::PickupGlowShimmerAmp
        * std::sinf(m_pickupGlowTimer * SparkleConst::PickupGlowShimmer);
    const float k = SparkleConst::PickupGlowIntensity * t * shimmer;

    // BeginBright 側で加算合成 + ZWrite 無効は設定済み。
    // Effekseer 描画後なので s0 サンプラとシェーダを張り直す。
    auto& shader = KdShaderManager::Instance().m_StandardShader;
    KdShaderManager::Instance().ChangeSamplerState(KdSamplerState::Anisotropic_Wrap, 0);
    shader.BeginUnLit();
    shader.SetDissolve(0.0f);

    // colRate(=g_BaseColor) を発光色で増幅 -> 加算で明るいシルエット -> ブルーム
    const Math::Color glow{ m_pickupGlowColor.x * k, m_pickupGlowColor.y * k,
                            m_pickupGlowColor.z * k, 1.0f };
    shader.DrawModel(m_modelWork, m_drawWorld, glow, Math::Vector3::Zero);

    KdShaderManager::Instance().UndoSamplerState(0);
}

// 取得演出の発光を開始（色指定）
void Player::TriggerPickupGlow(const Math::Color& color)
{
    m_pickupGlowColor = color;
    m_pickupGlowTimer = SparkleConst::PickupGlowDuration;
}

// 発光残り時間を進める（ヒットストップ中はシーン側から呼ばれる）
void Player::UpdatePickupGlow(float dt)
{
    if (m_pickupGlowTimer <= 0.0f) { return; }
    m_pickupGlowTimer -= dt;
    if (m_pickupGlowTimer < 0.0f) { m_pickupGlowTimer = 0.0f; }
}

void Player::Move()
{
    const float dt60 = KdFPSController::GetDt() * 60.0f;

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
                std::min(accel / speed * dt60, 1.0f));

            // 向き更新: tangent とのドット積で左右符号を即時決定する
            // Lerp+スナップは初期値(±1)から抜け出せなくなるバグがあるため使わない
            m_facingSign = (tangent.Dot(worldInput) >= 0.0f) ? 1.0f : -1.0f;
        }

        if (m_isGround) { m_state = State::Walk; }
        else if (m_state != State::Jump) { m_state = State::Fall; }
    }
    else
    {
        m_moveVelocity = Math::Vector3::Lerp(m_moveVelocity, Math::Vector3::Zero,
            std::min(PlayerConst::Deceleration / PlayerConst::MoveSpeed * dt60, 1.0f));
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

    // 風の水平速度を加算（Move() の上書きと干渉しないよう別メンバーで管理）
    m_velocity += m_windHorizVel;

    m_velocity.z = 0.0f;   // Z は常に固定

    // 範囲外のとき m_windHorizVel を減衰（重力と同じ感覚で自然に止まる）
    if (!m_wasInWind)
    {
        m_windHorizVel = Math::Vector3::Lerp(m_windHorizVel, Math::Vector3::Zero,
            std::min(0.1f * dt60, 1.0f));
        if (m_windHorizVel.LengthSquared() < 0.0001f) { m_windHorizVel = {}; }
    }
}

void Player::Jump()
{
	//連打防止を追加

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
        m_isAttacking   = true;

        if (m_state == State::Walk)
        {
            // Walk 中: 上半身だけ Attack で上書き
            const auto spAttackAnim = m_modelWork.GetAnimation("Attack");
            if (spAttackAnim)
            {
                m_animBlender.SetUpperBodyAnim(spAttackAnim, PlayerConst::UpperBodyNodes(), false);
            }
            // state は Walk のまま維持（下半身は Walk を継続）
        }
        else if (m_state == State::Fall || m_state == State::Jump)
        {
            // Fall/Jump 中: shoulder.L 以降の左腕チェーンのみ Attack で上書き
            const auto spAttackAnim = m_modelWork.GetAnimation("Attack");
            if (spAttackAnim)
            {
                m_animBlender.SetUpperBodyAnim(spAttackAnim, PlayerConst::LeftArmNodes(), false);
            }
            // state は Fall/Jump のまま維持（全身アニメは変えない）
        }
        else
        {
            // Idle 等 その他: 全身 Attack
            m_animBlender.ClearUpperBodyAnim();
            m_state = State::Attack;
        }
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
    if (IsDead())              { return; }   // 死体は被ダメージしない
    if (m_invincibleTimer > 0) { return; }
    Character::TakeDamage(_damage);
    m_invincibleTimer = PlayerConst::InvincibleFrame;
    KdShaderManager::Instance().m_postProcessShader.TriggerDamageFlash();
}

void Player::InstantDeath()
{
    if (IsDead()) { return; }
    // 即死：無敵を無視して HP を 0 に落とす
    Character::TakeDamage(PlayerConst::MaxHp);
    m_invincibleTimer = PlayerConst::InvincibleFrame;
    KdShaderManager::Instance().m_postProcessShader.TriggerDamageFlash();

    // 直前に他のダメージ源から付いたノックバック速度を消して、その場で停止させる
    m_velocity     = Math::Vector3::Zero;
    m_moveVelocity = Math::Vector3::Zero;
}

void Player::TakeDamageFrom(int _damage, const Math::Vector3& sourcePos)
{
    if (IsDead())              { return; }   // 死体はノックバック・被ダメージしない
    if (m_invincibleTimer > 0) { return; }
    Character::TakeDamage(_damage);
    m_invincibleTimer = PlayerConst::InvincibleFrame;
    KdShaderManager::Instance().m_postProcessShader.TriggerDamageFlash();

    // ── マリオギャラクシー風ノックバック ──────────────────
    const Math::Vector3 up = GetUpDir();

    // ソースからプレイヤーへのベクトルを上方向成分を除いて水平化
    Math::Vector3 knockDir = GetPos() - sourcePos;
    knockDir -= up * knockDir.Dot(up);

    if (knockDir.LengthSquared() > 1e-6f)
    {
        knockDir.Normalize();
    }
    else
    {
        // 同一位置の場合は現在の向きの逆方向（X軸ベース）
        knockDir = Math::Vector3{ -m_facingSign, 0.0f, 0.0f };
    }

    // m_velocity を直接置き換え（PostUpdate が同フレームで使うので即効く）
    m_velocity = knockDir * PlayerConst::KnockbackSpeed
               + up      * PlayerConst::KnockbackUpSpeed;
    m_velocity.z = 0.0f;

    // m_moveVelocity にも入れて次フレーム以降も自然に減衰させる
    m_moveVelocity   = knockDir * PlayerConst::KnockbackSpeed;
    m_moveVelocity.z = 0.0f;
}

void Player::ApplyWind(const Math::Vector3& windDir, float power)
{
    // パラソルを開いているときだけ風を受ける
    if (!m_isParasolOpen) { return; }

    m_wasInWind = true;

    // 左右風かどうかを記録（水平成分が支配的なら true）
    const Math::Vector3 horizCheck = windDir - m_upDir * windDir.Dot(m_upDir);
    if (horizCheck.Length() > 0.5f) { m_windIsHorizontal = true; }

    // 傾きエフェクト用に加算（PostUpdate で消費・リセット）
    m_windAccum += windDir * power;

    const float dt60 = KdFPSController::GetDt() * 60.0f;

    // 上向き風：m_upDir 方向に加速（MaxUpSpeed クランプ）
    const float upComponent = windDir.Dot(m_upDir);
    if (upComponent > 0.01f)
    {
        const float currentUp = m_velocity.Dot(m_upDir);
        if (currentUp < WindBoxConst::MaxUpSpeed)
        {
            const float add   = upComponent * power * dt60;
            const float newUp = std::min(currentUp + add, WindBoxConst::MaxUpSpeed);
            m_velocity += m_upDir * (newUp - currentUp);
        }
    }

    // 左右風：m_upDir 垂直面（水平）方向に加速（MaxSideSpeed クランプ）
    const Math::Vector3 horizDir = windDir - m_upDir * windDir.Dot(m_upDir);
    const float horizLen = horizDir.Length();
    if (horizLen > 0.01f)
    {
        const Math::Vector3 horizNorm   = horizDir / horizLen;
        const float         currentSide = m_windHorizVel.Dot(horizNorm);
        if (currentSide < WindBoxConst::MaxSideSpeed)
        {
            const float add     = horizLen * power * dt60;
            const float newSide = std::min(currentSide + add, WindBoxConst::MaxSideSpeed);
            m_windHorizVel += horizNorm * (newSide - currentSide);
        }
    }
}

void Player::ClearWindState()
{
    if (!m_wasInWind) { return; }
    m_wasInWind        = false;
    m_windIsHorizontal = false;
    // 水平風速は減衰に任せる（Move() 末尾で毎フレーム Lerp で縮小）
}
