#include "../../../Pch.h"
#include "WarpHole.h"

static constexpr float kTwoPi = 6.28318530718f;

// トランペットのベル形状：口元が大きく広がり → 急速に絞られ → 細い管で安定
static constexpr float kRadiusTable[] = { 1.0f, 0.55f, 0.32f, 0.27f, 0.25f, 0.25f, 0.25f, 0.25f };
static constexpr int   kRadiusTableSize = static_cast<int>(std::size(kRadiusTable));

//----------------------------------------------------------
// 色ヘルパー: Math::Color → unsigned int (ABGR)
//----------------------------------------------------------
static unsigned int ColorToUint(const Math::Color& c)
{
	const unsigned char r = static_cast<unsigned char>(std::min(c.R() * 255.0f, 255.0f));
	const unsigned char g = static_cast<unsigned char>(std::min(c.G() * 255.0f, 255.0f));
	const unsigned char b = static_cast<unsigned char>(std::min(c.B() * 255.0f, 255.0f));
	const unsigned char a = static_cast<unsigned char>(std::min(c.A() * 255.0f, 255.0f));
	return (static_cast<unsigned int>(a) << 24)
		 | (static_cast<unsigned int>(b) << 16)
		 | (static_cast<unsigned int>(g) << 8)
		 | static_cast<unsigned int>(r);
}

//==========================================================
WarpHole::WarpHole(const WarpHoleData& data)
	: m_data(data)
{
}

//----------------------------------------------------------
void WarpHole::Init()
{
	m_boxModel.SetModelData(WarpHoleConst::BoxModelPath);
	InitParticles();
	InitExitParticles();

	// ローポリファネル用ランダム頂点オフセット（固定シード）
	const int jitterCount = WarpHoleConst::FunnelRings * WarpHoleConst::FunnelSegments;
	m_entryJitter.resize(jitterCount);
	m_exitJitter.resize(jitterCount);
	for (int i = 0; i < jitterCount; ++i)
	{
		m_entryJitter[i] = KdRandom::GetFloat(-WarpHoleConst::FunnelJitter, WarpHoleConst::FunnelJitter);
		m_exitJitter[i]  = KdRandom::GetFloat(-WarpHoleConst::FunnelJitter, WarpHoleConst::FunnelJitter);
	}

	// トンネル用 jitter（TunnelRings × FunnelSegments）
	const int tunnelJitterCount = WarpHoleConst::TunnelRings * WarpHoleConst::FunnelSegments;
	m_tunnelJitter.resize(tunnelJitterCount);
	for (int i = 0; i < tunnelJitterCount; ++i)
	{
		m_tunnelJitter[i] = KdRandom::GetFloat(-WarpHoleConst::FunnelJitter, WarpHoleConst::FunnelJitter);
	}
}

//----------------------------------------------------------
void WarpHole::InitParticles()
{
	m_particles.resize(WarpHoleConst::ParticleCount);
	for (auto& p : m_particles)
	{
		SpawnParticle(p);
		// 初期位相をバラけさせる（全部同時スポーンを避ける）
		const float phase = KdRandom::GetFloat(0.0f, WarpHoleConst::ParticleSpawnRadius);
		const Math::Vector3 toEntry = m_data.EntryPos - p.Pos;
		if (toEntry.LengthSquared() > 1e-6f)
		{
			p.Pos += toEntry * (phase / WarpHoleConst::ParticleSpawnRadius);
		}
	}
}

