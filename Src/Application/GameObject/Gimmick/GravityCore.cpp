#include "../../../Pch.h"
#include "GravityCore.h"
#include "../../../Framework/Utility/KdFPSController.h"
#include "../../../Framework/Shader/KdShaderManager.h"
#include "../../Const/GravityCoreConst.h"

static constexpr float kTwoPi = 6.28318530718f;
static constexpr float kPi    = 3.14159265359f;

//----------------------------------------------------------
// 色ヘルパー (WarpHole と同じ実装)
//----------------------------------------------------------
static unsigned int GcColorToUint(const Math::Color& c)
{
	const unsigned char r = static_cast<unsigned char>(std::min(c.R() * 255.0f, 255.0f));
	const unsigned char g = static_cast<unsigned char>(std::min(c.G() * 255.0f, 255.0f));
	const unsigned char b = static_cast<unsigned char>(std::min(c.B() * 255.0f, 255.0f));
	const unsigned char a = static_cast<unsigned char>(std::min(c.A() * 255.0f, 255.0f));
	return (static_cast<unsigned int>(a) << 24)
		 | (static_cast<unsigned int>(b) << 16)
		 | (static_cast<unsigned int>(g) << 8)
		 |  static_cast<unsigned int>(r);
}

//----------------------------------------------------------
// 簡易 LCG 擬似乱数 (シード固定で毎回同じ形)
//----------------------------------------------------------
static float GcRand(unsigned int& seed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return static_cast<float>(seed & 0xFFFF) / static_cast<float>(0xFFFF);
}

//==========================================================
// Init
//==========================================================
void GravityCore::Init(const Math::Vector3& pos, float radius, CoreType type)
{
	m_pos    = pos;
	m_radius = radius;
	m_type   = type;

	// カリング半径を球より少し大きめに設定
	m_cullingRadius = radius * GravityCoreConst::CullingRadiusMul;

	// ワールド行列を位置に合わせる
	m_mWorld = Math::Matrix::CreateTranslation(m_pos);

	if (m_type == CoreType::Rock) { BakeMesh(); }

	// 本体色（タイプ別）：星・グローの色に流用
	if (m_type == CoreType::Glow)
	{
		m_coreColor = { GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
						GravityCoreConst::GlowFaceB, 1.0f };
	}
	else
	{
		m_coreColor = { GravityCoreConst::FaceColorR, GravityCoreConst::FaceColorG,
						GravityCoreConst::FaceColorB, 1.0f };
	}

	// 星きらめきのみ（Effekseer の広がるリングは使わない）
	ItemEffect::Params fxp;
	fxp.starSize    = SparkleConst::ParasolStarSize;
	fxp.orbitRadius = SparkleConst::ParasolStarRadius;
	fxp.colorShift  = SparkleConst::ParasolColorShift;
	fxp.color       = m_coreColor;
	m_effect.Init(m_pos, fxp);   // efkPath 無し → 星のみ

	// 中心をふんわり光らせる glow 画像ビルボード
	m_glowTex = std::make_shared<KdTexture>();
	if (m_glowTex->Load(GravityCoreConst::GlowSpriteTexPath))
	{
		m_glowPoly.SetMaterial(m_glowTex);
		m_glowPoly.SetScale(1.0f);
	}

	// Glow コアは取得判定コライダー（TypeEvent）を持つ → 触れるとゴールが開く
	if (m_type == CoreType::Glow)
	{
		m_pCollider = std::make_unique<KdCollider>();
		m_pCollider->RegisterCollisionShape("CoreHit", Math::Vector3::Zero,
			m_radius * GravityCoreConst::PickupRadiusMul, KdCollider::TypeEvent);
	}
}

