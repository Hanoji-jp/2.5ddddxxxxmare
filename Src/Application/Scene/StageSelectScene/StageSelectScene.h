#pragma once

#include "../BaseScene/BaseScene.h"
#include "../../GameObject/Character/Player/Player.h"
#include "../../GameObject/Gimmick/MovingFloor.h"

//==========================================================
// StageSelectScene
// タイトルとゲームの間のハブ空間。プレイヤーが平地を歩き回り、
// ステージ入口（ポータル）に触れるとそのステージへ入る。
// ※惑星(PlanetGravityManager)を汚さないよう、ハブは惑星から遠い座標に置き、
//   プレイヤーに手動重力Downを与えて静的な MovingFloor の上を歩かせる。
//==========================================================
class StageSelectScene : public BaseScene
{
public:

	StageSelectScene()  { Init(); }
	~StageSelectScene() {}

private:

	void Event() override;
	void Init()  override;

	void DrawEffectExtra() override;   // ポータル（加算グロー）
	void DrawSpriteExtra() override;   // プロンプト＋入場フェード

	std::shared_ptr<Player>      m_spPlayer;
	std::shared_ptr<MovingFloor> m_spFloor;

	// ステージ入口（ポータル）
	std::shared_ptr<KdTexture> m_portalTex;
	KdSquarePolygon            m_portalPoly;
	Math::Vector3              m_portalPos;

	float m_timer     = 0.0f;
	bool  m_entering  = false;   // 入場フェード中
	float m_fadeAlpha = 0.0f;    // 入場時の白フェード

	// ── 投げ出され演出（イントロ）──
	bool  m_introActive = true;  // 落下〜不時着の演出中
	bool  m_landed      = false; // 不時着したか
	float m_introFade   = 1.0f;  // 黒フェードイン（1=黒→0）
	float m_settle      = 0.0f;  // 着地後カメラ復帰 0(落下カメラ)→1(通常カメラ)
	float m_shake       = 0.0f;  // 不時着の揺れ
	float m_introElapsed = 0.0f; // 演出経過（カメラ旋回角に使用）
	float m_introSpin    = 0.0f; // プレイヤーのタンブル角（rad）
};
