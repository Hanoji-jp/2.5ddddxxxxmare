#include "../../../../Pch.h"
#include "EnemyMelee.h"
#include "../Character.h"

void EnemyMelee::Init()
{
	InitModel(EnemyConst::MeleeModelPath);

	// モデル初期化後にスポーン位置を記録（SetPos 後に呼ばれることが前提）
	m_spawnPos = GetPos();
}

void EnemyMelee::DoAttack()
{
	FaceTarget();

	// クールダウン中は待機
	if (m_attackCool > 0)
	{
		ChangeAnim("Idle");
		return;
	}

	// ターゲットにダメージを与える
	const auto spTarget = m_wpTarget.lock();
	if (spTarget)
	{
		// Character にキャストしてダメージを与える
		if (auto* pChar = dynamic_cast<Character*>(spTarget.get()))
		{
			pChar->TakeDamage(EnemyConst::MeleeDamage);
		}
	}

	ChangeAnim("Idle");
	m_attackCool = EnemyConst::MeleeAttackCool;
}
