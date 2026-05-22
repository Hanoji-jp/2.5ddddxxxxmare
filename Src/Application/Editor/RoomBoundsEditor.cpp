#include "../../Pch.h"
#include "RoomBoundsEditor.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include <fstream>
#include <sstream>

namespace
{
    // 縦線を描く高さ範囲
    constexpr float kLineBottom = -5.0f;
    constexpr float kLineTop    = 20.0f;
    constexpr float kLineZ      =  0.0f;  // 2.5D なので Z=0 平面上
}

void RoomBoundsEditor::DrawDebugLines() const
{
    KdDebugWireFrame wire;

    for (int i = 0; i < static_cast<int>(m_rooms.size()); ++i)
    {
        const RoomBounds& r = m_rooms[i];

        const bool isSelected = (i == m_selectedIdx);

        // minX : 青（選択中は明るい青）
        {
            const Math::Color col = isSelected
                ? Math::Color(0.3f, 0.6f, 1.0f, 1.0f)
                : Math::Color(0.0f, 0.3f, 1.0f, 0.8f);
            wire.AddDebugLine(
                { r.minX, kLineBottom, kLineZ },
                { r.minX, kLineTop,    kLineZ },
                col);
        }

        // maxX : 赤（選択中は明るい赤）
        {
            const Math::Color col = isSelected
                ? Math::Color(1.0f, 0.5f, 0.5f, 1.0f)
                : Math::Color(1.0f, 0.1f, 0.1f, 0.8f);
            wire.AddDebugLine(
                { r.maxX, kLineBottom, kLineZ },
                { r.maxX, kLineTop,    kLineZ },
                col);
        }

        // triggerX : 黄（FLT_MAX なら描かない）
        if (r.triggerX < FLT_MAX * 0.5f)
        {
            const Math::Color col = isSelected
                ? Math::Color(1.0f, 1.0f, 0.3f, 1.0f)
                : Math::Color(0.9f, 0.9f, 0.0f, 0.8f);
            wire.AddDebugLine(
                { r.triggerX, kLineBottom, kLineZ },
                { r.triggerX, kLineTop,    kLineZ },
                col);
        }
    }

    wire.Draw();
}


//  書式: minX,maxX,minY,maxY,triggerX,blendX
//----------------------------------------------------------
void RoomBoundsEditor::Save() const
{
    std::ofstream ofs(SavePath);
    if (!ofs) { return; }

    // ヘッダ行
    ofs << "minX,maxX,minY,maxY,triggerX,blendX\n";

    for (const auto& r : m_rooms)
    {
        ofs << r.minX    << ","
            << r.maxX    << ","
            << r.minY    << ","
            << r.maxY    << ","
            << r.triggerX << ","
            << r.blendX  << "\n";
    }
}

//----------------------------------------------------------
// CSV 読込
//----------------------------------------------------------
void RoomBoundsEditor::Load()
{
    std::ifstream ifs(SavePath);
    if (!ifs) { return; }

    m_rooms.clear();

    std::string line;
    bool firstLine = true;
    while (std::getline(ifs, line))
    {
        // ヘッダ行をスキップ
        if (firstLine) { firstLine = false; continue; }
        if (line.empty()) { continue; }

        std::istringstream ss(line);
        std::string token;
        RoomBounds r;

        auto nextFloat = [&](float& out) -> bool
        {
            if (!std::getline(ss, token, ',')) { return false; }
            try { out = std::stof(token); return true; }
            catch (...) { return false; }
        };

        if (!nextFloat(r.minX))     { continue; }
        if (!nextFloat(r.maxX))     { continue; }
        if (!nextFloat(r.minY))     { continue; }
        if (!nextFloat(r.maxY))     { continue; }
        if (!nextFloat(r.triggerX)) { continue; }
        if (!nextFloat(r.blendX))   { continue; }

        m_rooms.push_back(r);
    }

    m_dirty = true;
    m_selectedIdx = -1;
}

//----------------------------------------------------------
// ImGui ウィンドウ描画
//----------------------------------------------------------
void RoomBoundsEditor::DrawGui()
{
    if (!ImGui::Begin("Room Bounds Editor"))
    {
        ImGui::End();
        return;
    }

    // --- Save / Load ボタン ---
    if (ImGui::Button("Save CSV"))  { Save(); }
    ImGui::SameLine();
    if (ImGui::Button("Load CSV"))  { Load(); }
    ImGui::SameLine();
    if (ImGui::Button("Add Room"))
    {
        RoomBounds newRoom;
        // 直前のルームの maxX を基準にデフォルト値を設定
        if (!m_rooms.empty())
        {
            const float prevMax    = m_rooms.back().maxX;
            newRoom.minX           = prevMax;
            newRoom.maxX           = prevMax + 20.0f;
            newRoom.triggerX       = prevMax + 19.0f;
        }
        m_rooms.push_back(newRoom);
        m_selectedIdx = static_cast<int>(m_rooms.size()) - 1;
        m_dirty = true;
    }

    ImGui::Separator();

    // --- ルーム一覧 ---
    ImGui::Text("Rooms (%d)", static_cast<int>(m_rooms.size()));

    for (int i = 0; i < static_cast<int>(m_rooms.size()); ++i)
    {
        const bool selected = (m_selectedIdx == i);
        char label[32];
        std::snprintf(label, sizeof(label), "Room %d", i);

        if (ImGui::Selectable(label, selected))
        {
            m_selectedIdx = i;
        }
    }

    ImGui::Separator();

    // --- 選択中ルームのインスペクター ---
    if (m_selectedIdx >= 0 && m_selectedIdx < static_cast<int>(m_rooms.size()))
    {
        RoomBounds& r = m_rooms[m_selectedIdx];
        ImGui::Text("== Room %d ==", m_selectedIdx);

        bool changed = false;

        // X 範囲（ドアで見える範囲を制限）
        ImGui::Text("Camera X Range (door clamp)");
        changed |= ImGui::DragFloat("minX",     &r.minX,    0.1f);
        changed |= ImGui::DragFloat("maxX",     &r.maxX,    0.1f);

        // Y 範囲
        ImGui::Text("Camera Y Range");
        changed |= ImGui::DragFloat("minY",     &r.minY,    0.1f);
        changed |= ImGui::DragFloat("maxY",     &r.maxY,    0.1f);

        ImGui::Separator();

        // ルーム遷移
        ImGui::Text("Transition");
        changed |= ImGui::DragFloat("triggerX", &r.triggerX, 0.1f);
        changed |= ImGui::DragFloat("blendX",   &r.blendX,   0.1f, 0.0f, 50.0f);

        if (changed) { m_dirty = true; }

        // 削除ボタン
        ImGui::Spacing();
        if (ImGui::Button("Delete Room"))
        {
            m_rooms.erase(m_rooms.begin() + m_selectedIdx);
            m_selectedIdx = std::min(m_selectedIdx, static_cast<int>(m_rooms.size()) - 1);
            m_dirty = true;
        }
    }
    else
    {
        ImGui::TextDisabled("(ルームを選択してください)");
    }

    ImGui::End();
}
