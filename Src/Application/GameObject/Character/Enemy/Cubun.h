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
	void PostUpdate() override;
	void DrawLit()    override;
	void DrawOutline() override;   // 原神式アウトライン
	void DrawDebug()  override;

	float GetAttackRange() const override { return CubunConst::AttackRange; }

	// エディターから初期重力方向を反映（Init より前に呼ぶ）
	void SetInitGravDir(Character::ManualGravityDir dir) { m_initGravDir = dir; }

	// 踏みつけぺちゃんこ演出を開始（GameScene から呼ぶ）
	void StartStomp();
	bool IsSquashing() const { return m_squashActive; }

	// 本体押し出しコライダーは「地面にいるとき」だけ有効にする。
	// 空中（叩きつけ中）はめり込み防止を切り、プレイヤーを横へ押し出さず踏みつぶす。
	const KdCollider* GetCollider() const override
	{
		return m_isGround ? m_pCollider.get() : nullptr;
	}

	// 当たり判定（本体・棘それぞれ外部から照会できる）
	bool Intersects(const KdCollider::RayInfo& ray,
					std::list<KdCollider::CollisionResult>* result);

	// 棘エリアに当たっているか（足側＝-m_upDir 側。プレイヤー側から問い合わせ）
	bool IsSpikeHit(const Math::Vector3& playerPos) const;

	// 棘面がワールド上方向を向いているか（重力反転時に true）。
	// 通常重力 = 底面の踏みつぶし（即死）／反転時 = 棘ダメージ、に振り分けるのに使う。
	bool IsSpikeFacingUp() const { return m_upDir.y < 0.0f; }

	// プレイヤーが頭側（+m_upDir 側＝安全面）から踏みつけたか
	// playerVel が Cubun の上方向に対して降下中のときのみ成立
	bool CheckStomp(const Math::Vector3& playerPos, const Math::Vector3& playerVel) const;

protected:
	void DoAttack() override;

private:
	// ジャンプ制御
	void  TryJump();

	// 重力方向に対して体のビジュアル upDir を固定したワールド行列を計算
	Math::Matrix CalcVisualMatrix() const;

	// 初期重力方向（Init で反映）
	Character::ManualGravityDir m_initGravDir = Character::ManualGravityDir::None;

	// 状態
	float m_jumpTimer    = 0.0f;  // 着地後のジャンプ待機タイマー
	bool  m_squashActive = false; // ぺちゃんこ演出中
	float m_squashTimer  = 0.0f;  // 演出経過時間
	bool  m_rockDropped  = false; // 岩石ドロップ要求を1回だけ出すためのガード

	// 叩きつけコライダー（モデルの COL_Attack ノードをレイで判定）
	std::unique_ptr<KdCollider> m_pAttackCollider = nullptr;
};

