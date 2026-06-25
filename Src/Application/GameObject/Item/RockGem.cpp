#include "../../main.h"
#include "RockGem.h"

static constexpr float kGemTwoPi = 6.28318530718f;
static constexpr float kGemPi    = 3.14159265359f;

//----------------------------------------------------------
// 乱数・色ヘルパー
//----------------------------------------------------------
namespace
{
	unsigned int GemColorToUint(float r, float g, float b, float a)
	{
		auto u8 = [](float c) { return static_cast<unsigned char>(std::min(std::max(c, 0.0f), 1.0f) * 255.0f); };
		return (static_cast<unsigned int>(u8(a)) << 24)
			 | (static_cast<unsigned int>(u8(b)) << 16)
			 | (static_cast<unsigned int>(u8(g)) << 8)
			 |  static_cast<unsigned int>(u8(r));
	}

	float GemRand(unsigned int& seed)
	{
		seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
		return static_cast<float>(seed & 0xFFFF) / static_cast<float>(0xFFFF);
	}

	// HSV(h:0..1, s,v:0..1) → RGB
	Math::Color HsvToRgb(float h, float s, float v)
	{
		h = h - std::floor(h);
		const float i = std::floor(h * 6.0f);
		const float f = h * 6.0f - i;
		const float p = v * (1.0f - s);
		const float q = v * (1.0f - f * s);
		const float t = v * (1.0f - (1.0f - f) * s);
		switch (static_cast<int>(i) % 6)
		{
		case 0:  return Math::Color(v, t, p, 1.0f);
		case 1:  return Math::Color(q, v, p, 1.0f);
		case 2:  return Math::Color(p, v, t, 1.0f);
		case 3:  return Math::Color(p, q, v, 1.0f);
		case 4:  return Math::Color(t, p, v, 1.0f);
		default: return Math::Color(v, p, q, 1.0f);
		}
	}
}

//==========================================================
// 共有メッシュ（半径1ユニットのローポリ岩石・グレースケール）
//   インスタンスごとに色を変えるため、頂点色は「明るさのみ（白基調）」でベイクし、
//   描画時に DrawVertices の color でレインボー色を乗算する。
//==========================================================
namespace
{
	struct GemSharedMesh
	{
		std::vector<KdPolygon::Vertex> tri;
		std::vector<KdPolygon::Vertex> wire;
		bool baked = false;
	};

	GemSharedMesh& GetGemMesh()
	{
		static GemSharedMesh m;
		if (m.baked) { return m; }

		const int   lats = RockGemConst::LatitudeSegs;
		const int   lons = RockGemConst::LongitudeSegs;
		const float jStr = RockGemConst::JitterStrength;
		unsigned int seed = RockGemConst::JitterSeed;

		// グリッド頂点（半径1ユニット＋ジッター）
		const int vtxCnt = (lats + 1) * (lons + 1);
		std::vector<Math::Vector3> verts(static_cast<size_t>(vtxCnt));
		for (int lat = 0; lat <= lats; ++lat)
		{
			const float phi    = kGemPi * static_cast<float>(lat) / static_cast<float>(lats);
			const float sinPhi = std::sinf(phi);
			const float cosPhi = std::cosf(phi);
			for (int lon = 0; lon <= lons; ++lon)
			{
				const float theta  = kGemTwoPi * static_cast<float>(lon) / static_cast<float>(lons);
				const float pole   = std::sinf(phi);
				const float jitter = (GemRand(seed) * 2.0f - 1.0f) * jStr * pole;
				const float r      = 1.0f + jitter;
				const int idx = lat * (lons + 1) + lon;
				verts[static_cast<size_t>(idx)] = {
					r * sinPhi * std::cosf(theta),
					r * cosPhi,
					r * sinPhi * std::sinf(theta) };
			}
		}

		// 三角面（グレースケール：明るさのみ。色は描画時に乗算）
		m.tri.reserve(static_cast<size_t>(lats) * lons * 6);
		for (int lat = 0; lat < lats; ++lat)
		{
			for (int lon = 0; lon < lons; ++lon)
			{
				const int i00 = lat       * (lons + 1) + lon;
				const int i01 = lat       * (lons + 1) + (lon + 1);
				const int i10 = (lat + 1) * (lons + 1) + lon;
				const int i11 = (lat + 1) * (lons + 1) + (lon + 1);

				const float t      = static_cast<float>(lat) / static_cast<float>(lats - 1);
				const float bright = 1.0f - t * RockGemConst::DarkFactor;
				const float phase  = kGemTwoPi * static_cast<float>(lon) / static_cast<float>(lons)
									+ static_cast<float>(lat) * 0.9f;
				// 経度方向にわずかに明暗の揺らぎ（面の立体感）。色はグレー。
				const float shade = bright * (0.85f + 0.15f * std::sinf(phase));
				const unsigned int fc = GemColorToUint(shade, shade, shade, RockGemConst::FaceAlpha);

				KdPolygon::Vertex v{};
				v.color = fc;
				v.pos = verts[static_cast<size_t>(i00)]; m.tri.push_back(v);
				v.pos = verts[static_cast<size_t>(i01)]; m.tri.push_back(v);
				v.pos = verts[static_cast<size_t>(i10)]; m.tri.push_back(v);
				v.pos = verts[static_cast<size_t>(i01)]; m.tri.push_back(v);
				v.pos = verts[static_cast<size_t>(i11)]; m.tri.push_back(v);
				v.pos = verts[static_cast<size_t>(i10)]; m.tri.push_back(v);
			}
		}

		// ワイヤー（緯線＋経線。明るめのグレー）
		const unsigned int wc = GemColorToUint(
			RockGemConst::WireBoost, RockGemConst::WireBoost, RockGemConst::WireBoost, RockGemConst::WireAlpha);
		for (int lat = 0; lat <= lats; ++lat)
		{
			for (int lon = 0; lon < lons; ++lon)
			{
				const int iA = lat * (lons + 1) + lon;
				const int iB = lat * (lons + 1) + (lon + 1);
				KdPolygon::Vertex va{}, vb{};
				va.pos = verts[static_cast<size_t>(iA)]; va.color = wc;
				vb.pos = verts[static_cast<size_t>(iB)]; vb.color = wc;
				m.wire.push_back(va); m.wire.push_back(vb);
			}
		}
		for (int lon = 0; lon <= lons; ++lon)
		{
			for (int lat = 0; lat < lats; ++lat)
			{
				const int iA = lat       * (lons + 1) + lon;
				const int iB = (lat + 1) * (lons + 1) + lon;
				KdPolygon::Vertex va{}, vb{};
				va.pos = verts[static_cast<size_t>(iA)]; va.color = wc;
				vb.pos = verts[static_cast<size_t>(iB)]; vb.color = wc;
				m.wire.push_back(va); m.wire.push_back(vb);
			}
		}

		m.baked = true;
		return m;
	}
}

