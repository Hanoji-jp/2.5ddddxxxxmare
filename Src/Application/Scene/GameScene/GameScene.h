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
#include"../../GameObject/Camera/EditorCamera.h"

class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene() {}

private:

	void Event()              override;
	void Init()               override;
	void DrawGui()            override;
	void RebuildMapObjects();

	std::shared_ptr<Player> m_spPlayer   = nullptr;
	SideScrollCamera*       m_pCamera    = nullptr;
	std::vector<RoomBounds> m_rooms;

	// インゲームマップエディター
	MapEditor               m_mapEditor;
	bool                    m_editorMode  = false;
	bool                    m_f2Prev      = false;
	EditorCamera*           m_pEditorCam  = nullptr;

	// エディター配置オブジェクトリスト
	std::vector<std::shared_ptr<MapObject>> m_mapObjects;
};
