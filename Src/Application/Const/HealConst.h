#pragma once

// Heal (green-stone item) effect constants
namespace HealConst
{
    // Height above the player to spawn the "+" mark (world units, along up)
    constexpr float HeadOffsetUp = 2.5f;

    // Lifetime of the "+" mark (sec)
    constexpr float PlusLife = 0.8f;

    // Base size of the "+" mark (px) - small
    constexpr float PlusSize = 11.0f;

    // Number of small "+" marks per spawn point
    constexpr int   PlusCount = 4;

    // Vertical stagger between stacked "+" marks (px)
    constexpr float PlusSpacing = 15.0f;

    // Horizontal random jitter of each "+" (px, +/-)
    constexpr float PlusJitterX = 11.0f;

    // Extra lifetime added per stacked "+" (sec)
    constexpr float PlusLifeStagger = 0.10f;

    // Rise speed of the "+" mark (px/sec)
    constexpr float PlusRiseSpeed = 80.0f;

    // Color of the "+" mark (green)
    constexpr float PlusColorR = 0.35f;
    constexpr float PlusColorG = 1.00f;
    constexpr float PlusColorB = 0.45f;

    // Thickness of the "+" mark (px) - thin
    constexpr float PlusThickness = 2.0f;

    // Opacity of the "+" mark (faint)
    constexpr float PlusAlpha = 0.45f;
}
