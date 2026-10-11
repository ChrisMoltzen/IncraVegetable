// Achievements.h - the game's achievements: what they are, when they unlock,
// and which you've got.
//
// They belong to the player, not to a save slot: unlocking one in any farm
// unlocks it for good (kept in achievements.sav next to the save slots). Each
// unlock is also passed to the platform's own achievements (Steam, Game Center
// or Google Play Games, see PlatformServices.h) using the same id, so set them
// up there with exactly these ids:
//   Steam:       the achievement's "API Name"
//   Game Center: the achievement's "Achievement ID"
//   Google Play: give it the same name; Play makes the id, and the game finds it as
//                the resource achievement_<id> (see android/app/src/main/res/values/games-ids.xml)
//
// To add one: add it to the list in Achievements.cpp (and on each platform).
// Ids are never shown, but must never change once players have them.
#pragma once

#include "UI.h"

#include <SDL3/SDL.h>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <vector>

// What the unlock rules look at: the farm being played, worked out by the game.
struct AchievementProgress {
    long long picked = 0;        // vegetables picked on this farm, ever
    double lifetimeCoins = 0.0;  // coins earned on this farm, ever
    double bestDayCoins = 0.0;   // the most earned in one day (including today so far)
    int day = 0;                 // the day you're on
    int upgrades = 0;            // tech tree levels bought
    int farmers = 0;
    int maxFarmers = 0;          // the most farmers the tech tree can give
    bool helpingHand = false;    // auto-pick bought
    bool allCrops = false;       // every vegetable unlocked
    bool allUpgrades = false;    // every tech bought to its max
};

struct AchievementDef {
    const char* id;          // snake_case of the name, never changes (see above)
    const char* name;
    const char* description;
    std::function<bool(const AchievementProgress&)> done;
};

class Achievements {
public:
    static const std::vector<AchievementDef>& all();

    // Unlocks every achievement whose rule is met. Returns the newly unlocked ones' indices.
    std::vector<int> check(const AchievementProgress& p);
    bool unlock(int index); // false if it was already unlocked
    bool unlocked(int index) const { return got_.count(all()[index].id) > 0; }
    int unlockedCount() const;
    void lockAll() { got_.clear(); } // debug: start over (this computer only)

    // The file is a list of ids, one per line ("got <id>").
    std::string serialize() const;
    void parse(const std::string& text);

    // Unlock banners waiting to be shown, oldest first.
    std::deque<int>& popups() { return popups_; }

private:
    std::set<std::string> got_;
    std::deque<int> popups_;
};

// The "Achievement unlocked" banner that slides in under the top bar.
class AchievementBanner {
public:
    void update(float dt, Achievements& a); // takes the next banner from the queue when one finishes
    void render(SDL_Renderer* r) const;
    bool showing() const { return current_ >= 0; }

private:
    int current_ = -1;
    float time_ = 0.f;
    bool quick_ = false; // more are waiting, so this one is shown for less time
};

// The achievements page: every achievement, the ones you've got in colour.
// Opened from the trophy button on the main menu and from the stats page.
class AchievementsScreen {
public:
    void open();
    bool handleEvent(const SDL_Event& e); // true = close
    void render(SDL_Renderer* r, const Achievements& a) const;

    // An achievement's trophy (on the banner and the page). Your own art for it, if you've
    // drawn some: assets/achievements/<id>.png, and optionally <id>_locked.png for before
    // it's unlocked (without one, <id>.png is shown faded). Achievements without their own
    // art use ui/trophy and ui/trophy_locked. index -1 = the general trophy.
    static void drawTrophy(SDL_Renderer* r, const SDL_FRect& rc, bool got, int index = -1);
    // The art names for an achievement's own trophy.
    static std::string artName(int index, bool got);
    static void drawTrophyBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool got);
    static void drawButton(SDL_Renderer* r, const SDL_FRect& rc, bool hovered); // the main menu's trophy button
    static void drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);

private:
    ui::ButtonList buttons_; // Back
};
