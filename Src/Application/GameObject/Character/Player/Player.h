#pragma once
#include "../Character.h"
#include "../../../Const/PlayerConst.h"

class Player : public Character
{
public:
    Player()          { Init(); }
    virtual ~Player() {}

    void Init()   override;
    void Update() override;
    void DrawLit() override;

    bool IsVisible() const override { return true; }

private:
    void Move();
    void Jump();
    void CheckGround() override;

    KdModelWork m_modelWork;
};
