#pragma once
#include "../Character.h"
#include "../AnimBlender.h"
#include "../../../Const/EnemyConst.h"
#include "../../../Const/JuiceConst.h"

//==========================================================
// Enemy  ─ 敵共通の抽象基底クラス
//   近距離型 : EnemyMelee
//   遠距離型 : EnemyRanged
//==========================================================
class Enemy : public Character
{
public:
    // 敵AIの行動状態
    enum class AIState
    {
        Patrol,   // 巡回
        Chase,    // 追跡
        Attack,   // 攻撃
        Retreat,  // 後退（遠距離型用）
        Dead,     // 死亡
    };

    Enemy()          {}
    virtual ~Enemy() {}

    void Update()     override;
    void PostUpdate() override;
    void DrawLit()    override;
    void DrawOutline() override;   // 原神式アウトライン

    bool IsVisible() const override { return true; }

    void SetTarget(const std::weak_ptr<KdGameObject>& _target) { m_wpTarget = _target; }

    void Expire() { m_isExpired = true; }

    // 攻撃射程（派生クラスで実装）
    virtual float GetAttackRange() const = 0;

protected:
    // 各AIアクション（派生クラスで override 可）
    virtual void Patrol();
    virtual void Chase();
    virtual void DoAttack() = 0;   // 攻撃の実装は派生クラス必須

    // 指定方向に崖または壁があるか判定（true なら反転すべき）
    bool CheckEdgeAheadDir(const Math::Vector3& fwdDir) const;

    // 巡回方向（m_patrolRight）で崖/壁チェック
    bool CheckEdgeAhead() const;

    // モデル・アニメ初期化ヘルパー
    void InitModel(const char* _path);

    // アニメ切り替えヘルパー
    void ChangeAnim(const std::string& _name, bool _loop = true);

    // 向きを目標方向に向ける
    void FaceTarget();

    // 崖・壁反転検知を使うか（Cubun など崖で折り返す敵に true をセット）
    bool    m_useEdgeDetection  = false;

    // CheckEdgeAhead 間引きカウンタ（毎フレーム呼ぶと重いので数フレームに1回）
    int     m_edgeCheckInterval = 0;
    bool    m_edgeCheckCache    = false;  // 直前の CheckEdgeAhead の結果キャッシュ

    AIState m_aiState     = AIState::Patrol;
    int     m_attackCool  = 0;

    // 巡回用：スポーン位置を起点に左右折り返し
    Math::Vector3 m_spawnPos    = { 0.0f, 0.0f, 0.0f };
    bool          m_patrolRight = true;

    // 移動慣性速度（プレイヤーと同様の加減速用）
    Math::Vector3 m_moveVelocity = { 0.0f, 0.0f, 0.0f };

    Math::Vector3 m_facingDir   = { 0.0f, 0.0f, 1.0f };

    // 現在再生中のアニメ名（同名再セット防止）
    std::string m_currentAnimName;

    std::weak_ptr<KdGameObject> m_wpTarget;

    KdModelWork  m_modelWork;
    AnimBlender  m_animBlender;

    // 撃破バウンス
    bool  m_deathBounceActive = false;
    float m_deathBounceVelY   = 0.0f;  // 上方向速度
    float m_deathFadeAlpha    = 1.0f;  // 1=不透明 → 0=透明

public:
    // 撃破エフェクトスポーンフラグ（GameScene 側が1フレームだけ読んで AddObject する）
    bool  m_requestBurstEffect = false;
    // 撃破時の岩石ドロップ要求フラグ（GameScene 側が1フレームだけ読んで生成する）
    bool  m_requestRockDrop    = false;
};

