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

int ItemManager::Update(HitBox& _playerHitBox, bool& outParasolPickedUp)
{
	outParasolPickedUp = false;
	int collected = 0;

	const KdCollider::SphereInfo hitSphere = _playerHitBox.GetSphereInfo();
	// 取得演出はプレイヤーの体の中心で再生（足元寄りの中心から少し上げる）
	const Math::Vector3 playerPos = _playerHitBox.GetCenter()
		+ Math::Vector3{ 0.0f, SparkleConst::PickupSpawnOffsetY, 0.0f };

	for (const auto& coin : m_coins)
	{
		if (coin->IsExpired()) { continue; }

		coin->Update();

		// コインの KdCollider に対して HitBox の球を当てる
		if (coin->Intersects(hitSphere, nullptr))
		{
			// 取得演出（プレイヤー体中心・コインの色）。コインはヒットストップ無し
			PlayPickupEffect(playerPos,
				{ SparkleConst::CoinColorR, SparkleConst::CoinColorG,
				  SparkleConst::CoinColorB, SparkleConst::CoinColorA });
			coin->Expire();
			++collected;
		}
	}

	// ── パラソルアイテム取得判定のみ（Update は GameScene から直接呼ぶ）
	for (const auto& p : m_parasols)
	{
		if (p->IsExpired()) { continue; }

		if (p->Intersects(hitSphere, nullptr))
		{
			// 取得演出（プレイヤー体中心・パラソルの色）。ヒットストップは GameScene 側で発火
			PlayPickupEffect(playerPos,
				{ SparkleConst::ParasolColorR, SparkleConst::ParasolColorG,
				  SparkleConst::ParasolColorB, SparkleConst::ParasolColorA });
			p->MarkPickedUp();
			outParasolPickedUp = true;
		}
	}

	// ── 岩石ドロップ（敵撃破でばら撒かれる通貨）の更新＋取得判定 ──
	for (const auto& rock : m_rocks)
	{
		if (rock->IsExpired()) { continue; }

		rock->Update();

		// 散らばり猶予を過ぎたら取得可能
		if (rock->IsPickable() && rock->Intersects(hitSphere, nullptr))
		{
			// 岩石は Ring スタイル：中央からリング拡散＋星が上に飛んで放物線で落ちる（白）
			PlayPickupEffect(playerPos, { 1.0f, 1.0f, 1.0f, 1.0f }, PickupBurst::Style::Ring);
			rock->Expire();
			++m_rockCount;
		}
	}

	UpdatePickupEffects();

	return collected;
}

void ItemManager::UpdatePickupEffects()
{
	// 取得バーストの更新＋寿命切れ掃除（ヒットストップ中も呼ばれる）
	for (const auto& b : m_bursts) { b->Update(); }
	m_bursts.remove_if([](const std::shared_ptr<PickupBurst>& b) { return b->IsExpired(); });
}

void ItemManager::PlayPickupEffect(const Math::Vector3& pos, const Math::Color& baseColor,
	PickupBurst::Style style)
{
	// 自前CPU星バーストを生成（星ごとの微ランダム色シフトは PickupBurst 内で付与）
	auto b = std::make_shared<PickupBurst>();
	b->Spawn(pos, baseColor, style);
	m_bursts.push_back(std::move(b));
}

void ItemManager::DrawLit()
{
	for (const auto& coin : m_coins)
	{
		if (!coin->IsExpired()) { coin->DrawLit(); }
	}
	for (const auto& p : m_parasols)
	{
		if (!p->IsExpired()) { p->DrawLit(); }
	}
}

void ItemManager::DrawEffect()
{
	// 各アイテムが自分の統合エフェクト（星きらめき）を描画する
	for (const auto& coin : m_coins)
	{
		if (!coin->IsExpired()) { coin->DrawEffect(); }
	}
	for (const auto& p : m_parasols)
	{
		if (!p->IsExpired()) { p->DrawEffect(); }
	}
	// 岩石ドロップ（加算でローポリ描画）
	for (const auto& rock : m_rocks)
	{
		if (!rock->IsExpired()) { rock->DrawEffect(); }
	}
	// 取得バースト
	for (const auto& b : m_bursts)
	{
		if (!b->IsExpired()) { b->DrawEffect(); }
	}
}

