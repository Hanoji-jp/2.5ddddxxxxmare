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

	// m_upDir（ロジック用）の補間速度。角コーナーで即時スナップするのを防ぐ
	// Visual より速め（0.15〜0.25）にして物理的挙動とのズレを最小化
	constexpr float UpDirLogicSlerpSpeed  = 0.20f;

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

	// 芝生テクスチャのパス（Box惑星の上面ブレンド用）
	constexpr const char* GrassTexPath       = "Asset/Texture/grass.png";

	// 芝生法線マップのパス（盛り上がり表現用）
	constexpr const char* GrassNormalTexPath = "Asset/Texture/grass_normal.png";

	// 草エッジ（境目）テクスチャのパス（側面上部の土/境目表現）
	constexpr const char* GrassEdgeTexPath   = "Asset/Texture/grass_edge.png";

	// エッジ帯域の幅（upDot 空間。0.1〜0.5 推奨）
	constexpr float GrassEdgeWidth           = 0.3f;

	// エッジテクスチャのトリプレーナースケール
	constexpr float GrassEdgeTexScale        = 0.15f;

	// 芝生ブレンドの鋭さ（2〜8推奨。大きいほど境界がくっきり）
	constexpr float GrassBlendSharpness = 4.0f;

	// 芝生テクスチャのトリプレーナースケール
	constexpr float GrassTriplanarScale = 0.15f;

	// 草キャップモデル（Box惑星の上面に乗せる薄い板）
	// Box.gltf を Y スケール極小で代用（専用モデルがあれば差し替える）
	static constexpr const char* GrassCapModelPath = "Asset/Data/Box.gltf";

	// 草キャップの Y 方向の厚み（BoxHalfExtents.y に掛ける比率）
	constexpr float GrassCapThickness    = 0.3f;

	// 草キャップを上面からどれだけ上に浮かせるか（BoxHalfExtents.y に対する比率）
	constexpr float GrassCapYOffset      = 1.0f;  // 上面ちょうどに乗る

	// 草キャップの XZ スケール比率（Box の XZ より少し大きめにはみ出す）
	constexpr float GrassCapXZScale      = 1.08f;

	// 草キャップの最小はみ出し量（Left/Right キャップがない場合でも常にこの量だけ伸ばす）
	constexpr float GrassCapMinOverhang  = 0.29f;

	// Box本体の全面エッジブレンド強度（0=無効 1=フル上書き。薄暗くしたいので 0.3〜0.5 推奨）
	constexpr float BoxFullEdgeStrength  = 0.35f;

	// Box面判定ヒステリシス：現在の重力方向と一致する面にこの距離のバイアスを加える
	// 値が大きいほどコーナーで面が切り替わりにくくなる（0.3〜1.0 推奨）
	constexpr float BoxFaceHysteresis   = 0.8f;
}
