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

	// ── クリア後リザルト（カメラをプレイヤーへ寄せて横にパネル）──
	bool  m_resultActive  = false;   // リザルト表示中（入力は決定でしか抜けない）
	float m_resultZoom    = 0.0f;    // 0=俯瞰, 1=寄り（カメラ補間用）
	bool  m_resultAdvPrev = false;   // 決定キーのエッジ検出（閉じる用）
	int   m_resStageId    = 0;       // 0始まりの stageId
	int   m_resCoins      = 0;
	int   m_resRocks      = 0;
	int   m_resDeaths     = 0;
	float m_resTime       = 0.0f;
	int   m_resStep       = 1;       // 表示中のページ（1..Pages → 最後の次で閉じてタリーへ）
	float m_resCardAnim   = 0.0f;    // ページ入れ替えポップ用（経過秒）

	// アイコン（リザルト/HUD/タリー共用）
	std::shared_ptr<KdTexture> m_coinTex;
	std::shared_ptr<KdTexture> m_rockTex;

	// HUD合計の取得ポップ（加算時に一瞬拡大）
	float m_coinHudPop = 0.0f;
	float m_rockHudPop = 0.0f;

	// クリア入手分を UI へ飛ばす「タリー」演出
	bool  m_tallyActive = false;
	struct TallyFlyer
	{
		bool          isRock = false;
		Math::Vector3 start  = {};   // 開始(スクリーン中心原点)
		float         delay  = 0.0f; // 発射までの待ち
		float         t      = 0.0f; // 0..1 進行
		int           add    = 1;    // 到達時に合計へ加算する量
		bool          done   = false;
	};
	std::vector<TallyFlyer> m_flyers;

	// HUDアイコンのスクリーン位置（中心原点）。タリーの着地先
	Math::Vector3 HudCoinPos() const;
	Math::Vector3 HudRockPos() const;
	void          DrawTotalsHud();   // 左上の合計コイン/rock
	void          UpdateTally(float dt);
	void          StartTally();

	// ヘルパー
	Math::Vector3 MarkerPos() const;     // 補間込みのマーカー位置
	void          StartMove(int target); // 指定ノードへホップ開始
	void          DrawResultPanel();     // リザルトパネル描画
};
