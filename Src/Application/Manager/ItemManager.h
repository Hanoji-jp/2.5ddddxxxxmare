#pragma once
#include "../GameObject/Item/Coin.h"
#include "../GameObject/Item/HitBox.h"
#include "../GameObject/Item/ParasolItem.h"

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

	// 全アイテム更新 + プレイヤーの HitBox との取得判定
	// 戻り値: 取得コイン数
	// outParasolPickedUp: このフレームにパラソルを取得したか
	int  Update(HitBox& _playerHitBox, bool& outParasolPickedUp);

	void DrawLit();
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

	static constexpr const char* SavePath = "Asset/Data/coins.csv";

private:
	std::list<std::shared_ptr<Coin>>        m_coins;
	std::list<std::shared_ptr<ParasolItem>> m_parasols;
};