//----------------------------------------------------------
void WarpHole::SpawnParticle(BoxParticle& p) const
{
	// 口元の「外向き」方向（ラッパの先から外側）
	// GetEntryMouthDir() は EntryPos→ExitPos 方向なので、
	// 外側（プレイヤーが来る側）は その逆方向
	const Math::Vector3 mouthFwd = -m_data.GetEntryMouthDir(); // 外向き

	// mouthFwd を基準にした接線・従法線を作る
	Math::Vector3 tangent, bitangent;
	MakeBasis(mouthFwd, tangent, bitangent);

	// 前方半球上のランダム方向を生成
	// cosTheta を [0,1] にすることで前方半球に限定
	const float cosTheta = KdRandom::GetFloat(0.0f, 1.0f);
	const float sinTheta = std::sqrtf(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi      = KdRandom::GetFloat(0.0f, kTwoPi);

	const Math::Vector3 dir =
		mouthFwd  * cosTheta +
		tangent   * (sinTheta * std::cosf(phi)) +
		bitangent * (sinTheta * std::sinf(phi));

	const float r = WarpHoleConst::ParticleSpawnRadius;
	p.Pos      = m_data.EntryPos + dir * r;
	p.Velocity = Math::Vector3::Zero;

	// ランダムな回転軸（正規化）
	Math::Vector3 axis = {
		KdRandom::GetFloat(-1.0f, 1.0f),
		KdRandom::GetFloat(-1.0f, 1.0f),
		KdRandom::GetFloat(-1.0f, 1.0f) };
	if (axis.LengthSquared() < 1e-6f) { axis = Math::Vector3::Up; }
	axis.Normalize();
	p.RotAxis  = axis;
	p.RotAngle = KdRandom::GetFloat(0.0f, kTwoPi);
}

//----------------------------------------------------------
void WarpHole::InitExitParticles()
{
	m_exitParticles.resize(WarpHoleConst::ExitParticleCount);
	for (auto& p : m_exitParticles)
	{
		p.Active = false;
	}
}

//----------------------------------------------------------
void WarpHole::SpawnExitParticle(ExitParticle& p) const
{
	const Math::Vector3 mouthDir = -m_data.GetExitMouthDir(); // Exit→Entry を反転して外向きに
	Math::Vector3 tangent, bitangent;
	MakeBasis(mouthDir, tangent, bitangent);

	// 口元付近からランダムにオフセット
	const float ox = KdRandom::GetFloat(-WarpHoleConst::FunnelInnerRadius,
										 WarpHoleConst::FunnelInnerRadius);
	const float oy = KdRandom::GetFloat(-WarpHoleConst::FunnelInnerRadius,
										 WarpHoleConst::FunnelInnerRadius);
	p.Pos = m_data.ExitPos + tangent * ox + bitangent * oy;

	// 外向きに初速 + 横方向ランダム広がり
	const float sx = KdRandom::GetFloat(-WarpHoleConst::ExitParticleSpread,
										 WarpHoleConst::ExitParticleSpread);
	const float sy = KdRandom::GetFloat(-WarpHoleConst::ExitParticleSpread,
										 WarpHoleConst::ExitParticleSpread);
	p.Velocity = mouthDir * WarpHoleConst::ExitParticleInitSpeed
			   + tangent   * sx
			   + bitangent * sy;

	Math::Vector3 axis = {
		KdRandom::GetFloat(-1.0f, 1.0f),
		KdRandom::GetFloat(-1.0f, 1.0f),
		KdRandom::GetFloat(-1.0f, 1.0f) };
	if (axis.LengthSquared() < 1e-6f) { axis = Math::Vector3::Up; }
	axis.Normalize();
	p.RotAxis  = axis;
	p.RotAngle = KdRandom::GetFloat(0.0f, kTwoPi);
	p.Life     = WarpHoleConst::ExitParticleLifeTime;
	p.Active   = true;
}

//----------------------------------------------------------
void WarpHole::UpdateExitParticles()
{


	// タイマーで新規スポーン
	const float dt = KdFPSController::GetDt();
	m_exitSpawnTimer -= dt;
	if (m_exitSpawnTimer <= 0.0f)
	{
		m_exitSpawnTimer = WarpHoleConst::ExitParticleSpawnInterval;
		// 非アクティブなスロットに生成
		for (auto& p : m_exitParticles)
		{
			if (!p.Active)
			{
				SpawnExitParticle(p);
				break;
			}
		}
	}

	// 更新
	for (auto& p : m_exitParticles)
	{
		if (!p.Active) { continue; }

		p.Life -= dt;
		if (p.Life <= 0.0f)
		{
			p.Active = false;
			continue;
		}

		// 速度減衰（空気抵抗風）
		constexpr float kDamping = 0.97f;
		p.Velocity *= kDamping;
		p.Pos      += p.Velocity * dt;
		p.RotAngle += WarpHoleConst::ParticleRotSpeed * dt;
	}
}

//----------------------------------------------------------
void WarpHole::UpdateParticles()
{
	for (auto& p : m_particles)
	{
		const Math::Vector3 toEntry = m_data.EntryPos - p.Pos;
		const float distSq = toEntry.LengthSquared();

		// EntryPosに到達したらリスポーン
		if (distSq < WarpHoleConst::ParticleReachDist * WarpHoleConst::ParticleReachDist)
		{
			SpawnParticle(p);
			continue;
		}

		// 距離が近いほど引力が強くなる（inverse-square 風）
		const float dist   = std::sqrtf(distSq);
		const float accel  = WarpHoleConst::ParticleSuckAccel / std::max(dist, 0.5f);
		const Math::Vector3 dir = toEntry / dist;
		const float dt = KdFPSController::GetDt();
		p.Velocity += dir * accel * dt;
		p.Pos      += p.Velocity * dt;

		// 回転アニメ
		p.RotAngle += WarpHoleConst::ParticleRotSpeed * dt;
	}
}

//----------------------------------------------------------
void WarpHole::DrawLit()
{
	if (!m_data.Enabled || m_dead) { return; }

	// パーティクルをシアン色で描画（tintでベースカラーをシアンに染める）
	const Math::Color particleTint = {
		WarpHoleConst::EntryColorR,
		WarpHoleConst::EntryColorG,
		WarpHoleConst::EntryColorB,
		1.0f };
	const Math::Vector3 zeroEmissive = { 0.0f, 0.0f, 0.0f };

	const float s = WarpHoleConst::ParticleScale;
	for (const auto& p : m_particles)
	{
		const Math::Matrix rot   = Math::Matrix::CreateFromAxisAngle(p.RotAxis, p.RotAngle);
		const Math::Matrix scale = Math::Matrix::CreateScale(s, s, s);
		const Math::Matrix trans = Math::Matrix::CreateTranslation(p.Pos);
		const Math::Matrix world = scale * rot * trans;
		KdShaderManager::Instance().m_StandardShader.DrawModel(m_boxModel, world, particleTint, zeroEmissive);
	}

	// EXIT 吐き出しパーティクル（出口色：青紫系）
	const Math::Color exitTint = {
		WarpHoleConst::ExitColorR,
		WarpHoleConst::ExitColorG,
		WarpHoleConst::ExitColorB,
		1.0f };
	const float es = WarpHoleConst::ExitParticleScale;
	for (const auto& p : m_exitParticles)
	{
		if (!p.Active) { continue; }
		const float alpha = p.Life / WarpHoleConst::ExitParticleLifeTime;
		const Math::Color fadeTint = { exitTint.R(), exitTint.G(), exitTint.B(), alpha };
		const Math::Matrix rot   = Math::Matrix::CreateFromAxisAngle(p.RotAxis, p.RotAngle);
		const Math::Matrix scale = Math::Matrix::CreateScale(es, es, es);
		const Math::Matrix trans = Math::Matrix::CreateTranslation(p.Pos);
		const Math::Matrix world = scale * rot * trans;
		KdShaderManager::Instance().m_StandardShader.DrawModel(m_boxModel, world, fadeTint, zeroEmissive);
	}
}

//----------------------------------------------------------
void WarpHole::Update()
{
	if (!m_data.Enabled || m_dead) { return; }

	const float dt = KdFPSController::GetDt();

	// ワンウェイ通過後：収縮して消える
	if (m_consuming)
	{
		m_consumeScale -= WarpHoleConst::ConsumeShrinkSpeed * dt;
		if (m_consumeScale <= 0.0f)
		{
			m_consumeScale = 0.0f;
			m_dead = true;   // 完全消滅（以降は描画・判定なし）
			return;
		}
	}

	m_animOffset += WarpHoleConst::AnimSpeed * dt;
	if (m_animOffset > 1.0f) { m_animOffset -= 1.0f; }

	UpdateParticles();
	UpdateExitParticles();
}

//----------------------------------------------------------
void WarpHole::DrawEffect()
{
	if (!m_data.Enabled || m_dead) { return; }

	auto& shaderMgr = KdShaderManager::Instance();
	shaderMgr.ChangeBlendState(KdBlendState::Add);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	const Math::Color entryCol = { WarpHoleConst::EntryColorR,
								   WarpHoleConst::EntryColorG,
								   WarpHoleConst::EntryColorB,
								   WarpHoleConst::EntryColorA };
	const Math::Color exitCol  = { WarpHoleConst::ExitColorR,
								   WarpHoleConst::ExitColorG,
								   WarpHoleConst::ExitColorB,
								   WarpHoleConst::ExitColorA };

	// ローポリファネル（正面向きロート）を Entry/Exit に描画
	// OneWay はゲームプレイ的一方通行フラグであり、視覚描画には影響しない
	DrawFunnelFace(m_data.EntryPos, m_data.GetEntryMouthDir(),  entryCol, m_entryJitter, m_animOffset);
	DrawFunnelFace(m_data.ExitPos,  m_data.GetExitMouthDir(),   exitCol,  m_exitJitter,  m_animOffset);

	// トンネル（ファネル奥端同士をつなぐ面＋ワイヤーチューブ）
	// パス両端をファネル奥中心に固定してギャップをなくす
	{
		std::vector<Math::Vector3> path = BuildTunnelCenterPath();
		if (path.size() >= 2)
		{
			const float fl = WarpHoleConst::FunnelLength;
			path = TrimTunnelPath(path, fl);
			if (path.size() >= 2)
			{
				path.front() = m_data.EntryPos + m_data.GetEntryMouthDir() * fl;
				path.back()  = m_data.ExitPos  + m_data.GetExitMouthDir()  * fl;
			}
		}
		DrawTunnelFace(path, entryCol, exitCol, m_tunnelJitter, m_animOffset);

		// ブリッジ：ファネル奥端リングとトンネル端点リングを三角形で接続
		if (path.size() >= 2)
		{
			// Entry 側：トンネル先頭の接線
			{
				Math::Vector3 fwd = path[1] - path[0];
				if (fwd.LengthSquared() < 1e-8f) fwd = m_data.GetEntryMouthDir();
				else fwd.Normalize();
				Math::Vector3 tTang, tBitan;
				MakeBasis(fwd, tTang, tBitan);
				DrawFunnelTunnelBridge(
					m_data.EntryPos, m_data.GetEntryMouthDir(),
					path.front(), tTang, tBitan, entryCol);
			}
			// Exit 側：トンネル末端の接線
			{
				const int n = static_cast<int>(path.size());
				Math::Vector3 fwd = path[n - 1] - path[n - 2];
				if (fwd.LengthSquared() < 1e-8f) fwd = m_data.GetExitMouthDir();
				else fwd.Normalize();
				Math::Vector3 tTang, tBitan;
				MakeBasis(fwd, tTang, tBitan);
				DrawFunnelTunnelBridge(
					m_data.ExitPos, m_data.GetExitMouthDir(),
					path.back(), tTang, tBitan, exitCol);
			}
		}
	}

	shaderMgr.UndoBlendState();
	shaderMgr.UndoDepthStencilState();
}

//----------------------------------------------------------
// Bloomパス：ファネルのワイヤーを薄く再描画してにじみを出す
//----------------------------------------------------------
void WarpHole::DrawBright()
{
	if (!m_data.Enabled || m_dead) { return; }

	auto& shaderMgr = KdShaderManager::Instance();
	shaderMgr.ChangeBlendState(KdBlendState::Add);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	// Bloomパス用：ワイヤーを少し太めのアルファ・明るい色で再描画
	// BeginBright/EndBright 内なのでポストプロセスでぼかされてにじむ
	const Math::Color entryBloom = { WarpHoleConst::EntryColorR * 1.5f,
									 WarpHoleConst::EntryColorG * 1.5f,
									 WarpHoleConst::EntryColorB * 1.5f,
									 0.8f };
	const Math::Color exitBloom  = { WarpHoleConst::ExitColorR * 1.5f,
									 WarpHoleConst::ExitColorG * 1.5f,
									 WarpHoleConst::ExitColorB * 1.5f,
									 0.8f };

	// 口元ほど明るく→奥でフェードアウトするグラデーションBloom
	DrawFunnelFace(m_data.EntryPos, m_data.GetEntryMouthDir(), entryBloom, m_entryJitter, m_animOffset, false);
	DrawFunnelFace(m_data.ExitPos,  m_data.GetExitMouthDir(),  exitBloom,  m_exitJitter,  m_animOffset, false);

	// パーティクルも Bloom パスで描画してにじませる
	const Math::Color particleBloom = {
		WarpHoleConst::EntryColorR * WarpHoleConst::ParticleEmissive,
		WarpHoleConst::EntryColorG * WarpHoleConst::ParticleEmissive,
		WarpHoleConst::EntryColorB * WarpHoleConst::ParticleEmissive,
		1.0f };
	const Math::Vector3 zeroEmi = { 0.0f, 0.0f, 0.0f };
	const float s = WarpHoleConst::ParticleScale;
	for (const auto& p : m_particles)
	{
		const Math::Matrix rot   = Math::Matrix::CreateFromAxisAngle(p.RotAxis, p.RotAngle);
		const Math::Matrix scale = Math::Matrix::CreateScale(s, s, s);
		const Math::Matrix trans = Math::Matrix::CreateTranslation(p.Pos);
		const Math::Matrix world = scale * rot * trans;
		shaderMgr.m_StandardShader.DrawModel(m_boxModel, world, particleBloom, zeroEmi);
	}

	// EXIT パーティクルも bloom パスへ
	const Math::Color exitBloomP = {
		WarpHoleConst::ExitColorR * WarpHoleConst::ParticleEmissive,
		WarpHoleConst::ExitColorG * WarpHoleConst::ParticleEmissive,
		WarpHoleConst::ExitColorB * WarpHoleConst::ParticleEmissive,
		1.0f };
	const float es = WarpHoleConst::ExitParticleScale;
	for (const auto& p : m_exitParticles)
	{
		if (!p.Active) { continue; }
		const float alpha = p.Life / WarpHoleConst::ExitParticleLifeTime;
		const Math::Color fadeBloom = { exitBloomP.R(), exitBloomP.G(), exitBloomP.B(), alpha };
		const Math::Matrix rot   = Math::Matrix::CreateFromAxisAngle(p.RotAxis, p.RotAngle);
		const Math::Matrix scale = Math::Matrix::CreateScale(es, es, es);
		const Math::Matrix trans = Math::Matrix::CreateTranslation(p.Pos);
		const Math::Matrix world = scale * rot * trans;
		shaderMgr.m_StandardShader.DrawModel(m_boxModel, world, fadeBloom, zeroEmi);
	}

	shaderMgr.UndoBlendState();
	shaderMgr.UndoDepthStencilState();
}

//----------------------------------------------------------
void WarpHole::DrawDebug()
{
	if (!m_data.Enabled || m_dead) { return; }

	{
		KdDebugWireFrame wire;
		wire.AddDebugLine(m_data.EntryPos, m_data.ExitPos, { 1.0f, 1.0f, 0.0f, 1.0f });
		wire.Draw();
	}
	{
		const Math::Vector3 arrowEnd = m_data.ExitPos + m_data.ExitDir * 2.0f;
		KdDebugWireFrame wire;
		wire.AddDebugLine(m_data.ExitPos, arrowEnd, { 0.0f, 1.0f, 0.0f, 1.0f });
		wire.Draw();
	}
}

//----------------------------------------------------------
// ローポリファネル（正面向きロート）描画
//   center    : 口元の中心位置
//   mouthDir  : 口元が向く方向（奥に向かう向き）
//   col       : 基本色
//   jitterOffsets : FunnelRings×FunnelSegments の頂点半径オフセット比率
//----------------------------------------------------------
void WarpHole::DrawFunnelFace(const Math::Vector3& center,
							  const Math::Vector3& mouthDir,
							  const Math::Color&   col,
							  const std::vector<float>& jitterOffsets,
							  float animTime,
							  bool invertGradient) const
{
	const int   rings     = WarpHoleConst::FunnelRings;
	const int   segs      = WarpHoleConst::FunnelSegments;
	const float outerR    = WarpHoleConst::FunnelOuterRadius;
	const float innerR    = WarpHoleConst::FunnelInnerRadius;
	const float length    = WarpHoleConst::FunnelLength;
	const float faceAlpha = WarpHoleConst::FunnelFaceAlpha;
	const float wireAlpha = WarpHoleConst::FunnelWireAlpha;
	const float waveAmp   = WarpHoleConst::FunnelWaveAmp;
	const float waveFreqR = WarpHoleConst::FunnelWaveFreqRing;
	const float waveFreqS = WarpHoleConst::FunnelWaveFreqSeg;

	// mouthDirを基準に接線・従法線を作る
	Math::Vector3 tangent, bitangent;
	MakeBasis(mouthDir, tangent, bitangent);

	// 各リングの頂点位置を計算（波アニメあり）
	// ring=0 が口元（外側）、ring=rings-1 が奥（内側・暗い）
	auto GetVertex = [&](int ring, int seg) -> Math::Vector3
	{
		const float t      = static_cast<float>(ring) / static_cast<float>(rings - 1);
		const float radius = outerR + (innerR - outerR) * t;
		const float depth  = length * t;
		const float angle  = kTwoPi * static_cast<float>(seg) / static_cast<float>(segs);

		// 奥端リング（ring==rings-1）はトンネル端点と接続するため jitter/wave=0
		const bool isInnerRing = (ring == rings - 1);

		float jitter = 0.0f;
		float wave   = 0.0f;
		if (!isInnerRing)
		{
			const int idx = ring * segs + seg;
			jitter = (idx < static_cast<int>(jitterOffsets.size()))
					 ? jitterOffsets[idx] : 0.0f;

			const float wavePhase = kTwoPi * (waveFreqR * t - animTime)
								 + kTwoPi * waveFreqS * static_cast<float>(seg) / static_cast<float>(segs);
			wave = std::sinf(wavePhase) * waveAmp;
		}

		const float r = radius * (1.0f + jitter + wave) * m_consumeScale;   // 収縮消滅で縮む

		return center
			+ mouthDir  * (depth * m_consumeScale)
			+ tangent   * (r * std::cosf(angle))
			+ bitangent * (r * std::sinf(angle));
	};

	// 奥になるほど暗く・薄くする係数
	// invertGradient=true（Bloomパス）のときは逆：奥ほど明るく、口元でゼロ
	auto RingAlpha = [&](int ring) -> float
	{
		const float t = static_cast<float>(ring) / static_cast<float>(rings - 1);
		if (invertGradient)
		{
			// 口元(t=0)=0、奥(t=1)=faceAlpha のイーズイン曲線
			return faceAlpha * t * t;
		}
		return faceAlpha * (1.0f - t * 0.6f);
	};
	auto RingBright = [&](int ring) -> float
	{
		const float t = static_cast<float>(ring) / static_cast<float>(rings - 1);
		if (invertGradient)
		{
			return t;  // 口元=0、奥=1
		}
		return 1.0f - t * 0.65f;
	};

	// 面ごとにフラットな単色を割り当てる（6頂点全部同じ色）
	// seg・ring の組み合わせで大きく色が変わるように sin/cos でサイクル
	auto FaceColor = [&](int seg, int ring, float alpha) -> unsigned int
	{
		const float bright = RingBright(ring);
		// seg と ring を組み合わせてハッシュ的に色をばらす
		const float phase = kTwoPi * static_cast<float>(seg) / static_cast<float>(segs)
						  + static_cast<float>(ring) * 0.8f;
		// R: 0〜0.6 の範囲でサイクル（青〜シアン〜白のグラデーション）
		const float rVal = (std::sinf(phase) * 0.5f + 0.5f) * 0.6f;
		// G: ベース維持しつつ少しゆらす
		const float gVal = col.G() * (0.7f + std::cosf(phase * 0.7f) * 0.3f);
		// B: 常に高め
		const float bVal = col.B();
		const Math::Color c = {
			std::min(rVal * bright, 1.0f),
			std::min(gVal * bright, 1.0f),
			std::min(bVal * bright, 1.0f),
			alpha };
		return ColorToUint(c);
	};

	// ─── 三角ポリゴン面 ───
	m_vtxBuf.clear();
	m_vtxBuf.reserve(static_cast<size_t>(rings - 1) * segs * 6);

	for (int ring = 0; ring < rings - 1; ++ring)
	{
		for (int seg = 0; seg < segs; ++seg)
		{
			const int   next = (seg + 1) % segs;
			const Math::Vector3 p00 = GetVertex(ring,     seg);
			const Math::Vector3 p01 = GetVertex(ring,     next);
			const Math::Vector3 p10 = GetVertex(ring + 1, seg);
			const Math::Vector3 p11 = GetVertex(ring + 1, next);

			// 1面（quad=2tri）は6頂点全部同じ色でフラットに見せる
			const float faceAlphaVal = (RingAlpha(ring) + RingAlpha(ring + 1)) * 0.5f;
			const unsigned int fc = FaceColor(seg, ring, faceAlphaVal);

			KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
			v0.pos = p00; v0.color = fc;
			v1.pos = p01; v1.color = fc;
			v2.pos = p10; v2.color = fc;
			m_vtxBuf.push_back(v0); m_vtxBuf.push_back(v1); m_vtxBuf.push_back(v2);

			v3.pos = p01; v3.color = fc;
			v4.pos = p11; v4.color = fc;
			v5.pos = p10; v5.color = fc;
			m_vtxBuf.push_back(v3); m_vtxBuf.push_back(v4); m_vtxBuf.push_back(v5);
		}
	}

	if (!m_vtxBuf.empty())
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			m_vtxBuf, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ─── ワイヤー：面エッジに沿ったリング輪郭＋スポーク（どちらもGetVertex=ジッターあり）───
	m_vtxBuf.clear();
	m_vtxBuf.reserve(static_cast<size_t>(rings) * segs * 2
				+ static_cast<size_t>(segs) * (rings - 1) * 2);

	auto WireCol = [&](int ring) -> unsigned int
	{
		const float b = RingBright(ring);
		// invertGradient 時は口元ワイヤーを完全に消す（口元 wireAlpha=0）
		const float wa = invertGradient
			? wireAlpha * RingBright(ring)
			: wireAlpha;
		return ColorToUint({
			std::min(col.R() * b + 0.5f * b, 1.0f),
			std::min(col.G() * b + 0.25f * b, 1.0f),
			std::min(col.B() * b + 0.1f * b, 1.0f),
			wa });
	};

	// リング輪郭（面のseg頂点＝ジッターあり・面エッジそのまま）
	for (int ring = 0; ring < rings; ++ring)
	{
		const unsigned int wc = WireCol(ring);
		for (int seg = 0; seg < segs; ++seg)
		{
			const int next = (seg + 1) % segs;
			KdPolygon::Vertex v0{}, v1{};
			v0.pos = GetVertex(ring, seg);  v0.color = wc;
			v1.pos = GetVertex(ring, next); v1.color = wc;
			m_vtxBuf.push_back(v0); m_vtxBuf.push_back(v1);
		}
	}

	// スポーク（面のseg位置の縦エッジ・ジッターあり頂点をそのまま使う）
	for (int seg = 0; seg < segs; ++seg)
	{
		for (int ring = 0; ring < rings - 1; ++ring)
		{
			KdPolygon::Vertex v0{}, v1{};
			v0.pos = GetVertex(ring,     seg); v0.color = WireCol(ring);
			v1.pos = GetVertex(ring + 1, seg); v1.color = WireCol(ring + 1);
			m_vtxBuf.push_back(v0); m_vtxBuf.push_back(v1);
		}
	}

	if (!m_vtxBuf.empty())
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			m_vtxBuf, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable);
	}
}

