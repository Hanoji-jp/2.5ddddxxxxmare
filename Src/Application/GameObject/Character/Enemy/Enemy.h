#pragma once
#include "../Character.h"
#include "../../../Const/EnemyConst.h"

class Enemy : public Character
{
public:
    // 敵AIの行動状態
    enum class AIState
    {
        Patrol,   // 巡回
        Chase,    // 追跡
        Attack,   // 攻撃
        Dead,     // 死亡
    };

    Enemy()          { Init(); }
    virtual ~Enemy() {}

    void Init()    override;
    void Update()  override;
    void DrawLit() override;

    bool IsVisible() const override { return true; }

    void SetTarget(const std::weak_ptr<KdGameObject>& _target) { m_wpTarget = _target; }

protected:
    void Patrol();
    void Chase();
    void CheckGround() override;

    AIState m_aiState = AIState::Patrol;

    std::weak_ptr<KdGameObject> m_wpTarget;

    KdModelWork m_modelWork;

    // 巡回折り返し用フラグ
    bool m_patrolRight = true;
};
