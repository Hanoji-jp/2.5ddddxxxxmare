#include "../../Pch.h"
#include "RoomBoundsEditor.h"
#include "../Manager/StageManager.h"
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

        // minX : 青
        {
            const Math::Color col = isSelected
                ? Math::Color(0.3f, 0.6f, 1.0f, 1.0f)
                : Math::Color(0.0f, 0.3f, 1.0f, 0.8f);
            wire.AddDebugLine(
                { r.minX, kLineBottom, kLineZ },
                { r.minX, kLineTop,    kLineZ },
                col);
        }

        // maxX : 赤
        {
            const Math::Color col = isSelected
                ? Math::Color(1.0f, 0.5f, 0.5f, 1.0f)
                : Math::Color(1.0f, 0.1f, 0.1f, 0.8f);
            wire.AddDebugLine(
                { r.maxX, kLineBottom, kLineZ },
                { r.maxX, kLineTop,    kLineZ },
                col);
        }

        // triggerX : 黄
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

        // minY / maxY : 緑の水平線（X範囲内に引く）
        {
            const float lineXA = (r.minX > -FLT_MAX * 0.5f) ? r.minX : -50.0f;
            const float lineXB = (r.maxX <  FLT_MAX * 0.5f) ? r.maxX :  50.0f;

            if (r.minY > -FLT_MAX * 0.5f)
            {
                const Math::Color col = isSelected
                    ? Math::Color(0.3f, 1.0f, 0.5f, 1.0f)
                    : Math::Color(0.0f, 0.8f, 0.2f, 0.8f);
                wire.AddDebugLine({ lineXA, r.minY, kLineZ }, { lineXB, r.minY, kLineZ }, col);
            }
            if (r.maxY < FLT_MAX * 0.5f)
            {
                const Math::Color col = isSelected
                    ? Math::Color(0.3f, 1.0f, 0.5f, 1.0f)
                    : Math::Color(0.0f, 0.8f, 0.2f, 0.8f);
                wire.AddDebugLine({ lineXA, r.maxY, kLineZ }, { lineXB, r.maxY, kLineZ }, col);
            }
        }

        // cameraZ : マジェンタの縦線（X範囲内）
        {
            const float lineXA = (r.minX > -FLT_MAX * 0.5f) ? r.minX : -50.0f;
            const float lineXB = (r.maxX <  FLT_MAX * 0.5f) ? r.maxX :  50.0f;
            const Math::Color col = isSelected
                ? Math::Color(1.0f, 0.5f, 1.0f, 1.0f)
                : Math::Color(0.8f, 0.2f, 0.8f, 0.8f);
            wire.AddDebugLine({ lineXA, kLineBottom, r.cameraZ }, { lineXA, kLineTop, r.cameraZ }, col);
            wire.AddDebugLine({ lineXB, kLineBottom, r.cameraZ }, { lineXB, kLineTop, r.cameraZ }, col);
            wire.AddDebugLine({ lineXA, kLineBottom, r.cameraZ }, { lineXB, kLineBottom, r.cameraZ }, col);
            wire.AddDebugLine({ lineXA, kLineTop,    r.cameraZ }, { lineXB, kLineTop,    r.cameraZ }, col);
        }
    }

    wire.Draw();
}


//  書式: minX,maxX,minY,maxY,triggerX,blendX
//----------------------------------------------------------
void RoomBoundsEditor::Save() const
{
    std::ofstream ofs(StageManager::Instance().ResolvePath("rooms.csv"));
    if (!ofs) { return; }

    // ヘッダ行
    ofs << "minX,maxX,minY,maxY,triggerX,blendX,cameraZ,mode,focusX,focusY,focusZ,focusLerp,useOffOvr,ovrX,ovrY,ovrZ\n";

    for (const auto& r : m_rooms)
    {
        ofs << r.minX    << ","
            << r.maxX    << ","
            << r.minY    << ","
            << r.maxY    << ","
            << r.triggerX << ","
            << r.blendX  << ","
            << r.cameraZ << ","
            << static_cast<int>(r.mode) << ","
            << r.focusOffset.x << ","
            << r.focusOffset.y << ","
            << r.focusOffset.z << ","
            << r.focusLerpSpeed << ","
            << (r.useOffsetOverride ? 1 : 0) << ","
            << r.overrideOffsetX << ","
            << r.overrideOffsetY << ","
            << r.overrideOffsetZ << "\n";
    }
}

