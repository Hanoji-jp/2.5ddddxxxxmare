#include "../../../Pch.h"
#include "DamageBurst.h"

// 0.0〜1.0 の一様乱数
static float DBRand01()
{
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void DamageBurst::Spawn(const Math::Vector3&              center,
						const Math::Vector3&              upDir,
						const std::shared_ptr<KdModelData>& modelData)
{
	SetPos(center);   // CheckInScreen がオブジェクト位置を m_mWorld から取るため必須

	m_up = upDir;
	if (m_up.LengthSquared() < 1e-6f) { m_up = { 0.0f, 1.0f, 0.0f }; }
	m_up.Normalize();

	m_particles.reserve(JuiceConst::DmgBurstCount);

	// upDir に直交する右・前ベクトル（球面分布の基底）
	Math::Vector3 right = m_up.Cross(Math::Vector3::UnitZ);
	if (right.LengthSquared() < 1e-4f) { right = m_up.Cross(Math::Vector3::UnitX); }
	right.Normalize();
	const Math::Vector3 fwd = m_up.Cross(right);

	for (int i = 0; i < JuiceConst::DmgBurstCount; ++i)
	{
		Particle p;
		if (modelData) { p.modelWork.SetModelData(modelData); }

		// 全方向ランダム（球面分布）
		const float theta = DBRand01() * DirectX::XM_2PI;
		const float phi   = (DBRand01() - 0.5f) * DirectX::XM_PI;
		const float cosPhi = std::cos(phi);
		Math::Vector3 dir =  right * (cosPhi * std::cos(theta))
						  +  fwd   * (cosPhi * std::sin(theta))
						  +  m_up  * std::sin(phi);
		dir.Normalize();

		// スポーン位置は体の半径内にばらつかせる＝体全体から出る感じ
		p.pos = center + dir * (DBRand01() * JuiceConst::DmgBurstBodyRadius);

		const float speed = JuiceConst::DmgBurstSpeedMin
						  + DBRand01() * (JuiceConst::DmgBurstSpeedMax - JuiceConst::DmgBurstSpeedMin);
		p.vel = dir * speed + m_up * JuiceConst::DmgBurstUpBias;   // 少し上向きバイアス

		p.scale       = JuiceConst::DmgBurstScaleMin
					  + DBRand01() * (JuiceConst::DmgBurstScaleMax - JuiceConst::DmgBurstScaleMin);
		p.scaleShrink = JuiceConst::DmgBurstShrinkMin
					  + DBRand01() * (JuiceConst::DmgBurstShrinkMax - JuiceConst::DmgBurstShrinkMin);
		p.lifetime    = JuiceConst::DmgBurstLifetimeMin
					  + DBRand01() * (JuiceConst::DmgBurstLifetimeMax - JuiceConst::DmgBurstLifetimeMin);
		p.alpha       = 1.0f;
		p.spin        = DBRand01() * DirectX::XM_2PI;
		p.spinSpeed   = (DBRand01() - 0.5f) * 2.0f * JuiceConst::DmgBurstSpin;

		m_particles.push_back(std::move(p));
	}
}

void DamageBurst::Update()
{
	const float kDt = KdFPSController::GetDt();

	for (auto& p : m_particles)
	{
		if (p.lifetime <= 0.0f) { continue; }

		p.pos      += p.vel;
		p.vel      -= m_up * JuiceConst::DmgBurstGravity;   // 重力で落ちる
		p.scale    *= (1.0f - kDt * p.scaleShrink);
		p.spin     += p.spinSpeed * kDt;
		p.lifetime -= kDt;

		const float ratio = std::max(0.0f, p.lifetime / JuiceConst::DmgBurstLifetimeMax);
		p.alpha = ratio;
	}

	const bool allDead = std::all_of(m_particles.begin(), m_particles.end(),
		[](const Particle& p) { return p.lifetime <= 0.0f || p.scale < 0.001f; });
	if (allDead) { m_isExpired = true; }
}

void DamageBurst::DrawEffect()
{
	// 発光は控えめ（box本体の色を濃い赤にして白飛びを防ぐ）
	const float emv = JuiceConst::DmgBurstEmissive;
	const Math::Vector3 emissive{ JuiceConst::DmgBurstColR * emv,
								  JuiceConst::DmgBurstColG * emv,
								  JuiceConst::DmgBurstColB * emv };

	for (auto& p : m_particles)
	{
		if (!p.modelWork.IsEnable() || p.alpha <= 0.0f || p.scale < 0.001f) { continue; }

		const Math::Matrix world = Math::Matrix::CreateScale(p.scale)
								 * Math::Matrix::CreateFromYawPitchRoll(p.spin, p.spin * 0.7f, p.spin * 0.3f)
								 * Math::Matrix::CreateTranslation(p.pos);

		// box本体を濃い赤に着色（白モデルへ乗算）
		const Math::Color colRate{ JuiceConst::DmgBurstColR,
								   JuiceConst::DmgBurstColG,
								   JuiceConst::DmgBurstColB,
								   p.alpha };
		KdShaderManager::Instance().m_StandardShader.DrawModel(p.modelWork, world, colRate, emissive);
	}
}
