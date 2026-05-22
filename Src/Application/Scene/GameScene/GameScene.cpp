#include "GameScene.h"
#include"../SceneManager.h"
#include"../../Const/LightConst.h"
#include"../../../Framework/Utility/KdDebug/KdDebugGUI.h"

void GameScene::Event()
{
	// F2でエディターモードのトグル（チャタリング防止）
	const bool f2Now = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
	if (f2Now && !m_f2Prev)
	{
		m_editorMode = !m_editorMode;

		if (m_editorMode)
		{
			// エディターカメラに切り替え（SideScrollCamera は破棄されるので観察ポインタを先に null にする）
			m_pCamera        = nullptr;
			auto upEditorCam = std::make_unique<EditorCamera>();
			m_pEditorCam     = upEditorCam.get();
			m_camera         = std::move(upEditorCam);

			KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
		}
		else
		{
			// ゲームカメラに戻す
			auto upGameCam = std::make_unique<SideScrollCamera>();
			m_pCamera      = upGameCam.get();
			m_camera       = std::move(upGameCam);
			m_pCamera->SetRooms(m_rooms);
			m_pEditorCam   = nullptr;

			KdDebugGUI::Instance().ClearGuiCallback();
		}
	}
	m_f2Prev = f2Now;

	// カメラ更新
	if (m_editorMode)
	{
		if (m_pEditorCam) { m_pEditorCam->Update(); }

		// エディターのDirtyチェック → オブジェクト再構築
		m_mapEditor.Update();
		if (m_mapEditor.IsDirty())
		{
			RebuildMapObjects();
			m_mapEditor.ClearDirty();
		}
	}
	else
	{
		if (m_spPlayer && m_pCamera) { m_pCamera->Update(m_spPlayer->GetPos()); }
	}
}

void GameScene::DrawDebugExtra()
{
	if (m_editorMode)
	{
		m_roomEditor.DrawDebugLines();
	}
}

void GameScene::DrawGui()
{
	m_mapEditor.DrawGui();
	m_roomEditor.DrawGui();
	CameraSettings::Instance().DrawGui();

	if (m_roomEditor.IsDirty())
	{
		m_rooms = m_roomEditor.GetRooms();
		if (m_pCamera) { m_pCamera->SetRooms(m_rooms); }
		m_roomEditor.ClearDirty();
	}
}

void GameScene::RebuildMapObjects()
{
	// 既存のMapObjectをリストから除去
	for (auto& obj : m_mapObjects)
	{
		obj->Expire();
	}
	m_mapObjects.clear();

	// MapEditorのデータからMapObjectを再生成してシーンに追加
	for (const auto& data : m_mapEditor.GetObjectDataList())
	{
		auto spObj = std::make_shared<MapObject>();
		spObj->Init(data);
		m_mapObjects.push_back(spObj);
		AddObject(spObj);
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
	m_spPlayer->SetPos(Math::Vector3(0.0f, 2.0f, 0.0f));
	AddObject(m_spPlayer);

	// 敵
	auto spEnemy = std::make_shared<Enemy>();
	spEnemy->SetPos(Math::Vector3(5.0f, 0.0f, 0.0f));
	spEnemy->SetTarget(m_spPlayer);
	AddObject(spEnemy);

	// マップ
	auto spMap = std::make_shared<Map>();
	AddObject(spMap);

 // ルームエディター初期化・読込
	m_roomEditor.Load();
	if (m_roomEditor.GetRooms().empty())
	{
		// CSVがない場合のデフォルトルーム
		m_rooms.clear();
		m_rooms.push_back({ -10.0f, 10.0f, 0.0f, 10.0f,  9.5f, 3.0f });
		m_rooms.push_back({  10.0f, 30.0f, 0.0f, 10.0f, 29.5f, 3.0f });
		m_rooms.push_back({  20.0f, 20.0f, 5.0f,  5.0f, FLT_MAX, 0.0f });
		m_roomEditor.SetRooms(m_rooms);
	}
	m_roomEditor.ClearDirty();
	m_rooms = m_roomEditor.GetRooms();
	m_pCamera->SetRooms(m_rooms);

	// マップエディター初期化
	m_mapEditor.Init();

	// カメラ設定読込
	CameraSettings::Instance().Load();

	// ライト設定
	auto& ambient = KdShaderManager::Instance().WorkAmbientController();
	ambient.SetAmbientLight(LightConst::AmbientColor);
	ambient.SetDirLight(LightConst::DirLightDir, LightConst::DirLightColor);
}

