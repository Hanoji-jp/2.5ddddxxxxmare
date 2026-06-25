#include "../../Pch.h"
#include "ShowcaseCamEditor.h"
#include "../Manager/StageManager.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include <fstream>
#include <sstream>

//==========================================================
// Catmull-Rom spline. u in [0,1] across the whole path (pts.size()-1 spans).
//==========================================================
Math::Vector3 ShowcaseCamEditor::EvalSpline(const std::vector<Math::Vector3>& _pts, float _u)
{
    const int n = static_cast<int>(_pts.size());
    if (n == 0) { return Math::Vector3::Zero; }
    if (n == 1) { return _pts[0]; }

    _u = std::clamp(_u, 0.0f, 1.0f);
    const float fseg = _u * static_cast<float>(n - 1);
    int   seg = static_cast<int>(fseg);
    if (seg > n - 2) { seg = n - 2; }
    const float t = fseg - static_cast<float>(seg);

    auto at = [&](int i) -> const Math::Vector3&
    {
        return _pts[std::clamp(i, 0, n - 1)];
    };
    const Math::Vector3& p0 = at(seg - 1);
    const Math::Vector3& p1 = at(seg);
    const Math::Vector3& p2 = at(seg + 1);
    const Math::Vector3& p3 = at(seg + 2);

    const float t2 = t * t;
    const float t3 = t2 * t;
    return (p1 * 2.0f
        + (p2 - p0) * t
        + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
        + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

bool ShowcaseCamEditor::HasPath() const
{
    for (const auto& ph : m_phases)
    {
        if (ph.eye.size() >= 2) { return true; }
    }
    return false;
}

//==========================================================
// GameScene picking hooks
//==========================================================
void ShowcaseCamEditor::SetEyePoint(int _phase, int _idx, const Math::Vector3& _p)
{
    if (_phase < 0 || _phase >= static_cast<int>(m_phases.size())) { return; }
    auto& e = m_phases[_phase].eye;
    if (_idx < 0 || _idx >= static_cast<int>(e.size())) { return; }
    e[_idx] = _p;
    m_dirty = true;
}

void ShowcaseCamEditor::SetLookPoint(int _phase, int _idx, const Math::Vector3& _p)
{
    if (_phase < 0 || _phase >= static_cast<int>(m_phases.size())) { return; }
    auto& l = m_phases[_phase].look;
    if (_idx < 0 || _idx >= static_cast<int>(l.size())) { return; }
    l[_idx] = _p;
    m_dirty = true;
}

void ShowcaseCamEditor::SelectPoint(int _phase, int _idx, bool _isLook)
{
    m_selPhase  = _phase;
    m_selPoint  = _idx;
    m_selIsLook = _isLook;
}

//==========================================================
// ImGui
//==========================================================
void ShowcaseCamEditor::DrawGui()
{
    if (!ImGui::Begin(U8("見せカメラ")))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button(U8("フェーズ追加")))
    {
        m_phases.push_back(Phase{});
        m_selPhase = static_cast<int>(m_phases.size()) - 1;
        m_selPoint = -1;
        m_dirty = true;
    }
    ImGui::SameLine();
    ImGui::Text(U8("(カメラフェーズ順 = 上から下)"));

    for (int i = 0; i < static_cast<int>(m_phases.size()); ++i)
    {
        ImGui::PushID(i);

        // Put action buttons first (a full-width Selectable first would eat the buttons)
        bool deleted = false;
        if (ImGui::SmallButton("X"))
        {
            m_phases.erase(m_phases.begin() + i);
            if (m_selPhase == i)      { m_selPhase = -1; m_selPoint = -1; }
            else if (m_selPhase > i)  { --m_selPhase; }
            m_dirty = true;
            deleted = true;
        }
        if (!deleted)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(U8("上")) && i > 0)
            {
                std::swap(m_phases[i], m_phases[i - 1]); m_selPhase = i - 1; m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(U8("下")) && i < static_cast<int>(m_phases.size()) - 1)
            {
                std::swap(m_phases[i], m_phases[i + 1]); m_selPhase = i + 1; m_dirty = true;
            }
            ImGui::SameLine();

            char label[64];
            std::snprintf(label, sizeof(label), "Phase %d  (%.1fs, eye %d / look %d)",
                i + 1, m_phases[i].duration,
                static_cast<int>(m_phases[i].eye.size()),
                static_cast<int>(m_phases[i].look.size()));
            if (ImGui::Selectable(label, m_selPhase == i)) { m_selPhase = i; m_selPoint = -1; }
        }

        ImGui::PopID();
        if (deleted) { break; }
    }

    ImGui::Separator();

    if (m_selPhase >= 0 && m_selPhase < static_cast<int>(m_phases.size()))
    {
        Phase& ph = m_phases[m_selPhase];
        ImGui::Text(U8("フェーズ %d"), m_selPhase + 1);

        if (ImGui::DragFloat(U8("長さ(秒)"), &ph.duration, 0.05f, ShowcaseCamConst::MinDuration, 60.0f))
        {
            m_dirty = true;
        }
        if (ImGui::DragFloat(U8("速さ(倍)"), &ph.speed, 0.02f, 0.1f, 10.0f))
        {
            m_dirty = true;
        }
        if (ImGui::DragFloat(U8("終端保持(秒)"), &ph.hold, 0.05f, 0.0f, 30.0f))
        {
            m_dirty = true;
        }
        if (ImGui::Checkbox(U8("イーズイン"), &ph.easeIn))   { m_dirty = true; }
        ImGui::SameLine();
        if (ImGui::Checkbox(U8("イーズアウト"), &ph.easeOut)) { m_dirty = true; }

        // eye points
        ImGui::Separator();
        ImGui::Text(U8("視点（カメラ経路）"));
        if (ImGui::Button(U8("視点追加")))
        {
            Math::Vector3 base = ph.eye.empty()
                ? Math::Vector3(0.0f, ShowcaseCamConst::DefaultEyeY, 0.0f)
                : ph.eye.back() + Math::Vector3(2.0f, 0.0f, 0.0f);
            ph.eye.push_back(base);
            m_selPoint = static_cast<int>(ph.eye.size()) - 1; m_selIsLook = false; m_dirty = true;
        }
        for (int i = 0; i < static_cast<int>(ph.eye.size()); ++i)
        {
            ImGui::PushID(1000 + i);
            const bool sel = (!m_selIsLook && m_selPoint == i);
            char lb[48]; std::snprintf(lb, sizeof(lb), "E%d", i);
            if (ImGui::Selectable(lb, sel, 0, ImVec2(40, 0))) { m_selPoint = i; m_selIsLook = false; }
            ImGui::SameLine();
            float p[3] = { ph.eye[i].x, ph.eye[i].y, ph.eye[i].z };
            if (ImGui::DragFloat3("##e", p, 0.1f)) { ph.eye[i] = { p[0], p[1], p[2] }; m_dirty = true; }
            ImGui::PopID();
        }

        // look points
        ImGui::Separator();
        ImGui::Text(U8("注視点（狙い経路）"));
        if (ImGui::Button(U8("注視点追加")))
        {
            Math::Vector3 base = ph.look.empty()
                ? Math::Vector3(0.0f, ShowcaseCamConst::DefaultLookY, 0.0f)
                : ph.look.back() + Math::Vector3(2.0f, 0.0f, 0.0f);
            ph.look.push_back(base);
            m_selPoint = static_cast<int>(ph.look.size()) - 1; m_selIsLook = true; m_dirty = true;
        }
        for (int i = 0; i < static_cast<int>(ph.look.size()); ++i)
        {
            ImGui::PushID(2000 + i);
            const bool sel = (m_selIsLook && m_selPoint == i);
            char lb[48]; std::snprintf(lb, sizeof(lb), "L%d", i);
            if (ImGui::Selectable(lb, sel, 0, ImVec2(40, 0))) { m_selPoint = i; m_selIsLook = true; }
            ImGui::SameLine();
            float p[3] = { ph.look[i].x, ph.look[i].y, ph.look[i].z };
            if (ImGui::DragFloat3("##l", p, 0.1f)) { ph.look[i] = { p[0], p[1], p[2] }; m_dirty = true; }
            ImGui::PopID();
        }

        if (m_selPoint >= 0 && ImGui::Button(U8("選択点を削除")))
        {
            auto& v = m_selIsLook ? ph.look : ph.eye;
            if (m_selPoint < static_cast<int>(v.size())) { v.erase(v.begin() + m_selPoint); }
            m_selPoint = -1; m_dirty = true;
        }
    }

    ImGui::Separator();
    if (ImGui::Button(U8("保存"))) { Save(); }
    ImGui::SameLine();
    if (ImGui::Button(U8("読込"))) { Load(); }

    ImGui::End();
}

