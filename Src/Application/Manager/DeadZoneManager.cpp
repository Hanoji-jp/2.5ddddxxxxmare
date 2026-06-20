#include "../../Pch.h"
#include "DeadZoneManager.h"
#include "StageManager.h"
#include "ModelManager.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include <fstream>
#include <sstream>

namespace
{
    constexpr const char* kSaveFileName = "DeadZones.csv";   // file name inside the stage folder
    constexpr float kBackgroundDepth     = 2.0f;   // push background box back (Z+)
    constexpr float kBackgroundThickness = 0.5f;   // thin in Z
    constexpr float kBoxModelHalfSize    = 1.0f;   // Box.gltf half size (-1..+1)
}

void DeadZone::UpdateWorld()
{
    Math::Vector3 bgCenter = Center;
    bgCenter.z += kBackgroundDepth;

    Math::Vector3 bgScale;
    bgScale.x = HalfExtents.x / kBoxModelHalfSize;
    bgScale.y = HalfExtents.y / kBoxModelHalfSize;
    bgScale.z = kBackgroundThickness;

    const Math::Matrix scale = Math::Matrix::CreateScale(bgScale);
    const Math::Matrix trans = Math::Matrix::CreateTranslation(bgCenter);
    mWorld = scale * trans;
}

bool DeadZoneManager::IsInDeadZone(const Math::Vector3& _pos) const
{
    for (const auto& zone : m_zones)
    {
        if (!zone.bEnabled) { continue; }

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

void DeadZoneManager::DrawUnLit() const
{
    auto& shader = KdShaderManager::Instance().m_StandardShader;

    KdShaderManager::Instance().ChangeBlendState(KdBlendState::Alpha);

    for (const auto& zone : m_zones)
    {
        if (!zone.bEnabled || !zone.modelWork || !zone.modelWork->GetData()) { continue; }

        // translucent red
        const Math::Color bgColor = { 1.0f, 0.15f, 0.15f, 0.3f };
        shader.DrawModel(*zone.modelWork, zone.mWorld, bgColor);
    }

    KdShaderManager::Instance().UndoBlendState();
}

void DeadZoneManager::DrawDebugShapes() const
{
    for (int i = 0; i < static_cast<int>(m_zones.size()); ++i)
    {
        const auto& zone = m_zones[i];
        if (!zone.bEnabled) { continue; }

        const bool selected = (i == m_selectedIndex);
        const Math::Color color = selected
            ? Math::Color(1.0f, 1.0f, 0.0f, 0.9f)    // selected: yellow
            : Math::Color(1.0f, 0.0f, 0.0f, 0.9f);   // dead zone: red

        const float z = zone.Center.z;
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

void DeadZoneManager::DrawGui()
{
    if (!ImGui::Begin("Dead Zones", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "Dead Zone Editor (instant kill, red)");
    ImGui::Separator();

    if (ImGui::Button("Add Zone"))
    {
        DeadZone newZone;
        newZone.Center = { 0.0f, 0.0f, 0.0f };
        newZone.HalfExtents = { 20.0f, 20.0f, 5.0f };

        newZone.modelWork = std::make_shared<KdModelWork>();
        const auto spModel = ModelManager::Instance().GetModel("Asset/Data/Box.gltf");
        if (spModel) { newZone.modelWork->SetModelData(spModel); }
        newZone.UpdateWorld();

        m_zones.push_back(std::move(newZone));
        m_selectedIndex = static_cast<int>(m_zones.size()) - 1;
    }

    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_zones.size()); ++i)
    {
        char label[64];
        sprintf_s(label, "DeadZone %d", i);
        if (ImGui::Selectable(label, m_selectedIndex == i)) { m_selectedIndex = i; }
    }

    ImGui::Separator();

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size()))
    {
        auto& zone = m_zones[m_selectedIndex];

        ImGui::Text("DeadZone %d Settings:", m_selectedIndex);

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

        if (ImGui::Checkbox("Enabled", &zone.bEnabled)) { zone.UpdateWorld(); }

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

void DeadZoneManager::Save() const
{
    std::ofstream ofs(StageManager::Instance().ResolvePath(kSaveFileName));
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

void DeadZoneManager::Load()
{
    std::ifstream ifs(StageManager::Instance().ResolvePath(kSaveFileName));

    // Clear first so a new stage with no file becomes empty (no carry-over).
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

        DeadZone zone;
        zone.Center.x = std::stof(tokens[0]);
        zone.Center.y = std::stof(tokens[1]);
        zone.Center.z = std::stof(tokens[2]);
        zone.HalfExtents.x = std::stof(tokens[3]);
        zone.HalfExtents.y = std::stof(tokens[4]);
        zone.HalfExtents.z = std::stof(tokens[5]);
        zone.bEnabled = (std::stoi(tokens[6]) != 0);

        zone.modelWork = std::make_shared<KdModelWork>();
        const auto spModel = ModelManager::Instance().GetModel("Asset/Data/Box.gltf");
        if (spModel) { zone.modelWork->SetModelData(spModel); }
        zone.UpdateWorld();

        m_zones.push_back(zone);
    }
}
