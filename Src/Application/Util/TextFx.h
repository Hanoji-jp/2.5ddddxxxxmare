#pragma once
#include "Framework/Shader/SpriteShader/KdSpriteShader.h"
#include "../Const/TextFxConst.h"

// UI text helper: draw a right-down drop shadow, then the text on top.
// Used for all UI text except the conversation window.
namespace TextFx
{
    inline void DrawShadowed(KdSpriteShader& sprite,
        const Math::Vector2& pos, const Math::Color& col, const char* text)
    {
        const Math::Color sh(TextFxConst::ShadowR, TextFxConst::ShadowG,
            TextFxConst::ShadowB, col.w * TextFxConst::ShadowAlphaMul);
        sprite.DrawFont(
            Math::Vector2(pos.x + TextFxConst::ShadowOffX, pos.y - TextFxConst::ShadowOffY),
            &sh, "%s", text);
        sprite.DrawFont(pos, &col, "%s", text);
    }
}
