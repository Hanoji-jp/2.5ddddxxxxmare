#pragma once
// Stage-clear result shown on StageSelect: zoom the camera onto the player
// (the marker standing on the cleared box) and pop a panel beside it.
// Layout: top gold banner ("STAGE N CLEAR!") + three stat cards (TIME/COINS/DEATHS) + ENTER.
namespace ResultConst
{
    // --- Camera zoom (overview <-> closeup of the marker) ---
    constexpr float ZoomSpeed   = 2.2f;    // per second
    constexpr float CamEyeOffX  = 4.8f;    // closeup eye offset from the marker
    constexpr float CamEyeOffY  = 4.2f;
    constexpr float CamEyeOffZ  = -10.4f;
    constexpr float CamFocusUp  = 1.8f;    // look slightly above the marker base
    // Pan the camera to the right (added to both eye & target X) so the player
    // sits on the LEFT of the frame, leaving room for a wider window on the right.
    constexpr float CamPanX     = 4.5f;

    // --- Panel placement (screen pixels; center-origin, +Y up) ---
    constexpr float AnchorWorldUp = 1.6f;  // world height above marker used to anchor the panel
    constexpr int   PanelW        = 460;   // ~4:3 (W:H)
    constexpr int   PanelH        = 345;
    constexpr int   PanelOffsetX  = 420;   // panel center to the right of the marker
    constexpr int   ScreenMargin  = 20;    // keep the panel inside the screen by this much

    // --- Banner (top strip) ---
    constexpr float BannerH = 58.0f;

    // --- Cards (three side-by-side) ---
    constexpr float SidePad    = 18.0f;    // panel inner side padding
    constexpr float CardGap    = 12.0f;    // gap between cards
    constexpr float CardVPad   = 14.0f;    // card vertical padding inside the card area
    constexpr float FooterH    = 42.0f;    // ENTER prompt area at the bottom
    constexpr float LabelInset = 24.0f;    // label center down from card top
    constexpr float ValueDrop  = 12.0f;    // value center below card center

    // --- Colors (RGBA 0..1) ---
    constexpr float PanelR = 0.05f, PanelG = 0.07f, PanelB = 0.12f, PanelA = 0.88f;
    constexpr float EdgeR  = 1.0f,  EdgeG  = 0.85f, EdgeB  = 0.35f, EdgeA  = 0.95f; // gold edge/banner
    constexpr float TitleR = 0.05f, TitleG = 0.06f, TitleB = 0.10f;                 // banner text (dark on gold)
    constexpr float CardR  = 0.10f, CardG  = 0.13f, CardB  = 0.20f, CardA  = 0.95f; // card body
    constexpr float CardEdgeA = 0.45f;                                              // card edge (gold, faint)
    constexpr float LabelR = 1.0f,  LabelG = 0.85f, LabelB = 0.4f;                  // card label (gold-ish)
    constexpr float ValueR = 1.0f,  ValueG = 1.0f,  ValueB = 1.0f;                  // card value (white)
    constexpr int   EdgeThickness = 3;
    constexpr int   CardEdgeThickness = 2;

    // --- Content swap pop (the single content card scales up on each step) ---
    constexpr float CardPopTime      = 0.26f;  // pop duration (sec)
    constexpr float CardPopOvershoot = 1.7f;   // easeOutBack overshoot
    constexpr float CardPopStart     = 0.55f;  // start scale (0..1)

    // --- Progress dots (which element, 1/3) shown under the banner ---
    constexpr float DotRowH = 28.0f;
    constexpr float DotSize = 9.0f;    // half-extent of each dot square
    constexpr float DotGap  = 30.0f;   // spacing between dot centers
    constexpr float DotOnA  = 1.0f;    // filled (current/done) alpha
    constexpr float DotOffR = 0.4f, DotOffG = 0.45f, DotOffB = 0.55f, DotOffA = 0.6f;

    // --- Text ---
    constexpr const char* ClearWord  = "クリア！";
    constexpr const char* TimeLabel  = "タイム";
    constexpr const char* CoinLabel  = "コイン";
    constexpr const char* RockLabel  = "いわ";
    constexpr const char* CoreLabel  = "じゅうりょくコア";
    constexpr const char* DeathLabel = "ミス";
    constexpr const char* NextHint   = "ENTER：つぎへ";
    constexpr const char* CloseHint  = "ENTER：とじる";

    // --- Pages (1:message 2:time 3:coins 4:rocks 5:gravity core) ---
    constexpr int Pages = 5;

    // --- Icons ---
    constexpr const char* CoinIconPath = "Asset/Effect/Star_01.png";
    constexpr const char* RockIconPath = "Asset/Texture/RockIco/RockIco.png";
    constexpr int   PageIconSize = 56;     // icon size on the coins/rocks pages (half not applied)

    // --- Totals HUD (top-left) ---
    constexpr float HudMargin   = 28.0f;   // from screen corner
    constexpr int   HudIconSize = 44;
    constexpr float HudRowGap   = 52.0f;   // vertical gap between coin row and rock row
    constexpr float HudTextGap  = 12.0f;   // icon to number
    constexpr int   HudFontNo   = 6;       // reuse FontSmallNo size
    constexpr float HudPopTime  = 0.18f;   // total pop on add
    constexpr float HudPopScale = 0.5f;

    // --- Tally (fly from player to the HUD icons after the result) ---
    constexpr int   TallyMaxPerKind = 12;   // cap of flyers per kind
    constexpr float TallyFlyTime    = 0.7f;  // per-flyer travel time
    constexpr float TallyStagger    = 0.06f; // delay between consecutive flyers
    constexpr float TallyArcUp      = 160.0f;// upward arc height (px)
    constexpr float TallySpread     = 60.0f; // random scatter around the player start
    constexpr int   TallyFlySize    = 36;    // flyer icon size

    // --- Fonts (dedicated slots so sizes differ from other UI) ---
    constexpr int FontBigNo   = 5;   // banner title + the time value
    constexpr int FontBigH    = 54;
    constexpr int FontSmallNo = 6;   // labels + ENTER hint
    constexpr int FontSmallH  = 30;
    constexpr int FontMidNo   = 7;   // message lines on page 1
    constexpr int FontMidH    = 34;
}
