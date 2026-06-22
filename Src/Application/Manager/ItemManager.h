#pragma once
#include "../GameObject/Item/Coin.h"
#include "../GameObject/Item/HitBox.h"
#include "../GameObject/Item/ParasolItem.h"
#include "../GameObject/Item/RockDrop.h"
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

	// 全アイテム更新 + プレイヤーの HitBox との取得判定
	// 戻り値: 取得コイン数
	// outParasolPickedUp: このフレームにパラソルを取得したか
	// outRocksPicked: このフレームに取得した緑石(回復アイテム)の数
	int  Update(HitBox& _playerHitBox, bool& outParasolPickedUp, int& outRocksPicked);

	void DrawLit();
	void DrawOutline();  // アイテムの原神式アウトライン
	void DrawEffect();   // アイテム周りの星きらめき（エフェクトパスで呼ぶ）
	void UpdatePickupEffects();   // 取得バーストのみ更新（ヒットストップ中も呼ぶ）
	void DrawGui();   // ImGui によるアイテム配置エディター

	// 取得済みアイテムを除去
	void Refresh();

	void Save() const;
	void Load();

	void SaveParasols() const;
	void LoadParasols();

	int  GetCoinCount() const { return static_cast<int>(m_coins.size()); }

	// GameScene から直接 Update するためのアクセサ
	std::list<std::shared_ptr<ParasolItem>>& WorkParasols() { return m_parasols; }

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
	std::list<std::shared_ptr<PickupBurst>> m_bursts;
	int                                     m_rockCount = 0;   // 取得した岩石の累計
};
