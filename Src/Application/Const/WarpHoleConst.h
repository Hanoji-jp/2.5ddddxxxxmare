#pragma once

namespace WarpHoleConst
{
	//--------------------------------------------------
	// 判定
	//--------------------------------------------------
	// 吸い込み判定半径
	constexpr float SuckRadius          = 2.0f;

	// 射出速度
	constexpr float LaunchSpeed         = 12.0f;

	//--------------------------------------------------
	// ビジュアル：ワイヤーフレームトンネル
	//--------------------------------------------------
	// 同心円リングの分割数（1リングの頂点数）
	constexpr int   RingSegments        = 32;

	// 口元のリング半径
	constexpr float RingOuterRadius     = 2.5f;

	// トンネルの奥行き
	constexpr float FunnelDepth         = 8.0f;

	// 半径プロファイル制御点（口元→奥、線形補間で使用）
	// 1.0 → 0.7 → 0.4 → 0.45 → 0.40 → 0.40
	// ここを変えるだけでトンネルの形が変わる

	// 描画するリング枚数（奥行き方向のスライス数）
	constexpr int   RingLayers          = 12;

	// 放射スポーク（縦の格子線）の本数
	constexpr int   SpokeCount          = 16;

	// アニメ用：1秒に何リング分流れるか
	constexpr float TunnelScrollSpeed   = 1.5f;

	//--------------------------------------------------
	// ビジュアル：色・ブレンド
	//--------------------------------------------------
	// 入口リング色（RGBA） ─ 青紫系
	constexpr float EntryColorR         = 0.3f;
	constexpr float EntryColorG         = 0.0f;
	constexpr float EntryColorB         = 1.0f;
	constexpr float EntryColorA         = 0.85f;

	// 出口リング色（RGBA） ─ マゼンタ系
	constexpr float ExitColorR          = 1.0f;
	constexpr float ExitColorG          = 0.0f;
	constexpr float ExitColorB          = 0.8f;
	constexpr float ExitColorA          = 0.85f;

	//--------------------------------------------------
	// ビジュアル：アニメーション
	//--------------------------------------------------
	// トンネルスクロール速度（DrawEffect内で使用）
	constexpr float AnimSpeed           = 0.4f;

	//--------------------------------------------------
	// 保存
	//--------------------------------------------------
	constexpr const char* SavePath      = "Asset/Data/warp_holes.csv";
}
