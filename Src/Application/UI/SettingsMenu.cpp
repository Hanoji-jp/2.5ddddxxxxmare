#include "../main.h"
#include "SettingsMenu.h"
#include "../Manager/SettingsManager.h"
#include "../Manager/SoundManager.h"
#include "../Manager/CursorManager.h"
#include "../Const/SettingsConst.h"
#include "../Const/SoundConst.h"
#include "../Updater/Updater.h"
#include <algorithm>

namespace
{
	// アップデート行で決定したときの動作（現在の状態で分岐）
	void TriggerUpdateAction()
	{
		auto& up = Updater::GetInstance();
		switch (up.GetState())
		{
		case UpdaterState::UpdateAvailable: up.StartDownload();    break;   // 更新あり → DL開始
		case UpdaterState::Downloading:     up.CancelDownload();   break;   // DL中    → 中断
		case UpdaterState::Downloaded:      up.ApplyUpdate();      break;   // DL済み  → 適用＆再起動
		default:                            up.StartCheckVersion();break;   // それ以外 → 確認
		}
	}
}

void SettingsMenu::EnsureFont()
{
	if (m_fontReady) { return; }
	KdFontManager::Instance().AddFont(SettingsConst::FontNo, SettingsConst::FontName, SettingsConst::FontHeight);
	m_fontReady = true;
}

void SettingsMenu::Open()
{
	EnsureFont();
	m_open = true;
	m_row  = 0;
	m_fade = 0.0f;
	// 入力エッジを「押下済み」にして、開いた瞬間の決定/戻るの暴発を防ぐ
	m_prevUp = m_prevDown = m_prevLeft = m_prevRight = true;
	m_prevDecide = m_prevBack = true;
}

void SettingsMenu::Close()
{
	m_open = false;
	SettingsManager::Instance().Save();   // 変更を保存
	SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
}

