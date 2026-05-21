#pragma once
#include "../BaseScene/BaseScene.h"
#include "../../Editor/MapEditor.h"
#include "../../GameObject/Camera/EditorCamera.h"

class EditorScene : public BaseScene
{
public:
    EditorScene()  { Init(); }
    ~EditorScene() { KdDebugGUI::Instance().ClearGuiCallback(); }

private:
    void Init()    override;
    void Event()   override;
    void DrawGui();

    void RebuildObjects();

    MapEditor     m_mapEditor;
    EditorCamera* m_pCamera = nullptr;
};
