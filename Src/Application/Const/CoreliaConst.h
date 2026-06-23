#pragma once
#include "FontConst.h"

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
    // Ghost look: drawn translucent (dead spirit). 0=invisible, 1=opaque.
    constexpr float GhostAlpha = 0.45f;

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

    // 1つのヒント内のページ区切りトークン（W で次ページへ。会話バブル内で複数ページに分ける）
    constexpr const char* PageSep = "<P>";

    // Font (Japanese capable face)
    constexpr int         FontNo     = 2;
    constexpr const char* FontName   = FontConst::GameFontName;
    constexpr int         FontHeight = 48;

    // Small font for the "talk" prompt bubble above the NPC
    // (font slots are 0..9; use a free one)
    constexpr int         PromptFontNo = 3;
    constexpr int         PromptFontH  = 40;

    // 吹き出し配置（すべてNPCのUp方向＝nupを基準にする）
    constexpr float PromptHeadH    = 2.2f;   // 接地点からUp方向の頭の高さ(ワールド)
    constexpr float PromptFootH    = 0.6f;   // 接地点からUp方向の体下部の高さ(向き算出用)
    constexpr float PromptPadX     = 14.0f;  // 文字幅に足す左右パディング(px)
    constexpr float PromptPadY     = 8.0f;   // 文字高に足す上下パディング(px)
    constexpr float PromptGap      = 40.0f;  // 頭と吹き出しの隙間(px)
    constexpr int   PromptTailHalfW = 9;     // 尻尾の根元の半幅(px)
    constexpr float PromptMaxTail  = 17.0f;  // 尻尾の最大長(px)
    constexpr int   PromptCornerSegs = 8;    // 角丸の分割数

    // ---- Conversation camera ----
    constexpr float ConvoFocusToNpc = 0.55f;  // lerp player->npc for camera focus
    constexpr float ConvoZoomZMul   = 0.55f;  // offsetZ * this = closer (zoom in)
    constexpr float ConvoZoomLerp   = 0.12f;  // zoom in/out smoothing per frame

    // ---- Dialogue box (sprite, center-origin, +Y up) ----
    constexpr float BoxWidth   = 820.0f;
    constexpr float BoxHeight  = 230.0f;
    constexpr float BoxMargin  = 60.0f;   // from bottom of screen
    constexpr float TextPadX   = 40.0f;
    constexpr float TextPadY   = 64.0f;
    constexpr float NameOffsetY = 20.0f;  // name tag above the box
    constexpr int   WrapChars  = 14;      // wrap hint to this many (full-width) chars per line
    constexpr float LineHeight = 58.0f;

    // Text outline (drawn by offsetting a dark copy in 8 directions)
    constexpr int   OutlinePx = 2;        // outline thickness (pixels)
    constexpr float OutR = 0.0f, OutG = 0.0f, OutB = 0.0f, OutA = 0.9f;

    // Drop shadow (offset toward bottom-right; center-origin +Y up so down = -Y)
    constexpr float ShadowOffX = 5.0f;    // to the right
    constexpr float ShadowOffY = 5.0f;    // downward (applied as -Y)
    constexpr float ShadowR = 0.0f, ShadowG = 0.0f, ShadowB = 0.0f, ShadowA = 0.5f;

    // Box colors (RGBA) … StageSelectのウィンドウに合わせる：紺の本体＋薄い金の縁
    constexpr float BoxR = 0.10f, BoxG = 0.13f, BoxB = 0.20f, BoxA = 0.95f;   // 本体(紺)
    constexpr float EdgeR = 1.00f, EdgeG = 0.85f, EdgeB = 0.35f, EdgeA = 0.95f; // 金(名前文字・アクセント用)

    // 角丸パネルのデザイン
    constexpr float BoxRadius        = 20.0f;  // 角丸半径(px)
    constexpr int   BoxCornerSegs    = 8;      // 角丸の分割数
    constexpr int   BoxEdgeThickness = 2;      // 金縁の太さ(px)
    constexpr float BoxEdgeA         = 0.45f;  // 縁枠の薄い金のアルファ
    constexpr float BoxShadowA       = 0.5f;   // 影の濃さ

    // Speaker name (ASCII to avoid encoding issues; rendered via Japanese font is fine too)
    constexpr const char* SpeakerName = "Corelia";
    constexpr const char* ClosePrompt = "W : CLOSE";

    // 送り（次へ）マーク：LifeIcon.png を会話ボックス右下で上下に揺らして出す
    constexpr int   NextIconSize     = 40;     // マークのサイズ(px)
    constexpr float NextIconMarginX  = 46.0f;  // ボックス右端からマーク中心までの距離(px)
    constexpr float NextIconMarginY  = 36.0f;  // ボックス下端からマーク中心までの距離(px)
    constexpr float NextIconBobAmp   = 6.0f;   // 上下の揺れ幅(px)
    constexpr float NextIconBobSpeed = 6.0f;   // 揺れの速さ
}
