#include "../../../Pch.h"
#include "StarBurstEffect.h"

// 0.0〜1.0 の一様乱数
static float SBRand01()
{
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void StarBurstEffect::Spawn(const Math::Vector3&              pos,
							const Math::Vector3&              upDir,
							const std::shared_ptr<KdModelData>& modelData)
{
	m_particles.reserve(JuiceConst::StarBurstCount);

	// upDir に直交する右・前ベクトルを作る
	Math::Vector3 right = upDir.Cross(Math::Vector3::UnitZ);
	if (right.LengthSquared() < 1e-4f)
		right = upDir.Cross(Math::Vector3::UnitX);
	right.Normalize();
	const Math::Vector3 fwd = upDir.Cross(right);

	for (int i = 0; i < JuiceConst::StarBurstCount; ++i)
	{
		Particle p;

		if (modelData) { p.modelWork.SetModelData(modelData); }

		// 全方向ランダム飛散（球面分布）
		const float theta = SBRand01() * DirectX::XM_2PI;       // 水平角
		const float phi   = (SBRand01() - 0.5f) * DirectX::XM_PI; // 仰角
		const float speed = JuiceConst::StarBurstSpeedMin
						  + SBRand01() * (JuiceConst::StarBurstSpeedMax - JuiceConst::StarBurstSpeedMin);

		// 球面→直交座標
		const float cosPhi = std::cos(phi);
		Math::Vector3 dir =  right * (cosPhi * std::cos(theta))
						  +  fwd   * (cosPhi * std::sin(theta))
						  +  upDir * std::sin(phi);
		dir.Normalize();

		p.pos      = pos;
		p.vel      = dir * speed;
		p.scale    = JuiceConst::StarBurstScaleMin
				   + SBRand01() * (JuiceConst::StarBurstScaleMax - JuiceConst::StarBurstScaleMin);
		p.scaleShrink = JuiceConst::StarBurstShrinkMin
					  + SBRand01() * (JuiceConst::StarBurstShrinkMax - JuiceConst::StarBurstShrinkMin);
		p.lifetime = JuiceConst::StarBurstLifetimeMin
				   + SBRand01() * (JuiceConst::StarBurstLifetimeMax - JuiceConst::StarBurstLifetimeMin);
		p.alpha    = 1.0f;

		m_particles.push_back(std::move(p));
	}
}

void StarBurstEffect::Update()
{
	constexpr float kDt = 1.0f / 60.0f;

	for (auto& p : m_particles)
	{
		if (p.lifetime <= 0.0f) { continue; }

		p.pos      += p.vel;
		p.scale    *= (1.0f - kDt * p.scaleShrink);
		p.lifetime -= kDt;

		const float ratio = std::max(0.0f, p.lifetime / JuiceConst::StarBurstLifetimeMax);
		p.alpha = ratio;
	}

	// 全粒が消えたら自身も削除
	const bool allDead = std::all_of(m_particles.begin(), m_particles.end(),
		[](const Particle& p) { return p.lifetime <= 0.0f || p.scale < 0.001f; });
	if (allDead) { m_isExpired = true; }
}

void StarBurstEffect::DrawEffect()
{
	// 青白い発光色（R:0.6 G:0.8 B:1.0 → 青白）
	constexpr float kR = 0.6f;
	constexpr float kG = 0.9f;
	constexpr float kB = 1.0f;
	const float emv = JuiceConst::StarBurstEmissive;

	for (auto& p : m_particles)
	{
		if (!p.modelWork.IsEnable() || p.alpha <= 0.0f || p.scale < 0.001f) { continue; }

		const Math::Matrix world = Math::Matrix::CreateScale(p.scale)
								 * Math::Matrix::CreateTranslation(p.pos);

		const Math::Color   colRate { 1.0f, 1.0f, 1.0f, p.alpha };
		const Math::Vector3 emissive{ kR * emv, kG * emv, kB * emv };

		KdShaderManager::Instance().m_StandardShader.DrawModel(
			p.modelWork, world, colRate, emissive);
	}
}
