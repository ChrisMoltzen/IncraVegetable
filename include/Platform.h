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

// Demo builds (make demo / make demo-release): only the techs within
// INCRA_DEMO_RADIUS of The Barn can be bought; the rest of the tree shows but
// stays locked. The radius is in tech tree rows (112 px at 100% zoom; one step
// along an arm is about 1.2 rows). Saves are shared with the full game, so a
// demo farm carries straight over.
#ifndef INCRA_DEMO
#define INCRA_DEMO 0
#endif
#ifndef INCRA_DEMO_RADIUS
#define INCRA_DEMO_RADIUS 6
#endif
inline constexpr bool kIsDemo = INCRA_DEMO != 0;
inline constexpr float kDemoRadius = static_cast<float>(INCRA_DEMO_RADIUS);
inline constexpr const char* kGameTitle = kIsDemo ? "IncraVegetable Demo" : "IncraVegetable";
