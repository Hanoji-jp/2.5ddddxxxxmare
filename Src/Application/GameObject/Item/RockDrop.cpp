#include "../../main.h"
#include "RockDrop.h"
#include "../../Manager/PlanetGravityManager.h"
#include "../../Const/PlanetConst.h"

static constexpr float kRdTwoPi = 6.28318530718f;
static constexpr float kRdPi    = 3.14159265359f;

//----------------------------------------------------------
// 色・乱数ヘルパー
//----------------------------------------------------------
static unsigned int RdColorToUint(float r, float g, float b, float a)
{
	auto u8 = [](float c) { return static_cast<unsigned char>(std::min(std::max(c, 0.0f), 1.0f) * 255.0f); };
	return (static_cast<unsigned int>(u8(a)) << 24)
		 | (static_cast<unsigned int>(u8(b)) << 16)
		 | (static_cast<unsigned int>(u8(g)) << 8)
		 |  static_cast<unsigned int>(u8(r));
}

static float RdRand(unsigned int& seed)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
	return static_cast<float>(seed & 0xFFFF) / static_cast<float>(0xFFFF);
}

//==========================================================
// 共有メッシュ（半径1ユニットのローポリ岩石）：全インスタンス共通で1度だけベイク
//==========================================================
namespace
{
	struct RdSharedMesh
	{
		std::vector<KdPolygon::Vertex> tri;
		std::vector<KdPolygon::Vertex> wire;
		bool baked = false;
	};

