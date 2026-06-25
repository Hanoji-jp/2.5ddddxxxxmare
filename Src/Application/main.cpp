#include "main.h"

#include "Scene/SceneManager.h"
#include "Manager/SoundManager.h"
#include "Manager/SettingsManager.h"
#include "Manager/CursorManager.h"
#include "Const/SettingsConst.h"
#include "Framework/Utility/ThreadPool/KdThreadPool.h"
#include "Const/FontConst.h"
#include <chrono>
#include <cmath>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エントリーポイント
// アプリケーションはこの関数から進行する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_  HINSTANCE, _In_ LPSTR , _In_ int)
{
	// メモリリークを知らせる
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// COM初期化
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		CoUninitialize();

		return 0;
	}

	// mbstowcs_s関数で日本語対応にするために呼ぶ
	setlocale(LC_ALL, "japanese");

	//===================================================================
	// 実行]
	//===================================================================
	Application::Instance().Execute();

	// COM解放
	CoUninitialize();

	return 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginUpdate()
{
	// 入力状況の更新
	KdInputManager::Instance().Update();

	// 空間環境の更新
	KdShaderManager::Instance().WorkAmbientController().Update();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostUpdate()
{
	// 3DSoundListnerの行列を更新
	KdAudioManager::Instance().SetListnerMatrix(KdShaderManager::Instance().GetCameraCB().mView.Invert());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreUpdate()
{
	SceneManager::Instance().PreUpdate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Update()
{
	// 自前マウスカーソルを先に更新（位置＝解像度非依存／クリックをシーンが同フレームで使える）
	CursorManager::Instance().Update();

	SceneManager::Instance().Update();

	// BGM のフェード／こもり（ローパス）の補間を進める
	SoundManager::Instance().Update(KdFPSController::GetDt());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション更新の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostUpdate()
{
	SceneManager::Instance().PostUpdate();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画開始
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdBeginDraw(bool usePostProcess)
{
	KdDirect3D::Instance().ClearBackBuffer();

	KdShaderManager::Instance().WorkAmbientController().Draw();

	if (!usePostProcess) return;
	KdShaderManager::Instance().m_postProcessShader.Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画終了
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::KdPostDraw()
{
	// Imguiのレンダリング
	KdDebugGUI::Instance().GuiProcess();

	// BackBuffer -> 画面表示
	KdDirect3D::Instance().WorkSwapChain()->Present(0, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の前処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PreDraw()
{
	SceneManager::Instance().PreDraw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Draw()
{
	SceneManager::Instance().Draw();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション描画の後処理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::PostDraw()
{
	// 画面のぼかしや被写界深度処理の実施
	KdShaderManager::Instance().m_postProcessShader.PostEffectProcess();

	// 現在のシーンのデバッグ描画
	SceneManager::Instance().DrawDebug();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dスプライトの描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::DrawSprite()
{
	// エディタ画面中は2DのゲームUIをシーンRTへ描く（Gameウィンドウにも映るように）。
	// 通常時はこれまで通りバックバッファへ。
	const bool editor = KdDebugGUI::Instance().IsGameViewport();
	auto* ctx = KdDirect3D::Instance().WorkDevContext();
	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	bool redirected = false;
	if (editor)
	{
		const auto& sceneRT = KdShaderManager::Instance().m_postProcessShader.GetSceneRT();
		if (sceneRT && sceneRT->WorkRTView())
		{
			ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);
			ID3D11RenderTargetView* rtv = sceneRT->WorkRTView();
			ctx->OMSetRenderTargets(1, &rtv, nullptr);
			redirected = true;
		}
	}

	// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
	// 2Dの描画はこの間で行う
	KdShaderManager::Instance().m_spriteShader.Begin();
	{
		SceneManager::Instance().DrawSprite();

		// FPSカウンター（設定でオンのとき。左上に表示）
		if (SettingsManager::Instance().m_showFps)
		{
			const auto& bbTex = KdDirect3D::Instance().GetBackBuffer();
			const float fsw = static_cast<float>(bbTex->GetInfo().Width);
			const float fsh = static_cast<float>(bbTex->GetInfo().Height);
			char buf[32];
			sprintf_s(buf, "FPS %d", GetNowFPS());
			auto ff = KdFontManager::Instance().CreateFontTexture(SettingsConst::FpsCounterFontNo, buf, false);
			if (ff)
			{
				const Math::Vector2 pos(-fsw * 0.5f + 16.0f,
					fsh * 0.5f - 16.0f - static_cast<float>(SettingsConst::FpsCounterFontH));
				auto& sp = KdShaderManager::Instance().m_spriteShader;
				const Math::Color shc(0.0f, 0.0f, 0.0f, 0.7f);
				const Math::Color col(0.55f, 1.0f, 0.6f, 1.0f);
				sp.DrawFont(ff, Math::Vector2(pos.x + 1.0f, pos.y - 1.0f), &shc, 0);
				sp.DrawFont(ff, pos, &col, 0);
			}
		}

		// 自前マウスカーソルは最前面（エディタ画面へリダイレクト中は出さない）
		if (!editor) { CursorManager::Instance().Draw(); }
	}
	KdShaderManager::Instance().m_spriteShader.End();

	if (redirected)
	{
		ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
		if (prevRTV) { prevRTV->Release(); }
		if (prevDSV) { prevDSV->Release(); }
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 起動時ロード画面（進捗0..1）を1フレーム描いて即Present
// ※ Init中（ゲームループ前）に呼ばれるので、自前でClear→描画→Presentまで完結させる。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::DrawLoadingScreen(float progress)
{
	progress = std::clamp(progress, 0.0f, 1.0f);

	// 表示進捗をなめらかに追従させる（バーが一気に飛ばないように）。完了時は確実に満タン。
	static float s_shown = 0.0f;
	if (progress >= 1.0f) { s_shown = 1.0f; }
	else { s_shown += (progress - s_shown) * 0.30f; }

	// グロー素材は初回だけロード（CursorManager と同じ Glow.png）
	static std::shared_ptr<KdTexture> s_glow;
	static bool s_glowInit = false;
	if (!s_glowInit)
	{
		s_glowInit = true;
		s_glow = std::make_shared<KdTexture>();
		if (!s_glow->Load("Asset/Texture/Glow.png")) { s_glow = nullptr; }
	}

	// アニメ用の実時間（プリロードはブロッキングなので実時間で回す＝FPS非依存）
	static const auto s_start = std::chrono::steady_clock::now();
	const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - s_start).count();

	KdDirect3D::Instance().ClearBackBuffer();

	const auto& bbTex = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bbTex->GetInfo().Width);
	const float sh = static_cast<float>(bbTex->GetInfo().Height);
	const int   isw = static_cast<int>(sw);
	const int   ish = static_cast<int>(sh);

	auto& sm = KdShaderManager::Instance();
	auto& sp = sm.m_spriteShader;
	sp.Begin();
	{
		// DrawBox は (中心x, 中心y, ハーフ幅, ハーフ高)。画面中央が原点。

		// 文字を中央寄せで描く（DrawFontは左上アンカーなので幅/高さぶんずらす）。影付き。
		auto drawCenter = [&](int slot, const char* text, float cx, float cy, const Math::Color& col)
		{
			auto fs = KdFontManager::Instance().CreateFontTexture(slot, text, false);
			if (!fs) { return; }
			float w = 0.0f, hgt = 0.0f;
			for (const auto& d : fs->GetTexList())
			{
				if (d && d->FontTex)
				{
					w += static_cast<float>(d->FontTex->GetInfo().Width);
					hgt = std::max(hgt, static_cast<float>(d->FontTex->GetInfo().Height));
				}
			}
			const Math::Vector2 pos(cx - w * 0.5f, cy + hgt * 0.5f);
			const Math::Color shd(0.0f, 0.0f, 0.0f, col.A() * 0.6f);
			sp.DrawFont(fs, Math::Vector2(pos.x + 2.0f, pos.y - 2.0f), &shd, 0);
			sp.DrawFont(fs, pos, &col, 0);
		};

		// ── 背景：濃紺の縦グラデ（横帯を積んで近似）──
		const int bands = 24;
		for (int i = 0; i < bands; ++i)
		{
			const float f = static_cast<float>(i) / static_cast<float>(bands - 1); // 上0→下1
			const Math::Color top(0.03f, 0.05f, 0.10f, 1.0f);
			const Math::Color bot(0.06f, 0.09f, 0.16f, 1.0f);
			const Math::Color c(top.R() + (bot.R() - top.R()) * f,
				top.G() + (bot.G() - top.G()) * f,
				top.B() + (bot.B() - top.B()) * f, 1.0f);
			const int bandH = ish / bands + 2;
			const int cy = static_cast<int>(sh * 0.5f) - static_cast<int>((i + 0.5f) * sh / bands);
			sp.DrawBox(0, cy, isw / 2 + 2, bandH / 2 + 1, &c);
		}

		// コアリアの顔アイコン（リザルト窓と同じ LifeIco.png）。初回だけロード。
		static std::shared_ptr<KdTexture> s_face;
		static bool s_faceInit = false;
		if (!s_faceInit)
		{
			s_faceInit = true;
			s_face = std::make_shared<KdTexture>();
			if (!s_face->Load("Asset/Texture/LifeIco.png")) { s_face = nullptr; }
		}

		// ── タイトル "Corelia" ──
		drawCenter(SettingsConst::LoadingTitleFontNo, "Corelia", 0.0f, sh * 0.30f,
			Math::Color(0.95f, 0.92f, 0.75f, 1.0f));   // ゲームUIの淡い金

		// ── 「コアリアがドットの道を歩いてゴールへ向かう」ステージセレクト風の進捗 ──
		const float roadY    = -sh * 0.06f;          // 道の高さ
		const float roadLeft = -sw * 0.34f;
		const float roadRight=  sw * 0.34f;
		const float roadLen  = roadRight - roadLeft;
		const float coriaX   = roadLeft + roadLen * s_shown;

		// 道のドット（進捗より手前＝点灯、奥＝くすむ。ステージ解放の道と同じ表現）
		const int   dotN = 16;
		const Math::Color goldEdge(0.95f, 0.82f, 0.45f, 1.0f);
		for (int i = 0; i < dotN; ++i)
		{
			const float dt2 = static_cast<float>(i) / static_cast<float>(dotN - 1);
			const float dx = roadLeft + roadLen * dt2;
			const bool  lit = (dx <= coriaX + 4.0f);
			if (s_glow)
			{
				sm.ChangeBlendState(KdBlendState::Add);
				const float pulse = lit ? (0.8f + 0.2f * std::sin(t * 6.0f - i * 0.6f)) : 0.25f;
				const float sz = lit ? 18.0f : 12.0f;
				const Math::Color dc = lit ? Math::Color(0.55f, 0.9f, 1.0f, pulse)
					: Math::Color(0.35f, 0.45f, 0.6f, 0.35f);
				sp.DrawTex(s_glow.get(), static_cast<int>(dx), static_cast<int>(roadY),
					static_cast<int>(sz), static_cast<int>(sz), nullptr, &dc);
				sm.UndoBlendState();
			}
			else
			{
				const Math::Color dc = lit ? Math::Color(0.55f, 0.9f, 1.0f, 1.0f)
					: Math::Color(0.3f, 0.36f, 0.46f, 1.0f);
				sp.DrawCircle(static_cast<int>(dx), static_cast<int>(roadY), 5, &dc, true);
			}
		}

		// ── ゴール：金縁の角丸ノード（ステージノード風）＋星グロー ──
		{
			const float gx = roadRight + 26.0f;
			const float gnHalf = 22.0f;
			const float reached = (s_shown > 0.999f) ? 1.0f : 0.0f;
			const float gpulse = 0.6f + 0.4f * std::sin(t * 4.0f);
			if (s_glow)
			{
				sm.ChangeBlendState(KdBlendState::Add);
				const Math::Color gc(1.0f, 0.85f, 0.4f, (0.4f + 0.4f * reached) * gpulse);
				sp.DrawTex(s_glow.get(), static_cast<int>(gx), static_cast<int>(roadY),
					static_cast<int>(70.0f + 20.0f * reached), static_cast<int>(70.0f + 20.0f * reached), nullptr, &gc);
				sm.UndoBlendState();
			}
			const Math::Color gnEdge(0.95f, 0.82f, 0.45f, 1.0f);
			const Math::Color gnBody(0.16f, 0.18f, 0.10f, 1.0f);
			sp.DrawRoundedBox(static_cast<int>(gx), static_cast<int>(roadY),
				static_cast<int>(gnHalf), static_cast<int>(gnHalf), 8.0f, &gnEdge, 6);
			sp.DrawRoundedBox(static_cast<int>(gx), static_cast<int>(roadY),
				static_cast<int>(gnHalf - 4.0f), static_cast<int>(gnHalf - 4.0f), 6.0f, &gnBody, 6);
		}

		// ── コアリア本体（白い円盤＋顔。ぴょこぴょこ跳ねる）──
		{
			const float hop = std::abs(std::sin(t * 6.0f)) * 14.0f;   // 跳ね
			const float cx = coriaX;
			const float cy = roadY + 8.0f + hop;
			const float face = 70.0f;

			// 影（接地点。跳ねるほど小さく薄く）
			const float shScale = 1.0f - hop / 28.0f;
			const Math::Color shc(0.0f, 0.0f, 0.0f, 0.35f * shScale);
			sp.DrawRoundedBox(static_cast<int>(cx), static_cast<int>(roadY - 18.0f),
				static_cast<int>(26.0f * shScale), static_cast<int>(7.0f * shScale), 7.0f, &shc, 6);

			// 後光
			if (s_glow)
			{
				sm.ChangeBlendState(KdBlendState::Add);
				const Math::Color hc(0.5f, 0.85f, 1.0f, 0.5f);
				sp.DrawTex(s_glow.get(), static_cast<int>(cx), static_cast<int>(cy),
					static_cast<int>(face * 1.8f), static_cast<int>(face * 1.8f), nullptr, &hc);
				sm.UndoBlendState();
			}
			// 白い円盤
			const Math::Color disc(1.0f, 1.0f, 1.0f, 1.0f);
			sp.DrawCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(face * 0.52f), &disc, true);
			// 金の縁取り
			sp.DrawCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(face * 0.56f), &goldEdge, false);
			// 顔
			if (s_face)
			{
				const Math::Color fc(1.0f, 1.0f, 1.0f, 1.0f);
				sp.DrawTex(s_face.get(), static_cast<int>(cx), static_cast<int>(cy),
					static_cast<int>(face), static_cast<int>(face), nullptr, &fc);
			}
		}

		// ── 「よみこみちゅう」＋パーセント（道の下）──
		const int   nDots = 1 + (static_cast<int>(t * 2.0f) % 3);   // . .. ... が循環
		char msg[40] = "よみこみちゅう";
		for (int i = 0; i < nDots; ++i) { strcat_s(msg, "."); }
		drawCenter(SettingsConst::FpsCounterFontNo, msg, 0.0f, -sh * 0.30f,
			Math::Color(0.85f, 0.92f, 1.0f, 1.0f));

		char pct[16];
		sprintf_s(pct, "%d%%", static_cast<int>(s_shown * 100.0f + 0.5f));
		drawCenter(SettingsConst::FpsCounterFontNo, pct, 0.0f, -sh * 0.38f,
			Math::Color(0.95f, 0.82f, 0.45f, 1.0f));
	}
	sp.End();

	// 画面へ提示
	KdDirect3D::Instance().WorkSwapChain()->Present(0, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション初期設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool Application::Init(int w, int h)
{
	//===================================================================
	// ウィンドウ作成
	//===================================================================
	if (m_window.Create(w, h, "Corelia", "Window") == false) {
		MessageBoxA(nullptr, "ウィンドウ作成に失敗", "エラー", MB_OK);
		return false;
	}

	//===================================================================
	// フルスクリーン確認
	//===================================================================
	bool bFullScreen = false;
//	if (MessageBoxA(m_window.GetWndHandle(), "フルスクリーンにしますか？", "確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
//		bFullScreen = true;
//	}

	//===================================================================
	// Direct3D初期化
	//===================================================================

	// デバイスのデバッグモードを有効にする
	bool deviceDebugMode = false;
#ifdef _DEBUG
	deviceDebugMode = true;
#endif

	// Direct3D初期化
	std::string errorMsg;
	if (KdDirect3D::Instance().Init(m_window.GetWndHandle(), w, h, deviceDebugMode, errorMsg) == false) {
		MessageBoxA(m_window.GetWndHandle(), errorMsg.c_str(), "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
		return false;
	}

	// フルスクリーン設定
	if (bFullScreen) {
		HRESULT hr;

		hr = KdDirect3D::Instance().SetFullscreenState(TRUE, 0);
		if (FAILED(hr))
		{
			MessageBoxA(m_window.GetWndHandle(), "フルスクリーン設定失敗", "Direct3D初期化失敗", MB_OK | MB_ICONSTOP);
			return false;
		}
	}

	//===================================================================
	// imgui初期化
	//===================================================================
	KdDebugGUI::Instance().GuiInit(w, h);

	//===================================================================
	// シェーダー初期化
	//===================================================================
	KdShaderManager::Instance().Init();

	//===================================================================
	// Effekseer初期化
	//===================================================================
	KdEffekseerManager::GetInstance().Create(w, h);

	//===================================================================
	// オーディオ初期化
	//===================================================================
	KdAudioManager::Instance().Init();

	//===================================================================
	// フォント初期化
	//===================================================================
	KdFontManager::Instance().Init(GetWindowHandle());
	// 共通ゲームフォント(Kuramubon)を登録（以後 AddFont でファミリ名指定が効く）
	KdFontManager::Instance().AddFontResource(FontConst::GameFontPath);
	// FPSカウンター用フォント（全シーン共通で常駐）
	KdFontManager::Instance().AddFont(SettingsConst::FpsCounterFontNo, FontConst::GameFontName, SettingsConst::FpsCounterFontH);
	// 起動ロード画面のタイトル用フォント（大きめ）
	KdFontManager::Instance().AddFont(SettingsConst::LoadingTitleFontNo, FontConst::GameFontName, SettingsConst::LoadingTitleFontH);
	
	//===================================================================
	// スレッドプール初期化
	//===================================================================
	KdThreadPool::Instance().Init();

	//===================================================================
	// ゲーム固有の初期化
	//===================================================================
	// 例えばカーソルを消したい場合
	//ShowCursor(false);

	// ウィンドウサイズ/全画面/FPSの設定はロード画面より「先に」適用しておく。
	// （ロード画面表示中に解像度が切り替わってチラつくのを防ぐ＝最初から正しいサイズで出す）
	SettingsManager::Instance().ApplyAudio();
	SettingsManager::Instance().ApplyScreen();
	SettingsManager::Instance().ApplyFps();

	// 音声を起動時に全プリロード（BGM通常＋こもり版、SE）。重いのでロード画面を出す。
	// 以後どのシーンでも即再生でき、着地時にロードして合わせる事はしない。
	SoundManager::Instance().LoadSeAssign();
	DrawLoadingScreen(0.0f);
	SoundManager::Instance().PreloadAllWithProgress([this](float p) { DrawLoadingScreen(p); });
	KdDebugGUI::Instance().SetPersistentGuiCallback([] { SoundManager::Instance().DrawSeEditorGui(); });

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// アプリケーション実行
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void Application::Execute()
{
	KdCSVData windowData("Asset/Data/WindowSettings.csv");
	const std::vector<std::string>& sizeData = windowData.GetLine(0);

	// 内部描画（バックバッファ）は設計解像度で固定＝仮想解像度。
	// これでUI/HUD/カーソルは解像度に依らず一定サイズになる（ウィンドウへ引き伸ばし表示）。
	// 解像度設定は「出力ウィンドウのサイズ」だけに効かせる（ApplyScreen 側）。
	const int initW = atoi(sizeData[0].c_str());
	const int initH = atoi(sizeData[1].c_str());
	SettingsManager::Instance().Load();

	//===================================================================
	// 初期設定(ウィンドウ作成、Direct3D初期化など)
	//===================================================================
	if (Application::Instance().Init(initW, initH) == false) {
		return;
	}

	//===================================================================
	// ゲームループ
	//===================================================================

	// 時間
	m_fpsController.Init();

	// ループ
	while (1)
	{
		// 処理開始時間Get
		m_fpsController.UpdateStartTime();

		// ゲーム終了指定があるときはループ終了
		if (m_endFlag)
		{
			break;
		}

		//=========================================
		//
		// ウィンドウ関係の処理
		//
		//=========================================

		// ウィンドウのメッセージを処理する
		m_window.ProcessMessage();

		// ウィンドウが破棄されてるならループ終了
		if (m_window.IsCreated() == false)
		{
			break;
		}

		if (GetAsyncKeyState(VK_ESCAPE))
		{
//			if (MessageBoxA(m_window.GetWndHandle(), "本当にゲームを終了しますか？",
//				"終了確認", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
			{
				End();
			}
		}

		//=========================================
		//
		// アプリケーション更新処理
		//
		//=========================================

		KdBeginUpdate();
		{
			PreUpdate();

			Update();

			PostUpdate();
		}
		KdPostUpdate();

		//=========================================
		//
		// アプリケーション描画処理
		//
		//=========================================

		KdBeginDraw();
		{
			PreDraw();

			Draw();

			PostDraw();

			DrawSprite();
		}
		KdPostDraw();

		//=========================================
		//
		// フレームレート制御
		//
		//=========================================

		m_fpsController.Update();
	}

	//===================================================================
	// アプリケーション解放
	//===================================================================
	Release();
}

// アプリケーション終了
void Application::Release()
{
	KdEffekseerManager::GetInstance().Release();

	KdInputManager::Instance().Release();

	KdShaderManager::Instance().Release();

	KdAudioManager::Instance().Release();

	KdDirect3D::Instance().Release();

	KdThreadPool::Instance().Release();

	// ウィンドウ削除
	m_window.Release();
}
