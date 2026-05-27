#pragma once
#include "../../Const/GameConst.h"
#include "../../Const/CollisionConst.h"
#include "../../Manager/PlanetGravityManager.h"
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
        None,   // 自動（惑星重力に従う）
        Down,   // ↓
        Up,     // ↑
        Left,   // ←
        Right,  // →
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

    std::weak_ptr<KdGameObject> m_wpMap;

    std::unique_ptr<KdDebugWireFrame> m_pDebugWire;

    // 手動重力方向の設定（重力パズル用）
    // 方向が変わった瞬間に速度をリセット＋押し出し目標をセットして壁埋まりを防ぐ
    void SetManualGravity(ManualGravityDir _dir)
    {
        if (m_manualGravityDir != _dir)
        {
            m_manualGravityDir = _dir;
            m_velocity = { 0.0f, 0.0f, 0.0f };
            m_isGround = false;

            // 現在の「上」方向（＝今いる床から離れる方向）で押し出す
            // ※新しいup方向を使うと床に向かって押し込む場合があるため、現在のupDirを使う
            constexpr float kEjectDist = 1.5f;
            if (_dir == ManualGravityDir::None) { return; }
            m_ejectRemaining = m_upDir * kEjectDist;
        }
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
