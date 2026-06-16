#pragma once
#include "EffectBase.h"
#include "../../Const/SparkleConst.h"

//==========================================================
// PickupBurst - Mario-Galaxy style pickup flourish (CPU, additive).
//  phase 1: light streaks suck inward to the center, then a bright flash
//  phase 2: radial light beams shoot outward
// Drawn additively so it reads as glowing light. Auto-expires.
// Managed by ItemManager; keeps animating during hit-stop.
//==========================================================
class PickupBurst : public EffectBase
{
public:
    void Spawn(const Math::Vector3& pos, const Math::Color& color)
    {
        EnsureAssets();
        m_pos       = pos;
        m_color     = color;
        m_age       = 0.0f;
        m_isExpired = false;

        // 取得ごとに毎回振り直す（位置でなく乱数シード）
        m_seed      = static_cast<float>(std::rand());
        m_beamCount = SparkleConst::PickupBeamCountMin
                    + std::rand() % (SparkleConst::PickupBeamCountMax - SparkleConst::PickupBeamCountMin + 1);
    }

    void Update() override
    {
        if (m_isExpired) { return; }
        m_age += KdFPSController::GetDt();
        m_mWorld = Math::Matrix::CreateTranslation(m_pos);   // for frustum culling
        const float total = SparkleConst::PickupBurstLife + SparkleConst::PickupAfterglowLife;
        if (m_age >= total) { m_isExpired = true; }
    }

    void DrawEffect() override
    {
        if (m_isExpired) { return; }
        Assets& a = GetAssets();   // non-const: beam UV is animated below
        if (!a.ready) { return; }

        KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);
        // depth write OFF: stacked coplanar additive quads must not Z-fight (no jaggies)
        KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
        KdShaderManager::Instance().m_StandardShader.SetDissolve(0.0f);

        // After the main burst (and hit-stop) ends -> lingering afterglow, then return.
        if (m_age > SparkleConst::PickupBurstLife)
        {
            DrawAfterglow();
            KdShaderManager::Instance().UndoDepthStencilState();
            KdShaderManager::Instance().UndoBlendState();
            return;
        }

        const float t     = m_age / SparkleConst::PickupBurstLife;   // 0..1
        const float convT = SparkleConst::PickupConvergeTime;

        // soft glow halo: grows and fades
        {
            const float gs = SparkleConst::PickupGlowStartSize
                + (SparkleConst::PickupGlowEndSize - SparkleConst::PickupGlowStartSize) * t;
            const float ga = (1.0f - t) * SparkleConst::PickupGlowAlpha;
            const Math::Color   col { m_color.x, m_color.y, m_color.z, ga };
            const Math::Vector3 em  { m_color.x * ga, m_color.y * ga, m_color.z * ga };
            DrawBillboard(a.glow, m_pos, gs, m_age * SparkleConst::PickupGlowSpin, col, em);
        }

        // phase 1: converging streaks (suck inward)
        if (t < convT)
        {
            const float cp = t / convT;                          // 0..1 (1 = arrived at center)
            for (int i = 0; i < SparkleConst::PickupConvergeCount; ++i)
            {
                const float ang  = DirectX::XM_2PI * static_cast<float>(i)
                                 / static_cast<float>(SparkleConst::PickupConvergeCount)
                                 + Hash(m_seed + i) * 1.5f;
                const float dist = SparkleConst::PickupConvergeRadius * (1.0f - cp);
                const Math::Vector3 dir{ std::cosf(ang),
                                         std::sinf(ang * 1.3f) * 0.4f,
                                         std::sinf(ang) };
                const Math::Vector3 p = m_pos + dir * dist;

                const float fade = std::sinf(cp * DirectX::XM_PI);   // 0 -> 1 -> 0
                const Math::Color   col { m_color.x, m_color.y, m_color.z, fade };
                const Math::Vector3 em  { m_color.x * fade, m_color.y * fade, m_color.z * fade };
                DrawBillboard(a.star, p, SparkleConst::PickupConvergeSize, m_age * 6.0f + i, col, em);
            }
        }

