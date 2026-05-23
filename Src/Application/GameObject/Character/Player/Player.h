#pragma once
#include "../Character.h"
#include "../AnimBlender.h"
#include "../../../Const/PlayerConst.h"
#include "../../Weapon/Sword.h"
#include "../../Weapon/Bow.h"
#include "../../Weapon/Arrow.h"

class Player : public Character
{
public:
    Player()          { Init(); }
    virtual ~Player() {}

    void Init()       override;
    void Update()     override;
    void PostUpdate() override;
    void DrawLit()    override;

    bool IsVisible() const override { return true; }

private:
    void Move();
    void Jump();
    void AttackMelee();
    void AttackRanged();
    // アニメーション切り替え
    void ChangeAnim(const std::string& _animName, bool _isLoop = true);

    KdModelWork  m_modelWork;
    AnimBlender  m_animBlender;

    // 描画専用ワールド行列（ピボット補正オフセットを含む。コリジョンには使わない）
    Math::Matrix m_drawWorld;

    // 現在再生中のアニメーション名（同アニメの再セット防止）
    std::string  m_currentAnimName;

    // 装備
    std::shared_ptr<Sword>  m_sword;
    std::shared_ptr<Bow>    m_bow;

    // 飛翔中の矢リスト（シーンへの追加は GameScene が行う想定）
    std::vector<std::shared_ptr<Arrow>> m_arrows;

    // 向いている方向（XZ平面。初期値は右向き）
    Math::Vector3 m_facingDir    = { 1.0f, 0.0f, 0.0f };

    // XZ移動の慣性速度（加減速に使用）
    Math::Vector3 m_moveVelocity = { 0.0f, 0.0f, 0.0f };

    // 近接攻撃クールダウン
    int m_meleeCooldown = 0;

    // 遠距離攻撃クールダウン
    int m_rangedCooldown = 0;
};

