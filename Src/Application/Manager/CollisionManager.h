#pragma once
#include "../Const/CollisionConst.h"

//==========================================================
// CollisionManager
// Mapモデルの col_ メッシュを使ってキャラクターの
// 床・壁判定を行うシングルトン
//==========================================================
class CollisionManager
{
public:
    static CollisionManager& Instance()
    {
        static CollisionManager inst;
        return inst;
    }

    // マップモデルをセット（GameScene::Init で呼ぶ）
    void SetMapModel(const KdModelWork* _pModel) { m_pMapModel = _pModel; }

    // 床判定：足元にレイを飛ばして着地位置を返す
    // _pos       : キャラクター位置
    // _outGroundY: 着地Y座標（hit時のみ有効）
    // 戻り値     : 地面に当たったか
    bool CheckGround(const Math::Vector3& _pos, float& _outGroundY) const;

    // 壁判定：スフィアで押し返しベクトルを返す
    // _pos       : キャラクター位置
    // _outPush   : 押し返し量（hitしなければゼロ）
    // 戻り値     : 壁に当たったか
    bool CheckWall(const Math::Vector3& _pos, Math::Vector3& _outPush) const;

private:
    CollisionManager()  = default;
    ~CollisionManager() = default;

    const KdModelWork* m_pMapModel = nullptr;
};
