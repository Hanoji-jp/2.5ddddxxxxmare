#pragma once
#include "../../Const/GravityCoreConst.h"

//==========================================================
// GravityCore
// プレイヤーの故郷の惑星が破壊されて各ステージに散らばった
// 重力コアのかけら。
//  Rock : ローポリ岩石球体（静的メッシュ）
//  Glow : WarpHole風エネルギーコア（アニメ＋bloom）
//==========================================================
class GravityCore : public KdGameObject
{
public:
	GravityCore() {}
	~GravityCore() override {}

	void Init(const Math::Vector3& pos,
			  float radius        = GravityCoreConst::DefaultRadius,
			  CoreType type       = CoreType::Rock);

	void Update()      override;
	void DrawEffect()  override;
	void DrawBright()  override;   // Glow 用 bloom パス
	void DrawDebug()   override;

	bool IsVisible() const override { return true; }

private:
	void BakeMesh();           // Rock 用: ジッター球体を事前計算
	void DrawGlowEffect();     // Glow 描画（DrawEffect から呼ぶ）
	void DrawGlowBright();     // Glow bloom（DrawBright から呼ぶ）

	// ── 共通 ─────────────────────────────────────────────
	Math::Vector3 m_pos       = {};
	float         m_radius    = GravityCoreConst::DefaultRadius;
	CoreType      m_type      = CoreType::Rock;
	float         m_rotAngle  = 0.0f;
	float         m_animTime  = 0.0f;   // Glow 波アニメ用

	// ── Rock ─────────────────────────────────────────────
	// ジッター済み球体頂点
	std::vector<Math::Vector3>     m_baseVerts;

	// ── 共用頂点バッファ（DrawEffect/DrawBright で毎フレーム構築）──
	mutable std::vector<KdPolygon::Vertex> m_triVerts;
	mutable std::vector<KdPolygon::Vertex> m_wireVerts;
};
