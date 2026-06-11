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
    // スカイボックスは常時描画対象
    bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

private:
    KdModelWork m_modelWork;
};

