// PlatformServices.h - the store's own achievements: Steam, Game Center (iOS)
// and Google Play Games (Android). The game keeps its own list of what you've
// unlocked (Achievements.h); this passes each unlock on to whichever of these
// the build has, using the achievement's id.
//
//   Steam:        desktop builds made with STEAM=1 (Makefile) / -DINCRA_STEAM=ON (CMake),
//                 against the Steamworks SDK. See the README, "Achievements".
//   Game Center:  every iOS build (platform/ios/GameCenter.mm).
//   Play Games:   every Android build, once android/app/src/main/res/values/games-ids.xml
//                 has your game's ids (IncraVegetableActivity.java).
//   Anything else (itch.io, web, the demo): nothing to pass on; the game's own
//                 banner and achievements page are all there is.
#pragma once

#include <SDL3/SDL.h>

namespace services {

void init(SDL_Window* window); // after the window exists
void update();                 // once a frame (Steam needs its callbacks run)
void shutdown();

// Unlocks the achievement on the platform. Safe to call again for one that's
// already unlocked (the game re-sends every unlock at startup, in case one was
// earned while offline or before signing in).
void unlockAchievement(const char* id);

// True when the platform shows its own "achievement unlocked" pop-up, so the
// game doesn't show its banner as well.
bool showsOwnPopups();

// "Steam", "Game Center", "Google Play Games", or "" for none (shown on the achievements page).
const char* name();

} // namespace services
