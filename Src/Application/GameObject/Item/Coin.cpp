#include "../../main.h"
#include "Coin.h"
#include "../../Const/OutlineConst.h"

void Coin::Init()
{
	m_drawType = eDrawTypeLit;

	// モデル読み込み
	m_modelWork.SetModelData(ItemConst::CoinModelPath);

	// gltf の roughness=0 (完全鏡面) は IBL 近似で眩しすぎるため補正
	if (const auto& spData = m_modelWork.GetData())
	{
		for (auto& mat : spData->WorkMaterials())
		{
			mat.m_metallicRate  = ItemConst::CoinMetallic;
			mat.m_roughnessRate = ItemConst::CoinRoughness;
		}
	}

	// 取得判定コライダー登録（TypeEvent: 攻撃でも地形でもなくイベント用）
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("CoinHit",
		Math::Vector3::Zero,
		ItemConst::CoinHitRadius,
		KdCollider::TypeEvent);

	// 星きらめきエフェクト（コインは Effekseer ループなし＝星だけ）
	// 大きめ・金色・色幅ランダム
	ItemEffect::Params fxp;
	fxp.starSize    = SparkleConst::CoinStarSize;
	fxp.orbitRadius = SparkleConst::CoinStarRadius;
	fxp.color       = { SparkleConst::CoinColorR, SparkleConst::CoinColorG,
						SparkleConst::CoinColorB, SparkleConst::CoinColorA };
	fxp.colorShift  = SparkleConst::CoinColorShift;
	m_effect.Init(m_spawnPos, fxp);
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

	m_effect.Update(pos, KdFPSController::GetDt());
}

void Coin::DrawEffect()
{
	m_effect.DrawEffect(GetPos());
}

void Coin::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	const Math::Vector3 pos   = GetPos();
	const Math::Matrix  rot   = Math::Matrix::CreateRotationY(m_rotAngle);
	const Math::Matrix  trans = Math::Matrix::CreateTranslation(pos);

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, rot * trans);
}

void Coin::DrawOutline()
{
	if (!m_modelWork.IsEnable()) { return; }

	const Math::Vector3 pos   = GetPos();
	const Math::Matrix  rot   = Math::Matrix::CreateRotationY(m_rotAngle);
	const Math::Matrix  trans = Math::Matrix::CreateTranslation(pos);

	auto& shader = KdShaderManager::Instance().m_StandardShader;
	shader.SetOutlineWidth(OutlineConst::Width);
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
	shader.DrawModel(m_modelWork, rot * trans, c, Math::Vector3::Zero);
}
