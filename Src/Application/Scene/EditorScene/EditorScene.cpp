#include "../../../Pch.h"
#include "EditorScene.h"
#include "../SceneManager.h"
#include "../../GameObject/Map/MapObject.h"

void EditorScene::Init()
{
    // カメラ
    auto upCamera = std::make_unique<EditorCamera>();
    m_pCamera     = upCamera.get();
    m_camera      = std::move(upCamera);

    m_mapEditor.Init();

    KdDebugGUI::Instance().SetGuiCallback([this]() { DrawGui(); });

    RebuildObjects();
}

void EditorScene::Event()
{
    // F1キーでゲームシーンに戻る
    if (GetAsyncKeyState(VK_F1) & 0x8000)
    {
        SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
    }

    // カメラ更新
    if (m_pCamera) { m_pCamera->Update(); }
}

void EditorScene::DrawGui()
{
    m_mapEditor.DrawGui();

    if (m_mapEditor.IsDirty())
    {
        RebuildObjects();
        m_mapEditor.ClearDirty();
    }
}

void EditorScene::RebuildObjects()
{
    m_objList.clear();

    for (const auto& data : m_mapEditor.GetObjectDataList())
    {
        auto spObj = std::make_shared<MapObject>();
        spObj->Init(data);
        AddObject(spObj);
    }
}
