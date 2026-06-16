#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"

//==========================================================
// KdStretchPoly - a quad whose 4 corners are set directly,
// so we can make tapered / trapezoid beams (base width != tip width).
//==========================================================
class KdStretchPoly : public KdPolygon
{
public:
    KdStretchPoly() { m_vertices.resize(4); }

    // Local-space trapezoid: length along +Y, base at -Y (V=0), tip at +Y (V=1)
    void SetTrapezoid(float length, float baseWidth, float tipWidth)
    {
        if (m_vertices.size() < 4) { m_vertices.resize(4); }
        const float hl = length * 0.5f;
        auto set = [&](int i, float x, float y, float u, float v)
        {
            m_vertices[i].pos     = { x, y, 0.0f };
            m_vertices[i].UV      = { u, v };
            m_vertices[i].color   = 0xFFFFFFFF;
            m_vertices[i].normal  = Math::Vector3::Backward;
            m_vertices[i].tangent = Math::Vector3::Right;
        };
        // Texture runs horizontally: length = U (base U=0, tip U=1), width = V.
        set(0, -baseWidth * 0.5f, -hl, 0.0f, 0.0f);  // base-left  (U=0,V=0)
        set(1, -tipWidth  * 0.5f, +hl, 1.0f, 0.0f);  // tip-left   (U=1,V=0)
        set(2, +baseWidth * 0.5f, -hl, 0.0f, 1.0f);  // base-right (U=0,V=1)
        set(3, +tipWidth  * 0.5f, +hl, 1.0f, 1.0f);  // tip-right  (U=1,V=1)
    }
};

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

    // Radial light beam: a camera-facing quad stretched along an in-plane angle.
    //  center  : inner end of the beam (beam extends outward from here)
    //  angleRad: in-plane direction of the beam
    //  length  : beam length, width: beam thickness
    static void DrawBeam(const KdPolygon&     poly,
                         const Math::Vector3& center,
                         float                angleRad,
                         float                length,
                         float                width,
                         const Math::Color&   col,
                         const Math::Vector3& emissive = Math::Vector3::Zero)
    {
        const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
        Math::Vector3 toCam = camPos - center;
        if (toCam.LengthSquared() < 1e-6f) { toCam = Math::Vector3::Backward; }
        toCam.Normalize();

        Math::Vector3 up    = Math::Vector3::Up;
        Math::Vector3 right = up.Cross(toCam);
        if (right.LengthSquared() < 1e-6f) { right = Math::Vector3::Right; }
        right.Normalize();
        up = toCam.Cross(right);
        up.Normalize();

        const float c = std::cosf(angleRad);
        const float s = std::sinf(angleRad);
        const Math::Vector3 longAxis = up * c - right * s;     // beam direction (local Y)
        const Math::Vector3 perpAxis = right * c + up * s;     // beam thickness  (local X)

        // place the quad so its inner edge sits at 'center'
        const Math::Vector3 beamCenter = center + longAxis * (length * 0.5f);

        Math::Matrix w = Math::Matrix::Identity;
        w._11 = perpAxis.x * width;  w._12 = perpAxis.y * width;  w._13 = perpAxis.z * width;
        w._21 = longAxis.x * length; w._22 = longAxis.y * length; w._23 = longAxis.z * length;
        w._31 = toCam.x;             w._32 = toCam.y;             w._33 = toCam.z;
        w._41 = beamCenter.x;        w._42 = beamCenter.y;        w._43 = beamCenter.z;

        KdShaderManager::Instance().m_StandardShader.DrawPolygon(poly, w, col, emissive);
    }

    // Radial light beam with separate base/tip width (tapered or flared toward the tip).
    static void DrawBeamTrapezoid(KdStretchPoly&       poly,
                                  const Math::Vector3& center,
                                  float                angleRad,
                                  float                length,
                                  float                baseWidth,
                                  float                tipWidth,
                                  const Math::Color&   col,
                                  const Math::Vector3& emissive = Math::Vector3::Zero)
    {
        const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
        Math::Vector3 toCam = camPos - center;
        if (toCam.LengthSquared() < 1e-6f) { toCam = Math::Vector3::Backward; }
        toCam.Normalize();

        Math::Vector3 up    = Math::Vector3::Up;
        Math::Vector3 right = up.Cross(toCam);
        if (right.LengthSquared() < 1e-6f) { right = Math::Vector3::Right; }
        right.Normalize();
        up = toCam.Cross(right);
        up.Normalize();

        const float c = std::cosf(angleRad);
        const float s = std::sinf(angleRad);
        const Math::Vector3 longAxis = up * c - right * s;     // local +Y
        const Math::Vector3 perpAxis = right * c + up * s;     // local +X
        const Math::Vector3 beamCenter = center + longAxis * (length * 0.5f);

        poly.SetTrapezoid(length, baseWidth, tipWidth);

        // unit-basis matrix (geometry already carries the real size)
        Math::Matrix w = Math::Matrix::Identity;
        w._11 = perpAxis.x; w._12 = perpAxis.y; w._13 = perpAxis.z;
        w._21 = longAxis.x; w._22 = longAxis.y; w._23 = longAxis.z;
        w._31 = toCam.x;    w._32 = toCam.y;    w._33 = toCam.z;
        w._41 = beamCenter.x; w._42 = beamCenter.y; w._43 = beamCenter.z;

        KdShaderManager::Instance().m_StandardShader.DrawPolygon(poly, w, col, emissive);
    }

protected:
    Math::Vector3 m_pos = {};
    float         m_age = 0.0f;   // elapsed time (for derived effects)
};
