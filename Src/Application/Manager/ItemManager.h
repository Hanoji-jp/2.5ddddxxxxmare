#pragma once
#include "../GameObject/Item/Coin.h"
#include "../GameObject/Item/HitBox.h"
#include "../GameObject/Item/ParasolItem.h"
#include "../GameObject/Item/RockDrop.h"
#include "../GameObject/Item/RockGem.h"
#include "../GameObject/Effect/PickupBurst.h"

//==========================================================
// ItemManager
// コイン・パラソルなどアイテムの生成・更新・取得判定を一括管理
//==========================================================
class ItemManager
{
public:
	ItemManager()  = default;
	~ItemManager() = default;

	ItemManager(const ItemManager&)            = delete;
	ItemManager& operator=(const ItemManager&) = delete;

	// コインをスポーン座標に追加
	void SpawnCoin(const Math::Vector3& _pos);

	// 直線上に等間隔でコインを並べる
	void SpawnCoinLine(const Math::Vector3& _start, const Math::Vector3& _end, int _count);

	// 全コイン削除
	void ClearCoins();

	// パラソルアイテムをスポーン座標に追加
	void SpawnParasol(const Math::Vector3& pos);
	void ClearParasols();

	// 敵撃破時：spawnPos から up 基準で岩石を 6〜10 個ドロップ
	void SpawnRockBurst(const Math::Vector3& spawnPos, const Math::Vector3& upDir);
	void ClearRocks();
	int  GetRockCount() const { return m_rockCount; }

	// ── カラフル岩（スターピース風・コインエディタで配置する収集アイテム）──
	void SpawnRockGem(const Math::Vector3& pos);
	void SpawnRockGemLine(const Math::Vector3& start, const Math::Vector3& end, int count);  // 直線に等間隔

	// 図形パターン配置（XY平面。CubeEdgesのみZも使う）
	enum class GemShape { Circle, Star, Heart, CubeEdges };
	void SpawnRockGemShape(GemShape shape, const Math::Vector3& center, float size, int count);

	// コインも図形パターンで並べる（GemShape を流用）
	void SpawnCoinShape(GemShape shape, const Math::Vector3& center, float size, int count);

	// 配置前ガイド：ライン/図形のプレビューをデバッグワイヤーで表示（GameSceneのデバッグ描画から呼ぶ）
	void DrawPlacementPreview() const;

	void ClearRockGems();
	void SaveRockGems() const;
	void LoadRockGems();
	void ResetRockGemsCollected();                              // ステージやり直しで未取得に戻す
	int  GetRockGemCount() const { return static_cast<int>(m_rockGems.size()); }

	// ── 左クリックでカメラから岩を撃ち出す（投擲物。保存しない／寿命で消える）──
	void ShootRock(const Math::Vector3& start, const Math::Vector3& dir, float speed);
	void ClearThrownRocks();

	// マウスカーソルで岩を操作する入力（GameScene がカメラからレイを計算して渡す）
	struct CursorMagnet
	{
		bool          valid    = false;        // この入力を有効にするか
		Math::Vector3 rayOrigin = {};          // カーソル位置のワールドレイ始点（カメラ）
		Math::Vector3 rayDir    = {};          // カーソル位置のワールドレイ方向（正規化）
		bool          clicked   = false;       // このフレームに左クリックしたか
	};

	// 全アイテム更新 + プレイヤーの HitBox との取得判定
	// 戻り値: 取得コイン数
	// outParasolPickedUp: このフレームにパラソルを取得したか
	// outRocksPicked: このフレームに取得した緑石(回復アイテム)の数
	// outGemsPicked : このフレームに取得したカラフル岩(収集のみ)の数
	// cursor        : カーソル磁石入力（吸い寄せ取得＋左クリックで飛ばす）
	int  Update(HitBox& _playerHitBox, bool& outParasolPickedUp, int& outRocksPicked, int& outGemsPicked,
		const CursorMagnet& cursor);

	void DrawLit();
	void DrawOutline();  // アイテムの原神式アウトライン
	void DrawEffect();   // アイテム周りの星きらめき（エフェクトパスで呼ぶ）
	void UpdatePickupEffects();   // 取得バーストのみ更新（ヒットストップ中も呼ぶ）
	void DrawGui();   // ImGui によるアイテム配置エディター

