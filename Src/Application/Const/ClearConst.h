#pragma once
// Stage clear sequence (collect the goal core = instant clear, Mario-Galaxy style).
namespace ClearConst
{
    // Time showing the get effect (world frozen) before the screen starts fading out.
    constexpr float GetTime  = 3.4f;

    // Fade-to-black duration after the get effect, then return to StageSelect.
    constexpr float FadeTime = 1.2f;

    // White flash strength on pickup.
    constexpr float FlashStrength = 1.0f;

    // --- Star-get camera (dynamic: whoosh-in + sweep up & around) ---
    // Distance: far -> near (push in)
    constexpr float CamStartDist     = 16.0f;
    constexpr float CamEndDist       = 6.5f;
    // Pitch: look up from below (negative) -> sweep up over the player (positive)
    constexpr float CamStartPitchDeg = -24.0f;
    constexpr float CamEndPitchDeg   =  12.0f;
    // Focus height: low -> high (player lifts the core up)
    constexpr float CamStartFocusUp  = 0.8f;
    constexpr float CamEndFocusUp    = 3.2f;
    // Orbit one full loop (Bezier eased). End yaw = 180 + 360 = 540 (= 180),
    // so the final eye sits on the -Z side looking toward +Z (behind the player),
    // and the player faces +Z too (hero shot looking into +Z).
    constexpr float CamStartYawDeg   = 180.0f;
    constexpr float CamTotalOrbitDeg = 360.0f;

    // After this progress (0..1), the player turns to face +Z for the final pose.
    constexpr float FaceZStart       = 0.62f;

    // Camera roll (tilt around the view/forward axis = Dutch angle) while orbiting.
    // Oscillates and fades out near the end so the final pose is level.
    constexpr float CamRollAmpDeg = 10.0f;  // max tilt angle
    constexpr float CamRollFreq   = 1.8f;   // tilt sway speed (rad/sec) ≒ 1 cycle over GetTime

    // --- Final "decide" pose (within GetTime, after the orbit) ---
    // 0..OrbitPhaseEnd : orbit.  OrbitPhaseEnd..1 : the decide move.
    constexpr float OrbitPhaseEnd  = 0.55f;
    constexpr float CamMidDist     = 9.5f;    // distance at the end of the orbit
    constexpr float DecideTiltDeg  = -12.0f;  // pre-zoom tilt established during the orbit (negative = left)
    constexpr float DecideTiltSnapDeg = 14.0f; // opposite-side tilt swung to during the snap-zoom (positive = right)
    constexpr float DecidePullback = 3.0f;    // slight pull-back hump during the decide

    // After GetTime: pull the camera back while fading out.
    constexpr float FadePullbackDist = 16.0f;

    // --- Continuous camera timeline (seconds, m_clearTimer based) ---
    // One continuous distance curve (no stop at joints). Rotation only in the orbit window.
    constexpr float CamOrbitTime = 1.8f;   // yaw sweep ends here (rotation stops, zoom continues)
    // Orbit yaw overshoot (drift feel). Larger = more overshoot before settling.
    constexpr float CamOrbitOvershoot = 1.9f;
    constexpr float CamZoomEnd   = 2.7f;   // distance reaches EndDist (zoom finished)
    // Zoom overshoot (drift feel): punches slightly past, then settles. Larger = more drift.
    constexpr float CamZoomOvershoot = 1.3f;

    // --- Decide snap (the "kime"): during orbit stay loose, then SNAP in fast ---
    // How far the framing progresses during the orbit (0..1). Lower = bigger snap left for the decide.
    constexpr float CamMidFrac      = 0.55f;
    // Decide snap end time (fast). Orbit end (CamOrbitTime) -> here = the punch-in window.
    constexpr float CamDecideEnd    = 2.6f;
    // Decide snap overshoot (punch past then settle = recoil/hitstop feel). Larger = punchier.
    constexpr float CamDecideOvershoot = 1.1f;
    // Small white flash fired at the decide snap moment for extra impact.
    constexpr float DecideFlash     = 0.28f;

    constexpr float CamHoldEnd   = 3.3f;   // hold at EndDist until here
    constexpr float CamPullEnd   = 6.2f;   // pull back ends -> return to StageSelect
    // Iris close window: starts with the pull-back, finishes a bit before the cut
    // so it can hold full black for a moment (no half-open flash at the switch).
    constexpr float FadeBeginT   = 3.3f;   // = CamHoldEnd (close in sync with the pull-back)
    constexpr float FadeEndT     = 5.7f;   // fully closed here, then hold black until CamPullEnd

    // --- Iris mask transition (Mario-style closing circle) ---
    // Black texture with a transparent circular hole in the center.
    constexpr const char* IrisMaskPath = "Asset/Texture/Transition/IrisMask.png";
    // Mask quad size at fully-open (multiple of screen width). Big = scene fully visible.
    constexpr float IrisOpenScale  = 3.2f;
    // Mask quad size at fully-closed (pixels). 0 = closes to a point.
    constexpr float IrisCloseSize  = 0.0f;
}
