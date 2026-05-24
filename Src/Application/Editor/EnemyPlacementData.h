#pragma once

// エディターで配置する敵の種類
enum class EnemyType
{
	Melee,    // 近距離型
	Ranged,   // 遠距離型
};

// 敵1体分の配置データ
struct EnemyPlacementData
{
	EnemyType     type     = EnemyType::Melee;
	Math::Vector3 position = { 0.0f, 0.0f, 0.0f };
};
