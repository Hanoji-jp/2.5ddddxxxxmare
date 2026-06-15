#pragma once
#include "../GameObject/Character/Character.h"

// エディターで配置する敵の種類
enum class EnemyType
{
	Ranged,   // 遠距離型
	Cubun,    // ジャンプ棘付き敵
};

// 敵1体分の配置データ
struct EnemyPlacementData
{
	EnemyType                    type        = EnemyType::Cubun;
	Math::Vector3                position    = { 0.0f, 0.0f, 0.0f };
	Character::ManualGravityDir  initGravDir = Character::ManualGravityDir::None;
};
