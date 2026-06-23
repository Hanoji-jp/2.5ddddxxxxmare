#pragma once
#include "../../Framework/Shader/SpriteShader/KdSpriteShader.h"
#include "../Const/GravityCoreConst.h"
#include "../Const/RockConst.h"
#include <vector>
#include <algorithm>
#include <cmath>

//==========================================================
// CoreIcon
// 重力コアを 2D HUD へ DrawTriangle/DrawLine で投影描画する共通ヘルパー。
// テクスチャを使わず頂点を生成して描く（HPの宝石描画と同じ方式）。
//   glow=false : Rock型（不透明な岩・茶色）
//   glow=true  : Glow型（半透明の青いクリスタル＋ワイヤー＋外周ハロー）。
//                色は GravityCore::DrawGlowEffect の FaceCol/WireCol をそのまま移植。
//   cx,cy : 画面中心原点(+X右 / +Y上)  size : 直径(px)  spin : 自転角/アニメ時間(rad)
//==========================================================
namespace CoreIcon
{
	inline void Draw(KdSpriteShader& sprite, int cx, int cy, int size, float spin,
		const Math::Color& baseColor = Math::Color(GravityCoreConst::FaceColorR,
			GravityCoreConst::FaceColorG, GravityCoreConst::FaceColorB, 1.0f),
		bool glow = false)
	{
		const float R = size * 0.5f;
		const int lats = glow ? GravityCoreConst::LatitudeSegs : RockConst::LatitudeSegs;
		const int lons = glow ? GravityCoreConst::LongitudeSegs : RockConst::LongitudeSegs;
		constexpr float kPi = 3.14159265f, kTau = 6.28318530f;

		struct PV { float x, y, z; };
		std::vector<PV> proj(static_cast<size_t>((lats + 1) * (lons + 1)));
		auto vidx = [&](int la, int lo) { return la * (lons + 1) + lo; };

		const float cY = std::cos(spin),  sY = std::sin(spin);
		const float cX = std::cos(-0.3f), sX = std::sin(-0.3f);

		auto jit = [&](int la, int lo) -> float
		{
			lo = ((lo % lons) + lons) % lons;
			unsigned int h = static_cast<unsigned int>(la * 73856093) ^ static_cast<unsigned int>((lo + 1) * 19349663);
			h ^= h >> 13; h *= 0x85EBCA6Bu; h ^= h >> 16;
			return static_cast<float>(h & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;
		};

		for (int la = 0; la <= lats; ++la)
		{
			const float phi = kPi * la / lats;
			const float ring = std::sin(phi), zb = std::cos(phi);
			for (int lo = 0; lo <= lons; ++lo)
			{
				const float th = kTau * lo / lons;
				float f;
				if (glow)
				{
					// 実機 GetVert と同じ波アニメ
					const float t     = static_cast<float>(la) / static_cast<float>(lats - 1);
					const float phase = kTau * (GravityCoreConst::GlowWaveFreqR * t - spin)
									  + kTau * GravityCoreConst::GlowWaveFreqS * lo / lons;
					f = 1.0f + std::sin(phase) * GravityCoreConst::GlowWaveAmp;
				}
				else
				{
					f = 1.0f + jit(la, lo) * RockConst::JitterStrength * ring;   // 岩のゴツゴツ
				}
				const float px = std::cos(th) * ring * R * f;
				const float py = zb * R * f;
				const float pz = std::sin(th) * ring * R * f;
				const float x1 =  px * cY + pz * sY;
				const float z1 = -px * sY + pz * cY;
				const float y2 =  py * cX - z1 * sX;
				const float z2 =  py * sX + z1 * cX;
				proj[vidx(la, lo)] = { x1, y2, z2 };
			}
		}

		struct Tri { int a, b, c, la, lo; float depth, shade; };
		auto shadeOf = [&](const PV& a, const PV& b, const PV& c) -> float
		{
			const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
			const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
			float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
			const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
			if (len < 1e-5f) { return 0.7f; }
			nx /= len; ny /= len; nz /= len;
			if (nz < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
			const float d = std::clamp(nx * (-0.4f) + ny * 0.5f + nz * 0.77f, 0.0f, 1.0f);
			return 0.35f + 0.65f * d;
		};

		std::vector<Tri> tris; tris.reserve(static_cast<size_t>(lats) * lons * 2);
		for (int la = 0; la < lats; ++la)
		{
			for (int lo = 0; lo < lons; ++lo)
			{
				const int i00 = vidx(la, lo), i01 = vidx(la, lo + 1), i10 = vidx(la + 1, lo), i11 = vidx(la + 1, lo + 1);
				tris.push_back({ i00, i01, i10, la, lo, (proj[i00].z + proj[i01].z + proj[i10].z) / 3.0f, shadeOf(proj[i00], proj[i01], proj[i10]) });
				tris.push_back({ i01, i11, i10, la, lo, (proj[i01].z + proj[i11].z + proj[i10].z) / 3.0f, shadeOf(proj[i01], proj[i11], proj[i10]) });
			}
		}
		std::sort(tris.begin(), tris.end(), [](const Tri& p, const Tri& q) { return p.depth < q.depth; });

		auto fillTri = [&](const Tri& t, const Math::Color& col)
		{
			sprite.DrawTriangle(
				cx + static_cast<int>(proj[t.a].x), cy + static_cast<int>(proj[t.a].y),
				cx + static_cast<int>(proj[t.b].x), cy + static_cast<int>(proj[t.b].y),
				cx + static_cast<int>(proj[t.c].x), cy + static_cast<int>(proj[t.c].y),
				&col, true);
		};

		if (!glow)
		{
			// ── Rock型：緑のエメラルド（敵ドロップの回復石 RockDrop を 2D 再現）──
			//   面色・ワイヤー色は RockDrop の式をそのまま移植（緯度で暗く＋経度で色相ゆらぎ）。
			(void)baseColor;
			auto& smRock = KdShaderManager::Instance();

			// 0) 外周ハロー（加算で柔らかい緑光＝エメラルドの発光）
			smRock.ChangeBlendState(KdBlendState::Add);
			{
				constexpr int LAYERS = 14;
				for (int li = LAYERS - 1; li >= 0; --li)
				{
					const float u   = static_cast<float>(li) / (LAYERS - 1);
					const float rad = R * (1.0f + u * 0.85f);
					const float a   = 0.10f * std::exp(-3.2f * u * u);
					const Math::Color hc(RockConst::ColorR, RockConst::ColorG, RockConst::ColorB, a);
					sprite.DrawCircle(cx, cy, static_cast<int>(rad), &hc, true);
				}
			}
			smRock.UndoBlendState();

			for (const Tri& t : tris)
			{
				const float tt     = static_cast<float>(t.la) / static_cast<float>(std::max(lats - 1, 1));
				const float bright = (1.0f - tt * RockConst::DarkFactor) * t.shade;
				const float phase  = kTau * static_cast<float>(t.lo) / static_cast<float>(lons)
								   + static_cast<float>(t.la) * 0.9f;
				fillTri(t, Math::Color(
					RockConst::ColorR * bright * (0.8f + 0.2f * std::sin(phase)),
					RockConst::ColorG * bright * (0.8f + 0.2f * std::cos(phase * 0.7f)),
					RockConst::ColorB * bright, 1.0f));
			}
			// 本体の発光：手前の面を加算で薄く重ねてエメラルドを光らせる
			smRock.ChangeBlendState(KdBlendState::Add);
			for (const Tri& t : tris)
			{
				if ((proj[t.a].z + proj[t.b].z + proj[t.c].z) < 0.0f) { continue; } // 手前のみ
				const float e = 0.35f * t.shade;
				fillTri(t, Math::Color(RockConst::ColorR * e, RockConst::ColorG * e, RockConst::ColorB * e, 1.0f));
			}
			smRock.UndoBlendState();

			// ワイヤー（手前側の緯線・経線）でエッジを立てる
			auto redge = [&](int ia, int ib, int la)
			{
				if ((proj[ia].z + proj[ib].z) * 0.5f < 0.0f) { return; }
				const float tt = static_cast<float>(la) / static_cast<float>(lats);
				const float b  = (1.0f - tt * RockConst::DarkFactor) * RockConst::WireBoost;
				const Math::Color c(
					std::min(RockConst::ColorR * b + 0.35f * b, 1.0f),
					std::min(RockConst::ColorG * b + 0.20f * b, 1.0f),
					std::min(RockConst::ColorB * b + 0.10f * b, 1.0f),
					RockConst::WireAlpha);
				sprite.DrawLine(
					cx + static_cast<int>(proj[ia].x), cy + static_cast<int>(proj[ia].y),
					cx + static_cast<int>(proj[ib].x), cy + static_cast<int>(proj[ib].y), &c);
			};
			for (int la = 0; la <= lats; ++la)
			{
				for (int lo = 0; lo < lons; ++lo)
				{
					redge(vidx(la, lo), vidx(la, lo + 1), la);
					if (la < lats) { redge(vidx(la, lo), vidx(la + 1, lo), la); }
				}
			}
			return;
		}

		// ── Glow型：上＝明るいシアン白 → 下＝濃い青 の縦カラーグラデ ──
		// 画面の縦位置(gy)で色を補間。additiveだと飽和して均一に見えるのでアルファ合成で描く。
		const Math::Color gTop(
			std::min(GravityCoreConst::GlowFaceR + 0.55f, 1.0f),
			std::min(GravityCoreConst::GlowFaceG + 0.40f, 1.0f),
			std::min(GravityCoreConst::GlowFaceB,         1.0f));
		const Math::Color gBot(
			GravityCoreConst::GlowFaceR * 0.10f,
			GravityCoreConst::GlowFaceG * 0.18f,
			GravityCoreConst::GlowFaceB * 0.45f);
		auto faceCol = [&](const Tri& t) -> Math::Color
		{
			const float gy = ((proj[t.a].y + proj[t.b].y + proj[t.c].y) / 3.0f) / std::max(R, 1.0f); // -1(下)..1(上)
			const float u  = std::clamp((gy + 1.0f) * 0.5f, 0.0f, 1.0f);
			const float m  = 0.75f + 0.25f * t.shade;   // 面の陰影を少し残す
			return Math::Color(
				std::min((gBot.x + (gTop.x - gBot.x) * u) * m, 1.0f),
				std::min((gBot.y + (gTop.y - gBot.y) * u) * m, 1.0f),
				std::min((gBot.z + (gTop.z - gBot.z) * u) * m, 1.0f),
				GravityCoreConst::GlowFaceAlpha);
		};
		auto wireCol = [&](int la) -> Math::Color
		{
			const float t = static_cast<float>(la) / static_cast<float>(lats);
			const float b = 1.0f - t * 0.5f;
			return Math::Color(
				std::min(GravityCoreConst::GlowFaceR * b + 0.5f * b, 1.0f),
				std::min(GravityCoreConst::GlowFaceG * b + 0.25f * b, 1.0f),
				std::min(GravityCoreConst::GlowFaceB * b + 0.10f * b, 1.0f),
				GravityCoreConst::GlowWireAlpha);
		};

		auto& sm = KdShaderManager::Instance();

		// 1) 外周ハロー（加算で柔らかい青光）
		sm.ChangeBlendState(KdBlendState::Add);
		{
			constexpr int LAYERS = 16;
			for (int li = LAYERS - 1; li >= 0; --li)
			{
				const float u = static_cast<float>(li) / (LAYERS - 1);
				const float rad = R * (1.0f + u * 1.05f);
				const float a = 0.12f * std::exp(-3.0f * u * u);
				const Math::Color hc(GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG, GravityCoreConst::GlowFaceB, a);
				sprite.DrawCircle(cx, cy, static_cast<int>(rad), &hc, true);
			}
		}
		sm.UndoBlendState();

		// 2) クリスタルの面（アルファ合成・奥行きソート・縦グラデ）
		for (const Tri& t : tris) { fillTri(t, faceCol(t)); }

		// 3) ワイヤーフレーム（緯線・経線。手前側のみ）
		auto edge = [&](int ia, int ib, int la)
		{
			if ((proj[ia].z + proj[ib].z) * 0.5f < 0.0f) { return; }
			const Math::Color c = wireCol(la);
			sprite.DrawLine(
				cx + static_cast<int>(proj[ia].x), cy + static_cast<int>(proj[ia].y),
				cx + static_cast<int>(proj[ib].x), cy + static_cast<int>(proj[ib].y), &c);
		};
		for (int la = 0; la <= lats; ++la)
		{
			for (int lo = 0; lo < lons; ++lo)
			{
				edge(vidx(la, lo), vidx(la, lo + 1), la);
				if (la < lats) { edge(vidx(la, lo), vidx(la + 1, lo), la); }
			}
		}
	}
}
