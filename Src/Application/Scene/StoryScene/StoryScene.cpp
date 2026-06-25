#include "StoryScene.h"
#include "../SceneManager.h"
#include "../../Manager/StageManager.h"
#include "../../Const/StoryConst.h"
#include "../../Const/SoundConst.h"
#include "../../Manager/SoundManager.h"
#include "../../Manager/CursorManager.h"
#include "../../Util/TextFx.h"
#include <algorithm>

using namespace StoryConst;

//----------------------------------------------------------
void StoryScene::Init()
{
	// BGM（ストーリー。ファイル未配置なら無音）
	SoundManager::Instance().PlayBGM(SoundConst::BgmStory, SoundConst::BgmVolume);

	// 2D 専用シーンだが、Effekseer 等の処理に備えてダミーカメラを用意
	auto cam = std::make_shared<KdCamera>();
	cam->SetProjectionMatrix(60.0f);
	m_camera = cam;

	// ページ画像を読み込み（1始まり）
	m_pages.clear();
	for (int i = 1; i <= PageCount; ++i)
	{
		char path[256];
		std::snprintf(path, sizeof(path), PagePathFmt, i);
		auto tex = std::make_shared<KdTexture>();
		tex->Load(path);
		m_pages.push_back(tex);
	}

	// フォント（通常＋「マウスでめくる」用の大きめ）
	KdFontManager::Instance().AddFont(FontNo, FontName, FontHeight);
	KdFontManager::Instance().AddFont(BigFontNo, FontName, BigFontHeight);

	m_page  = 0;
	m_phase = Phase::In;
	m_fade  = 0.0f;
	m_turn  = 0.0f;
}

