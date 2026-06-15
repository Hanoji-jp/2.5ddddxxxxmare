#pragma once
#include "EffectBase.h"
#include "../../Const/SparkleConst.h"

//==========================================================
// SparkleEffect - reusable component that makes star_01 orbit and
// twinkle around a center position. ItemManager keeps one instance and
// calls DrawAround() for each item (the texture/quad are shared).
//==========================================================
class SparkleEffect
{
public:
    // Prepare texture and quad (first call only)
    void Init()
    {
        if (m_ready) { return; }
        m_tex = std::make_shared<KdTexture>();
        if (!m_tex->Load(SparkleConst::StarTexPath)) { return; }
        m_poly.SetMaterial(m_tex);
        m_poly.SetScale(1.0f);   // unit size (scaled by 'size' when drawn)
        m_ready = true;
    }

    // Advance animation time (call once per frame)
    void Update(float dt) { m_time += dt; }

    bool IsReady() const { return m_ready; }

    // Draw orbiting/twinkling stars around 'center'
    void DrawAround(const Math::Vector3& center) const
    {
        if (!m_ready) { return; }

        const Math::Vector3 base = center
            + Math::Vector3{ 0.0f, SparkleConst::CenterOffsetY, 0.0f };

        for (int i = 0; i < SparkleConst::Count; ++i)
        {
            const float frac  = static_cast<float>(i) / static_cast<float>(SparkleConst::Count);
            const float phase = DirectX::XM_2PI * frac;

            // life: 0..1 looping per star, offset so stars are staggered
            float life = m_time / SparkleConst::LifeTime + frac;
            life = life - std::floorf(life);

            // rise upward over the lifecycle
            const float rise = life * SparkleConst::RiseHeight;

            // fade in then out (appear -> peak -> disappear)
            const float fade = std::sinf(life * DirectX::XM_PI);

            // gentle horizontal orbit, tightening as it rises
            const float ang    = m_time * SparkleConst::OrbitSpeed + phase;
            const float radius = SparkleConst::OrbitRadius * (1.0f - life * 0.5f);

            const Math::Vector3 pos = base + Math::Vector3{
                std::cosf(ang) * radius,
                rise,
                std::sinf(ang) * radius };

            // twinkle adds a fast flicker on top of the fade
            const float tw      = 0.5f + 0.5f * std::sinf(m_time * SparkleConst::TwinkleSpeed + phase);
            const float sizeMul = SparkleConst::TwinkleMin + (1.0f - SparkleConst::TwinkleMin) * tw;
            const float size    = SparkleConst::BaseSize * sizeMul * fade;   // pop in/out with fade
            const float spin    = m_time * SparkleConst::SpinSpeed + phase;

            const float a = fade;
            const Math::Color   col{ 1.0f, 1.0f, 1.0f, a };
            const Math::Vector3 emissive{ a, a, a };

            EffectBase::DrawBillboard(m_poly, pos, size, spin, col, emissive);
        }
    }

private:
    KdSquarePolygon            m_poly;
    std::shared_ptr<KdTexture> m_tex;
    float                      m_time  = 0.0f;
    bool                       m_ready = false;
};