//==========================================================
// BakeMesh ─ ジッター込みのローポリ球体頂点を事前計算
//==========================================================
void GravityCore::BakeMesh()
{
	const int   lats = GravityCoreConst::LatitudeSegs;
	const int   lons = GravityCoreConst::LongitudeSegs;
	const float jStr = GravityCoreConst::JitterStrength;

	unsigned int seed = GravityCoreConst::JitterSeed;

	// (lats+1) × (lons+1) のグリッド頂点
	const int vtxCnt = (lats + 1) * (lons + 1);
	m_baseVerts.resize(static_cast<size_t>(vtxCnt));

	for (int lat = 0; lat <= lats; ++lat)
	{
		const float phi    = kPi * static_cast<float>(lat) / static_cast<float>(lats);
		const float sinPhi = std::sinf(phi);
		const float cosPhi = std::cosf(phi);

		for (int lon = 0; lon <= lons; ++lon)
		{
			const float theta    = kTwoPi * static_cast<float>(lon) / static_cast<float>(lons);
			const float sinTheta = std::sinf(theta);
			const float cosTheta = std::cosf(theta);

			// 極では jitter を抑えてトポロジの破綻を防ぐ
			const float poleBlend = std::sinf(phi); // 0 at poles, 1 at equator
			const float jitter    = (GcRand(seed) * 2.0f - 1.0f) * jStr * poleBlend;
			const float r         = m_radius * (1.0f + jitter);

			const int idx = lat * (lons + 1) + lon;
			m_baseVerts[static_cast<size_t>(idx)] = {
				r * sinPhi * cosTheta,
				r * cosPhi,
				r * sinPhi * sinTheta
			};
		}
	}

	// ── 三角面バッファを事前生成 ──────────────────────────────
	// quad = 2 tri = 6 頂点
	m_triVerts.clear();
	m_triVerts.reserve(static_cast<size_t>(lats) * lons * 6);

	// 面ごとの色：緯度×経度でフラットシェーディング風にばらす
	for (int lat = 0; lat < lats; ++lat)
	{
		for (int lon = 0; lon < lons; ++lon)
		{
			const int i00 = lat       * (lons + 1) + lon;
			const int i01 = lat       * (lons + 1) + (lon + 1);
			const int i10 = (lat + 1) * (lons + 1) + lon;
			const int i11 = (lat + 1) * (lons + 1) + (lon + 1);

			// 緯度に応じた暗さ（上極=明るい、下極=暗い）
			const float t      = static_cast<float>(lat) / static_cast<float>(lats - 1);
			const float bright = 1.0f - t * GravityCoreConst::DarkFactor;

			// 面ごとのバリエーション（位相をずらして岩石感）
			const float phase  = kTwoPi * static_cast<float>(lon) / static_cast<float>(lons)
								+ static_cast<float>(lat) * 0.9f;
			const float rMod   = GravityCoreConst::FaceColorR * bright * (0.8f + 0.2f * std::sinf(phase));
			const float gMod   = GravityCoreConst::FaceColorG * bright * (0.8f + 0.2f * std::cosf(phase * 0.7f));
			const float bMod   = GravityCoreConst::FaceColorB * bright;

			const unsigned int fc = GcColorToUint({
				std::min(rMod, 1.0f),
				std::min(gMod, 1.0f),
				std::min(bMod, 1.0f),
				GravityCoreConst::FaceAlpha });

			// tri 1: i00, i01, i10
			KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
			v0.pos = m_baseVerts[static_cast<size_t>(i00)]; v0.color = fc;
			v1.pos = m_baseVerts[static_cast<size_t>(i01)]; v1.color = fc;
			v2.pos = m_baseVerts[static_cast<size_t>(i10)]; v2.color = fc;
			m_triVerts.push_back(v0);
			m_triVerts.push_back(v1);
			m_triVerts.push_back(v2);

			// tri 2: i01, i11, i10
			v3.pos = m_baseVerts[static_cast<size_t>(i01)]; v3.color = fc;
			v4.pos = m_baseVerts[static_cast<size_t>(i11)]; v4.color = fc;
			v5.pos = m_baseVerts[static_cast<size_t>(i10)]; v5.color = fc;
			m_triVerts.push_back(v3);
			m_triVerts.push_back(v4);
			m_triVerts.push_back(v5);
		}
	}

	// ── ワイヤーバッファを事前生成 ──────────────────────────────
	// 緯線＋経線
	m_wireVerts.clear();
	m_wireVerts.reserve(
		static_cast<size_t>(lats + 1) * lons * 2     // 緯線
		+ static_cast<size_t>(lats) * (lons + 1) * 2 // 経線（縦エッジ）
	);

	// 緯線リング
	for (int lat = 0; lat <= lats; ++lat)
	{
		const float t      = static_cast<float>(lat) / static_cast<float>(lats);
		const float bright = (1.0f - t * GravityCoreConst::DarkFactor) * GravityCoreConst::WireBoost;
		const unsigned int wc = GcColorToUint({
			std::min(GravityCoreConst::FaceColorR * bright + 0.35f * bright, 1.0f),
			std::min(GravityCoreConst::FaceColorG * bright + 0.20f * bright, 1.0f),
			std::min(GravityCoreConst::FaceColorB * bright + 0.10f * bright, 1.0f),
			GravityCoreConst::WireAlpha });

		for (int lon = 0; lon < lons; ++lon)
		{
			const int iA = lat * (lons + 1) + lon;
			const int iB = lat * (lons + 1) + (lon + 1);
			KdPolygon::Vertex va{}, vb{};
			va.pos = m_baseVerts[static_cast<size_t>(iA)]; va.color = wc;
			vb.pos = m_baseVerts[static_cast<size_t>(iB)]; vb.color = wc;
			m_wireVerts.push_back(va);
			m_wireVerts.push_back(vb);
		}
	}

	// 経線スポーク
	for (int lon = 0; lon <= lons; ++lon)
	{
		for (int lat = 0; lat < lats; ++lat)
		{
			const float tA      = static_cast<float>(lat)     / static_cast<float>(lats);
			const float tB      = static_cast<float>(lat + 1) / static_cast<float>(lats);
			const float brightA = (1.0f - tA * GravityCoreConst::DarkFactor) * GravityCoreConst::WireBoost;
			const float brightB = (1.0f - tB * GravityCoreConst::DarkFactor) * GravityCoreConst::WireBoost;

			const unsigned int wcA = GcColorToUint({
				std::min(GravityCoreConst::FaceColorR * brightA + 0.35f * brightA, 1.0f),
				std::min(GravityCoreConst::FaceColorG * brightA + 0.20f * brightA, 1.0f),
				std::min(GravityCoreConst::FaceColorB * brightA + 0.10f * brightA, 1.0f),
				GravityCoreConst::WireAlpha });

			const unsigned int wcB = GcColorToUint({
				std::min(GravityCoreConst::FaceColorR * brightB + 0.35f * brightB, 1.0f),
				std::min(GravityCoreConst::FaceColorG * brightB + 0.20f * brightB, 1.0f),
				std::min(GravityCoreConst::FaceColorB * brightB + 0.10f * brightB, 1.0f),
				GravityCoreConst::WireAlpha });

			const int iA = lat       * (lons + 1) + lon;
			const int iB = (lat + 1) * (lons + 1) + lon;
			KdPolygon::Vertex va{}, vb{};
			va.pos = m_baseVerts[static_cast<size_t>(iA)]; va.color = wcA;
			vb.pos = m_baseVerts[static_cast<size_t>(iB)]; vb.color = wcB;
			m_wireVerts.push_back(va);
			m_wireVerts.push_back(vb);
		}
	}
}

