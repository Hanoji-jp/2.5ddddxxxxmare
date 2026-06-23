#pragma once

//==========================================================
// DebugFlags
// Global debug-draw toggles. GameScene mirrors its "Show Debug
// Zones (1)" state here so every object's DrawDebug() (called
// generically from BaseScene) can be turned off in one place.
//==========================================================
namespace DebugFlags
{
	// false のときはすべてのデバッグワイヤー（オブジェクト側含む）を描かない。
	inline bool g_debugDrawVisible = false;
}
