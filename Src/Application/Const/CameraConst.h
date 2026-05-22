#pragma once

namespace CameraConst
{
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

	// 浮遊ボブ
	constexpr float FloatAmplitude   = 0.25f;  // 上下の浮遊幅
	constexpr float FloatSpeed       = 0.8f;   // 浮遊の周期速度（rad/s）

	// ロール（左右傾き）
	constexpr float RollMaxDeg       = 4.0f;   // 最大傾き角度
	constexpr float RollLerp         = 0.05f;  // ロールの補間速度
	constexpr float RollSensitivity  = 0.15f;  // プレイヤーオフセットからロールへの変換係数

	// ルーム遷移
	constexpr float TransitionLerp   = 0.07f;
}