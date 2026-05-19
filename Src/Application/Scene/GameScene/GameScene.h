#pragma once

#include"../BaseScene/BaseScene.h"
#include"../../GameObject/Character/Player/Player.h"
#include"../../GameObject/Character/Enemy/Enemy.h"
#include"../../GameObject/BackGround/BackGround.h"
#include"../../GameObject/Camera/SideScrollCamera.h"
#include"../../GameObject/Map/Map.h"

class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene() {}

private:

	void Event() override;
	void Init()  override;

	std::shared_ptr<Player> m_spPlayer  = nullptr;
	SideScrollCamera*       m_pCamera   = nullptr;
};
