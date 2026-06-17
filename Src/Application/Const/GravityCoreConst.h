#pragma once

// 重力コアの種別
enum class CoreType : int
{
	Rock = 0,   // 岩石ローポリ（既存）
	Glow = 1,   // WarpHole風エネルギーコア（新規）
};

// 重力コアオブジェクトに関わる定数
namespace GravityCoreConst
{
	// ── 球体メッシュ ──────────────────────────────────────
	// 緯度方向の分割数（ローポリ感：6〜10 推奨）
	constexpr int   LatitudeSegs  = 7;

	// 経度方向の分割数
	constexpr int   LongitudeSegs = 8;

	// デフォルト半径
	constexpr float DefaultRadius = 0.8f;

	// ── ジッター（頂点ランダム変位）─────────────────────
	// 各頂点の半径に乗算するランダムオフセット最大比率（0〜1）
	// 大きいほど凸凹した岩石感が強くなる
	constexpr float JitterStrength = 0.38f;

	// ジッターのシード（同じ形状を再現したいとき変える）
	constexpr unsigned int JitterSeed = 0x5EEDBEEF;

	// ── Rock 色 ─────────────────────────────────────────────
	// 面の基本色（茶〜グレー：岩・破片イメージ）
	constexpr float FaceColorR = 0.45f;
	constexpr float FaceColorG = 0.38f;
	constexpr float FaceColorB = 0.30f;

	// 面アルファ
	constexpr float FaceAlpha  = 0.92f;

	// ワイヤーフレーム明るさ倍率
	constexpr float WireBoost  = 1.6f;

	// ワイヤーアルファ
	constexpr float WireAlpha  = 0.55f;

	// 奥面を暗くするグラデーション係数（0=なし、1=最大）
	constexpr float DarkFactor = 0.55f;

	// ── Glow 色（WarpHole風：シアン/青/白）──────────────
	// 面の基本色（R低め・G高め・B高め）
	constexpr float GlowFaceR  = 0.05f;
	constexpr float GlowFaceG  = 0.55f;
	constexpr float GlowFaceB  = 1.00f;

	// 面アルファ
	constexpr float GlowFaceAlpha  = 0.75f;

	// ワイヤーアルファ
	constexpr float GlowWireAlpha  = 0.90f;

	// 波アニメーション振幅（頂点の半径変動幅）
	constexpr float GlowWaveAmp    = 0.12f;

	// 波の周波数（リング方向・周方向）
	constexpr float GlowWaveFreqR  = 1.8f;
	constexpr float GlowWaveFreqS  = 2.5f;

	// ブルームパスのアルファ倍率（DrawBright で使う）
	constexpr float GlowBloomAlpha = 0.85f;

	// Glow 自転速度
	constexpr float GlowRotSpeed   = 0.9f;

	// ── アニメーション ───────────────────────────────────
	// Rock ゆっくり自転（ラジアン/秒）
	constexpr float RotSpeed   = 0.4f;

	// ── グロー画像（中心をふんわり光らせる加算ビルボード）──────
	// Effekseer のリングの代わりに、glow テクスチャでやわらかく光らせる
	constexpr const char* GlowSpriteTexPath = "Asset/Effect/Particle04_bokashi_hard.png"; // 柔らかいハロー
	constexpr float GlowSpriteSizeMul = 3.2f;  // radius に対する見かけサイズ倍率
	constexpr float GlowSpriteAlpha   = 0.55f; // 基本アルファ（控えめに光らす）
	constexpr float GlowSpritePulseSpeed = 2.2f; // 明滅速度（rad/秒）
	constexpr float GlowSpritePulseAmp   = 0.18f; // サイズ・明るさの脈動幅

	// ワイヤーを面より少しだけ外側に出して描く倍率（Z-fight 回避＋前面のみ表示）
	constexpr float WireDepthOutset = 1.02f;

	// Glow コアの取得判定半径（radius に対する倍率）
	constexpr float PickupRadiusMul = 1.4f;

	// 取得時に開くゴール用 WarpHole の出口を、入口(コア位置)から up 方向にどれだけ離すか
	constexpr float GoalWarpExitUp  = 30.0f;

	// ── カリング ────────────────────────────────────────
	constexpr float CullingRadiusMul = 2.0f;  // radius * この係数 = m_cullingRadius

	// ── CSV ─────────────────────────────────────────────
	constexpr const char* SavePath = "Asset/Data/gravity_cores.csv";
}
