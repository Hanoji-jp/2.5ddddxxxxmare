#include "../../../Pch.h"
#include "GravityRingEffect.h"

//----------------------------------------------------------
// Update
//----------------------------------------------------------
void GravityRingEffect::Update()
{
	if (!m_pPlanet) { return; }

	constexpr float kDt    = 1.0f / 60.0f;
	constexpr float kTwoPi = 6.28318530f;

	if (m_pPlanet->Shape == PlanetShape::Sphere)
	{
		m_rotAngle += GravityRingConst::RotateSpeed * kDt;
		if (m_rotAngle > kTwoPi) { m_rotAngle -= kTwoPi; }
	}

	UpdateDebris();
	UpdateParticles();
}

//----------------------------------------------------------
// Draw
//----------------------------------------------------------
void GravityRingEffect::Draw()
{
	if (!m_pPlanet) { return; }

	if (m_pPlanet->Shape == PlanetShape::Sphere)
	{
		// デブリ・パーティクルのみ
	}
	else
	{
		// デブリ・パーティクルのみ
	}

	DrawDebris();
	DrawParticles();
}

//----------------------------------------------------------
// Sphere : GravityRadius 上を回る Box マーカーのリング
//----------------------------------------------------------
void GravityRingEffect::DrawSphereRing() const
{
	const float r = m_pPlanet->GravityRadius;
	if (r <= 0.0f) { return; }

	constexpr float kTwoPi = 6.28318530f;
	const Math::Vector3 kSize = { GravityRingConst::MarkerSize,
								  GravityRingConst::MarkerSize,
								  GravityRingConst::MarkerSize };
	KdDebugWireFrame wire;

	for (int i = 0; i < GravityRingConst::MarkerCount; ++i)
	{
		const float angle = m_rotAngle + (kTwoPi / GravityRingConst::MarkerCount) * i;
		const Math::Vector3 pos =
		{
			m_pPlanet->Position.x + std::cosf(angle) * r,
			m_pPlanet->Position.y + std::sinf(angle) * r,
			m_pPlanet->Position.z
		};
		Math::Matrix mat = Math::Matrix::Identity;
		mat.Translation(pos);
		wire.AddDebugBox(mat, kSize, Math::Vector3::Zero, false, GravityRingConst::MarkerColor);
	}

	wire.Draw();
}

//----------------------------------------------------------
// Box : 各面の外側に重力方向矢印を描画
//----------------------------------------------------------
void GravityRingEffect::DrawBoxArrows()
{
	const Math::Vector3& c = m_pPlanet->Position;
	const Math::Vector3& h = m_pPlanet->BoxHalfExtents;

	// 面ごとの（外向き法線, 面中心オフセット, 重力モード）
	struct FaceInfo
	{
		Math::Vector3      normal;
		Math::Vector3      centerOffset;
		BoxFaceGravityMode mode;
	};

	const FaceInfo faces[4] =
	{
		{ { 0.0f,  1.0f, 0.0f }, {  0.0f,  h.y, 0.0f }, m_pPlanet->BoxFaceGravityTop    },
		{ { 0.0f, -1.0f, 0.0f }, {  0.0f, -h.y, 0.0f }, m_pPlanet->BoxFaceGravityBottom },
		{ {-1.0f,  0.0f, 0.0f }, { -h.x,  0.0f, 0.0f }, m_pPlanet->BoxFaceGravityLeft   },
		{ { 1.0f,  0.0f, 0.0f }, {  h.x,  0.0f, 0.0f }, m_pPlanet->BoxFaceGravityRight  },
	};

	KdDebugWireFrame wire;

	for (const auto& f : faces)
	{
		Math::Vector3 arrowDir;
		Math::Color   col;
		ResolveFaceArrow(f.mode, f.normal, arrowDir, col);

		const Math::Vector3 origin = c + f.centerOffset + f.normal * GravityRingConst::ArrowOffset;

		// モデルが読み込み済みならモデルで描画、なければデバッグ矢印にフォールバック
		if (m_spArrowData)
		{
			// arrowDir 方向に Y 軸を向ける行列を構築
			Math::Vector3 up   = arrowDir;
			Math::Vector3 fwd  = { 0.0f, 0.0f, 1.0f };
			if (std::fabsf(up.Dot(fwd)) > 0.99f) { fwd = { 1.0f, 0.0f, 0.0f }; }
			Math::Vector3 right3;
			up.Cross(fwd, right3);   right3.Normalize();
			right3.Cross(up, fwd);   fwd.Normalize();

			Math::Matrix arrowMat;
			arrowMat.m[0][0] = right3.x; arrowMat.m[0][1] = right3.y; arrowMat.m[0][2] = right3.z; arrowMat.m[0][3] = 0.0f;
			arrowMat.m[1][0] = up.x;     arrowMat.m[1][1] = up.y;     arrowMat.m[1][2] = up.z;     arrowMat.m[1][3] = 0.0f;
			arrowMat.m[2][0] = fwd.x;    arrowMat.m[2][1] = fwd.y;    arrowMat.m[2][2] = fwd.z;    arrowMat.m[2][3] = 0.0f;
			arrowMat.m[3][0] = origin.x; arrowMat.m[3][1] = origin.y; arrowMat.m[3][2] = origin.z; arrowMat.m[3][3] = 1.0f;

			DrawArrowModel(arrowMat);
		}
		else
		{
			DrawArrow(wire, origin, arrowDir, col);
		}
	}

	wire.Draw();
}