void SettingsMenu::Update()
{
	if (!m_open) { return; }
	using namespace SettingsConst;

	m_blinkTimer += KdFPSController::GetDt();
	if (m_fade < 1.0f) { m_fade = std::min(1.0f, m_fade + FadeSpeed * KdFPSController::GetDt()); }

	auto& set = SettingsManager::Instance();

	const bool up     = ((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)    & 0x8000) != 0);
	const bool down   = ((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN)  & 0x8000) != 0);
	const bool left   = ((GetAsyncKeyState('A') & 0x8000) != 0) || ((GetAsyncKeyState(VK_LEFT)  & 0x8000) != 0);
	const bool right  = ((GetAsyncKeyState('D') & 0x8000) != 0) || ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0);
	const bool decide = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
	const bool back   = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

	const bool eUp = up && !m_prevUp, eDown = down && !m_prevDown;
	const bool eLeft = left && !m_prevLeft, eRight = right && !m_prevRight;
	const bool eDecide = decide && !m_prevDecide, eBack = back && !m_prevBack;
	m_prevUp = up; m_prevDown = down; m_prevLeft = left; m_prevRight = right;
	m_prevDecide = decide; m_prevBack = back;

	auto& snd = SoundManager::Instance();

	// 行移動
	if (eUp)   { m_row = (m_row + Count - 1) % Count; snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
	if (eDown) { m_row = (m_row + 1) % Count;         snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); }

	// 値変更（A=減 / D=増）
	const int dir = (eRight ? 1 : 0) - (eLeft ? 1 : 0);
	if (dir != 0)
	{
		bool changed = true;
		switch (m_row)
		{
		case Master: set.m_master = std::clamp(set.m_master + dir * VolumeStep, 0.0f, 1.0f); set.ApplyAudio(); break;
		case Bgm:    set.m_bgm    = std::clamp(set.m_bgm    + dir * VolumeStep, 0.0f, 1.0f); set.ApplyAudio(); break;
		case Se:     set.m_se     = std::clamp(set.m_se     + dir * VolumeStep, 0.0f, 1.0f); set.ApplyAudio(); break;
		case Screen: set.m_fullscreen = !set.m_fullscreen; set.ApplyScreen(); break;
		case Resolution: set.m_resIndex = (set.m_resIndex + dir + ResolutionCount) % ResolutionCount; break;
		case Fps:        set.m_fpsIndex = (set.m_fpsIndex + dir + FpsCount) % FpsCount; set.ApplyFps(); break;
		case FpsShow:    set.m_showFps = !set.m_showFps; break;
		default: changed = false; break;
		}
		// 音量はその場でプレビュー（新しい音量でMenuMoveが鳴る）
		if (changed) { snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
	}

	// ── マウス操作（ホバーで選択／クリックで決定・変更／音量バーはドラッグ）──
	{
		auto& cur = CursorManager::Instance();
		if (cur.IsActive())
		{
			// Draw と同じレイアウト計算
			const float extraBottom = 6.0f;
			const float panelCY = -extraBottom * 0.5f;
			const float hw      = PanelW * 0.5f;
			const float panelH  = BannerH + ContentPadTop + Count * RowH + ContentPadBottom + extraBottom;
			const float hh      = panelH * 0.5f;
			const float top     = panelCY + hh;
			const float bannerCY = top - BannerH * 0.5f;
			const float firstRowY = bannerCY - BannerH * 0.5f - ContentPadTop - RowH * 0.5f;
			const float rightX  = hw - SidePad;
			const float rowHalfW = hw - SidePad * 0.5f;

			for (int i = 0; i < Count; ++i)
			{
				const float y = firstRowY - i * RowH;
				if (!cur.HitRect(0.0f, y, rowHalfW, RowH * 0.5f)) { continue; }

				// ホバーで選択
				if (m_row != i) { m_row = i; snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); }

				// 音量バー：押下中はドラッグで値を設定
				if ((i == Master || i == Bgm || i == Se) && cur.ClickHeld())
				{
					const float barCx   = rightX - BarW * 0.5f;
					const float barLeft = barCx - BarW * 0.5f;
					const float v = std::clamp((cur.PosX() - barLeft) / BarW, 0.0f, 1.0f);
					if (i == Master) { set.m_master = v; }
					else if (i == Bgm) { set.m_bgm = v; }
					else { set.m_se = v; }
					set.ApplyAudio();
				}

				// クリックで決定／切替
				if (cur.Clicked())
				{
					switch (i)
					{
					case Screen:     set.m_fullscreen = !set.m_fullscreen; set.ApplyScreen(); snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); break;
					case Resolution: set.m_resIndex = (set.m_resIndex + 1) % ResolutionCount;  snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); break;
					case Fps:        set.m_fpsIndex = (set.m_fpsIndex + 1) % FpsCount; set.ApplyFps(); snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); break;
					case FpsShow:    set.m_showFps = !set.m_showFps; snd.PlaySE(SeId::MenuMove, SoundConst::SeVolume); break;
					case SettingsConst::Update: TriggerUpdateAction(); snd.PlaySE(SeId::MenuDecide, SoundConst::SeVolume); break;
					case Back:       Close(); return;
					default: break;   // 音量はドラッグで処理済み
					}
				}
				break;
			}
		}
	}

	// アップデート行：決定でアクション（状態に応じて 確認／DL／中断／適用）
	if (eDecide && m_row == SettingsConst::Update)
	{
		TriggerUpdateAction();
		snd.PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
	}

	// 決定（もどる）／TABで閉じる
	if ((eDecide && m_row == Back) || eBack) { Close(); return; }
}

