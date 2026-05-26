#include "../../Pch.h"
#include "PlanetGravityManager.h"
#include <fstream>
#include <sstream>

void PlanetGravityManager::DrawGui()
{
	if (!ImGui::Begin("Planet Gravity"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Add Planet"))
	{
		m_planets.push_back(PlanetData{});
		m_selectedIndex = static_cast<int>(m_planets.size()) - 1;
	}

	ImGui::Separator();
	ImGui::Text("Planet List");

	for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
	{
		const auto& p = m_planets[i];
		char label[80];
		std::snprintf(label, sizeof(label), "[%d] (%.1f, %.1f, %.1f) R=%.1f##%d",
			i, p.Position.x, p.Position.y, p.Position.z, p.SurfaceRadius, i);

		if (ImGui::Selectable(label, m_selectedIndex == i))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_planets.size()))
	{
		auto& p = m_planets[m_selectedIndex];
		ImGui::Text("Inspector");

		float pos[3] = { p.Position.x, p.Position.y, p.Position.z };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			p.Position = { pos[0], pos[1], pos[2] };
		}

		ImGui::DragFloat("Surface Radius",  &p.SurfaceRadius, 0.1f, 0.5f, 1000.0f);
		ImGui::DragFloat("Gravity Radius",  &p.GravityRadius, 0.1f, 1.0f, 2000.0f);

		if (ImGui::Button("Delete"))
		{
			m_planets.erase(m_planets.begin() + m_selectedIndex);
			m_selectedIndex = -1;
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Save")) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(); }

	ImGui::End();
}

void PlanetGravityManager::DrawDebugSpheres() const
{
	for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
	{
		const auto& p       = m_planets[i];
		const bool selected = (i == m_selectedIndex);

		// サーフェス球（水色 or 黄色）
		const Math::Vector4 surfaceColor = selected
			? Math::Vector4(1.0f, 1.0f, 0.0f, 1.0f)
			: Math::Vector4(PlanetConst::SurfaceColorR, PlanetConst::SurfaceColorG,
							PlanetConst::SurfaceColorB, PlanetConst::SurfaceColorA);

		KdDebugWireFrame wireSurface;
		wireSurface.AddDebugSphere(p.Position, p.SurfaceRadius, surfaceColor);
		wireSurface.Draw();

		// 引力範囲球（薄い青）
		KdDebugWireFrame wireGravity;
		wireGravity.AddDebugSphere(p.Position, p.GravityRadius,
			{ PlanetConst::GravityColorR, PlanetConst::GravityColorG,
			  PlanetConst::GravityColorB, PlanetConst::GravityColorA });
		wireGravity.Draw();
	}
}

const PlanetData* PlanetGravityManager::FindNearestPlanet(const Math::Vector3& _charPos) const
{
	const PlanetData* pBest   = nullptr;
	float             bestDist = FLT_MAX;

	for (const auto& p : m_planets)
	{
		// Z軸は無視してXY平面距離で判定（シリンダー形地面）
		const float dx   = _charPos.x - p.Position.x;
		const float dy   = _charPos.y - p.Position.y;
		const float dist = std::sqrtf(dx * dx + dy * dy);
		if (dist < p.GravityRadius && dist < bestDist)
		{
			bestDist = dist;
			pBest    = &p;
		}
	}

	return pBest;
}

void PlanetGravityManager::Save() const
{
	std::ofstream ofs(PlanetConst::SavePath);
	if (!ofs) { return; }

	for (const auto& p : m_planets)
	{
		ofs << p.Position.x << ","
			<< p.Position.y << ","
			<< p.Position.z << ","
			<< p.SurfaceRadius << ","
			<< p.GravityRadius << "\n";
	}
}

void PlanetGravityManager::Load()
{
	std::ifstream ifs(PlanetConst::SavePath);
	if (!ifs) { return; }

	m_planets.clear();

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 5) { continue; }

		PlanetData p;
		p.Position.x    = std::stof(tokens[0]);
		p.Position.y    = std::stof(tokens[1]);
		p.Position.z    = std::stof(tokens[2]);
		p.SurfaceRadius = std::stof(tokens[3]);
		p.GravityRadius = std::stof(tokens[4]);
		m_planets.push_back(p);
	}
}
