#pragma once
#include "../BaseScene/BaseScene.h"
#include "../../GameObject/Camera/EditorCamera.h"

class EditorScene : public BaseScene
{
public:
    EditorScene()  { Init(); }
    ~EditorScene() { KdDebugGUI::Instance().ClearGuiCallback(); }

private:
    void Init()    override;
    void Event()   override;

    EditorCamera* m_pCamera = nullptr;
};
