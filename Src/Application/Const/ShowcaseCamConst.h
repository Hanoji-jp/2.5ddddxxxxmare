#pragma once
// ShowcaseCamConst.h  (ASCII comments only: this file is BOM-less)
//==========================================================
// Stage-showcase camera (Mario-Galaxy style fly-through shown when entering
// a stage, BEFORE the intro fall). Authored in the editor as ordered phases
// (camerafaze1,2,3...). Each phase is a spline path of camera (eye) points and
// a matching spline of look-at points, with its own duration + easing.
//==========================================================
namespace ShowcaseCamConst
{
    // per-stage CSV (resolved through StageManager::ResolvePath)
    constexpr const char* SaveName = "showcase_cam.csv";

    // defaults for a new phase / point
    constexpr float DefaultDuration = 3.0f;   // seconds
    constexpr float DefaultEyeY     = 8.0f;   // spawn height for a fresh eye point
    constexpr float DefaultLookY    = 1.0f;

    // spline sampling for the debug wire (segments drawn between control points)
    constexpr int   WireSegsPerSpan = 14;

    // debug wire colors
    constexpr float EyeColR = 0.3f, EyeColG = 0.9f, EyeColB = 1.0f;   // cyan = eye path
    constexpr float LookColR = 1.0f, LookColG = 0.8f, LookColB = 0.2f; // gold = look path
    constexpr float LinkColR = 0.6f, LinkColG = 0.6f, LinkColB = 0.6f; // gray = eye->look links
    constexpr float PointRadius = 0.4f;       // control point ghost sphere radius
    constexpr int   LinkEveryNthSample = 4;   // draw an eye->look link every Nth sample

    // playback
    constexpr float MinDuration = 0.05f;      // clamp so we never divide by ~0

    // cinematic letterbox bars (top & bottom) + close/open transition
    constexpr float LetterboxFrac = 0.12f;    // bar height during the fly-through (fraction of screen height)
    constexpr float BarInSpeed    = 0.6f;     // bars slide in to the letterbox amount (per second)
    constexpr float CloseSpeed    = 1.4f;     // bars close toward the center to full black (per second)
    constexpr float OpenSpeed     = 1.2f;     // bars open back out to reveal the game (per second)
}
