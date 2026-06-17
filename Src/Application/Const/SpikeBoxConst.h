#pragma once

// 棘ボックスギミックに関わる定数
namespace SpikeBoxConst
{
	// デフォルトのスケール（モデル自然サイズに対する倍率）
	constexpr float DefaultSizeX = 1.0f;
	constexpr float DefaultSizeY = 1.0f;
	constexpr float DefaultSizeZ = 1.0f;

	// 棘ダメージ量
	constexpr int   SpikeDamage = 2;

	// モデル（spikebox.gltf）
	constexpr const char* BoxModelPath = "Asset/Data/spikebox.gltf";

	// 押し出し当たり判定ノード名
	constexpr const char* ColNodeName = "COL";

	// 棘ダメージ判定ノード名
	constexpr const char* AttackNodeName = "COL_SpikeAttack";

	// プレイヤーとの棘当たり判定半径（体に合わせて少し大きめ）
	constexpr float HitRadius = 0.6f;

	// CSV 保存パス
	constexpr const char* SavePath = "Asset/Data/spike_boxes.csv";
}