//----------------------------------------------------------
// ファネル奥端リングとトンネル端点リングを三角形でブリッジ接続する
// 両リングは jitter/wave = 0 の純粋な円なので頂点座標を再計算して繋ぐ
//----------------------------------------------------------
void WarpHole::DrawFunnelTunnelBridge(
	const Math::Vector3& funnelCenter,
	const Math::Vector3& funnelMouthDir,
	const Math::Vector3& tunnelEndPos,
	const Math::Vector3& tunnelTangent,
	const Math::Vector3& tunnelBitangent,
	const Math::Color&   col) const
{
	const int   segs    = WarpHoleConst::FunnelSegments;
	const float innerR  = WarpHoleConst::FunnelInnerRadius;
	const float fl      = WarpHoleConst::FunnelLength;
	const float alpha   = WarpHoleConst::FunnelFaceAlpha * 0.6f;

	// ファネル奥端の基底
	Math::Vector3 fTang, fBitan;
	MakeBasis(funnelMouthDir, fTang, fBitan);
	const Math::Vector3 funnelInnerCenter = funnelCenter + funnelMouthDir * fl;

	// ファネル奥端リング頂点（jitter/wave=0）
	auto FunnelVert = [&](int seg) -> Math::Vector3
	{
		const float angle = kTwoPi * static_cast<float>(seg) / static_cast<float>(segs);
		return funnelInnerCenter
			+ fTang   * (innerR * std::cosf(angle))
			+ fBitan  * (innerR * std::sinf(angle));
	};

	// トンネル端点リング頂点（jitter/wave=0）
	auto TunnelVert = [&](int seg) -> Math::Vector3
	{
		const float angle = kTwoPi * static_cast<float>(seg) / static_cast<float>(segs);
		return tunnelEndPos
			+ tunnelTangent   * (innerR * std::cosf(angle))
			+ tunnelBitangent * (innerR * std::sinf(angle));
	};

	const unsigned int fc = ColorToUint({ col.R() * 0.8f, col.G() * 0.8f, col.B() * 0.8f, alpha });

	std::vector<KdPolygon::Vertex> tris;
	tris.reserve(static_cast<size_t>(segs) * 6);
	for (int seg = 0; seg < segs; ++seg)
	{
		const int next = (seg + 1) % segs;
		const Math::Vector3 f0 = FunnelVert(seg);
		const Math::Vector3 f1 = FunnelVert(next);
		const Math::Vector3 t0 = TunnelVert(seg);
		const Math::Vector3 t1 = TunnelVert(next);

		KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
		v0.pos = f0; v0.color = fc;
		v1.pos = f1; v1.color = fc;
		v2.pos = t0; v2.color = fc;
		tris.push_back(v0); tris.push_back(v1); tris.push_back(v2);
		v3.pos = f1; v3.color = fc;
		v4.pos = t1; v4.color = fc;
		v5.pos = t0; v5.color = fc;
		tris.push_back(v3); tris.push_back(v4); tris.push_back(v5);
	}

	if (!tris.empty())
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			tris, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}

