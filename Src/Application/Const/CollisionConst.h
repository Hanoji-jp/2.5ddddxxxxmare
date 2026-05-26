#pragma once

namespace CollisionConst
{
    // 当たり判定メッシュのノード名プレフィックス
    constexpr const char* ColPrefix     = "col_";

    // 床判定レイの長さ（キャラクター足元から下に飛ばす）
    constexpr float GroundRayLength     = 2.5f;

    // 床とみなすレイ起点のオフセット（足元より少し上から）
    // MaxFallSpeed(1.0f) より大きくしないとトンネリングが起きる
    constexpr float GroundRayOffset     = 1.2f;

    // 壁判定：水平レイの長さ（キャラクター半径）
    constexpr float WallRayLength       = 0.4f;

    // 壁判定：レイを飛ばす高さ（複数段）
    constexpr float WallRayOffsetY0     = 0.3f;   // 足元付近
    constexpr float WallRayOffsetY1     = 0.9f;   // 腰付近
    constexpr float WallRayOffsetY2     = 1.5f;   // 胸付近

    // 壁判定スフィア半径（旧、残置）
    constexpr float WallSphereRadius    = 0.4f;
    constexpr float WallSphereOffsetY   = 0.8f;

    // 着地時のスナップ許容距離
    constexpr float GroundSnapDist      = 0.5f;

    // 壁レイ方向（8方向）の数
    constexpr int   WallRayDirCount     = 8;
}
