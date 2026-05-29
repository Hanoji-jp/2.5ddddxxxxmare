#include "../../../Pch.h"
#include "WarpHole.h"

static constexpr float kTwoPi          = 6.28318530718f;
static constexpr float kFixedDeltaTime = 1.0f / 60.0f;

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
void WarpHole::Update()
{
	if (!m_data.Enabled) { return; }

	m_animOffset += WarpHoleConst::AnimSpeed * kFixedDeltaTime;
	if (m_animOffset > 1.0f) { m_animOffset -= 1.0f; }
}

//----------------------------------------------------------
void WarpHole::DrawEffect()
{
	if (!m_data.Enabled) { return; }

	auto& shaderMgr = KdShaderManager::Instance();
	shaderMgr.ChangeBlendState(KdBlendState::Add);

	Math::Vector3 axisEntry = m_data.ExitPos - m_data.EntryPos;
	if (axisEntry.LengthSquared() < 1e-6f) { axisEntry = Math::Vector3::Up; }
	axisEntry.Normalize();

	const Math::Color entryCol = { WarpHoleConst::EntryColorR,
								   WarpHoleConst::EntryColorG,
								   WarpHoleConst::EntryColorB,
								   WarpHoleConst::EntryColorA };
	const Math::Color exitCol  = { WarpHoleConst::ExitColorR,
								   WarpHoleConst::ExitColorG,
								   WarpHoleConst::ExitColorB,
								   WarpHoleConst::ExitColorA };

	// 入口：吸い込み方向（口元→奥）
	DrawTunnel(m_data.EntryPos,  axisEntry, entryCol,  m_animOffset);
	// 出口：吐き出し方向（奥→口元）なので符号反転
	DrawTunnel(m_data.ExitPos,  -axisEntry, exitCol,  -m_animOffset);

	shaderMgr.UndoBlendState();
}