	RdSharedMesh& GetSharedMesh()
	{
		static RdSharedMesh m;
		if (m.baked) { return m; }

		const int   lats = RockConst::LatitudeSegs;
		const int   lons = RockConst::LongitudeSegs;
		const float jStr = RockConst::JitterStrength;
		unsigned int seed = RockConst::JitterSeed;

		// グリッド頂点（半径1ユニット＋ジッター）
		const int vtxCnt = (lats + 1) * (lons + 1);
		std::vector<Math::Vector3> verts(static_cast<size_t>(vtxCnt));
		for (int lat = 0; lat <= lats; ++lat)
		{
			const float phi    = kRdPi * static_cast<float>(lat) / static_cast<float>(lats);
			const float sinPhi = std::sinf(phi);
			const float cosPhi = std::cosf(phi);
			for (int lon = 0; lon <= lons; ++lon)
			{
				const float theta  = kRdTwoPi * static_cast<float>(lon) / static_cast<float>(lons);
				const float pole   = std::sinf(phi);
				const float jitter = (RdRand(seed) * 2.0f - 1.0f) * jStr * pole;
				const float r      = 1.0f + jitter;
				const int idx = lat * (lons + 1) + lon;
				verts[static_cast<size_t>(idx)] = {
					r * sinPhi * std::cosf(theta),
					r * cosPhi,
					r * sinPhi * std::sinf(theta) };
			}
		}

		// 三角面
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
				const float bright = 1.0f - t * RockConst::DarkFactor;
				const float phase  = kRdTwoPi * static_cast<float>(lon) / static_cast<float>(lons)
									+ static_cast<float>(lat) * 0.9f;
				const unsigned int fc = RdColorToUint(
					RockConst::ColorR * bright * (0.8f + 0.2f * std::sinf(phase)),
					RockConst::ColorG * bright * (0.8f + 0.2f * std::cosf(phase * 0.7f)),
					RockConst::ColorB * bright,
					RockConst::FaceAlpha);

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

		// ワイヤー（緯線＋経線）
		for (int lat = 0; lat <= lats; ++lat)
		{
			const float t      = static_cast<float>(lat) / static_cast<float>(lats);
			const float bright = (1.0f - t * RockConst::DarkFactor) * RockConst::WireBoost;
			const unsigned int wc = RdColorToUint(
				RockConst::ColorR * bright + 0.35f * bright,
				RockConst::ColorG * bright + 0.20f * bright,
				RockConst::ColorB * bright + 0.10f * bright,
				RockConst::WireAlpha);
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
				const float tA = static_cast<float>(lat)     / static_cast<float>(lats);
				const float bA = (1.0f - tA * RockConst::DarkFactor) * RockConst::WireBoost;
				const unsigned int wc = RdColorToUint(
					RockConst::ColorR * bA + 0.35f * bA,
					RockConst::ColorG * bA + 0.20f * bA,
					RockConst::ColorB * bA + 0.10f * bA,
					RockConst::WireAlpha);
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

const std::vector<KdPolygon::Vertex>& RockDrop::TriVerts()  { return GetSharedMesh().tri; }
const std::vector<KdPolygon::Vertex>& RockDrop::WireVerts() { return GetSharedMesh().wire; }
void RockDrop::BakeShared() { GetSharedMesh(); }

//==========================================================
// Spawn ─ upDir 基準で打ち上げて散らばらせる
//==========================================================
void RockDrop::Spawn(const Math::Vector3& spawnPos, const Math::Vector3& upDir)
{
	m_pos    = spawnPos;
	m_upDir  = upDir;
	m_upDir.Normalize();
	// 着地面：spawn 高さではなく、下向きレイで検出した「実際の地面」に合わせる。
	// （敵を倒した高さに関係なく、必ず地面の上に落ちる）
	{
		// フォールバック（地面が見つからない場合）
		m_groundLvl = m_pos.Dot(m_upDir) + RockConst::LandHeightOffset;

		const Math::Vector3 rayStart = m_pos + m_upDir * RockConst::GroundRayUp;
		const KdCollider::RayInfo ray(
			KdCollider::TypeGround, rayStart, -m_upDir, RockConst::GroundRayLen);

		const float capSurface = PlanetConst::GrassCapThickness * 2.0f; // 草キャップ上面に合わせる
		bool  found  = false;
		float bestUp = 0.0f;
		for (const auto& p : PlanetGravityManager::Instance().GetPlanets())
		{
			if (p.Shape != PlanetShape::Box || !p.pCollider) { continue; }
			std::list<KdCollider::CollisionResult> results;
			if (!p.pCollider->Intersects(ray, p.mWorld, &results)) { continue; }
			for (const auto& r : results)
			{
				if (r.m_hitNDir.Dot(m_upDir) < 0.5f) { continue; }   // 上向き面のみ
				const float hu = r.m_hitPos.Dot(m_upDir);
				if (!found || hu > bestUp) { bestUp = hu; found = true; }   // 一番上の面
			}
		}
		if (found)
		{
			m_groundLvl = bestUp + capSurface + RockConst::LandHeightOffset;
		}
		m_hasGround = found;                       // 地面が無ければ落下し続ける
		m_spawnUp   = m_pos.Dot(m_upDir);          // 落下消滅判定の基準
	}

	// このゲームは Z 方向に移動しない 2.5D なので、散らばりは XY 平面内のみに限定。
	// up に垂直な XY 平面内の横方向（Z 成分なし）
	Math::Vector3 tanXY{ -m_upDir.y, m_upDir.x, 0.0f };
	if (tanXY.LengthSquared() < 1e-6f) { tanXY = { 1.0f, 0.0f, 0.0f }; }
	tanXY.Normalize();

	static unsigned int s_counter = 0x9E3779B9;
	s_counter = s_counter * 1664525u + 1013904223u;   // 個体ごとに振り直す
	unsigned int seed = static_cast<unsigned int>(
		(spawnPos.x * 7349.0f) + (spawnPos.y * 1277.0f) + (spawnPos.z * 977.0f)) ^ s_counter;
	if (seed == 0) { seed = 0x1234567; }

	const float sign  = (RdRand(seed) < 0.5f) ? -1.0f : 1.0f;   // 左右どちらかへ
	const float hSpd  = (RockConst::BurstSpeedMin + RdRand(seed) * (RockConst::BurstSpeedMax - RockConst::BurstSpeedMin)) * sign;
	const float upSpd = RockConst::BurstUpMin + RdRand(seed) * (RockConst::BurstUpMax - RockConst::BurstUpMin);

	// 横(XY平面内) ＋ 上 のみ。Z 初速は 0 なので Z 座標は spawn のまま固定。
	m_velocity = tanXY * hSpd + m_upDir * upSpd;

	// 取得判定コライダー（TypeEvent）。m_mWorld は平行移動のみ（スケールを含めない）
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape("RockHit", Math::Vector3::Zero,
		RockConst::HitRadius, KdCollider::TypeEvent);

	m_mWorld = Math::Matrix::CreateTranslation(m_pos);
}

//==========================================================
// Update ─ 物理（落下・着地）＋自転
//==========================================================
void RockDrop::Update()
{
	const float dt = KdFPSController::GetDt();
	m_age += dt;
	m_rotAngle += RockConst::SpinSpeed * dt;

	if (!m_landed)
	{
		m_velocity -= m_upDir * (RockConst::Gravity * dt);
		m_pos += m_velocity * dt;

		// 真下に地面が無ければ着地せず落下し続け、十分落ちたら消滅
		if (!m_hasGround)
		{
			if (m_spawnUp - m_pos.Dot(m_upDir) >= RockConst::FallDespawnDist) { m_expired = true; }
			m_mWorld = Math::Matrix::CreateTranslation(m_pos);
			SetPos(m_pos);
			return;
		}

		// 着地面（up 座標）を下回ったらクランプ＆バウンド
		const float up = m_pos.Dot(m_upDir);
		if (up <= m_groundLvl)
		{
			m_pos += m_upDir * (m_groundLvl - up);   // 面に戻す
			const float vUp = m_velocity.Dot(m_upDir);
			if (vUp < 0.0f)
			{
				// 水平成分（摩擦で減衰）と上成分（反発）を分けて作り直す
				Math::Vector3 horiz = m_velocity - m_upDir * vUp;     // 水平成分のみ
				horiz *= RockConst::BounceFriction;
				const float bounceUp = -vUp * RockConst::Restitution; // 反発の上向き速度（フル）
				m_velocity = horiz + m_upDir * bounceUp;
			}
			if (m_velocity.Length() < RockConst::RestThreshold) { m_landed = true; }
		}
	}

	// 描画/コライダー用の中心位置（着地後は軽くバウンド）
	Math::Vector3 drawPos = m_pos;
	if (m_landed)
	{
		const float bob = std::sinf(m_age * RockConst::LandedBobSpeed) * RockConst::LandedBobAmp;
		drawPos += m_upDir * bob;
	}

	// コライダーは平行移動のみ（スケールを含めない）
	m_mWorld = Math::Matrix::CreateTranslation(drawPos);
	SetPos(drawPos);
}

//==========================================================
// DrawEffect ─ 加算でローポリ岩石を描画（小型）
//==========================================================
void RockDrop::DrawEffect()
{
	auto& sm = KdShaderManager::Instance();
	auto& shader = sm.m_StandardShader;

	const Math::Vector3 pos = GetPos();
	const Math::Matrix rot  = Math::Matrix::CreateFromAxisAngle(m_upDir, m_rotAngle);

	// ── アウトライン：一回り大きい暗い面を前面カリングで描く（背面がシルエット）──
	//   不透明で深度を書き、この後の本体が内側を上書きして縁だけ残る。
	{
		const Math::Matrix oWorld =
			Math::Matrix::CreateScale(RockConst::Radius * RockConst::OutlineScale) * rot *
			Math::Matrix::CreateTranslation(pos);
		shader.DrawVertices(TriVerts(), oWorld,
			Math::Color(0.03f, 0.03f, 0.05f, 1.0f),
			KdDepthStencilState::ZEnable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			KdRasterizerState::CullFront);
	}

	sm.ChangeBlendState(KdBlendState::Add);

	// 面：少し縮めて深度を書く → 裏側の線を隠す
	const Math::Matrix faceWorld =
		Math::Matrix::CreateScale(RockConst::Radius) * rot * Math::Matrix::CreateTranslation(pos);
	shader.DrawVertices(TriVerts(), faceWorld,
		Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZEnable,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ワイヤー：面より少し外側＋深度テストのみ
	const Math::Matrix wireWorld =
		Math::Matrix::CreateScale(RockConst::Radius * RockConst::WireDepthOutset) * rot
		* Math::Matrix::CreateTranslation(pos);
	shader.DrawVertices(WireVerts(), wireWorld,
		Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZWriteDisable);

	sm.UndoBlendState();
}
