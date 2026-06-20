#pragma once

//==========================================================
// PauseMenuConst.h
// ゲーム中のポーズメニュー（タイトル／ステージセレクトへ戻る）用の定数
//==========================================================
namespace PauseMenuConst
{
	// 項目
	enum Item { Resume = 0, StageSelect, Title, Count };

	// フォント（他シーンと別番号にして衝突を避ける）
	constexpr int          FontNo     = 1;
	constexpr int          FontHeight = 40;
	constexpr const char*  FontName   = "Arial";

	constexpr const char*  TitleText  = "- PAUSE -";

	// レイアウト（画面中心が原点・+Y上）
	constexpr float TitleYRatio   = 0.20f;   // 見出しの高さ（中心からの割合）
	constexpr float ItemTopYRatio = 0.04f;   // 先頭項目の高さ
	constexpr float ItemSpacing   = 64.0f;   // 項目間の間隔（px）

	// 背景の暗転
	constexpr float DimAlpha = 0.6f;

	// 選択中の点滅速度
	constexpr float BlinkSpeed = 4.0f;

	// ── パネル・ハイライト・アクセント ──
	constexpr float PanelWidth     = 470.0f; // メニューパネルの幅(px)
	constexpr float PanelPadTop    = 78.0f;  // 見出し上の余白(px)
	constexpr float PanelPadBottom = 54.0f;  // 末尾項目下の余白(px)
	constexpr float HighlightW     = 400.0f; // 選択ハイライトバーの幅(px)
	constexpr float HighlightH     = 54.0f;  // 選択ハイライトバーの高さ(px)
	constexpr int   AccentIconSize = 40;     // 選択中に出すコアリアアイコンのサイズ(px)
	constexpr float AccentGap      = 18.0f;  // バー左端からアイコン中心までの距離(px)
}
