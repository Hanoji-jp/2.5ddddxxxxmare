#include "../../../../Pch.h"
#include "Cubun.h"
#include "../Character.h"

//==========================================================
// 初期化
//==========================================================
void Cubun::Init()
{
	// 基底クラスのモデルロード（hp も設定される）
	InitModel(CubunConst::ModelPath);

	// スポーン位置を記録
	m_spawnPos = GetPos();

	// 出現向き：FaceDown の場合は上下を反転して出現させる
	if (m_faceDir == CubunFaceDir::Down)
	{
		// upDir を -Y に設定（Character の内部メンバに直接アクセスできないため velocity反転で代用）
		// Character の m_upDir / m_upDirVisual を直接書き換えたいが protected なので
		// SetManualGravity(Up) を呼ぶことで疑似的に上下反転させる
		SetManualGravity(ManualGravityDir::Up);
	}
	else if (m_initGravDir != ManualGravityDir::None)
	{
		// 初期重力方向を設定（FaceUp 出現時でも手動重力指定があれば適用）
		SetManualGravity(m_initGravDir);
	}

	// 崖・壁で折り返す挙動を有効化（マリオの赤クリボーと同じ動き）
	m_useEdgeDetection = true;

	// 棘コライダー（体の下方向）
	m_pSpikeCollider = std::make_unique<KdCollider>();
	const DirectX::BoundingOrientedBox spikeObb(
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(CubunConst::SpikeRadius,
						  CubunConst::SpikeRadius,
						  CubunConst::SpikeRadius),
		DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
	m_pSpikeCollider->RegisterCollisionShape("Spike", spikeObb, KdCollider::TypeBump);

	m_hp = CubunConst::Hp;
}

//==========================================================
// 更新
//==========================================================
void Cubun::Update()
{
	// 初回死亡検知でバーストエフェクトをリクエスト
	if (IsDead() && !m_deathBounceActive)
	{
		m_requestBurstEffect = true;
	}

	if (IsDead()) { return; }

	// ジャンプ制御（着地後 JumpInterval 秒ごとにジャンプ）
	TryJump();

	// 移動・AI は Enemy::Update() に全部任せる（クリボーと同じ）
	Enemy::Update();
}

//==========================================================
// 描画
//==========================================================
void Cubun::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	// ビジュアル行列（体は回転しない）
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, CalcVisualMatrix());
}

void Cubun::DrawDebug()
{
	if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }

	// 本体
	const Math::Vector3 bodySize = {
		CubunConst::CollisionRadius * 2.0f,
		CubunConst::CollisionHeight,
		CubunConst::CollisionRadius * 2.0f
	};
	m_pDebugWire->AddDebugBox(CalcVisualMatrix(), bodySize, Math::Vector3::Zero, false, { 0.8f, 0.4f, 0.0f, 1.0f });

	// 棘エリア（体の下）
	const Math::Vector3 spikePos = GetPos() - m_upDir * (CubunConst::CollisionHeight * 0.5f
											 + CubunConst::SpikeOffset);
	const Math::Matrix spikeMat  = Math::Matrix::CreateTranslation(spikePos);
	const Math::Vector3 spikeSize = {
		CubunConst::SpikeRadius * 2.0f,
		CubunConst::SpikeRadius * 2.0f,
		CubunConst::SpikeRadius * 2.0f
	};
	m_pDebugWire->AddDebugBox(spikeMat, spikeSize, Math::Vector3::Zero, false, { 1.0f, 0.0f, 0.0f, 1.0f });

	m_pDebugWire->Draw();
}

//==========================================================
// 攻撃（近接タックル）
//==========================================================
void Cubun::DoAttack()
{
	FaceTarget();

	if (m_attackCool > 0) { ChangeAnim("Idle"); return; }

	const auto spTarget = m_wpTarget.lock();
	if (spTarget)
	{
		if (auto* pChar = dynamic_cast<Character*>(spTarget.get()))
		{
			pChar->TakeDamage(CubunConst::ContactDamage);
		}
	}

	ChangeAnim("Idle");
	m_attackCool = static_cast<int>(CubunConst::JumpInterval * 60.0f);
}

//==========================================================
// 棘判定（外部から問い合わせ）
// 体の「下面」から SpikeOffset だけ下の球の範囲内にいるか
//==========================================================
bool Cubun::IsSpikeHit(const Math::Vector3& playerPos) const
{
	// 体の「下」方向 = -m_upDir
	const Math::Vector3 spikeCenter = GetPos()
		- m_upDir * (CubunConst::CollisionHeight * 0.5f + CubunConst::SpikeOffset);
	const Math::Vector3 diff = playerPos - spikeCenter;
	return diff.LengthSquared() <= CubunConst::SpikeRadius * CubunConst::SpikeRadius;
}

bool Cubun::Intersects(const KdCollider::RayInfo& ray,
					   std::list<KdCollider::CollisionResult>* result)
{
	if (!m_pCollider) { return false; }
	return m_pCollider->Intersects(ray, m_mWorld, result);
}

//==========================================================
// プライベート：ジャンプ（着地後 JumpInterval 秒後に再ジャンプ）
//==========================================================
void Cubun::TryJump()
{
	const float kDt = KdFPSController::GetDt();

	if (m_isGround)
	{
		m_jumpTimer += kDt;
		if (m_jumpTimer >= CubunConst::JumpInterval)
		{
			m_jumpTimer = 0.0f;
			// 上方向（m_upDir）にジャンプ速度を加算
			const float currentUp = m_velocity.Dot(m_upDir);
			m_velocity += m_upDir * (CubunConst::JumpPower - currentUp);
		}
	}
	else
	{
		m_jumpTimer = 0.0f;
	}
}

//==========================================================
// プライベート：ビジュアル行列（体の向きは重力に関係なく常に上向き固定）
//==========================================================
Math::Matrix Cubun::CalcVisualMatrix() const
{
	const Math::Vector3 pos   = GetPos();
	const float         yaw   = std::atan2f(m_facingDir.x, m_facingDir.z);
	const float         scale = EnemyConst::ModelScale;

	// 体は常にワールドY上向き（重力で回転しない）
	return DirectX::XMMatrixScaling(scale, scale, scale)
		 * DirectX::XMMatrixRotationY(yaw)
		 * DirectX::XMMatrixTranslation(pos.x,
			   pos.y + CubunConst::ModelOffsetY,
			   pos.z);
}

