#pragma once

// スカイボックス背景オブジェクト
class BackGround : public KdGameObject
{
public:
    BackGround()          { Init(); }
    virtual ~BackGround() {}

    void Init()      override;
    void Update()    override;
    void DrawUnLit() override;

    bool IsVisible() const override { return true; }

private:
    KdModelWork m_modelWork;
};