	// ショーケース等で「取得判定なし・見た目だけ」更新する（ふわふわ/自転/きらめき/バースト）
	void UpdateVisualOnly();

	// 取得済みアイテムを除去
	void Refresh();

	void Save() const;
	void Load();

	void SaveParasols() const;
	void LoadParasols();

	int  GetCoinCount() const { return static_cast<int>(m_coins.size()); }

	// GameScene から直接 Update するためのアクセサ
	std::list<std::shared_ptr<ParasolItem>>& WorkParasols() { return m_parasols; }

	// ── エディタのギズモ編集用（クリック選択＋軸ドラッグでコイン/カラフル岩を配置）──
	std::list<std::shared_ptr<Coin>>&    WorkCoins()    { return m_coins; }
	std::list<std::shared_ptr<RockGem>>& WorkRockGems() { return m_rockGems; }
	void RemoveLastCoin()    { if (!m_coins.empty())    { m_coins.back()->Expire();    m_coins.pop_back(); } }
	void RemoveLastRockGem() { if (!m_rockGems.empty()) { m_rockGems.back()->Expire(); m_rockGems.pop_back(); } }
	void SetCoinPos(int i, const Math::Vector3& p)
	{
		int k = 0; for (auto& c : m_coins)    { if (k++ == i) { c->SetSpawnPos(p);  return; } }
	}
	void SetRockGemPos(int i, const Math::Vector3& p)
	{
		int k = 0; for (auto& g : m_rockGems) { if (k++ == i) { g->SetPlacedPos(p); return; } }
	}

	// 任意の位置に星バーストを出す（クリアのキメ演出など外部から使う）
	void SpawnBurstAt(const Math::Vector3& pos, const Math::Color& baseColor,
		PickupBurst::Style style = PickupBurst::Style::Full)
	{
		PlayPickupEffect(pos, baseColor, style);
	}

	static constexpr const char* SavePath = "Asset/Data/coins.csv";

private:
	// 取得時の演出：自前CPU星バースト（アイテム色＋星ごと微ランダム色）
	// style でスタイル切替（Full=通常 / Calm=控えめ / Ring=リング拡散＋星放物線）
	void PlayPickupEffect(const Math::Vector3& pos, const Math::Color& baseColor,
		PickupBurst::Style style = PickupBurst::Style::Full);

	std::list<std::shared_ptr<Coin>>        m_coins;
	std::list<std::shared_ptr<ParasolItem>> m_parasols;
	std::list<std::shared_ptr<RockDrop>>    m_rocks;
	std::list<std::shared_ptr<RockGem>>     m_rockGems;   // 配置するカラフル岩（収集）
	std::list<std::shared_ptr<RockGem>>     m_thrown;     // 左クリックで撃ち出した岩（投擲物・非保存）

	// 図形/ラインの点を生成（配置とプレビューで共用）
	static std::vector<Math::Vector3> BuildShapePoints(GemShape shape, const Math::Vector3& center, float size, int count);
	static std::vector<Math::Vector3> BuildLinePoints(const Math::Vector3& start, const Math::Vector3& end, int count);

	// 配置プレビュー状態（DrawGui で毎フレーム更新。エディタを閉じると false）
	bool          m_pvLineOn   = false;
	Math::Vector3 m_pvLineStart = {};
	Math::Vector3 m_pvLineEnd   = {};
	int           m_pvLineCount = 0;
	bool          m_pvShapeOn  = false;
	GemShape      m_pvShapeKind = GemShape::Circle;
	Math::Vector3 m_pvShapeCenter = {};
	float         m_pvShapeSize  = 0.0f;
	int           m_pvShapeCount = 0;
	// コイン用プレビュー
	bool          m_pvCoinLineOn  = false;
	Math::Vector3 m_pvCoinLineStart = {};
	Math::Vector3 m_pvCoinLineEnd   = {};
	int           m_pvCoinLineCount = 0;
	bool          m_pvCoinShapeOn = false;
	GemShape      m_pvCoinShapeKind = GemShape::Circle;
	Math::Vector3 m_pvCoinShapeCenter = {};
	float         m_pvCoinShapeSize  = 0.0f;
	int           m_pvCoinShapeCount = 0;
	std::list<std::shared_ptr<PickupBurst>> m_bursts;
	int                                     m_rockCount = 0;   // 取得した岩石の累計
};
