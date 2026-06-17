#pragma once

// Sparkle (star_01) effect constants shown around items.
namespace SparkleConst
{
    // Star texture
    inline constexpr const char* StarTexPath = "Asset/Effect/Star_01.png";
    // Glow particle texture (same one StarBlue uses) for a soft halo
    inline constexpr const char* GlowTexPath = "Asset/Effect/Particle03.png";

    // Pickup Effekseer effect (this engine's runtime loads .efk, relative to "Asset/Effect/")
    inline constexpr const char* PickupEfkPath   = "StarBurst.efk";
    constexpr float              PickupEfkScale   = 0.5f;
    // slight random color shift applied to the whole pickup effect each time (0 = none)
    constexpr float              PickupColorRandom = 0.15f;

    // Pickup burst (self-made CPU star burst, no Effekseer / fully colorable)
    constexpr int   PickupBurstCount   = 14;    // stars per burst (more = fuller)
    constexpr float PickupBurstLife    = 0.65f; // max star lifetime (burst duration)
    constexpr float PickupBurstLifeMin = 0.5f;  // min star lifetime (per-star random 0.5..0.65)
    constexpr float PickupBurstSpeed   = 8.0f;  // outward speed (units/sec) — spread wide
    constexpr float PickupBurstUpSpeed = 8.0f;  // upward speed (≒ outward so up reach ≒ side)
    constexpr float PickupBurstGravity = 4.0f;  // downward accel (lower = spreads more)
    constexpr float PickupBurstSize    = 1.0f;  // star size
    constexpr float PickupBurstSpin    = 6.0f;  // in-plane spin (rad/sec)

    // Central glow: one flash at the center that scales up and fades out
    constexpr float PickupGlowStartSize = 1.0f;
    constexpr float PickupGlowEndSize   = 6.5f;
    constexpr float PickupGlowAlpha     = 0.85f;  // initial alpha (fades to 0)
    constexpr float PickupGlowSpin      = 2.0f;   // slow spin (rad/sec)
    // Per-star slight random color shift (0 = none). Only the stars vary; glow stays base.
    constexpr float PickupStarColorShift = 0.18f;

    // Glow texture for the pickup burst (match StarBurst's look)
    inline constexpr const char* PickupGlowTexPath = "Asset/Effect/Particle04_bokashi_hard.png";
    // Central bright "core" flash (StarBurst uses Particle04_clear_hard)
    inline constexpr const char* PickupCoreTexPath = "Asset/Effect/Particle04_clear_hard.png";
    constexpr float PickupCoreSize     = 4.2f;   // core flash size (bright center)
    constexpr float PickupCoreInnerMul = 0.45f;  // extra pure-white inner spark (x core size)

    // ---- Mario-Galaxy style: light sucks in, then radial beams shoot out ----
    constexpr int   PickupConvergeCount  = 12;    // inward light streaks
    constexpr float PickupConvergeRadius = 4.5f;  // streak start distance
    constexpr float PickupConvergeTime   = 0.5f;  // life fraction when converge completes (flash)
    constexpr float PickupConvergeSize   = 0.55f; // streak size
    constexpr int   PickupBeamCountMin   = 6;     // radial beams: random count per pickup
    constexpr int   PickupBeamCountMax   = 8;
    constexpr float PickupBeamMaxLen     = 6.5f;  // long ray length
    constexpr float PickupBeamBaseWidth  = 0.15f; // width at the center (root)
    constexpr float PickupBeamTipWidth   = 1.0f;  // width at the tip (clearly flared)
    constexpr float PickupBeamSpin       = 0.6f;  // whole-fan slow rotation (rad/sec)
    constexpr float PickupBeamAngleJitter = 1.4f; // per-ray random angle offset (x spacing) = patchy
    constexpr float PickupBeamShortMul   = 0.45f; // shortest ray length factor (random length min)
    constexpr float PickupBeamWidthMin   = 0.5f;  // per-ray width multiplier (thin)
    constexpr float PickupBeamWidthMax   = 1.7f;  // per-ray width multiplier (thick)
    constexpr float PickupRayWhiten      = 0.65f; // blend ray/core color toward white (0..1)

    // ---- Afterglow (lingering halo after the burst / hit-stop ends) ----
    constexpr float PickupAfterglowLife  = 0.8f;  // seconds the afterglow lingers (unfrozen)
    constexpr float PickupAfterglowStart = 4.0f;  // halo start size
    constexpr float PickupAfterglowEnd   = 9.0f;  // halo end size (gently expands)
    constexpr float PickupAfterglowAlpha = 0.5f;  // initial alpha
    constexpr float PickupRainbowSpeed   = 1.2f;  // hue cycles per second (0 = static)
    constexpr float PickupRainbowSat     = 0.5f;  // 0 = white, 1 = full color (pastel rainbow)
    // Afterglow sparkle stars (Star_01) twinkling around the center
    constexpr int   PickupAfterStarCount  = 7;
    constexpr float PickupAfterStarRadius = 2.6f; // spread around center
    constexpr float PickupAfterStarSize   = 0.7f;
    constexpr float PickupAfterStarTwinkle = 12.0f; // blink speed (rad/sec)
    // Beam texture (light shaft, tapers to a point)
    inline constexpr const char* HakumeiBeamTexPath = "Asset/Effect/HakumeiBeamLight.png";
    constexpr float PickupBeamTipFreq    = 11.0f; // subtle length pulse speed (rad/sec)
    constexpr float PickupCoreLifeFrac = 0.4f;   // visible during first 40% of life
    // Flicker (chika-chika) while the stars fade out
    constexpr float PickupFlickerStart = 0.45f;   // begin flicker after this life fraction (0..1)
    constexpr float PickupFlickerFreq  = 34.0f;   // blink speed (rad/sec)
    constexpr float PickupFlickerLow   = 0.15f;   // dim level during the off-phase

