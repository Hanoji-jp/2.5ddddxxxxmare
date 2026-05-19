#pragma once

// プレイヤーに関わる定数
namespace PlayerConst
{
    // 移動速度
    constexpr float MoveSpeed       = 0.08f;

    // ジャンプ初速
    constexpr float JumpPower       = 0.4f;

    // 最大HP
    constexpr int   MaxHp           = 5;

    // 無敵時間（フレーム数）
    constexpr int   InvincibleFrame = 60;

    // モデルの拡大率
    constexpr float ModelScale      = 0.01f;
}