//==========================================================
// Editor ghost: spline wires + control point spheres
//==========================================================
void ShowcaseCamEditor::DrawDebug() const
{
    using namespace ShowcaseCamConst;
    const Math::Color eyeCol (EyeColR, EyeColG, EyeColB, 1.0f);
    const Math::Color lookCol(LookColR, LookColG, LookColB, 1.0f);
    const Math::Color linkCol(LinkColR, LinkColG, LinkColB, 1.0f);
    const Math::Color selCol (1.0f, 1.0f, 0.0f, 1.0f);

    KdDebugWireFrame wire;

    for (int pi = 0; pi < static_cast<int>(m_phases.size()); ++pi)
    {
        const Phase& ph = m_phases[pi];

        // eye spline
        if (ph.eye.size() >= 2)
        {
            const int segs = (static_cast<int>(ph.eye.size()) - 1) * WireSegsPerSpan;
            Math::Vector3 prev = EvalSpline(ph.eye, 0.0f);
            for (int s = 1; s <= segs; ++s)
            {
                const float u = static_cast<float>(s) / static_cast<float>(segs);
                const Math::Vector3 cur = EvalSpline(ph.eye, u);
                wire.AddDebugLine(prev, cur, eyeCol);
                // eye -> look links
                if (ph.look.size() >= 1 && (s % LinkEveryNthSample) == 0)
                {
                    wire.AddDebugLine(cur, EvalSpline(ph.look, u), linkCol);
                }
                prev = cur;
            }
        }
        // look spline
        if (ph.look.size() >= 2)
        {
            const int segs = (static_cast<int>(ph.look.size()) - 1) * WireSegsPerSpan;
            Math::Vector3 prev = EvalSpline(ph.look, 0.0f);
            for (int s = 1; s <= segs; ++s)
            {
                const float u = static_cast<float>(s) / static_cast<float>(segs);
                const Math::Vector3 cur = EvalSpline(ph.look, u);
                wire.AddDebugLine(prev, cur, lookCol);
                prev = cur;
            }
        }

        // control point ghosts
        for (int i = 0; i < static_cast<int>(ph.eye.size()); ++i)
        {
            const bool sel = (pi == m_selPhase && !m_selIsLook && i == m_selPoint);
            wire.AddDebugSphere(ph.eye[i], PointRadius, sel ? selCol : eyeCol);
        }
        for (int i = 0; i < static_cast<int>(ph.look.size()); ++i)
        {
            const bool sel = (pi == m_selPhase && m_selIsLook && i == m_selPoint);
            wire.AddDebugSphere(ph.look[i], PointRadius, sel ? selCol : lookCol);
        }
    }

    wire.Draw();
}

