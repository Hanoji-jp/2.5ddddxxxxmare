#include "../../../Pch.h"
#include "FootDust.h"

// 0.0〜1.0 の一様乱数
static float Rand01()
{
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

// -1.0〜1.0 の一様乱数
static float RandSym()
{
	return (Rand01() - 0.5f) * 2.0f;
}

void FootDust::Spawn(const Math::Vector3&              pos,
					 const Math::Vector3&              backDir,
					 const Math::Vector3&              upDir,
					 const std::shared_ptr<KdModelData>& modelData)
{
	if (modelData) { m_modelWork.SetModelData(modelData); }

	// 初期スケール：ランダム
	m_scale = JuiceConst::DustScaleMin
			+ Rand01() * (JuiceConst::DustScaleMax - JuiceConst::DustScaleMin);

	// 縮み速度：遅い粒〜速い粒（ふわっと残るものとすぐ消えるものを混在）
	m_scaleShrink = 1.2f + Rand01() * 4.0f;

	// 左右軸
	Math::Vector3 right = upDir.Cross(backDir);
	if (right.LengthSquared() < 1e-6f) { right = { 1.0f, 0.0f, 0.0f }; }
	right.Normalize();

	// スポーン座標：後方＋横にランダムばらつき
	const float spreadBack = Rand01() * JuiceConst::DustSpreadBack;
	const float spreadSide = RandSym() * JuiceConst::DustSpreadSide;
	m_pos = pos + backDir * spreadBack + right * spreadSide;

	// 速度：上昇量と横流れをそれぞれランダムに
	const float riseSpeed = JuiceConst::DustRiseSpeedMin
						  + Rand01() * (JuiceConst::DustRiseSpeedMax - JuiceConst::DustRiseSpeedMin);
	const float driftSpeed = RandSym() * JuiceConst::DustDriftSpeed;
	m_vel = upDir * riseSpeed + right * driftSpeed;

	// ライフタイムもランダムでばらつかせる
	m_lifetime = JuiceConst::DustLifetimeMin
			   + Rand01() * (JuiceConst::DustLifetimeMax - JuiceConst::DustLifetimeMin);

	m_alpha = JuiceConst::DustAlphaStart;

	// 白〜グレーのランダム色
	m_gray = JuiceConst::DustGrayMin
		   + Rand01() * (JuiceConst::DustGrayMax - JuiceConst::DustGrayMin);
}

void FootDust::Update()
{
	constexpr float kDt = 1.0f / 60.0f;

	// 速度で移動
	m_pos += m_vel;

	// 徐々に縮む（粒ごとの速度）
	m_scale *= (1.0f - kDt * m_scaleShrink);

	// ライフタイム経過でフェードアウト
	m_lifetime -= kDt;
	const float ratio = std::max(0.0f, m_lifetime / JuiceConst::DustLifetimeMax);
	m_alpha = JuiceConst::DustAlphaStart * ratio;

	if (m_lifetime <= 0.0f || m_scale < 0.001f) { m_isExpired = true; }
}

void FootDust::DrawEffect()
{
	if (!m_modelWork.IsEnable() || m_alpha <= 0.0f || m_scale <= 0.0f) { return; }

	const Math::Matrix world = Math::Matrix::CreateScale(m_scale)
							 * Math::Matrix::CreateTranslation(m_pos);

	const Math::Color   colRate { 1.0f, 1.0f, 1.0f, m_alpha };
	const Math::Vector3 emissive{ m_gray, m_gray, m_gray };
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, world, colRate, emissive);
}
