#include "../../../../Pch.h"
#include "EnemyMelee.h"

void EnemyMelee::Init()
{
	// スポーン位置を記録
	m_spawnPos = GetPos();

	InitModel(EnemyConst::MeleeModelPath);
}

void EnemyMelee::DoAttack()
{
	// 攻撃中は移動しない
	m_velocity.x = 0.0f;
	m_velocity.z = 0.0f;

	FaceTarget();

	// クールダウン中は待機アニメ
	if (m_attackCool > 0)
	{
		ChangeAnim("Idle");
		return;
	}

	// 攻撃を実行（仮：当たり判定フラグのみ。後でヒットボックス実装）
	// TODO: プレイヤーへのダメージ処理を追加
	ChangeAnim("Idle");

	m_attackCool = EnemyConst::MeleeAttackCool;
}
