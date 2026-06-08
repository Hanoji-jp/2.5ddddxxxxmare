#include "../../../Pch.h"
#include "EditorScene.h"
#include "../SceneManager.h"

void EditorScene::Init()
{
    auto spCamera = std::make_shared<EditorCamera>();
    m_pCamera     = spCamera.get();
    m_camera      = spCamera;
}

void EditorScene::Event()
{
    if (GetAsyncKeyState(VK_F1) & 0x8000)
    {
        SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
    }

    if (m_pCamera) { m_pCamera->Update(); }
}
