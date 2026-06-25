#pragma once

#include "../BaseScene/BaseScene.h"
#include "../../GameObject/Character/AnimBlender.h"
#include "../../Util/UiButton.h"
#include "../../Util/CoinModelIcon.h"
#include "../../UI/SettingsMenu.h"
#include "../../../Framework/Shader/KdRenderTargetChange.h"

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
	float m_enterAnim = 0.0f;   // 入場演出の経過秒（ぴょん→縮んで箱へ）
	float m_fadeAlpha = 0.0f;
	bool  m_prevW = false, m_prevA = false, m_prevS = false, m_prevD = false; // キーのエッジ検出
	bool  m_advPrev   = false;  // 決定キーのエッジ検出

	// グリッド構成（行×列）
	static constexpr int kCols = 3;
	static constexpr int kRows = 2;

	// ── ステージ選択のワンクッション（決定でカメラが寄って詳細表示）──
	bool  m_selecting   = false;   // 詳細画面表示中（カメラが選択ノードへ寄る）
	float m_selZoom     = 0.0f;    // 0=俯瞰, 1=寄り
	bool  m_selBackPrev = false;   // 戻るキーのエッジ検出
	int   m_selChoice   = 0;       // 詳細画面の選択肢（0=GO, 1=BACK）。A/Dで切替
	UiButton m_btnGo;             // GO ボタン
	UiButton m_btnBack;           // BACK ボタン
	std::vector<std::shared_ptr<KdTexture>> m_stageThumbs;   // stageId別のサムネ画像（無ければnullptr）
	void  DrawSelectPanel();       // 選択ステージの詳細（名前/説明/クリア/最高コイン/ベストタイム）
	// 角丸フレーム（金縁＋背景）を1回で描く共通ヘルパー
	void  DrawRoundedFrame(float cx, float cy, float halfW, float halfH, float radius,
		const Math::Color& body, const Math::Color& edge);

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
	float m_resultOutAnim = 0.0f;    // 登場演出（箱から出てくる）の経過秒

	// アイコン（リザルト/HUD/タリー共用）
	std::shared_ptr<KdTexture> m_coinTex;
	CoinModelIcon              m_coinIcon;   // 3DコインをRTへ描く共通ヘルパー（結果/HUD共用）
	std::shared_ptr<KdTexture> m_rockTex;
	std::shared_ptr<KdTexture> m_lifeIcoTex;   // リザルト窓のコアリア顔アイコン（lifeico.png）

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
	float         MarkerDrawYaw() const; // 描画用の向き（ズーム中はカメラを向く）
	float         MarkerDrawScale() const; // 描画用スケール（入場演出で縮む）
	void          StartMove(int target); // 指定ノードへホップ開始
	void          DrawResultPanel();     // リザルトパネル描画

	// ステージ解放判定：ステージ0は常に解放。以降は前ステージをクリア済みなら解放。
	bool          IsUnlocked(int stageId) const;

	// ── TABメニュー（背景ぼかし。つづける／タイトルへ）──
	void          UpdateMenu();         // Event から呼ぶ
	void          DrawMenuBackground(); // 背景ぼかし＋暗幕（UIより先に描く）
	void          DrawMenu();           // パネル本体（最前面）。DrawSpriteExtra から呼ぶ
	bool          m_menuOpen     = false;
	int           m_menuIndex    = 0;
	bool          m_menuTabPrev  = false;
	SettingsMenu  m_settingsMenu;   // TABメニューから開く設定ウィンドウ
	bool          m_menuNavPrev  = false;
	bool          m_menuConfPrev = false;
	float         m_menuBlink    = 0.0f;
	KdRenderTargetPack m_menuBlurRT;       // 背景ぼかしの出力先
	bool          m_menuBlurInit = false;

	// ノードの見た目（未解放＝くすんだ色・小さめ／解放＝通常）。解放アニメ中は補間。
	void          NodeAppearance(int nodeIdx, float& outScale, Math::Color& outColor) const;

	// 解放アニメ（クリア後に次ステージが色・大きさを取り戻す）
	int   m_unlockNode = -1;    // 取り戻すノードのindex（-1=なし）
	float m_unlockAnim = 0.0f;  // 0→1 の進行

	// 「行けない！」演出（未解放ノードへ行こうとしたら道を赤く点滅）
	int   m_denyNode  = -1;     // 拒否対象ノードのindex（-1=なし）
	float m_denyTimer = 0.0f;   // 残り点滅時間(秒)
};
