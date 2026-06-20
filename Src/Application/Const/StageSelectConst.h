#pragma once

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

	//----------------------------------------------------------
	// ステージノード（箱）
	//----------------------------------------------------------
	constexpr const char* NodeModel       = "Asset/Data/Box.gltf";
	constexpr float       NodeScale       = 1.5f;   // 箱の大きさ
	constexpr float       NodeSelectScale = 1.25f;  // 選択中の拡大倍率
	constexpr float       NodeBobAmp      = 0.25f;  // 選択中の上下ふわふわ
	constexpr float       NodeBobSpeed    = 3.0f;

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

	constexpr int          FontNo      = 0;
	constexpr int          FontHeight  = 32;
	constexpr const char*  FontName    = "Arial";
	constexpr const char*  TitleText   = "STAGE SELECT";
	constexpr const char*  HintText    = "WASD : MOVE     ENTER : GO";
	constexpr float        TitleYRatio = 0.40f;   // 上寄り
	constexpr float        HintYRatio  = 0.42f;   // 下寄り

	//----------------------------------------------------------
	// ステージ名（仮表示。いずれ各「島」のちゃんとした名前に差し替える）
	// stageId（0始まり）でインデックスする。範囲外は StageNameFallback。
	//----------------------------------------------------------
	constexpr const char* StageNames[] =
	{
		"STAGE 1 - GREEN PLANET",
		"STAGE 2 - (WIP)",
		"STAGE 3 - (WIP)",
		"STAGE 4 - (WIP)",
		"STAGE 5 - (WIP)",
		"STAGE 6 - (WIP)",
	};
	constexpr int         StageNameCount    = static_cast<int>(sizeof(StageNames) / sizeof(StageNames[0]));
	constexpr const char* StageNameFallback = "STAGE ?";
	constexpr float       StageNameYRatio   = 0.26f;   // 見出しの少し下に表示
}