//----------------------------------------------------------
// ResolveFaceArrow : モード → 矢印方向 & 色
//----------------------------------------------------------
void GravityRingEffect::ResolveFaceArrow(BoxFaceGravityMode  _mode,
										  const Math::Vector3& _faceNormal,
										  Math::Vector3&       _outDir,
										  Math::Color&         _outColor) const
{
	switch (_mode)
	{
	case BoxFaceGravityMode::Inward:
		// 面の内向き（プレイヤーを引き寄せる）
		_outDir   = -_faceNormal;
		_outColor = GravityRingConst::ColorInward;
		break;
	case BoxFaceGravityMode::Outward:
		// 面の外向き（弾き飛ばす）
		_outDir   = _faceNormal;
		_outColor = GravityRingConst::ColorOutward;
		break;
	case BoxFaceGravityMode::Inherit:
		_outDir   = { 0.0f, -1.0f, 0.0f };
		_outColor = GravityRingConst::ColorInherit;
		break;
	case BoxFaceGravityMode::Down:
		_outDir   = { 0.0f, -1.0f, 0.0f };
		_outColor = GravityRingConst::ColorDown;
		break;
	case BoxFaceGravityMode::Up:
		_outDir   = { 0.0f,  1.0f, 0.0f };
		_outColor = GravityRingConst::ColorUp;
		break;
	case BoxFaceGravityMode::Left:
		_outDir   = { -1.0f, 0.0f, 0.0f };
		_outColor = GravityRingConst::ColorLeft;
		break;
	case BoxFaceGravityMode::Right:
		_outDir   = {  1.0f, 0.0f, 0.0f };
		_outColor = GravityRingConst::ColorRight;
		break;
	default:
		_outDir   = -_faceNormal;
		_outColor = GravityRingConst::ColorInward;
		break;
	}
}

//----------------------------------------------------------
// DrawArrow : 軸線 + V字矢尻
//----------------------------------------------------------
void GravityRingEffect::DrawArrow(KdDebugWireFrame&    _wire,
								   const Math::Vector3& _origin,
								   const Math::Vector3& _dir,
								   const Math::Color&   _col) const
{
	const Math::Vector3 tip = _origin + _dir * GravityRingConst::ArrowLength;

	// 軸線
	_wire.AddDebugLine(_origin, tip, _col);

	// 矢尻（V字）：_dir と直交する軸を求める
	// 2Dゲームなので Z=0 断面で X/Y 方向に広げる
	Math::Vector3 perp;
	const Math::Vector3 worldZ = { 0.0f, 0.0f, 1.0f };
	_dir.Cross(worldZ, perp);
	if (perp.LengthSquared() < 0.0001f)
	{
		perp = { 1.0f, 0.0f, 0.0f };
	}
	else
	{
		perp.Normalize();
	}

	const Math::Vector3 headBase = tip - _dir * GravityRingConst::ArrowHeadLength;
	const Math::Vector3 left     = headBase + perp *  GravityRingConst::ArrowHeadWidth;
	const Math::Vector3 right    = headBase + perp * -GravityRingConst::ArrowHeadWidth;

	_wire.AddDebugLine(tip, left,  _col);
	_wire.AddDebugLine(tip, right, _col);
}

//----------------------------------------------------------
// DrawArrowModel : Map_Arrowモデルを指定ワールド行列で描画
//----------------------------------------------------------
void GravityRingEffect::DrawArrowModel(const Math::Matrix& _world)
{
	m_arrowWork.CalcNodeMatrices();
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_arrowWork, _world);
}

