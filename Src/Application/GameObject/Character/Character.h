#pragma once
#include "../../Const/GameConst.h"

// Player / Enemy 共通の基底クラス
class Character : public KdGameObject
{
public:
    // キャラクターの状態
    enum class State
    {
        Idle,
        Walk,
        Jump,
        Fall,
        Dead,
        Attack,
    };

    Character() {}
    virtual ~Character() {}

    void Update()     override;
    void PostUpdate() override;

    int  GetHp()      const { return m_hp; }
    bool IsDead()     const { return m_hp <= 0; }

    virtual void TakeDamage(int _damage);

protected:
    // 重力処理
    void ApplyGravity();

    // 移動量をワールド行列に反映（Z固定）
    void ApplyVelocity();

    // 着地判定（派生クラスで実装）
    virtual void CheckGround() {}

    int   m_hp          = 1;
    State m_state       = State::Idle;

    Math::Vector3 m_velocity = { 0.0f, 0.0f, 0.0f };

    bool  m_isGround    = false;
};
