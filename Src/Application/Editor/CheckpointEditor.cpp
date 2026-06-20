#include "../../Pch.h"
#include "CheckpointEditor.h"
#include "../Manager/StageManager.h"
#include <fstream>
#include <sstream>

void CheckpointEditor::DrawGui()
{
	if (!ImGui::Begin("Checkpoint Editor"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Add Checkpoint"))
	{
		m_positions.push_back({ 0.0f, 0.0f, 0.0f });
		m_selectedIndex = static_cast<int>(m_positions.size()) - 1;
		m_dirty = true;
	}

	ImGui::Separator();
	ImGui::Text("Checkpoint List");

	for (int i = 0; i < static_cast<int>(m_positions.size()); ++i)
	{
		const auto& p = m_positions[i];
		char label[64];
		std::snprintf(label, sizeof(label), "[%d] (%.1f, %.1f, %.1f)##%d",
			i, p.x, p.y, p.z, i);

		if (ImGui::Selectable(label, m_selectedIndex == i))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_positions.size()))
	{
		auto& p = m_positions[m_selectedIndex];
		ImGui::Text("Inspector");

		float pos[3] = { p.x, p.y, p.z };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			p = { pos[0], pos[1], pos[2] };
			m_dirty = true;
		}

		if (ImGui::Button("Delete"))
		{
			m_positions.erase(m_positions.begin() + m_selectedIndex);
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

void CheckpointEditor::DrawDebugSpheres() const
{
	for (int i = 0; i < static_cast<int>(m_positions.size()); ++i)
	{
		// 選択中は黄色、それ以外は緑
		const bool selected = (i == m_selectedIndex);
		const Math::Vector4 color = selected
			? Math::Vector4(1.0f, 1.0f, 0.0f, 1.0f)
			: Math::Vector4(CheckpointConst::DebugColorR,
							CheckpointConst::DebugColorG,
							CheckpointConst::DebugColorB,
							CheckpointConst::DebugColorA);

		KdDebugWireFrame wire;
		wire.AddDebugSphere(m_positions[i], CheckpointConst::TriggerRadius, color);
		wire.Draw();
	}
}

void CheckpointEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("checkpoints.csv"));
	if (!ofs) { return; }

	for (const auto& p : m_positions)
	{
		ofs << p.x << "," << p.y << "," << p.z << "\n";
	}
}

void CheckpointEditor::Load()
{
	std::ifstream ifs(StageManager::Instance().ResolvePath("checkpoints.csv"));
	if (!ifs) { return; }

	m_positions.clear();

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
		m_positions.push_back(p);
	}

	m_dirty = true;
}