        // core flash: brightest at the moment of convergence
        {
            const float d      = std::fabsf(t - convT) / convT;     // 0 at convergence
            const float cflash = (d < 1.0f) ? (1.0f - d) : 0.0f;
            if (cflash > 0.0f)
            {
                const float cs = SparkleConst::PickupCoreSize * (0.6f + 0.8f * cflash);
                const float w  = SparkleConst::PickupRayWhiten;   // bright white center
                const float cr = m_color.x * (1.0f - w) + w;
                const float cg = m_color.y * (1.0f - w) + w;
                const float cb = m_color.z * (1.0f - w) + w;
                const Math::Color   col { cr, cg, cb, cflash };
                const Math::Vector3 em  { cr, cg, cb };
                DrawBillboard(a.core, m_pos, cs, 0.0f, col, em);

                // extra pure-white inner spark on top (additive -> bright bloom center)
                const float inner = SparkleConst::PickupCoreSize * SparkleConst::PickupCoreInnerMul
                                  * (0.5f + 0.7f * cflash);
                const Math::Color   wcol { 1.0f, 1.0f, 1.0f, cflash };
                const Math::Vector3 wem  { 1.0f, 1.0f, 1.0f };
                DrawBillboard(a.core, m_pos, inner, m_age * 2.0f, wcol, wem);
            }
        }

        // phase 2: radial god-ray fan (thin, sharp, varying length, white)
        if (t >= convT)
        {
            const float bt   = (t - convT) / (1.0f - convT);        // 0..1
            const float grow = std::sqrtf(bt);                      // shoots out fast
            const float ba   = 1.0f - bt;                           // fade out
            const float wmul = 0.5f + 0.5f * ba;                     // thin out as it fades

            // rays are mostly white with a hint of the item color
            const float w  = SparkleConst::PickupRayWhiten;
            const float cr = m_color.x * (1.0f - w) + w;
            const float cg = m_color.y * (1.0f - w) + w;
            const float cb = m_color.z * (1.0f - w) + w;
            const Math::Color   col { cr, cg, cb, ba };
            const Math::Vector3 em  { cr * ba, cg * ba, cb * ba };

            const float spacing = DirectX::XM_2PI / static_cast<float>(m_beamCount);
            for (int j = 0; j < m_beamCount; ++j)
            {
                // 等間隔ベース＋ランダムな角度ズレで“まだら”に散らす
                const float jitter = (Hash(m_seed + j * 9.1f) - 0.5f)
                                   * spacing * SparkleConst::PickupBeamAngleJitter;
                const float ang = spacing * static_cast<float>(j) + jitter
                                + m_age * SparkleConst::PickupBeamSpin;

                // fully random length per ray (short..long) + subtle tip pulse
                const float lr = Hash(m_seed + j * 2.3f);
                float lmul = SparkleConst::PickupBeamShortMul
                           + (1.0f - SparkleConst::PickupBeamShortMul) * lr;
                lmul *= 1.0f + 0.08f * std::sinf(m_age * SparkleConst::PickupBeamTipFreq + j);

                // width is bipolar: each ray is clearly THIN or clearly THICK
                const float wr = (Hash(m_seed + j * 5.7f) < 0.5f)
                    ? SparkleConst::PickupBeamWidthMin
                    : SparkleConst::PickupBeamWidthMax;

                const float len   = SparkleConst::PickupBeamMaxLen   * grow * lmul;
                const float baseW = SparkleConst::PickupBeamBaseWidth * wmul * wr;
                const float tipW  = SparkleConst::PickupBeamTipWidth  * wmul * wr;
                DrawBeamTrapezoid(a.beam, m_pos, ang, len, baseW, tipW, col, em);
            }
        }

        KdShaderManager::Instance().UndoDepthStencilState();
        KdShaderManager::Instance().UndoBlendState();
    }

