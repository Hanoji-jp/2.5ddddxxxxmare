#include "GameScene.h"
#include"../SceneManager.h"
#include"../../Const/LightConst.h"
#include"../../../Framework/Utility/KdDebug/KdDebugGUI.h"
#include <fstream>
#include <sstream>

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

			// HP UIはゲーム中も常時表示するのでコールバックは維持
			KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
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
		if (m_enemyEditor.IsDirty())
		{
			RebuildEnemies();
			m_enemyEditor.ClearDirty();
		}
		if (m_checkpointEditor.IsDirty())
		{
			RebuildCheckpoints();
			m_checkpointEditor.ClearDirty();
		}
	}
	else
	{
		if (m_spPlayer && m_pCamera) { m_pCamera->Update(m_spPlayer->GetPos()); }

		// ── 落下死チェック ──────────────────────────────
		if (m_spPlayer && !m_spPlayer->IsExpired())
		{
			if (m_spPlayer->GetPos().y < CheckpointConst::DeathY)
			{
				Respawn();
			}
		}

		// ── チェックポイント更新 ─────────────────────────
		for (auto& cp : m_checkpoints)
		{
			if (cp->IsActivated())
			{
				// このチェックポイントを有効化、他を無効化
				m_respawnPos = cp->GetPos();
				for (auto& other : m_checkpoints)
				{
					if (other != cp) { other->Deactivate(); }
				}
			}
		}
	}
}

void GameScene::DrawDebugExtra()
{
	if (m_editorMode)
	{
		m_roomEditor.DrawDebugLines();
		m_enemyEditor.DrawDebugSpheres();
		m_checkpointEditor.DrawDebugSpheres();
		PlanetGravityManager::Instance().DrawDebugSpheres();
	}
}

void GameScene::DrawGui()
{
	// HP UI は常時表示（エディターモード問わず）
	if (m_spHpUI) { m_spHpUI->DrawGui(); }

	// エディターモード時のみエディターGUIを表示
	m_mapEditor.DrawGui();
	m_roomEditor.DrawGui();
	m_enemyEditor.DrawGui();
	m_checkpointEditor.DrawGui();
	CameraSettings::Instance().DrawGui();
	PlanetGravityManager::Instance().DrawGui();

	if (m_roomEditor.IsDirty())
	{
		m_rooms = m_roomEditor.GetRooms();
		if (m_pCamera) { m_pCamera->SetRooms(m_rooms); }
		m_roomEditor.ClearDirty();
	}

	// スポーン位置エディター
	if (ImGui::Begin("Spawn Settings"))
	{
		float pos[3] = { m_spawnPos.x, m_spawnPos.y, m_spawnPos.z };
		if (ImGui::DragFloat3("Spawn Position", pos, 0.1f))
		{
			m_spawnPos = { pos[0], pos[1], pos[2] };
		}

		if (ImGui::Button("Apply (Respawn)"))
		{
			if (m_spPlayer) { m_spPlayer->SetPos(m_spawnPos); }
		}
		ImGui::SameLine();
		if (ImGui::Button("Save"))
		{
			SaveSpawn();
		}
	}
	ImGui::End();
}

void GameScene::Respawn()
{
	if (!m_spPlayer) { return; }
	m_spPlayer->SetPos(m_respawnPos);
	// HPを全回復（Player側にリセット関数があれば呼ぶ）
	m_spPlayer->Init();
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

void GameScene::RebuildEnemies()
{
	// 既存の敵をシーンから除去
	for (auto& e : m_enemies)
	{
		e->Expire();
	}
	m_enemies.clear();

	// EnemyPlacementEditor のデータから敵を生成
	for (const auto& data : m_enemyEditor.GetPlacements())
	{
		std::shared_ptr<Enemy> spEnemy;

		if (data.type == EnemyType::Melee)
		{
			auto sp = std::make_shared<EnemyMelee>();
			sp->SetPos(data.position);
			sp->Init();
			spEnemy = sp;
		}
		else
		{
			auto sp = std::make_shared<EnemyRanged>();
			sp->SetPos(data.position);
			sp->Init();
			spEnemy = sp;
		}

		if (m_spMap)    { spEnemy->SetMapObject(m_spMap); }
		if (m_spPlayer) { spEnemy->SetTarget(m_spPlayer); }

		m_enemies.push_back(spEnemy);
		AddObject(spEnemy);
	}
}

void GameScene::RebuildCheckpoints()
{
	// 既存チェックポイントを除去
	for (auto& cp : m_checkpoints) { cp->Expire(); }
	m_checkpoints.clear();

	for (const auto& pos : m_checkpointEditor.GetPositions())
	{
		auto cp = std::make_shared<Checkpoint>();
		cp->SetPos(pos);
		cp->SetPlayer(m_spPlayer);
		m_checkpoints.push_back(cp);
		AddObject(cp);
	}
}

void GameScene::Init()
{
	// カメラ（BaseSceneのm_cameraに所有権を渡し、観察用ポインタだけ保持）
	auto upCamera  = std::make_unique<SideScrollCamera>();
	m_pCamera      = upCamera.get();
	m_camera       = std::move(upCamera);

	// マップ（先にAddObjectしてPostUpdate→CalcNodeMatricesが敵より前に走るようにする）
	m_spMap = std::make_shared<Map>();
	AddObject(m_spMap);

	// プレイヤー
	m_spPlayer = std::make_shared<Player>();
	LoadSpawn();
	m_spPlayer->SetPos(m_spawnPos);
	m_spPlayer->SetMapObject(m_spMap);
	AddObject(m_spPlayer);

	// リスポーン座標の初期値をスポーン座標と揃える
	m_respawnPos = m_spawnPos;

	// HP UI
	m_spHpUI = std::make_shared<HpUI>();
	m_spHpUI->SetPlayer(m_spPlayer);
	AddObject(m_spHpUI);

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

	// 敵配置エディター（Loadはコンストラクタ内で実行済み）→敵を生成
	if (m_enemyEditor.IsDirty())
	{
		RebuildEnemies();
		m_enemyEditor.ClearDirty();
	}

	// チェックポイントエディター → チェックポイント生成
	if (m_checkpointEditor.IsDirty())
	{
		RebuildCheckpoints();
		m_checkpointEditor.ClearDirty();
	}

	// カメラ設定読込
	CameraSettings::Instance().Load();

	// ライト設定
	auto& ambient = KdShaderManager::Instance().WorkAmbientController();
	ambient.SetAmbientLight(LightConst::AmbientColor);
	ambient.SetDirLight(LightConst::DirLightDir, LightConst::DirLightColor);

	// HP UI を常時表示するためゲーム開始時からコールバックをセット
	KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
}

void GameScene::SaveSpawn()
{
	std::ofstream ofs(SpawnConst::SavePath);
	if (!ofs) { return; }
	ofs << m_spawnPos.x << "," << m_spawnPos.y << "," << m_spawnPos.z << "\n";
}

void GameScene::LoadSpawn()
{
	std::ifstream ifs(SpawnConst::SavePath);
	if (!ifs) { return; }

	std::string line;
	if (!std::getline(ifs, line)) { return; }

	std::istringstream ss(line);
	std::string token;
	float vals[3] = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };
	int i = 0;
	while (std::getline(ss, token, ',') && i < 3)
	{
		vals[i++] = std::stof(token);
	}
	m_spawnPos = { vals[0], vals[1], vals[2] };
}
