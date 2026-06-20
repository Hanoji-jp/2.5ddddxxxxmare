#include "SceneManager.h"

#include <Windows.h>
#include "BaseScene/BaseScene.h"
#include "TitleScene/TitleScene.h"
#include "StoryScene/StoryScene.h"
#include "StageSelectScene/StageSelectScene.h"
#include "GameScene/GameScene.h"
#include "EditorScene/EditorScene.h"

void SceneManager::PreUpdate()
{
	// 入力ロックを先に減らす（この後シーン切替があれば ChangeScene で再セットされ、
	// 切替フレームは満タンのロックが維持される）
	if (m_inputLockFrames > 0) { --m_inputLockFrames; }

	// 切替後、決定キー(Enter/Space/Tab)が一度でも離されたら持ち越しロックを解除。
	// （押しっぱなしのまま新シーンへ来ても、離すまで決定が効かない）
	const bool confirmDown =
		((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) ||
		((GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0) ||
		((GetAsyncKeyState(VK_TAB)    & 0x8000) != 0);
	if (!confirmDown) { m_confirmReleased = true; }

	// シーン切替
	if (m_currentSceneType != m_nextSceneType)
	{
		ChangeScene(m_nextSceneType);
	}

	m_currentScene->PreUpdate();
}

void SceneManager::Update()
{
	m_currentScene->Update();
}

void SceneManager::PostUpdate()
{
	m_currentScene->PostUpdate();
}

void SceneManager::PreDraw()
{
	m_currentScene->PreDraw();
}

void SceneManager::Draw()
{
	m_currentScene->Draw();
}

void SceneManager::DrawSprite()
{
	m_currentScene->DrawSprite();
}

void SceneManager::DrawDebug()
{
	m_currentScene->DrawDebug();
}

const std::list<std::shared_ptr<KdGameObject>>& SceneManager::GetObjList()
{
	return m_currentScene->GetObjList();
}

void SceneManager::AddObject(const std::shared_ptr<KdGameObject>& _obj)
{
	m_currentScene->AddObject(_obj);
}

void SceneManager::ChangeScene(SceneType _sceneType)
{
	// 次のシーンを作成し、現在のシーンにする
	switch (_sceneType)
	{
	case SceneType::Title:
		m_currentScene = std::make_shared<TitleScene>();
		break;
	case SceneType::Story:
		m_currentScene = std::make_shared<StoryScene>();
		break;
	case SceneType::StageSelect:
		m_currentScene = std::make_shared<StageSelectScene>();
		break;
	case SceneType::Game:
		m_currentScene = std::make_shared<GameScene>();
		break;
	case SceneType::Editor:
		m_currentScene = std::make_shared<EditorScene>();
		break;
	}

	// 現在のシーン情報を更新
	m_currentSceneType = _sceneType;

	// シーン切替直後は入力をロック（前シーンの押しっぱなしが即発火するのを防ぐ）
	// タイマー＋「決定キーを一度離すまで」の二重ロック
	m_inputLockFrames = kInputLockFrames;
	m_confirmReleased = false;
}
