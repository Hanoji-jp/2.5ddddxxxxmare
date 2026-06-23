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

    // ── 重力コア(Rock)取得カウンター（コインの下。アイコンは DrawTriangle で岩を2D描画）──
    constexpr int   CoreIconSize     = 38;    // 岩アイコンの直径(px)
    constexpr float CoreMargin       = 18.0f; // 画面端からの距離(px)
    constexpr float CoreGapBelowCoin = 14.0f; // コインカウンターの下に空ける距離(px)
    constexpr float CorePopTime      = 0.3f;  // 取得時の拡大ポップ時間(秒)
    constexpr float CorePopScale     = 0.5f;  // ポップ時の最大拡大率
    constexpr float CoreSpinSpeed    = 1.0f;  // アイコン自転(ラジアン/秒)

    // ── 低HP時の黒ビネット（画面端が暗くなる。被ダメ赤フラッシュの代わり）──
    constexpr const char* LowHpVignetteTex = "Asset/Texture/Vignette.png"; // 黒枠・中央透明のPNG
    constexpr float LowHpThreshold   = 0.45f;  // HP割合がこれ以下で出始める
    constexpr float LowHpVignetteMaxA = 0.65f; // 最大の濃さ（HP0付近）
    constexpr float LowHpPulseSpeed  = 4.5f;   // 鼓動の速さ
    constexpr float LowHpPulseAmp    = 0.15f;  // 鼓動でゆれる濃さ幅

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

    // ── コインアイコンを 3Dモデル(coin.gltf)で表示する設定 ──
    //    3Dモデルを専用RTへ描画し、その結果テクスチャをUIへスプライト貼りする。
    constexpr int   Coin3DRTSize    = 256;    // オフスクリーンRTの一辺(px。大きいほど高精細)
    constexpr float Coin3DFovDeg    = 35.0f;  // プレビューカメラの画角(度)
    constexpr float Coin3DCamDist   = 4.0f;   // カメラからコインまでの距離(モデルが切れる/小さい時に調整)
    constexpr float Coin3DScale     = 1.0f;   // コインモデルの拡大率(モデルが大きすぎ/小さすぎる時に調整)
    constexpr float Coin3DSpinSpeed = 2.0f;   // 自転速度(rad/秒。Y軸回り)
    constexpr float Coin3DTiltDeg   = 18.0f;  // 少し傾けて立体感を出す(度。X軸回り)
    constexpr bool  Coin3DLit       = true;   // true=陰影あり(Lit) / false=フラット(UnLit)
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

