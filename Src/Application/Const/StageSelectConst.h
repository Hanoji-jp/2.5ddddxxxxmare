#pragma once
#include "FontConst.h"

//==========================================================
// StageSelectConst.h
// ステージセレクト（マリギャラ2のワールドマップ式）の定数。
// 箱ノードをドットの道で繋ぎ、プレイヤー(カーソル)がWASDでノード間を
// ホップ移動して選び、Enter/Spaceで決定してステージへ入る。
//==========================================================
namespace StageSelectConst
{
	//----------------------------------------------------------
	// カメラ（固定の見下ろし気味アイソメ）
	//----------------------------------------------------------
	constexpr float CamEyeY      = 17.0f;
	constexpr float CamEyeZ      = -19.0f;
	constexpr float CamTargetY   = 1.0f;
	constexpr float CamFov       = 48.0f;
	constexpr float CamSwayX     = 0.6f;   // 待機ゆらぎ
	constexpr float CamSwayY     = 0.3f;
	constexpr float CamSwaySpeed = 0.3f;
	constexpr float CamFocusLerp   = 0.045f; // 選択ノードへの追従の滑らかさ（小さいほどゆったり）
	constexpr float CamFocusAmount = 0.25f;  // 注視を寄せる割合（0=中心固定, 1=完全にノード中心）
	constexpr float CamFocusMargin = 4.5f;   // デッドゾーン：この距離ぶんは追わない（遊び）
	// マウスパララックス（視点ずらし量。大きいほど奥行きが強い）
	constexpr float ParallaxX      = 0.9f;
	constexpr float ParallaxY      = 0.5f;

	//----------------------------------------------------------
	// ステージノード（箱）
	//----------------------------------------------------------
	constexpr const char* NodeModel       = "Asset/Data/Box.gltf";
	constexpr float       NodeScale       = 1.5f;   // 箱の大きさ
	constexpr float       NodeSelectScale = 1.25f;  // 選択中の拡大倍率
	constexpr float       NodeBobAmp      = 0.25f;  // 選択中の上下ふわふわ
	constexpr float       NodeBobSpeed    = 3.0f;

	// 未解放ノードの見た目（ほぼ黒・小さめ）と解放アニメ
	constexpr float       LockedNodeScaleMul = 0.6f;   // 未解放時の大きさ倍率
	constexpr float       LockedColorV       = 0.08f;  // 未解放時の色（完全じゃない黒＝暗いグレー）
	constexpr float       UnlockAnimTime     = 0.7f;   // 取り戻すアニメの長さ(秒)

	// 「行けない！」演出（未解放ノードへの道を赤点滅）
	constexpr float       DenyFlashTime  = 0.6f;   // 点滅の長さ(秒)
	constexpr float       DenyFlashSpeed = 30.0f;  // 点滅の速さ
	constexpr float       DenyColR = 1.0f, DenyColG = 0.15f, DenyColB = 0.15f;  // 赤

	//----------------------------------------------------------
	// マーカー（プレイヤー＝カーソル。物理なし・モデルのみ）
	//----------------------------------------------------------
	constexpr const char* MarkerModel  = "Asset/Data/Player.gltf";
	constexpr float       MarkerScale   = 0.5f;
	constexpr float       MarkerYOffset = 0.9f;   // 箱の上に立つ高さ
	constexpr float       MoveDuration  = 0.25f;  // ノード間ホップ時間（秒）
	constexpr float       HopHeight     = 1.4f;   // ホップの高さ

	//----------------------------------------------------------
	// 方向選択（押した方向に最も合う隣ノードへ）
	//----------------------------------------------------------
	constexpr float DirDotThreshold = 0.35f;
	// 候補が2つ近い角度で並んだ時のあいまい判定マージン。
	// 最良スコアと次点の差がこれ未満なら「方向が曖昧」として移動しない
	// （＝奥の2ノードへは縦だけでなく斜め入力で行き先を確定させる）。
	constexpr float DirAmbiguityMargin = 0.15f;

	//----------------------------------------------------------
	// 連結ドット（ノード間の道）
	//----------------------------------------------------------
	constexpr const char* DotTex     = "Asset/Effect/Particle03.png";
	constexpr float       DotSpacing = 1.3f;   // ドット間隔
	constexpr float       DotSize    = 0.35f;
	constexpr float       DotColR    = 0.8f;
	constexpr float       DotColG    = 0.9f;
	constexpr float       DotColB    = 1.0f;

	// 選択中ノードの後光
	constexpr float HaloSize  = 4.5f;
	constexpr float HaloPulse = 2.0f;

	//----------------------------------------------------------
	// 背景の回転惑星（奥行き）
	//----------------------------------------------------------
	constexpr const char* BgPlanetModel = "Asset/Data/Planet.gltf";
	constexpr float BgPlanetX     = -24.0f;
	constexpr float BgPlanetY     = 13.0f;
	constexpr float BgPlanetZ     = 55.0f;
	constexpr float BgPlanetScale = 3.0f;
	constexpr float BgPlanetSpin  = 0.06f;

	//----------------------------------------------------------
	// 漂う光の粒（モート）
	//----------------------------------------------------------
	constexpr const char* MoteTex = "Asset/Effect/Particle03.png";
	constexpr int   MoteCount   = 36;
	constexpr float MoteAreaX   = 40.0f;
	constexpr float MoteAreaY   = 24.0f;
	constexpr float MoteAreaZ   = 24.0f;
	constexpr float MoteRise    = 0.6f;
	constexpr float MoteSizeMin = 0.12f;
	constexpr float MoteSizeMax = 0.4f;
	constexpr float MoteColR    = 0.6f;
	constexpr float MoteColG    = 0.8f;
	constexpr float MoteColB    = 1.0f;

