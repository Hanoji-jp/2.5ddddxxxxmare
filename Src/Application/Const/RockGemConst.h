#pragma once
#include "RockConst.h"

//==========================================================
// RockGemConst
// コインエディタで配置する「カラフル岩（スターピース風）」の定数。
// 形状は RockDrop（敵ドロップの緑エメラルド）と同じローポリ岩を流用するが、
// メッシュはグレースケールでベイクし、インスタンスごとにレインボー色で着色する。
//==========================================================
namespace RockGemConst
{
	// ── 形状（RockConst のジオメトリをそのまま流用＝同じ岩の見た目）──
	constexpr int          LatitudeSegs   = RockConst::LatitudeSegs;
	constexpr int          LongitudeSegs  = RockConst::LongitudeSegs;
	constexpr float        JitterStrength = RockConst::JitterStrength;
	constexpr unsigned int JitterSeed     = RockConst::JitterSeed;
	constexpr float        DarkFactor     = RockConst::DarkFactor;   // 奥面を暗く

	// ── サイズ・描画 ──
	constexpr float Radius          = 0.34f;   // スターピースとして少し大きめ
	constexpr float FaceAlpha       = 0.95f;
	constexpr float WireBoost       = 1.7f;
	constexpr float WireAlpha       = 0.7f;
	constexpr float WireDepthOutset = 1.03f;   // ワイヤーを面より少し外へ（裏線を隠す）
	constexpr float OutlineScale    = 1.18f;   // アウトライン用に一回り大きく描く倍率

	// ── レインボー着色（HSV）。色相はインスタンスごとにランダム ──
	constexpr float Saturation = 0.85f;
	constexpr float Value      = 1.0f;

	// ── 浮遊アニメ（その場でふわふわ＋自転）──
	constexpr float BobSpeed = 2.2f;   // 上下の速さ
	constexpr float BobAmp   = 0.18f;  // 上下の振幅
	constexpr float SpinSpeed = 1.6f;  // 自転（rad/秒）
	constexpr float Drag      = 0.985f; // カーソルで飛ばした後の速度減衰（60fps基準。勢いを保つ）

	// ── 取得 ──
	constexpr float HitRadius = 0.7f;  // プレイヤーとの取得判定半径

	// 配置データの保存ファイル名（StageManager::ResolvePath で各ステージ別に解決）
	constexpr const char* SaveFile = "rockgems.csv";
}
