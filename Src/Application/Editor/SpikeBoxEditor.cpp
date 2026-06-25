#include "../../Pch.h"
#include "SpikeBoxEditor.h"
#include "../Manager/StageManager.h"
#include <fstream>
#include <sstream>

//----------------------------------------------------------
// DrawGui
//----------------------------------------------------------
void SpikeBoxEditor::DrawGui()
{
	if (!ImGui::Begin(U8("スパイクボックス エディタ")))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(U8("ボックス追加")))
	{
		m_boxes.push_back(SpikeBoxData{});
		m_selectedIndex = static_cast<int>(m_boxes.size()) - 1;
		m_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("保存")))   { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("再読込"))) { Load(); }

	ImGui::Separator();

	ImGui::Text(U8("スパイクボックス一覧"));
	for (int i = 0; i < static_cast<int>(m_boxes.size()); ++i)
	{
		const auto& b = m_boxes[i];
		char label[128];
		std::snprintf(label, sizeof(label),
			"[%d] (%.1f, %.1f, %.1f) size(%.2f,%.2f,%.2f)##%d",
			i, b.center.x, b.center.y, b.center.z,
			b.size.x, b.size.y, b.size.z, i);

		const bool selected = (m_selectedIndex == i);
		if (ImGui::Selectable(label, selected)) { m_selectedIndex = i; }
	}

	ImGui::Separator();

	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_boxes.size()))
	{
		auto& b = m_boxes[m_selectedIndex];

		ImGui::Text(U8("インスペクタ [%d]"), m_selectedIndex);

		if (ImGui::Checkbox(U8("有効"), &b.enabled)) { m_dirty = true; }

		float center[3] = { b.center.x, b.center.y, b.center.z };
		if (ImGui::DragFloat3(U8("中心"), center, 0.1f))
		{
			b.center = { center[0], center[1], center[2] };
			m_dirty  = true;
		}

		float size[3] = { b.size.x, b.size.y, b.size.z };
		if (ImGui::DragFloat3(U8("サイズ"), size, 0.05f, 0.05f, 100.0f))
		{
			b.size  = { size[0], size[1], size[2] };
			m_dirty = true;
		}

		// 棘の向き（箱ごと回転）
		{
			const char* dirItems[] = { U8("上"), U8("下"), U8("左"), U8("右") };
			int dirIdx = static_cast<int>(b.dir);
			if (ImGui::Combo(U8("向き"), &dirIdx, dirItems, IM_ARRAYSIZE(dirItems)))
			{
				b.dir   = static_cast<SpikeBoxConst::SpikeDir>(dirIdx);
				m_dirty = true;
			}
		}

		// 追従する移動床（-1=なし）。移動床のindexを入れるとその床に乗って動く
		if (ImGui::InputInt(U8("追従する移動床(-1=なし)"), &b.attachFloor)) { m_dirty = true; }

		ImGui::Spacing();

		if (ImGui::Button(U8("削除")))
		{
			m_boxes.erase(m_boxes.begin() + m_selectedIndex);
			m_selectedIndex = -1;
			m_dirty = true;
		}
	}

	ImGui::End();
}

//----------------------------------------------------------
// DrawDebug ─ ワイヤーフレームで範囲を可視化
//----------------------------------------------------------
void SpikeBoxEditor::DrawDebug() const
{
	for (const auto& b : m_boxes)
	{
		if (!b.enabled) { continue; }
		KdDebugWireFrame wire;
		wire.AddDebugBox(Math::Matrix::CreateTranslation(b.center), b.size,
			Math::Vector3::Zero, false, { 1.0f, 0.3f, 0.2f, 0.7f });
		wire.Draw();
	}
}

//----------------------------------------------------------
// Save
//----------------------------------------------------------
void SpikeBoxEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("spike_boxes.csv"));
	if (!ofs) { return; }

	for (const auto& b : m_boxes)
	{
		ofs << b.center.x << ","
			<< b.center.y << ","
			<< b.center.z << ","
			<< b.size.x   << ","
			<< b.size.y   << ","
			<< b.size.z   << ","
			<< (b.enabled ? 1 : 0) << ","
			<< static_cast<int>(b.dir) << ","
			<< b.attachFloor << "\n";
	}
}

//----------------------------------------------------------
// Load
//----------------------------------------------------------
void SpikeBoxEditor::Load()
{
	m_boxes.clear();
	m_selectedIndex = -1;

	std::ifstream ifs(StageManager::Instance().ResolvePath("spike_boxes.csv"));
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 6) { continue; }

		SpikeBoxData d;
		d.center  = { std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2]) };
		d.size    = { std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]) };
		if (tokens.size() >= 7) { d.enabled = (std::stoi(tokens[6]) != 0); }
		if (tokens.size() >= 8)
		{
			const int di = std::stoi(tokens[7]);
			if (di >= 0 && di <= 3) { d.dir = static_cast<SpikeBoxConst::SpikeDir>(di); }
		}
		if (tokens.size() >= 9) { d.attachFloor = std::stoi(tokens[8]); }

		m_boxes.push_back(d);
	}
}