//----------------------------------------------------------
void WarpHole::DrawDebug()
{
	if (!m_data.Enabled) { return; }

	{
		KdDebugWireFrame wire;
		wire.AddDebugSphere(m_data.EntryPos, WarpHoleConst::SuckRadius, { 0.0f, 1.0f, 1.0f, 1.0f });
		wire.Draw();
	}
	{
		KdDebugWireFrame wire;
		wire.AddDebugSphere(m_data.ExitPos, WarpHoleConst::SuckRadius, { 1.0f, 0.0f, 1.0f, 1.0f });
		wire.Draw();
	}
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
bool WarpHole::CheckWarpTrigger(const Math::Vector3& pPos) const
{
	if (!m_data.Enabled) { return false; }

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
void WarpHole::DrawTunnel(const Math::Vector3& origin,
					   const Math::Vector3& axis,
					   const Math::Color& col,
					   float animOffset) const
{
	Math::Vector3 tan, bitan;
	MakeBasis(axis, tan, bitan);

	const unsigned int colFull = ColorToUint(col);

	// 各リング位置を格納（LINE_LIST 形式: 2頂点1本）
	std::vector<KdPolygon::Vertex> lines;
	lines.reserve(
		WarpHoleConst::RingLayers * WarpHoleConst::RingSegments * 2  // 同心円
		+ WarpHoleConst::SpokeCount * (WarpHoleConst::RingLayers - 1) * 2  // 縦格子
	);

	// リングごとの中心位置と半径を求めておく
	std::vector<Math::Vector3> ringCenter(WarpHoleConst::RingLayers);
	std::vector<float>         ringRadius(WarpHoleConst::RingLayers);

	// ─────────────────────────────────────────────
	// パス1：静的なトランペット形状（常時表示・暗め）
	// ─────────────────────────────────────────────
	for (int layer = 0; layer < WarpHoleConst::RingLayers; ++layer)
	{
		const float shapeT = static_cast<float>(layer) / (WarpHoleConst::RingLayers - 1);

		ringCenter[layer] = origin + axis * (WarpHoleConst::FunnelDepth * shapeT);

		const float fIdx        = shapeT * (kRadiusTableSize - 1);
		const int   idx0        = static_cast<int>(fIdx);
		const int   idx1        = std::min(idx0 + 1, kRadiusTableSize - 1);
		const float frac        = fIdx - static_cast<float>(idx0);
		const float radiusScale = kRadiusTable[idx0] * (1.0f - frac)
								+ kRadiusTable[idx1] * frac;
		ringRadius[layer] = WarpHoleConst::RingOuterRadius * radiusScale;

		// 骨格（奥に向かってフェードアウト）
		const float alpha = (1.0f - shapeT * 0.85f) * col.A() * 0.35f;
		Math::Color ringCol = { col.R(), col.G(), col.B(), alpha };
		const unsigned int colRing = ColorToUint(ringCol);

		// ── 同心円リング（LINE_LIST） ──
		for (int seg = 0; seg < WarpHoleConst::RingSegments; ++seg)
		{
			const float a0 = kTwoPi * static_cast<float>(seg)     / WarpHoleConst::RingSegments;
			const float a1 = kTwoPi * static_cast<float>(seg + 1) / WarpHoleConst::RingSegments;

			KdPolygon::Vertex v0{}, v1{};
			v0.pos   = ringCenter[layer]
					   + tan   * (std::cos(a0) * ringRadius[layer])
					   + bitan * (std::sin(a0) * ringRadius[layer]);
			v0.color = colRing;

			v1.pos   = ringCenter[layer]
					   + tan   * (std::cos(a1) * ringRadius[layer])
					   + bitan * (std::sin(a1) * ringRadius[layer]);
			v1.color = colRing;

			lines.push_back(v0);
			lines.push_back(v1);
		}
	}

	// ── スポーク（縦格子線）：隣のリング間を結ぶ LINE_LIST ──
	for (int spoke = 0; spoke < WarpHoleConst::SpokeCount; ++spoke)
	{
		const float angle = kTwoPi * static_cast<float>(spoke) / WarpHoleConst::SpokeCount;
		const float cx    = std::cos(angle);
		const float cy    = std::sin(angle);

		for (int layer = 0; layer < WarpHoleConst::RingLayers - 1; ++layer)
		{
			const float a0 = (1.0f - static_cast<float>(layer)     / (WarpHoleConst::RingLayers - 1) * 0.85f) * col.A() * 0.35f;
			const float a1 = (1.0f - static_cast<float>(layer + 1) / (WarpHoleConst::RingLayers - 1) * 0.85f) * col.A() * 0.35f;
			KdPolygon::Vertex v0{}, v1{};
			v0.pos   = ringCenter[layer]
					   + tan   * (cx * ringRadius[layer])
					   + bitan * (cy * ringRadius[layer]);
			v0.color = ColorToUint({ col.R(), col.G(), col.B(), a0 });

			v1.pos   = ringCenter[layer + 1]
					   + tan   * (cx * ringRadius[layer + 1])
					   + bitan * (cy * ringRadius[layer + 1]);
			v1.color = ColorToUint({ col.R(), col.G(), col.B(), a1 });

			lines.push_back(v0);
			lines.push_back(v1);
		}
	}

	if (lines.size() >= 2)
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(lines, Math::Matrix::Identity);
	}

	// ─────────────────────────────────────────────
	// パス2：輝くリングが口元→奥に向かって流れる
	// ─────────────────────────────────────────────
	static constexpr int kGlowCount = 24;
	for (int g = 0; g < kGlowCount; ++g)
	{
		// 各輝きリングの奥行き位置 0..1 をアニメで動かす（口元→奥方向）
		const float rawT  = static_cast<float>(g) / kGlowCount + animOffset;
		const float t     = rawT - std::floor(rawT);  // 0=口元, 1=奥

		// 形状テーブルから半径を取得
		const float fIdx        = t * (kRadiusTableSize - 1);
		const int   idx0        = static_cast<int>(fIdx);
		const int   idx1        = std::min(idx0 + 1, kRadiusTableSize - 1);
		const float frac        = fIdx - static_cast<float>(idx0);
		const float radiusScale = kRadiusTable[idx0] * (1.0f - frac)
								+ kRadiusTable[idx1] * frac;
		const float glowR = WarpHoleConst::RingOuterRadius * radiusScale;

		const Math::Vector3 glowCenter = origin + axis * (WarpHoleConst::FunnelDepth * t);

		// 奥に向かってフェードアウト
		const float alpha = (1.0f - t * 0.85f) * col.A();
		const unsigned int glowCol = ColorToUint({ col.R(), col.G(), col.B(), alpha });

		std::vector<KdPolygon::Vertex> glowLines;
		glowLines.reserve(WarpHoleConst::RingSegments * 2);
		for (int seg = 0; seg < WarpHoleConst::RingSegments; ++seg)
		{
			const float a0 = kTwoPi * static_cast<float>(seg)     / WarpHoleConst::RingSegments;
			const float a1 = kTwoPi * static_cast<float>(seg + 1) / WarpHoleConst::RingSegments;

			KdPolygon::Vertex v0{}, v1{};
			v0.pos   = glowCenter + tan * (std::cos(a0) * glowR) + bitan * (std::sin(a0) * glowR);
			v0.color = glowCol;
			v1.pos   = glowCenter + tan * (std::cos(a1) * glowR) + bitan * (std::sin(a1) * glowR);
			v1.color = glowCol;
			glowLines.push_back(v0);
			glowLines.push_back(v1);
		}
		if (glowLines.size() >= 2)
		{
			KdShaderManager::Instance().m_StandardShader.DrawVertices(glowLines, Math::Matrix::Identity);
		}
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
