#include "../main.h"
#include "ItemManager.h"
#include "StageManager.h"
#include "../Const/RockGemConst.h"
#include "../Const/ItemMagnetConst.h"
#include "PlanetGravityManager.h"
#include <fstream>
#include <sstream>
#include <cfloat>

void ItemManager::SpawnCoin(const Math::Vector3& _pos)
{
	auto coin = std::make_shared<Coin>();
	coin->SetSpawnPos(_pos);
	coin->Init();
	m_coins.push_back(std::move(coin));
}

int ItemManager::Update(HitBox& _playerHitBox, bool& outParasolPickedUp, int& outRocksPicked, int& outGemsPicked,
	const CursorMagnet& cursor)
{
	outParasolPickedUp = false;
	outRocksPicked = 0;
	outGemsPicked = 0;
	int collected = 0;

	const KdCollider::SphereInfo hitSphere = _playerHitBox.GetSphereInfo();
	// 取得演出はプレイヤーの体の中心で再生（足元寄りの中心から少し上げる）
	const Math::Vector3 playerPos = _playerHitBox.GetCenter()
		+ Math::Vector3{ 0.0f, SparkleConst::PickupSpawnOffsetY, 0.0f };

	for (const auto& coin : m_coins)
	{
		if (coin->IsExpired() || coin->IsCollected()) { continue; }

		coin->Update();

		// コインの KdCollider に対して HitBox の球を当てる
		if (coin->Intersects(hitSphere, nullptr))
		{
			// 取得演出（プレイヤー体中心・コインの色）。コインはヒットストップ無し
			PlayPickupEffect(playerPos,
				{ SparkleConst::CoinColorR, SparkleConst::CoinColorG,
				  SparkleConst::CoinColorB, SparkleConst::CoinColorA });
			coin->Collect();   // ※Expireではない：配置データは残し、保存しても消えない
			++collected;
		}
	}

	// ── パラソルアイテム取得判定のみ（Update は GameScene から直接呼ぶ）
	for (const auto& p : m_parasols)
	{
		if (p->IsExpired() || p->IsPickedUp()) { continue; }

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
			// 緑石(回復)：リング状の緑バースト
			PlayPickupEffect(playerPos, { 0.30f, 1.0f, 0.45f, 1.0f }, PickupBurst::Style::Ring);
			rock->Expire();
			++m_rockCount;
			++outRocksPicked;
		}
	}

	// ── カラフル岩（配置した収集アイテム）の更新＋取得判定（回復はしない）──
	for (const auto& gem : m_rockGems)
	{
		if (gem->IsExpired() || gem->IsCollected()) { continue; }

		gem->Update();

		if (gem->Intersects(hitSphere, nullptr))
		{
			// 取得演出：そのジェムの色でリングバースト
			PlayPickupEffect(playerPos, gem->GetColor(), PickupBurst::Style::Ring);
			gem->Collect();   // ※Expireではない：配置データは残し、保存しても消えない
			++outGemsPicked;
		}
	}

	// ── カーソル磁石：岩（エメラルド／カラフル岩）を吸い寄せ取得＋左クリックで飛ばす ──
	if (cursor.valid)
	{
		const float dt   = KdFPSController::GetDt();
		const float lerp = std::min(ItemMagnetConst::PullLerp * dt * 60.0f, 1.0f);

		// レイ(始点+方向)を、対象の z 平面と交差させたワールド点を返す（2.5D）
		auto rayPointAtZ = [&](float z) -> Math::Vector3
		{
			const float dz = cursor.rayDir.z;
			if (std::abs(dz) < 1e-5f) { return cursor.rayOrigin; }
			const float t = (z - cursor.rayOrigin.z) / dz;
			return cursor.rayOrigin + cursor.rayDir * t;
		};
		auto distXY = [](const Math::Vector3& a, const Math::Vector3& b) -> float
		{
			const float dx = a.x - b.x, dy = a.y - b.y;
			return std::sqrt(dx * dx + dy * dy);
		};

		// 緑エメラルド（敵ドロップ・回復）：カーソルに吸い寄せて取得
		for (const auto& rock : m_rocks)
		{
			if (rock->IsExpired() || !rock->IsPickable()) { continue; }
			const Math::Vector3 p      = rock->GetPos();
			const Math::Vector3 target = rayPointAtZ(p.z);
			const float d = distXY(p, target);

			if (d < ItemMagnetConst::AttractRadius)
			{
				rock->PullTo(target, lerp);
				if (d < ItemMagnetConst::CollectRadius)
				{
					PlayPickupEffect(playerPos, { 0.30f, 1.0f, 0.45f, 1.0f }, PickupBurst::Style::Ring);
					rock->Expire();
					++m_rockCount;
					++outRocksPicked;
				}
			}
		}

		// カラフル岩（配置・収集）：カーソルに吸い寄せて取得
		for (const auto& gem : m_rockGems)
		{
			if (gem->IsExpired() || gem->IsCollected()) { continue; }
			const Math::Vector3 p      = gem->GetPos();
			const Math::Vector3 target = rayPointAtZ(p.z);
			const float d = distXY(p, target);

			if (d < ItemMagnetConst::AttractRadius)
			{
				gem->PullTo(target, lerp);
				if (d < ItemMagnetConst::CollectRadius)
				{
					PlayPickupEffect(playerPos, gem->GetColor(), PickupBurst::Style::Ring);
					gem->Collect();
					++outGemsPicked;
				}
			}
		}
	}

	// 撃ち出した投擲物（飛んで寿命で消える）を更新。地形に当たったらパリンと割れる。
	for (const auto& g : m_thrown)
	{
		if (g->IsExpired()) { continue; }

		const Math::Vector3 oldP = g->GetPos();
		g->Update();
		const Math::Vector3 newP = g->GetPos();

		// このフレームの移動区間を進行方向へレイで判定（壁にめり込む前に割る）
		Math::Vector3 seg = newP - oldP;
		const float len = seg.Length();
		if (len < 1e-4f) { continue; }
		Math::Vector3 dir = seg / len;

		const KdCollider::RayInfo ray(KdCollider::TypeGround, oldP, dir, len + RockGemConst::Radius);
		bool  hit = false;
		float best = FLT_MAX;
		Math::Vector3 hitPos;
		for (const auto& p : PlanetGravityManager::Instance().GetPlanets())
		{
			if (!p.pCollider) { continue; }
			std::list<KdCollider::CollisionResult> results;
			if (!p.pCollider->Intersects(ray, p.mWorld, &results)) { continue; }
			for (const auto& r : results)
			{
				const float dd = (r.m_hitPos - oldP).LengthSquared();
				if (dd < best) { best = dd; hitPos = r.m_hitPos; hit = true; }
			}
		}
		if (hit)
		{
			// パリンと割れる演出（ジェムの色で星バースト）
			PlayPickupEffect(hitPos, g->GetColor(), PickupBurst::Style::Full);
			g->Expire();
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
		if (!coin->IsExpired() && !coin->IsCollected()) { coin->DrawLit(); }
	}
	for (const auto& p : m_parasols)
	{
		if (!p->IsExpired()) { p->DrawLit(); }
	}
}

