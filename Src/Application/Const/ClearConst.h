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

    // Camera FOV during the clear sequence (lower = telephoto / compressed = more dramatic).
    // Normal game FOV is 60.
    constexpr float CamFov = 38.0f;

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
    constexpr float DecideTiltDeg  = -6.0f;   // tilt while swaying left (negative = left)
    constexpr float DecideTiltSnapDeg = 7.0f; // tilt while swaying right (positive = right)
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
    // Decide snap end time. Orbit end (CamOrbitTime) -> here = the drift/zoom-in window.
    // Quick drift+settle so the following "stop" pause reads clearly.
    constexpr float CamDecideEnd    = 2.7f;
    // Decide snap overshoot (punch past then settle = recoil/hitstop feel). Larger = punchier.
    constexpr float CamDecideOvershoot = 1.1f;
    // Small white flash fired at the decide snap moment for extra impact.
    constexpr float DecideFlash     = 0.28f;

    // Reverse drift = a pendulum (damped sine) that carries the orbit drift's momentum.
    // After the orbit drift, the yaw swings to the opposite side, overshoots back,
    // smaller, ... decaying to center. A sine never dwells at the turn = no choppiness.
    constexpr float CamPendAmp   = 22.0f;  // pendulum amplitude (deg) at the start
    constexpr float CamPendOmega = 5.2f;   // angular freq (rad/s). Bigger = faster swings
    constexpr float CamPendDecay = 1.6f;   // decay rate (bigger = settles faster / fewer swings)
    constexpr float CamYawLeanScale = 0.5f; // roll lean per degree of swing offset
    constexpr float CamYawLeanMax   = 10.0f; // max roll lean (deg)

    constexpr float CamSwayEnd   = 3.0f;   // (timing ref for the zoom start)
    // Lateral positional slide for the drift (world units). The camera trucks sideways
    // with inertia (left -> right -> center), so the player slides across the frame.
    constexpr float CamDriftDist      = 4.5f;
    // Inertia overshoot for each drift segment (easeOutBack): slides past then settles.
    constexpr float CamDriftOvershoot = 1.6f;

    // Kime zoom: right after the orbit drift, push in quickly (eased).
    constexpr float CamZoomInEnd  = 2.4f;  // zoom-in ends here (starts at CamOrbitTime)
    constexpr float CamZoomInDist = 5.0f;  // distance at the zoom-in peak (smaller = closer)

    // Stop: hold still for ~1 second after the kime zoom (the beat before the pull).
    constexpr float CamStopEnd   = 3.4f;   // = CamZoomInEnd + 1.0s

    // Pull back: the camera keeps pulling continuously the whole time (Mario-Galaxy style),
    // never stops until the cut. CamPullEnd is the scene cut.
    constexpr float CamPullEnd   = 6.5f;   // pull spans [CamStopEnd, CamPullEnd], then cut

    // Iris: starts when the pull starts, closes over ~3s while the camera keeps pulling.
    constexpr float FadeBeginT   = 3.4f;   // = CamStopEnd (iris starts with the pull)
    constexpr float FadeEndT     = 6.4f;   // ~3s transition; camera still pulling until here

    // --- Iris mask transition (Mario-style closing circle) ---
    // Black texture with a transparent circular hole in the center.
    constexpr const char* IrisMaskPath = "Asset/Texture/Transition/IrisMask.png";
    // Mask quad size at fully-open (multiple of screen width). ~1.3 = circle just covers
    // the screen at the start, so the close is visible immediately (not a delayed shrink).
    constexpr float IrisOpenScale  = 1.35f;
    // Mask quad size at fully-closed (pixels). 0 = closes to a point.
    constexpr float IrisCloseSize  = 0.0f;
}
