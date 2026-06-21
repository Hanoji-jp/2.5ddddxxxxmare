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

    // -------------------------------------------------------
    // 天井判定（Box底面への頭突き検出）
    // -------------------------------------------------------
    // 頭上からレイを飛ばす起点オフセット（キャラクター身長の頂点付近）
    constexpr float CeilingRayOffset    = 1.0f;
    // 天井とみなすスナップ許容距離
    constexpr float CeilingSnapDist     = 0.3f;

    // Normal Box 壁判定：ヒット面法線の上方向成分がこれ以上なら底面・天面と判断して壁判定から除外
    constexpr float WallNormalUpThreshold   = 0.5f;

    // -------------------------------------------------------
    // 壁判定（NormalGravity Box 用）
    // -------------------------------------------------------
    constexpr float WallRayLength_Normal    = 0.45f;  // 水平レイの長さ
    constexpr float WallRayOffsetYNeg_Normal= -0.12f; // 足元より少し下（コーナー埋まり防止）
    constexpr float WallRayOffsetY0_Normal  = 0.0f;   // 足元ぴったり
    constexpr float WallRayOffsetY1_Normal  = 0.4f;   // 腰付近
    constexpr float WallRayOffsetY2_Normal  = 0.9f;   // 胸付近

    // -------------------------------------------------------
    // 壁判定（非 Normal Box 惑星 / 通常マップ 用）
    // -------------------------------------------------------
    constexpr float WallRayLength           = 0.75f;  // 水平レイの長さ
    constexpr float WallRayOffsetYNeg       = -0.15f; // 足元より少し下
    constexpr float WallRayOffsetY0         = 0.0f;   // 足元ぴったり
    constexpr float WallRayOffsetY1         = 0.15f;  // 足元付近
    constexpr float WallRayOffsetY2         = 0.5f;   // 腰付近
    constexpr float WallRayOffsetY3         = 1.0f;   // 胸付近

    // 床着地判定：ヒット点の水平距離がこれ以上なら「壁の天面」と判断して無視
    constexpr float GroundHitHorizontalMax = 0.5f;
    constexpr float WallSphereRadius    = 0.4f;
    constexpr float WallSphereOffsetY   = 0.8f;

    // 着地時のスナップ許容距離
    constexpr float GroundSnapDist          = 0.5f;

    // 床判定：プレイヤー中心だけでなく体の幅ぶん外側にもレイを撒くサンプル半径。
    // 端で中心レイが外れても、体が床にかかっていれば接地できる（端めり込み防止）。
    constexpr float GroundSampleRadius      = 0.5f;

    // Box床判定：hitDist がこれ未満（レイ起点のすぐ直下）はBox内部誤検出として除外
    // GroundRayOffset - GroundSnapDist より小さければ内側からのヒット
    constexpr float GroundHitDistMin        = 0.2f;

    // 着地時の m_upDir 補正速度（lerp係数）：1.0f=瞬時、小さいほど滑らか
    constexpr float GroundUpDirSlerpSpeed   = 0.8f;

    // 壁レイ方向（8方向）の数
    constexpr int   WallRayDirCount     = 8;

    // -------------------------------------------------------
    // 形状押し出し（球カプセル vs 地形）：本体のめり込み解決
    //   レイではなく「形状が重なったら法線方向へ押し戻す」方式。
    //   角(コーナー)でも最近接点方向へ斜めに押し出されるので端めり込みが起きない。
    // -------------------------------------------------------
    constexpr float BodyRadius          = 0.4f;  // プレイヤー本体の球半径（見た目の体半幅）
    constexpr float BodyLowerOffset     = 0.4f;  // 下側の球：足元(m_upDir)からのオフセット
    constexpr float BodyUpperOffset     = 1.0f;  // 上側の球：足元(m_upDir)からのオフセット
    constexpr int   PenetrationIterations = 3;   // 複数接触の収束反復回数
}
