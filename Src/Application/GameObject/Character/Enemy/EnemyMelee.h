#pragma once
#include "Enemy.h"

//==========================================================
// EnemyMelee  ─ 近距離攻撃型の敵
//   プレイヤーに接近して近接攻撃を行う
//==========================================================
class EnemyMelee : public Enemy
{
public:
	EnemyMelee()          { Init(); }
	virtual ~EnemyMelee() {}

	void Init() override;

	float GetAttackRange() const override { return EnemyConst::MeleeAttackRange; }

protected:
	void DoAttack() override;
};
