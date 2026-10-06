// Platform.h - compile-time platform flags.
#pragma once

#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
inline constexpr bool kIsMobile = true;
#else
inline constexpr bool kIsMobile = false;
#endif

// Desktop-only options: window resolution, fullscreen toggle and a Quit button.
// (Mobile apps are always fullscreen and are closed by the OS, not the app.)
inline constexpr bool kIsDesktop = !kIsMobile;

inline constexpr const char* kGameVersion = "0.2";
