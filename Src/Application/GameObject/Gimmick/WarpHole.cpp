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

	const Math::Color entryCol = { WarpHoleConst::EntryColorR,
								   WarpHoleConst::EntryColorG,
								   WarpHoleConst::EntryColorB,
								   WarpHoleConst::EntryColorA };
	const Math::Color exitCol  = { WarpHoleConst::ExitColorR,
								   WarpHoleConst::ExitColorG,
								   WarpHoleConst::ExitColorB,
								   WarpHoleConst::ExitColorA };

	if (m_data.Teleport)
	{
		// テレポート型：入口と出口を繋がず、それぞれ独立した口元リングのみ描画
		// 2点の疎なパスを ResamplePath で密にしてからトンネル描画する
		// entryMouthOnly=true で path[0] 側だけ口元、奥は細管になる
		const Math::Vector3 entryDir = m_data.GetEntryMouthDir();
		const std::vector<Math::Vector3> entryRaw =
		{
			m_data.EntryPos,
			m_data.EntryPos + entryDir * WarpHoleConst::FunnelDepth * 2.0f
		};
		const auto entryPath = ResamplePath(entryRaw, WarpHoleConst::CenterPathSpacing);
		DrawTunnelAlongPath(entryPath, entryCol, entryCol, m_animOffset, true);

		const Math::Vector3 exitDir = m_data.GetExitMouthDir();
		if (!m_data.OneWay)
		{
			// 双方向：Exit も吸い込む方向（animOffset 反転）
			const std::vector<Math::Vector3> exitRaw =
			{
				m_data.ExitPos,
				m_data.ExitPos + exitDir * WarpHoleConst::FunnelDepth * 2.0f
			};
			const auto exitPath = ResamplePath(exitRaw, WarpHoleConst::CenterPathSpacing);
			DrawTunnelAlongPath(exitPath, exitCol, exitCol, 1.0f - m_animOffset, true);
		}
		else
		{
			// 一方通行：Exit は「吐き出す穴」→ アニメを逆向きにする
			const std::vector<Math::Vector3> exitRaw =
			{
				m_data.ExitPos,
				m_data.ExitPos + exitDir * WarpHoleConst::FunnelDepth * 2.0f
			};
			const auto exitPath = ResamplePath(exitRaw, WarpHoleConst::CenterPathSpacing);
			DrawTunnelAlongPath(exitPath, exitCol, exitCol, 1.0f - m_animOffset, true);
		}
	}
	else
	{
		// 通常型：OneWay/双方向どちらも Waypoint を通る一本のトンネルで描画
		// グロウリングの流れ方向（Entry→Exit）で一方通行を表現する
		BuildTunnelCenterPath();
		DrawTunnelAlongPath(m_centerPathCache, entryCol, exitCol, m_animOffset,
							/*entryMouthOnly=*/false);
	}

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
void WarpHole::DrawTunnelAlongPath(const std::vector<Math::Vector3>& path,
								   const Math::Color& entryCol,
								   const Math::Color& exitCol,
								   float animOffset,
								   bool entryMouthOnly) const
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
			(entryCol.A() * (1.0f - u) + exitCol.A() * u) * 0.35f
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
		KdShaderManager::Instance().m_StandardShader.DrawVertices(lines, Math::Matrix::Identity);
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
		KdShaderManager::Instance().m_StandardShader.DrawVertices(glowLines, Math::Matrix::Identity);
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
