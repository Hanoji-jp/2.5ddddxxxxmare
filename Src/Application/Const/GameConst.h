#pragma once

// ゲーム全体に関わる定数
namespace GameConst
{
    // 重力加速度
    constexpr float Gravity         = -0.02f;

    // 最大落下速度
    constexpr float MaxFallSpeed    = -1.0f;

    // 画面奥行き方向の固定Z座標（2.5D用）
    constexpr float FixedZ          = 0.0f;

    // カメラのオフセット（プレイヤーからの相対位置）
    constexpr float CameraOffsetY   = 3.0f;
    constexpr float CameraOffsetZ   = -15.0f;
}
