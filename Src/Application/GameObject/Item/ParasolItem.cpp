#include "../../main.h"
#include "ParasolItem.h"

void ParasolItem::Init()
{
	m_drawType = eDrawTypeLit;

	// モデル読み込み
	m_modelWork.SetModelData(ItemConst::ParasolModelPath);

	// 取得判定コライダー（TypeEvent）
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("ParasolHit",
		Math::Vector3::Zero,
		ItemConst::ParasolHitRadius,
		KdCollider::TypeEvent);

	// ループエフェクト開始（EffekseerPath はマネージャー内部で自動付加される）
	m_wpEffect = KdEffekseerManager::GetInstance().Play(
		ItemConst::ParasolEffectPath,
		m_spawnPos,
		ItemConst::ParasolEffectScale,
		1.0f,
		true);
}

void ParasolItem::Update()
{
	if (m_pickedUp) { return; }

	const float dt = KdFPSController::GetDt();

	m_bobTimer += ItemConst::ParasolBobSpeed * dt * 60.0f;
	m_rotAngle += ItemConst::ParasolRotSpeed * dt;
	if (m_rotAngle > DirectX::XM_2PI) { m_rotAngle -= DirectX::XM_2PI; }

	const float bobOffset = std::sinf(m_bobTimer) * ItemConst::ParasolBobAmplitude;
	Math::Vector3 pos = m_spawnPos;
	pos.y += bobOffset;
	SetPos(pos);

	// エフェクト位置を追従
	if (const auto spEfk = m_wpEffect.lock())
	{
		spEfk->SetPos(pos);
	}
}

void ParasolItem::DrawLit()
{
	if (m_pickedUp || !m_modelWork.IsEnable()) { return; }

	const Math::Vector3 pos   = GetPos();

	// モデル側で傾き・スケール設定済み → Y 軸回転 + 平行移動のみ
	const Math::Matrix rotY  = Math::Matrix::CreateRotationY(m_rotAngle);
	const Math::Matrix trans = Math::Matrix::CreateTranslation(pos);

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, rotY * trans);
}

void ParasolItem::MarkPickedUp()
{
	if (m_pickedUp) { return; }
	m_pickedUp = true;

	// ループエフェクトを即時停止
	if (const auto spEfk = m_wpEffect.lock())
	{
		spEfk->SetLoop(false);
		KdEffekseerManager::GetInstance().StopEffect(spEfk->GetHandle());
	}

	Expire();
}
