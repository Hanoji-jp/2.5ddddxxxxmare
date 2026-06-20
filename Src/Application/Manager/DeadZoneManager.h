#pragma once
#include "../../Framework/Direct3D/KdModel.h"

//==========================================================
// DeadZone
// Kill zone (AABB). Player dies instantly when inside. Drawn in red.
//==========================================================
struct DeadZone
{
    Math::Vector3 Center      = { 0.0f, 0.0f, 0.0f };
    Math::Vector3 HalfExtents = { 10.0f, 10.0f, 10.0f };
    bool bEnabled = true;

    // Drawing
    std::shared_ptr<KdModelWork> modelWork;
    Math::Matrix mWorld = Math::Matrix::Identity;

    void UpdateWorld();
};

//==========================================================
// DeadZoneManager
// Manages dead zones and tests whether the player is inside one.
// Mirrors ManualGravityZoneManager (editor + CSV + AABB).
//==========================================================
class DeadZoneManager
{
public:
    static DeadZoneManager& Instance()
    {
        static DeadZoneManager s;
        return s;
    }

    // Returns true if the position is inside any enabled dead zone.
    bool IsInDeadZone(const Math::Vector3& _pos) const;

    void   AddZone(const DeadZone& _zone) { m_zones.push_back(_zone); }
    void   ClearZones() { m_zones.clear(); }
    size_t GetZoneCount() const { return m_zones.size(); }

    // Editor: set the currently selected index.
    void SetSelected(int _i) { m_selectedIndex = _i; }

    // Editor: remove the last zone (used to undo a create).
    void RemoveLastZone() { if (!m_zones.empty()) { m_zones.pop_back(); } }

    DeadZone* GetZone(int _index)
    {
        if (_index < 0 || _index >= static_cast<int>(m_zones.size())) { return nullptr; }
        return &m_zones[_index];
    }

    // Debug wireframe (always shown)
    void DrawDebugShapes() const;

    // Translucent red background box
    void DrawUnLit() const;

    // ImGui editor
    void DrawGui();

    // Save / load CSV
    void Save() const;
    void Load();

private:
    DeadZoneManager() = default;

    std::vector<DeadZone> m_zones;
    int m_selectedIndex = -1;
};
