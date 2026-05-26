#include "../../../../Pch.h"
#include "EnemyRanged.h"

void EnemyRanged::Init()
{
	InitModel(EnemyConst::RangedModelPath);
	m_spawnPos = GetPos();
}

void EnemyRanged::PostUpdate()
{
	// 矢を更新して消滅したものを除去
	for (auto& arrow : m_arrows) { arrow->Update(); }
	m_arrows.erase(
		std::remove_if(m_arrows.begin(), m_arrows.end(),
			[](const std::shared_ptr<Arrow>& a) { return a->IsExpired(); }),
		m_arrows.end());

	Enemy::PostUpdate();
}

void EnemyRanged::DrawLit()
{
	Enemy::DrawLit();

	for (auto& arrow : m_arrows) { arrow->DrawLit(); }
}

void EnemyRanged::Chase()
{
	const auto spTarget = m_wpTarget.lock();
	if (!spTarget) { return; }

	const Math::Vector3 toTarget = spTarget->GetPos() - GetPos();
	const float dist = std::sqrtf(toTarget.LengthSquared());

	if (dist < EnemyConst::RangedKeepDist)
	{
		// 近すぎる → 後退
		m_aiState = AIState::Retreat;
		const Math::Vector3 awayDir = -toTarget * (1.0f / dist);
		m_velocity.x = awayDir.x * EnemyConst::MoveSpeed;
		m_velocity.z = awayDir.z * EnemyConst::MoveSpeed;
		FaceTarget();
		ChangeAnim("Walk");
	}
	else
	{
		Enemy::Chase();
	}
}

void EnemyRanged::DoAttack()
{
	// 攻撃中は移動停止
	m_velocity.x = 0.0f;
	m_velocity.z = 0.0f;

	FaceTarget();
	ChangeAnim("Idle");

	if (m_attackCool > 0) { return; }

	// プレイヤーに向けて矢を発射
	const auto spTarget = m_wpTarget.lock();
	if (spTarget)
	{
		// 発射位置はキャラ中心から少し上
		const Math::Vector3 firePos = GetPos() + Math::Vector3(0.0f, EnemyConst::RangedFireOffsetY, 0.0f);

		Math::Vector3 dir = spTarget->GetPos() + Math::Vector3(0.0f, EnemyConst::RangedFireOffsetY, 0.0f) - firePos;
		if (dir.LengthSquared() > 0.0f)
		{
			dir.Normalize();

			auto arrow = std::make_shared<Arrow>();
			arrow->Init();
			arrow->Launch(firePos, dir);
			m_arrows.push_back(arrow);
		}
	}

	m_attackCool = EnemyConst::RangedAttackCool;
}
