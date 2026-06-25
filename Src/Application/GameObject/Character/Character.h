#pragma once
#include "../../Const/GameConst.h"
#include "../../Const/CollisionConst.h"
#include "../../Manager/PlanetGravityManager.h"
#include "../../Manager/ManualGravityZoneManager.h"
#include "../../../Framework/Utility/KdDebug/KdDebugWireFrame.h"

class MovingFloor;  // 前方宣言

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
    // ワープ中はワープ進行方向を返す
    const Math::Vector3& GetUpDir() const
    {
        return m_warpUpOverrideActive ? m_warpUpOverride : m_upDirVisual;
    }

    // ワープ演出用スケール倍率（1.0 = 通常、0.0 = 消滅）
    void  SetWarpScale(float s) { m_warpScale = s; }
    float GetWarpScale()  const { return m_warpScale; }

    // ワープ中の向き上書き（GameScene から毎フレーム呼ぶ）
    // target に向かって Slerp で滑らかに回転する
    void SetWarpUpOverride(const Math::Vector3& target, float slerpT)
    {
        if (!m_warpUpOverrideActive)
        {
            // 初回：現在の visual up から開始
            m_warpUpOverride       = m_upDirVisual;
            m_warpUpOverrideActive = true;
        }
        m_warpUpOverride = Math::Vector3::Lerp(m_warpUpOverride, target, slerpT);
        if (m_warpUpOverride.LengthSquared() > 0.0001f)
        {
            m_warpUpOverride.Normalize();
        }
    }
    void ClearWarpUpOverride()
    {
        m_warpUpOverrideActive = false;
        m_warpStretchActive    = false;
    }

    // ワープストレッチON/OFF（Traveling フェーズ中のみ true にする）
    void SetWarpStretch(bool active) { m_warpStretchActive = active; }
    bool GetWarpStretch()      const { return m_warpStretchActive; }

    // 物理・速度計算用「上」方向（即切り替え、重力切り替え直後も正確）
    const Math::Vector3& GetPhysicsUpDir() const { return m_upDir; }

    // モデル姿勢用「上」方向（ワープオーバーライドを含まない、常に通常のVisual up）
    const Math::Vector3& GetVisualUpDir() const { return m_upDirVisual; }

    // 速度アクセサ（ワープなど外部から速度を書き換える場合に使用）
    const Math::Vector3& GetVelocity() const { return m_velocity; }
    void SetVelocity(const Math::Vector3& v)  { m_velocity = v; }

    bool IsGround()            const { return m_isGround; }
    int  GetCurrentPlanetIndex() const { return m_currentPlanetIndex; }

    virtual void TakeDamage(int _damage);
    // ダメージ＋ノックバック（sourcePos = ダメージ源の位置）
    // 基底は TakeDamage を呼ぶだけ。Player が override してノックバックを加える。
    virtual void TakeDamageFrom(int _damage, const Math::Vector3& sourcePos)
    {
        TakeDamage(_damage);
    }

    // マップオブジェクトをセット（コリジョン判定に使用）
    void SetMapObject(const std::weak_ptr<KdGameObject>& _wpMap) { m_wpMap = _wpMap; }

    // 移動床リストをセット（床・壁・天井判定に使用）
    void SetMovingFloorObjects(const std::vector<std::weak_ptr<MovingFloor>>& floors)
    {
        m_movingFloors = floors;
    }

    // 風ボックスリストをセット（地面着地判定に使用）
    void SetWindBoxObjects(const std::vector<std::weak_ptr<KdGameObject>>& windBoxes)
    {
        m_windBoxColliders = windBoxes;
    }

    // 敵障害物リストをセット（めり込み防止の押し出し判定に使用）
    void SetEnemyObstacles(const std::vector<std::weak_ptr<KdGameObject>>& obstacles)
    {
        m_enemyObstacles = obstacles;
    }

    // 棘ボックスリストをセット（本体の押し出し判定に使用）
    void SetSpikeBoxObjects(const std::vector<std::weak_ptr<KdGameObject>>& spikeBoxes)
    {
        m_spikeBoxColliders = spikeBoxes;
    }

    // コリジョンデバッグUI（ImGui）
    void DrawCollisionDebugGui();

