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

	// ── 見た目（旗：DrawVertices で自作）──
	// ポール（細い角柱）
	constexpr float PoleHeight = 2.6f;    // 高さ
	constexpr float PoleHalf   = 0.08f;   // 太さの半分（X/Z）
	// 旗の布
	constexpr float FlagLength   = 1.3f;  // ポールから横へ伸びる長さ(+X)
	constexpr float FlagHeight   = 0.75f; // 布の縦
	constexpr int   FlagSegments = 12;    // 長さ方向(横)の分割数
	constexpr int   FlagRows     = 6;     // 縦方向の分割数（横ポリゴンを追加）
	constexpr float FlagTopGap   = 0.15f; // ポール先端から布上端までの隙間
	// はためき（長さ方向＋縦方向の合成波で2次元的に揺らす）
	constexpr float WaveAmp   = 0.22f;    // 揺れ幅(Z)
	constexpr float WaveSpeed = 5.0f;     // 揺れの速さ
	constexpr float WaveFreq  = 3.0f;     // 波の細かさ（長さ方向）
	constexpr float WaveFreqV = 1.2f;     // 波の細かさ（縦方向）
	constexpr float WaveAmpV  = 0.06f;    // 縦方向の揺れの寄与

	// 色：ポール＝灰、布＝未通過は緑／通過したら金に変化
	constexpr float PoleColorR = 0.55f, PoleColorG = 0.56f, PoleColorB = 0.60f;
	constexpr float FlagColorR = 0.18f, FlagColorG = 0.85f, FlagColorB = 0.38f;  // 未通過(緑)
	constexpr float FlagActiveR = 1.00f, FlagActiveG = 0.82f, FlagActiveB = 0.20f; // 通過(金)

	// ── アウトライン（全周。濃色で一回り大きく先に描いて縁取りにする）──
	constexpr float OutlineColorR = 0.03f, OutlineColorG = 0.04f, OutlineColorB = 0.05f;
	constexpr float FlagOutlineScale = 1.14f;  // 布を中心から拡大する倍率（縁の太さ）
	constexpr float PoleOutlineExpand = 0.05f; // ポールを各面へ広げる量（縁の太さ）
}

