#pragma once

#include <functional>
#include"../BaseScene/BaseScene.h"
#include"../../GameObject/Character/Player/Player.h"
#include"../../GameObject/Character/Enemy/Enemy.h"
#include"../../GameObject/Character/Enemy/EnemyRanged.h"
#include"../../GameObject/BackGround/BackGround.h"
#include"../../GameObject/BackGround/StarField.h"
#include"../../GameObject/Camera/SideScrollCamera.h"
#include"../../GameObject/Camera/RoomBounds.h"
#include"../../Editor/RoomBoundsEditor.h"
#include"../../Editor/EnemyPlacementEditor.h"
#include"../../Editor/CheckpointEditor.h"
#include"../../Editor/WarpHoleEditor.h"
#include"../../Editor/MovingFloorEditor.h"
#include"../../GameObject/Gimmick/WarpHole.h"
#include"../../Manager/PlanetGravityManager.h"
#include"../../Manager/ManualGravityZoneManager.h"
#include"../../Manager/DeadZoneManager.h"
#include"../../Manager/CoreliaManager.h"
#include"../../Camera/CameraSettings.h"
#include"../../GameObject/Camera/EditorCamera.h"
#include"../../Const/SpawnConst.h"
#include"../../Const/EditorPickConst.h"
#include"../../Const/JuiceConst.h"
#include"../../Const/PauseMenuConst.h"
#include"../../Const/DeathConst.h"
#include"../../GameObject/Checkpoint/Checkpoint.h"
#include"../../Const/CheckpointConst.h"
#include"../../GameObject/UI/HpUI.h"
#include"../../GameObject/Light/PointLightObject.h"
#include"../../Const/LightConst.h"
#include"../../GameObject/Effect/FootDust.h"
#include"../../Manager/ItemManager.h"
#include"../../GameObject/Gimmick/MovingFloor.h"
#include"../../GameObject/Character/Enemy/Cubun.h"
#include"../../GameObject/Effect/StarBurstEffect.h"
#include"../../Editor/WindBoxEditor.h"
#include"../../GameObject/Gimmick/WindBox.h"
#include"../../Editor/GravityCoreEditor.h"
#include"../../GameObject/Gimmick/GravityCore.h"
#include"../../Editor/SpikeBoxEditor.h"
#include"../../GameObject/Gimmick/SpikeBox.h"
class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene();

