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
    // 取得演出のスタイル
    enum class Style { Full, Calm, Ring, Coin };

    void Spawn(const Math::Vector3& pos, const Math::Color& color, Style style = Style::Full)
    {
        EnsureAssets();
        m_pos       = pos;
        m_color     = color;
        m_age       = 0.0f;
        m_isExpired = false;
        m_style     = style;

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
        float total = SparkleConst::PickupBurstLife + SparkleConst::PickupAfterglowLife;
        if      (m_style == Style::Calm) { total = SparkleConst::CalmPickupLife; }
        else if (m_style == Style::Ring) { total = SparkleConst::RingPickupLife; }
        else if (m_style == Style::Coin) { total = SparkleConst::RingPickupLife; }
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

        // スタイル別の演出（Full 以外はここで描いて終了）
        if (m_style != Style::Full)
        {
            if      (m_style == Style::Calm) { DrawCalm(); }
            else if (m_style == Style::Coin) { DrawCoin(); }
            else                             { DrawRing(); }
            KdShaderManager::Instance().UndoDepthStencilState();
            KdShaderManager::Instance().UndoBlendState();
            return;
        }

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
    // 落ち着いた取得演出：やわらかいグロー＋小さな星がふわっと上昇して消える
    void DrawCalm() const
    {
        const Assets& a = GetAssets();
        const float t    = m_age / SparkleConst::CalmPickupLife;   // 0..1
        const float fade = 1.0f - t;                               // フェードアウト

        // やわらかいグロー（ゆるく広がりつつ薄くなる）
        const float gs = SparkleConst::CalmGlowStart
            + (SparkleConst::CalmGlowEnd - SparkleConst::CalmGlowStart) * t;
        const float ga = fade * SparkleConst::CalmGlowAlpha;
        const Math::Color   gcol { m_color.x, m_color.y, m_color.z, ga };
        const Math::Vector3 gem  { m_color.x * ga, m_color.y * ga, m_color.z * ga };
        DrawBillboard(a.glow, m_pos, gs, m_age * SparkleConst::CalmGlowSpin, gcol, gem);

        // 立ち上がりの白いポップ（元気さ）
        const float pop = (t < SparkleConst::CalmPopFrac)
            ? (1.0f - t / SparkleConst::CalmPopFrac) : 0.0f;
        if (pop > 0.0f)
        {
            const float cs = SparkleConst::CalmCoreSize * (0.5f + 0.6f * pop);
            const Math::Color   pcol { 1.0f, 1.0f, 1.0f, pop };
            const Math::Vector3 pem  { 1.0f, 1.0f, 1.0f };
            DrawBillboard(a.core, m_pos, cs, m_age * 2.0f, pcol, pem);
        }

        // 星の色は元色を白寄りにブレンド（白く光らせる）
        const float w  = SparkleConst::CalmStarWhiten;
        const float scr = m_color.x * (1.0f - w) + w;
        const float scg = m_color.y * (1.0f - w) + w;
        const float scb = m_color.z * (1.0f - w) + w;

        // 小さな星が少しだけ広がりながら上昇＋またたいて消える
        for (int i = 0; i < SparkleConst::CalmStarCount; ++i)
        {
            const float ang    = DirectX::XM_2PI * Hash(m_seed + i * 1.7f);
            const float spread = SparkleConst::CalmStarSpread * (0.4f + 0.6f * Hash(m_seed + i * 3.3f))
                               * (0.5f + 0.5f * t);
            const float rise   = t * SparkleConst::CalmStarRise;
            Math::Vector3 p = m_pos;
            p.x += std::cosf(ang) * spread;
            p.y += rise;
            p.z += std::sinf(ang) * spread;

            const float tw = 0.5f + 0.5f * std::sinf(m_age * SparkleConst::CalmStarTwinkle + i * 2.0f);
            const float sa = fade * tw;
            const float sz = SparkleConst::CalmStarSize * fade * (0.7f + 0.3f * tw);
            const Math::Color   scol { scr, scg, scb, sa };
            const Math::Vector3 sem  { scr * sa, scg * sa, scb * sa };
            DrawBillboard(a.star, p, sz, m_age * 3.0f + i, scol, sem);
        }
    }

    // Object_Ring_Glow 風：中央からリング状に広がる＋星が上に飛んで放物線で落ちる
    void DrawRing() const
    {
        const Assets& a = GetAssets();
        const float t    = m_age / SparkleConst::RingPickupLife;   // 0..1
        const float fade = 1.0f - t;

        // 中央から広がるリング（速く消える：寿命の RingFadeFrac で消滅、アルファは 0.5 から減衰）
        const float ringT = (t < SparkleConst::RingFadeFrac)
            ? (t / SparkleConst::RingFadeFrac) : 1.0f;   // 0..1（リング進行）
        if (ringT < 1.0f)
        {
            const float rs = SparkleConst::RingGlowStart
                + (SparkleConst::RingGlowEnd - SparkleConst::RingGlowStart) * ringT;
            const float ra = SparkleConst::RingGlowAlpha * (1.0f - ringT);   // 0.5 → 0
            const Math::Color   rcol { m_color.x, m_color.y, m_color.z, ra };
            const Math::Vector3 rem  { m_color.x * ra, m_color.y * ra, m_color.z * ra };
            const KdPolygon& ringPoly = a.hasRing ? a.ring : a.glow;   // 画像が無ければグローで代用
            DrawBillboard(ringPoly, m_pos, rs, m_age * SparkleConst::RingGlowSpin, rcol, rem);
        }

        // 中央の初期フラッシュ（白）
        const float pop = (t < SparkleConst::RingCoreFrac)
            ? (1.0f - t / SparkleConst::RingCoreFrac) : 0.0f;
        if (pop > 0.0f)
        {
            const float cs = SparkleConst::RingCoreSize * (0.5f + 0.6f * pop);
            const Math::Color   pcol { 1.0f, 1.0f, 1.0f, pop };
            const Math::Vector3 pem  { 1.0f, 1.0f, 1.0f };
            DrawBillboard(a.core, m_pos, cs, m_age * 2.0f, pcol, pem);
        }

        // 星：上＋外へ飛び、重力で徐々に落ちる（放物線）。色は星ごとにランダムでカラフル。
        const float age = m_age;
        for (int i = 0; i < SparkleConst::RingStarCount; ++i)
        {
            const float ang     = DirectX::XM_2PI * static_cast<float>(i)
                                / static_cast<float>(SparkleConst::RingStarCount)
                                + Hash(m_seed + i) * 0.8f;
            const float outSpd  = SparkleConst::RingStarOut * (0.6f + 0.8f * Hash(m_seed + i * 2.7f));
            const float upSpd   = SparkleConst::RingStarUp  * (0.7f + 0.6f * Hash(m_seed + i * 4.3f));

            // 放物線：横は等速、縦は up*age - 0.5*g*age^2
            Math::Vector3 p = m_pos;
            p.x += std::cosf(ang) * outSpd * age;
            p.z += std::sinf(ang) * outSpd * age;
            p.y += upSpd * age - 0.5f * SparkleConst::RingStarGravity * age * age;

            // 星ごとのランダムな色（虹の中から1色）を白とブレンドして淡いパステルに
            const Math::Vector3 raw = Rainbow(Hash(m_seed + i * 12.9898f));
            const float sat = SparkleConst::RingStarSat;
            const Math::Vector3 rc{
                (1.0f - sat) + raw.x * sat,
                (1.0f - sat) + raw.y * sat,
                (1.0f - sat) + raw.z * sat };

            const float tw = 0.6f + 0.4f * std::sinf(age * SparkleConst::RingStarTwinkle + i * 2.0f);
            const float sa = fade * tw;
            const float sz = SparkleConst::RingStarSize * (0.6f + 0.4f * fade) * tw;
            const Math::Color   scol { rc.x, rc.y, rc.z, sa };
            const Math::Vector3 sem  { rc.x * sa, rc.y * sa, rc.z * sa };
            // 画像を回転させながら落下（spin = age 依存 ＋ 個体オフセット）
            const float spin = age * (4.0f + 2.0f * Hash(m_seed + i * 5.1f)) + i;
            DrawBillboard(a.star, p, sz, spin, scol, sem);
        }
    }

    // コイン専用：金色のリングがパッと広がり、白フラッシュ＋金の星が上へ舞い上がってきらめく
    void DrawCoin() const
    {
        const Assets& a = GetAssets();
        const float t    = m_age / SparkleConst::RingPickupLife;   // 0..1
        const float fade = 1.0f - t;
        const Math::Vector3 gold{ 1.0f, 0.82f, 0.30f };

        // 金のリング（速く広がって消える）
        const float ringT = (t < SparkleConst::RingFadeFrac) ? (t / SparkleConst::RingFadeFrac) : 1.0f;
        if (ringT < 1.0f)
        {
            const float rs = SparkleConst::RingGlowStart
                + (SparkleConst::RingGlowEnd - SparkleConst::RingGlowStart) * ringT;
            const float ra = SparkleConst::RingGlowAlpha * (1.0f - ringT);
            const Math::Color   rcol{ gold.x, gold.y, gold.z, ra };
            const Math::Vector3 rem { gold.x * ra, gold.y * ra, gold.z * ra };
            const KdPolygon& ringPoly = a.hasRing ? a.ring : a.glow;
            DrawBillboard(ringPoly, m_pos, rs, m_age * SparkleConst::RingGlowSpin, rcol, rem);
        }

        // 中央の白フラッシュ
        const float pop = (t < SparkleConst::RingCoreFrac) ? (1.0f - t / SparkleConst::RingCoreFrac) : 0.0f;
        if (pop > 0.0f)
        {
            const float cs = SparkleConst::RingCoreSize * (0.5f + 0.6f * pop);
            DrawBillboard(a.core, m_pos, cs, m_age * 2.0f, { 1.0f, 1.0f, 1.0f, pop }, { 1.0f, 1.0f, 1.0f });
        }

        // 金の星：主に上へ舞い上がる（コインらしく）。横は控えめ、重力で少し落ちる。
        const float age = m_age;
        for (int i = 0; i < SparkleConst::RingStarCount; ++i)
        {
            const float ang    = DirectX::XM_2PI * static_cast<float>(i)
                               / static_cast<float>(SparkleConst::RingStarCount) + Hash(m_seed + i) * 0.8f;
            const float outSpd = SparkleConst::RingStarOut * 0.45f * (0.5f + 0.6f * Hash(m_seed + i * 2.7f));
            const float upSpd  = SparkleConst::RingStarUp  * 1.35f * (0.8f + 0.4f * Hash(m_seed + i * 4.3f));

            Math::Vector3 p = m_pos;
            p.x += std::cosf(ang) * outSpd * age;
            p.z += std::sinf(ang) * outSpd * age;
            p.y += upSpd * age - 0.5f * SparkleConst::RingStarGravity * age * age;

            // 金色（個体差で少し明暗）＋またたき
            const float v = 0.85f + 0.15f * Hash(m_seed + i * 7.7f);
            const float tw = 0.6f + 0.4f * std::sinf(age * SparkleConst::RingStarTwinkle + i * 2.0f);
            const float sa = fade * tw;
            const float sz = SparkleConst::RingStarSize * (0.6f + 0.4f * fade) * tw;
            const Math::Color   scol{ gold.x * v, gold.y * v, gold.z * v, sa };
            const Math::Vector3 sem { gold.x * v * sa, gold.y * v * sa, gold.z * v * sa };
            const float spin = age * (4.0f + 2.0f * Hash(m_seed + i * 5.1f)) + i;
            DrawBillboard(a.star, p, sz, spin, scol, sem);
        }
    }

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
        KdSquarePolygon            ring;   // Ring スタイル用（Particle02）
        KdStretchPoly              beam;   // tapered/flared radial beam
        std::shared_ptr<KdTexture> starTex;
        std::shared_ptr<KdTexture> glowTex;
        std::shared_ptr<KdTexture> coreTex;
        std::shared_ptr<KdTexture> ringTex;
        std::shared_ptr<KdTexture> beamTex;
        bool                       ready   = false;
        bool                       hasRing = false;   // リング画像の読込成否
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

        // Ring 画像（任意：読めなければグローで代用）
        a.ringTex = std::make_shared<KdTexture>();
        a.hasRing = a.ringTex->Load(SparkleConst::RingTexPath);
        if (a.hasRing) { a.ring.SetMaterial(a.ringTex); a.ring.SetScale(1.0f); }

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
    Style       m_style = Style::Full;   // 取得演出スタイル
};
