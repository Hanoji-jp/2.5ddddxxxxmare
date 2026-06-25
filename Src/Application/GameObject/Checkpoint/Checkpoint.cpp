#include "../../../Pch.h"
#include "Checkpoint.h"

namespace
{
	// 0xAABBGGRR の頂点カラー（RockDrop と同じ並び）
	unsigned int CpColToU(float r, float g, float b, float a = 1.0f)
	{
		auto u8 = [](float c) { return static_cast<unsigned int>(std::min(std::max(c, 0.0f), 1.0f) * 255.0f); };
		return (u8(a) << 24) | (u8(b) << 16) | (u8(g) << 8) | u8(r);
	}

	void AddTri(std::vector<KdPolygon::Vertex>& v,
		const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c, unsigned int col)
	{
		KdPolygon::Vertex t{}; t.color = col;
		t.pos = a; v.push_back(t);
		t.pos = b; v.push_back(t);
		t.pos = c; v.push_back(t);
	}

	void AddQuad(std::vector<KdPolygon::Vertex>& v,
		const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c, const Math::Vector3& d, unsigned int col)
	{
		AddTri(v, a, b, c, col);
		AddTri(v, a, c, d, col);
	}

	// 軸並行ボックス（6面）を追加。上面だけ少し明るくして立体感を出す。
	void AddBox(std::vector<KdPolygon::Vertex>& v,
		const Math::Vector3& mn, const Math::Vector3& mx, float r, float g, float b)
	{
		const unsigned int side = CpColToU(r, g, b);
		const unsigned int top  = CpColToU(r * 1.2f, g * 1.2f, b * 1.2f);
		const Math::Vector3 p000{ mn.x, mn.y, mn.z }, p100{ mx.x, mn.y, mn.z };
		const Math::Vector3 p110{ mx.x, mx.y, mn.z }, p010{ mn.x, mx.y, mn.z };
		const Math::Vector3 p001{ mn.x, mn.y, mx.z }, p101{ mx.x, mn.y, mx.z };
		const Math::Vector3 p111{ mx.x, mx.y, mx.z }, p011{ mn.x, mx.y, mx.z };
		AddQuad(v, p001, p101, p111, p011, side);   // +Z
		AddQuad(v, p100, p000, p010, p110, side);   // -Z
		AddQuad(v, p101, p100, p110, p111, side);   // +X
		AddQuad(v, p000, p001, p011, p010, side);   // -X
		AddQuad(v, p010, p011, p111, p110, top);    // +Y(上面)
		AddQuad(v, p000, p100, p101, p001, side);   // -Y(底)
	}
}

void Checkpoint::Init()
{
	m_pDebugWire = std::make_unique<KdDebugWireFrame>();
	m_cullingRadius = CheckpointConst::PoleHeight + CheckpointConst::FlagLength;
}

void Checkpoint::Update()
{
	m_time += KdFPSController::GetDt();

	const auto spPlayer = m_wpPlayer.lock();
	if (!spPlayer) { return; }

	const Math::Vector3 toPlayer = spPlayer->GetPos() - GetPos();
	const float distSq = toPlayer.LengthSquared();
	const float r      = CheckpointConst::TriggerRadius;

	if (distSq <= r * r)
	{
		m_activated = true;
	}
}