//----------------------------------------------------------
// パス上を面＋ワイヤーで繋ぐチューブ描画
//   DrawFunnelFace と同じ面スタイル（フラットシェーディング・波アニメ）で
//   トンネル全体をぐにょぐにょ動く面チューブとして描画する
//----------------------------------------------------------
void WarpHole::DrawTunnelFace(const std::vector<Math::Vector3>& path,
							  const Math::Color& entryCol,
							  const Math::Color& exitCol,
							  const std::vector<float>& jitterOffsets,
							  float animTime) const
{
	const int pathCount = static_cast<int>(path.size());
	if (pathCount < 2) { return; }

	const int   segs      = WarpHoleConst::FunnelSegments;
	const float tubeR     = WarpHoleConst::FunnelInnerRadius; // ファネル奥端と同一半径
	const float faceAlpha = WarpHoleConst::FunnelFaceAlpha;
	const float wireAlpha = WarpHoleConst::FunnelWireAlpha;
	const float waveAmp   = WarpHoleConst::TunnelWaveAmp;
	const float waveFreqA = WarpHoleConst::TunnelWaveFreqAlong;
	const float waveFreqS = WarpHoleConst::TunnelWaveFreqSeg;

	// パスの総弧長
	std::vector<float> arc(pathCount, 0.0f);
	for (int i = 1; i < pathCount; ++i)
	{
		arc[i] = arc[i - 1] + (path[i] - path[i - 1]).Length();
	}
	const float totalLen = arc[pathCount - 1];
	if (totalLen < 1e-4f) { return; }

	// 各パス点の接線・法線基底
	std::vector<Math::Vector3> tang(pathCount), bitan(pathCount);
	for (int i = 0; i < pathCount; ++i)
	{
		const int   prev = std::max(i - 1, 0);
		const int   next = std::min(i + 1, pathCount - 1);
		Math::Vector3 fwd = path[next] - path[prev];
		if (fwd.LengthSquared() < 1e-8f) { fwd = Math::Vector3::Up; }
		fwd.Normalize();
		MakeBasis(fwd, tang[i], bitan[i]);
	}

	// パス位置 i、円周方向 seg の頂点
	// 両端（i==0, i==pathCount-1）は jitter/wave=0 の純粋な円にして
	// ファネル奥端リングと頂点が完全一致するようにする
	auto GetVert = [&](int i, int seg) -> Math::Vector3
	{
		const float angle = kTwoPi * static_cast<float>(seg) / static_cast<float>(segs);

		// 両端リングはきれいな円（ギャップ防止）
		const bool isEndRing = (i == 0 || i == pathCount - 1);

		float jitter = 0.0f;
		float wave   = 0.0f;
		if (!isEndRing)
		{
			const int   idx = i * segs + seg;
			jitter = (idx < static_cast<int>(jitterOffsets.size()))
					 ? jitterOffsets[idx] : 0.0f;

			const float wavePhase = kTwoPi * (waveFreqA * arc[i] - animTime * 5.0f)
								  + kTwoPi * waveFreqS * static_cast<float>(seg) / static_cast<float>(segs);
			wave = std::sinf(wavePhase) * waveAmp;
		}

		const float r = tubeR * (1.0f + jitter + wave);
		return path[i]
			+ tang[i]  * (r * std::cosf(angle))
			+ bitan[i] * (r * std::sinf(angle));
	};

	// Entry→Exit で色補間
	auto LerpColor = [&](float u) -> Math::Color
	{
		return {
			entryCol.R() * (1.0f - u) + exitCol.R() * u,
			entryCol.G() * (1.0f - u) + exitCol.G() * u,
			entryCol.B() * (1.0f - u) + exitCol.B() * u,
			faceAlpha };
	};

	// ファネルと同じフラット面色（seg/ring で位相をずらす）
	auto FaceCol = [&](int seg, int i, float alpha) -> unsigned int
	{
		const float u      = arc[i] / totalLen;
		const Math::Color base = LerpColor(u);
		const float bright = 1.0f - u * 0.4f;
		const float phase  = kTwoPi * static_cast<float>(seg) / static_cast<float>(segs)
						   + static_cast<float>(i) * 0.5f;
		const float rVal = (std::sinf(phase) * 0.5f + 0.5f) * 0.6f;
		const float gVal = base.G() * (0.7f + std::cosf(phase * 0.7f) * 0.3f);
		const float bVal = base.B();
		const Math::Color c = {
			std::min(rVal * bright, 1.0f),
			std::min(gVal * bright, 1.0f),
			std::min(bVal * bright, 1.0f),
			alpha };
		return ColorToUint(c);
	};

	// ─── 三角面 ───
	m_vtxBuf.clear();
	m_vtxBuf.reserve(static_cast<size_t>(pathCount - 1) * segs * 6);

	for (int i = 0; i < pathCount - 1; ++i)
	{
		const float u   = (arc[i] + arc[i + 1]) * 0.5f / totalLen;
		const float alp = faceAlpha * (1.0f - u * 0.4f);

		for (int seg = 0; seg < segs; ++seg)
		{
			const int next = (seg + 1) % segs;
			const Math::Vector3 p00 = GetVert(i,     seg);
			const Math::Vector3 p01 = GetVert(i,     next);
			const Math::Vector3 p10 = GetVert(i + 1, seg);
			const Math::Vector3 p11 = GetVert(i + 1, next);

			const unsigned int fc = FaceCol(seg, i, alp);
			KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
			v0.pos = p00; v0.color = fc;
			v1.pos = p01; v1.color = fc;
			v2.pos = p10; v2.color = fc;
			m_vtxBuf.push_back(v0); m_vtxBuf.push_back(v1); m_vtxBuf.push_back(v2);
			v3.pos = p01; v3.color = fc;
			v4.pos = p11; v4.color = fc;
			v5.pos = p10; v5.color = fc;
			m_vtxBuf.push_back(v3); m_vtxBuf.push_back(v4); m_vtxBuf.push_back(v5);
		}
	}

	if (!m_vtxBuf.empty())
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			m_vtxBuf, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ─── ワイヤー（面エッジ）───
	m_vtxBuf.clear();
	m_vtxBuf.reserve(static_cast<size_t>(pathCount) * segs * 2
				+ static_cast<size_t>(segs) * (pathCount - 1) * 2);

	auto WireC = [&](int i) -> unsigned int
	{
		const float u = arc[i] / totalLen;
		const Math::Color base = LerpColor(u);
		const float b = 1.0f - u * 0.4f;
		return ColorToUint({
			std::min(base.R() * b + 0.4f * b, 1.0f),
			std::min(base.G() * b + 0.2f * b, 1.0f),
			std::min(base.B() * b + 0.1f * b, 1.0f),
			wireAlpha });
	};

	for (int i = 0; i < pathCount; ++i)
	{
		const unsigned int wc = WireC(i);
		for (int seg = 0; seg < segs; ++seg)
		{
			const int next = (seg + 1) % segs;
			KdPolygon::Vertex v0{}, v1{};
			v0.pos = GetVert(i, seg);  v0.color = wc;
			v1.pos = GetVert(i, next); v1.color = wc;
			m_vtxBuf.push_back(v0); m_vtxBuf.push_back(v1);
		}
	}
	for (int seg = 0; seg < segs; ++seg)
	{
		for (int i = 0; i < pathCount - 1; ++i)
		{
			KdPolygon::Vertex v0{}, v1{};
			v0.pos = GetVert(i,     seg); v0.color = WireC(i);
			v1.pos = GetVert(i + 1, seg); v1.color = WireC(i + 1);
			m_vtxBuf.push_back(v0); m_vtxBuf.push_back(v1);
		}
	}

	if (!m_vtxBuf.empty())
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			m_vtxBuf, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable);
	}
}

