#pragma once

// 敵に関わる定数
namespace EnemyConst
{
    // 移動速度（基本）
    constexpr float MoveSpeed       = 0.04f;

    // 最大HP
    constexpr int   MaxHp           = 3;

    // 索敵半径
    constexpr float SearchRadius    = 8.0f;

    // 攻撃射程
    constexpr float AttackRange     = 1.5f;

    // モデルの拡大率
    constexpr float ModelScale      = 0.01f;
}
