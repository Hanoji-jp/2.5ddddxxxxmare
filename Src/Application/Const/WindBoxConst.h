#pragma once

// 風ボックスギミックに関わる定数
namespace WindBoxConst
{
	// デフォルトのボックスサイズ（幅・高さ・奥行き）
	constexpr float DefaultSizeX = 4.0f;
	constexpr float DefaultSizeY = 4.0f;
	constexpr float DefaultSizeZ = 4.0f;

	// デフォルトの風の強さ（上昇加速度 m/s相当、毎フレーム加算）
	constexpr float DefaultPower    = 0.03f;

	// デフォルトの風が届く距離（COL範囲の外側に延長する長さ）
	constexpr float DefaultLength   = 3.0f;

	// 風で上昇できる最大速度（重力加速度とのバランス用）
	constexpr float MaxUpSpeed      = 0.4f;

	// 風で横に飛ばされる最大速度
	constexpr float MaxSideSpeed    = 0.3f;

	// 風の中にいるときの重力スケール（ほぼ落下しない）
	constexpr float WindGravityScale       = 0.05f;

	// 横風の中での最大落下速度（ほぼ静止させる）
	constexpr float WindHorizMaxFallSpeed  = 0.01f;

	// 風がプレイヤーに与える最大傾き角度（度）
	constexpr float MaxTiltDeg = 25.0f;

	// 傾き補間速度（1.0=即座、小さいほど緩やか）
	constexpr float TiltLerpSpeed = 0.1f;

	// 風がない時の傾き戻り速度
	constexpr float TiltResetSpeed = 0.08f;

	// デバッグ表示の矢印本数
	constexpr int DebugArrowCount = 5;

	// CSVの保存パス
	constexpr const char* SavePath = "Asset/Data/wind_boxes.csv";

	// ── 風パーティクル ──────────────────────────────
	// 同時に飛ばすパーティクル数
	constexpr int   ParticleCount      = 12;

	// パーティクル1枚のワールドサイズ
	constexpr float ParticleSizeMin    = 0.4f;
	constexpr float ParticleSizeMax    = 1.0f;

	// 螺旋の最大半径（ボックス短辺 * この係数）
	constexpr float SpiralRadiusScale  = 0.35f;

	// 1秒あたりの自転回転速度（ラジアン）
	constexpr float SelfRotSpeed       = 3.0f;

	// 1秒あたりの公転速度（螺旋の位相、ラジアン）
	constexpr float OrbitSpeed         = 2.5f;

	// フェードイン・アウト境界（ライフタイム比率）
	constexpr float FadeEdge           = 0.2f;

	// パーティクルの最大アルファ
	constexpr float ParticleAlphaMax   = 0.55f;

	// テクスチャパス
	constexpr const char* ParticleTexPath = "Asset/Effect/wind02.png";

	// ── ボックスモデル ──────────────────────────────
	// windbox.gltf（外箱スケールはモデル側で設定済み）
	constexpr const char* BoxModelPath = "Asset/Data/windbox.gltf";

	// プロペラノード名（windbox.gltf 内）
	constexpr const char* PropellerNodeName = "立方体";

	// 当たり判定ノード名（windbox.gltf 内）
	constexpr const char* ColNodeName = "COL";

	// プロペラ1秒あたりの回転速度（ラジアン）
	constexpr float PropellerRotSpeed = 4.0f;

	// windbox.gltf の自然サイズ（Cube ノードの scale から算出）
	// Cube: scale(1, 0.5, 1) → X/Z 方向 = 1.0、Y 方向 = 0.5
	constexpr float ModelNaturalXZ = 1.0f;
	constexpr float ModelNaturalY  = 0.5f;

	// 風が出る面の中心オフセット：モデルの +windDir 面まで（外箱の半サイズ Y=0.5 相当）
	constexpr float ModelFaceOffset = 0.5f;

	// パーティクル散布用：windbox.gltf 出口面の半サイズ
	// Cube ノードが scale(1, 0.5, 1) なので X/Z 方向=1.0, Y 方向=0.5
	constexpr float ModelFaceHalfW = 1.0f;   // 出口面の横方向半サイズ
	constexpr float ModelFaceHalfH = 0.5f;   // 出口面の縦方向半サイズ
}