//----------------------------------------------------------
bool WarpHole::CheckWarpTrigger(const Math::Vector3& pPos) const
{
	if (!m_data.Enabled || m_dead || m_consuming) { return false; }

	const float distSq   = (pPos - m_data.EntryPos).LengthSquared();
	const float radiusSq = WarpHoleConst::SuckRadius * WarpHoleConst::SuckRadius;
	return distSq <= radiusSq;
}

//----------------------------------------------------------
// ワイヤーフレーム格子トンネルを LINE_LIST で描画
// 構造：
//   ・同心円リングが奥に向かって小さくなる（RingLayers 枚）
//   ・SpokeCount 本の縦格子線が外周から中心へ伸びる
//   ・m_animOffset によって奥方向にスクロール
//----------------------------------------------------------
void WarpHole::DrawTunnelAlongPath(const std::vector<Math::Vector3>& path,
								   const Math::Color& entryCol,
								   const Math::Color& exitCol,
								   float animOffset,
								   bool entryMouthOnly,
								   float animTime) const
{
	const int pointCount = static_cast<int>(path.size());
	if (pointCount < 2) { return; }

	// ── 各点の累積距離（弧長）を求める ──
	std::vector<float> arc(pointCount, 0.0f);
	for (int i = 1; i < pointCount; ++i)
	{
		arc[i] = arc[i - 1] + (path[i] - path[i - 1]).Length();
	}
	const float totalLen = arc[pointCount - 1];
	if (totalLen < 1e-4f) { return; }

	// 端からの距離 → トランペット半径。
	//   口元（端）が太く、奥（FunnelDepth より内側）で細い管に絞られる。
	//   両端を口元にすることで、入口→中継点→出口まで一本に繋がる。
	auto RadiusAt = [&](float distFromNearEnd) -> float
	{
		const float depthT  = std::min(distFromNearEnd / WarpHoleConst::FunnelDepth, 1.0f);
		const float fIdx    = depthT * (kRadiusTableSize - 1);
		const int   idx0    = static_cast<int>(fIdx);
		const int   idx1    = std::min(idx0 + 1, kRadiusTableSize - 1);
		const float frac    = fIdx - static_cast<float>(idx0);
		const float scale   = kRadiusTable[idx0] * (1.0f - frac)
							 + kRadiusTable[idx1] * frac;
		return WarpHoleConst::RingOuterRadius * scale;
	};

	// 各点のフレーム（接線に垂直な基底）と半径・色をまとめて算出
	std::vector<Math::Vector3> tan(pointCount), bitan(pointCount);
	std::vector<float>         radius(pointCount);
	std::vector<float>         along(pointCount);   // 0..1（入口→出口）

	// 口元の固定向き（IMGUI設定／未設定なら入口⇔出口の直線方向）
	const Math::Vector3 entryMouthDir = m_data.GetEntryMouthDir();
	const Math::Vector3 exitMouthDir  = m_data.GetExitMouthDir();

	for (int i = 0; i < pointCount; ++i)
	{
		// 接線：前後の点から中央差分（端は片側差分）
		const int   prev = std::max(i - 1, 0);
		const int   next = std::min(i + 1, pointCount - 1);
		Math::Vector3 fwd = path[next] - path[prev];
		if (fwd.LengthSquared() < 1e-8f) { fwd = Math::Vector3::Up; }
		fwd.Normalize();

		// ── 口元の向きを固定する ──
		//   端からの弧長が MouthAlignDist 以内なら、中継点（子）の向きではなく
		//   口元の固定向きに寄せる。距離が離れるほど本来の軌道接線へブレンドし、
		//   カクつかずに馴染ませる。
		const float distFromEntry = arc[i];
		const float distFromExit  = totalLen - arc[i];
		if (WarpHoleConst::MouthAlignDist > 1e-4f)
		{
			if (distFromEntry < distFromExit)
			{
				// 入口側：0=口元 → 1=軌道
				const float blend = std::clamp(distFromEntry / WarpHoleConst::MouthAlignDist, 0.0f, 1.0f);
				fwd = Math::Vector3::Lerp(entryMouthDir, fwd, blend);
			}
			else
			{
				// 出口側：口元向きは「外向き」なので接線（入口→出口）に合わせて反転
				const float blend = std::clamp(distFromExit / WarpHoleConst::MouthAlignDist, 0.0f, 1.0f);
				fwd = Math::Vector3::Lerp(-exitMouthDir, fwd, blend);
			}
			if (fwd.LengthSquared() > 1e-8f) { fwd.Normalize(); }
			else { fwd = (i == 0) ? entryMouthDir : -exitMouthDir; }
		}

		MakeBasis(fwd, tan[i], bitan[i]);

		// 両端から近い方の距離でトランペット半径を決める
		// entryMouthOnly の場合は Entry 端だけ口元にする（テレポート型用）
		if (entryMouthOnly)
		{
			radius[i] = RadiusAt(distFromEntry);
		}
		else
		{
			radius[i] = RadiusAt(std::min(distFromEntry, distFromExit));
		}

		// トンネル wave：弧長方向と時間で半径をうねらせる
		{
			const float wavePhase = kTwoPi * (WarpHoleConst::TunnelWaveFreqAlong * arc[i] - animTime * WarpHoleConst::AnimSpeed * 5.0f);
			const float wave = std::sinf(wavePhase) * WarpHoleConst::TunnelWaveAmp;
			radius[i] *= (1.0f + wave) * m_consumeScale;   // 収縮消滅で縮む
		}

		along[i]  = arc[i] / totalLen;
	}

	// 入口色→出口色で補間した骨格色（奥ほど薄め）
	auto SkeletonColor = [&](int i) -> Math::Color
	{
		const float u = along[i];
		Math::Color c = {
			entryCol.R() * (1.0f - u) + exitCol.R() * u,
			entryCol.G() * (1.0f - u) + exitCol.G() * u,
			entryCol.B() * (1.0f - u) + exitCol.B() * u,
			(entryCol.A() * (1.0f - u) + exitCol.A() * u) * 0.85f
		};
		return c;
	};

	// ─────────────────────────────────────────────
	// パス1：静的なトンネル骨格（同心円リング＋縦格子）
	// ─────────────────────────────────────────────
	std::vector<KdPolygon::Vertex> lines;
	lines.reserve(
		pointCount * WarpHoleConst::RingSegments * 2
		+ WarpHoleConst::SpokeCount * (pointCount - 1) * 2
	);

	// ── 同心円リング ──
	for (int i = 0; i < pointCount; ++i)
	{
		const unsigned int colRing = ColorToUint(SkeletonColor(i));
		for (int seg = 0; seg < WarpHoleConst::RingSegments; ++seg)
		{
			const float a0 = kTwoPi * static_cast<float>(seg)     / WarpHoleConst::RingSegments;
			const float a1 = kTwoPi * static_cast<float>(seg + 1) / WarpHoleConst::RingSegments;

			KdPolygon::Vertex v0{}, v1{};
			v0.pos   = path[i] + tan[i] * (std::cos(a0) * radius[i])
							   + bitan[i] * (std::sin(a0) * radius[i]);
			v0.color = colRing;
			v1.pos   = path[i] + tan[i] * (std::cos(a1) * radius[i])
							   + bitan[i] * (std::sin(a1) * radius[i]);
			v1.color = colRing;
			lines.push_back(v0);
			lines.push_back(v1);
		}
	}

	// ── スポーク（縦格子線）：隣り合う点を結ぶ ──
	for (int spoke = 0; spoke < WarpHoleConst::SpokeCount; ++spoke)
	{
		const float angle = kTwoPi * static_cast<float>(spoke) / WarpHoleConst::SpokeCount;
		const float cx    = std::cos(angle);
		const float cy    = std::sin(angle);

		for (int i = 0; i < pointCount - 1; ++i)
		{
			KdPolygon::Vertex v0{}, v1{};
			v0.pos   = path[i] + tan[i] * (cx * radius[i])
							   + bitan[i] * (cy * radius[i]);
			v0.color = ColorToUint(SkeletonColor(i));
			v1.pos   = path[i + 1] + tan[i + 1] * (cx * radius[i + 1])
								   + bitan[i + 1] * (cy * radius[i + 1]);
			v1.color = ColorToUint(SkeletonColor(i + 1));
			lines.push_back(v0);
			lines.push_back(v1);
		}
	}

	if (lines.size() >= 2)
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(lines, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable);
	}

	// ─────────────────────────────────────────────
	// パス2：輝くリングが入口→出口へ流れる
	//   全リングを1つの頂点配列にまとめて1回のドローコールで描く（高速化）
	// ─────────────────────────────────────────────
	static constexpr int kGlowCount = 24;
	std::vector<KdPolygon::Vertex> glowLines;
	glowLines.reserve(kGlowCount * WarpHoleConst::RingSegments * 2);
	for (int g = 0; g < kGlowCount; ++g)
	{
		const float rawU = static_cast<float>(g) / kGlowCount + animOffset;
		const float u    = rawU - std::floor(rawU);   // 0=入口, 1=出口

		// u（0..1）に対応する点を弧長で見つけ、隣接点間を線形補間
		const float target = u * totalLen;
		int seg = 0;
		while (seg + 1 < pointCount && arc[seg + 1] < target) { ++seg; }
		const int   segNext = std::min(seg + 1, pointCount - 1);
		const float segLen  = arc[segNext] - arc[seg];
		const float lerpT   = (segLen > 1e-6f) ? (target - arc[seg]) / segLen : 0.0f;

		const Math::Vector3 glowCenter = Math::Vector3::Lerp(path[seg], path[segNext], lerpT);
		Math::Vector3 glowTan   = Math::Vector3::Lerp(tan[seg],   tan[segNext],   lerpT);
		Math::Vector3 glowBitan = Math::Vector3::Lerp(bitan[seg], bitan[segNext], lerpT);
		if (glowTan.LengthSquared()   > 1e-6f) { glowTan.Normalize(); }
		if (glowBitan.LengthSquared() > 1e-6f) { glowBitan.Normalize(); }
		const float glowR = radius[seg] * (1.0f - lerpT) + radius[segNext] * lerpT;

		Math::Color cc = {
			entryCol.R() * (1.0f - u) + exitCol.R() * u,
			entryCol.G() * (1.0f - u) + exitCol.G() * u,
			entryCol.B() * (1.0f - u) + exitCol.B() * u,
			entryCol.A() * (1.0f - u) + exitCol.A() * u
		};
		const unsigned int glowCol = ColorToUint(cc);

		for (int s = 0; s < WarpHoleConst::RingSegments; ++s)
		{
			const float a0 = kTwoPi * static_cast<float>(s)     / WarpHoleConst::RingSegments;
			const float a1 = kTwoPi * static_cast<float>(s + 1) / WarpHoleConst::RingSegments;

			KdPolygon::Vertex v0{}, v1{};
			v0.pos   = glowCenter + glowTan * (std::cos(a0) * glowR) + glowBitan * (std::sin(a0) * glowR);
			v0.color = glowCol;
			v1.pos   = glowCenter + glowTan * (std::cos(a1) * glowR) + glowBitan * (std::sin(a1) * glowR);
			v1.color = glowCol;
			glowLines.push_back(v0);
			glowLines.push_back(v1);
		}
	}
	if (glowLines.size() >= 2)
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(glowLines, Math::Matrix::Identity,
			Math::Color(1, 1, 1, 1), KdDepthStencilState::ZWriteDisable);
	}
}

