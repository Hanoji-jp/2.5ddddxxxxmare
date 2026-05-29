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
#include"../../Editor/WarpHoleEditor.h"
#include"../../GameObject/Gimmick/WarpHole.h"
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
	void RebuildWarpHoles();

	std::shared_ptr<Player>  m_spPlayer   = nullptr;
	SideScrollCamera*        m_pCamera    = nullptr;
	std::vector<RoomBounds> m_rooms;

	// インゲームマップエディター
	RoomBoundsEditor        m_roomEditor;
	EnemyPlacementEditor    m_enemyEditor;
	CheckpointEditor        m_checkpointEditor;
	WarpHoleEditor          m_warpHoleEditor;

	// ワープホールオブジェクトリスト
	std::vector<std::shared_ptr<WarpHole>> m_warpHoles;

	//---- Waypoint ワープ進行状態 ----
	enum class WarpPhase
	{
		None,       // 通常
		Sucking,    // 吸い込み中（入口に向かって収縮・SetPos制御）
		Traveling,  // パス移動中
	};
	WarpPhase                              m_warpPhase         = WarpPhase::None;
	std::vector<Math::Vector3>             m_warpPath;
	int                                    m_warpSegment       = 0;
	float                                  m_warpSegProgress   = 0.0f;
	Math::Vector3                          m_warpExitDir;
	Math::Vector3                          m_warpEntryPos;
	Math::Vector3                          m_warpSuckStartPos; // 吸い込み開始時のプレイヤー位置
	float                                  m_warpSuckProgress  = 0.0f; // 0→1
	float                                  m_warpSuckStartAngle = 0.0f; // 螺旋開始角度
	float                                  m_warpPlayerScale   = 1.0f;

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
