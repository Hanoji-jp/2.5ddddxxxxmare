#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"

//==========================================================
// EffectBase - template base for custom (non-Effekseer) effects.
//
// Derive from this for effects you control in code (things Effekseer
// cannot do). Implement Update()/DrawEffect() in the derived class.
// DrawBillboard() renders a texture on a camera-facing quad.
//==========================================================
class EffectBase : public KdGameObject
{
public:
    void          SetPos(const Math::Vector3& p) override { m_pos = p; m_mWorld = Math::Matrix::CreateTranslation(p); }
    Math::Vector3 GetPos() const                 override { return m_pos; }
    bool          IsVisible() const              override { return true; }

    // Draw a camera-facing billboard quad with a texture.
    //  poly     : unit-size KdSquarePolygon with a material/texture (SetScale(1.0f))
    //  worldPos : center in world space
    //  size     : world size
    //  spinRad  : in-plane rotation (radians)
    //  col      : multiply color (alpha included)
    //  emissive : self illumination (use to glow)
    static void DrawBillboard(const KdPolygon&     poly,
                              const Math::Vector3& worldPos,
                              float                size,
                              float                spinRad,
                              const Math::Color&   col,
                              const Math::Vector3& emissive = Math::Vector3::Zero)
    {
        // Axis toward the camera
        const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
        Math::Vector3 toCam = camPos - worldPos;
        if (toCam.LengthSquared() < 1e-6f) { toCam = Math::Vector3::Backward; }
        toCam.Normalize();

        // Build right/up from world up (spherical billboard)
        Math::Vector3 up    = Math::Vector3::Up;
        Math::Vector3 right = up.Cross(toCam);
        if (right.LengthSquared() < 1e-6f) { right = Math::Vector3::Right; }
        right.Normalize();
        up = toCam.Cross(right);
        up.Normalize();

        // In-plane spin
        const float cs = std::cosf(spinRad);
        const float sn = std::sinf(spinRad);
        const Math::Vector3 r2 = right * cs + up * sn;
        const Math::Vector3 u2 = up * cs - right * sn;

        // Map local quad (XY plane, +Z facing) onto r2 / u2 / toCam basis
        Math::Matrix w = Math::Matrix::Identity;
        w._11 = r2.x * size; w._12 = r2.y * size; w._13 = r2.z * size;
        w._21 = u2.x * size; w._22 = u2.y * size; w._23 = u2.z * size;
        w._31 = toCam.x;     w._32 = toCam.y;     w._33 = toCam.z;
        w._41 = worldPos.x;  w._42 = worldPos.y;  w._43 = worldPos.z;

        KdShaderManager::Instance().m_StandardShader.DrawPolygon(poly, w, col, emissive);
    }

protected:
    Math::Vector3 m_pos = {};
    float         m_age = 0.0f;   // elapsed time (for derived effects)
};
