#pragma once

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
#include"../../Camera/CameraSettings.h"
#include"../../GameObject/Camera/EditorCamera.h"
#include"../../Const/SpawnConst.h"
#include"../../Const/JuiceConst.h"
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
	~GameScene() {}

private:

	void Event()              override;
	void Init()               override;
	void DrawGui()            override;
	void DrawDebugExtra()     override;
	void DrawUnLitExtra()     override;  // 背景Box描画
	void DrawLitExtra()       override;  // 惑星モデル描画
	void DrawEffectExtra()    override;  // アイテム星きらめき描画
	void UpdateDuringHitStop() override; // ヒットストップ中も取得演出を更新
	void DrawSpriteExtra() override;  // フェードオーバーレイ描画

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

	// HP ダメージ検知（前フレームの HP を保持）
	int             m_prevPlayerHp         = -1;

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

	// チェックポイントリスト＋現在有効なリスポーン座標
	std::vector<std::shared_ptr<Checkpoint>> m_checkpoints;
	Math::Vector3           m_respawnPos     = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };

	// HP UI
	std::shared_ptr<HpUI>   m_spHpUI;

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

