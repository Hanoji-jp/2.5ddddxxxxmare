#pragma once
#include "EffectBase.h"
#include "../../Const/SparkleConst.h"

//==========================================================
// ItemEffect - unified per-item effect (EffectBase template family).
//  - rising / twinkling star_01 sparkles (CPU billboard, shared assets)
//  - optional looping Effekseer effect that follows the item
//
// Each item owns one ItemEffect:
//   Init(pos, params, efkPath) : star always; Effekseer loop if efkPath given
//   Update(pos, dt)            : advance star anim + move Effekseer to pos
//   DrawEffect(pos)            : draw the star sparkles (call in effect pass)
//   Stop()                     : stop the Effekseer loop (on pickup / removal)
//==========================================================
class ItemEffect
{
public:
    // Configurable look per item
    struct Params
    {
        float       starSize    = SparkleConst::BaseSize;    // star size
        float       orbitRadius = SparkleConst::OrbitRadius; // spread radius (spawn)
        Math::Color color       { SparkleConst::BaseColorR, SparkleConst::BaseColorG,
                                  SparkleConst::BaseColorB, SparkleConst::BaseColorA };
        float       colorShift  = SparkleConst::ColorShift;  // random color width (0..1)
    };

    // efkPath empty -> star only (no Effekseer loop)
    void Init(const Math::Vector3& pos,
              const Params&        params   = {},
              const std::string&   efkPath  = "",
              float                efkScale = 1.0f,
              float                efkSpeed = 1.0f)
    {
        EnsureStarAssets();

        m_params = params;

        // desync star phase / color between items
        m_seed = pos.x * 12.9898f + pos.z * 78.233f;
        m_time = std::fmodf(std::fabsf(pos.x * 0.37f + pos.z * 0.53f), SparkleConst::LifeTime);

        m_efkPath = efkPath;
        if (!m_efkPath.empty())
        {
            // loop Effekseer uses this ItemEffect's color, same as the star/pickup
            m_wpEfk = KdEffekseerManager::GetInstance().Play(
                m_efkPath, pos, m_params.color, efkScale, efkSpeed, true);
        }
    }

    void Update(const Math::Vector3& pos, float dt)
    {
        m_time += dt;
        if (const auto sp = m_wpEfk.lock()) { sp->SetPos(pos); }
    }

    // Draw rising / fading stars around 'center'
    void DrawEffect(const Math::Vector3& center) const
    {
        const Assets& a = GetAssets();
        if (!a.ready) { return; }

        // additive blend so the stars glow
        KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);
        KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
        KdShaderManager::Instance().m_StandardShader.SetDissolve(0.0f);

        const Math::Vector3 base = center
            + Math::Vector3{ 0.0f, SparkleConst::CenterOffsetY, 0.0f };

        for (int i = 0; i < SparkleConst::Count; ++i)
        {
            const float frac  = static_cast<float>(i) / static_cast<float>(SparkleConst::Count);
            const float phase = DirectX::XM_2PI * frac;

            // life 0..1 looping, staggered per star
            float life = m_time / SparkleConst::LifeTime + frac;
            life = life - std::floorf(life);

            const float rise = life * SparkleConst::RiseHeight;     // float upward
            const float fade = std::sinf(life * DirectX::XM_PI);    // appear -> peak -> vanish

            const float ang    = m_time * SparkleConst::OrbitSpeed + phase;
            const float radius = m_params.orbitRadius * (1.0f - life * 0.5f);

            const Math::Vector3 pos = base + Math::Vector3{
                std::cosf(ang) * radius,
                rise,
                std::sinf(ang) * radius };

            const float tw      = 0.5f + 0.5f * std::sinf(m_time * SparkleConst::TwinkleSpeed + phase);
            const float sizeMul = SparkleConst::TwinkleMin + (1.0f - SparkleConst::TwinkleMin) * tw;
            const float size    = m_params.starSize * sizeMul * fade;
            const float spin    = m_time * SparkleConst::SpinSpeed + phase;

            // per-star random color offset within colorShift width
            const float shift = m_params.colorShift;
            const float cr = Clamp01(m_params.color.x + (Hash(m_seed + i * 1.1f) - 0.5f) * 2.0f * shift);
            const float cg = Clamp01(m_params.color.y + (Hash(m_seed + i * 2.3f) - 0.5f) * 2.0f * shift);
            const float cb = Clamp01(m_params.color.z + (Hash(m_seed + i * 3.7f) - 0.5f) * 2.0f * shift);
            const float ca = fade * m_params.color.w;

            const Math::Color   col{ cr, cg, cb, ca };
            const Math::Vector3 emissive{ cr * fade, cg * fade, cb * fade };

            EffectBase::DrawBillboard(a.poly, pos, size, spin, col, emissive);
        }

        KdShaderManager::Instance().UndoDepthStencilState();
        KdShaderManager::Instance().UndoBlendState();
    }

    // Stop the looping Effekseer effect (safe when none)
    void Stop()
    {
        if (!m_efkPath.empty())
        {
            KdEffekseerManager::GetInstance().StopEffectsByFileName(m_efkPath);
        }
    }


private:
    // Star texture / quad shared by every item (loaded once)
    struct Assets
    {
        KdSquarePolygon            poly;
        std::shared_ptr<KdTexture> tex;
        bool                       ready = false;
    };
    static Assets& GetAssets() { static Assets a; return a; }

    static void EnsureStarAssets()
    {
        Assets& a = GetAssets();
        if (a.ready) { return; }
        a.tex = std::make_shared<KdTexture>();
        if (!a.tex->Load(SparkleConst::StarTexPath)) { return; }
        a.poly.SetMaterial(a.tex);
        a.poly.SetScale(1.0f);
        a.ready = true;
    }

    // deterministic 0..1 pseudo random (same star -> same color each frame)
    static float Hash(float n)
    {
        const float s = std::sinf(n) * 43758.5453f;
        return s - std::floorf(s);
    }
    static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    std::weak_ptr<KdEffekseerObject> m_wpEfk;
    std::string                      m_efkPath;
    Params                           m_params;
    float                            m_time = 0.0f;
    float                            m_seed = 0.0f;
};