    // ---- Calm pickup (落ち着いた取得演出：岩石など) ----
    // ビーム/フラッシュ/虹なし。やわらかいグロー＋小さな星がふわっと上昇して消える。
    constexpr float CalmPickupLife   = 0.6f;   // 全体の寿命（秒）
    constexpr float CalmGlowStart    = 0.5f;   // グロー開始サイズ
    constexpr float CalmGlowEnd      = 2.0f;   // グロー終了サイズ（ゆるく広がる）
    constexpr float CalmGlowAlpha    = 0.7f;   // グロー初期アルファ（明るめ）
    constexpr float CalmGlowSpin     = 0.6f;   // グローのゆっくり回転
    constexpr int   CalmStarCount    = 12;     // 小さな星の数（キラキラ多め）
    constexpr float CalmStarSize     = 0.6f;   // 星サイズ
    constexpr float CalmStarSpread   = 0.85f;  // 横の散らばり
    constexpr float CalmStarRise     = 1.5f;   // 上昇量
    constexpr float CalmStarTwinkle  = 18.0f;  // またたき速度（速め＝チカチカ感）
    constexpr float CalmStarWhiten   = 0.5f;   // 星を白寄りにブレンド（0=元色, 1=白）
    // 立ち上がりの白いポップ（元気さ用）
    constexpr float CalmCoreSize     = 2.2f;   // 中心の白フラッシュのサイズ
    constexpr float CalmPopFrac      = 0.35f;  // 寿命の前半この割合だけポップが出る

    // ---- Ring pickup (Object_Ring_Glow 風：中央からリング状に広がる＋星が放物線で落ちる) ----
    inline constexpr const char* RingTexPath = "Asset/Effect/Particle02.png"; // リング画像
    constexpr float RingPickupLife   = 0.9f;   // 全体の寿命（秒）
    constexpr float RingGlowStart    = 0.4f;   // リング開始サイズ
    constexpr float RingGlowEnd      = 5.5f;   // リング終了サイズ（まぁまぁな速度で広がる）
    constexpr float RingGlowAlpha    = 0.5f;   // リング初期アルファ（0.5 から減衰）
    constexpr float RingGlowSpin     = 1.0f;   // リングの回転
    constexpr float RingFadeFrac     = 0.4f;   // 寿命のこの割合でリングは消える（速く消す）
    constexpr float RingCoreSize     = 2.6f;   // 中央の初期フラッシュ
    constexpr float RingCoreFrac     = 0.3f;   // 寿命の前半この割合だけ中央が光る
    constexpr int   RingStarCount    = 11;     // 飛び散る星の数
    constexpr float RingStarSize     = 0.55f;  // 星サイズ
    constexpr float RingStarOut      = 2.8f;   // 横の飛び出し速度
    constexpr float RingStarUp       = 5.5f;   // 上方向の初速
    constexpr float RingStarGravity  = 9.0f;   // 落下加速度（上に飛んで徐々に落ちる）
    constexpr float RingStarSat      = 0.45f;  // 星の彩度（0=白, 1=ビビッド）→ 淡いパステル
    constexpr float RingStarTwinkle  = 14.0f;  // またたき速度

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

    // Pickup burst spawns at the player's body center (offset up from the hitbox center/feet)
    constexpr float PickupSpawnOffsetY = 1.0f;

    // ---- Pickup player glow (additive bloom on the player while a pickup plays) ----
    constexpr float PickupGlowDuration   = 0.9f;  // seconds the player stays lit (>= hit-stop so it lingers)
    constexpr float PickupGlowIntensity  = 2.2f;  // max additive brightness (colRate multiplier)
    constexpr float PickupGlowShimmer    = 18.0f; // shimmer speed (rad/sec)
    constexpr float PickupGlowShimmerAmp = 0.25f; // shimmer amplitude (0..1)

    // Parasol: bluish
    constexpr float ParasolStarSize   = 0.45f;
    constexpr float ParasolStarRadius = 0.55f;
    constexpr float ParasolColorShift = 0.4f;
    constexpr float ParasolColorR     = 0.6f;
    constexpr float ParasolColorG     = 0.85f;
    constexpr float ParasolColorB     = 1.0f;
    constexpr float ParasolColorA     = 1.0f;
}
