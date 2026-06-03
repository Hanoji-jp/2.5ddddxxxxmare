#include "../main.h"
#include "ItemManager.h"

void ItemManager::SpawnCoin(const Math::Vector3& _pos)
{
	auto coin = std::make_shared<Coin>();
	coin->SetSpawnPos(_pos);
	coin->Init();
	m_coins.push_back(std::move(coin));
}

int ItemManager::Update(HitBox& _playerHitBox)
{
	int collected = 0;

	const KdCollider::SphereInfo hitSphere = _playerHitBox.GetSphereInfo();

	for (const auto& coin : m_coins)
	{
		if (coin->IsExpired()) { continue; }

		coin->Update();

		// コインの KdCollider に対して HitBox の球を当てる
		if (coin->Intersects(hitSphere, nullptr))
		{
			coin->Expire();
			++collected;
		}
	}

	return collected;
}

void ItemManager::DrawLit()
{
	for (const auto& coin : m_coins)
	{
		if (!coin->IsExpired()) { coin->DrawLit(); }
	}
}

void ItemManager::Refresh()
{
	m_coins.remove_if([](const std::shared_ptr<Coin>& c) { return c->IsExpired(); });
}

void ItemManager::DrawGui()
{
	if (!ImGui::Begin("Item Manager"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Coin Count: %d", static_cast<int>(m_coins.size()));
	ImGui::Separator();

	// 新規コイン追加
	static Math::Vector3 s_newPos = { 0.0f, 2.0f, 0.0f };
	ImGui::DragFloat3("New Coin Pos", &s_newPos.x, 0.1f);
	if (ImGui::Button("Add Coin"))
	{
		SpawnCoin(s_newPos);
	}

	ImGui::Separator();

	// 既存コインの位置編集・削除
	int idx = 0;
	for (auto& coin : m_coins)
	{
		if (coin->IsExpired()) { ++idx; continue; }

		ImGui::PushID(idx);

		Math::Vector3 pos = coin->GetSpawnPos();
		if (ImGui::DragFloat3("##pos", &pos.x, 0.1f))
		{
			coin->SetSpawnPos(pos);
		}
		ImGui::SameLine();
		if (ImGui::Button("Del"))
		{
			coin->Expire();
		}

		ImGui::PopID();
		++idx;
	}

	ImGui::End();
}
