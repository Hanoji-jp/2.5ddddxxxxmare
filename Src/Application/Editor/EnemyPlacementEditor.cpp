#include "../../Pch.h"
#include "EnemyPlacementEditor.h"
#include "../Const/EnemyConst.h"
#include <fstream>
#include <sstream>

void EnemyPlacementEditor::DrawGui()
{
	if (!ImGui::Begin("Enemy Placement Editor"))
	{
		ImGui::End();
		return;
	}

	// ── 追加ボタン ──────────────────────────────────────────
	if (ImGui::Button("Add Melee"))
	{
		EnemyPlacementData d;
		d.type = EnemyType::Melee;
		m_placements.push_back(d);
		m_selectedIndex = static_cast<int>(m_placements.size()) - 1;
		m_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Ranged"))
	{
		EnemyPlacementData d;
		d.type = EnemyType::Ranged;
		m_placements.push_back(d);
		m_selectedIndex = static_cast<int>(m_placements.size()) - 1;
		m_dirty = true;
	}

	ImGui::Separator();

	// ── リスト ──────────────────────────────────────────────
	ImGui::Text("Enemy List");
	for (int i = 0; i < static_cast<int>(m_placements.size()); ++i)
	{
		const auto& p   = m_placements[i];
		const char* typeName = (p.type == EnemyType::Melee) ? "Melee" : "Ranged";
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

		ImGui::Text("Inspector");

		// 種類切替
		int typeIdx = (p.type == EnemyType::Melee) ? 0 : 1;
		const char* typeNames[] = { "Melee", "Ranged" };
		if (ImGui::Combo("Type", &typeIdx, typeNames, 2))
		{
			p.type  = (typeIdx == 0) ? EnemyType::Melee : EnemyType::Ranged;
			m_dirty = true;
		}

		float pos[3] = { p.position.x, p.position.y, p.position.z };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			p.position = { pos[0], pos[1], pos[2] };
			m_dirty    = true;
		}

		if (ImGui::Button("Delete"))
		{
			m_placements.erase(m_placements.begin() + m_selectedIndex);
			m_selectedIndex = -1;
			m_dirty = true;
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Save")) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(); }

	ImGui::End();
}

void EnemyPlacementEditor::DrawDebugSpheres() const
{
	for (const auto& p : m_placements)
	{
		// 種類別に色を変える（Melee=赤、Ranged=青）
		const Math::Vector4 color = (p.type == EnemyType::Melee)
			? Math::Vector4(1.0f, 0.2f, 0.2f, 1.0f)
			: Math::Vector4(0.2f, 0.4f, 1.0f, 1.0f);

		KdDebugWireFrame wire;
		wire.AddDebugSphere(p.position, EnemyConst::GhostSphereRadius, color);
		wire.Draw();
	}
}

void EnemyPlacementEditor::Save() const
{
	std::ofstream ofs(SavePath);
	if (!ofs) { return; }

	for (const auto& p : m_placements)
	{
		const int typeInt = (p.type == EnemyType::Melee) ? 0 : 1;
		ofs << typeInt << ","
			<< p.position.x << "," << p.position.y << "," << p.position.z
			<< "\n";
	}
}

void EnemyPlacementEditor::Load()
{
	std::ifstream ifs(SavePath);
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
		d.type       = (std::stoi(tokens[0]) == 0) ? EnemyType::Melee : EnemyType::Ranged;
		d.position.x = std::stof(tokens[1]);
		d.position.y = std::stof(tokens[2]);
		d.position.z = std::stof(tokens[3]);
		m_placements.push_back(d);
	}

	m_dirty = true;
}
