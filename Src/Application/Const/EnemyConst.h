#pragma once

// 敵に関わる定数
namespace EnemyConst
{
    // -------------------------------------------------------
    // 共通
    // -------------------------------------------------------
    // 移動速度（基本）
    constexpr float MoveSpeed        = 0.04f;

    // 加速度（プレイヤーと同様の Lerp 係数）
    constexpr float Acceleration     = 0.008f;

    // 減速度
    constexpr float Deceleration     = 0.012f;

    // 向き補間速度（Slerp の t 値）
    constexpr float RotationSpeed    = 0.15f;

    // 最大HP
    constexpr int   MaxHp            = 3;

    // 索敵半径
    constexpr float SearchRadius     = 8.0f;

    // モデルの拡大率（プレイヤーと同スケール）
    constexpr float ModelScale       = 0.3f;

    // モデルのピボット補正
    constexpr float ModelOffsetY     = -0.56f;

    // アニメーションブレンドフレーム数
    constexpr int   AnimBlendFrames  = 8;

    // 巡回折り返し距離（スポーン中心からの半幅）
    constexpr float PatrolRange      = 4.0f;

    // -------------------------------------------------------
    // 近距離型（EnemyMelee）固有
    // -------------------------------------------------------
    // 攻撃射程
    constexpr float MeleeAttackRange = 1.5f;

    // 攻撃クールダウン（フレーム）
    constexpr int   MeleeAttackCool  = 60;

    // 近接攻撃のダメージ量
    constexpr int   MeleeDamage      = 1;

    // -------------------------------------------------------
    // 遠距離型（EnemyRanged）固有
    // -------------------------------------------------------
    // 攻撃射程（遠距離）
    constexpr float RangedAttackRange = 6.0f;

    // 攻撃を止める最低距離（近すぎたら逃げる）
    constexpr float RangedKeepDist    = 3.5f;

    // 攻撃クールダウン（フレーム）
    constexpr int   RangedAttackCool  = 90;

    // 遠距離攻撃の弾速
    constexpr float RangedBulletSpeed = 0.12f;

    // 発射位置の高さオフセット
    constexpr float RangedFireOffsetY = 1.0f;

    // -------------------------------------------------------
    // 仮モデルパス（後で差し替え）
    // -------------------------------------------------------
    constexpr const char* MeleeModelPath  = "Asset/Data/Player.gltf";
    constexpr const char* RangedModelPath = "Asset/Data/Player.gltf";
    // -------------------------------------------------------
    // エディター表示
    // -------------------------------------------------------
    // ゴーストスフィアの半径
    constexpr float GhostSphereRadius = 0.6f;
}
