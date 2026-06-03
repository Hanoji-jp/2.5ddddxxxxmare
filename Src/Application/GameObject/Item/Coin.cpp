#include "../../main.h"
#include "Coin.h"

void Coin::Init()
{
	m_drawType = eDrawTypeLit;

	// モデル読み込み
	m_modelWork.SetModelData(ItemConst::CoinModelPath);

	// 取得判定コライダー登録（TypeEvent: 攻撃でも地形でもなくイベント用）
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("CoinHit",
		Math::Vector3::Zero,
		ItemConst::CoinHitRadius,
		KdCollider::TypeEvent);
}

void Coin::Update()
{
	m_bobTimer  += ItemConst::CoinBobSpeed;
	m_rotAngle  += ItemConst::CoinRotateSpeed;
	if (m_rotAngle > DirectX::XM_2PI) { m_rotAngle -= DirectX::XM_2PI; }

	const float bobOffset = std::sinf(m_bobTimer) * ItemConst::CoinBobAmplitude;
	Math::Vector3 pos = m_spawnPos;
	pos.y += bobOffset;
	SetPos(pos);
}

void Coin::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	const Math::Vector3 pos  = GetPos();
	const Math::Matrix  rot  = Math::Matrix::CreateRotationY(m_rotAngle);
	const Math::Matrix  trans = Math::Matrix::CreateTranslation(pos);
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, rot * trans);
}
