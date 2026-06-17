#pragma once

// 敵撃破時にドロップする小さな岩石アイテム（取得できる通貨）の定数
namespace RockConst
{
	// ── 見た目（GravityCore の岩石を小型化したローポリ球）──
	constexpr int          LatitudeSegs  = 4;        // 緯度分割（少なめ＝ローポリ）
	constexpr int          LongitudeSegs = 5;        // 経度分割
	constexpr float        JitterStrength = 0.38f;   // 頂点ランダム変位
	constexpr unsigned int JitterSeed     = 0x10C77E;
	constexpr float        Radius         = 0.22f;   // 小さめ
	constexpr float        DarkFactor     = 0.55f;   // 奥面を暗く

	// 岩石色（GravityCore の FaceColor 系）
	constexpr float ColorR = 0.55f;
	constexpr float ColorG = 0.45f;
	constexpr float ColorB = 0.35f;
	constexpr float FaceAlpha = 0.95f;
	constexpr float WireBoost = 1.6f;
	constexpr float WireAlpha = 0.6f;
	constexpr float WireDepthOutset = 1.03f;  // ワイヤーを面より少し外へ（裏線を隠す）

	// ── 物理（重力は upDir 基準。横重力ステージでも正しく落ちる）──
	constexpr float Gravity        = 22.0f;   // -upDir 方向の加速度（units/s^2）
	constexpr float BurstSpeedMin  = 2.5f;    // 水平方向の散らばり初速
	constexpr float BurstSpeedMax  = 5.5f;
	constexpr float BurstUpMin     = 4.0f;    // up 方向の打ち上げ初速
	constexpr float BurstUpMax     = 7.0f;
	constexpr float Restitution    = 0.55f;   // 着地時の反発（大きいほどよく弾む）
	constexpr float BounceFriction = 0.8f;    // 着地時の水平減衰（1に近いほど横に転がる）
	constexpr float RestThreshold  = 0.4f;    // この速度未満で着地確定（小さいほど多くバウンド）
	constexpr float SpinSpeed      = 5.0f;    // 自転（rad/秒）

	// 着地面の高さオフセット（検出した地面からこの分だけ上で止まる＝少し浮かせる）
	constexpr float LandHeightOffset = 0.45f;

	// 床検出レイ（spawn 時に下向きへ撃って実際の地面を探す。敵を倒す高さに依存しないため）
	constexpr float GroundRayUp  = 2.0f;    // spawn より少し上から
	constexpr float GroundRayLen = 200.0f;  // 高所で倒しても地面まで届く長さ

	// ── 着地後の演出（ふわふわ上下）──
	constexpr float LandedBobSpeed = 2.5f;   // 上下の速さ（ゆっくり）
	constexpr float LandedBobAmp   = 0.2f;   // 上下の振幅（はっきり浮遊）

	// ── 取得 ──
	constexpr float HitRadius  = 0.7f;        // プレイヤーとの取得判定半径（歩いて拾いやすく）

	// ── ドロップ数（敵1体あたり：6〜10個ランダム）──
	constexpr int   DropCountMin = 6;
	constexpr int   DropCountMax = 10;

	// 拾えるようになるまでの猶予（散らばり中の誤取得防止）
	constexpr float PickupDelay = 0.25f;
}
