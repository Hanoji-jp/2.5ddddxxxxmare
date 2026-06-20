#include "../../main.h"
#include "ParasolItem.h"
#include "../../Const/OutlineConst.h"

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

	// 星きらめき＋Effekseerループを統合エフェクトで開始（青系・色幅ランダム）
	ItemEffect::Params fxp;
	fxp.starSize    = SparkleConst::ParasolStarSize;
	fxp.orbitRadius = SparkleConst::ParasolStarRadius;
	fxp.color       = { SparkleConst::ParasolColorR, SparkleConst::ParasolColorG,
						SparkleConst::ParasolColorB, SparkleConst::ParasolColorA };
	fxp.colorShift  = SparkleConst::ParasolColorShift;
	m_effect.Init(m_spawnPos, fxp, ItemConst::ParasolEffectPath, ItemConst::ParasolEffectScale);
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

	// 星きらめき更新＋Effekseer位置追従
	m_effect.Update(pos, dt);
}

void ParasolItem::DrawEffect()
{
	if (m_pickedUp) { return; }
	m_effect.DrawEffect(GetPos());
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

void ParasolItem::DrawOutline()
{
	if (m_pickedUp || !m_modelWork.IsEnable()) { return; }

	const Math::Vector3 pos   = GetPos();
	const Math::Matrix  rotY  = Math::Matrix::CreateRotationY(m_rotAngle);
	const Math::Matrix  trans = Math::Matrix::CreateTranslation(pos);

	auto& shader = KdShaderManager::Instance().m_StandardShader;
	shader.SetOutlineWidth(OutlineConst::Width);
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
	shader.DrawModel(m_modelWork, rotY * trans, c, Math::Vector3::Zero);
}

void ParasolItem::MarkPickedUp()
{
	if (m_pickedUp) { return; }
	m_pickedUp = true;

	// 取得バーストは ItemManager 側で再生。ここではループを停止するだけ。
	m_effect.Stop();

	Expire();
}
