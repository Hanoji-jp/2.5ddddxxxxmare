#pragma once
// Intro cutscene constants (player is flung in, tumbles, then lands on the planet).
// Only plays on the stage whose index equals IntroConst::Stage.
namespace IntroConst
{
    // Stage index (1-based) that plays the intro. Others skip it.
    constexpr int   Stage        = 1;

    // Start offset from the spawn point.
    constexpr float Height       = 70.0f;   // up offset (高さ)
    constexpr float SideOffset   = 25.0f;   // horizontal offset so the entry arcs in

    // Throw impulse toward the spawn point.
    constexpr float LaunchSpeed  = 22.0f;

    // Gravity multiplier during the fall (slightly faster for drama).
    constexpr float GravityScale = 1.3f;

    // Scripted descent duration (sec): time to travel from start pos to the spawn.
    constexpr float Duration     = 3.2f;

    // Tumble roll speed (radians / sec).
    constexpr float SpinSpeed    = 9.0f;

    // Ignore ground detection for this long after start (avoid instant landing).
    constexpr float MinTime      = 0.3f;

    // Safety timeout: end the cutscene even if never grounded (e.g. no planet yet).
    constexpr float MaxTime      = 8.0f;

    // White flash strength on landing.
    constexpr float LandFlash    = 0.5f;

    // Cutscene camera (Mario-Galaxy style): starts wide / high / angled, then
    // orbits + zooms in as the player descends, and converges to the normal
    // gameplay camera pose at the end (seamless handoff).
    constexpr float CamStartYawDeg = 42.0f;   // start orbit angle around Y (0 = side view)
    constexpr float CamStartDist   = 60.0f;   // start distance (far)
    constexpr float CamStartHeight = -22.0f;  // start BELOW the player (looks up at it)
    constexpr float CamStartLookUp = 0.0f;    // look straight at the player at the start

    // Distance choreography: far -> rush in very close -> pull back for the crash landing.
    constexpr float CamCloseDist    = 2.5f;   // closest distance during the "whoosh" zoom-in
    constexpr float CamPullBackStart = 0.62f; // progress (0..1) where the pull-back begins
    // (the pull-back ends at the normal gameplay distance = |CameraSettings.OffsetZ|)
}
