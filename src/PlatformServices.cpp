#include "PlatformServices.h"

#include "Platform.h"

// ===========================================================================
// Steam (desktop, built with INCRA_STEAM)
// ===========================================================================
#if defined(INCRA_STEAM)
#include "steam/steam_api.h"

namespace services {
namespace {
bool gSteam = false;
}

void init(SDL_Window*) {
    if (kIsDemo) return; // the demo is its own app on Steam, without achievements
    SteamErrMsg err{};
    if (SteamAPI_InitEx(&err) != k_ESteamAPIInitResult_OK) {
        // Not started from Steam (and no steam_appid.txt next to the game), or Steam isn't running.
        SDL_Log("Steam: not available (%s) - achievements stay in the game", err);
        return;
    }
    gSteam = true;
    SDL_Log("Steam: signed in, achievements go to Steam");
}

void update() {
    if (gSteam) SteamAPI_RunCallbacks();
}

void shutdown() {
    if (gSteam) SteamAPI_Shutdown();
    gSteam = false;
}

void unlockAchievement(const char* id) {
    if (!gSteam || !SteamUserStats()) return;
    bool already = false;
    if (SteamUserStats()->GetAchievement(id, &already) && already) return;
    if (!SteamUserStats()->SetAchievement(id)) {
        SDL_Log("Steam: no achievement with API name '%s' (add it in Steamworks)", id);
        return;
    }
    SteamUserStats()->StoreStats(); // sends it, and Steam shows its pop-up
}

bool showsOwnPopups() { return gSteam; }
const char* name() { return gSteam ? "Steam" : ""; }

} // namespace services

// ===========================================================================
// Game Center (iOS, unless built with -DINCRA_GAME_CENTER=OFF): the work is in
// platform/ios/GameCenter.mm
// ===========================================================================
#elif defined(SDL_PLATFORM_IOS) && defined(INCRA_GAME_CENTER)

namespace gamecenter {
void init(SDL_Window* window);
void unlock(const char* id);
bool signedIn();
} // namespace gamecenter

namespace services {
void init(SDL_Window* window) {
    if (!kIsDemo) gamecenter::init(window);
}
void update() {}
void shutdown() {}
void unlockAchievement(const char* id) {
    if (!kIsDemo) gamecenter::unlock(id);
}
bool showsOwnPopups() { return gamecenter::signedIn(); }
const char* name() { return gamecenter::signedIn() ? "Game Center" : ""; }
} // namespace services

// ===========================================================================
// Google Play Games (Android): IncraVegetableActivity.java does the work,
// called through JNI.
// ===========================================================================
#elif defined(SDL_PLATFORM_ANDROID)
#include <jni.h>

namespace services {
namespace {
// Calls a method on the game's activity. sig is its JNI signature.
bool callActivity(const char* method, const char* sig, const char* arg, bool* result = nullptr) {
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (!env) return false;
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!activity) return false;
    bool ok = false;
    jclass cls = env->GetObjectClass(activity);
    jmethodID m = env->GetMethodID(cls, method, sig);
    if (m) {
        if (arg) {
            jstring s = env->NewStringUTF(arg);
            env->CallVoidMethod(activity, m, s);
            env->DeleteLocalRef(s);
        } else if (result) {
            *result = env->CallBooleanMethod(activity, m);
        }
        ok = !env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return ok;
}
bool gPlayGames = false;
} // namespace

void init(SDL_Window*) {
    if (kIsDemo) return;
    bool on = false;
    if (callActivity("hasPlayGames", "()Z", nullptr, &on)) gPlayGames = on;
    SDL_Log(gPlayGames ? "Play Games: on, achievements go to Google Play"
                       : "Play Games: not set up (games-ids.xml) - achievements stay in the game");
}
void update() {}
void shutdown() {}
void unlockAchievement(const char* id) {
    if (gPlayGames) callActivity("unlockAchievement", "(Ljava/lang/String;)V", id);
}
bool showsOwnPopups() { return gPlayGames; }
const char* name() { return gPlayGames ? "Google Play Games" : ""; }
} // namespace services

// ===========================================================================
// Everything else: no store achievements
// ===========================================================================
#else

namespace services {
void init(SDL_Window*) {}
void update() {}
void shutdown() {}
void unlockAchievement(const char*) {}
bool showsOwnPopups() { return false; }
const char* name() { return ""; }
} // namespace services

#endif