void Checkpoint::DrawLit()
{
	using namespace CheckpointConst;
	constexpr float kTau = 6.2831853f;

	const float topY  = PoleHeight - FlagTopGap;
	const float botY  = topY - FlagHeight;
	const float baseX = PoleHalf;
	const float cx    = baseX + FlagLength * 0.5f;   // 布の中心（拡大の基準）
	const float cy    = (topY + botY) * 0.5f;

	// 通過＝金、未通過＝緑
	const float fr = m_activated ? FlagActiveR : FlagColorR;
	const float fg = m_activated ? FlagActiveG : FlagColorG;
	const float fb = m_activated ? FlagActiveB : FlagColorB;

	// グリッド頂点（長さ方向 tx ×縦方向 ty）。Zを2次元的にはためかせる。
	// flagScale>1 で中心から拡大（アウトライン用）。
	auto gridPt = [&](int i, int j, float flagScale) -> Math::Vector3
	{
		const float tx = static_cast<float>(i) / static_cast<float>(FlagSegments);
		const float ty = static_cast<float>(j) / static_cast<float>(FlagRows);
		float x = baseX + tx * FlagLength;
		float y = topY  - ty * FlagHeight;
		const float z = std::sinf(m_time * WaveSpeed + tx * WaveFreq * kTau) * WaveAmp * tx
					 + std::sinf(m_time * WaveSpeed * 0.8f + ty * WaveFreqV * kTau) * WaveAmpV * tx;
		// 中心から拡大（縁取り）
		x = cx + (x - cx) * flagScale;
		y = cy + (y - cy) * flagScale;
		return { x, y, z };
	};

	// 旗（ポール＋布）を1パスぶん構築する。
	//   outline=true … 全頂点を濃色・一回り大きく（縁取り）
	auto build = [&](std::vector<KdPolygon::Vertex>& out, bool outline)
	{
		out.clear();
		out.reserve(static_cast<size_t>(FlagSegments) * FlagRows * 12 + 36);

		// ポール
		const float pe = outline ? PoleOutlineExpand : 0.0f;
		if (outline)
		{
			AddBox(out, { -PoleHalf - pe, -pe, -PoleHalf - pe },
				{ PoleHalf + pe, PoleHeight + pe, PoleHalf + pe },
				OutlineColorR, OutlineColorG, OutlineColorB);
		}
		else
		{
			AddBox(out, { -PoleHalf, 0.0f, -PoleHalf },
				{ PoleHalf, PoleHeight, PoleHalf },
				PoleColorR, PoleColorG, PoleColorB);
		}

		// 布
		const float flagScale = outline ? FlagOutlineScale : 1.0f;
		for (int i = 0; i < FlagSegments; ++i)
		{
			unsigned int col;
			if (outline)
			{
				col = CpColToU(OutlineColorR, OutlineColorG, OutlineColorB);
			}
			else
			{
				const float txc = (static_cast<float>(i) + 0.5f) / static_cast<float>(FlagSegments);
				const float shade = 0.82f + 0.18f * (0.5f + 0.5f * std::sinf(m_time * WaveSpeed + txc * WaveFreq * kTau));
				col = CpColToU(fr * shade, fg * shade, fb * shade);
			}

			for (int j = 0; j < FlagRows; ++j)
			{
				const Math::Vector3 p00 = gridPt(i,     j,     flagScale);
				const Math::Vector3 p10 = gridPt(i + 1, j,     flagScale);
				const Math::Vector3 p11 = gridPt(i + 1, j + 1, flagScale);
				const Math::Vector3 p01 = gridPt(i,     j + 1, flagScale);
				AddQuad(out, p00, p10, p11, p01, col);   // 表
				AddQuad(out, p00, p01, p11, p10, col);   // 裏（両面表示）
			}
		}
	};

	const Math::Matrix world = Math::Matrix::CreateTranslation(GetPos());
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// ① アウトライン（濃色・一回り大きく・深度書き込みなしで先に描く＝縁が全周に残る）
	std::vector<KdPolygon::Vertex> ov;
	build(ov, true);
	shader.DrawVertices(ov, world, Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZWriteDisable, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ② 本体（通常色・深度書き込みあり。中心を上書きして縁だけ残す）
	std::vector<KdPolygon::Vertex> verts;
	build(verts, false);
	shader.DrawVertices(verts, world, Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZEnable, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Checkpoint::DrawDebug()
{
	if (!m_pDebugWire) { return; }

	m_pDebugWire->AddDebugSphere(
		GetPos(),
		CheckpointConst::TriggerRadius,
		{ CheckpointConst::DebugColorR,
		  CheckpointConst::DebugColorG,
		  CheckpointConst::DebugColorB,
		  CheckpointConst::DebugColorA });
	m_pDebugWire->Draw();
}
