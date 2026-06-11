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
	if (ImGui::Button("Add Cubun"))
	{
		EnemyPlacementData d;
		d.type = EnemyType::Cubun;
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

		ImGui::Text("Inspector");

		// 種類切替
		int typeIdx = (p.type == EnemyType::Cubun) ? 0 : 1;
		const char* typeNames[] = { "Cubun", "Ranged" };
		if (ImGui::Combo("Type", &typeIdx, typeNames, 2))
		{
			p.type  = (typeIdx == 0) ? EnemyType::Cubun : EnemyType::Ranged;
			m_dirty = true;
		}

		float pos[3] = { p.position.x, p.position.y, p.position.z };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			p.position = { pos[0], pos[1], pos[2] };
			m_dirty    = true;
		}

		// Cubun 専用設定
		if (p.type == EnemyType::Cubun)
		{
			ImGui::Separator();
			ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "-- Cubun Settings --");

			// 出現向き
			int faceIdx = (p.cubunFaceDir == CubunFaceDir::Up) ? 0 : 1;
			const char* faceNames[] = { "Up (通常)", "Down (逆さ)" };
			if (ImGui::Combo("Face Dir", &faceIdx, faceNames, 2))
			{
				p.cubunFaceDir = (faceIdx == 0) ? CubunFaceDir::Up : CubunFaceDir::Down;
				m_dirty = true;
			}

			// 初期手動重力方向
			int gravIdx = 0;
			if      (p.initGravDir == Character::ManualGravityDir::None) { gravIdx = 0; }
			else if (p.initGravDir == Character::ManualGravityDir::Down) { gravIdx = 1; }
			else if (p.initGravDir == Character::ManualGravityDir::Up)   { gravIdx = 2; }
			const char* gravNames[] = { "None (自動)", "Down (下)", "Up (上)" };
			if (ImGui::Combo("Init Gravity", &gravIdx, gravNames, 3))
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
	std::ofstream ofs(SavePath);
	if (!ofs) { return; }

	for (const auto& p : m_placements)
	{
		// type, x, y, z, faceDir, initGravDir
		const int typeInt = (p.type == EnemyType::Cubun) ? 0 : 1;
		const int faceInt = (p.cubunFaceDir == CubunFaceDir::Up) ? 0 : 1;
		int gravInt = 0;

		if      (p.initGravDir == Character::ManualGravityDir::Down) { gravInt = 1; }

		else if (p.initGravDir == Character::ManualGravityDir::Up)   { gravInt = 2; }

		ofs << typeInt << ","
			<< p.position.x << "," << p.position.y << "," << p.position.z << ","
			<< faceInt << "," << gravInt
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
		d.type       = (std::stoi(tokens[0]) == 0) ? EnemyType::Cubun : EnemyType::Ranged;
		d.position.x = std::stof(tokens[1]);
		d.position.y = std::stof(tokens[2]);
		d.position.z = std::stof(tokens[3]);

		// 旧フォーマット（4列）との互換性を保ちつつ新列を読む
		if (tokens.size() >= 5)
		{
			d.cubunFaceDir = (std::stoi(tokens[4]) == 0) ? CubunFaceDir::Up : CubunFaceDir::Down;
		}
		if (tokens.size() >= 6)
		{
			const int g = std::stoi(tokens[5]);
			if      (g == 1) { d.initGravDir = Character::ManualGravityDir::Down; }
			else if (g == 2) { d.initGravDir = Character::ManualGravityDir::Up;   }
			else             { d.initGravDir = Character::ManualGravityDir::None;  }
		}

		m_placements.push_back(d);
	}

	m_dirty = true;
}

