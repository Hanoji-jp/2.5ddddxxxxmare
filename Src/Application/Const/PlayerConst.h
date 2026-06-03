#pragma once

// プレイヤーに関わる定数
namespace PlayerConst
{
    // 移動速度
    constexpr float MoveSpeed        = 0.08f;

    // 加速度（入力あり）
    constexpr float Acceleration     = 0.012f;

    // 減速度（入力なし）
    constexpr float Deceleration     = 0.018f;

    // 向き補間速度（1.0fで即座、小さいほど緩やか）
    constexpr float RotationSpeed    = 0.15f;

    // ジャンプ初速
    constexpr float JumpPower        = 0.4f;

    // 最大HP
    constexpr int   MaxHp            = 5;

    // 無敵時間（フレーム数）
    constexpr int   InvincibleFrame  = 60;

    // モデルの拡大率
    constexpr float ModelScale       = 0.3f;

    // モデルのピボット補正（足元がずれている場合に調整）
    constexpr float ModelOffsetY     = -0.56f;

    // 近接攻撃クールダウン（フレーム）
    constexpr int   MeleeCooldown    = 30;

    // 遠距離攻撃クールダウン（フレーム）
    constexpr int   RangedCooldown   = 45;

    // アニメーションブレンドフレーム数
    constexpr int   AnimBlendFrames  = 8;

    // 矢の発射オフセット
    constexpr float ArrowOffsetY     = 1.0f;

    // ダッシュ倍率（Shift 長押し）
    constexpr float DashSpeedMul     = 1.85f;  // 移動速度の倍率
    constexpr float DashAccelMul     = 1.6f;   // 加速度の倍率
    constexpr float DashAnimSpeedMul = 1.7f;   // アニメ再生速度の倍率

    // モデルパス
    constexpr const char* ModelPath  = "Asset/Data/Player.gltf";
    constexpr const char* SwordPath  = "Asset/Data/Sword.gltf";
    constexpr const char* BowPath    = "Asset/Data/Bow.gltf";
    constexpr const char* ArrowPath  = "Asset/Data/Bow_Arrow.gltf";
    constexpr const char* DustPath   = "Asset/Data/Box.gltf";  // 足元煙パーティクル用
}
