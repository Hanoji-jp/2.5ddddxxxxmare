#pragma once
#include "Framework/Shader/SpriteShader/KdSpriteShader.h"
#include "Framework/Font/KdFont.h"
#include "../Const/TextFxConst.h"
#include <string>
#include <algorithm>

//==========================================================
// UiButton
// One rounded selectable button. When selected it is highlighted
// (scaled up + gold + pulse); otherwise dimmed. Size auto-fits the text.
//   m_btn.Set("ENTER : GO", fontNo);
//   const float hw = m_btn.HalfWidth(padX);   // for layout
//   m_btn.Draw(cx, cy, halfH, padX, radius, selected, alpha, pulse);
//==========================================================
class UiButton
{
public:
    UiButton() = default;

    void Set(const std::string& text, int fontNo) { m_text = text; m_fontNo = fontNo; }
    const std::string& Text() const { return m_text; }

    // half width (px) = text width / 2 + side padding (for centered layout)
    float HalfWidth(float padX) const
    {
        return MeasureW() * 0.5f + padX;
    }

    // draw the rounded button (cx,cy = center), highlighted if selected
    void Draw(float cx, float cy, float halfH, float padX, float radius,
              bool selected, float alpha, float pulse) const
    {
        auto& sprite = KdShaderManager::Instance().m_spriteShader;

        const float hw  = HalfWidth(padX);
        const float scl = selected ? (1.12f + 0.06f * pulse) : 1.0f;
        const float hwS = hw    * scl;
        const float hhS = halfH * scl;

        Math::Color edge, body, text;
        if (selected)
        {
            edge = Math::Color(1.0f, 0.95f, 0.5f, alpha);                           // bright gold edge
            body = Math::Color(1.0f, 0.85f, 0.35f, (0.85f + 0.15f * pulse) * alpha); // gold fill
            text = Math::Color(0.05f, 0.06f, 0.10f, alpha);                         // dark text
        }
        else
        {
            edge = Math::Color(1.0f, 0.85f, 0.35f, 0.5f * alpha);
            body = Math::Color(0.05f, 0.07f, 0.12f, 0.85f * alpha);
            text = Math::Color(0.75f, 0.78f, 0.85f, alpha);
        }

        constexpr float kEdge = 3.0f;   // edge thickness
        sprite.DrawRoundedBox(static_cast<int>(cx), static_cast<int>(cy),
            static_cast<int>(hwS + kEdge), static_cast<int>(hhS + kEdge), radius + kEdge, &edge, 8);
        sprite.DrawRoundedBox(static_cast<int>(cx), static_cast<int>(cy),
            static_cast<int>(hwS), static_cast<int>(hhS), radius, &body, 8);

        // text (centered + bottom-right shadow)
        auto fs = KdFontManager::Instance().CreateFontTexture(m_fontNo, m_text.c_str(), false);
        if (fs)
        {
            float w = 0.0f, h = 0.0f;
            for (const auto& d : fs->GetTexList())
            {
                if (d && d->FontTex)
                {
                    w += static_cast<float>(d->FontTex->GetInfo().Width);
                    h  = std::max(h, static_cast<float>(d->FontTex->GetInfo().Height));
                }
            }
            const Math::Vector2 pos(cx - w * 0.5f, cy - h * 0.5f);
            const Math::Color shc(0.0f, 0.0f, 0.0f, text.w * TextFxConst::ShadowAlphaMul);
            sprite.DrawFont(fs, Math::Vector2(pos.x + TextFxConst::ShadowOffX, pos.y - TextFxConst::ShadowOffY), &shc, 0);
            sprite.DrawFont(fs, pos, &text, 0);
        }
    }

private:
    float MeasureW() const
    {
        float w = 0.0f;
        if (auto fs = KdFontManager::Instance().CreateFontTexture(m_fontNo, m_text.c_str(), false))
        {
            for (const auto& d : fs->GetTexList())
            {
                if (d && d->FontTex) { w += static_cast<float>(d->FontTex->GetInfo().Width); }
            }
        }
        return w;
    }

    std::string m_text;
    int         m_fontNo = 0;
};
