#pragma once
#include "PauseMenuConst.h"

//==========================================================
// GameOverConst.h
// 死亡回数が上限に達したときのゲームオーバー画面用の定数。
// レイアウト・フォントはポーズメニュー(PauseMenuConst)を流用し、
// 色とテキストだけ専用にする（赤系バナー）。
//==========================================================
namespace GameOverConst
{
	// 項目
	enum Item { Retry = 0, StageSelect, Title, Count };

	// 表示テキスト（日本語）
	constexpr const char* TitleText       = "ゲームオーバー";
	constexpr const char* ItemRetry       = "もういちど";
	constexpr const char* ItemStageSelect = "ステージせんたく";
	constexpr const char* ItemTitle       = "タイトルへ";

	// フォント（ポーズと共用）
	constexpr int FontNo      = PauseMenuConst::FontNo;
	constexpr int TitleFontNo = PauseMenuConst::TitleFontNo;

	// レイアウト（ポーズと共用）
	constexpr float PanelFullW       = PauseMenuConst::PanelFullW;
	constexpr float BannerH          = PauseMenuConst::BannerH;
	constexpr float ContentPadTop    = PauseMenuConst::ContentPadTop;
	constexpr float ContentPadBottom = PauseMenuConst::ContentPadBottom;
	constexpr float ItemRowH         = PauseMenuConst::ItemRowH;
	constexpr float SidePad          = PauseMenuConst::SidePad;
	constexpr float PanelRadius      = PauseMenuConst::PanelRadius;
	constexpr int   PanelCornerSegs  = PauseMenuConst::PanelCornerSegs;
	constexpr int   PanelEdgeThickness = PauseMenuConst::PanelEdgeThickness;
	constexpr float PanelShadowA     = PauseMenuConst::PanelShadowA;
	constexpr float HighlightH       = PauseMenuConst::HighlightH;
	constexpr float HighlightA       = PauseMenuConst::HighlightA;
	constexpr float HighlightBlinkA  = PauseMenuConst::HighlightBlinkA;
	constexpr float BlinkSpeed       = PauseMenuConst::BlinkSpeed;

	// 背景の暗転（ゲームオーバーは濃いめ）
	constexpr float DimAlpha = 0.78f;

	// 出現フェード（秒）
	constexpr float FadeInTime = 0.5f;

	// 色：本体は暗い赤紫、縁・バナーは赤、バナー文字は明るい白
	constexpr float PanelBodyR = 0.12f, PanelBodyG = 0.04f, PanelBodyB = 0.06f, PanelBodyA = 0.96f;
	constexpr float PanelEdgeR = 0.90f, PanelEdgeG = 0.20f, PanelEdgeB = 0.22f, PanelEdgeA = 0.55f;
	constexpr float BannerR = 0.78f, BannerG = 0.12f, BannerB = 0.14f, BannerA = 1.0f;   // 赤バナー
	constexpr float BannerTextR = 1.0f, BannerTextG = 0.95f, BannerTextB = 0.92f;        // 明るい文字
	constexpr float HighlightR = 1.0f, HighlightG = 0.35f, HighlightB = 0.35f;           // 選択バー(赤)

	// ── シネマティック表示（箱パネルを使わない新レイアウト）──
	constexpr float TitleYRatio    = 0.20f;   // タイトルY（画面中心から上へ：画面高×この比率）
	constexpr float TitlePulse     = 2.4f;    // タイトル赤の鼓動の速さ
	constexpr float TitleGlowMax   = 0.45f;   // タイトル背後の赤グロー最大濃さ
	constexpr float DividerHalfW   = 230.0f;  // タイトル下の区切り線の半幅(px)
	constexpr float DividerH       = 2.0f;    // 区切り線の太さ(px)
	constexpr float DividerGap     = 54.0f;   // タイトル中心から区切り線までの距離(px)
	constexpr float ItemStartY     = -30.0f;  // 1項目目のY（中心原点・+Y上）
	constexpr float ItemGapPx      = 66.0f;   // 項目の縦間隔(px)
	constexpr float ItemUnderlineW = 120.0f;  // 選択項目の下線・半幅(px)
	constexpr float TitleColR = 0.95f, TitleColG = 0.13f, TitleColB = 0.13f;   // タイトル赤
}