//----------------------------------------------------------
void WarpHole::MakeBasis(const Math::Vector3& normal,
						  Math::Vector3& outTangent,
						  Math::Vector3& outBitangent)
{
	Math::Vector3 up = { 0.0f, 1.0f, 0.0f };
	if (std::abs(normal.Dot(up)) > 0.99f) { up = { 1.0f, 0.0f, 0.0f }; }
	outTangent = up.Cross(normal);
	outTangent.Normalize();
	outBitangent = normal.Cross(outTangent);
	outBitangent.Normalize();
}

//----------------------------------------------------------
// トンネルのビジュアル中心線。
//   中心線は「実際のワープ経路（入口→中継点→出口）」そのものを使う。
//   こうするとトンネルの見た目とプレイヤーの飛行軌道が完全に一致する。
//   トランペット形状（口元が太い）は DrawTunnelAlongPath 側が
//   「端からの距離」で半径を決めて表現するので、中心線を曲げる必要はない。
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::BuildTunnelCenterPath() const
{
	// キャッシュが有効ならそのまま返す（入口/出口/中継点が変わらない限り再計算しない）
	if (!m_centerPathDirty && !m_centerPathCache.empty())
	{
		return m_centerPathCache;
	}

	// 全 waypoint（入口→中継点→出口）をそのまま制御点にする
	const std::vector<Math::Vector3> path = m_data.GetFullPath();
	if (path.size() < 2)
	{
		m_centerPathCache = path;
		m_centerPathDirty = false;
		return path;
	}

	// 中継点間を Catmull-Rom スプラインで滑らかに曲げる。
	// 各 waypoint を必ず通過しつつ、点と点の間を曲線で繋ぐ。
	const std::vector<Math::Vector3> smooth =
		BuildSpline(path, WarpHoleConst::CenterSplineSubdiv);

	// 点密度を均一化して繋ぎ目のガクつきを防ぐ
	m_centerPathCache = ResamplePath(smooth, WarpHoleConst::CenterPathSpacing);
	m_centerPathDirty = false;
	return m_centerPathCache;
}

