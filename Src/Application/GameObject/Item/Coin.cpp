#include "../../main.h"
#include "Coin.h"
#include "../../Const/OutlineConst.h"

// 向き → Z軸回りの基準回転角（ラジアン）
static float CoinDirAngleZ(Coin::CoinDir d)
{
	switch (d)
	{
	case Coin::CoinDir::Down:  return DirectX::XM_PI;
	case Coin::CoinDir::Left:  return DirectX::XM_PIDIV2;
	case Coin::CoinDir::Right: return -DirectX::XM_PIDIV2;
	default:                   return 0.0f;   // Up
	}
}

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
	// フレームレート非依存：60fps基準に換算（高FPSで回転・上下が速くなるのを防ぐ）
	const float fs = KdFPSController::GetDt() * 60.0f;
	m_bobTimer  += ItemConst::CoinBobSpeed   * fs;
	m_rotAngle  += ItemConst::CoinRotateSpeed * fs;
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
	const Math::Matrix  rot   = Math::Matrix::CreateRotationY(m_rotAngle)
							  * Math::Matrix::CreateRotationZ(CoinDirAngleZ(m_dir));
	const Math::Matrix  trans = Math::Matrix::CreateTranslation(pos);

	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, rot * trans);
}

void Coin::DrawOutline()
{
	if (!m_modelWork.IsEnable()) { return; }

	const Math::Vector3 pos   = GetPos();
	const Math::Matrix  rot   = Math::Matrix::CreateRotationY(m_rotAngle)
							  * Math::Matrix::CreateRotationZ(CoinDirAngleZ(m_dir));
	const Math::Matrix  trans = Math::Matrix::CreateTranslation(pos);

	auto& shader = KdShaderManager::Instance().m_StandardShader;
	shader.SetOutlineWidth(OutlineConst::Width);
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
	shader.DrawModel(m_modelWork, rot * trans, c, Math::Vector3::Zero);
}
