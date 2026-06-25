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
			// 取得演出：コイン専用エフェクト（金のリング＋金の星が舞い上がる）。ヒットストップ無し
			PlayPickupEffect(playerPos,
				{ SparkleConst::CoinColorR, SparkleConst::CoinColorG,
				  SparkleConst::CoinColorB, SparkleConst::CoinColorA },
				PickupBurst::Style::Coin);
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

void ItemManager::UpdateVisualOnly()
{
	// 取得判定はせず、各アイテムの見た目（ふわふわ・自転・きらめき）だけ進める
	for (const auto& coin : m_coins)    { if (!coin->IsExpired() && !coin->IsCollected()) { coin->Update(); } }
	for (const auto& p    : m_parasols) { if (!p->IsExpired())   { p->Update(); } }
	for (const auto& rock : m_rocks)    { if (!rock->IsExpired()) { rock->Update(); } }
	for (const auto& gem  : m_rockGems) { if (!gem->IsExpired() && !gem->IsCollected()) { gem->Update(); } }
	for (const auto& g    : m_thrown)   { if (!g->IsExpired())   { g->Update(); } }
	UpdatePickupEffects();   // 取得バーストも進める
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

// 直線上の等間隔点
std::vector<Math::Vector3> ItemManager::BuildLinePoints(const Math::Vector3& start, const Math::Vector3& end, int count)
{
	std::vector<Math::Vector3> pts;
	if (count <= 0) { return pts; }
	if (count == 1) { pts.push_back(start); return pts; }
	pts.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(count - 1);
		pts.push_back(Math::Vector3::Lerp(start, end, t));
	}
	return pts;
}