//----------------------------------------------------------
// CSV 読込
//----------------------------------------------------------
void RoomBoundsEditor::Load()
{
    std::ifstream ifs(StageManager::Instance().ResolvePath("rooms.csv"));

    // 新ステージのファイルが無くても空にする（前ステージのルーム＝カメラY制限を残さない）
    m_rooms.clear();
    if (!ifs) { return; }

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
        // cameraZ（旧CSVには無い列 → デフォルト 0.0f）
        float cameraZVal = 0.0f;
        if (nextFloat(cameraZVal)) { r.cameraZ = cameraZVal; }
        // mode（旧CSVには無い列 → デフォルト SideScroll）
        float modeVal = 0.0f;
        if (nextFloat(modeVal))
        {
            r.mode = static_cast<CameraConst::CameraMode>(static_cast<int>(modeVal));
        }
        // focusOffset / focusLerpSpeed（旧CSVには無い列 → デフォルト 0.0f）
        float fx = 0.0f, fy = 0.0f, fz = 0.0f, fl = 0.0f;
        if (nextFloat(fx)) { r.focusOffset.x = fx; }
        if (nextFloat(fy)) { r.focusOffset.y = fy; }
        if (nextFloat(fz)) { r.focusOffset.z = fz; }
        if (nextFloat(fl)) { r.focusLerpSpeed = fl; }
        // useOffsetOverride / overrideOffset（旧CSVには無い列 → デフォルト false / 0.0f）
        float useOvr = 0.0f, ox = 0.0f, oy = 3.0f, oz = -14.0f;
        if (nextFloat(useOvr)) { r.useOffsetOverride = (useOvr > 0.5f); }
        if (nextFloat(ox)) { r.overrideOffsetX = ox; }
        if (nextFloat(oy)) { r.overrideOffsetY = oy; }
        if (nextFloat(oz)) { r.overrideOffsetZ = oz; }

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

        // Z 基準位置
        ImGui::Text("Camera Z Position");
        changed |= ImGui::DragFloat("cameraZ",  &r.cameraZ, 0.1f);

        ImGui::Separator();

        // ルーム遷移
        ImGui::Text("Transition");
        changed |= ImGui::DragFloat("triggerX", &r.triggerX, 0.1f);
        changed |= ImGui::DragFloat("blendX",   &r.blendX,   0.1f, 0.0f, 50.0f);

        if (changed) { m_dirty = true; }

        // カメラモード
        ImGui::Separator();
        ImGui::Text("Camera Mode");
        {
            static constexpr const char* kModeNames[] = {
                "SideScroll (2.5D)",
                "Fixed2D (純2D固定)",
                "TopDown (俯瞰)",
            };
            int modeIdx = static_cast<int>(r.mode);
            if (ImGui::Combo("Mode##camMode", &modeIdx, kModeNames, 3))
            {
                r.mode  = static_cast<CameraConst::CameraMode>(modeIdx);
                changed = true;
            }
        }

        // フォーカスオフセット
        ImGui::Separator();
        ImGui::Text("Focus Offset (Gravity Local)");
        ImGui::TextDisabled("X=Right  Y=Up(along gravity)  Z=Forward");
        changed |= ImGui::DragFloat3("focusOffset", &r.focusOffset.x, 0.05f, -50.0f, 50.0f);

        // Basic Offset オーバーライド
        ImGui::Separator();
        ImGui::Text("Basic Offset Override");
        ImGui::TextDisabled("ON にするとこのルームだけ独自の XYZ オフセットを使用");
        changed |= ImGui::Checkbox("useOffsetOverride", &r.useOffsetOverride);
        if (r.useOffsetOverride)
        {
            changed |= ImGui::DragFloat("overrideOffsetX", &r.overrideOffsetX, 0.05f, -50.0f, 50.0f);
            changed |= ImGui::DragFloat("overrideOffsetY", &r.overrideOffsetY, 0.05f, -50.0f, 50.0f);
            changed |= ImGui::DragFloat("overrideOffsetZ", &r.overrideOffsetZ, 0.1f, -100.0f, -1.0f);
            if (ImGui::Button("Reset##offsetOverride"))
            {
                r.overrideOffsetX = 0.0f;
                r.overrideOffsetY = 3.0f;
                r.overrideOffsetZ = -14.0f;
                changed = true;
            }
        }

        // focusLerpSpeed: 0.0f のときデフォルト速度を使用
        ImGui::TextDisabled("focusLerp: 0=use default (%.3f)", CameraConst::FocusOffsetLerp);
        changed |= ImGui::DragFloat("focusLerpSpeed", &r.focusLerpSpeed, 0.005f, 0.0f, 1.0f);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("0.0 = デフォルト速度を使用\n大きいほどカメラが素早く追従");
        }

        // リセットボタン
        ImGui::SameLine();
        if (ImGui::Button("Reset##focus"))
        {
            r.focusOffset    = { 0.0f, 0.0f, 0.0f };
            r.focusLerpSpeed = 0.0f;
            changed = true;
        }

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
