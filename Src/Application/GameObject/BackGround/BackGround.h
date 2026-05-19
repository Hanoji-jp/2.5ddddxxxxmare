#pragma once

// 背景オブジェクト（2.5D奥行き表現用）
class BackGround : public KdGameObject
{
public:
    BackGround()          { Init(); }
    virtual ~BackGround() {}

    void Init()    override;
    void DrawLit() override;

    bool IsVisible() const override { return true; }

private:
    KdModelWork m_modelWork;
};