const std::vector<KdPolygon::Vertex>& RockGem::TriVerts()  { return GetGemMesh().tri; }
const std::vector<KdPolygon::Vertex>& RockGem::WireVerts() { return GetGemMesh().wire; }

//==========================================================
// Init ─ コライダー登録＋配置位置からレインボー色を決定
//==========================================================
void RockGem::Init()
{
	// 取得判定コライダー（TypeEvent）。Coin/RockDrop と同じ方式。
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("RockGemHit", Math::Vector3::Zero,
		RockGemConst::HitRadius, KdCollider::TypeEvent);

	// 配置位置をシードに色相を決める（リロードしても色が変わらないよう決定的に）
	unsigned int seed = static_cast<unsigned int>(
		(m_spawnPos.x * 7349.0f) + (m_spawnPos.y * 1277.0f) + (m_spawnPos.z * 977.0f));
	if (seed == 0) { seed = 0x1234567; }
	const float hue = GemRand(seed);
	m_color = HsvToRgb(hue, RockGemConst::Saturation, RockGemConst::Value);

	m_basePos = m_spawnPos;
	m_vel     = Math::Vector3::Zero;
	m_mWorld  = Math::Matrix::CreateTranslation(m_spawnPos);
	SetPos(m_spawnPos);
}

//==========================================================
// Update ─ その場でふわふわ＋自転
//==========================================================
void RockGem::Update()
{
	const float dt = KdFPSController::GetDt();
	m_bobTimer += RockGemConst::BobSpeed * dt;
	m_rotAngle += RockGemConst::SpinSpeed * dt;
	if (m_rotAngle > kGemTwoPi) { m_rotAngle -= kGemTwoPi; }

	// 投擲物は寿命で消える
	if (m_life >= 0.0f)
	{
		m_life -= dt;
		if (m_life <= 0.0f) { m_expired = true; }
	}

	// 飛ばした後の慣性（速度を減衰させながら基準位置を移動）
	m_basePos += m_vel * dt;
	m_vel *= std::pow(RockGemConst::Drag, dt * 60.0f);

	Math::Vector3 pos = m_basePos;
	pos.y += std::sinf(m_bobTimer) * RockGemConst::BobAmp;

	m_mWorld = Math::Matrix::CreateTranslation(pos);
	SetPos(pos);
}

//==========================================================
// DrawEffect ─ 加算でローポリ岩石をレインボー着色して描画
//==========================================================
void RockGem::DrawEffect()
{
	auto& sm = KdShaderManager::Instance();
	auto& shader = sm.m_StandardShader;

	const Math::Vector3 pos = GetPos();
	const Math::Matrix rot  = Math::Matrix::CreateFromAxisAngle(Math::Vector3::UnitY, m_rotAngle);

	// アウトライン：一回り大きい暗い面を前面カリングで描く（背面がシルエット）
	{
		const Math::Matrix oWorld =
			Math::Matrix::CreateScale(RockGemConst::Radius * RockGemConst::OutlineScale) * rot *
			Math::Matrix::CreateTranslation(pos);
		shader.DrawVertices(TriVerts(), oWorld,
			Math::Color(0.03f, 0.03f, 0.05f, 1.0f),
			KdDepthStencilState::ZEnable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			KdRasterizerState::CullFront);
	}

	sm.ChangeBlendState(KdBlendState::Add);

	// 面：グレースケールメッシュ × このジェムのレインボー色
	const Math::Matrix faceWorld =
		Math::Matrix::CreateScale(RockGemConst::Radius) * rot * Math::Matrix::CreateTranslation(pos);
	shader.DrawVertices(TriVerts(), faceWorld,
		m_color,
		KdDepthStencilState::ZEnable,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ワイヤー：面より少し外側＋深度テストのみ。白寄りで縁を立てる。
	const Math::Matrix wireWorld =
		Math::Matrix::CreateScale(RockGemConst::Radius * RockGemConst::WireDepthOutset) * rot
		* Math::Matrix::CreateTranslation(pos);
	const Math::Color wireCol(
		std::min(m_color.R() + 0.4f, 1.0f),
		std::min(m_color.G() + 0.4f, 1.0f),
		std::min(m_color.B() + 0.4f, 1.0f), 1.0f);
	shader.DrawVertices(WireVerts(), wireWorld,
		wireCol,
		KdDepthStencilState::ZWriteDisable);

	sm.UndoBlendState();
}
