#pragma once

#include "../BaseScene/BaseScene.h"
#include "../../GameObject/Character/AnimBlender.h"

//==========================================================
// StageSelectScene
// マリギャラ2のワールドマップ式ステージセレクト。
// 箱ノード（ステージ）をドットの道で繋ぎ、プレイヤー(カーソル)が
// WASDでノード間をホップ移動して選び、Enter/Spaceで決定して入る。
//==========================================================
class StageSelectScene : public BaseScene
{
public:

	StageSelectScene()  { Init(); }
	~StageSelectScene() {}

private:

	void Event() override;
	void Init()  override;

	void DrawLitExtra()    override;   // 箱ノード＋マーカー＋背景惑星
	void DrawOutlineExtra() override;  // ノード＆マーカーのアウトライン
	void DrawEffectExtra() override;   // 連結ドット＋後光＋光の粒
	void DrawSpriteExtra() override;   // 見出し＋操作ヒント＋フェード

	// ステージノード
	struct Node { Math::Vector3 pos; int stageId; Math::Vector3 color; };
	std::vector<Node>               m_nodes;
	std::vector<std::pair<int,int>> m_links;   // ノード間の道
	int   m_current  = 0;                       // 現在選択中のノード

	// カーソル移動（ホップ）
	bool  m_moving   = false;
	int   m_moveFrom = 0;
	int   m_moveTo   = 0;
	float m_moveT    = 0.0f;
	float m_markerYaw = 0.0f;

	// モデル
	KdModelWork m_nodeModel;   // 箱（ステージノード・共有）
	KdModelWork m_marker;      // プレイヤー（カーソル）
	AnimBlender m_markerAnim;
	KdModelWork m_bgPlanet;    // 背景惑星

	// グロー素材
	std::shared_ptr<KdTexture> m_dotTex;
	KdSquarePolygon            m_dotPoly;

	// 漂う光の粒
	struct Mote { Math::Vector3 pos; float size; float spin; };
	std::vector<Mote> m_motes;

	Math::Vector3 m_camFocus = { 0.0f, 0.0f, 0.0f }; // カメラ注視点（選択中ノードへ追従）

	float m_timer     = 0.0f;
	float m_introFade = 1.0f;   // 開始の黒フェード
	bool  m_entering  = false;  // 決定→入場中
	float m_fadeAlpha = 0.0f;
	bool  m_prevW = false, m_prevA = false, m_prevS = false, m_prevD = false; // キーのエッジ検出
	bool  m_advPrev   = false;  // 決定キーのエッジ検出

	// グリッド構成（行×列）
	static constexpr int kCols = 3;
	static constexpr int kRows = 2;

	// ヘルパー
	Math::Vector3 MarkerPos() const;     // 補間込みのマーカー位置
	void          StartMove(int target); // 指定ノードへホップ開始
};
