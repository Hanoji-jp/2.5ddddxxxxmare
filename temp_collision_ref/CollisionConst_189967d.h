#pragma once

namespace CollisionConst
{
    // 当たり判定メッシュのノード名プレフィックス
    constexpr const char* ColPrefix     = "col_";

    // 床判定レイの長さ（キャラクター足元から下に飛ばす）
    constexpr float GroundRayLength        = 2.5f;

    // 惑星重力無効化用：マップ床検出レイの長さ（長めに設定して床の真上を広く検知）
    constexpr float MapGroundDetectLength  = 30.0f;

    // 床とみなすレイ起点のオフセット（足元より少し上から）
    // MaxFallSpeed(1.0f) より大きくしないとトンネリングが起きる
    constexpr float GroundRayOffset     = 1.2f;

    // 壁判定：水平レイの長さ（キャラクター半径）
    // 壁から一定距離を保つことでbottom裏面へのめり込みを防ぐ
    constexpr float WallRayLength       = 0.7f;

    // 壁判定：レイを飛ばす高さ（複数段）
    constexpr float WallRayOffsetY0     = 0.15f;  // 足元（床面より少し上、コーナー干渉を避ける）
    constexpr float WallRayOffsetY1     = 0.5f;   // 足元付近
    constexpr float WallRayOffsetY2     = 1.0f;   // 腰付近
    constexpr float WallRayOffsetY3     = 1.5f;   // 胸付近

    // 壁判定スフィア半径（旧、残置）
    constexpr float WallSphereRadius    = 0.4f;
    constexpr float WallSphereOffsetY   = 0.8f;

    // 着地時のスナップ許容距離
    constexpr float GroundSnapDist          = 0.5f;

    // 着地時の m_upDir 補正速度（lerp係数）：1.0f=瞬時、小さいほど滑らか
    constexpr float GroundUpDirSlerpSpeed   = 0.3f;

    // 壁レイ方向（8方向）の数
    constexpr int   WallRayDirCount     = 8;
}