void ItemManager::DrawOutline()
{
	for (const auto& coin : m_coins)
	{
		if (!coin->IsExpired() && !coin->IsCollected()) { coin->DrawOutline(); }
	}
	for (const auto& p : m_parasols)
	{
		if (!p->IsExpired()) { p->DrawOutline(); }
	}
}

void ItemManager::DrawEffect()
{
	// 各アイテムが自分の統合エフェクト（星きらめき）を描画する
	for (const auto& coin : m_coins)
	{
		if (!coin->IsExpired() && !coin->IsCollected()) { coin->DrawEffect(); }
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
	// カラフル岩（配置・収集。取得済みは描かない）
	for (const auto& gem : m_rockGems)
	{
		if (!gem->IsExpired() && !gem->IsCollected()) { gem->DrawEffect(); }
	}
	// 撃ち出した投擲物
	for (const auto& g : m_thrown)
	{
		if (!g->IsExpired()) { g->DrawEffect(); }
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
	m_rockGems.remove_if([](const std::shared_ptr<RockGem>& g) { return g->IsExpired(); });
	m_thrown.remove_if([](const std::shared_ptr<RockGem>& g) { return g->IsExpired(); });
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

//==========================================================
// カラフル岩（スターピース風・コインエディタで配置する収集アイテム）
//==========================================================
void ItemManager::SpawnRockGem(const Math::Vector3& pos)
{
	auto gem = std::make_shared<RockGem>();
	gem->SetSpawnPos(pos);
	gem->Init();
	m_rockGems.push_back(std::move(gem));
}

void ItemManager::ClearRockGems()
{
	for (auto& g : m_rockGems) { g->Expire(); }
	m_rockGems.remove_if([](const std::shared_ptr<RockGem>& g) { return g->IsExpired(); });
}

void ItemManager::ResetRockGemsCollected()
{
	for (auto& g : m_rockGems) { g->ResetCollected(); }
}

void ItemManager::SaveRockGems() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath(RockGemConst::SaveFile));
	if (!ofs) { return; }

	for (const auto& g : m_rockGems)
	{
		if (g->IsExpired()) { continue; }
		const Math::Vector3& p = g->GetSpawnPos();
		ofs << p.x << "," << p.y << "," << p.z << "\n";
	}
}

void ItemManager::LoadRockGems()
{
	std::ifstream ifs(StageManager::Instance().ResolvePath(RockGemConst::SaveFile));
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
		SpawnRockGem(p);
	}
}