	//----------------------------------------------------------
	// フェード / フォント / プロンプト
	//----------------------------------------------------------
	constexpr float IntroFadeSpeed = 1.2f;   // 開始の黒フェードイン
	constexpr float FadeOutSpeed   = 1.8f;   // 決定時の白フェード（GameSceneへ繋ぐ）

	// GO 入場演出：ぴょんとジャンプ→縮みながら箱の中へ沈んで入る
	constexpr float EnterHopTime   = 0.45f;  // 入場アニメの時間(秒)
	constexpr float EnterHopHeight = 1.8f;   // ジャンプの高さ
	constexpr float EnterSink      = 1.3f;   // 箱の中へ沈む量(px相当の高さ)

	// クリア後リザルトの登場演出：入場の逆。箱の中から小さく→大きくなりながらせり上がる
	// （入場と同じ EnterHopTime/Height/Sink を逆再生で使う）
	constexpr float ResultOutDelay = 0.6f;   // リザルト突入からポップ開始までの待ち(秒)

	constexpr int          FontNo      = 0;
	constexpr int          FontHeight  = 32;
	constexpr const char*  FontName    = FontConst::GameFontName;
	constexpr const char*  TitleText   = "ステージセレクト";
	constexpr const char*  HintText    = "WASD：いどう　　ENTER：けってい";
	constexpr float        TitleYRatio = 0.40f;   // 上寄り
	constexpr float        HintYRatio  = 0.42f;   // 下寄り

	//----------------------------------------------------------
	// ステージ名（仮表示。いずれ各「島」のちゃんとした名前に差し替える）
	// stageId（0始まり）でインデックスする。範囲外は StageNameFallback。
	//----------------------------------------------------------
	constexpr const char* StageNames[] =
	{
		"ステージ１　まるとしかくのほし",
		"ステージ２　かぜとパラソル",
		"ステージ３　うごくゆかととげ",
		"ステージ４（準備中）",
		"ステージ５（準備中）",
	};
	constexpr int         StageNameCount    = static_cast<int>(sizeof(StageNames) / sizeof(StageNames[0]));
	constexpr const char* StageNameFallback = "ステージ ？";
	constexpr float       StageNameYRatio   = 0.26f;   // 見出しの少し下に表示

	// ステージ説明（仮。stageId で参照、範囲外は空）
	constexpr const char* StageDescs[] =
	{
		"Reclaim the gravity core!",
		"(coming soon)",
		"Ride moving floors, dodge the spikes!",
		"(coming soon)",
		"(coming soon)",
	};
	constexpr int StageDescCount = static_cast<int>(sizeof(StageDescs) / sizeof(StageDescs[0]));

	// 選択詳細（画面下部の情報バー）の文言・レイアウト
	constexpr const char* SelClearedMark   = "クリア済み";
	constexpr const char* SelNotClearedMark = "未クリア";
	constexpr const char* SelLockedMark    = "ロック";
	constexpr const char* SelLockedHint    = "前のステージをクリアしよう";
	constexpr float       SelLockedGoAlpha = 0.35f;   // ロック時のGOボタンの暗さ
	constexpr float       SelLockedHintGap = 40.0f;   // GOボタン上のヒント文の距離(px)
	constexpr const char* SelBestCoinLabel = "さいこうコイン";
	constexpr const char* SelBestTimeLabel = "ベストタイム";
	constexpr const char* SelNoTime        = "--:--.--";
	constexpr const char* SelHint          = "ENTER：すすむ　　TAB：もどる";
	constexpr const char* SelGoHint        = "ENTER：すすむ";
	constexpr const char* SelBackHint      = "TAB：もどる";
	constexpr float       SelHintGap       = 28.0f;   // GO枠とBACK枠の間隔(px)

	// 下部情報バー
	constexpr float SelBarH       = 140.0f;  // バー高さ(px)
	constexpr float SelBarWFrac   = 0.94f;   // バー幅 = 画面幅 * これ
	constexpr float SelBarMargin  = 36.0f;   // 画面下端からの余白(px)
	constexpr float SelBarPad     = 40.0f;   // バー内の左右余白(px)
	constexpr float SelBarA       = 0.85f;   // バー背景アルファ
	constexpr float SelHintAboveBar = 18.0f; // バーの上に出すヒントの隙間(px)
	// GO/BACK ヒントの個別枠
	constexpr float SelHintBoxPadX = 48.0f;  // テキスト左右の余白(px)
	constexpr float SelHintBoxH    = 68.0f;  // 枠の高さ(px)
	constexpr float SelHintBoxA    = 0.85f;  // 枠背景アルファ
	constexpr float SelHintBoxRadius = 20.0f; // 角丸半径(px)

	// ステージのサムネ画像（スコアバーの角丸ボックス内に背景として敷く）
	// 画像は "Asset/Texture/StageThumb/Stage01.png"（stageId+1 の2桁）規約で読む。
	// 画像が無いステージは従来どおりの単色背景のまま。
	constexpr const char* ThumbPathFmt   = "Asset/Texture/StageThumb/Stage%02d.png";
	constexpr int   ThumbCornerSegs      = 8;     // 角の分割数（滑らかさ）
	constexpr float SelBarImgAlpha       = 1.0f;  // 背景画像の不透明度（1=そのままの明るさ）
	constexpr float SelBarImgBright      = 1.0f;  // 画像の明るさ倍率（sRGB無効化で素のまま表示するので既定1.0）
	constexpr float SelBarScrimAlpha     = 0.0f; // 画像の上に重ねる暗幕（文字可読性用。小さいほど明るい）
}