//----------------------------------------------------------
void StoryScene::Event()
{
	const float dt = KdFPSController::GetDt();
	m_timer += dt;

	// 入力エッジ検出（押した瞬間だけ反応）。シーン切替直後は持ち越し/連打を無視。
	const bool locked = SceneManager::Instance().IsInputLocked();
	const bool advNow = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
	                 || ((GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0);
	const bool advEdge = advNow && !m_advancePrev && !locked;
	m_advancePrev = advNow;

	const bool skipNow  = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	const bool skipEdge = skipNow && !m_skipPrev && !locked;
	m_skipPrev = skipNow;

	// TAB でストーリーをスキップ → 初回フラグを記録して Stage1 へ直行
	if (skipEdge)
	{
		SoundManager::Instance().PlaySE(SeId::StorySkip, SoundConst::SeVolume);
		StageManager::Instance().MarkLaunched();
		StageManager::Instance().SetStageIndex(1);
		SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
		return;
	}

	switch (m_phase)
	{
	case Phase::In:
		m_fade += FadeSpeed * dt;
		if (m_fade >= 1.0f) { m_fade = 1.0f; m_phase = Phase::Hold; }
		break;

	case Phase::Hold:
	{
		// マウスで角を掴んでドラッグ → その角からめくれる（コーナーピール）
		int pw, ph; CalcPageSize(pw, ph);
		const float pwHalf = pw * 0.5f;
		const float phHalf = ph * 0.5f;
		const float diag   = std::sqrt(static_cast<float>(pw) * pw + static_cast<float>(ph) * ph);

		// 角の位置（u:0=左/1=右, v:0=上/1=下）。画面は +Y が上。
		auto cornerPos = [&](int u, int v) {
			return Math::Vector2(u ? pwHalf : -pwHalf, v ? -phHalf : phHalf);
		};

		auto& cur = CursorManager::Instance();
		const Math::Vector2 cpos(cur.PosX(), cur.PosY());

		if (!m_dragging)
		{
			// ページ上で左クリックした瞬間に「最寄りの角」を掴む
			if (cur.IsActive() && cur.Clicked() && cur.HitRect(0.0f, 0.0f, pwHalf, phHalf))
			{
				m_dragging = true;
				m_cornerU  = (cpos.x > 0.0f) ? 1 : 0;
				m_cornerV  = (cpos.y > 0.0f) ? 0 : 1;   // 上半分→上の角(v=0)
				m_fold     = cpos - cornerPos(m_cornerU, m_cornerV);
			}
		}

		if (m_dragging)
		{
			// 掴んだ角がカーソルに追従（角からの変位＝折り返し量）
			m_fold = cpos - cornerPos(m_cornerU, m_cornerV);
			m_turn = std::clamp(m_fold.Length() / diag, 0.0f, 1.0f);

			if (!cur.ClickHeld())   // 離した
			{
				m_dragging = false;
				if (m_turn >= DragCommit)
				{
					m_phase = Phase::Out;   // 現在の角・変位から最後までめくり切る
					SoundManager::Instance().PlaySE(SeId::StoryAdvance, SoundConst::SeVolume);
				}
			}
		}
		else
		{
			// 掴んでいない時：折り返しを0へなめらかに戻す（めくるのはマウスのみ）
			m_fold -= m_fold * std::min(1.0f, DragReturnSpeed * dt);
			m_turn  = std::clamp(m_fold.Length() / diag, 0.0f, 1.0f);
		}
		break;
	}

	case Phase::Out:
	{
		// 掴んだ角を「対角の角」へ向けて動かし、めくり切る
		int pw, ph; CalcPageSize(pw, ph);
		const float pwHalf = pw * 0.5f;
		const float phHalf = ph * 0.5f;
		const float diag   = std::sqrt(static_cast<float>(pw) * pw + static_cast<float>(ph) * ph);
		auto cornerPos = [&](int u, int v) {
			return Math::Vector2(u ? pwHalf : -pwHalf, v ? -phHalf : phHalf);
		};
		const Math::Vector2 C0       = cornerPos(m_cornerU, m_cornerV);
		const Math::Vector2 Copp     = cornerPos(1 - m_cornerU, 1 - m_cornerV);
		const Math::Vector2 foldFull = Copp - C0;

		const float step = TurnSpeed * dt * diag;
		Math::Vector2 toGo = foldFull - m_fold;
		const float remain = toGo.Length();
		bool done = false;
		if (remain <= step) { m_fold = foldFull; done = true; }
		else                { m_fold += toGo * (step / remain); }
		m_turn = std::clamp(m_fold.Length() / diag, 0.0f, 1.0f);

		if (done)
		{
			if (m_page >= PageCount - 1)
			{
				m_turn = 1.0f;
				StageManager::Instance().MarkLaunched();
				StageManager::Instance().SetStageIndex(1);
				SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
			}
			else
			{
				m_turn = 0.0f;
				m_fold = { 0.0f, 0.0f };
				++m_page;
				m_phase = Phase::Hold;
			}
		}
		break;
	}
	}
}

//----------------------------------------------------------
// 16:9 を維持して画面に収まる表示サイズ（contain＝レターボックス）
void StoryScene::CalcPageSize(int& outW, int& outH) const
{
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);
	const float aspect = static_cast<float>(sw) / static_cast<float>(sh);

	if (aspect > PageAspect)
	{
		outH = sh;                                       // 高さ基準（左右に黒帯）
		outW = static_cast<int>(sh * PageAspect);
	}
	else
	{
		outW = sw;                                       // 幅基準（上下に黒帯）
		outH = static_cast<int>(sw / PageAspect);
	}
}