//==========================================================
// Update ─ 自転 + Glow アニメ時間更新
//==========================================================
void GravityCore::Update()
{
	const float dt  = KdFPSController::GetDt();
	const float rot = (m_type == CoreType::Glow)
					  ? GravityCoreConst::GlowRotSpeed
					  : GravityCoreConst::RotSpeed;
	m_rotAngle += rot * dt;
	m_animTime += dt;

	m_mWorld = Math::Matrix::CreateRotationY(m_rotAngle)
			 * Math::Matrix::CreateTranslation(m_pos);

	// 星きらめき更新＋Effekseer位置追従
	m_effect.Update(m_pos, dt);
}

//==========================================================
// DrawEffect ─ タイプ分岐
//==========================================================
void GravityCore::DrawEffect()
{
	// 星きらめきエフェクト（タイプ問わず常時）
	m_effect.DrawEffect(m_pos);

	// 中心をふんわり光らせる glow 画像（加算・ゆるく脈動）
	if (m_glowTex)
	{
		auto& sm = KdShaderManager::Instance();
		sm.ChangeBlendState(KdBlendState::Add);
		sm.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
		sm.m_StandardShader.SetDissolve(0.0f);

		const float pulse = 1.0f + GravityCoreConst::GlowSpritePulseAmp
			* std::sinf(m_animTime * GravityCoreConst::GlowSpritePulseSpeed);
		const float size  = m_radius * GravityCoreConst::GlowSpriteSizeMul * pulse;

		const Math::Color   col{ m_coreColor.x, m_coreColor.y, m_coreColor.z,
								 GravityCoreConst::GlowSpriteAlpha * pulse };
		const Math::Vector3 emis{ m_coreColor.x, m_coreColor.y, m_coreColor.z };
		EffectBase::DrawBillboard(m_glowPoly, m_pos, size, 0.0f, col, emis);

		sm.UndoDepthStencilState();
		sm.UndoBlendState();
	}

	if (m_type == CoreType::Rock)
	{
		auto& sm = KdShaderManager::Instance();
		// 加算ブレンドで描画 → 不透明な面で奥を埋めず、光を足すだけになる
		sm.ChangeBlendState(KdBlendState::Add);
		auto& shader = sm.m_StandardShader;

		// 面は深度を書く（ZEnable）→ 前面の半球が裏側の線を隠す
		if (!m_triVerts.empty())
		{
			shader.DrawVertices(m_triVerts, m_mWorld,
				Math::Color(1, 1, 1, 1),
				KdDepthStencilState::ZEnable,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}
		// ワイヤーは面より少し外側＋深度テストのみ（書き込まない）
		// → 手前側の線だけが通り、裏側の線は前面の面に隠れて消える
		if (!m_wireVerts.empty())
		{
			const Math::Matrix wireWorld =
				Math::Matrix::CreateScale(GravityCoreConst::WireDepthOutset) * m_mWorld;
			shader.DrawVertices(m_wireVerts, wireWorld,
				Math::Color(1, 1, 1, 1),
				KdDepthStencilState::ZWriteDisable);
		}
		sm.UndoBlendState();
	}
	else
	{
		DrawGlowEffect();
	}
}

//==========================================================
// DrawDebug ─ 球体の半径範囲を ImGui で表示
//==========================================================
void GravityCore::DrawDebug()
{
	// デバッグ表示は KdDebugGUI ではなく ImGui ウィンドウ外で OK
}

//==========================================================
// Glow 描画ヘルパー
// WarpHole の DrawFunnelFace を球体版に焼き直したもの
//==========================================================
void GravityCore::DrawGlowEffect()
{
	const int   lats  = GravityCoreConst::LatitudeSegs;
	const int   lons  = GravityCoreConst::LongitudeSegs;
	const float r     = m_radius;
	const float wAmp  = GravityCoreConst::GlowWaveAmp;
	const float wFreqR = GravityCoreConst::GlowWaveFreqR;
	const float wFreqS = GravityCoreConst::GlowWaveFreqS;

	// ワールド行列なしでローカル座標を直接生成し m_mWorld を渡す
	auto GetVert = [&](int lat, int lon) -> Math::Vector3
	{
		const float phi   = kPi    * static_cast<float>(lat) / static_cast<float>(lats);
		const float theta = kTwoPi * static_cast<float>(lon) / static_cast<float>(lons);
		const float t     = static_cast<float>(lat) / static_cast<float>(lats - 1);

		// 波アニメ（WarpHole の FunnelWave を球体版に）
		const float phase = kTwoPi * (wFreqR * t - m_animTime)
						  + kTwoPi * wFreqS * static_cast<float>(lon) / static_cast<float>(lons);
		const float wave  = std::sinf(phase) * wAmp;
		const float rv    = r * (1.0f + wave);

		return {
			rv * std::sinf(phi) * std::cosf(theta),
			rv * std::cosf(phi),
			rv * std::sinf(phi) * std::sinf(theta)
		};
	};

	// WarpHole の FaceColor に相当：シアン/青/白グラデーション
	auto FaceCol = [&](int lat, int lon, float alpha) -> unsigned int
	{
		const float t      = static_cast<float>(lat) / static_cast<float>(lats - 1);
		const float bright = 1.0f - t * 0.5f;
		const float phase  = kTwoPi * static_cast<float>(lon) / static_cast<float>(lons)
						   + static_cast<float>(lat) * 0.8f;
		const float rVal = GravityCoreConst::GlowFaceR * (0.5f + std::sinf(phase) * 0.5f);
		const float gVal = GravityCoreConst::GlowFaceG * bright * (0.7f + std::cosf(phase * 0.7f) * 0.3f);
		const float bVal = GravityCoreConst::GlowFaceB * bright;
		return GcColorToUint({
			std::min(rVal, 1.0f),
			std::min(gVal, 1.0f),
			std::min(bVal, 1.0f),
			alpha });
	};

	auto WireCol = [&](int lat) -> unsigned int
	{
		const float t = static_cast<float>(lat) / static_cast<float>(lats);
		const float b = (1.0f - t * 0.5f);
		return GcColorToUint({
			std::min(GravityCoreConst::GlowFaceR * b + 0.5f * b, 1.0f),
			std::min(GravityCoreConst::GlowFaceG * b + 0.25f * b, 1.0f),
			std::min(GravityCoreConst::GlowFaceB * b + 0.10f * b, 1.0f),
			GravityCoreConst::GlowWireAlpha });
	};

	// ── 三角面 ──────────────────────────────────────────────
	m_triVerts.clear();
	m_triVerts.reserve(static_cast<size_t>(lats) * lons * 6);
	for (int lat = 0; lat < lats; ++lat)
	{
		for (int lon = 0; lon < lons; ++lon)
		{
			const int lonNext = (lon + 1) % lons;
			const Math::Vector3 p00 = GetVert(lat,     lon);
			const Math::Vector3 p01 = GetVert(lat,     lonNext);
			const Math::Vector3 p10 = GetVert(lat + 1, lon);
			const Math::Vector3 p11 = GetVert(lat + 1, lonNext);
			const float alpha = GravityCoreConst::GlowFaceAlpha;
			const unsigned int fc = FaceCol(lat, lon, alpha);

			KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
			v0.pos = p00; v0.color = fc;
			v1.pos = p01; v1.color = fc;
			v2.pos = p10; v2.color = fc;
			m_triVerts.push_back(v0); m_triVerts.push_back(v1); m_triVerts.push_back(v2);
			v3.pos = p01; v3.color = fc;
			v4.pos = p11; v4.color = fc;
			v5.pos = p10; v5.color = fc;
			m_triVerts.push_back(v3); m_triVerts.push_back(v4); m_triVerts.push_back(v5);
		}
	}

	// カメラ手前側の線分だけ描く（裏側の線を出さないようにする）
	const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
	auto segFront = [&](const Math::Vector3& la, const Math::Vector3& lb) -> bool
	{
		const Math::Vector3 wm = Math::Vector3::Transform((la + lb) * 0.5f, m_mWorld);
		return (wm - m_pos).Dot(camPos - wm) > 0.0f;
	};

	// ── ワイヤー ─────────────────────────────────────────────
	m_wireVerts.clear();
	m_wireVerts.reserve(
		static_cast<size_t>(lats + 1) * lons * 2
		+ static_cast<size_t>(lats) * (lons + 1) * 2);
	for (int lat = 0; lat <= lats; ++lat)
	{
		const unsigned int wc = WireCol(lat);
		for (int lon = 0; lon < lons; ++lon)
		{
			const int lonNext = (lon + 1) % lons;
			const Math::Vector3 a = GetVert(lat, lon);
			const Math::Vector3 b = GetVert(lat, lonNext);
			if (!segFront(a, b)) { continue; }   // 裏側の線はスキップ
			KdPolygon::Vertex va{}, vb{};
			va.pos = a; va.color = wc;
			vb.pos = b; vb.color = wc;
			m_wireVerts.push_back(va); m_wireVerts.push_back(vb);
		}
	}
	for (int lon = 0; lon <= lons; ++lon)
	{
		for (int lat = 0; lat < lats; ++lat)
		{
			const Math::Vector3 a = GetVert(lat,     lon);
			const Math::Vector3 b = GetVert(lat + 1, lon);
			if (!segFront(a, b)) { continue; }   // 裏側の線はスキップ
			KdPolygon::Vertex va{}, vb{};
			va.pos = a; va.color = WireCol(lat);
			vb.pos = b; vb.color = WireCol(lat + 1);
			m_wireVerts.push_back(va); m_wireVerts.push_back(vb);
		}
	}

	auto& shader = KdShaderManager::Instance().m_StandardShader;
	if (!m_triVerts.empty())
	{
		shader.DrawVertices(m_triVerts, m_mWorld,
			Math::Color(1, 1, 1, 1),
			KdDepthStencilState::ZWriteDisable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
	if (!m_wireVerts.empty())
	{
		shader.DrawVertices(m_wireVerts, m_mWorld,
			Math::Color(1, 1, 1, 1),
			KdDepthStencilState::ZWriteDisable);
	}
}

//==========================================================
// DrawBright ─ Glow 用 bloom パス（Rock は何もしない）
//==========================================================
void GravityCore::DrawBright()
{
	if (m_type != CoreType::Glow) { return; }
	DrawGlowBright();
}

void GravityCore::DrawGlowBright()
{
	const int   lats  = GravityCoreConst::LatitudeSegs;
	const int   lons  = GravityCoreConst::LongitudeSegs;
	const float r     = m_radius;
	const float wAmp  = GravityCoreConst::GlowWaveAmp;
	const float wFreqR = GravityCoreConst::GlowWaveFreqR;
	const float wFreqS = GravityCoreConst::GlowWaveFreqS;
	const float alpha  = GravityCoreConst::GlowBloomAlpha;

	auto GetVert = [&](int lat, int lon) -> Math::Vector3
	{
		const float phi   = kPi    * static_cast<float>(lat) / static_cast<float>(lats);
		const float theta = kTwoPi * static_cast<float>(lon) / static_cast<float>(lons);
		const float t     = static_cast<float>(lat) / static_cast<float>(lats - 1);
		const float phase = kTwoPi * (wFreqR * t - m_animTime)
						  + kTwoPi * wFreqS * static_cast<float>(lon) / static_cast<float>(lons);
		const float wave  = std::sinf(phase) * wAmp;
		const float rv    = r * (1.0f + wave);
		return {
			rv * std::sinf(phi) * std::cosf(theta),
			rv * std::cosf(phi),
			rv * std::sinf(phi) * std::sinf(theta)
		};
	};

	// Bloom 用：中心(t=0)は暗く、外周(t=1)が明るい（WarpHole のinvertGradient 相当）
	auto BloomCol = [&](int lat) -> unsigned int
	{
		const float t  = static_cast<float>(lat) / static_cast<float>(lats);
		const float br = t * t;   // 外に向かうほど明るく
		return GcColorToUint({
			std::min(GravityCoreConst::GlowFaceR * br + 0.8f * br, 1.0f),
			std::min(GravityCoreConst::GlowFaceG * br + 0.5f * br, 1.0f),
			std::min(GravityCoreConst::GlowFaceB * br, 1.0f),
			alpha * br });
	};

	m_triVerts.clear();
	m_triVerts.reserve(static_cast<size_t>(lats) * lons * 6);
	for (int lat = 0; lat < lats; ++lat)
	{
		for (int lon = 0; lon < lons; ++lon)
		{
			const int lonNext = (lon + 1) % lons;
			const Math::Vector3 p00 = GetVert(lat,     lon);
			const Math::Vector3 p01 = GetVert(lat,     lonNext);
			const Math::Vector3 p10 = GetVert(lat + 1, lon);
			const Math::Vector3 p11 = GetVert(lat + 1, lonNext);

			const unsigned int fc = BloomCol(lat);
			KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
			v0.pos = p00; v0.color = fc;
			v1.pos = p01; v1.color = fc;
			v2.pos = p10; v2.color = fc;
			m_triVerts.push_back(v0); m_triVerts.push_back(v1); m_triVerts.push_back(v2);
			v3.pos = p01; v3.color = fc;
			v4.pos = p11; v4.color = fc;
			v5.pos = p10; v5.color = fc;
			m_triVerts.push_back(v3); m_triVerts.push_back(v4); m_triVerts.push_back(v5);
		}
	}

	if (!m_triVerts.empty())
	{
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			m_triVerts, m_mWorld,
			Math::Color(1, 1, 1, 1),
			KdDepthStencilState::ZWriteDisable,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}
