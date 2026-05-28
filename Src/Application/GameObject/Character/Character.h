#pragma once
#include "../../Const/GameConst.h"
#include "../../Const/CollisionConst.h"
#include "../../Manager/PlanetGravityManager.h"
#include "../../Manager/ManualGravityZoneManager.h"
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

    // 手動重力方向（重力パズル用）
    enum class ManualGravityDir
    {
        None,  // 自動（惑星重力に従う）
        Down,  // ↓
        Up,    // ↑
    };

    Character() {}
    virtual ~Character() {}

    void Update()     override;
    void PostUpdate() override;
    void DrawDebug()  override;

    int  GetHp()      const { return m_hp; }
    bool IsDead()     const { return m_hp <= 0; }

    // 現在の「上」方向を取得（カメラ・モデル回転用、Slerp補間済み）
    const Math::Vector3& GetUpDir() const { return m_upDirVisual; }

    // 物理・速度計算用「上」方向（即切り替え、重力切り替え直後も正確）
    const Math::Vector3& GetPhysicsUpDir() const { return m_upDir; }

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

    // 惑星重力用：現在の「上」方向（物理・判定用、即スナップ）
    Math::Vector3 m_upDir = { 0.0f, 1.0f, 0.0f };

    // 見た目用「上」方向（Slerpで補間、モデル回転に使用）
    Math::Vector3 m_upDirVisual = { 0.0f, 1.0f, 0.0f };

    // 現在影響を受けている惑星のインデックス（-1 なら通常重力）
    int m_currentPlanetIndex = -1;

    // 毎フレーム m_currentPlanetIndex から取得するヘルパー（フレーム内のみ有効）
    const PlanetData* m_pCurrentPlanet = nullptr;

    // 前フレームの惑星インデックス（惑星乗り移り検出用）
    int m_prevPlanetIndex = -1;

    std::weak_ptr<KdGameObject> m_wpMap;

    std::unique_ptr<KdDebugWireFrame> m_pDebugWire;

    // 手動重力方向の設定（重力パズル用）
    // 方向が変わった瞬間に速度をリセット＋押し出し目標をセットして壁埋まりを防ぐ
    void SetManualGravity(ManualGravityDir _dir)
    {
        if (m_manualGravityDir == _dir) { return; }
        m_manualGravityDir = _dir;

        if (_dir == ManualGravityDir::None)
        {
            m_isGround = false;
            m_ejectRemaining = { 0.0f, 0.0f, 0.0f };
            return;
        }

        // 新しい「上」方向（新重力の床から離れる方向）
        const Math::Vector3 newUp = (_dir == ManualGravityDir::Up)
            ? Math::Vector3{ 0.0f, -1.0f, 0.0f }
            : Math::Vector3{ 0.0f,  1.0f, 0.0f };

        // 新しい床に向かう速度成分を除去
        const float intoNewFloor = m_velocity.Dot(-newUp);
        if (intoNewFloor > 0.0f)
            m_velocity += newUp * intoNewFloor;

        // 現在の床（旧m_upDir方向）から離れるキックを直接velocityに加算
        // ejectではなくvelocityにすることで重力と打ち消し合わない
        // ※キック前に上下成分をリセットして歩き速度の蓄積による爆速を防ぐ
        constexpr float kKickSpeed = 0.3f;
        const float lateralVel = m_velocity.Dot(m_upDir); // 旧up方向成分
        m_velocity -= m_upDir * lateralVel;               // 旧up成分だけ除去
        m_velocity += m_upDir * kKickSpeed;               // キック加算
        m_velocity.z = 0.0f;

        m_isGround       = false;
        m_ejectRemaining = { 0.0f, 0.0f, 0.0f }; // ejectは使わない
    }
    ManualGravityDir GetManualGravity() const { return m_manualGravityDir; }

    // 空中での重力切り替え回数をチェック（1回まで）
    bool CanSwitchGravityInAir() const { return m_airGravitySwitchCount < 1; }
    void ConsumeAirGravitySwitch() { m_airGravitySwitchCount++; }

private:
    // 手動重力方向（Noneなら自動、それ以外なら固定方向）
    ManualGravityDir m_manualGravityDir = ManualGravityDir::None;

    // 空中での重力切り替え回数（着地でリセット）
    int m_airGravitySwitchCount = 0;

    // 重力切り替え時の押し出し残量（毎フレームLerpで消費）
    Math::Vector3 m_ejectRemaining = { 0.0f, 0.0f, 0.0f };
};
