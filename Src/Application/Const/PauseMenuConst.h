#pragma once
#include "FontConst.h"

//==========================================================
// PauseMenuConst.h
// ゲーム中のポーズメニュー（タイトル／ステージセレクトへ戻る）用の定数
// デザインは StageSelect のウィンドウに合わせる：
//   紺の角丸パネル＋薄い金の縁取り＋上部に金バナー（暗い文字）。
//==========================================================
namespace PauseMenuConst
{
	// 項目
	enum Item { Resume = 0, Settings, Retry, StageSelect, Title, Count };

	// フォント（他シーンと別番号にして衝突を避ける）
	constexpr int          FontNo     = 1;
	constexpr int          FontHeight = 40;
	constexpr const char*  FontName   = FontConst::GameFontName;

	// バナー「PAUSE」用の大きいフォント（空きスロット4。他シーンと衝突しない番号）
	constexpr int          TitleFontNo = 4;
	constexpr int          TitleFontH  = 60;

	// 表示テキスト（日本語）
	constexpr const char*  TitleText       = "ポーズ";
	constexpr const char*  ItemResume      = "つづける";
	constexpr const char*  ItemSettings    = "せってい";
	constexpr const char*  ItemRetry       = "やりなおす";
	constexpr const char*  ItemStageSelect = "ステージせんたく";
	constexpr const char*  ItemTitle       = "タイトルへ";

	// 背景の暗転＋ぼかし
	constexpr float DimAlpha   = 0.35f;  // ぼかしの上にうっすら暗幕
	constexpr int   BlurRadius = 8;      // 背景ぼかしの強さ（サンプリング半径）

	// メニューからシーン遷移するときの黒フェードアウト速度（1秒あたり。大きいほど速い）
	constexpr float ExitFadeSpeed = 2.2f;

	// 選択中の点滅速度
	constexpr float BlinkSpeed = 4.0f;

	// ── パネルのデザイン（StageSelectと同値）──
	constexpr float PanelRadius        = 20.0f;  // 角丸半径(px)
	constexpr int   PanelCornerSegs    = 8;      // 角丸の分割数
	constexpr int   PanelEdgeThickness = 3;      // 金縁の太さ(px)
	constexpr float PanelBodyR = 0.05f, PanelBodyG = 0.07f, PanelBodyB = 0.12f, PanelBodyA = 0.94f; // 本体(紺)
	constexpr float PanelEdgeR = 1.00f, PanelEdgeG = 0.85f, PanelEdgeB = 0.35f, PanelEdgeA = 0.95f; // 縁・バナー(金)
	constexpr float PanelShadowA = 0.5f;         // 影の濃さ
	constexpr float BannerTextR = 0.05f, BannerTextG = 0.06f, BannerTextB = 0.10f; // バナー文字(暗)

	// ── レイアウト（画面中心が原点・+Y上。パネルは画面中央）──
	constexpr float PanelFullW       = 560.0f;   // パネル全幅(px)
	constexpr float BannerH          = 78.0f;    // 上部金バナーの高さ(px)
	constexpr float ContentPadTop    = 26.0f;    // バナー下〜最初の項目までの余白(px)
	constexpr float ContentPadBottom = 30.0f;    // 最後の項目下の余白(px)
	constexpr float ItemRowH         = 64.0f;    // 項目1行の高さ(px)
	constexpr float SidePad          = 24.0f;    // パネル内側の左右余白(px)

	// ── 選択ハイライト・アクセント ──
	constexpr float HighlightH     = 52.0f;  // 選択ハイライトバーの高さ(px)
	constexpr float HighlightA     = 0.20f;  // ハイライトの基本アルファ
	constexpr float HighlightBlinkA = 0.14f; // 点滅で増減するアルファ幅
	constexpr int   AccentIconSize = 36;     // 選択中に出すコアリアアイコンのサイズ(px)
	constexpr float AccentGap      = 22.0f;  // バー左端からアイコン中心までの距離(px)
}