//==========================================================
// CSV (per stage). Lines:
//   P,duration,easeIn,easeOut   -> starts a new phase
//   E,x,y,z                     -> eye point of the current phase
//   L,x,y,z                     -> look point of the current phase
//==========================================================
void ShowcaseCamEditor::Save() const
{
    std::ofstream ofs(StageManager::Instance().ResolvePath(ShowcaseCamConst::SaveName));
    if (!ofs) { return; }

    for (const auto& ph : m_phases)
    {
        ofs << "P," << ph.duration << "," << (ph.easeIn ? 1 : 0) << "," << (ph.easeOut ? 1 : 0)
            << "," << ph.speed << "," << ph.hold << "\n";
        for (const auto& p : ph.eye)  { ofs << "E," << p.x << "," << p.y << "," << p.z << "\n"; }
        for (const auto& p : ph.look) { ofs << "L," << p.x << "," << p.y << "," << p.z << "\n"; }
    }
}

void ShowcaseCamEditor::Load()
{
    std::ifstream ifs(StageManager::Instance().ResolvePath(ShowcaseCamConst::SaveName));
    m_phases.clear();
    m_selPhase = -1; m_selPoint = -1;
    if (!ifs) { return; }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty()) { continue; }
        std::istringstream ss(line);
        std::string tag;
        if (!std::getline(ss, tag, ',')) { continue; }

        auto readVec = [&](Math::Vector3& out) -> bool
        {
            std::string a, b, c;
            if (!std::getline(ss, a, ',')) { return false; }
            if (!std::getline(ss, b, ',')) { return false; }
            if (!std::getline(ss, c, ',')) { return false; }
            out = { std::stof(a), std::stof(b), std::stof(c) };
            return true;
        };

        if (tag == "P")
        {
            Phase ph;
            std::string d, ei, eo, sp, hd;
            if (std::getline(ss, d, ','))  { ph.duration = std::stof(d); }
            if (std::getline(ss, ei, ',')) { ph.easeIn  = (std::stoi(ei) != 0); }
            if (std::getline(ss, eo, ',')) { ph.easeOut = (std::stoi(eo) != 0); }
            if (std::getline(ss, sp, ','))  { ph.speed = std::stof(sp); }  // 旧データは無→既定1.0
            if (std::getline(ss, hd, ','))  { ph.hold  = std::stof(hd); }  // 旧データは無→既定0
            m_phases.push_back(ph);
        }
        else if (tag == "E" && !m_phases.empty())
        {
            Math::Vector3 v; if (readVec(v)) { m_phases.back().eye.push_back(v); }
        }
        else if (tag == "L" && !m_phases.empty())
        {
            Math::Vector3 v; if (readVec(v)) { m_phases.back().look.push_back(v); }
        }
    }
    m_dirty = false;
}
