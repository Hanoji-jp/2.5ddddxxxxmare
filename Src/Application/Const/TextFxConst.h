#pragma once
// Shared text drop-shadow settings for UI text (everything except the
// conversation window, which uses its own outline+shadow style).
// Center-origin sprite space, +Y up, so "down" is -Y.
namespace TextFxConst
{
    constexpr float ShadowOffX = 4.0f;     // to the right
    constexpr float ShadowOffY = 4.0f;     // downward (applied as -Y)
    constexpr float ShadowR = 0.0f, ShadowG = 0.0f, ShadowB = 0.0f;
    constexpr float ShadowAlphaMul = 0.6f; // shadow alpha = text alpha * this
}
