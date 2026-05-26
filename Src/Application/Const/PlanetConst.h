#pragma once

// 惑星重力システムに関わる定数
namespace PlanetConst
{
	// デフォルトのサーフェス半径（プレイヤーが立つ球面半径）
	constexpr float DefaultSurfaceRadius  = 10.0f;

	// デフォルトの引力影響半径（この範囲内にいると惑星重力が有効）
	constexpr float DefaultGravityRadius  = 30.0f;

	// 惑星重力加速度（惑星中心方向へ）
	constexpr float GravityAccel          = 0.02f;

	// 最大落下速度（惑星表面方向）
	constexpr float MaxFallSpeed          = 1.0f;

	// 地面スナップ許容距離（サーフェス半径からこの距離以内に入ったら着地）
	constexpr float GroundSnapTolerance   = 0.3f;

	// デバッグ表示色（サーフェス円）
	constexpr float SurfaceColorR = 0.3f;
	constexpr float SurfaceColorG = 0.8f;
	constexpr float SurfaceColorB = 1.0f;
	constexpr float SurfaceColorA = 1.0f;

	// デバッグ表示色（引力範囲円）
	constexpr float GravityColorR = 0.2f;
	constexpr float GravityColorG = 0.4f;
	constexpr float GravityColorB = 1.0f;
	constexpr float GravityColorA = 0.4f;

	// CSV 保存パス
	static constexpr const char* SavePath = "Asset/Data/planets.csv";
}
