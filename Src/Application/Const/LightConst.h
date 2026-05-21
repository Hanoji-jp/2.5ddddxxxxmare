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
}
