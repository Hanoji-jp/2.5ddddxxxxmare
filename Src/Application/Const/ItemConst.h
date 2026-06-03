#pragma once

namespace ItemConst
{
	// ---- Coin ----
	static constexpr const char* CoinModelPath  = "Asset/Data/coin.gltf";

	// コイン取得判定の球半径
	constexpr float CoinHitRadius       = 1.2f;

	// コインの回転速度（ラジアン/フレーム）
	constexpr float CoinRotateSpeed     = 0.05f;

	// コイン浮遊の振幅・速度
	constexpr float CoinBobAmplitude    = 0.15f;
	constexpr float CoinBobSpeed        = 0.07f;

	// コインのマテリアル補正（gltf の roughness=0 が眩しすぎるため上書き）
	constexpr float CoinMetallic   = 1.0f;   // 金属感は維持
	constexpr float CoinRoughness  = 0.08f;  // 金属コインらしい高光沢（0.0だと眩しすぎ・0.35だと光沢なし）

	// ---- HitBox ----
	// プレイヤーのアイテム取得用ヒットボックス半径
	constexpr float PlayerPickupRadius  = 1.5f;
}
