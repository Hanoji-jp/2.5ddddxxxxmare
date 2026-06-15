#pragma once
#include "../Character.h"
#include "../AnimBlender.h"
#include "../../../Const/PlayerConst.h"
#include "../../../Const/WindBoxConst.h"
#include "../../Weapon/Sword.h"
#include "../../Weapon/Bow.h"
#include "../../Weapon/Arrow.h"
#include "../../Item/HitBox.h"

class Player : public Character
{
public:
    Player()          { Init(); }
    virtual ~Player() {}

    // m_animBlender が m_modelWork の生アドレスを持つためコピー・ムーブ禁止
    Player(const Player&)            = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&)                 = delete;
    Player& operator=(Player&&)      = delete;

    void Init()       override;
    void Update()     override;
    void PostUpdate() override;
    void DrawLit()    override;

    bool IsVisible() const override { return true; }

    void TakeDamage(int _damage) override;
    void TakeDamageFrom(int _damage, const Math::Vector3& sourcePos) override;

    // 即死（無敵を無視して HP を 0 にする。ドッスンの叩きつけ等）
    void InstantDeath();

    // GhostTrail 用：描画ワールド行列を公開
    const Math::Matrix& GetDrawWorld() const { return m_drawWorld; }
    // GhostTrail 用：モデルデータを公開
    std::shared_ptr<KdModelData> GetModelData() const { return m_modelWork.GetData(); }
    // FootDust 用：ダッシュ中かどうか
    bool IsDashing() const { return m_isDashing; }

    // アイテム取得用ヒットボックスを公開（ItemManager が毎フレーム参照）
    HitBox& GetPickupHitBox() { return m_pickupHitBox; }

    // 今フレームに惑星が切り替わったか（GameScene→カメラへのズームトリガー用）
    bool IsPlanetChanged() const { return m_planetChangedThisFrame; }
    void ResetPlanetChangedFlag()  { m_planetChangedThisFrame = false; }

    // パラソルアイテム取得
    void GiveParasol() { m_hasParasol = true; }

private:
    void Move();
    void Jump();
    void AttackMelee();
    void AttackRanged();
    // アニメーション切り替え
    void ChangeAnim(const std::string& _animName, bool _isLoop = true);
    bool ChangeAnimIfExist(const std::string& _animName, bool _isLoop = true);

    KdModelWork  m_modelWork;
    AnimBlender  m_animBlender;

    // アイテム取得用ヒットボックス
    HitBox       m_pickupHitBox;

    // 描画専用ワールド行列（ピボット補正オフセットを含む。コリジョンには使わない）
    Math::Matrix m_drawWorld;

    // 現在再生中のアニメーション名（同アニメの再セット防止）
    std::string  m_currentAnimName;

    // 装備
    std::shared_ptr<Sword>  m_sword;
    std::shared_ptr<Bow>    m_bow;

    // 飛翔中の矢リスト（シーンへの追加は GameScene が行う想定）
    std::vector<std::shared_ptr<Arrow>> m_arrows;

    // 接線方向の左右符号（+1 = tangent 向き、-1 = 逆向き）
    float m_facingSign = 1.0f;

    // XZ移動の慣性速度（加減速に使用）
    Math::Vector3 m_moveVelocity  = {};

    // 被ダメージ後の無敵タイマー（フレーム。> 0 の間は再被弾しない）
    int m_invincibleTimer = 0;

    // 近接攻撃クールダウン
    int m_meleeCooldown = 0;

    // 遠距離攻撃クールダウン
    int m_rangedCooldown = 0;

    // 着地スクワッシュ（0=なし、>0=スクワッシュ中）
    float m_squashTimer = 0.0f;

    // 前フレームの着地状態（エッジ検出用）
    bool m_wasGround = false;

    // ダッシュ中フラグ
    bool m_isDashing = false;

    // 攻撃モーション再生中フラグ（上半身 Attack 上書き管理用）
    bool m_isAttacking = false;

    // 傘アイテム所持中（未実装：デバッグキー P で取得/無）
    bool m_hasParasol = false;

    // 傘を開いているか（Fall 中のみ有効）
    bool m_isParasolOpen = false;

    // 今フレームに惑星切り替わりが発生したか
    bool m_planetChangedThisFrame = false;

    // アニメーション再生速度倍率（通常=1.0、ダッシュ時は DashAnimSpeedMul）
    float m_animSpeed = 1.0f;

    // ──── 風ギミック ──────────────────────────────────────────
    // 今フレームの風ベクトル（GameScene から ApplyWind() で積算、PostUpdate で消費）
    Math::Vector3 m_windAccum     = {};

    // 風による見た目の傾き角度（度）：正=右へ傾く
    float         m_windTiltAngle = 0.0f;

    // 風ボックス範囲内かどうかのフラグ（前フレーム）
    bool          m_wasInWind        = false;

    // 今フレームの風が左右方向かどうか
    bool          m_windIsHorizontal = false;

    // 水平方向の風速（Move() に上書きされないよう別管理）
    Math::Vector3 m_windHorizVel  = {};

public:
    // 風ボックスから毎フレーム呼ぶ（複数ボックスが重なっても加算で対応）
    void ApplyWind(const Math::Vector3& windDir, float power);

    // 風ボックス範囲外に出たときに呼ぶ（傾きと上昇フラグをリセット）
    void ClearWindState();
};


