#pragma once
// Shared game font (Kuramubon).
// The .otf is registered once at startup via KdFontManager::AddFontResource(GameFontPath);
// after that, AddFont(slot, GameFontName, height) uses the family name below.
namespace FontConst
{
    constexpr const char* GameFontPath = "Asset/Font/Kuramubon.otf";
    constexpr const char* GameFontName = "Kuramubon";
}
