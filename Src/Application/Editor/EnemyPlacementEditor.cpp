#include "../../Pch.h"
#include "EnemyPlacementEditor.h"
#include "../Manager/StageManager.h"
#include "../Const/EnemyConst.h"
#include <fstream>
#include <sstream>

void EnemyPlacementEditor::DrawGui()
{
	if (!ImGui::Begin(U8("敵配置 エディタ")))
	{
		ImGui::End();
		return;
	}

	// ── 追加ボタン ──────────────────────────────────────────
	if (ImGui::Button(U8("クブン追加")))
	{
		EnemyPlacementData d;
		d.type = EnemyType::Cubun;
		m_placements.push_back(d);
		m_selectedIndex = static_cast<int>(m_placements.size()) - 1;
		m_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("遠距離敵追加")))
	{
		EnemyPlacementData d;
		d.type = EnemyType::Ranged;
		m_placements.push_back(d);
		m_selectedIndex = static_cast<int>(m_placements.size()) - 1;
		m_dirty = true;
	}

	ImGui::Separator();

	// ── リスト ──────────────────────────────────────────────
	ImGui::Text(U8("敵一覧"));
	for (int i = 0; i < static_cast<int>(m_placements.size()); ++i)
	{
		const auto& p        = m_placements[i];
		const char* typeName = (p.type == EnemyType::Cubun) ? "Cubun" : "Ranged";
		char label[64];
		std::snprintf(label, sizeof(label), "[%d] %s  (%.1f, %.1f, %.1f)##%d",
			i, typeName, p.position.x, p.position.y, p.position.z, i);

		const bool selected = (m_selectedIndex == i);
		if (ImGui::Selectable(label, selected))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	// ── インスペクター ───────────────────────────────────────
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_placements.size()))
	{
		auto& p = m_placements[m_selectedIndex];

		ImGui::Text(U8("インスペクタ"));

		// 種類切替
		int typeIdx = (p.type == EnemyType::Cubun) ? 0 : 1;
		const char* typeNames[] = { U8("クブン"), U8("遠距離") };
		if (ImGui::Combo(U8("種類"), &typeIdx, typeNames, 2))
		{
			p.type  = (typeIdx == 0) ? EnemyType::Cubun : EnemyType::Ranged;
			m_dirty = true;
		}

		float pos[3] = { p.position.x, p.position.y, p.position.z };
		if (ImGui::DragFloat3(U8("位置"), pos, 0.1f))
		{
			p.position = { pos[0], pos[1], pos[2] };
			m_dirty    = true;
		}

		// 初期重力方向
		if (p.type == EnemyType::Cubun)
		{
			ImGui::Separator();
			ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "-- Cubun Settings --");

			int gravIdx = 0;
			if      (p.initGravDir == Character::ManualGravityDir::Down) { gravIdx = 1; }
			else if (p.initGravDir == Character::ManualGravityDir::Up)   { gravIdx = 2; }
			const char* gravNames[] = { U8("なし（惑星に従う）"), U8("下（床歩き）"), U8("上（天井歩き）") };
			if (ImGui::Combo(U8("初期重力"), &gravIdx, gravNames, 3))
			{
				switch (gravIdx)
				{
				case 0:  p.initGravDir = Character::ManualGravityDir::None; break;
				case 1:  p.initGravDir = Character::ManualGravityDir::Down; break;
				default: p.initGravDir = Character::ManualGravityDir::Up;   break;
				}
				m_dirty = true;
			}
		}

		ImGui::Separator();
		if (ImGui::Button(U8("削除")))
		{
			m_placements.erase(m_placements.begin() + m_selectedIndex);
			m_selectedIndex = -1;
			m_dirty = true;
		}
	}

	ImGui::Separator();

	if (ImGui::Button(U8("保存"))) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("読込"))) { Load(); }

	ImGui::End();
}

void EnemyPlacementEditor::DrawDebugSpheres() const
{
	for (const auto& p : m_placements)
	{
		// 種類別に色を変える（Cubun=橙、Ranged=青）
		const Math::Vector4 color = (p.type == EnemyType::Cubun)
			? Math::Vector4(1.0f, 0.5f, 0.0f, 1.0f)
			: Math::Vector4(0.2f, 0.4f, 1.0f, 1.0f);

		KdDebugWireFrame wire;
		wire.AddDebugSphere(p.position, EnemyConst::GhostSphereRadius, color);
		wire.Draw();
	}
}

void EnemyPlacementEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("enemy_placements.csv"));
	if (!ofs) { return; }

	for (const auto& p : m_placements)
	{
		const int typeInt = (p.type == EnemyType::Cubun) ? 0 : 1;
		int gravInt = 0;
		if      (p.initGravDir == Character::ManualGravityDir::Down) { gravInt = 1; }
		else if (p.initGravDir == Character::ManualGravityDir::Up)   { gravInt = 2; }
		ofs << typeInt << ","
			<< p.position.x << "," << p.position.y << "," << p.position.z << ","
			<< gravInt
			<< "\n";
	}
}

void EnemyPlacementEditor::Load()
{
	std::ifstream ifs(StageManager::Instance().ResolvePath("enemy_placements.csv"));
	if (!ifs) { return; }

	m_placements.clear();

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ','))
		{
			tokens.push_back(token);
		}
		if (tokens.size() < 4) { continue; }

		EnemyPlacementData d;
		d.type       = (std::stoi(tokens[0]) == 0) ? EnemyType::Cubun : EnemyType::Ranged;
		d.position.x = std::stof(tokens[1]);
		d.position.y = std::stof(tokens[2]);
		d.position.z = std::stof(tokens[3]);
		if (tokens.size() >= 5)
		{
			const int g = std::stoi(tokens[4]);
			if      (g == 1) { d.initGravDir = Character::ManualGravityDir::Down; }
			else if (g == 2) { d.initGravDir = Character::ManualGravityDir::Up;   }
			else             { d.initGravDir = Character::ManualGravityDir::None;  }
		}

		m_placements.push_back(d);
	}

	m_dirty = true;
}

