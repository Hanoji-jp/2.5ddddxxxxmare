#pragma once
#include "FontConst.h"

//==========================================================
// StoryConst.h
// ストーリー（オープニング）ページ表示用の定数
//==========================================================
namespace StoryConst
{
	//----------------------------------------------------------
	// ページ画像（Asset/Texture/Story/StoryP1.png 〜 StoryP7.png）
	//----------------------------------------------------------
	constexpr int          PageCount   = 7;
	constexpr const char*  PagePathFmt = "Asset/Texture/Story/StoryP%d.png";  // 1始まり

	//----------------------------------------------------------
	// 演出
	//----------------------------------------------------------
	constexpr float FadeSpeed        = 1.6f;   // 1ページ目の黒フェードイン速度
	constexpr float PromptBlinkSpeed = 3.0f;   // 「PRESS ENTER」の点滅速度

	// 画像のアスペクト比（16:9 を維持してレターボックス表示する）
	constexpr float PageAspect       = 16.0f / 9.0f;

	//----------------------------------------------------------
	// ページめくり（2Dソフトカール）
	// ページを縦の短冊に分割し、右へ行くほど・進むほど曲げて巻く。
	// 2D(スプライト)描画なのでポストエフェクト(DoF/ブラー)が乗らず高画質のまま。
	//----------------------------------------------------------
	constexpr int   StripCount     = 64;      // 分割数（多いほど滑らか）
	constexpr float MaxBendDeg     = 210.0f;  // めくり切る時の右端の曲がり角（度）
	constexpr float TurnSpeed      = 1.4f;    // めくり進行(0→1)の速さ（1/秒）
	constexpr float CurlFadeStart  = 0.85f;   // この進行度から最後のひと巻きを消し始める
	constexpr float CurlShadeMin   = 0.35f;   // カール部の最暗ブライト（丸み表現）

	//----------------------------------------------------------
	// マウスドラッグでめくる（ページ上を左ドラッグ → めくり角が変わる）
	//----------------------------------------------------------
	constexpr float DragRangeRatio  = 0.8f;   // ページ幅のこの割合ぶん左へdrag＝めくり切り
	constexpr float DragCommit      = 0.5f;   // 離した時このめくり量以上なら確定して進む
	constexpr float DragReturnSpeed = 5.0f;   // 確定せず離した時、元へ戻る速さ(1/秒)

	// 角めくり（コーナーピール）：掴んだ角をシリンダー状に丸める（厚紙の硬いカール）
	constexpr int   PeelGridX       = 40;     // メッシュ分割（横）多いほど滑らか
	constexpr int   PeelGridY       = 28;     // メッシュ分割（縦）
	constexpr float PeelRadiusRatio = 0.22f;  // カールの半径（ページ幅比）。大きいほど厚紙＝硬い丸み

	//----------------------------------------------------------
	// フォント / プロンプト
	//----------------------------------------------------------
	constexpr int          FontNo      = 0;
	constexpr int          FontHeight  = 30;
	constexpr const char*  FontName    = FontConst::GameFontName;
	constexpr const char*  SkipText    = "TAB : SKIP";
	constexpr float        SkipYRatio   = 0.46f;  // 画面下

	// 最初のページだけ「マウスでめくる」を薄く大きく表示
	constexpr int          BigFontNo     = 1;
	constexpr int          BigFontHeight = 92;
	constexpr const char*  HintText      = "マウスでめくる";
	constexpr float        HintAlpha     = 0.16f;  // うっすら
}