//----------------------------------------------------------
// テレポート型用：Entry 口元 → 奥 の密なパス
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::BuildTeleportEntryPath() const
{
	const Math::Vector3 dir = m_data.GetEntryMouthDir();
	const std::vector<Math::Vector3> raw =
	{
		m_data.EntryPos,
		m_data.EntryPos + dir * WarpHoleConst::FunnelDepth
	};
	return ResamplePath(raw, WarpHoleConst::CenterPathSpacing);
}

//----------------------------------------------------------
// テレポート型用：Exit 奥 → 口元 の密なパス（吐き出し方向）
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::BuildTeleportExitPath() const
{
	const Math::Vector3 dir = m_data.GetExitMouthDir();
	// 奥から口元へ向かう順（吐き出し）
	const std::vector<Math::Vector3> raw =
	{
		m_data.ExitPos + dir * WarpHoleConst::FunnelDepth,
		m_data.ExitPos
	};
	return ResamplePath(raw, WarpHoleConst::CenterPathSpacing);
}

//----------------------------------------------------------
// 逆走用：Exit 口元 → 奥（逆走時の吸い込みパス）
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::BuildTeleportExitPathReverse() const
{
	auto path = BuildTeleportExitPath();
	std::reverse(path.begin(), path.end());
	return path;
}