private:

	void Event()              override;
	void Init()               override;
	void DrawGui()            override;
	void DrawDebugExtra()     override;
	void DrawDebugWires();    // デバッグワイヤー実体（シーンRTへ。DrawEffectExtraから呼ぶ）
	void DrawUnLitExtra()     override;  // 背景Box描画
	void DrawLitExtra()       override;  // 惑星モデル描画
	void DrawOutlineExtra()   override;  // 惑星の地形アウトライン
	void DrawEffectExtra()    override;  // アイテム星きらめき描画
	void UpdateDuringHitStop() override; // ヒットストップ中も取得演出を更新
	void DrawSpriteExtra() override;  // フェードオーバーレイ描画

	// ポーズメニュー or コアリア会話中はワールドを止める。
	// エディタ中は「他Updateを止める」トグル(m_editorFreeze)が ON ならワールドを停止。
	// 惑星(地面)が1つも無い未作成ステージは、プレイヤーが奈落に落ち続けてカメラが
	// 暴れるのを防ぐため停止する（マップ作成中の安定化）。
	bool IsUpdatePaused() const override
	{
		const bool unbuiltStage = PlanetGravityManager::Instance().GetPlanets().empty();
		return m_menuOpen || m_convoActive || (m_editorMode && m_editorFreeze) || unbuiltStage || m_clearActive;
	}
	void UpdatePauseMenu();   // メニュー操作（Event から呼ぶ）
	void DrawPauseMenu();     // メニュー描画（DrawSpriteExtra から呼ぶ）

	// 回復「＋」マークをプレイヤー位置とHP UI位置に出す（緑石取得時）
	void SpawnHealPlus(int count);

	// 重力ゾーンの重力方向を壁に2D矢印で表示（マリギャラ風）
	void DrawGravityArrows();

	void RebuildEnemies();
	void RebuildCheckpoints();
	void RebuildWarpHoles();
	// ─── StarBurst ビューア（手動テスト用）────────────────────
	bool m_starBurstTestRequest = false;

	// ─── Effekseer ビューア ──────────────────────────────────────
	char  m_efkViewerPath[256] = "StarBlue.efk";
	float m_efkViewerScale     = 1.0f;
	float m_efkViewerSpeed     = 1.0f;
	bool  m_efkViewerLoop      = false;

	void RebuildMovingFloors();
	void RebuildWindBoxes();
	void RebuildGravityCores();
	void RebuildSpikeBoxes();

	// Glow コア取得時に、その位置へゴール用 WarpHole を生成して開く
	void SpawnGoalWarpHole(const Math::Vector3& pos);

	// 画面フラッシュをトリガー（強さ 0〜1）
	void TriggerFlash(float alpha) { m_flashAlpha = std::max(m_flashAlpha, alpha); }

	std::shared_ptr<Player>  m_spPlayer   = nullptr;
	SideScrollCamera*        m_pCamera    = nullptr;

	std::vector<RoomBounds> m_rooms;

	// インゲームマップエディター
	RoomBoundsEditor        m_roomEditor;
	EnemyPlacementEditor    m_enemyEditor;
	CheckpointEditor        m_checkpointEditor;
	WarpHoleEditor          m_warpHoleEditor;
	MovingFloorEditor       m_movingFloorEditor;

	// ワープホールオブジェクトリスト
	std::vector<std::shared_ptr<WarpHole>> m_warpHoles;

	//---- Waypoint ワープ進行状態 ----
	enum class WarpPhase
	{
		None,                 // 通常
		Sucking,              // 吸い込み中（入口に向かって収縮）
		Traveling,            // パス移動中（トンネル型）
		TeleportFadeOut,      // テレポート型：Entry奥へ移動しながら暗転
		TeleportHold,         // テレポート型：完全暗転＋瞬間移動
		TeleportFadeIn,       // テレポート型：Exit奥から口元へ移動しながら明転
	};
	WarpPhase                              m_warpPhase         = WarpPhase::None;
	std::vector<Math::Vector3>             m_warpPath;
	int                                    m_warpSegment       = 0;
	float                                  m_warpSegProgress   = 0.0f;
	Math::Vector3                          m_warpExitDir;
	Math::Vector3                          m_warpEntryPos;

	// 弧長ベースの等速移動用：ウェイポイントを Catmull-Rom で密にサンプリングした曲線
	std::vector<Math::Vector3>             m_warpCurve;        // サンプリング済み曲線点
	float                                  m_warpCurveTotalLen = 0.0f; // 曲線の総延長
	float                                  m_warpDist          = 0.0f;  // 始点からの進行距離（弧長）
	float                                  m_warpProgress      = 0.0f;  // 0→1 イージング用タイム進捗
	Math::Vector3                          m_warpSuckStartPos; // 吸い込み開始時のプレイヤー位置
	float                                  m_warpSuckProgress  = 0.0f; // 0→1
	float                                  m_warpSuckStartAngle = 0.0f; // 螺旋開始角度
	float                                  m_warpPlayerScale   = 1.0f;

	bool                    m_editorMode  = false;
	bool                    m_f2Prev      = false;
	EditorCamera*           m_pEditorCam  = nullptr;

	// ─── エディタ：オブジェクト選択・操作（自前マウスピッキング＋ドラッグ）───
	// 生成・コピー対象の種別
	enum class EditKind { Planet, WindBox, SpikeBox, GravityCore, MovingFloor,
						   ManualZone, DeadZone, Corelia };

	// ギズモの操作軸
	enum class GizmoAxis { None, X, Y, Z };

	// 1 個の編集対象（位置の取得／設定／複製を抽象化して全オブジェクトを統一的に扱う）
	struct EditEntry
	{
		Math::Vector3                              pos;        // 現在位置
		std::function<void(const Math::Vector3&)>  setPos;     // 位置を設定（dirty 反映込み）siss
		std::function<void()>                      dup;        // 複製
		std::function<void()>                      select;     // 該当エディタの選択indexを同期
		std::string                                label;      // 表示名
		EditKind                                   kind;       // 種別
		int                                        idx = -1;   // 種別内のコンテナindex
	};
	std::vector<EditEntry> m_editEntries;        // 毎フレーム再構築
	int   m_selEntry      = -1;                  // 選択中エントリ(index)
	bool  m_lmbPrev       = false;               // 左クリックのエッジ検出
	bool  m_editorFreeze  = true;                // 他オブジェクトのUpdateを止めるトグル

	// ギズモ（軸ドラッグ）
	GizmoAxis     m_dragAxis    = GizmoAxis::None; // ドラッグ中の軸
	Math::Vector3 m_dragStartPos = {};            // ドラッグ開始時のオブジェクト位置
	float         m_dragStartS  = 0.0f;           // ドラッグ開始時の軸上パラメータ
	bool          m_moveActive  = false;          // 軸ドラッグ移動中か
	Math::Vector3 m_movePreStart = {};            // 移動前位置（アンドゥ記録用）

	// アンドゥ／リドゥ
	struct EditCommand
	{
		std::function<void()> undo;
		std::function<void()> redo;
	};
	std::vector<EditCommand> m_undoStack;
	std::vector<EditCommand> m_redoStack;
	bool m_ctrlZPrev = false;
	bool m_ctrlYPrev = false;

	// グリッドスナップ（カクカク移動）
	bool  m_snapEnabled = false;
	float m_snapSize    = EditorPickConst::DefaultSnapSize;

	void BuildEditEntries();                     // 全オブジェクトから編集エントリを構築
	void UpdateEditorPick();                     // マウス選択＋軸ドラッグ＋アンドゥ（Event から）
	bool ScreenRayFromGameUV(float _u, float _v,
		Math::Vector3& _outOrigin, Math::Vector3& _outDir) const;
	GizmoAxis  PickGizmoAxis(const Math::Vector3& _ro, const Math::Vector3& _rd,
		const Math::Vector3& _selPos) const;     // 軸ハンドルのピック
	void SpawnAtCursor(EditKind _kind);          // エディタカメラ前方に新規生成
	void CreateDefaultAt(EditKind _kind, const Math::Vector3& _pos); // 既定オブジェクト生成
	void RemoveLastOfKind(EditKind _kind);       // 種別の末尾を削除（生成のアンドゥ）
	void SetPosByKindIndex(EditKind _kind, int _idx, const Math::Vector3& _pos); // 位置設定
	void PushMoveCommand(EditKind _kind, int _idx,
		const Math::Vector3& _oldPos, const Math::Vector3& _newPos);
	void EditUndo();
	void EditRedo();
	void DrawSelectionMarker();                  // 選択中オブジェクト＋軸ギズモの描画

	// テレポート型ワープのフェード状態
	float           m_teleportFadeAlpha    = 0.0f;
	float           m_teleportHoldTimer    = 0.0f;
	Math::Vector3   m_teleportExitPos;
	Math::Vector3   m_teleportExitDir;
	bool            m_currentWarpTeleport  = false;

	// テレポート出現スケールポップ
	float           m_teleportPopTimer     = 0.0f;  // >0 でポップアニメ再生中

	// 画面フラッシュ（白）
	float           m_flashAlpha           = 0.0f;  // 現在の白フラッシュ強度

	// シーン開始時のフェードイン（白→透明）
	float           m_introFadeAlpha       = JuiceConst::IntroFadeStart;

	// 導入カットシーン（Stage1限定：投げ出され→落下きりもみ→着地で操作開始）
	bool            m_introCutscene = false;
	float           m_introTimer    = 0.0f;   // 経過時間
	float           m_introSpin     = 0.0f;   // タンブル回転角
	// 着地リアクション（ズサー…バタン）
	bool            m_introLanding   = false;
	float           m_introLandTimer = 0.0f;
	Math::Vector3   m_landSkidDir    = {};    // 横滑り方向（水平）
	void StartIntroCutscene();
	void UpdateIntroCutscene();

	// ステージクリア（ゴールコア取得＝即クリア→演出→StageSelectへ）
	bool  m_clearActive = false;
	float m_clearTimer  = 0.0f;
	bool  m_clearDecideFlashed = false;   // 決めスナップ時の小フラッシュ（1回だけ）
	float m_playTime    = 0.0f;           // このランの経過時間（クリア時にリザルトへ渡す）
	float m_clearYaw    = 0.0f;           // クリアカメラのyaw（バネ追従＝振り子ドリフト用）
	float m_clearYawVel = 0.0f;           // 同 角速度
	std::shared_ptr<GravityCore> m_spHeldCore;  // 取得して手に持っているコア（クリア演出中）
	void StartStageClear(const Math::Vector3& corePos);

	// ポーズメニュー状態
	bool            m_menuOpen        = false;
	int             m_menuIndex       = 0;
	bool            m_menuEscPrev     = false;
	bool            m_menuNavPrev     = false;
	bool            m_menuConfirmPrev = false;
	float           m_menuBlinkTimer  = 0.0f;

	// HP ダメージ検知（前フレームの HP を保持）
	int             m_prevPlayerHp         = -1;

	// 死亡シーケンス（暗転→復活→明転）
	bool            m_deathActive  = false;
	float           m_deathTimer   = 0.0f;
	bool            m_deathRevived = false;
	bool            m_checkpointReached = false; // チェックポイントを踏んだか（未踏なら初期スポーン）
	float           m_airTime           = 0.0f;  // 連続で接地していない時間（落下死判定）
	int             m_deathCount        = 0;     // このステージで死んだ回数（Maxに達したらリトライ）
	Math::Vector3   m_deathCamFocus = { 0.0f, 0.0f, 0.0f }; // 死亡カメラの注視点（死んだ場所）

	// FootDust 生成タイマー
	float           m_dustTimer            = 0.0f;

	// 着地エッジ検出用（前フレームの着地状態）
	bool            m_prevPlayerGround     = false;

	// ワープ完了後の再トリガー防止クールダウン（秒）
	float           m_warpCooldown         = 0.0f;

	// テレポート型の入退場パス（密にサンプリング済み）
	std::vector<Math::Vector3> m_teleportEntryPath;  // Entry口元→奥（吸い込みパス）
	std::vector<Math::Vector3> m_teleportExitPath;   // Exit奥→口元（吐き出しパス）
	float           m_teleportPathDist     = 0.0f;   // パス上の進行距離
	float           m_teleportPathTotalLen = 0.0f;   // パス全長

	// エディター配置敵リスト
	std::vector<std::shared_ptr<Enemy>> m_enemies;

	// 移動する床リスト
	std::vector<std::shared_ptr<MovingFloor>> m_movingFloors;

	// 風ボックスエディタ・オブジェクトリスト
	WindBoxEditor                             m_windBoxEditor;
	std::vector<std::shared_ptr<WindBox>>     m_windBoxes;

	// 重力コアエディタ・オブジェクトリスト
	GravityCoreEditor                            m_gravityCoreEditor;
	std::vector<std::shared_ptr<GravityCore>>    m_gravityCores;

	SpikeBoxEditor                               m_spikeBoxEditor;
	std::vector<std::shared_ptr<SpikeBox>>       m_spikeBoxes;

	// Cubun 敵リスト
	std::vector<std::shared_ptr<Cubun>> m_cubuns;

	// プレイヤースポーン座標
	Math::Vector3           m_spawnPos       = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };

	// 導入カットシーンの「飛んでくる」開始位置（エディタで設定・保存。ここからスポーンへ投げる）
	// 斜め上空から頭から突っ込んでくる（横ずれ＋高さ）
	Math::Vector3           m_introStartPos  = { SpawnConst::DefaultX - 700.0f, SpawnConst::DefaultY + 1500.0f, SpawnConst::DefaultZ };

	// チェックポイントリスト＋現在有効なリスポーン座標
	std::vector<std::shared_ptr<Checkpoint>> m_checkpoints;
	Math::Vector3           m_respawnPos     = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };

	// HP UI
	std::shared_ptr<HpUI>   m_spHpUI;

	// クリア暗転用アイリスマスク（中央が透明な穴・外周が黒。マリオ風の閉じる暗転）
	std::shared_ptr<KdTexture> m_irisMaskTex;

	// コアリア残機アイコン（死亡暗転画面に中央表示）
	std::shared_ptr<KdTexture> m_lifeIconTex;
	std::shared_ptr<KdTexture> m_lifeStarTex;          // 減る瞬間に散る星屑
	bool                       m_lifeLostSePlayed = false; // 減るSEを1回だけ鳴らす用

	// コイン（収集物）カウンター
	std::shared_ptr<KdTexture> m_coinIconTex;
	int                        m_coinTotal    = 0;     // 取得した累計
	float                      m_coinPopTimer = 0.0f;  // 取得時の拡大演出残り

	// HP（星1個で完結する満ち欠けゲージ）
	std::shared_ptr<KdTexture> m_hpStarTex;
	float                      m_hpShakeTimer = 0.0f;  // 被ダメ時の揺れ残り
	float                      m_hpCoreAnim   = 0.0f;  // 重力コア2D表示の波アニメ時間
	int                        m_hpPrevHp     = -1;    // 前フレームHP（凹み発生検知用）

	// HPコアが凹んだ瞬間に飛ばすパーティクル（DrawTriangleで描画）
	struct HpParticle
	{
		Math::Vector2 pos{ 0.0f, 0.0f };   // アイコン中心からのローカル座標
		Math::Vector2 vel{ 0.0f, 0.0f };
		float life = 0.0f;                 // 残り寿命(秒)
		float maxLife = 0.5f;
		float size = 6.0f;
		float rot = 0.0f;
		Math::Color col{ 0.6f, 0.9f, 1.0f, 1.0f };
	};
	std::vector<HpParticle> m_hpParticles;

	// 回復(緑石取得)時に上昇する「＋」マーク（プレイヤー位置・HP UI位置の両方）
	struct HealPlus
	{
		Math::Vector2 pos{ 0.0f, 0.0f };   // スクリーン座標(中心原点・+Yが上)
		float life = 0.0f;
		float maxLife = 0.8f;
		float size = 18.0f;
	};
	std::vector<HealPlus> m_healPluses;

	// ── コアリア会話（ヒントNPC）──
	bool          m_convoActive   = false;  // 会話中か
	Math::Vector3 m_convoNpcPos   = {};     // ズーム対象のNPC位置
	std::string   m_convoText;              // 表示するヒント本文（SJIS）
	float         m_convoZoom     = 0.0f;   // 0=通常, 1=ズームイン完了
	bool          m_interactPrev  = false;  // Wキーの押下エッジ検出

	// デバッグ可視化（ManualZone以外＝デッドゾーン/コアリア判定）。1キーでトグル
	bool          m_debugZonesVisible = false;
	bool          m_debugKeyPrev      = false;

	float         m_gravArrowScroll   = 0.0f;  // 重力矢印の流れ（スクロール）時間

	// エディタ画面（ゲームをImGuiウィンドウに表示）。F3でトグル。OFFで通常フルスクリーン描画
	bool          m_editorScreen   = true;
	bool          m_editorKeyPrev  = false;
	void UpdateCorelia();                   // 会話・インタラクト処理（Event から呼ぶ）
	void DrawCorelia();                     // 会話UI描画（DrawSpriteExtra から呼ぶ）

	// ポイントライトリスト
	std::vector<std::shared_ptr<PointLightObject>> m_pointLights;

	// アイテムマネージャー
	ItemManager m_itemManager;

	// 太陽光（ディレクショナルライト）設定
	Math::Vector3 m_sunDir      = LightConst::DirLightDir;
	Math::Vector3 m_sunColor    = LightConst::DirLightColor;
	Math::Vector4 m_ambientColor = LightConst::AmbientColor;

	void Respawn();
	void SaveSpawn();
	void LoadSpawn();
	void ApplySunLight();   // 保持している値をシェーダーへ反映
	void SaveSunLight();    // 太陽光設定をCSVへ保存
	void LoadSunLight();    // 太陽光設定をCSVから読み込み

	// WarpHole通過完了時にカメラZ基準を ExitDir から更新する
	void UpdateCameraZFromExitDir(const Math::Vector3& exitDir);
};

