#pragma once

//==========================================================
// JuiceConst.h
// 気持ちいい演出系定数（スクワッシュ・シェイク・フラッシュ等）
//==========================================================
namespace JuiceConst
{
	//----------------------------------------------------------
	// カメラシェイク
	//----------------------------------------------------------
	constexpr float ShakeDecay       = 8.0f;   // 1秒で強さが 1/e^8 に減衰
	constexpr float ShakeLandStr     = 0.18f;  // 着地時の強さ
	constexpr float ShakeHitStr      = 0.22f;  // 被ダメ時の強さ
	constexpr float ShakeDeathStr    = 0.35f;  // 敵撃破時の強さ

	//----------------------------------------------------------
	// 着地スクワッシュ＆ストレッチ
	//----------------------------------------------------------
	constexpr float SquashScaleX     = 1.35f;  // 着地時 X/Z 方向に広がる倍率
	constexpr float SquashScaleY     = 0.65f;  // 着地時 Y 方向に縮む倍率
	constexpr float SquashDuration   = 0.12f;  // スクワッシュ持続時間（秒）
	constexpr float SquashRecoverSpd = 12.0f;  // 元のスケールへの復元速度

	//----------------------------------------------------------
	// ジャンプ頂点スロー（頂点付近で重力を弱くする）
	//----------------------------------------------------------
	constexpr float ApexVelThreshold = 1.8f;   // 上下速度がこれ以下で「頂点」判定
	constexpr float ApexGravityScale = 0.35f;  // 頂点時の重力倍率

	//----------------------------------------------------------
	// テレポート出現スケールポップ
	//----------------------------------------------------------
	constexpr float PopOvershoot     = 1.28f;  // 最初に膨らむ倍率
	constexpr float PopDuration      = 0.18f;  // オーバーシュート持続（秒）
	constexpr float PopRecoverSpd    = 14.0f;  // 1.0 への復元速度

	//----------------------------------------------------------
	// ヒットストップ
	//----------------------------------------------------------
	constexpr float HitStopDuration  = 0.07f;  // 止まる秒数（約4フレーム）
	// ユニークアイテム取得時のヒットストップ（長め）
	constexpr float PickupHitStopDuration = 0.22f;  // 約13フレーム

	//----------------------------------------------------------
	// 撃破バウンス
	//----------------------------------------------------------
	constexpr float DeathBounceVel   = 5.5f;   // 死亡時の上方向初速
	constexpr float DeathFadeSpeed   = 2.2f;   // 死亡後のフェードアウト速度

	//----------------------------------------------------------
	// 画面フラッシュ（白）
	//----------------------------------------------------------
	constexpr float FlashHitAlpha    = 0.55f;  // ヒット時の最大白透明度
	constexpr float FlashDecay       = 6.0f;   // フラッシュ減衰速度

	//----------------------------------------------------------
	// HP ゲージヒットリアクション
	//----------------------------------------------------------
	constexpr float HpShakeAmp      = 6.0f;   // シェイク振れ幅（ピクセル）
	constexpr float HpShakeDuration = 0.4f;   // シェイク持続（秒）
	constexpr float HpShakeFreq     = 28.0f;  // シェイク周波数（Hz）

	//----------------------------------------------------------
	// 足元煙パーティクル（FootDust）
	//----------------------------------------------------------
	constexpr float DustIntervalWalk   = 0.06f;  // 歩き時のスポーン間隔（秒）
	constexpr float DustIntervalDash   = 0.02f;  // ダッシュ時のスポーン間隔（秒）
	constexpr float DustLifetimeMin    = 0.25f;  // 寿命の最小値（秒）
	constexpr float DustLifetimeMax    = 0.60f;  // 寿命の最大値（秒）
	constexpr float DustScaleMin       = 0.03f;  // 初期スケール最小
	constexpr float DustScaleMax       = 0.10f;  // 初期スケール最大
	constexpr float DustRiseSpeedMin   = 0.003f; // 上昇速度の最小（浮きにくい粒）
	constexpr float DustRiseSpeedMax   = 0.030f; // 上昇速度の最大（よく浮く粒）
	constexpr float DustDriftSpeed     = 0.008f; // 横流れの最大幅（±この値）
	constexpr float DustSpreadBack     = 0.45f;  // 後方ランダム広がり幅
	constexpr float DustSpreadSide     = 0.35f;  // 横ランダム広がり幅
	constexpr float DustAlphaStart     = 0.75f;  // 初期アルファ
	constexpr float DustSpeedMin       = 0.02f;  // この速度以上で出す（フレーム単位）
	constexpr float DustGrayMin        = 1.5f;   // グレー最暗値（emissive 強度、1以上でライト影響を超える）
	constexpr float DustGrayMax        = 3.0f;   // 白（emissive オーバードライブ）

	//----------------------------------------------------------
	// 惑星到達カメラズームアウト→戻り
	//----------------------------------------------------------
	constexpr float PlanetZoomOutAmount = 1.0f;   // ズームアウト量（offsetZ に加算）

	//----------------------------------------------------------
	// StarBurst パーティクル（撃破エフェクト）
	//----------------------------------------------------------
	constexpr int   StarBurstCount       = 18;     // スポーン粒数
	constexpr float StarBurstScaleMin    = 0.05f;  // 初期スケール最小
	constexpr float StarBurstScaleMax    = 0.18f;  // 初期スケール最大
	constexpr float StarBurstShrinkMin   = 1.5f;   // 縮み速度最小
	constexpr float StarBurstShrinkMax   = 4.5f;   // 縮み速度最大
	constexpr float StarBurstSpeedMin    = 0.04f;  // 飛散速度最小
	constexpr float StarBurstSpeedMax    = 0.18f;  // 飛散速度最大
	constexpr float StarBurstLifetimeMin = 0.3f;   // 寿命最小（秒）
	constexpr float StarBurstLifetimeMax = 0.7f;   // 寿命最大（秒）
	constexpr float StarBurstEmissive    = 4.0f;   // 発光強度（青白く光る）
}