// 図形（丸・星・ハート・立方体の辺）の点を生成
std::vector<Math::Vector3> ItemManager::BuildShapePoints(GemShape shape, const Math::Vector3& center, float size, int count)
{
	std::vector<Math::Vector3> pts;
	if (count <= 0 || size <= 0.0f) { return pts; }
	constexpr float kPi  = 3.14159265358979f;
	constexpr float kTau = 6.28318530718f;
	pts.reserve(count);

	switch (shape)
	{
	case GemShape::Circle:
		for (int i = 0; i < count; ++i)
		{
			const float a = kTau * static_cast<float>(i) / static_cast<float>(count);
			pts.push_back(center + Math::Vector3(std::cos(a), std::sin(a), 0.0f) * size);
		}
		break;
	case GemShape::Star:
	{
		const int kPts = 10;
		Math::Vector3 verts[10];
		for (int k = 0; k < kPts; ++k)
		{
			const float a = -kPi * 0.5f + kPi * static_cast<float>(k) / 5.0f;
			const float r = (k % 2 == 0) ? size : size * 0.42f;
			verts[k] = center + Math::Vector3(std::cos(a), std::sin(a), 0.0f) * r;
		}
		for (int i = 0; i < count; ++i)
		{
			const float fp = static_cast<float>(kPts) * static_cast<float>(i) / static_cast<float>(count);
			const int   seg = static_cast<int>(fp) % kPts;
			const float ft  = fp - std::floor(fp);
			pts.push_back(Math::Vector3::Lerp(verts[seg], verts[(seg + 1) % kPts], ft));
		}
		break;
	}
	case GemShape::Heart:
		for (int i = 0; i < count; ++i)
		{
			const float t = kTau * static_cast<float>(i) / static_cast<float>(count);
			const float st = std::sin(t);
			const float x = 16.0f * st * st * st;
			const float y = 13.0f * std::cos(t) - 5.0f * std::cos(2.0f * t)
						  - 2.0f * std::cos(3.0f * t) - std::cos(4.0f * t);
			pts.push_back(center + Math::Vector3(x, y, 0.0f) * (size / 16.0f));
		}
		break;
	case GemShape::CubeEdges:
	{
		const Math::Vector3 c[8] = {
			{-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
			{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1} };
		const int e[12][2] = {
			{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7} };
		const int perEdge = std::max(1, count / 12);
		for (int ei = 0; ei < 12; ++ei)
		{
			const Math::Vector3 a = center + c[e[ei][0]] * size;
			const Math::Vector3 b = center + c[e[ei][1]] * size;
			for (int k = 0; k < perEdge; ++k)
			{
				const float t = static_cast<float>(k) / static_cast<float>(perEdge);
				pts.push_back(Math::Vector3::Lerp(a, b, t));
			}
		}
		break;
	}
	}
	return pts;
}

void ItemManager::SpawnRockGemLine(const Math::Vector3& start, const Math::Vector3& end, int count)
{
	for (const auto& p : BuildLinePoints(start, end, count)) { SpawnRockGem(p); }
}

void ItemManager::SpawnRockGemShape(GemShape shape, const Math::Vector3& center, float size, int count)
{
	for (const auto& p : BuildShapePoints(shape, center, size, count)) { SpawnRockGem(p); }
}

void ItemManager::DrawPlacementPreview() const
{
	if (!m_pvLineOn && !m_pvShapeOn && !m_pvCoinLineOn && !m_pvCoinShapeOn) { return; }

	KdDebugWireFrame wire;
	const Math::Color gemCol (1.0f, 0.55f, 0.15f, 1.0f);   // 岩＝オレンジ寄り金
	const Math::Color coinCol(1.0f, 0.9f,  0.2f,  1.0f);   // コイン＝黄色
	const float r = std::max(0.15f, RockGemConst::Radius);

	// カラフル岩
	if (m_pvLineOn)
	{
		for (const auto& p : BuildLinePoints(m_pvLineStart, m_pvLineEnd, m_pvLineCount)) { wire.AddDebugSphere(p, r, gemCol); }
		wire.AddDebugLine(m_pvLineStart, m_pvLineEnd, { 0.4f, 1.0f, 0.4f, 1.0f });
	}
	if (m_pvShapeOn)
	{
		for (const auto& p : BuildShapePoints(m_pvShapeKind, m_pvShapeCenter, m_pvShapeSize, m_pvShapeCount)) { wire.AddDebugSphere(p, r, gemCol); }
	}
	// コイン
	if (m_pvCoinLineOn)
	{
		for (const auto& p : BuildLinePoints(m_pvCoinLineStart, m_pvCoinLineEnd, m_pvCoinLineCount)) { wire.AddDebugSphere(p, r, coinCol); }
		wire.AddDebugLine(m_pvCoinLineStart, m_pvCoinLineEnd, { 0.4f, 0.9f, 1.0f, 1.0f });
	}
	if (m_pvCoinShapeOn)
	{
		for (const auto& p : BuildShapePoints(m_pvCoinShapeKind, m_pvCoinShapeCenter, m_pvCoinShapeSize, m_pvCoinShapeCount)) { wire.AddDebugSphere(p, r, coinCol); }
	}
	wire.Draw();
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

void ItemManager::SpawnCoinShape(GemShape shape, const Math::Vector3& center, float size, int count)
{
	for (const auto& p : BuildShapePoints(shape, center, size, count)) { SpawnCoin(p); }
}

void ItemManager::ClearCoins()
{
	for (auto& coin : m_coins) { coin->Expire(); }
	Refresh();
}

void ItemManager::DrawGui()
{
	// 配置プレビューはこのフレームのGUIで再設定する（ウィンドウ非表示なら消える）
	m_pvLineOn  = false;
	m_pvShapeOn = false;
	m_pvCoinLineOn  = false;
	m_pvCoinShapeOn = false;

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
		ImGui::SliderInt(U8("個数##line"), &s_lineCount, 2, 30);
		if (ImGui::Button(U8("配置##line")))
		{
			SpawnCoinLine(s_lineStart, s_lineEnd, s_lineCount);
		}
		// 配置前ガイド
		m_pvCoinLineOn = true; m_pvCoinLineStart = s_lineStart; m_pvCoinLineEnd = s_lineEnd; m_pvCoinLineCount = s_lineCount;
	}

	// ──────────────────────────────────────────
	// Spawn Shape（図形配置：丸/星/ハート/立方体の辺）
	// ──────────────────────────────────────────
	if (ImGui::CollapsingHeader(U8("図形配置")))
	{
		static Math::Vector3 s_cShapeCenter = { 0.0f, 5.0f, 0.0f };
		static float         s_cShapeSize   = 4.0f;
		static int           s_cShapeCount  = 16;
		static int           s_cShapeKind   = 0;
		const char* shapeItems[] = { U8("丸"), U8("星"), U8("ハート"), U8("立方体の辺") };
		ImGui::Combo(U8("図形##coinshape"), &s_cShapeKind, shapeItems, IM_ARRAYSIZE(shapeItems));
		ImGui::DragFloat3(U8("中心##coinshape"), &s_cShapeCenter.x, 0.1f);
		ImGui::DragFloat(U8("大きさ##coinshape"), &s_cShapeSize, 0.1f, 0.5f, 50.0f);
		ImGui::SliderInt(U8("個数##coinshape"), &s_cShapeCount, 3, 120);
		if (ImGui::Button(U8("図形配置##coin")))
		{
			SpawnCoinShape(static_cast<GemShape>(s_cShapeKind), s_cShapeCenter, s_cShapeSize, s_cShapeCount);
		}
		// 配置前ガイド
		m_pvCoinShapeOn = true; m_pvCoinShapeKind = static_cast<GemShape>(s_cShapeKind);
		m_pvCoinShapeCenter = s_cShapeCenter; m_pvCoinShapeSize = s_cShapeSize; m_pvCoinShapeCount = s_cShapeCount;
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

			// 向き（モデルの傾き）
			const char* dirItems[] = { U8("上"), U8("下"), U8("左"), U8("右") };
			int dirIdx = static_cast<int>(coin->GetDir());
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::Combo(U8("向き##coindir"), &dirIdx, dirItems, IM_ARRAYSIZE(dirItems)))
			{
				coin->SetDir(static_cast<Coin::CoinDir>(dirIdx));
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

	// ── ライン配置 ──
	{
		static Math::Vector3 s_gemLineStart = { -5.0f, 2.0f, 0.0f };
		static Math::Vector3 s_gemLineEnd   = {  5.0f, 2.0f, 0.0f };
		static int           s_gemLineCount = 5;
		ImGui::DragFloat3(U8("始点##gemline"), &s_gemLineStart.x, 0.1f);
		ImGui::DragFloat3(U8("終点##gemline"), &s_gemLineEnd.x,   0.1f);
		ImGui::SliderInt(U8("個数##gemline"), &s_gemLineCount, 2, 30);
		if (ImGui::Button(U8("ライン配置##gem")))
		{
			SpawnRockGemLine(s_gemLineStart, s_gemLineEnd, s_gemLineCount);
		}
		// 配置前ガイド（ワイヤー球＋線）を表示
		m_pvLineOn = true; m_pvLineStart = s_gemLineStart; m_pvLineEnd = s_gemLineEnd; m_pvLineCount = s_gemLineCount;
	}

	// ── 図形配置（星・ハート・丸・立方体の辺）──
	{
		static Math::Vector3 s_shapeCenter = { 0.0f, 5.0f, 0.0f };
		static float         s_shapeSize   = 4.0f;
		static int           s_shapeCount  = 16;
		static int           s_shapeKind   = 0;   // 0=丸 1=星 2=ハート 3=立方体の辺
		const char* shapeItems[] = { U8("丸"), U8("星"), U8("ハート"), U8("立方体の辺") };
		ImGui::Combo(U8("図形##gemshape"), &s_shapeKind, shapeItems, IM_ARRAYSIZE(shapeItems));
		ImGui::DragFloat3(U8("中心##gemshape"), &s_shapeCenter.x, 0.1f);
		ImGui::DragFloat(U8("大きさ##gemshape"), &s_shapeSize, 0.1f, 0.5f, 50.0f);
		ImGui::SliderInt(U8("個数##gemshape"), &s_shapeCount, 3, 120);
		if (ImGui::Button(U8("図形配置##gem")))
		{
			SpawnRockGemShape(static_cast<GemShape>(s_shapeKind), s_shapeCenter, s_shapeSize, s_shapeCount);
		}
		// 配置前ガイド（ワイヤー球）を表示
		m_pvShapeOn = true; m_pvShapeKind = static_cast<GemShape>(s_shapeKind);
		m_pvShapeCenter = s_shapeCenter; m_pvShapeSize = s_shapeSize; m_pvShapeCount = s_shapeCount;
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
		ofs << p.x << "," << p.y << "," << p.z << ","
			<< static_cast<int>(coin->GetDir()) << "\n";
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
		if (tokens.size() >= 4 && !m_coins.empty())
		{
			const int di = std::stoi(tokens[3]);
			if (di >= 0 && di <= 3) { m_coins.back()->SetDir(static_cast<Coin::CoinDir>(di)); }
		}
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
