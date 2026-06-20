#pragma once

// Corelia hint NPC constants
namespace CoreliaConst
{
    // Model (player model as placeholder). Tinted + smaller to read as a Toad-like helper.
    constexpr const char* ModelPath   = "Asset/Data/Player.gltf";
    constexpr float       ModelScale   = 0.3f;   // = PlayerConst::ModelScale
    constexpr float       ModelOffsetY = 0.0f;   // along up, nudge after ground-snap

    // Body tint (so it doesn't look like a player clone). Soft Corelia cyan.
    constexpr float TintR = 0.55f;
    constexpr float TintG = 0.95f;
    constexpr float TintB = 1.00f;

    // Ground snapping (raycast down along -up to the floor)
    constexpr float GroundRayUp  = 2.0f;     // start the ray this far above Pos
    constexpr float GroundRayLen = 300.0f;   // max distance to search for floor

    // Idle float / spin
    constexpr float BobSpeed = 2.2f;
    constexpr float BobAmp   = 0.0f;    // no bob (stand still)
    constexpr float SpinSpeed = 0.0f;   // no spin
    constexpr float FloatUp   = 0.0f;   // sit on the ground

    // Interaction
    constexpr float InteractRadius = 3.5f;   // player must be within this to talk

    // ── 追尾（頭が先に回り、首の限界を超えたら体が回る）──
    constexpr float NeckLimitDeg = 60.0f;   // 首だけで向ける最大角(度)
    constexpr float HeadTurnLerp = 0.30f;   // 頭の追従の速さ(0-1)
    constexpr float BodyTurnLerp = 0.10f;   // 体の追従の速さ(0-1。頭より遅い)
    // 頭ボーン候補名（最初に見つかったものを回す）
    constexpr const char* HeadBoneNames[] = { "Head", "head", "mixamorig:Head", "Neck", "neck", "atama" };

    // 非表示にするノード名（手持ち武器・背中の剣・パラソルなど。NPCには持たせない）
    constexpr const char* HiddenNodeNames[] = { "BackSword", "HandledSword", "ClosedParasol", "OpenedParasol" };

    // CSV (position + hintId)
    constexpr const char* SavePath = "Asset/Data/Corelias.csv";

    // Hint body text file (one hint per line, must be saved as Shift-JIS)
    constexpr const char* HintsPath = "Asset/Data/CoreliaHints.txt";

    // Font (Japanese capable face)
    constexpr int         FontNo     = 2;
    constexpr const char* FontName   = "Meiryo";
    constexpr int         FontHeight = 28;

    // ---- Conversation camera ----
    constexpr float ConvoFocusToNpc = 0.55f;  // lerp player->npc for camera focus
    constexpr float ConvoZoomZMul   = 0.55f;  // offsetZ * this = closer (zoom in)
    constexpr float ConvoZoomLerp   = 0.12f;  // zoom in/out smoothing per frame

    // ---- Dialogue box (sprite, center-origin, +Y up) ----
    constexpr float BoxWidth   = 760.0f;
    constexpr float BoxHeight  = 180.0f;
    constexpr float BoxMargin  = 60.0f;   // from bottom of screen
    constexpr float TextPadX   = 40.0f;
    constexpr float TextPadY   = 36.0f;
    constexpr float NameOffsetY = 20.0f;  // name tag above the box
    constexpr int   WrapChars  = 28;      // wrap hint to this many (full-width) chars per line
    constexpr float LineHeight = 38.0f;

    // Box colors (RGBA)
    constexpr float BoxR = 0.05f, BoxG = 0.10f, BoxB = 0.18f, BoxA = 0.85f;
    constexpr float EdgeR = 0.55f, EdgeG = 0.92f, EdgeB = 1.0f, EdgeA = 0.9f;

    // Speaker name (ASCII to avoid encoding issues; rendered via Japanese font is fine too)
    constexpr const char* SpeakerName = "Corelia";
    constexpr const char* ClosePrompt = "W : CLOSE";
}
