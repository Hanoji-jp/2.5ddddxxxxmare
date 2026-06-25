#pragma once

#include"../BaseScene/BaseScene.h"
#include"../../GameObject/Camera/EditorCamera.h"
#include"../../GameObject/Character/AnimBlender.h"
#include"../../UI/SettingsMenu.h"

//==========================================================
// TitleScene
// 宇宙に投げ出された世界観：流れ星（重力コア彗星＋トレイル）が流れ、
// Cubun とコアリアが無重力でくるくる回りながら漂う。
// 手前に CORELIA ロゴ＋点滅 PRESS ENTER。
//==========================================================
class TitleScene : public BaseScene
{
public :

	TitleScene()  { Init(); }
	~TitleScene() {}

private :

	void Event() override;
	void Init()  override;

	void DrawLitExtra()    override;   // Cubun / コアリア（3D）
	void DrawEffectExtra() override;   // 流れ星（加算）
	void DrawBrightExtra() override;   // ロゴ登場時の bloom（エンジンのブラーへ）
	void DrawSpriteExtra() override;   // ロゴ＋PRESS ENTER（2D）

	// 流れ星（重力コア彗星）
	struct Comet
	{
		Math::Vector3              pos;
		Math::Vector3              vel;
		Math::Color                color;
		float                      headSize  = 1.0f;
		std::vector<Math::Vector3> history;   // 尾用の位置履歴
	};
	void RespawnComet(Comet& c);

	// Enter 演出の進行度 0..1（光って縮む）
	float StartProgress() const;

	std::shared_ptr<KdTexture> m_logoTex;
	std::shared_ptr<KdTexture> m_cometCoreTex;
	std::shared_ptr<KdTexture> m_cometGlowTex;
	KdSquarePolygon            m_cometCorePoly;
	KdSquarePolygon            m_cometGlowPoly;
	std::vector<Comet>         m_comets;

	// 無重力で漂うキャラ
	KdModelWork  m_cubun;
	KdModelWork  m_corelia;
	AnimBlender  m_coreliaAnim;

	// カメラ：通常スクリプト／F2 フリーカメラ
	std::shared_ptr<KdCamera>     m_titleCam;
	std::shared_ptr<EditorCamera> m_freeCam;
	bool                          m_freeCamOn = false;
	bool                          m_f2Prev    = false;

	float m_timer    = 0.0f;
	float m_logTimer = 0.0f;   // カメラ値ログ出力の間引き用

	// Enter 演出
	bool  m_starting     = false;
	float m_startTimer   = 0.0f;
	bool  m_startKeyPrev = false;   // スタートEnterのエッジ判定（連打/貫通防止）

	// 設定ウィンドウ（TABで開く）
	SettingsMenu m_settingsMenu;
	bool         m_settingsTabPrev = false;
};
