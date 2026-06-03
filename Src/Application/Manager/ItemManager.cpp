#include "../main.h"
#include "ItemManager.h"
#include <fstream>
#include <sstream>

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

void ItemManager::SpawnCoinLine(const Math::Vector3& _start, const Math::Vector3& _end, int _count)
{
	if (_count <= 0) { return; }
	if (_count == 1) { SpawnCoin(_start); return; }

	for (int i = 0; i < _count; ++i)
	{
		const float t   = static_cast<float>(i) / static_cast<float>(_count - 1);
		const Math::Vector3 pos = Math::Vector3::Lerp(_start, _end, t);
		SpawnCoin(pos);
	}
}

void ItemManager::ClearCoins()
{
	for (auto& coin : m_coins) { coin->Expire(); }
	Refresh();
}

void ItemManager::DrawGui()
{
	if (!ImGui::Begin("Coin Editor"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Coin Count: %d", static_cast<int>(m_coins.size()));

	// ──────────────────────────────────────────
	// Spawn Single
	// ──────────────────────────────────────────
	if (ImGui::CollapsingHeader("Spawn Single"))
	{
		static Math::Vector3 s_pos = { 0.0f, 2.0f, 0.0f };
		ImGui::DragFloat3("Position##single", &s_pos.x, 0.1f);
		if (ImGui::Button("Add##single"))
		{
			SpawnCoin(s_pos);
		}
	}

	// ──────────────────────────────────────────
	// Spawn Line
	// ──────────────────────────────────────────
	if (ImGui::CollapsingHeader("Spawn Line"))
	{
		static Math::Vector3 s_lineStart = { -5.0f, 2.0f, 0.0f };
		static Math::Vector3 s_lineEnd   = {  5.0f, 2.0f, 0.0f };
		static int           s_lineCount = 5;
		ImGui::DragFloat3("Start##line", &s_lineStart.x, 0.1f);
		ImGui::DragFloat3("End##line",   &s_lineEnd.x,   0.1f);
		ImGui::SliderInt("Count##line", &s_lineCount, 2, 20);
		if (ImGui::Button("Place##line"))
		{
			SpawnCoinLine(s_lineStart, s_lineEnd, s_lineCount);
		}
	}

	// ──────────────────────────────────────────
	// Coin List
	// ──────────────────────────────────────────
	ImGui::Separator();
	if (ImGui::Button("Clear All"))
	{
		ClearCoins();
	}
	ImGui::Separator();

	if (ImGui::BeginChild("CoinList", ImVec2(0.0f, 260.0f), true))
	{
		int idx = 0;
		for (auto& coin : m_coins)
		{
			if (coin->IsExpired()) { ++idx; continue; }

			ImGui::PushID(idx);

			ImGui::Text("[%d]", idx);
			ImGui::SameLine();

			Math::Vector3 pos = coin->GetSpawnPos();
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::DragFloat3("##pos", &pos.x, 0.1f))
			{
				coin->SetSpawnPos(pos);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Del"))
			{
				coin->Expire();
			}

			ImGui::PopID();
			++idx;
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	if (ImGui::Button("Save"))  { Save(); }
	ImGui::SameLine();
	if (ImGui::Button("Load"))  { ClearCoins(); Load(); }

	ImGui::End();
}

void ItemManager::Save() const
{
	std::ofstream ofs(SavePath);
	if (!ofs) { return; }

	for (const auto& coin : m_coins)
	{
		if (coin->IsExpired()) { continue; }
		const Math::Vector3& p = coin->GetSpawnPos();
		ofs << p.x << "," << p.y << "," << p.z << "\n";
	}
}

void ItemManager::Load()
{
	std::ifstream ifs(SavePath);
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 3) { continue; }

		Math::Vector3 p;
		p.x = std::stof(tokens[0]);
		p.y = std::stof(tokens[1]);
		p.z = std::stof(tokens[2]);
		SpawnCoin(p);
	}
}
