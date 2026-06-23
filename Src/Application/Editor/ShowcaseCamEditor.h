#pragma once
#include "../Const/ShowcaseCamConst.h"
// ShowcaseCamEditor  (ASCII comments only: this file is BOM-less)
//==========================================================
// Authors the stage-showcase camera as an ordered list of phases.
// Each phase = a spline path of camera (eye) control points + a matching
// spline of look-at control points, plus a duration and ease-in/out flags.
// Edited via ImGui; points are also pickable/draggable in the 3D editor
// (GameScene feeds them into its edit-entry system). Saved per stage as CSV.
//==========================================================
class ShowcaseCamEditor
{
public:
    struct Phase
    {
        float duration = ShowcaseCamConst::DefaultDuration; // seconds (base length)
        float speed    = 1.0f;   // speed multiplier (higher = faster; effective move time = duration/speed)
        float hold     = 0.0f;   // extra seconds held at the end pose before the next phase
        bool  easeIn   = true;   // ease-in (start slow)
        bool  easeOut  = true;   // ease-out (stop softly)
        std::vector<Math::Vector3> eye;    // camera position control points
        std::vector<Math::Vector3> look;   // look-at control points
    };

    ShowcaseCamEditor()  { Load(); }
    ~ShowcaseCamEditor() {}

    void DrawGui();
    void DrawDebug() const;   // editor ghost: spline wires + control points

    void Save() const;
    void Load();

    const std::vector<Phase>& GetPhases() const { return m_phases; }
    // true if there is at least one playable phase (>= 2 eye points)
    bool HasPath() const;

    bool IsDirty()  const { return m_dirty; }
    void ClearDirty()     { m_dirty = false; }
    void MarkDirty()      { m_dirty = true; }

    // ---- GameScene picking hooks (drag a control point in 3D) ----
    void SetEyePoint(int _phase, int _idx, const Math::Vector3& _p);
    void SetLookPoint(int _phase, int _idx, const Math::Vector3& _p);
    void SelectPoint(int _phase, int _idx, bool _isLook);

    // ---- Catmull-Rom spline evaluation (u in [0,1] across the whole path) ----
    static Math::Vector3 EvalSpline(const std::vector<Math::Vector3>& _pts, float _u);

private:
    std::vector<Phase> m_phases;   // order = index = camerafaze1,2,3...
    int  m_selPhase  = -1;
    int  m_selPoint  = -1;
    bool m_selIsLook = false;
    bool m_dirty     = false;
};
