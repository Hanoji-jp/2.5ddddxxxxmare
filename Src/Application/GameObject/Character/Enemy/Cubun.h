#pragma once
#include "Enemy.h"
#include "../../../Const/CubunConst.h"
#include "../../../Editor/EnemyPlacementData.h"

//==========================================================
// Cubun ─ ジャンプしながら巡回する棘付きの敵
//
//  ・体の「下」には棘 → プレイヤーが踏むと被ダメージ(SpikeDamage)
//  ・体の「上」は安全  → プレイヤーが上から踏むと撃破できる
//  ・重力が変わっても体は回転しない（見た目のUpDir固定）
//  ・着地するたびにジャンプを繰り返しながら巡回
//==========================================================
class Cubun : public Enemy
{
public:
	Cubun()          {}
	~Cubun() override {}

	void Init()       override;
	void Update()     override;
	void DrawLit()    override;
	void DrawDebug()  override;

	float GetAttackRange() const override { return CubunConst::AttackRange; }

	// エディターから出現設定を反映（Init より前に呼ぶ）
	void SetFaceDir(CubunFaceDir dir)                    { m_faceDir    = dir; }
	void SetInitGravDir(Character::ManualGravityDir dir) { m_initGravDir = dir; }

	// 当たり判定（本体・棘それぞれ外部から照会できる）
	bool Intersects(const KdCollider::RayInfo& ray,
					std::list<KdCollider::CollisionResult>* result);

	// 棘エリアに当たっているか（プレイヤー側から問い合わせ）
	bool IsSpikeHit(const Math::Vector3& playerPos) const;

protected:
	void DoAttack() override;

private:
	// ジャンプ制御
	void  TryJump();

	// 重力方向に対して体のビジュアル upDir を固定したワールド行列を計算
	Math::Matrix CalcVisualMatrix() const;

	// 出現向き・初期重力（Init で反映）
	CubunFaceDir                m_faceDir     = CubunFaceDir::Up;
	Character::ManualGravityDir m_initGravDir = Character::ManualGravityDir::None;

	// 状態
	float m_jumpTimer = 0.0f;    // 着地後のジャンプ待機タイマー

	// 棘コライダー用（体の下面オフセット）
	std::unique_ptr<KdCollider> m_pSpikeCollider = nullptr;
};

