#pragma once

// Gravity-direction arrows drawn on zone walls (Mario-Galaxy style)
namespace GravityArrowConst
{
    constexpr float Spacing      = 9.0f;   // world units between triangles (grid)
    constexpr float FrontOffsetZ = 0.2f;   // nudge toward camera (+Z) so it sits on the wall
    constexpr float ScrollSpeed  = 4.0f;   // world units/sec the triangles flow along gravity

    // Big soft glowing triangle (arrowhead only), WORLD units, world-fixed on the wall
    constexpr float TriHalfLen   = 3.0f;   // tip distance from center (gravity dir)
    constexpr float TriHalfWide  = 2.8f;   // base half width

    // Two additive layers (faint big + bright center) for the soft glow look
    constexpr float OuterScale   = 1.0f,  OuterAlpha = 0.16f;
    constexpr float InnerScale   = 0.55f, InnerAlpha = 0.28f;

    // Glow color：下向き=青、上向き=赤
    constexpr float ColorR = 0.35f, ColorG = 0.70f, ColorB = 1.0f;   // Down (blue)
    constexpr float UpColorR = 1.0f, UpColorG = 0.30f, UpColorB = 0.25f;  // Up (red)

    constexpr int   MaxArrows    = 600;    // safety cap per frame
}
