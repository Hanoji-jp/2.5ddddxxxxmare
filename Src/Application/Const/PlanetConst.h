#pragma once

// 惑星重力システムに関わる定数
namespace PlanetConst
{
	// デフォルトのサーフェス半径（プレイヤーが立つ球面半径）
	constexpr float DefaultSurfaceRadius  = 10.0f;

	// デフォルトの地面判定半径（着地・ジャンプ判定。SurfaceRadius より少し大きく）
	constexpr float DefaultGroundRadius   = 12.0f;

	// デフォルトの引力影響半径（この範囲内にいると惑星重力が有効。GroundRadius より大きく）
	constexpr float DefaultGravityRadius  = 30.0f;

	// デフォルトの優先度（高いほど優先。同じ値なら距離で決める）
	constexpr int   DefaultPriority       = 0;

	// 惑星重力加速度（惑星中心方向へ）
	constexpr float GravityAccel          = 0.01f;

	// 最大落下速度（惑星表面方向）
	constexpr float MaxFallSpeed          = 0.5f;

	// 重力合成：影響力計算用のイプシロン（ゼロ除算防止）
	constexpr float GravityInfluenceEpsilon = 0.1f;

	// 重力合成：影響力の強度係数（大きいほど遠い惑星の影響が強い）
	constexpr float GravityInfluenceStrength = 100.0f;

	// 地面スナップ許容距離（サーフェス半径からこの距離以内に入ったら着地）
	constexpr float GroundSnapTolerance   = 0.3f;

	// upDir の補間速度（1.0 = 即時、小さいほど緩やか。0.05〜0.1 が自然）
	constexpr float UpDirSlerpSpeed       = 0.08f;

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

	// 惑星モデルパス
	static constexpr const char* ModelPath    = "Asset/Data/Planet.gltf";
	static constexpr const char* BoxModelPath = "Asset/Data/Box.gltf";

	// 惑星モデルに対するレイキャスト開始オフセット（キャラ足元から上方向へ）
	constexpr float PlanetRayOffset = 5.0f;

	// 惑星モデルに対するレイキャスト最大長
	constexpr float PlanetRayLength = 20.0f;

	// モデル半径に加算する GroundRadius のマージン（モデル表面より少し広め）
	constexpr float GroundRadiusMargin  = 2.0f;

	// モデル半径に加算する GravityRadius のマージン（引力圏をさらに広く）
	constexpr float GravityRadiusMargin = 15.0f;

	// CSV 保存パス
	static constexpr const char* SavePath = "Asset/Data/planets.csv";

	// トリプレーナーUV のワールド座標スケール（値が小さいほどテクスチャが細かく繰り返す）
	constexpr float TriplanarScale = 0.1f;
}
