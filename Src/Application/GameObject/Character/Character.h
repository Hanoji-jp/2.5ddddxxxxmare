#pragma once
#include "../../Const/GameConst.h"
#include "../../Const/CollisionConst.h"
#include "../../../Framework/Utility/KdDebug/KdDebugWireFrame.h"

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
    void DrawDebug()  override;

    int  GetHp()      const { return m_hp; }
    bool IsDead()     const { return m_hp <= 0; }

    virtual void TakeDamage(int _damage);

    // マップオブジェクトをセット（コリジョン判定に使用）
    void SetMapObject(const std::weak_ptr<KdGameObject>& _wpMap) { m_wpMap = _wpMap; }

protected:
    // 重力処理
    void ApplyGravity();

    // 移動量をワールド行列に反映
    void ApplyVelocity();

    // 着地判定
    void CheckGround();

    // 壁判定
    void CheckWall();

    int   m_hp          = 1;
    State m_state       = State::Idle;

    Math::Vector3 m_velocity = { 0.0f, 0.0f, 0.0f };

    bool  m_isGround    = false;

    std::weak_ptr<KdGameObject> m_wpMap;

    std::unique_ptr<KdDebugWireFrame> m_pDebugWire;
};
