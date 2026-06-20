#include "../../Pch.h"
#include "WarpHoleEditor.h"
#include "../Manager/StageManager.h"
#include <fstream>
#include <sstream>

void WarpHoleEditor::DrawGui()
{
	if (!ImGui::Begin("WarpHole Editor"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Add WarpHole"))
	{
		m_holes.push_back(WarpHoleData{});
		m_selectedIndex = static_cast<int>(m_holes.size()) - 1;
		m_dirty = true;
	}

	ImGui::Separator();
	ImGui::Text("WarpHole List");

	for (int i = 0; i < static_cast<int>(m_holes.size()); ++i)
	{
		const auto& h = m_holes[i];
		char label[64];
		std::snprintf(label, sizeof(label), "[%d] Entry(%.1f,%.1f) -> Exit(%.1f,%.1f)##%d",
			i, h.EntryPos.x, h.EntryPos.y, h.ExitPos.x, h.ExitPos.y, i);

		if (ImGui::Selectable(label, m_selectedIndex == i))
			m_selectedIndex = i;
	}

	ImGui::Separator();

	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_holes.size()))
	{
		auto& h = m_holes[m_selectedIndex];
		ImGui::Text("Inspector [%d]", m_selectedIndex);

		float entry[3] = { h.EntryPos.x, h.EntryPos.y, h.EntryPos.z };
		if (ImGui::DragFloat3("Entry Pos", entry, 0.1f))
		{
			h.EntryPos = { entry[0], entry[1], entry[2] };
			m_dirty = true;
		}

		float exitP[3] = { h.ExitPos.x, h.ExitPos.y, h.ExitPos.z };
		if (ImGui::DragFloat3("Exit Pos", exitP, 0.1f))
		{
			h.ExitPos = { exitP[0], exitP[1], exitP[2] };
			m_dirty = true;
		}

		float exitDir[3] = { h.ExitDir.x, h.ExitDir.y, h.ExitDir.z };
		if (ImGui::DragFloat3("Exit Dir", exitDir, 0.01f, -1.0f, 1.0f))
		{
			h.ExitDir = Math::Vector3(exitDir[0], exitDir[1], exitDir[2]);
			h.ExitDir.Normalize();
			m_dirty = true;
		}

		// ---- 口元（開口部）の向き ----
		//   (0,0,0) なら自動（入口⇔出口の直線方向）。中継点の向きに影響されない。
		ImGui::Separator();
		ImGui::TextDisabled("Mouth Dir (0,0,0 = Auto)");

		float entryMouth[3] = { h.EntryMouthDir.x, h.EntryMouthDir.y, h.EntryMouthDir.z };
		if (ImGui::DragFloat3("Entry Mouth Dir", entryMouth, 0.01f, -1.0f, 1.0f))
		{
			h.EntryMouthDir = { entryMouth[0], entryMouth[1], entryMouth[2] };
			m_dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset##EntryMouth"))
		{
			h.EntryMouthDir = { 0.0f, 0.0f, 0.0f };
			m_dirty = true;
		}

		float exitMouth[3] = { h.ExitMouthDir.x, h.ExitMouthDir.y, h.ExitMouthDir.z };
		if (ImGui::DragFloat3("Exit Mouth Dir", exitMouth, 0.01f, -1.0f, 1.0f))
		{
			h.ExitMouthDir = { exitMouth[0], exitMouth[1], exitMouth[2] };
			m_dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset##ExitMouth"))
		{
			h.ExitMouthDir = { 0.0f, 0.0f, 0.0f };
			m_dirty = true;
		}
		ImGui::Separator();

		bool enabled = h.Enabled;
		if (ImGui::Checkbox("Enabled", &enabled))
		{
			h.Enabled  = enabled;
			m_dirty    = true;
		}

		// テレポート型かどうか
		bool teleport = h.Teleport;
		if (ImGui::Checkbox("Teleport (暗転瞬間移動)", &teleport))
		{
			h.Teleport = teleport;
			m_dirty    = true;
		}
		if (h.Teleport)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("← Waypoints は使用されません");
		}

		// 一歩通行 / 双方向
		bool oneWay = h.OneWay;
		if (ImGui::Checkbox("One Way (一歩通行)", &oneWay))
		{
			h.OneWay = oneWay;
			m_dirty  = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled(h.OneWay ? "Entry→Exit のみ" : "Entry⇔Exit 双方向");

		// ---- Waypoint リスト ----
		ImGui::Separator();
		ImGui::Text("Waypoints (%d)", static_cast<int>(h.Waypoints.size()));
		if (ImGui::Button("Add Waypoint"))
		{
			// 最後のポイントの少し先に追加
			Math::Vector3 last = h.Waypoints.empty() ? h.EntryPos : h.Waypoints.back();
			h.Waypoints.push_back(last + Math::Vector3(2.0f, 2.0f, 0.0f));
			m_dirty = true;
		}
		for (int wi = 0; wi < static_cast<int>(h.Waypoints.size()); ++wi)
		{
			ImGui::PushID(wi);
			float wp[3] = { h.Waypoints[wi].x, h.Waypoints[wi].y, h.Waypoints[wi].z };
			char label[32];
			sprintf_s(label, "WP [%d]", wi);
			if (ImGui::DragFloat3(label, wp, 0.1f))
			{
				h.Waypoints[wi] = { wp[0], wp[1], wp[2] };
				m_dirty = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Del"))
			{
				h.Waypoints.erase(h.Waypoints.begin() + wi);
				m_dirty = true;
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::Separator();

		ImGui::Spacing();
		if (ImGui::Button("Delete"))
		{
			m_holes.erase(m_holes.begin() + m_selectedIndex);
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

void WarpHoleEditor::DrawDebug() const
{
	for (int i = 0; i < static_cast<int>(m_holes.size()); ++i)
	{
		const auto& h = m_holes[i];
		if (!h.Enabled) { continue; }

		// 入口：シアン
		{
			KdDebugWireFrame wire;
			wire.AddDebugSphere(h.EntryPos, WarpHoleConst::SuckRadius, { 0.0f, 1.0f, 1.0f, 1.0f });
			wire.Draw();
		}
		// 出口：マゼンタ
		{
			KdDebugWireFrame wire;
			wire.AddDebugSphere(h.ExitPos, WarpHoleConst::SuckRadius, { 1.0f, 0.0f, 1.0f, 1.0f });
			wire.Draw();
		}
		// 経路ライン（入口→Waypoints→出口、黄色）
		{
			const auto path = h.GetFullPath();
			KdDebugWireFrame wire;
			for (int pi = 0; pi + 1 < static_cast<int>(path.size()); ++pi)
			{
				wire.AddDebugLine(path[pi], path[pi + 1], { 1.0f, 1.0f, 0.0f, 1.0f });
			}
			wire.Draw();
		}
		// Waypoint 球（オレンジ）
		{
			KdDebugWireFrame wire;
			for (const auto& wp : h.Waypoints)
			{
				wire.AddDebugSphere(wp, 0.4f, { 1.0f, 0.5f, 0.0f, 1.0f });
			}
			wire.Draw();
		}
		// 射出方向矢印（緑）
		{
			const Math::Vector3 arrowEnd = h.ExitPos + h.ExitDir * 2.0f;
			KdDebugWireFrame wire;
			wire.AddDebugLine(h.ExitPos, arrowEnd, { 0.0f, 1.0f, 0.0f, 1.0f });
			wire.Draw();
		}
	}
}

void WarpHoleEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("warp_holes.csv"));
	if (!ofs) { return; }

	for (const auto& h : m_holes)
	{
		ofs << h.EntryPos.x << "," << h.EntryPos.y << "," << h.EntryPos.z << ","
			<< h.ExitPos.x  << "," << h.ExitPos.y  << "," << h.ExitPos.z  << ","
			<< h.ExitDir.x  << "," << h.ExitDir.y  << "," << h.ExitDir.z  << ","
			<< (h.Enabled ? 1 : 0) << ","
			<< static_cast<int>(h.Waypoints.size());
		for (const auto& wp : h.Waypoints)
		{
			ofs << "," << wp.x << "," << wp.y << "," << wp.z;
		}
		// 口元方向（後方互換のため末尾に追記）
		ofs << "," << h.EntryMouthDir.x << "," << h.EntryMouthDir.y << "," << h.EntryMouthDir.z
			<< "," << h.ExitMouthDir.x  << "," << h.ExitMouthDir.y  << "," << h.ExitMouthDir.z;
		// テレポート型フラグ（後方互換）
		ofs << "," << (h.Teleport ? 1 : 0);
		// 一歩通行フラグ（後方互換）
		ofs << "," << (h.OneWay ? 1 : 0);
		ofs << "\n";
	}
}

void WarpHoleEditor::Load()
{
	m_holes.clear();
	std::ifstream ifs(StageManager::Instance().ResolvePath("warp_holes.csv"));
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }
		std::istringstream ss(line);
		std::string token;
		std::vector<float> vals;
		while (std::getline(ss, token, ','))
		{
			try { vals.push_back(std::stof(token)); }
			catch (...) {}
		}
		if (vals.size() < 11) { continue; }

		WarpHoleData h;
		h.EntryPos = { vals[0], vals[1], vals[2] };
		h.ExitPos  = { vals[3], vals[4], vals[5] };
		h.ExitDir  = { vals[6], vals[7], vals[8] };
		h.Enabled  = (static_cast<int>(vals[9]) != 0);
		const int wpCount = static_cast<int>(vals[10]);
		for (int wi = 0; wi < wpCount; ++wi)
		{
			const int base = 11 + wi * 3;
			if (base + 2 < static_cast<int>(vals.size()))
			{
				h.Waypoints.push_back({ vals[base], vals[base + 1], vals[base + 2] });
			}
		}
		// 口元方向（後方互換：存在すれば読み込む。無ければ自動=(0,0,0)）
		const int mouthBase = 11 + wpCount * 3;
		if (mouthBase + 5 < static_cast<int>(vals.size()))
		{
			h.EntryMouthDir = { vals[mouthBase],     vals[mouthBase + 1], vals[mouthBase + 2] };
			h.ExitMouthDir  = { vals[mouthBase + 3], vals[mouthBase + 4], vals[mouthBase + 5] };
		}
		// テレポート型フラグ（後方互換）
		const int teleportIdx = mouthBase + 6;
		if (teleportIdx < static_cast<int>(vals.size()))
		{
			h.Teleport = (static_cast<int>(vals[teleportIdx]) != 0);
		}
		// 一歩通行フラグ（後方互換：存在しなければデフォルト=一歩通行）
		const int oneWayIdx = mouthBase + 7;
		if (oneWayIdx < static_cast<int>(vals.size()))
		{
			h.OneWay = (static_cast<int>(vals[oneWayIdx]) != 0);
		}
		m_holes.push_back(h);
	}
}