//----------------------------------------------------------
// UpdateDebris : 浮遊デブリの公転 + 引力方向ドリフト
//----------------------------------------------------------
void GravityRingEffect::UpdateDebris()
{
	if (!m_spBoxData) { return; }
	if (!m_pPlanet)   { return; }

	constexpr float kDt    = 1.0f / 60.0f;
	constexpr float kTwoPi = 6.28318530f;

	const float r = (m_pPlanet->Shape == PlanetShape::Sphere)
		? m_pPlanet->GravityRadius * GravityRingConst::DebrisRadiusRatio
		: m_pPlanet->SurfaceRadius * GravityRingConst::DebrisRadiusRatio;

	for (auto& d : m_debris)
	{
		// 公転
		d.orbitAngle += GravityRingConst::DebrisOrbitSpeed * kDt;
		if (d.orbitAngle > kTwoPi) { d.orbitAngle -= kTwoPi; }

		// 引力方向ドリフト（往復）
		d.driftOffset += GravityRingConst::DebrisDriftSpeed * d.driftDir * kDt;
		if (d.driftOffset >  GravityRingConst::DebrisDriftMax) { d.driftDir = -1.0f; }
		if (d.driftOffset < -GravityRingConst::DebrisDriftMax) { d.driftDir =  1.0f; }

		// 位置計算：惑星中心から公転角 + ドリフト
		const Math::Vector3 pos =
		{
			m_pPlanet->Position.x + std::cosf(d.orbitAngle) * (r + d.driftOffset),
			m_pPlanet->Position.y + std::sinf(d.orbitAngle) * (r + d.driftOffset),
			m_pPlanet->Position.z
		};

		Math::Matrix mat = Math::Matrix::CreateScale(GravityRingConst::DebrisScale);
		mat.Translation(pos);
		d.cachedPos = pos;
	}
}

//----------------------------------------------------------
// DrawDebris
//----------------------------------------------------------
void GravityRingEffect::DrawDebris()
{
	if (!m_spBoxData) { return; }
	if (!m_pPlanet)   { return; }
	for (auto& d : m_debris)
	{
		if (!d.work.IsEnable()) { continue; }
		d.work.CalcNodeMatrices();
		Math::Matrix world = Math::Matrix::CreateScale(GravityRingConst::DebrisScale);
		world.Translation(d.cachedPos);
		KdShaderManager::Instance().m_StandardShader.DrawModel(d.work, world);
	}
}

//----------------------------------------------------------
// UpdateParticles : 引力方向へ流れるパーティクル
//----------------------------------------------------------
void GravityRingEffect::UpdateParticles()
{
	if (!m_spBoxData) { return; }
	if (!m_pPlanet)   { return; }

	constexpr float kDt    = 1.0f / 60.0f;
	constexpr float kTwoPi = 6.28318530f;

	// スポーン
	m_spawnTimer += kDt;
	if (m_spawnTimer >= GravityRingConst::ParticleSpawnInterval)
	{
		m_spawnTimer = 0.0f;

		// 非アクティブスロットを探してスポーン
		for (auto& p : m_particles)
		{
			if (p.active) { continue; }

			// 惑星周囲をランダム角でスポーン
			const float spawnAngle = static_cast<float>(std::rand() % 1000) / 1000.0f * kTwoPi;
			const float spawnR     = (m_pPlanet->Shape == PlanetShape::Sphere)
				? m_pPlanet->GravityRadius * GravityRingConst::ParticleSpawnRadiusRatio
				: m_pPlanet->SurfaceRadius * GravityRingConst::ParticleSpawnRadiusRatio;

			p.pos =
			{
				m_pPlanet->Position.x + std::cosf(spawnAngle) * spawnR,
				m_pPlanet->Position.y + std::sinf(spawnAngle) * spawnR,
				m_pPlanet->Position.z
			};

			// 惑星中心方向（引力方向）へ向かう速度
			Math::Vector3 toCenter = m_pPlanet->Position - p.pos;
			toCenter.Normalize();
			p.velocity = toCenter * GravityRingConst::ParticleSpeed;
			p.life     = GravityRingConst::ParticleLifetime;
			p.active   = true;
			break;
		}
	}

	// 更新
	for (auto& p : m_particles)
	{
		if (!p.active) { continue; }
		p.pos  += p.velocity * kDt;
		p.life -= kDt;
		if (p.life <= 0.0f) { p.active = false; }
	}
}

//----------------------------------------------------------
// DrawParticles
//----------------------------------------------------------
void GravityRingEffect::DrawParticles()
{
	if (!m_spBoxData) { return; }
	if (!m_pPlanet)   { return; }
	for (auto& p : m_particles)
	{
		if (!p.active)          { continue; }
		if (!p.work.IsEnable()) { continue; }

		// 寿命に応じてフェードスケール
		const float lifeRatio = p.life / GravityRingConst::ParticleLifetime;
		const float scale     = GravityRingConst::ParticleScale * lifeRatio;

		Math::Matrix mat = Math::Matrix::CreateScale(scale);
		mat.Translation(p.pos);
		p.work.CalcNodeMatrices();
		KdShaderManager::Instance().m_StandardShader.DrawModel(p.work, mat);
	}
}
