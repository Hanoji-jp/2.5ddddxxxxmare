#include "../../Pch.h"
#include "ManualGravityZoneManager.h"
#include "StageManager.h"
#include "ModelManager.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include <fstream>
#include <sstream>

constexpr const char* SaveFileName = "ManualGravityZones.csv";   // ステージ別フォルダ内のファイル名
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

float ManualGravityZoneManager::GetWallFrontZ(int _index) const
{
	if (_index < 0 || _index >= static_cast<int>(m_zones.size())) { return 0.0f; }
	// 背景Boxはカメラの奥(+Z)へ kBackgroundDepth ずらして厚み(±halfDepth)を持つ。
	// そのカメラ側(小さいZ)の面 = center.z + depth - halfDepth
	const float halfDepth = kBackgroundThickness * kBoxModelHalfSize;
	return m_zones[_index].Center.z + kBackgroundDepth - halfDepth;
}

bool ManualGravityZoneManager::CanUseManualGravity(const Math::Vector3& _pos) const
{
	// ゾーンが1つもない場合はどこでも使用可能
	if (m_zones.empty()) { return true; }

	// ManualGravityタイプのゾーン内にいれば使用可能
	for (const auto& zone : m_zones)
	{
		if (!zone.bEnabled) { continue; }
		if (zone.Type != ZoneType::ManualGravity) { continue; }

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

bool ManualGravityZoneManager::IsInsideManualZone(const Math::Vector3& _pos) const
{
	// ゾーンが無い場合は false（惑星重力を抑制しない）
	for (const auto& zone : m_zones)
	{
		if (!zone.bEnabled) { continue; }
		if (zone.Type != ZoneType::ManualGravity) { continue; }

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

bool ManualGravityZoneManager::IsInNormalGravityZone(const Math::Vector3& _pos, Math::Vector3& gravDirOut) const
{
	for (const auto& zone : m_zones)
	{
		if (!zone.bEnabled) { continue; }
		if (zone.Type != ZoneType::NormalGravity) { continue; }

		const Math::Vector3 localPos = _pos - zone.Center;
		if (std::abs(localPos.x) <= zone.HalfExtents.x &&
			std::abs(localPos.y) <= zone.HalfExtents.y &&
			std::abs(localPos.z) <= zone.HalfExtents.z)
		{
			switch (zone.GravityDir)
			{
			case ZoneGravityDir::Down:  gravDirOut = {  0.0f, -1.0f, 0.0f }; break;
			case ZoneGravityDir::Up:    gravDirOut = {  0.0f,  1.0f, 0.0f }; break;
			case ZoneGravityDir::Left:  gravDirOut = { -1.0f,  0.0f, 0.0f }; break;
			case ZoneGravityDir::Right: gravDirOut = {  1.0f,  0.0f, 0.0f }; break;
			}
			return true;
		}
	}
	return false;
}

void ManualGravityZoneManager::DrawUnLit(bool showNormalGravity, bool manualGravityUp) const
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 半透明描画を有効化
	KdShaderManager::Instance().ChangeBlendState(KdBlendState::Alpha);

	for (const auto& zone : m_zones)
	{
		if (!zone.bEnabled || !zone.modelWork || !zone.modelWork->GetData()) { continue; }
		// 通常は ManualGravity の箱のみ。NormalGravity の箱は debug 時だけ
		if (!showNormalGravity && zone.Type == ZoneType::NormalGravity) { continue; }

		// 半透明の背景として描画。ManualGravity は重力方向で色を変える（上=赤/下=青）
		Math::Color bgColor = { 0.5f, 0.7f, 1.0f, 0.3f };   // 既定：青
		if (zone.Type == ZoneType::ManualGravity && manualGravityUp)
		{
			bgColor = { 1.0f, 0.3f, 0.25f, 0.3f };          // 上向き：赤
		}
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
			? Math::Color(1.0f, 1.0f, 0.0f, 0.8f)   // 選択中：黄色
			: (zone.Type == ZoneType::NormalGravity
				? Math::Color(1.0f, 0.4f, 0.0f, 0.8f)   // NormalGravity：オレンジ
				: Math::Color(0.0f, 1.0f, 1.0f, 0.6f)); // ManualGravity：シアン

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
	if (!ImGui::Begin(U8("手動重力ゾーン"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::Text(U8("手動重力ゾーン エディタ"));
	ImGui::Separator();

	if (ImGui::Button(U8("ゾーン追加")))
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

		ImGui::Text(U8("ゾーン %d 設定:"), m_selectedIndex);

		// ZoneType選択
		const char* zoneTypeNames[] = { U8("手動重力"), U8("通常重力 (-Y)") };
		int typeIdx = static_cast<int>(zone.Type);
		if (ImGui::Combo(U8("ゾーン種類"), &typeIdx, zoneTypeNames, IM_ARRAYSIZE(zoneTypeNames)))
		{
			zone.Type = static_cast<ZoneType>(typeIdx);
		}
		if (zone.Type == ZoneType::NormalGravity)
		{
			ImGui::SameLine();
			ImGui::TextColored({ 1.0f, 0.4f, 0.0f, 1.0f }, U8("<- 重力固定ゾーン"));

			const char* dirNames[] = { U8("下 (-Y)"), U8("上 (+Y)"), U8("左 (-X)"), U8("右 (+X)") };
			int dirIdx = static_cast<int>(zone.GravityDir);
			if (ImGui::Combo(U8("重力の向き"), &dirIdx, dirNames, IM_ARRAYSIZE(dirNames)))
			{
				zone.GravityDir = static_cast<ZoneGravityDir>(dirIdx);
			}
		}

		float center[3] = { zone.Center.x, zone.Center.y, zone.Center.z };
		if (ImGui::DragFloat3(U8("中心"), center, 0.5f, -1000.0f, 1000.0f))
		{
			zone.Center = { center[0], center[1], center[2] };
			zone.UpdateWorld();
		}

		float halfExtents[3] = { zone.HalfExtents.x, zone.HalfExtents.y, zone.HalfExtents.z };
		if (ImGui::DragFloat3(U8("ハーフサイズ"), halfExtents, 0.5f, 0.1f, 1000.0f))
		{
			zone.HalfExtents = { halfExtents[0], halfExtents[1], halfExtents[2] };
			zone.UpdateWorld();
		}

		if (ImGui::Checkbox(U8("有効"), &zone.bEnabled))
		{
			zone.UpdateWorld();
		}

		if (ImGui::Button(U8("削除")))
		{
			m_zones.erase(m_zones.begin() + m_selectedIndex);
			m_selectedIndex = -1;
		}
	}

	ImGui::Separator();
	if (ImGui::Button(U8("保存"))) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("読込"))) { Load(); }

	ImGui::End();
}

void ManualGravityZoneManager::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath(SaveFileName));
	if (!ofs) { return; }

	for (const auto& zone : m_zones)
	{
		ofs << zone.Center.x << ","
			<< zone.Center.y << ","
			<< zone.Center.z << ","
			<< zone.HalfExtents.x << ","
			<< zone.HalfExtents.y << ","
			<< zone.HalfExtents.z << ","
			<< (zone.bEnabled ? 1 : 0) << ","
			<< static_cast<int>(zone.Type) << ","
			<< static_cast<int>(zone.GravityDir) << "\n";
	}
}

void ManualGravityZoneManager::Load()
{
	KdAssetIStream ifs(StageManager::Instance().ResolvePath(SaveFileName));

	// 新ステージのファイルが無くても空にする（前ステージのゾーンを残さない）
	m_zones.clear();
	if (!ifs) { return; }

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
		zone.Type = (tokens.size() >= 8)
			? static_cast<ZoneType>(std::stoi(tokens[7]))
			: ZoneType::ManualGravity;
		zone.GravityDir = (tokens.size() >= 9)
			? static_cast<ZoneGravityDir>(std::stoi(tokens[8]))
			: ZoneGravityDir::Down;

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
