#pragma once

//==========================================================
// BuildConfig
//   Feature flags by build type. In the Distribute (release)
//   build, all debug features and editors are disabled.
//==========================================================
// Showcase: keep debug keys/editors enabled even in the Distribute build.
// Set this back to 0 to fully disable them for a real (production) release.
#define ENABLE_DEBUG_IN_DISTRIBUTE 0

#if defined(DISTRIBUTE_BUILD) && !ENABLE_DEBUG_IN_DISTRIBUTE
inline constexpr bool kDebugFeatures = false;   // release: debug/editor OFF
#else
inline constexpr bool kDebugFeatures = true;    // dev / showcase: enabled
#endif