//----------------------------------------------------------
// 逆走用：Entry 奥 → 口元（逆走時の吐き出しパス）
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::BuildTeleportEntryPathReverse() const
{
	auto path = BuildTeleportEntryPath();
	std::reverse(path.begin(), path.end());
	return path;
}

//----------------------------------------------------------
// 制御点列を Catmull-Rom スプラインで滑らかな曲線に変換する。
//   各制御点を必ず通過しつつ、点と点の間を曲線で補間する。
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::BuildSpline(
	const std::vector<Math::Vector3>& points, int subdivPerSegment)
{
	const int n = static_cast<int>(points.size());
	if (n < 3 || subdivPerSegment < 1) { return points; }

	std::vector<Math::Vector3> curve;
	curve.reserve(static_cast<size_t>(n - 1) * subdivPerSegment + 1);

	for (int i = 0; i + 1 < n; ++i)
	{
		// Catmull-Rom：区間 [p1→p2] を前後の点 p0,p3 で曲げる
		const Math::Vector3& p0 = points[std::max(i - 1, 0)];
		const Math::Vector3& p1 = points[i];
		const Math::Vector3& p2 = points[i + 1];
		const Math::Vector3& p3 = points[std::min(i + 2, n - 1)];

		// 最終区間だけ終端を含める（区間の継ぎ目で重複しないように）
		const int stepEnd = (i + 2 == n) ? subdivPerSegment : subdivPerSegment - 1;
		for (int s = 0; s <= stepEnd; ++s)
		{
			const float t  = static_cast<float>(s) / subdivPerSegment;
			const float t2 = t * t;
			const float t3 = t2 * t;

			// Catmull-Rom 基底（張力0.5）
			const Math::Vector3 pos =
				  p0 * (-0.5f * t3 + t2 - 0.5f * t)
				+ p1 * ( 1.5f * t3 - 2.5f * t2 + 1.0f)
				+ p2 * (-1.5f * t3 + 2.0f * t2 + 0.5f * t)
				+ p3 * ( 0.5f * t3 - 0.5f * t2);
			curve.push_back(pos);
		}
	}
	return curve;
}

//----------------------------------------------------------
// パスの両端から trimDist 分の点を弧長ベースで削除し、端点を補間して正確な位置に置く
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::TrimTunnelPath(
	const std::vector<Math::Vector3>& path, float trimDist)
{
	const int n = static_cast<int>(path.size());
	if (n < 2) { return path; }

	// 累積弧長
	std::vector<float> arc(n, 0.0f);
	for (int i = 1; i < n; ++i)
	{
		arc[i] = arc[i - 1] + (path[i] - path[i - 1]).Length();
	}
	const float total = arc[n - 1];
	const float tStart = trimDist;
	const float tEnd   = total - trimDist;

	if (tEnd <= tStart) { return {}; } // パスが短すぎる

	// tStart / tEnd に対応する補間点を求めてから、その間の点だけ抽出
	auto Interpolate = [&](float t) -> Math::Vector3
	{
		for (int i = 0; i + 1 < n; ++i)
		{
			if (arc[i + 1] >= t)
			{
				const float segLen = arc[i + 1] - arc[i];
				const float frac   = (segLen > 1e-6f) ? (t - arc[i]) / segLen : 0.0f;
				return Math::Vector3::Lerp(path[i], path[i + 1], frac);
			}
		}
		return path[n - 1];
	};

	std::vector<Math::Vector3> result;
	result.push_back(Interpolate(tStart));
	for (int i = 0; i < n; ++i)
	{
		if (arc[i] > tStart && arc[i] < tEnd)
		{
			result.push_back(path[i]);
		}
	}
	result.push_back(Interpolate(tEnd));
	return result;
}

//----------------------------------------------------------
// ポリラインを一定間隔で等間隔リサンプリングする
//----------------------------------------------------------
std::vector<Math::Vector3> WarpHole::ResamplePath(
	const std::vector<Math::Vector3>& src, float spacing)
{
	std::vector<Math::Vector3> out;
	const int n = static_cast<int>(src.size());
	if (n < 2 || spacing <= 1e-4f) { return src; }

	out.push_back(src[0]);
	float carry = 0.0f;   // 直前区間で消費しきれず持ち越した距離

	for (int i = 0; i + 1 < n; ++i)
	{
		Math::Vector3 seg = src[i + 1] - src[i];
		float segLen = seg.Length();
		if (segLen < 1e-6f) { continue; }

		const Math::Vector3 dir = seg / segLen;

		// この区間内で spacing 間隔の点を打てるだけ打つ
		float distAlong = spacing - carry;
		while (distAlong <= segLen)
		{
			out.push_back(src[i] + dir * distAlong);
			distAlong += spacing;
		}
		// 余り（次区間へ持ち越す距離）
		carry = segLen - (distAlong - spacing);
	}

	// 終端を必ず含める
	out.push_back(src[n - 1]);
	return out;
}

