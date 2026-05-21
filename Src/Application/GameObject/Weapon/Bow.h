#pragma once

// 弓モデルを表示する装備クラス
class Bow : public KdGameObject
{
public:
    Bow()          {}
    virtual ~Bow() {}

    void Init()    override;
    void DrawLit() override;

    bool IsVisible() const override { return true; }

    // 親（プレイヤー）のワールド行列を受け取って追従する
    void AttachTo(const Math::Matrix& _parentWorld);

private:
    KdModelWork m_modelWork;
    bool        m_isAttached = false;
};
