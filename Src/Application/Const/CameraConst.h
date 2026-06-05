#pragma once

namespace CameraConst
{
	//==========================================================
	// カメラモード
	// ルーム（ZONE）ごとに挙動を切り替える
	//==========================================================
	enum class CameraMode
	{
		SideScroll,   // 既存：2.5D横スクロール（LittleNightmares風）
		Fixed2D,      // 純2D：カメラを完全固定・横スクロールなし
		TopDown,      // 俯瞰：マップ中心の真上から見下ろす
	};

	// 基本オフセット
	constexpr float OffsetY          = 3.0f;   // ターゲットより上にずらす量
	constexpr float OffsetZ          = -15.0f; // ターゲットより手前にずらす量

	// 追従
	constexpr float PosLerp          = 0.10f;  // カメラ位置の補間速度（小さいほどふわふわ）
	constexpr float LookAtLerp       = 0.06f;  // 注視点の補間速度（位置より遅くして"フォーカス感"）
	constexpr float FollowWeightX    = 0.60f;  // ルーム中心からプレイヤーに寄る横割合（本家6割）
	constexpr float FollowWeightY    = 0.35f;  // 縦割合
	constexpr float MinFollowWeightX = 0.10f;  // 完全停止しないための最低追従量
	constexpr float MinFollowWeightY = 0.05f;

	// 注視点
	constexpr float LookAtHeight     = 1.5f;   // ターゲット頭上オフセット
	constexpr float LookAheadX       = 1.5f;   // 進行方向への先読みオフセット
	constexpr float LookAheadY       = 0.2f;

	// ルーム遷移
	constexpr float RoomTransitionSpeed = 0.8f;  // 遷移速度（約1.25秒でスゥンと切り替わる）
	constexpr float FloatAmplitude   = 0.25f;  // 上下の浮遊幅
	constexpr float FloatSpeed       = 0.8f;   // 浮遊の周期速度（rad/s）

	// ロール（左右傾き）
	constexpr float RollMaxDeg       = 4.0f;   // 最大傾き角度
	constexpr float RollLerp         = 0.05f;  // ロールの補間速度
	constexpr float RollSensitivity  = 0.15f;  // プレイヤーオフセットからロールへの変換係数

	// ピッチ制限（カメラが上を向かないようにする最大仰角）
	constexpr float MaxPitchDeg      = 30.0f;  // 上向き最大角度（度）

	// カメラY最低オフセット（プレイヤーY + この値 より下には行かない）
	// OffsetY と同じかそれ以上にしておくこと
	constexpr float MinCamOffsetY    = 3.0f;

	// ルーム遷移
	constexpr float TransitionLerp   = 0.07f;

	// ── TopDown モード専用 ──────────────────────────────────────
	constexpr float TopDownHeight    = 25.0f;  // カメラの高さ（プレイヤー上方）
	constexpr float TopDownOffsetZ   =  0.0f;  // 奥行きオフセット（真上なので0）
	constexpr float TopDownFovDeg    = 60.0f;  // TopDown時のFOV（広め）

	// ── Fixed2D モード専用 ─────────────────────────────────────
	// マリギャラ2Dモード風：プレイヤーをX/Yとも追従、Z固定、フワフワなし・ロールなし
	constexpr float Fixed2DPosLerp  = 0.12f;   // カメラ位置補間
	constexpr float Fixed2DLookLerp = 0.10f;   // 注視点補間
	constexpr float Fixed2DOffsetY  = 2.0f;    // プレイヤーからの縦オフセット
	constexpr float Fixed2DOffsetZ  = -14.0f;  // 深度オフセット（完全横から見る）

	// ── フォーカスオフセット ────────────────────────────────────
	// ルームごとに設定できる重力ローカル空間のカメラフォーカスオフセット補間速度
	constexpr float FocusOffsetLerp        = 0.06f;  // デフォルト補間速度（ゆったり）
	constexpr float FocusOffsetReturnLerp  = 0.04f;  // デフォルトへ戻るときの補間速度
}