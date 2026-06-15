#pragma once

// Sparkle (star_01) effect constants shown around items.
namespace SparkleConst
{
    // Star texture
    inline constexpr const char* StarTexPath = "Asset/Effect/Star_01.png";

    // Number of stars per item
    constexpr int   Count         = 4;

    // Orbit radius (world units) and angular speed (rad/sec)
    constexpr float OrbitRadius   = 0.45f;
    constexpr float OrbitSpeed    = 1.5f;

    // Rising lifecycle: each star spawns low, floats up, then fades out.
    constexpr float LifeTime      = 1.6f;   // seconds for one rise cycle
    constexpr float RiseHeight    = 1.6f;   // how far it rises during its life

    // Base star size
    constexpr float BaseSize      = 0.35f;

    // Twinkle speed and minimum scale ratio (1.0 = no twinkle)
    constexpr float TwinkleSpeed  = 8.0f;
    constexpr float TwinkleMin    = 0.6f;

    // In-plane spin speed (rad/sec)
    constexpr float SpinSpeed     = 3.0f;

    // Base height above the item center (spawn height)
    constexpr float CenterOffsetY = 0.1f;

    // Default base color (RGBA) and color shift (random per-star variation, 0..1)
    constexpr float BaseColorR    = 1.0f;
    constexpr float BaseColorG    = 1.0f;
    constexpr float BaseColorB    = 1.0f;
    constexpr float BaseColorA    = 1.0f;
    constexpr float ColorShift    = 0.0f;

    // ---- Per-item presets ----
    // Coin: bigger, golden, with color variation
    constexpr float CoinStarSize    = 0.6f;
    constexpr float CoinStarRadius  = 0.7f;
    constexpr float CoinColorShift  = 0.35f;
    constexpr float CoinColorR      = 1.0f;
    constexpr float CoinColorG      = 0.9f;
    constexpr float CoinColorB      = 0.45f;
    constexpr float CoinColorA      = 1.0f;

    // Parasol: bluish
    constexpr float ParasolStarSize   = 0.45f;
    constexpr float ParasolStarRadius = 0.55f;
    constexpr float ParasolColorShift = 0.4f;
    constexpr float ParasolColorR     = 0.6f;
    constexpr float ParasolColorG     = 0.85f;
    constexpr float ParasolColorB     = 1.0f;
    constexpr float ParasolColorA     = 1.0f;
}
