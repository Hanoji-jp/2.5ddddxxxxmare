#pragma once

// UI に関わる定数
namespace UIConst
{
    // HPアイコンの起点座標
    constexpr float HpIconStartX   = 20.0f;
    constexpr float HpIconStartY   = 20.0f;

    // HPアイコン間隔
    constexpr float HpIconSpacing  = 40.0f;

    // HPアイコンサイズ
    constexpr float HpIconSize     = 32.0f;

    // フェードアルファの変化量（1フレームあたり）
    constexpr float FadeSpeed      = 0.02f;

    // ── コアリア残機アイコン（死亡後の暗転画面に中央表示。マリギャラ風）──
    // ※ 仮画像。後で「顔だけ」版に差し替えるときは LifeIconPath を変えるだけ。
    constexpr const char* LifeIconPath    = "Asset/Texture/CoreliaLifeIco/LifeIco.png";
    constexpr int         LifeIconSize    = 72;    // アイコンの一辺(px)
    constexpr float       LifeIconSpacing = 88.0f; // アイコン間隔(px)
    constexpr float       LifeIconCenterY = 0.0f;  // 縦位置(中心原点・+Yが上。0=画面中央)
    constexpr const char* LifeStarPath    = "Asset/Effect/Star_01.png"; // 減る瞬間の星屑テクスチャ

    // ── コイン（収集物）カウンター：右上にアイコン＋数字 ──
    // ※ 仮アイコン。後でコイン画像に差し替えるときは CoinIconPath を変えるだけ。
    constexpr const char* CoinIconPath = "Asset/Effect/Star_01.png";
    constexpr int         CoinIconSize = 48;     // アイコンの一辺(px)
    constexpr float       CoinMargin   = 30.0f;  // 画面端からの余白(px)
    constexpr float       CoinTextGap  = 6.0f;   // アイコンと数字の間隔(px)
    constexpr float       CoinPopTime  = 0.25f;  // 取得時の拡大演出の長さ(秒)
    constexpr float       CoinPopScale = 0.5f;   // 取得時の追加拡大量

    // ── HP：重力コア(2D)アイコン1個で完結（下から HP ぶん満ちるエネルギー球）──
    // ※ 仮アイコンは既存コアのグロー画像。本番の「2D重力コア」画像ができたら HpStarPath を差し替え。
    constexpr const char* HpStarPath  = "Asset/Effect/Particle04_bokashi_hard.png";
    constexpr int         HpStarSize  = 84;     // コアの大きさ(px)
    constexpr float       HpMarginX   = 38.0f;  // 左端からの余白(px)
    constexpr float       HpMarginY   = 34.0f;  // 上端からの余白(px)
    constexpr float       HpShakeTime = 0.3f;   // 被ダメ時の揺れの長さ(秒)
    constexpr float       HpShakeAmp  = 10.0f;  // 揺れ幅(px)

    // ── 重力コンパス（左下）：ダイヤル＋方向マーカー（回転不要）──
    constexpr float GravCompassMarginX = 50.0f; // 左端からの余白(px)
    constexpr float GravCompassMarginY = 50.0f; // 下端からの余白(px)
    constexpr float GravCompassRadius  = 40.0f; // ダイヤル半径(px)
    constexpr int   GravCompassDots    = 12;    // ダイヤルの目盛り点数
    constexpr int   GravCompassDotSize = 6;     // 目盛り点サイズ(px)
    constexpr int   GravCompassMark    = 30;    // 方向マーカー（星）サイズ(px)
    constexpr int   GravCompassCenter  = 14;    // 中心ドットサイズ(px。色で切替可否)
}

