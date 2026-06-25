#pragma once

// 棘ボックスギミックに関わる定数
namespace SpikeBoxConst
{
	// 棘の向き（箱ごと90°回転で実現）。モデルは既定で棘が +Y(上) を向いている前提。
	enum class SpikeDir : int { Up = 0, Down = 1, Left = 2, Right = 3 };

	// 向き→Z軸回りの回転角(ラジアン)。Up=0 / Down=180 / Left=+90(+Y→-X) / Right=-90(+Y→+X)
	inline float DirToAngleZ(SpikeDir dir)
	{
		switch (dir)
		{
		case SpikeDir::Down:  return 3.14159265f;
		case SpikeDir::Left:  return 1.57079633f;
		case SpikeDir::Right: return -1.57079633f;
		case SpikeDir::Up:
		default:              return 0.0f;
		}
	}

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
