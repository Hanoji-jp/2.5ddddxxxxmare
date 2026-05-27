#include "../../Pch.h"
#include "ManualGravityZoneManager.h"
#include "ModelManager.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include <fstream>
#include <sstream>

constexpr const char* SavePath = "Asset/Data/ManualGravityZones.csv";
constexpr float kBackgroundDepth = 2.0f;  // 背景Boxを奥に配置する距離
constexpr float kBackgroundThickness = 0.5f;  // 背景Boxの厚み（Z方向）
constexpr float kBoxModelHalfSize = 1.0f;  // Box.gltfのハーフサイズ（-1〜+1なので1.0）

void ManualGravityZone::UpdateWorld()
{
	// 背景用に少し奥（Z+方向）に配置し、薄く（Z方向に圧縮）する
	Math::Vector3 bgCenter = Center;
	bgCenter.z += kBackgroundDepth;

	// モデルをそのままのスケールで使用
	Math::Vector3 bgScale;
	bgScale.x = HalfExtents.x / kBoxModelHalfSize;  // モデルのハーフサイズで割る
	bgScale.y = HalfExtents.y / kBoxModelHalfSize;
	bgScale.z = kBackgroundThickness;  // Z方向は薄く固定

	const Math::Matrix scale = Math::Matrix::CreateScale(bgScale);
	const Math::Matrix trans = Math::Matrix::CreateTranslation(bgCenter);
	mWorld = scale * trans;
}

bool ManualGravityZoneManager::CanUseManualGravity(const Math::Vector3& _pos) const
{
	// ゾーンが1つもない場合はどこでも使用可能
	if (m_zones.empty()) { return true; }

	// いずれかのゾーン内にいれば使用可能
	for (const auto& zone : m_zones)
	{
		if (!zone.bEnabled) { continue; }

		// AABB 判定
		const Math::Vector3 localPos = _pos - zone.Center;
		if (std::abs(localPos.x) <= zone.HalfExtents.x &&
			std::abs(localPos.y) <= zone.HalfExtents.y &&
			std::abs(localPos.z) <= zone.HalfExtents.z)
		{
			return true;
		}
	}

	return false;
}

void ManualGravityZoneManager::DrawUnLit() const
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 半透明描画を有効化
	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Alpha);

	for (const auto& zone : m_zones)
	{
		if (!zone.bEnabled || !zone.modelWork || !zone.modelWork->GetData()) { continue; }

		// 半透明の背景として描画（色レートで透明度調整）
		const Math::Color bgColor = { 0.5f, 0.7f, 1.0f, 0.3f };  // 青っぽい半透明
		shader.DrawModel(*zone.modelWork, zone.mWorld, bgColor);
	}

	// ブレンドステートを元に戻す
	KdShaderManager::Instance().UndoBlendState();
}

void ManualGravityZoneManager::DrawDebugShapes() const
{
	for (int i = 0; i < static_cast<int>(m_zones.size()); ++i)
	{
		const auto& zone = m_zones[i];
		if (!zone.bEnabled) { continue; }

		const bool selected = (i == m_selectedIndex);
		const Math::Color color = selected 
			? Math::Color(1.0f, 1.0f, 0.0f, 0.8f)  // 選択中：黄色
			: Math::Color(0.0f, 1.0f, 1.0f, 0.6f); // 通常：シアン

		const float z = zone.Center.z;

		// AABB のワイヤーフレーム描画
		const float minX = zone.Center.x - zone.HalfExtents.x;
		const float maxX = zone.Center.x + zone.HalfExtents.x;
		const float minY = zone.Center.y - zone.HalfExtents.y;
		const float maxY = zone.Center.y + zone.HalfExtents.y;

		KdDebugWireFrame wire;
		wire.AddDebugLine({ minX, minY, z }, { maxX, minY, z }, color);
		wire.AddDebugLine({ maxX, minY, z }, { maxX, maxY, z }, color);
		wire.AddDebugLine({ maxX, maxY, z }, { minX, maxY, z }, color);
		wire.AddDebugLine({ minX, maxY, z }, { minX, minY, z }, color);
		wire.Draw();
	}
}

