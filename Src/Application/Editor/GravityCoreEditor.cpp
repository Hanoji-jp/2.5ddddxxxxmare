#include "../../Pch.h"
#include "GravityCoreEditor.h"
#include "../Manager/StageManager.h"
#include <fstream>
#include <sstream>

//----------------------------------------------------------
// DrawGui
//----------------------------------------------------------
void GravityCoreEditor::DrawGui()
{
	ImGui::SetNextWindowPos(ImVec2(20, 300), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(340, 280), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(U8("重力コア エディタ")))
	{
		ImGui::End();
		return;
	}

	// ── ボタン ─────────────────────────────────────────────
	if (ImGui::Button(U8("コア追加")))
	{
		m_cores.push_back(GravityCoreData{});
		m_selectedIndex = static_cast<int>(m_cores.size()) - 1;
		m_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("保存")))   { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("再読込"))) { Load(); }

	ImGui::Separator();

	// ── リスト ─────────────────────────────────────────────
	ImGui::Text(U8("コア一覧  (合計%d)"), static_cast<int>(m_cores.size()));
	for (int i = 0; i < static_cast<int>(m_cores.size()); ++i)
	{
		const auto& c = m_cores[i];
		char label[128];
		std::snprintf(label, sizeof(label),
			"[%d] (%.1f, %.1f, %.1f) r=%.2f%s##%d",
			i, c.pos.x, c.pos.y, c.pos.z, c.radius,
			c.enabled ? "" : " [OFF]", i);

		const bool selected = (m_selectedIndex == i);
		if (ImGui::Selectable(label, selected)) { m_selectedIndex = i; }
	}

	ImGui::Separator();

	// ── インスペクター ────────────────────────────────────
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_cores.size()))
	{
		auto& c = m_cores[m_selectedIndex];
		ImGui::Text(U8("インスペクタ [%d]"), m_selectedIndex);

		if (ImGui::Checkbox(U8("有効"), &c.enabled)) { m_dirty = true; }

		float pos[3] = { c.pos.x, c.pos.y, c.pos.z };
		if (ImGui::DragFloat3(U8("位置"), pos, 0.1f))
		{
			c.pos   = { pos[0], pos[1], pos[2] };
			m_dirty = true;
		}

		if (ImGui::DragFloat(U8("半径"), &c.radius, 0.05f, 0.1f, 10.0f))
		{
			m_dirty = true;
		}

		// タイプ選択
		{
			int typeInt = static_cast<int>(c.type);
			if (ImGui::RadioButton(U8("岩"), typeInt == 0)) { c.type = CoreType::Rock; m_dirty = true; }
			ImGui::SameLine();
			if (ImGui::RadioButton(U8("発光"), typeInt == 1)) { c.type = CoreType::Glow; m_dirty = true; }
		}

		ImGui::Spacing();
		if (ImGui::Button(U8("削除")))
		{
			m_cores.erase(m_cores.begin() + m_selectedIndex);
			m_selectedIndex = -1;
			m_dirty = true;
		}
	}

	ImGui::End();
}

//----------------------------------------------------------
// DrawDebug ─ コアの位置をワイヤー球で可視化
//----------------------------------------------------------
void GravityCoreEditor::DrawDebug() const
{
	for (const auto& c : m_cores)
	{
		if (!c.enabled) { continue; }

		KdDebugWireFrame wire;
		// 球の代わりにボックスで代用（エンジンにデバッグ球がない場合）
		const Math::Vector3 boxSize = { c.radius * 2.0f, c.radius * 2.0f, c.radius * 2.0f };
		const Math::Vector4 debugCol = (c.type == CoreType::Glow)
			? Math::Vector4{ 0.1f, 0.7f, 1.0f, 0.9f }   // Glow: シアン
			: Math::Vector4{ 1.0f, 0.7f, 0.1f, 0.8f };  // Rock: 黄橙

		wire.AddDebugBox(
			Math::Matrix::CreateTranslation(c.pos),
			boxSize,
			Math::Vector3::Zero,
			false,
			debugCol);

		wire.Draw();
	}
}

//----------------------------------------------------------
// Save ─ CSV へ書き出し
//----------------------------------------------------------
void GravityCoreEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("gravity_cores.csv"));
	if (!ofs) { return; }

	for (const auto& c : m_cores)
	{
		ofs << c.pos.x    << ","
			<< c.pos.y    << ","
			<< c.pos.z    << ","
			<< c.radius   << ","
			<< static_cast<int>(c.type) << ","
			<< (c.enabled ? 1 : 0) << "\n";
	}
}

//----------------------------------------------------------
// Load ─ CSV から読み込み
//----------------------------------------------------------
void GravityCoreEditor::Load()
{
	m_cores.clear();
	m_selectedIndex = -1;

	KdAssetIStream ifs(StageManager::Instance().ResolvePath("gravity_cores.csv"));
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 5) { continue; }

		GravityCoreData d;
		d.pos     = { std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2]) };
		d.radius  = std::stof(tokens[3]);
		if (tokens.size() >= 6)
		{
			d.type    = static_cast<CoreType>(std::stoi(tokens[4]));
			d.enabled = (std::stoi(tokens[5]) != 0);
		}
		else
		{
			// 旧 CSV（type なし）との後方互換
			d.type    = CoreType::Rock;
			d.enabled = (std::stoi(tokens[4]) != 0);
		}
		m_cores.push_back(d);
	}
}
