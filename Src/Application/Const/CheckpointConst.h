#pragma once

// チェックポイントに関わる定数
namespace CheckpointConst
{
	// 判定球の半径
	constexpr float TriggerRadius   = 1.5f;

	// 落下死と判定するY座標（これより下に落ちたらリスポーン）
	constexpr float DeathY          = -10.0f;

	// チェックポイントのデバッグ球の色（緑）
	constexpr float DebugColorR     = 0.0f;
	constexpr float DebugColorG     = 1.0f;
	constexpr float DebugColorB     = 0.0f;
	constexpr float DebugColorA     = 1.0f;
}
