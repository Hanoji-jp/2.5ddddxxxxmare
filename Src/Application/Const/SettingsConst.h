#pragma once
#include "FontConst.h"

//==========================================================
// SettingsConst.h
// 設定画面（音量・画面モード・解像度）の定数。
// ・ポーズメニュー／タイトルの両方から開ける共通設定ウィンドウ。
// ・音量と全画面/窓はその場で反映。解像度は保存して次回起動で反映。
//==========================================================
namespace SettingsConst
{
	// 保存ファイル
	constexpr const char* FilePath = "Asset/Data/settings.csv";

	// 解像度の選択肢
	struct Res { int w; int h; const char* label; };
	constexpr Res Resolutions[] =
	{
		{ 1280,  720, "1280 x 720"  },
		{ 1600,  900, "1600 x 900"  },
		{ 1920, 1080, "1920 x 1080" },
		{ 2560, 1440, "2560 x 1440" },
	};
	constexpr int ResolutionCount = static_cast<int>(sizeof(Resolutions) / sizeof(Resolutions[0]));

	// 音量の増減ステップ
	constexpr float VolumeStep = 0.05f;

	// FPS（フレームレート上限）の選択肢
	struct FpsOpt { int fps; const char* label; };
	constexpr FpsOpt FpsOptions[] =
	{
		{ 30,   "30"     },
		{ 60,   "60"     },
		{ 120,  "120"    },
		{ 144,  "144"    },
		{ 240,  "240"    },
		{ 1000, "無制限" },   // 実質上限なし
	};
	constexpr int FpsCount = static_cast<int>(sizeof(FpsOptions) / sizeof(FpsOptions[0]));

	// ── 行（項目）──
	enum Row { Master = 0, Bgm, Se, Screen, Resolution, Fps, FpsShow, Back, Count };

	// FPSカウンター表示用フォント（拡張した空きスロット）
	constexpr int   FpsCounterFontNo = 10;
	constexpr int   FpsCounterFontH  = 22;

	// 起動ロード画面のタイトル用フォント（大きめ。空きスロット11）
	constexpr int   LoadingTitleFontNo = 11;
	constexpr int   LoadingTitleFontH  = 52;

	// ── フォント（空きスロット7）──
	constexpr int          FontNo     = 7;
	constexpr int          FontHeight = 32;
	constexpr const char*  FontName   = FontConst::GameFontName;

	// ── 表示テキスト ──
	constexpr const char*  TitleText  = "せってい";
	constexpr const char*  LabelMaster = "ぜんたい音量";
	constexpr const char*  LabelBgm    = "ＢＧＭ音量";
	constexpr const char*  LabelSe     = "ＳＥ音量";
	constexpr const char*  LabelScreen = "がめんモード";
	constexpr const char*  LabelRes    = "かいぞうど";
	constexpr const char*  LabelFps     = "ＦＰＳ";
	constexpr const char*  LabelFpsShow = "ＦＰＳひょうじ";
	constexpr const char*  LabelBack    = "もどる";
	constexpr const char*  ValueOn      = "オン";
	constexpr const char*  ValueOff     = "オフ";
	constexpr const char*  ValueFull   = "ぜんがめん";
	constexpr const char*  ValueWindow = "ウィンドウ";
	constexpr const char*  ResNote     = "（再起動後に反映）";
	constexpr const char*  Hint        = "Ｗ/Ｓ：せんたく　Ａ/Ｄ：へんこう　ＴＡＢ：とじる";

	// ── パネルのデザイン（PauseMenu と同系統の紺＋金）──
	constexpr float PanelW            = 660.0f;   // パネル幅(px)
	constexpr float BannerH           = 58.0f;    // 上部金バナーの高さ(px)
	constexpr float ContentPadTop     = 22.0f;
	constexpr float ContentPadBottom  = 48.0f;    // 最終行〜下端の余白（ヒスト分の余裕を含む）
	constexpr float RowH              = 58.0f;    // 1行の高さ(px)
	constexpr float SidePad           = 30.0f;    // 内側左右余白(px)
	constexpr int   EdgeThickness     = 3;        // 金縁の太さ(px)
	constexpr float PanelRadius       = 18.0f;    // 角丸半径(px)
	constexpr int   CornerSegs        = 8;        // 角丸の分割数

	constexpr float BodyR = 0.05f, BodyG = 0.07f, BodyB = 0.12f, BodyA = 0.96f; // 本体(紺)
	constexpr float EdgeR = 1.00f, EdgeG = 0.85f, EdgeB = 0.35f, EdgeA = 0.97f; // 縁・バナー(金)
	constexpr float BannerTextR = 0.05f, BannerTextG = 0.06f, BannerTextB = 0.10f; // バナー文字(暗)
	constexpr float ShadowA = 0.5f;

	// テキスト色
	constexpr float TextR = 0.92f, TextG = 0.95f, TextB = 1.00f;       // 通常文字(白)
	constexpr float TextSelR = 1.00f, TextSelG = 0.90f, TextSelB = 0.45f; // 選択中(金)

	// 選択ハイライト
	constexpr float HighlightA = 0.20f;

	// 音量バー
	constexpr float BarW      = 220.0f;  // バー全幅(px)
	constexpr float BarH      = 16.0f;   // バー高さ(px)
	constexpr float BarBgR = 0.20f, BarBgG = 0.22f, BarBgB = 0.30f, BarBgA = 0.9f; // 空き部分
	constexpr float BarFillR = 0.45f, BarFillG = 0.80f, BarFillB = 1.0f, BarFillA = 1.0f; // 充填

	// 全体のフェードイン
	constexpr float FadeSpeed = 6.0f;

	// 背景ぼかしの強さ（サンプリング半径）
	constexpr int   BlurRadius = 8;
}
