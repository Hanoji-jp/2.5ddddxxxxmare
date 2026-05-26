#pragma once
#include "Enemy.h"
#include "../../Weapon/Arrow.h"

//==========================================================
// EnemyRanged  ─ 遠距離攻撃型の敵
//   一定距離を保ちながら Bow_Arrow.gltf の矢を発射する
//   近づきすぎたら後退する
//==========================================================
class EnemyRanged : public Enemy
{
public:
	EnemyRanged()          {}
	virtual ~EnemyRanged() {}

	void Init()       override;
	void PostUpdate() override;
	void DrawLit()    override;

	float GetAttackRange() const override { return EnemyConst::RangedAttackRange; }

protected:
	void Chase()    override;   // 距離を保つ挙動に上書き
	void DoAttack() override;

private:
	std::vector<std::shared_ptr<Arrow>> m_arrows;
};
