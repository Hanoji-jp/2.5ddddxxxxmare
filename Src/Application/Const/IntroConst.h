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
    constexpr float Duration     = 4.5f;

    // Progress (0..1) where the player starts righting itself (head-first -> head-up).
    constexpr float UprightStart = 0.72f;

    // Tumble roll speed (radians / sec).
    constexpr float SpinSpeed    = 9.0f;

    // Ignore ground detection for this long after start (avoid instant landing).
    constexpr float MinTime      = 0.3f;

    // Safety timeout: end the cutscene even if never grounded (e.g. no planet yet).
    constexpr float MaxTime      = 8.0f;

    // White flash strength on landing.
    constexpr float LandFlash    = 0.5f;

    // Landing reaction ("zusaa... batan"): skid then thud.
    constexpr float LandRecoverTime = 0.55f;  // recovery time before control returns
    constexpr float SkidDist        = 7.0f;   // how far the player slides on touchdown
    constexpr float LandShake       = 0.45f;  // camera shake strength on touchdown

    // 着地時の box パーティクル爆発（飛んできた着地は派手に）。StarBurstEffect の倍率。
    constexpr float LandBurstCountMul = 3.0f;  // 粒数の倍率（多め）
    constexpr float LandBurstScaleMul = 2.2f;  // 大きさ・飛散の倍率（大きめ）

    // Cutscene camera (Mario-Galaxy style): starts wide / high / angled, then
    // orbits + zooms in as the player descends, and converges to the normal
    // gameplay camera pose at the end (seamless handoff).
    constexpr float CamStartYawDeg = 42.0f;   // start orbit angle around Y (0 = side view)
    constexpr float CamStartDist   = 35.0f;   // start distance below the player (not too far)
    constexpr float CamStartHeight = -22.0f;  // start BELOW the player (looks up at it)
    constexpr float CamStartLookUp = 0.0f;    // look straight at the player at the start

    // Distance choreography: far -> rush in very close -> pull back for the crash landing.
    constexpr float CamCloseDist    = 7.0f;   // closest distance (head-down dive)

    // "About to land" pose: peek from ABOVE looking down, eased to a stop.
    constexpr float CamAboveHeight  = 18.0f;  // eye height above the player
    constexpr float CamAboveBack    = 8.0f;   // eye offset toward -Z (slightly in front)
    constexpr float CamPullBackStart = 0.62f; // progress (0..1) where the pull-back begins
    // (the pull-back ends at the normal gameplay distance = |CameraSettings.OffsetZ|)

    // --- "Welcome to <stage>" banner shown after the landing (Mario-Galaxy style) ---
    constexpr int   ArrivalFontNo = 9;     // dedicated font slot
    constexpr int   ArrivalFontH  = 78;    // big
    constexpr float ArrivalShowTime = 3.2f;  // total visible time (sec)
    constexpr float ArrivalFadeTime = 0.5f;  // fade-in / fade-out duration (sec)
    constexpr float ArrivalYRatio   = 0.30f; // center Y near the top (fraction of half-screen up from center)
    constexpr int   ArrivalOutlinePx = 3;
    constexpr float ArrivalR = 1.0f,  ArrivalG = 0.9f,  ArrivalB = 0.35f; // gold
    constexpr float ArrivalOutR = 0.1f, ArrivalOutG = 0.05f, ArrivalOutB = 0.0f;

    // --- ステージ1の着地時、ステージ名の下に出す操作説明 ---
    constexpr const char* ControlHintText  = "ＡとＤで いどう　　スペースで ジャンプ";
    constexpr int         ControlHintFontNo = 3;     // 既存の日本語フォント(PromptFontNo相当)を流用
    constexpr float       ControlHintGap    = 86.0f; // ステージ名の中心から下へのオフセット(px)
    constexpr int         ControlHintOutlinePx = 2;
    constexpr float       ControlHintR = 1.0f, ControlHintG = 1.0f, ControlHintB = 1.0f; // 白文字

    // --- Falling trail (connected camera-facing ribbon = comet streak) ---
    constexpr int   TrailLen      = 24;     // history points (length of the tail)
    constexpr int   TrailSubdiv   = 6;      // interpolated points between history points (smooths the line)
    constexpr float TrailWidth    = 1.1f;   // ribbon half-width near the head (world units)
    constexpr float TrailTailMul  = 0.15f;  // width multiplier at the tail (taper)
    constexpr float TrailAlpha    = 0.7f;   // additive alpha at the bright center/head
    constexpr float TrailColR = 0.55f, TrailColG = 0.8f, TrailColB = 1.0f; // bluish comet
    constexpr float TrailCoreWidthMul = 0.35f; // inner bright core width vs TrailWidth
    constexpr float TrailCoreR = 0.9f, TrailCoreG = 0.97f, TrailCoreB = 1.0f; // near-white core
    // head attach: push the head toward the player body along its up so the tail
    // visibly comes out of the player (not detached from the feet).
    constexpr float TrailAttachUp = 1.0f;   // world units up (toward body center)
    // Glow laid over the player's feet to hide the trail-to-player seam.
    constexpr const char* TrailFootGlowTex = "Asset/Texture/Glow.png";
    constexpr float TrailFootGlowSize  = 3.2f;  // glow size (covers the seam)
    constexpr float TrailFootGlowAlpha = 0.9f;  // glow alpha
    constexpr float TrailFootGlowUp    = 0.9f;  // push from the Body bone toward the body center (along up)
}
