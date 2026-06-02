#pragma once

// ライト・環境光に関わる定数
namespace LightConst
{
    // 環境光（アルファが全体の明るさ。1.0fで最大、0.0fで点光源のみ）
    constexpr Math::Vector4 AmbientColor     = { 0.4f, 0.4f, 0.5f, 0.6f };

    // 平行光の方向（正規化済み）
    constexpr Math::Vector3 DirLightDir      = { 0.4f, -0.8f, 0.4f };

    // 平行光の色（RGBを下げると柔らかくなる）
    constexpr Math::Vector3 DirLightColor    = { 0.6f, 0.6f, 0.65f };

    // ポイントライトのデフォルト値
    constexpr Math::Vector3 PointLightDefaultColor  = { 1.0f, 0.9f, 0.7f };
    constexpr float         PointLightDefaultRadius = 15.0f;

    // 平行光の影（シャドウマップ）の描画範囲
    // ShadowAreaSize : 影を生成する正方領域の一辺の長さ（大きいほど遠くまで影が落ちる）
    // ShadowAreaHeight : ライト方向の深度範囲（高さ）。範囲外のオブジェクトは影を落とさない
    constexpr Math::Vector2 ShadowAreaSize   = { 80.0f, 80.0f };
    constexpr float         ShadowAreaHeight = 100.0f;
}