//----------------------------------------------------------
void StoryScene::DrawSpriteExtra()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	// 背景は黒（クリア色・レターボックスを覆う）
	{
		const Math::Color black(0.0f, 0.0f, 0.0f, 1.0f);
		sprite.DrawBox(0, 0, sw, sh, &black, true);
	}

	int pw, ph;
	CalcPageSize(pw, ph);
	const float leftX = -pw * 0.5f;   // ページ左端（中心原点）

	// ページ全体を1枚で平置き（高画質）
	auto drawFlat = [&](int idx, float alpha)
	{
		if (idx < 0 || idx >= static_cast<int>(m_pages.size()) || !m_pages[idx]) { return; }
		const Math::Color col(1.0f, 1.0f, 1.0f, alpha);
		sprite.DrawTex(m_pages[idx].get(), 0, 0, pw, ph, nullptr, &col);
	};

	// 掴んだ角を「シリンダー状に丸めて」めくる（厚紙の硬いカール）。
	// めくり方向(dir)に沿って、折り目から先を半径Rの円筒に巻き付けるように頂点を変形。
	// 平らに折り返さない＝濡れた紙ではなく硬い紙の丸みになる。
	auto drawPeel = [&](int idx)
	{
		if (idx < 0 || idx >= static_cast<int>(m_pages.size()) || !m_pages[idx]) { return; }
		KdTexture* tex = m_pages[idx].get();

		const float pwf = static_cast<float>(pw);
		const float phf = static_cast<float>(ph);
		auto cornerPos = [&](int u, int v) {
			return Math::Vector2(u ? pwf * 0.5f : -pwf * 0.5f, v ? -phf * 0.5f : phf * 0.5f);
		};

		// 最後のひと巻きはフェードして消す
		float fade = 1.0f;
		if (m_turn > CurlFadeStart) { fade = std::max(0.0f, (1.0f - m_turn) / (1.0f - CurlFadeStart)); }

		const Math::Vector2 C0 = cornerPos(m_cornerU, m_cornerV);    // 掴んだ角
		const Math::Vector2 d  = m_fold;                             // 角の変位（めくり方向・量）
		const float L = d.Length();
		if (L < 1.0f) { drawFlat(idx, fade); return; }               // ほぼ平ら

		const Math::Vector2 dir   = d / L;
		const float         sFold = L;                               // 折り目の位置（角からの距離）
		const float         R     = std::max(20.0f, pwf * PeelRadiusRatio);  // カール半径（大=厚紙）

		// 各頂点：折り目より角側(s<sFold)を円筒に巻く。facing=cosθ で陰影。
		const int NX = PeelGridX, NY = PeelGridY;
		auto vtxAt = [&](int ix, int iy, KdSpriteShader::Vertex& out, float& facing, bool& flap)
		{
			const float u = static_cast<float>(ix) / NX;
			const float v = static_cast<float>(iy) / NY;
			const Math::Vector2 P(-pwf * 0.5f + u * pwf, phf * 0.5f - v * phf);
			const float s = (P - C0).Dot(dir);                       // 角=0、内側ほど大
			out.UV = { u, v };
			if (s >= sFold) { out.Pos = { P.x, P.y, 0.0f }; facing = 1.0f; flap = false; return; }

			const float a       = sFold - s;                         // 折り目から角側への距離
			const float halfArc = 3.14159265f * R;                   // 半円筒（180°）ぶんの弧長
			float along;                                             // dir方向の新しい位置
			if (a <= halfArc)
			{
				// 半円筒に巻き付く部分（ここが丸み）。θ:0→π
				const float th = a / R;
				along  = sFold - R * std::sinf(th);
				facing = std::cosf(th);                              // 正面1→側面0→裏-1
			}
			else
			{
				// 巻き切った先は「裏返って平らに寝た」部分（これ以上巻かない＝破綻しない）
				along  = sFold - (a - halfArc);
				facing = -1.0f;                                      // 裏面＝最暗
			}
			const Math::Vector2 Pd = P + dir * (along - s);
			out.Pos = { Pd.x, Pd.y, 0.0f };
			flap    = true;
		};

		// 陰影は三角形ごとに明るさをバケツ分けして近似（頂点色を持てないため複数ドロー）
		constexpr int BUCKETS = 16;
		std::vector<KdSpriteShader::Vertex> flat;
		std::vector<KdSpriteShader::Vertex> bucket[BUCKETS];
		flat.reserve(static_cast<size_t>(NX) * NY * 6);

		auto pushTri = [&](const KdSpriteShader::Vertex& A, const KdSpriteShader::Vertex& B,
			const KdSpriteShader::Vertex& C, float fA, float fB, float fC, bool isFlap)
		{
			if (!isFlap) { flat.push_back(A); flat.push_back(B); flat.push_back(C); return; }
			const float favg = (fA + fB + fC) / 3.0f;
			const float bri  = CurlShadeMin + (1.0f - CurlShadeMin) * std::clamp(favg, 0.0f, 1.0f);
			int k = static_cast<int>(std::lround(bri * (BUCKETS - 1)));
			k = std::clamp(k, 0, BUCKETS - 1);
			bucket[k].push_back(A); bucket[k].push_back(B); bucket[k].push_back(C);
		};

		for (int iy = 0; iy < NY; ++iy)
		{
			for (int ix = 0; ix < NX; ++ix)
			{
				KdSpriteShader::Vertex v00, v10, v01, v11;
				float f00, f10, f01, f11; bool p00, p10, p01, p11;
				vtxAt(ix,     iy,     v00, f00, p00);
				vtxAt(ix + 1, iy,     v10, f10, p10);
				vtxAt(ix,     iy + 1, v01, f01, p01);
				vtxAt(ix + 1, iy + 1, v11, f11, p11);
				pushTri(v00, v10, v01, f00, f10, f01, (p00 || p10 || p01));
				pushTri(v10, v11, v01, f10, f11, f01, (p10 || p11 || p01));
			}
		}

		const Math::Color colFlat(1.0f, 1.0f, 1.0f, fade);
		sprite.DrawTexVertices(tex, flat, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &colFlat);
		for (int k = 0; k < BUCKETS; ++k)
		{
			if (bucket[k].empty()) { continue; }
			const float bri = static_cast<float>(k) / (BUCKETS - 1);
			const Math::Color col(bri, bri, bri, fade);
			sprite.DrawTexVertices(tex, bucket[k], D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &col);
		}
	};

	switch (m_phase)
	{
	case Phase::In:
		drawFlat(m_page, m_fade);            // 1ページ目フェードイン
		break;
	case Phase::Hold:
		if (m_turn > 0.001f)                 // ドラッグ中：めくりかけを表示
		{
			drawFlat(m_page + 1, 1.0f);      // 次ページを下に
			drawPeel(m_page);                // 現ページの角を折り返す
		}
		else
		{
			drawFlat(m_page, 1.0f);
		}
		break;
	case Phase::Out:
		drawFlat(m_page + 1, 1.0f);          // 次ページを下に表示
		drawPeel(m_page);                    // 現ページをめくり切る
		break;
	}

	// 最初のページだけ：「マウスでめくる」を薄く大きく画面中央に表示（操作中は隠す）
	if (m_page == 0 && m_phase == Phase::Hold && m_turn <= 0.001f && !m_dragging)
	{
		auto fs = KdFontManager::Instance().CreateFontTexture(BigFontNo, HintText, false);
		if (fs)
		{
			float textW = 0.0f, textH = 0.0f;
			for (const auto& d : fs->GetTexList())
			{
				if (!d || !d->FontTex) { continue; }
				textW += static_cast<float>(d->FontTex->GetInfo().Width);
				textH  = std::max(textH, static_cast<float>(d->FontTex->GetInfo().Height));
			}
			const Math::Vector2 pos(-textW * 0.5f, textH * 0.5f);
			const Math::Color col(1.0f, 1.0f, 1.0f, HintAlpha);
			sprite.DrawFont(fs, pos, &col, 0);
		}
	}

	// 「ESC : SKIP」（常時、控えめに表示）
	{
		const Math::Color col(1.0f, 1.0f, 1.0f, 0.5f);
		auto measure = KdFontManager::Instance().CreateFontTexture(FontNo, SkipText, false);
		float textW = 0.0f, textH = 0.0f;
		if (measure)
		{
			for (const auto& d : measure->GetTexList())
			{
				if (!d || !d->FontTex) { continue; }
				textW += static_cast<float>(d->FontTex->GetInfo().Width);
				textH  = std::max(textH, static_cast<float>(d->FontTex->GetInfo().Height));
			}
		}
		const Math::Vector2 pos(sw * 0.5f - textW - 16.0f, -sh * SkipYRatio - textH * 0.5f);
		TextFx::DrawShadowed(sprite, pos, col, SkipText);
	}
}
