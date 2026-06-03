#pragma once
#include "../GameObject/Item/Coin.h"
#include "../GameObject/Item/HitBox.h"

//==========================================================
// ItemManager
// コインなどアイテムの生成・更新・取得判定を一括管理
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

	// 全アイテム更新 + プレイヤーの HitBox との取得判定
	// 取得されたコインは即 Expire → 次の Refresh で除去
	int  Update(HitBox& _playerHitBox);

	void DrawLit();
	void DrawGui();   // ImGui によるコイン配置エディター

	// 取得済みコインを除去
	void Refresh();

	int  GetCoinCount() const { return static_cast<int>(m_coins.size()); }

private:
	std::list<std::shared_ptr<Coin>> m_coins;
};
