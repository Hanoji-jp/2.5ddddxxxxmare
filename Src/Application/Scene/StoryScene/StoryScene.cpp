#include "StoryScene.h"
#include "../SceneManager.h"
#include "../../Const/StoryConst.h"

using namespace StoryConst;

//----------------------------------------------------------
void StoryScene::Init()
{
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

	// フォント
	KdFontManager::Instance().AddFont(FontNo, FontName, FontHeight);

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

	// 入力エッジ検出（押した瞬間だけ反応）
	const bool advNow = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
	                 || ((GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0);
	const bool advEdge = advNow && !m_advancePrev;
	m_advancePrev = advNow;

	const bool skipNow  = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
	const bool skipEdge = skipNow && !m_skipPrev;
	m_skipPrev = skipNow;

	// ESC でストーリーをスキップ → ステージセレクトへ
	if (skipEdge)
	{
		SceneManager::Instance().SetNextScene(SceneManager::SceneType::StageSelect);
		return;
	}

	switch (m_phase)
	{
	case Phase::In:
		m_fade += FadeSpeed * dt;
		if (m_fade >= 1.0f) { m_fade = 1.0f; m_phase = Phase::Hold; }
		break;

	case Phase::Hold:
		if (advEdge) { m_phase = Phase::Out; m_turn = 0.0f; }
		break;

	case Phase::Out:
		// ページをめくる（右からめくれていく）。次ページは下に見えている。
		m_turn += TurnSpeed * dt;
		if (m_turn >= 1.0f)
		{
			if (m_page >= PageCount - 1)
			{
				// 最後まで見たらステージセレクトへ。
				// ※ m_turn は 1.0 のまま維持する（巻き切ってフェードで消えた状態）。
				//   0 に戻すとシーン切替の1フレーム遅延の間に最終ページが全表示で
				//   一瞬戻ってしまうため。
				m_turn = 1.0f;
				SceneManager::Instance().SetNextScene(SceneManager::SceneType::StageSelect);
			}
			else
			{
				// めくり完了：次ページはすでに表示済みなので Hold へ（フェード不要）
				m_turn  = 0.0f;
				++m_page;
				m_phase = Phase::Hold;
			}
		}
		break;
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

	// ページを縦短冊に分割し、右ほど・進むほど曲げて柔らかく巻く
	auto drawCurl = [&](int idx, float t)
	{
		if (idx < 0 || idx >= static_cast<int>(m_pages.size()) || !m_pages[idx]) { return; }
		KdTexture* tex = m_pages[idx].get();
		const int texW = static_cast<int>(tex->GetInfo().Width);
		const int texH = static_cast<int>(tex->GetInfo().Height);

		const float maxBend = DirectX::XMConvertToRadians(MaxBendDeg);

		// 最後のひと巻きはフェードして消す
		float fade = 1.0f;
		if (t > CurlFadeStart) { fade = std::max(0.0f, (1.0f - t) / (1.0f - CurlFadeStart)); }

		float x = leftX;
		for (int i = 0; i < StripCount; ++i)
		{
			const float u0   = static_cast<float>(i)     / StripCount;
			const float u1   = static_cast<float>(i + 1) / StripCount;
			const float uMid = (u0 + u1) * 0.5f;

			// 右へ行くほど・進むほど大きく曲がる（左端=背は固定）
			const float phi  = uMid * t * maxBend;
			const float cosv = std::cosf(phi);

			// 短冊の画面幅（曲がりで前後に縮む＝パース風の柔らかい丸み）
			const float dw = (static_cast<float>(pw) / StripCount) * cosv;

			const float centerX = x + dw * 0.5f;
			x += dw;

			const int widthPx = static_cast<int>(std::ceil(std::abs(dw))) + 1;
			if (widthPx <= 0) { continue; }

			// 丸み表現：正面ほど明るく、立つほど暗い
			const float bright = CurlShadeMin + (1.0f - CurlShadeMin) * std::abs(cosv);
			const Math::Color col(bright, bright, bright, fade);

			// テクスチャの対応する縦帯
			Math::Rectangle src;
			src.x      = static_cast<long>(u0 * texW);
			src.y      = 0;
			src.width  = static_cast<long>((u1 - u0) * texW) + 1;
			src.height = texH;

			sprite.DrawTex(tex, static_cast<int>(std::lround(centerX)), 0,
				widthPx, ph, &src, &col);
		}
	};

	switch (m_phase)
	{
	case Phase::In:
		drawFlat(m_page, m_fade);            // 1ページ目フェードイン
		break;
	case Phase::Hold:
		drawFlat(m_page, 1.0f);
		break;
	case Phase::Out:
		drawFlat(m_page + 1, 1.0f);          // 次ページを下に表示
		drawCurl(m_page, m_turn);            // 現ページを巻く
		break;
	}

	// 「PRESS ENTER」（ページ全表示中のみ点滅）
	if (m_phase == Phase::Hold)
	{
		const float blink = 0.3f + 0.7f * (0.5f + 0.5f * std::sinf(m_timer * PromptBlinkSpeed));
		const Math::Color col(1.0f, 1.0f, 1.0f, blink);

		auto measure = KdFontManager::Instance().CreateFontTexture(FontNo, PromptText, false);
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
		const Math::Vector2 pos(-textW * 0.5f, -sh * PromptYRatio - textH * 0.5f);
		sprite.DrawFont(pos, &col, "%s", PromptText);
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
		sprite.DrawFont(pos, &col, "%s", SkipText);
	}
}
