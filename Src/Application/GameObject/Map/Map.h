#pragma once

class Map : public KdGameObject
{
public:
    Map()          { Init(); }
    virtual ~Map() {}

    void Init()       override;
    void PostUpdate() override;
    void DrawLit()    override;

    bool IsVisible()  const override { return true; }
    bool IsRideable() const override { return true; }

private:
    KdModelWork m_modelWork;
};
