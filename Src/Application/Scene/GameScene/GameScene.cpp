#include "GameScene.h"
#include"../SceneManager.h"

void GameScene::Event()
{
	if (m_spPlayer && m_pCamera)
	{
		m_pCamera->Update(m_spPlayer->GetPos());
	}

	if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}
}

void GameScene::Init()
{
	// カメラ（BaseSceneのm_cameraに所有権を渡し、観察用ポインタだけ保持）
	auto upCamera  = std::make_unique<SideScrollCamera>();
	m_pCamera      = upCamera.get();
	m_camera       = std::move(upCamera);

	// プレイヤー
	m_spPlayer = std::make_shared<Player>();
	AddObject(m_spPlayer);

	// 敵
	auto spEnemy = std::make_shared<Enemy>();
	spEnemy->SetPos(Math::Vector3(5.0f, 0.0f, 0.0f));
	spEnemy->SetTarget(m_spPlayer);
	AddObject(spEnemy);

	// マップ
	auto spMap = std::make_shared<Map>();
	AddObject(spMap);
}

