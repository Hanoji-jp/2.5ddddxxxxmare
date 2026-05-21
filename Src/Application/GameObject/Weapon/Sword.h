#pragma once

// 剣モデルを表示する装備クラス
// プレイヤーの手の位置に追従して描画する
class Sword : public KdGameObject
{
public:
    Sword()          {}
    virtual ~Sword() {}

    void Init()    override;
    void DrawLit() override;

    bool IsVisible() const override { return true; }

    // 親（プレイヤー）のワールド行列を受け取って追従する
    void AttachTo(const Math::Matrix& _parentWorld);

private:
    KdModelWork m_modelWork;
    bool        m_isAttached = false;  // AttachTo が一度でも呼ばれたか
};
