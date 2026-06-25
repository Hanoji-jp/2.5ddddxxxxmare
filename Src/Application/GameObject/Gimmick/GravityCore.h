#pragma once
#include "../../Const/GravityCoreConst.h"
#include "../../Const/ItemConst.h"
#include "../Effect/ItemEffect.h"

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

	// Glow タイプか（ゴール用 WarpHole を開くトリガー）
	bool IsGlow() const { return m_type == CoreType::Glow; }

	// Glow の発光色を上書きする（Init 後に呼ぶ）。
	// チェックポイント等、ゴールコアと色で差別化したい用途に使う。
	void SetGlowColor(const Math::Vector3& c)
	{
		m_glowTint  = c;
		m_coreColor = { c.x, c.y, c.z, 1.0f };
	}

	// 取得後にプレイヤーのボーンへ追従させる用（クリア演出）。
	// 本体メッシュは m_mWorld で描くので平行移動も更新する（回転は前回値を保持）。
	void SetPos(const Math::Vector3& p)
	{
		m_pos = p;
		m_mWorld._41 = p.x; m_mWorld._42 = p.y; m_mWorld._43 = p.z;
	}

private:
	void BakeMesh();           // Rock 用: ジッター球体を事前計算
	void DrawGlowEffect();     // Glow 描画（DrawEffect から呼ぶ）
	void DrawGlowBright();     // Glow bloom（DrawBright から呼ぶ）
	void UpdateShadow();       // 真下へレイ → 地面位置・法線を求める（位置変化時のみ）
	void DrawShadow();         // 地面に柔らかい暗い円（ブロブシャドウ）を描く

	// ── 共通 ─────────────────────────────────────────────
	Math::Vector3 m_pos       = {};
	float         m_radius    = GravityCoreConst::DefaultRadius;
	CoreType      m_type      = CoreType::Rock;
	float         m_rotAngle  = 0.0f;
	float         m_animTime  = 0.0f;   // Glow 波アニメ用

	// Glow の発光色（既定はゴールのシアン青）。SetGlowColor で上書き可能。
	Math::Vector3 m_glowTint{ GravityCoreConst::GlowFaceR,
							  GravityCoreConst::GlowFaceG,
							  GravityCoreConst::GlowFaceB };

	// ── Rock ─────────────────────────────────────────────
	// ジッター済み球体頂点
	std::vector<Math::Vector3>     m_baseVerts;

	// ── 共用頂点バッファ（DrawEffect/DrawBright で毎フレーム構築）──
	mutable std::vector<KdPolygon::Vertex> m_triVerts;
	mutable std::vector<KdPolygon::Vertex> m_wireVerts;

	// ── 星きらめきエフェクト（Effekseer ループは使わず星だけ）──
	ItemEffect m_effect;

	// ── 中心をふんわり光らせる glow 画像ビルボード（加算）──
	KdSquarePolygon            m_glowPoly;
	std::shared_ptr<KdTexture> m_glowTex;
	Math::Color                m_coreColor{ 1.0f, 1.0f, 1.0f, 1.0f }; // タイプ別の本体色

	// ── 地面に落とす影（ブロブシャドウ）──
	bool          m_shadowValid    = false;          // 真下に地面が見つかったか
	bool          m_shadowComputed = false;          // 一度でも計算したか
	Math::Vector3 m_shadowPos      = {};             // 地面のヒット位置
	Math::Vector3 m_shadowNormal   = { 0.0f, 1.0f, 0.0f }; // 地面の法線
	Math::Vector3 m_shadowFromPos  = {};             // 前回計算したコア位置（移動検出用）
	float         m_shadowHeight   = 0.0f;           // コア〜地面の距離
};