private:
    // lingering soft halo after the burst (pastel rainbow, gently expanding + fading)
    void DrawAfterglow() const
    {
        const Assets& a = GetAssets();
        const float at = (m_age - SparkleConst::PickupBurstLife) / SparkleConst::PickupAfterglowLife; // 0..1
        const float af = 1.0f - at;                                   // fade out
        const float size = SparkleConst::PickupAfterglowStart
            + (SparkleConst::PickupAfterglowEnd - SparkleConst::PickupAfterglowStart) * at;

        // pastel color (white blended with a slowly cycling rainbow hue)
        const Math::Vector3 rb = Rainbow(m_age * SparkleConst::PickupRainbowSpeed);
        const float s = SparkleConst::PickupRainbowSat;
        const float cr = (1.0f - s) + rb.x * s;
        const float cg = (1.0f - s) + rb.y * s;
        const float cb = (1.0f - s) + rb.z * s;

        const float aa = af * SparkleConst::PickupAfterglowAlpha;
        const Math::Color   col { cr, cg, cb, aa };
        const Math::Vector3 em  { cr * aa, cg * aa, cb * aa };
        DrawBillboard(a.glow, m_pos, size, m_age * 0.4f, col, em);

        // small bright lingering center
        const float ca = af * af * 0.7f;
        const Math::Color   ccol { cr, cg, cb, ca };
        const Math::Vector3 cem  { cr, cg, cb };
        DrawBillboard(a.core, m_pos, SparkleConst::PickupCoreSize * (0.5f + 0.5f * af), 0.0f, ccol, cem);

        // twinkling stars (Star_01) drifting out and blinking
        for (int i = 0; i < SparkleConst::PickupAfterStarCount; ++i)
        {
            const float ang = DirectX::XM_2PI * Hash(m_seed + i * 1.9f);
            const float rad = SparkleConst::PickupAfterStarRadius
                * (0.3f + 0.7f * Hash(m_seed + i * 4.1f)) * (0.6f + 0.5f * at);   // drift out
            const Math::Vector3 dir{ std::cosf(ang), std::sinf(ang * 1.7f) * 0.5f, std::sinf(ang) };
            const Math::Vector3 p = m_pos + dir * rad;

            const float tw    = 0.5f + 0.5f * std::sinf(m_age * SparkleConst::PickupAfterStarTwinkle + i * 2.0f);
            const float sa    = af * tw;
            const Math::Color   scol { cr, cg, cb, sa };
            const Math::Vector3 sem  { cr * sa, cg * sa, cb * sa };
            DrawBillboard(a.star, p, SparkleConst::PickupAfterStarSize * (0.6f + 0.4f * af),
                          m_age * 5.0f + i, scol, sem);
        }
    }

    // hue (0..1, wraps) -> RGB
    static Math::Vector3 Rainbow(float h)
    {
        h = h - std::floorf(h);
        auto cl = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
        return { cl(std::fabsf(h * 6.0f - 3.0f) - 1.0f),
                 cl(2.0f - std::fabsf(h * 6.0f - 2.0f)),
                 cl(2.0f - std::fabsf(h * 6.0f - 4.0f)) };
    }

    // shared textures / quads (loaded once)
    struct Assets
    {
        KdSquarePolygon            star;
        KdSquarePolygon            glow;
        KdSquarePolygon            core;
        KdStretchPoly              beam;   // tapered/flared radial beam
        std::shared_ptr<KdTexture> starTex;
        std::shared_ptr<KdTexture> glowTex;
        std::shared_ptr<KdTexture> coreTex;
        std::shared_ptr<KdTexture> beamTex;
        bool                       ready = false;
    };
    static Assets& GetAssets() { static Assets a; return a; }
    static void EnsureAssets()
    {
        Assets& a = GetAssets();
        if (a.ready) { return; }
        a.starTex = std::make_shared<KdTexture>();
        a.glowTex = std::make_shared<KdTexture>();
        a.coreTex = std::make_shared<KdTexture>();
        a.beamTex = std::make_shared<KdTexture>();
        if (!a.starTex->Load(SparkleConst::StarTexPath))      { return; }
        if (!a.glowTex->Load(SparkleConst::PickupGlowTexPath)) { return; }
        if (!a.coreTex->Load(SparkleConst::PickupCoreTexPath)) { return; }
        if (!a.beamTex->Load(SparkleConst::HakumeiBeamTexPath)){ return; }
        a.star.SetMaterial(a.starTex); a.star.SetScale(1.0f);
        a.glow.SetMaterial(a.glowTex); a.glow.SetScale(1.0f);
        a.core.SetMaterial(a.coreTex); a.core.SetScale(1.0f);
        a.beam.SetMaterial(a.beamTex);   // KdStretchPoly: corners set per-draw
        a.ready = true;
    }
    static float Hash(float n)
    {
        const float s = std::sinf(n) * 43758.5453f;
        return s - std::floorf(s);
    }

    Math::Color m_color { 1.0f, 1.0f, 1.0f, 1.0f };
    float       m_seed = 0.0f;
    int         m_beamCount = SparkleConst::PickupBeamCountMin;   // re-rolled each pickup
};
