#pragma once

//==========================================================
// StageSelectConst.h
// ステージセレクト（ハブ）用の定数
//==========================================================
namespace StageSelectConst
{
	//----------------------------------------------------------
	// ハブの配置
	// PlanetGravityManager / ManualGravityZone はシングルトンでゲームの惑星を
	// グローバル共有している。ハブで床惑星を足すとゲームを汚すため、ここでは
	// 惑星の影響圏(GravityRadius=30程度)から十分離れた遠方にハブを置き、
	// プレイヤーには手動重力Downを与えて静的な MovingFloor の上を歩かせる。
	//----------------------------------------------------------
	constexpr float HubOriginX = 3000.0f;   // ハブの基準X（惑星から遠ざける）

	//----------------------------------------------------------
	// 床（静的 MovingFloor）。サイズは半辺長。
	//----------------------------------------------------------
	constexpr float FloorHalfX   = 32.0f;
	constexpr float FloorHalfY   = 1.0f;
	constexpr float FloorHalfZ   = 5.0f;
	constexpr float FloorCenterY = -2.0f;
	// 床上面のワールドY（着地面の目安。カメラ・配置の基準に使う）
	constexpr float FloorTopY    = FloorCenterY + FloorHalfY;

	//----------------------------------------------------------
	// プレイヤー初期位置（ハブローカル座標。X は HubOriginX に加算）
	//----------------------------------------------------------
	constexpr float PlayerStartLocalX = -18.0f;
	constexpr float PlayerStartY      = FloorTopY + 3.0f;  // 少し上から落として着地させる

	//----------------------------------------------------------
	// ステージ入口（ポータル）。触れると入場する。
	//----------------------------------------------------------
	constexpr float        PortalLocalX       = 16.0f;          // ハブローカルX
	constexpr float        PortalHeight       = 2.6f;           // 床上の中心高さ
	constexpr float        PortalSize         = 4.5f;           // ビルボードの大きさ
	constexpr float        PortalTriggerRange = 2.4f;           // この距離以内で入場
	constexpr float        PortalHintRange    = 7.0f;           // この距離以内でヒント表示
	constexpr float        PortalPulseSpeed   = 3.0f;           // 明滅速度
	constexpr float        PortalPulseAmp     = 0.25f;          // 明滅振幅
	constexpr float        PortalSpinSpeed    = 1.2f;           // 回転速度
	constexpr const char*  PortalTexPath      = "Asset/Effect/Particle04_bokashi_hard.png";
	// ポータルの色（シアン系の光）
	constexpr float        PortalColR = 0.4f;
	constexpr float        PortalColG = 0.85f;
	constexpr float        PortalColB = 1.0f;

	//----------------------------------------------------------
	// カメラ（横スクロール追従。プレイヤーのXだけ追う）
	//----------------------------------------------------------
	constexpr float CamOffsetY = 5.0f;     // 床上面からの高さ
	constexpr float CamOffsetZ = -22.0f;   // 手前(-Z)から +Z を見る
	constexpr float CamTargetY = 2.0f;     // 注視点の高さ（床上面基準）
	constexpr float CamFov     = 60.0f;
	constexpr float CamFollowLerp = 0.12f; // 追従の滑らかさ

	//----------------------------------------------------------
	// 投げ出され演出（イントロ）：絵本→黒フェード→上空から落下→不時着→通常カメラ
	//----------------------------------------------------------
	constexpr float IntroStartLocalX    = -4.0f;          // 落下開始のハブローカルX（中央寄り）
	constexpr float IntroStartHeight    = 34.0f;          // 床上面からの落下開始高さ
	constexpr float IntroThrowVX        = 0.05f;          // 吹き飛ばしの横初速（少し流す）
	constexpr float IntroThrowVY        = 0.0f;           // 縦初速（0=そのまま落下）
	constexpr float IntroFadeSpeed      = 1.2f;           // 黒フェードイン速度（絵本の黒から明ける）
	constexpr float IntroSettleSpeed    = 1.6f;           // 不時着後にカメラが通常へ戻る速さ
	constexpr float IntroShakeStr       = 0.8f;           // 不時着の揺れ強さ
	constexpr float IntroShakeDecay     = 3.0f;           // 揺れ減衰
	// 落下追従カメラ（プレイヤーを主体に、ぐるぐる旋回しながら追う）
	constexpr float IntroFallCamOffsetY = 2.5f;           // 落下中カメラの高さオフセット
	constexpr float IntroFallTargetY    = 0.5f;           // 落下中の注視点オフセット
	constexpr float IntroOrbitRadius    = 15.0f;          // 旋回半径（プレイヤーからの距離）
	constexpr float IntroOrbitStartDeg  = 180.0f;         // 旋回開始角（180=正面 -Z から開始）
	constexpr float IntroOrbitSpeedDeg  = 130.0f;         // 旋回速度（度/秒）

	// プレイヤーのタンブル（吹っ飛ばされてくるくる回る）
	constexpr float IntroSpinSpeed      = 7.0f;           // 落下中の回転速度（rad/秒）
	constexpr float IntroSpinSettle     = 9.0f;           // 着地後に回転を 0 へ戻す速さ

	//----------------------------------------------------------
	// 入場フェード（白へ。GameScene 側の白フェードインへ繋ぐ）
	//----------------------------------------------------------
	constexpr float FadeOutSpeed = 1.8f;

	//----------------------------------------------------------
	// フォント（プロンプト表示）
	//----------------------------------------------------------
	constexpr int          FontNo      = 0;
	constexpr int          FontHeight  = 36;
	constexpr const char*  FontName    = "Arial";
	constexpr const char*  HintText    = "WALK INTO THE GATE";
	constexpr const char*  TitleText   = "STAGE SELECT";
	constexpr float        HintYRatio  = 0.34f;   // 画面下寄り
	constexpr float        TitleYRatio = 0.40f;   // 画面上寄り
}