void ItemManager::ShootRock(const Math::Vector3& start, const Math::Vector3& dir, float speed)
{
	// 左クリックで「カメラの位置からクリックした方向へ」撃ち出す投擲物。
	// カラフル岩を流用し、寿命付きで飛ばす（保存しない）。
	auto g = std::make_shared<RockGem>();
	g->SetSpawnPos(start);
	g->Init();
	g->FlingFrom(start, dir, speed);
	g->SetLife(ItemMagnetConst::ThrownLife);
	m_thrown.push_back(std::move(g));
}

void ItemManager::ClearThrownRocks()
{
	for (auto& g : m_thrown) { g->Expire(); }
	m_thrown.remove_if([](const std::shared_ptr<RockGem>& g) { return g->IsExpired(); });
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
	if (!ImGui::Begin(U8("コインエディタ")))
	{
		ImGui::End();
		return;
	}

	ImGui::Text(U8("コイン数: %d"), static_cast<int>(m_coins.size()));

	// ──────────────────────────────────────────
	// Spawn Single
	// ──────────────────────────────────────────
	if (ImGui::CollapsingHeader(U8("単体配置")))
	{
		static Math::Vector3 s_pos = { 0.0f, 2.0f, 0.0f };
		ImGui::DragFloat3(U8("位置##single"), &s_pos.x, 0.1f);
		if (ImGui::Button(U8("追加##single")))
		{
			SpawnCoin(s_pos);
		}
	}

	// ──────────────────────────────────────────
	// Spawn Line
	// ──────────────────────────────────────────
	if (ImGui::CollapsingHeader(U8("ライン配置")))
	{
		static Math::Vector3 s_lineStart = { -5.0f, 2.0f, 0.0f };
		static Math::Vector3 s_lineEnd   = {  5.0f, 2.0f, 0.0f };
		static int           s_lineCount = 5;
		ImGui::DragFloat3(U8("始点##line"), &s_lineStart.x, 0.1f);
		ImGui::DragFloat3(U8("終点##line"),   &s_lineEnd.x,   0.1f);
		ImGui::SliderInt(U8("個数##line"), &s_lineCount, 2, 20);
		if (ImGui::Button(U8("配置##line")))
		{
			SpawnCoinLine(s_lineStart, s_lineEnd, s_lineCount);
		}
	}

	// ──────────────────────────────────────────
	// Coin List
	// ──────────────────────────────────────────
	ImGui::Separator();
	if (ImGui::Button(U8("全消去")))
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
			if (ImGui::SmallButton(U8("削除")))
			{
				coin->Expire();
			}

			ImGui::PopID();
			++idx;
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	if (ImGui::Button(U8("保存")))  { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("読込")))  { ClearCoins(); Load(); }

	// ── Parasol セクション ────────────────────────────────────
	ImGui::Separator();
	ImGui::Text(U8("--- パラソル (%d) ---"), static_cast<int>(m_parasols.size()));

	// 新規追加：プレイヤーがいる場所などに手動で座標入力
	static Math::Vector3 s_parasolPos = { 0.0f, 2.0f, 0.0f };
	ImGui::SetNextItemWidth(210.0f);
	ImGui::DragFloat3("##newppos", &s_parasolPos.x, 0.1f);
	ImGui::SameLine();
	if (ImGui::Button(U8("追加##parasol")))
	{
		SpawnParasol(s_parasolPos);
	}

	if (ImGui::Button(U8("全消去##parasols"))) { ClearParasols(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("保存##parasols")))  { SaveParasols(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("再読込##parasols"))) { ClearParasols(); LoadParasols(); }

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
			if (ImGui::SmallButton(U8("削除")))
			{
				item->Expire();
			}

			ImGui::PopID();
			++idx;
		}
	}
	ImGui::EndChild();

	// ── カラフル岩（スターピース風・収集アイテム）セクション ──────────────
	ImGui::Separator();
	ImGui::Text(U8("--- カラフル岩 (%d) ---"), static_cast<int>(m_rockGems.size()));

	static Math::Vector3 s_gemPos = { 0.0f, 2.0f, 0.0f };
	ImGui::SetNextItemWidth(210.0f);
	ImGui::DragFloat3("##newgempos", &s_gemPos.x, 0.1f);
	ImGui::SameLine();
	if (ImGui::Button(U8("追加##gem")))
	{
		SpawnRockGem(s_gemPos);
	}

	if (ImGui::Button(U8("全消去##gems"))) { ClearRockGems(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("保存##gems")))  { SaveRockGems(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("再読込##gems"))) { ClearRockGems(); LoadRockGems(); }

	if (ImGui::BeginChild("RockGemList", ImVec2(0.0f, 200.0f), true))
	{
		int idx = 0;
		for (auto& gem : m_rockGems)
		{
			if (gem->IsExpired()) { ++idx; continue; }

			ImGui::PushID(idx);

			ImGui::Text("[%d]", idx);
			ImGui::SameLine();

			Math::Vector3 editPos = gem->GetSpawnPos();
			ImGui::SetNextItemWidth(195.0f);
			if (ImGui::DragFloat3("##gempos", &editPos.x, 0.1f))
			{
				gem->SetSpawnPos(editPos);
				gem->Init();   // 位置に応じて色も振り直す
			}
			ImGui::SameLine();
			if (ImGui::SmallButton(U8("削除")))
			{
				gem->Expire();
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
	std::ofstream ofs(StageManager::Instance().ResolvePath("coins.csv"));
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
	std::ifstream ifs(StageManager::Instance().ResolvePath("coins.csv"));
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
	std::ofstream ofs(StageManager::Instance().ResolvePath("parasol_items.csv"));
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
	std::ifstream ifs(StageManager::Instance().ResolvePath("parasol_items.csv"));
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
