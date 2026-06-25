#include "../../Pch.h"
#include "WindBoxEditor.h"
#include "../Manager/StageManager.h"
#include <fstream>
#include <sstream>
#include <cmath>

//----------------------------------------------------------
// DrawGui
//----------------------------------------------------------
void WindBoxEditor::DrawGui()
{
	if (!ImGui::Begin(U8("風ボックス エディタ")))
	{
		ImGui::End();
		return;
	}

	// ── 追加ボタン ─────────────────────────────────────────
	if (ImGui::Button(U8("ボックス追加")))
	{
		m_boxes.push_back(WindBoxData{});
		m_selectedIndex = static_cast<int>(m_boxes.size()) - 1;
		m_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("保存")))   { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("再読込"))) { Load(); }

	ImGui::Separator();

	// ── リスト ─────────────────────────────────────────────
	ImGui::Text(U8("風ボックス一覧"));
	for (int i = 0; i < static_cast<int>(m_boxes.size()); ++i)
	{
		const auto& b = m_boxes[i];
		char label[128];
		std::snprintf(label, sizeof(label),
			"[%d] (%.1f, %.1f, %.1f) dir(%.2f,%.2f,%.2f) pow=%.3f len=%.1f##%d",
			i, b.center.x, b.center.y, b.center.z,
			b.windDir.x, b.windDir.y, b.windDir.z, b.power, b.length, i);

		const bool selected = (m_selectedIndex == i);
		if (ImGui::Selectable(label, selected))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	// ── インスペクター ────────────────────────────────────
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_boxes.size()))
	{
		auto& b = m_boxes[m_selectedIndex];

		ImGui::Text(U8("インスペクタ [%d]"), m_selectedIndex);

		// 有効フラグ
		if (ImGui::Checkbox(U8("有効"), &b.enabled)) { m_dirty = true; }

		// 中心位置
		float center[3] = { b.center.x, b.center.y, b.center.z };
		if (ImGui::DragFloat3(U8("中心"), center, 0.1f))
		{
			b.center = { center[0], center[1], center[2] };
			m_dirty  = true;
		}

		// サイズ
		float size[3] = { b.size.x, b.size.y, b.size.z };
		if (ImGui::DragFloat3(U8("サイズ"), size, 0.1f, 0.1f, 100.0f))
		{
			b.size  = { size[0], size[1], size[2] };
			m_dirty = true;
		}

		// 風向き：上・左・右の3択ボタン
		{
			// 現在の方向から選択中を判定
			enum class Dir { Up, Left, Right };
			Dir cur = Dir::Up;
			if      (b.windDir.x >  0.5f) { cur = Dir::Right; }
			else if (b.windDir.x < -0.5f) { cur = Dir::Left;  }

			bool changed = false;
			if (ImGui::RadioButton(U8("上"),    cur == Dir::Up))    { b.windDir = {  0.0f, 1.0f, 0.0f }; changed = true; }
			ImGui::SameLine();
			if (ImGui::RadioButton(U8("左"),  cur == Dir::Left))  { b.windDir = { -1.0f, 0.0f, 0.0f }; changed = true; }
			ImGui::SameLine();
			if (ImGui::RadioButton(U8("右"), cur == Dir::Right)) { b.windDir = {  1.0f, 0.0f, 0.0f }; changed = true; }

			if (changed) { m_dirty = true; }

			ImGui::Text(U8("向き: (%.2f, %.2f, %.2f)"), b.windDir.x, b.windDir.y, b.windDir.z);
		}

		// 上昇力
		if (ImGui::DragFloat(U8("強さ"), &b.power, 0.001f, 0.0f, 1.0f))
		{
			m_dirty = true;
		}

		// 風が届く距離（COL上端からの延長）
		if (ImGui::DragFloat(U8("長さ"), &b.length, 0.1f, 0.0f, 50.0f))
		{
			m_dirty = true;
		}

		// 追従する移動床（-1=なし）
		if (ImGui::InputInt(U8("追従する移動床(-1=なし)"), &b.attachFloor)) { m_dirty = true; }

		ImGui::Spacing();

		// 削除ボタン
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
// DrawDebug  ─ ワイヤーフレームで範囲と風向きを可視化
//----------------------------------------------------------
void WindBoxEditor::DrawDebug() const
{
	for (const auto& b : m_boxes)
	{
		if (!b.enabled) { continue; }

		KdDebugWireFrame wire;
		// 範囲ボックス（水色）
		wire.AddDebugBox(Math::Matrix::CreateTranslation(b.center), b.size,
			Math::Vector3::Zero, false, { 0.2f, 0.8f, 1.0f, 0.7f });

		// 風向き矢印（中心から）
		const Math::Vector3 tip = b.center + b.windDir * (b.size.x * 0.5f + 0.5f);
		wire.AddDebugLine(b.center, tip, { 0.0f, 1.0f, 0.5f, 1.0f });

		wire.Draw();
	}
}

//----------------------------------------------------------
// Save  ─ CSV へ書き出し
//----------------------------------------------------------
void WindBoxEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("wind_boxes.csv"));
	if (!ofs) { return; }

	for (const auto& b : m_boxes)
	{
		ofs << b.center.x << ","
			<< b.center.y << ","
			<< b.center.z << ","
			<< b.size.x   << ","
			<< b.size.y   << ","
			<< b.size.z   << ","
			<< b.windDir.x << ","
				<< b.windDir.y << ","
				<< b.windDir.z << ","
				<< b.power     << ","
				<< b.length    << ","
				<< (b.enabled ? 1 : 0) << ","
				<< b.attachFloor << "\n";
	}
}

//----------------------------------------------------------
// Load  ─ CSV から読み込み
//----------------------------------------------------------
void WindBoxEditor::Load()
{
	m_boxes.clear();
	m_selectedIndex = -1;

	std::ifstream ifs(StageManager::Instance().ResolvePath("wind_boxes.csv"));
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 10) { continue; }

		WindBoxData d;
		d.center  = { std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2]) };
		d.size    = { std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]) };
		d.windDir = { std::stof(tokens[6]), std::stof(tokens[7]), std::stof(tokens[8]) };
		// tokens[9] = power, tokens[10] = length (旧フォーマットは strength のみ)
		d.power  = std::stof(tokens[9]);
		if (tokens.size() >= 12)
		{
			d.length  = std::stof(tokens[10]);
			d.enabled = (std::stoi(tokens[11]) != 0);
			if (tokens.size() >= 13) { d.attachFloor = std::stoi(tokens[12]); }
		}
		else if (tokens.size() >= 11)
		{
			// 旧 CSV (strength, enabled) の後方互換
			d.enabled = (std::stoi(tokens[10]) != 0);
		}

		// 安全のため正規化
		if (d.windDir.LengthSquared() > 0.0001f) { d.windDir.Normalize(); }

		m_boxes.push_back(d);
	}
}