void ItemManager::Refresh()
{
	m_coins.remove_if([](const std::shared_ptr<Coin>& c) { return c->IsExpired(); });
	m_parasols.remove_if([](const std::shared_ptr<ParasolItem>& p) { return p->IsExpired(); });
	m_rocks.remove_if([](const std::shared_ptr<RockDrop>& r) { return r->IsExpired(); });
}

void ItemManager::SpawnRockBurst(const Math::Vector3& spawnPos, const Math::Vector3& upDir)
{
	// 6〜10 個ランダム
	const int range = RockConst::DropCountMax - RockConst::DropCountMin + 1;
	const int count = RockConst::DropCountMin + (std::rand() % (range > 0 ? range : 1));
	for (int i = 0; i < count; ++i)
	{
		auto r = std::make_shared<RockDrop>();
		r->Spawn(spawnPos, upDir);
		m_rocks.push_back(std::move(r));
	}
}

void ItemManager::ClearRocks()
{
	for (auto& r : m_rocks) { r->Expire(); }
	m_rocks.remove_if([](const std::shared_ptr<RockDrop>& r) { return r->IsExpired(); });
	m_rockCount = 0;
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

	// ── Parasol セクション ────────────────────────────────────
	ImGui::Separator();
	ImGui::Text("--- Parasol Items (%d) ---", static_cast<int>(m_parasols.size()));

	// 新規追加：プレイヤーがいる場所などに手動で座標入力
	static Math::Vector3 s_parasolPos = { 0.0f, 2.0f, 0.0f };
	ImGui::SetNextItemWidth(210.0f);
	ImGui::DragFloat3("##newppos", &s_parasolPos.x, 0.1f);
	ImGui::SameLine();
	if (ImGui::Button("Add##parasol"))
	{
		SpawnParasol(s_parasolPos);
	}

	if (ImGui::Button("Clear##parasols")) { ClearParasols(); }
	ImGui::SameLine();
	if (ImGui::Button("Save##parasols"))  { SaveParasols(); }
	ImGui::SameLine();
	if (ImGui::Button("Reload##parasols")) { ClearParasols(); LoadParasols(); }

	// リスト：各アイテムの pos を編集 → Set で spawnPos に反映
	if (ImGui::BeginChild("ParasolList", ImVec2(0.0f, 200.0f), true))
	{
		int idx = 0;
		for (auto& item : m_parasols)
		{
			if (item->IsExpired()) { ++idx; continue; }

			ImGui::PushID(idx);

			ImGui::Text("[%d]", idx);
			ImGui::SameLine();

			// DragFloat3 で直接 spawnPos を編集
			Math::Vector3 editPos = item->GetSpawnPos();
			ImGui::SetNextItemWidth(195.0f);
			if (ImGui::DragFloat3("##ppos", &editPos.x, 0.1f))
			{
				item->SetSpawnPos(editPos);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Del"))
			{
				item->Expire();
			}

			ImGui::PopID();
			++idx;
		}
	}
	ImGui::EndChild();

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

void ItemManager::SpawnParasol(const Math::Vector3& pos)
{
	auto item = std::make_shared<ParasolItem>();
	item->SetSpawnPos(pos);
	item->Init();
	m_parasols.push_back(std::move(item));
}

void ItemManager::ClearParasols()
{
	for (auto& p : m_parasols) { p->Expire(); }
	m_parasols.remove_if([](const std::shared_ptr<ParasolItem>& p) { return p->IsExpired(); });
}

void ItemManager::SaveParasols() const
{
	std::ofstream ofs(ItemConst::ParasolSavePath);
	if (!ofs)
	{
		OutputDebugStringA("[ItemManager] SaveParasols: failed to open file\n");
		return;
	}

	int count = 0;
	for (const auto& item : m_parasols)
	{
		if (item->IsExpired()) { continue; }
		const Math::Vector3& p = item->GetSpawnPos();
		ofs << p.x << "," << p.y << "," << p.z << "\n";
		++count;
	}
	OutputDebugStringA(("[ItemManager] SaveParasols: saved " + std::to_string(count) + " items\n").c_str());
}

void ItemManager::LoadParasols()
{
	std::ifstream ifs(ItemConst::ParasolSavePath);
	if (!ifs)
	{
		OutputDebugStringA("[ItemManager] LoadParasols: file not found\n");
		return;
	}

	int count = 0;
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
		SpawnParasol(p);
		++count;
	}
	OutputDebugStringA(("[ItemManager] LoadParasols: loaded " + std::to_string(count) + " items\n").c_str());
}
