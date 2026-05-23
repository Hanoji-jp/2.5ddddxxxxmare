#pragma once

#include"../BaseScene/BaseScene.h"
#include"../../GameObject/Character/Player/Player.h"
#include"../../GameObject/Character/Enemy/Enemy.h"
#include"../../GameObject/BackGround/BackGround.h"
#include"../../GameObject/Camera/SideScrollCamera.h"
#include"../../GameObject/Camera/RoomBounds.h"
#include"../../GameObject/Map/Map.h"
#include"../../GameObject/Map/MapObject.h"
#include"../../Editor/MapEditor.h"
#include"../../Editor/RoomBoundsEditor.h"
#include"../../Camera/CameraSettings.h"
#include"../../GameObject/Camera/EditorCamera.h"
#include"../../Const/SpawnConst.h"

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
	void RebuildMapObjects();

	std::shared_ptr<Player>  m_spPlayer   = nullptr;
	std::shared_ptr<Map>     m_spMap      = nullptr;
	SideScrollCamera*        m_pCamera    = nullptr;
	std::vector<RoomBounds> m_rooms;

	// インゲームマップエディター
	MapEditor               m_mapEditor;
	RoomBoundsEditor        m_roomEditor;
	bool                    m_editorMode  = false;
	bool                    m_f2Prev      = false;
	EditorCamera*           m_pEditorCam  = nullptr;

	// エディター配置オブジェクトリスト
	std::vector<std::shared_ptr<MapObject>> m_mapObjects;

	// プレイヤースポーン座標
	Math::Vector3           m_spawnPos    = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };
	void SaveSpawn();
	void LoadSpawn();
};
