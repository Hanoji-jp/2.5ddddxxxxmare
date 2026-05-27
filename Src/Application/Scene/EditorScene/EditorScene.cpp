#include "../../../Pch.h"
#include "EditorScene.h"
#include "../SceneManager.h"

void EditorScene::Init()
{
    auto upCamera = std::make_unique<EditorCamera>();
    m_pCamera     = upCamera.get();
    m_camera      = std::move(upCamera);
}

void EditorScene::Event()
{
    if (GetAsyncKeyState(VK_F1) & 0x8000)
    {
        SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
    }

    if (m_pCamera) { m_pCamera->Update(); }
}