void ManualGravityZoneManager::DrawGui()
{
	if (!ImGui::Begin("Manual Gravity Zones", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Manual Gravity Zone Editor");
	ImGui::Separator();

	if (ImGui::Button("Add Zone"))
	{
		ManualGravityZone newZone;
		newZone.Center = { 0.0f, 0.0f, 0.0f };
		newZone.HalfExtents = { 20.0f, 20.0f, 5.0f };

		// Boxモデルを読み込み
		newZone.modelWork = std::make_shared<KdModelWork>();
		const auto spModel = ModelManager::Instance().GetModel("Asset/Data/Box.gltf");
		if (spModel)
		{
			newZone.modelWork->SetModelData(spModel);
		}
		newZone.UpdateWorld();

		m_zones.push_back(std::move(newZone));
		m_selectedIndex = static_cast<int>(m_zones.size()) - 1;
	}

	ImGui::Separator();

	// ゾーンリスト
	for (int i = 0; i < static_cast<int>(m_zones.size()); ++i)
	{
		char label[64];
		sprintf_s(label, "Zone %d", i);
		if (ImGui::Selectable(label, m_selectedIndex == i))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	// 選択中のゾーン編集
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size()))
	{
		auto& zone = m_zones[m_selectedIndex];

		ImGui::Text("Zone %d Settings:", m_selectedIndex);

		float center[3] = { zone.Center.x, zone.Center.y, zone.Center.z };
		if (ImGui::DragFloat3("Center", center, 0.5f, -1000.0f, 1000.0f))
		{
			zone.Center = { center[0], center[1], center[2] };
			zone.UpdateWorld();
		}

		float halfExtents[3] = { zone.HalfExtents.x, zone.HalfExtents.y, zone.HalfExtents.z };
		if (ImGui::DragFloat3("Half Extents", halfExtents, 0.5f, 0.1f, 1000.0f))
		{
			zone.HalfExtents = { halfExtents[0], halfExtents[1], halfExtents[2] };
			zone.UpdateWorld();
		}

		if (ImGui::Checkbox("Enabled", &zone.bEnabled))
		{
			zone.UpdateWorld();
		}

		if (ImGui::Button("Delete"))
		{
			m_zones.erase(m_zones.begin() + m_selectedIndex);
			m_selectedIndex = -1;
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Save")) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(); }

	ImGui::End();
}

void ManualGravityZoneManager::Save() const
{
	std::ofstream ofs(SavePath);
	if (!ofs) { return; }

	for (const auto& zone : m_zones)
	{
		ofs << zone.Center.x << ","
			<< zone.Center.y << ","
			<< zone.Center.z << ","
			<< zone.HalfExtents.x << ","
			<< zone.HalfExtents.y << ","
			<< zone.HalfExtents.z << ","
			<< (zone.bEnabled ? 1 : 0) << "\n";
	}
}

void ManualGravityZoneManager::Load()
{
	std::ifstream ifs(SavePath);
	if (!ifs) { return; }

	m_zones.clear();

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 7) { continue; }

		ManualGravityZone zone;
		zone.Center.x = std::stof(tokens[0]);
		zone.Center.y = std::stof(tokens[1]);
		zone.Center.z = std::stof(tokens[2]);
		zone.HalfExtents.x = std::stof(tokens[3]);
		zone.HalfExtents.y = std::stof(tokens[4]);
		zone.HalfExtents.z = std::stof(tokens[5]);
		zone.bEnabled = (std::stoi(tokens[6]) != 0);

		// Boxモデルを読み込み
		zone.modelWork = std::make_shared<KdModelWork>();
		const auto spModel = ModelManager::Instance().GetModel("Asset/Data/Box.gltf");
		if (spModel)
		{
			zone.modelWork->SetModelData(spModel);
		}
		zone.UpdateWorld();

		m_zones.push_back(zone);
	}
}