protected:
    // ---- デバッグ用コリジョンログ ----
    struct CollisionFrameLog
    {
        Math::Vector3 pos           = {};
        Math::Vector3 velocity      = {};
        Math::Vector3 upDir         = {};
        bool          isGround      = false;

        // CheckWall
        struct WallHit
        {
            float         height     = 0.0f;   // レイ高さ
            Math::Vector3 rayDir     = {};
            Math::Vector3 hitNDir    = {};
            float         overlap    = 0.0f;
            bool          isNormal   = false;
            bool          filtered   = false;   // 底面/天面フィルタで除外されたか
        };
        std::vector<WallHit> wallHits;
        Math::Vector3        wallTotalPush = {};

        // CheckGround
        struct GroundHit
        {
            Math::Vector3 hitPos    = {};
            Math::Vector3 hitNDir   = {};
            float         hitDist   = 0.0f;
            bool          filtered  = false;
        };
        std::vector<GroundHit> groundHits;
        bool               groundSnapped = false;
    };

    static constexpr int kDebugLogFrames = 120;
    std::vector<CollisionFrameLog> m_collisionLog;
    int                            m_collisionLogIdx = 0;
    bool                           m_debugLogEnabled = false;
    CollisionFrameLog              m_currentFrameLog = {};
    // 重力処理
    void ApplyGravity();

    // 移動量をワールド行列に反映
    void ApplyVelocity();

    // 速度適用（分軸処理用）
    void ApplyVelocityHorizontal();
    void ApplyVelocityVertical();

    // 着地判定
    void CheckGround();

    // 天井判定（Box底面へのすり抜け防止）
    void CheckCeiling();

    // 壁判定
    void CheckWall();

    // 本体めり込み解決：球カプセル vs 地形(Box=AABB) を最近接点で押し出す。
    // 角でも斜めに押し出されるため端めり込みが起きない（レイ非依存）。
    void ResolvePenetration();

    int   m_hp          = 1;
    State m_state       = State::Idle;

    Math::Vector3 m_velocity = { 0.0f, 0.0f, 0.0f };

    bool  m_isGround    = false;

    // 重力スケール（1.0=通常、小さいほど落下が遅い。傘スロー落下などに使う）
    float m_gravityScale = 1.0f;

    // 惑星重力用：現在の「上」方向（物理・判定用、即スナップ）
    Math::Vector3 m_upDir = { 0.0f, 1.0f, 0.0f };

    // 見た目用「上」方向（Slerpで補間、モデル回転に使用）
    Math::Vector3 m_upDirVisual = { 0.0f, 1.0f, 0.0f };

    // ワープ演出用スケール倍率
    float         m_warpScale           = 1.0f;

    // ワープ中の向き上書き
    Math::Vector3 m_warpUpOverride       = { 0.0f, 1.0f, 0.0f };
    bool          m_warpUpOverrideActive = false;
    bool          m_warpStretchActive    = false;  // Traveling フェーズ中のみ true

    // 現在影響を受けている惑星のインデックス（-1 なら通常重力）
    int m_currentPlanetIndex = -1;

    // 現在惑星をインデックスから毎回取り直すヘルパー（生ポインタをキャッシュしない＝
    // エディタで惑星ベクタが再確保されてもダングリングしない）。範囲外なら nullptr。
    const PlanetData* CurrentPlanet() const;

    // 前フレームの惑星インデックス（惑星乗り移り検出用）
    int m_prevPlanetIndex = -1;

    // ComputeGravityInfluence の結果キャッシュ（ApplyGravity で1回だけ計算して使い回す）
    GravityInfluenceResult m_gravCache;
    bool                   m_gravCacheDirty = true;

    std::weak_ptr<KdGameObject> m_wpMap;

    // 移動床リスト（床・壁・天井判定に使用）
    std::vector<std::weak_ptr<MovingFloor>> m_movingFloors;

    // 風ボックスリスト（上面に乗れる床として判定）
    std::vector<std::weak_ptr<KdGameObject>> m_windBoxColliders;

    // 敵障害物リスト（めり込み防止押し出し）
    std::vector<std::weak_ptr<KdGameObject>> m_enemyObstacles;
    std::vector<std::weak_ptr<KdGameObject>> m_spikeBoxColliders;

    // CheckGround の移動床 XZ 判定用：ApplyVelocityHorizontal 前の位置
    Math::Vector3 m_preMovePos = {};

    // 現在乗っている移動床（nullptr なら乗っていない）
    // CheckGround で毎フレーム更新される
    const std::weak_ptr<MovingFloor>* m_pRidingFloor = nullptr;

    std::unique_ptr<KdDebugWireFrame> m_pDebugWire;

    // true にすると重力ゾーンの影響を受けない（Enemy用）
    // ManualGravity は保持したまま、ゾーンによるリセット・上書きをスキップする
    bool m_ignoreGravityZones = false;

    // 初期配置専用：キック・速度操作なしで重力方向だけをスナップセットする
    void SetInitialGravityDir(ManualGravityDir dir)
    {
        m_manualGravityDir = dir;
        if (dir == ManualGravityDir::None) { return; }
        m_upDir = (dir == ManualGravityDir::Up)
            ? Math::Vector3{ 0.0f, -1.0f, 0.0f }
            : Math::Vector3{ 0.0f,  1.0f, 0.0f };
        m_upDirVisual = m_upDir;
    }

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
