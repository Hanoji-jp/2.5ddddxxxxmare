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

	// ---- HitBox ----
	// プレイヤーのアイテム取得用ヒットボックス半径
	constexpr float PlayerPickupRadius  = 1.5f;
}
