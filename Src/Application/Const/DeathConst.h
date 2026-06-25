#pragma once

//==========================================================
// DeathConst.h
// プレイヤー死亡時の演出（カメラシェイク＋画面エフェクト＋復活）用の定数
//==========================================================
namespace DeathConst
{
	constexpr float FadeOutTime = 1.6f;   // 暗転（赤→黒）＆カメラ寄りの時間（秒）
	constexpr float HoldTime    = 2.6f;   // 暗転しきった黒画面で残機が1つ減るのを見せる間（秒）
	constexpr float FadeInTime  = 1.1f;   // 復活後の明転の時間（秒）

	// アイコンの出入り（暗幕とは別。黒くなりきってから出て、明転前に先に消す）
	constexpr float IconAppearTime = 0.45f; // 黒画面になってからアイコンが現れるまで（秒）
	constexpr float IconVanishTime = 0.5f;  // 明転が始まる前にアイコンが消えるまで（秒）

	// 残機が1つ消えるアニメ（HoldTime の中で再生）
	constexpr float LoseAnimDelay = 0.8f;  // 黒画面になってから減り始めるまでの溜め（秒）
	constexpr float LoseAnimTime  = 1.1f;  // 1つ消えるアニメの長さ（秒）
	constexpr float LosePopScale  = 1.45f; // 消える直前に一瞬大きくなる倍率

	// 残機が1つ減る瞬間の演出（星屑・UI揺れ・SE）
	constexpr float UiShakeAmp  = 12.0f;   // 減る瞬間のUIアイコン揺れ幅(px)
	constexpr float UiShakeFreq = 46.0f;   // 揺れの速さ
	constexpr int   SparkCount  = 12;      // 星屑の粒数
	constexpr float SparkMaxR   = 100.0f;  // 星屑が散る最大半径(px)
	constexpr float SparkSize   = 26.0f;   // 星屑1粒の基準サイズ(px)
	constexpr float SparkStartP = 0.15f;   // 星屑が散り始めるアニメ進行(0〜1)
	// 減る瞬間のSE。※ファイルは未配置。ここに .wav を置けば自動で鳴る（無ければ無音）。
	constexpr const char* LifeLostSePath = "Asset/Sound/SE/LifeLost.wav";

	// 落下死：この秒数ずっと接地していない（＝場外に落ちた）と死亡。
	// メインの死亡判定はデッドゾーン(ZONE)で行うため、これは保険として長め(10秒超)にしてある。
	constexpr float FallTime    = 15.0f;

	// ステージごとに死ねる回数。これに達したらステージを最初からやり直す。
	// 例）3 のとき：1・2回目は復活、3回目の死亡でステージ最初へリトライ。
	constexpr int   MaxDeathsPerStage = 3;

	constexpr float ShakeStr    = 0.45f;  // 死亡時のカメラシェイク強さ
	constexpr float HitStop     = 0.10f;  // 死亡の瞬間の一瞬停止（秒）

	// ── ディゾルブ（死亡時：本体が溶けて消える）──
	// しきい値を 0→1 へ上げると、ノイズの暗いところから順にピクセルが消えていく。
	constexpr float DissolveTime      = 0.9f;   // 溶けきるまでの時間（秒）
	constexpr float DissolveEdgeRange = 0.06f;  // 溶け際の縁の太さ（0〜1）
	constexpr float DissolveEdgeR = 1.0f;       // 縁の発光色（燃えるようなオレンジ）
	constexpr float DissolveEdgeG = 0.45f;
	constexpr float DissolveEdgeB = 0.12f;

	// 暗転の色（赤みを帯びてから黒へ）
	constexpr float TintR = 0.55f;
	constexpr float TintG = 0.0f;
	constexpr float TintB = 0.0f;

	//----------------------------------------------------------
	// 死亡カメラ（マリギャラ風：死んだ場所へ寄りながらゆっくり回り込む）
	//----------------------------------------------------------
	constexpr float CamFocusHeight = 1.0f;    // 注視点の高さ（プレイヤー中心）
	constexpr float CamStartDist   = 16.0f;   // 寄り始めの距離
	constexpr float CamCloseDist   = 5.5f;    // 寄り切った距離（ズームイン）
	constexpr float CamStartYawDeg = 180.0f;  // 開始方位（180=正面/-Z 側＝通常視点に近い）
	constexpr float CamOrbitDeg    = 50.0f;   // 暗転までに回り込む角度
	constexpr float CamPitchDeg    = 18.0f;   // 見下ろし角
	constexpr float CamShake       = 0.5f;    // 死亡の瞬間の揺れ（減衰）
}
