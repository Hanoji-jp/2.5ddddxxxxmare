#pragma once

#include"../BaseScene/BaseScene.h"
#include"../../GameObject/Character/Player/Player.h"
#include"../../GameObject/Character/Enemy/Enemy.h"
#include"../../GameObject/Character/Enemy/EnemyMelee.h"
#include"../../GameObject/Character/Enemy/EnemyRanged.h"
#include"../../GameObject/BackGround/BackGround.h"
#include"../../GameObject/Camera/SideScrollCamera.h"
#include"../../GameObject/Camera/RoomBounds.h"
#include"../../Editor/RoomBoundsEditor.h"
#include"../../Editor/EnemyPlacementEditor.h"
#include"../../Editor/CheckpointEditor.h"
#include"../../Manager/PlanetGravityManager.h"
#include"../../Manager/ManualGravityZoneManager.h"
#include"../../Camera/CameraSettings.h"
#include"../../GameObject/Camera/EditorCamera.h"
#include"../../Const/SpawnConst.h"
#include"../../GameObject/Checkpoint/Checkpoint.h"
#include"../../Const/CheckpointConst.h"
#include"../../GameObject/UI/HpUI.h"
#include"../../GameObject/Light/PointLightObject.h"

class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene() {}

private:

	void Event()              override;
	void Init()               override;
	void DrawGui()            override;
	void DrawDebugExtra()     override;
	void DrawUnLitExtra()     override;  // 背景Box描画
	void DrawLitExtra()       override;  // 惑星モデル描画

	void RebuildEnemies();
	void RebuildCheckpoints();

	std::shared_ptr<Player>  m_spPlayer   = nullptr;
	SideScrollCamera*        m_pCamera    = nullptr;
	std::vector<RoomBounds> m_rooms;

	// インゲームマップエディター
	RoomBoundsEditor        m_roomEditor;
	EnemyPlacementEditor    m_enemyEditor;
	CheckpointEditor        m_checkpointEditor;
	bool                    m_editorMode  = false;
	bool                    m_f2Prev      = false;
	EditorCamera*           m_pEditorCam  = nullptr;

	// エディター配置敵リスト
	std::vector<std::shared_ptr<Enemy>> m_enemies;

	// プレイヤースポーン座標
	Math::Vector3           m_spawnPos       = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };

	// チェックポイントリスト＋現在有効なリスポーン座標
	std::vector<std::shared_ptr<Checkpoint>> m_checkpoints;
	Math::Vector3           m_respawnPos     = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };

	// HP UI
	std::shared_ptr<HpUI>   m_spHpUI;

	// ポイントライトリスト
	std::vector<std::shared_ptr<PointLightObject>> m_pointLights;

	void Respawn();
	void SaveSpawn();
	void LoadSpawn();
};