void SettingsMenu::Draw()
{
	if (!m_open) { return; }   // 閉じたら描画しない（フェードアウトは無し＝即消え）
	using namespace SettingsConst;

	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);
	const float a = std::clamp(m_fade, 0.0f, 1.0f);

	// 背景ぼかし（シーンRTをぼかして全画面に合成）。RTは初回に生成。
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		if (!m_blurInit && bb)
		{
			m_blurRT.CreateRenderTarget(bb->GetWidth(), bb->GetHeight());
			m_blurInit = (m_blurRT.m_RTTexture != nullptr);
		}
		auto sceneRT = pp.GetSceneRT();
		if (m_blurInit && sceneRT && sceneRT->WorkRTView())
		{
			sprite.End();   // 一旦スプライトバッチを閉じる（ステート確定）
			pp.GenerateBlurTexture(sceneRT, m_blurRT.m_RTTexture, m_blurRT.m_viewPort, SettingsConst::BlurRadius);
			sprite.Begin();
			const Math::Color white(1.0f, 1.0f, 1.0f, a);
			sprite.DrawTex(m_blurRT.m_RTTexture.get(), 0, 0, sw, sh, nullptr, &white);
		}
	}

	// 背景うっすら暗幕
	{
		const Math::Color dim(0.0f, 0.0f, 0.0f, 0.40f * a);
		sprite.DrawBox(0, 0, sw, sh, &dim, true);
	}

	// レイアウト（中心原点・+Yが上）。下方向へ少し拡張する（ExtraBottom 分だけ下へ伸ばす）
	const float extraBottom = 6.0f;                 // 下に増やす量(px)
	const float panelCY = -extraBottom * 0.5f;      // 中心を下へずらして「下だけ」増やす
	const float hw     = PanelW * 0.5f;
	const float panelH = BannerH + ContentPadTop + Count * RowH + ContentPadBottom + extraBottom;
	const float hh     = panelH * 0.5f;
	const float top    = panelCY + hh;

	// パネル（影 → 金縁 → 紺本体、すべて角丸）
	{
		const int ct = EdgeThickness;
		const int cyi = static_cast<int>(panelCY);
		const Math::Color shadow(0.0f, 0.0f, 0.0f, ShadowA * a);
		sprite.DrawRoundedBox(6, cyi - 6, static_cast<int>(hw) + ct, static_cast<int>(hh) + ct, PanelRadius + ct, &shadow, CornerSegs);
		const Math::Color edge(EdgeR, EdgeG, EdgeB, EdgeA * a);
		sprite.DrawRoundedBox(0, cyi, static_cast<int>(hw) + ct, static_cast<int>(hh) + ct, PanelRadius + ct, &edge, CornerSegs);
		const Math::Color body(BodyR, BodyG, BodyB, BodyA * a);
		sprite.DrawRoundedBox(0, cyi, static_cast<int>(hw), static_cast<int>(hh), PanelRadius, &body, CornerSegs);
	}

	// 上部バナー＋タイトル
	const float bannerCY = top - BannerH * 0.5f;
	{
		const Math::Color banner(EdgeR, EdgeG, EdgeB, EdgeA * a);
		sprite.DrawRoundedBox(0, static_cast<int>(bannerCY), static_cast<int>(hw), static_cast<int>(BannerH * 0.5f), PanelRadius, &banner, CornerSegs);
		auto tfs = KdFontManager::Instance().CreateFontTexture(FontNo, TitleText, false);
		if (tfs)
		{
			float tw = 0.0f, th = 0.0f;
			for (const auto& d : tfs->GetTexList())
			{
				if (!d || !d->FontTex) { continue; }
				tw += static_cast<float>(d->FontTex->GetInfo().Width);
				th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
			}
			const Math::Color tcol(BannerTextR, BannerTextG, BannerTextB, a);
			sprite.DrawFont(tfs, Math::Vector2(-tw * 0.5f, bannerCY - th * 0.5f), &tcol, 0);
		}
	}

	// テキスト描画ヘルパ（左寄せ / 右寄せ / 中央）
	auto drawText = [&](const char* text, float x, float y, const Math::Color& col, int align)
	{
		// align: 0=左寄せ(xが左端) / 1=右寄せ(xが右端) / 2=中央(xが中心)
		auto fs = KdFontManager::Instance().CreateFontTexture(FontNo, text, false);
		if (!fs) { return; }
		float tw = 0.0f, th = 0.0f;
		for (const auto& d : fs->GetTexList())
		{
			if (!d || !d->FontTex) { continue; }
			tw += static_cast<float>(d->FontTex->GetInfo().Width);
			th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
		}
		float px = x;
		if      (align == 1) { px = x - tw; }
		else if (align == 2) { px = x - tw * 0.5f; }
		const Math::Vector2 pos(px, y - th * 0.5f);
		const Math::Color shc(0.0f, 0.0f, 0.0f, 0.5f * col.w);
		sprite.DrawFont(fs, Math::Vector2(pos.x + 2.0f, pos.y - 2.0f), &shc, 0);
		sprite.DrawFont(fs, pos, &col, 0);
	};

	const float leftX  = -hw + SidePad;
	const float rightX =  hw - SidePad;
	const float firstRowY = bannerCY - BannerH * 0.5f - ContentPadTop - RowH * 0.5f;
	const float blink = 0.5f + 0.5f * std::sinf(m_blinkTimer * 4.0f);

	auto& set = SettingsManager::Instance();

	const char* labels[Count] = { LabelMaster, LabelBgm, LabelSe, LabelScreen, LabelRes, LabelFps, LabelFpsShow, LabelUpdate, LabelBack };

	for (int i = 0; i < Count; ++i)
	{
		const bool  sel = (i == m_row);
		const float y   = firstRowY - i * RowH;

		// 選択ハイライト
		if (sel)
		{
			const Math::Color hl(EdgeR, EdgeG, EdgeB, (HighlightA + 0.12f * blink) * a);
			sprite.DrawRoundedBox(0, static_cast<int>(y), static_cast<int>(hw - SidePad * 0.5f), static_cast<int>(RowH * 0.5f - 4.0f), PanelRadius * 0.6f, &hl, CornerSegs);
		}

		const Math::Color col = sel
			? Math::Color(TextSelR, TextSelG, TextSelB, a)
			: Math::Color(TextR, TextG, TextB, 0.9f * a);

		if (i == Back)
		{
			drawText(labels[i], 0.0f, y, col, 2);   // 中央
			continue;
		}

		drawText(labels[i], leftX, y, col, 0);      // ラベル左寄せ

		// 値の表示
		switch (i)
		{
		case Master:
		case Bgm:
		case Se:
		{
			const float v = (i == Master) ? set.m_master : (i == Bgm) ? set.m_bgm : set.m_se;
			// バー（右寄せ配置）
			const float barCx = rightX - BarW * 0.5f;
			const Math::Color bg(BarBgR, BarBgG, BarBgB, BarBgA * a);
			sprite.DrawBox(static_cast<int>(barCx), static_cast<int>(y), static_cast<int>(BarW * 0.5f), static_cast<int>(BarH * 0.5f), &bg, true);
			const float fillW = BarW * v;
			const float barLeft = barCx - BarW * 0.5f;
			const Math::Color fl(BarFillR, BarFillG, BarFillB, BarFillA * a);
			if (fillW > 1.0f)
			{
				sprite.DrawBox(static_cast<int>(barLeft + fillW * 0.5f), static_cast<int>(y), static_cast<int>(fillW * 0.5f), static_cast<int>(BarH * 0.5f), &fl, true);
			}
			// パーセント数値（バーの左に右寄せ）
			char buf[16];
			sprintf_s(buf, "%d%%", static_cast<int>(v * 100.0f + 0.5f));
			drawText(buf, barLeft - 14.0f, y, col, 1);
			break;
		}
		case Screen:
			drawText(set.m_fullscreen ? ValueFull : ValueWindow, rightX, y, col, 1);
			break;
		case Resolution:
		{
			const auto& r = Resolutions[set.m_resIndex];
			drawText(r.label, rightX, y, col, 1);
			// 再起動注記（小さく左下寄り）
			const Math::Color note(TextR, TextG, TextB, 0.5f * a);
			drawText(ResNote, leftX + 220.0f, y, note, 0);
			break;
		}
		case Fps:
			drawText(FpsOptions[set.m_fpsIndex].label, rightX, y, col, 1);
			break;
		case FpsShow:
			drawText(set.m_showFps ? ValueOn : ValueOff, rightX, y, col, 1);
			break;
		case SettingsConst::Update:
		{
			// 現在の状態を右寄せで表示（DL中は％）
			char dlbuf[16];
			const char* st = UpdIdle;
			switch (Updater::GetInstance().GetState())
			{
			case UpdaterState::Idle:            st = UpdIdle;       break;
			case UpdaterState::Checking:        st = UpdChecking;   break;
			case UpdaterState::UpToDate:        st = UpdLatest;     break;
			case UpdaterState::UpdateAvailable: st = UpdAvailable;  break;
			case UpdaterState::Downloading:
				sprintf_s(dlbuf, "%d%%", static_cast<int>(Updater::GetInstance().GetProgress() * 100.0f + 0.5f));
				st = dlbuf;
				break;
			case UpdaterState::Downloaded:      st = UpdDownloaded; break;
			case UpdaterState::Error:           st = UpdError;      break;
			}
			drawText(st, rightX, y, col, 1);
			break;
		}
		default: break;
		}
	}

	// 下部ヒント
	{
		const Math::Color hint(TextR, TextG, TextB, 0.7f * a);
		drawText(Hint, 0.0f, panelCY - hh + ContentPadBottom * 0.5f, hint, 2);
	}
}
