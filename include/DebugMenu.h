// DebugMenu.h - the developer/debug screen (F1, or the bug button in the
// bottom-left corner of every screen).
//
// Turn it off for release builds with:  cmake -DINCRA_DEBUG_TOOLS=OFF
#pragma once

#include "Farm.h"
#include "TechTree.h"
#include "UI.h"

#include <SDL3/SDL.h>
#include <array>
#include <functional>
#include <string>
#include <vector>

#ifndef INCRA_DEBUG_TOOLS
#define INCRA_DEBUG_TOOLS 1
#endif
inline constexpr bool kDebugTools = INCRA_DEBUG_TOOLS != 0;

// Cheats/overrides that live only for this session (they're never saved).
struct DebugOptions {
    float dayLengthOverride = 0.f; // seconds; 0 = use the tech tree value
    float gameSpeed = 1.f;
    bool freezeTimer = false;
    bool instantGrow = false;
    bool instantPick = false;
    bool showTileInfo = false;
    bool showFps = false;
};

class DebugMenu {
public:
    // Everything the debug screen can poke at. Game fills this in.
    struct Hooks {
        double* coins = nullptr;
        double* lifetimeCoins = nullptr;
        int* day = nullptr;
        TechTree* tree = nullptr;
        Farm* farm = nullptr;
        DebugOptions* options = nullptr;
        std::function<bool()> inGame;      // a farm is loaded
        std::function<bool()> dayRunning;  // a day is in progress (possibly paused)
        std::function<Stats()> stats;      // stats in effect, overrides included
        std::function<void()> statsChanged; // re-apply stats to the running day
        std::function<void()> endDay;
        std::function<void()> restartDay;
        std::function<bool()> saveNow;
        std::function<bool()> reloadSave;
        std::function<std::vector<std::string>()> info; // extra lines for the Info tab
    };

    void setHooks(Hooks h) { hooks_ = std::move(h); }
    bool isOpen() const { return open_; }
    void open(SDL_Window* window);
    void close();
    void handleEvent(const SDL_Event& e);
    void update(float dt);
    void render(SDL_Renderer* r);

    // The small button shown on every screen.
    static SDL_FRect buttonRect() { return SDL_FRect{8.f, 672.f, 40.f, 40.f}; }
    static void drawOpenButton(SDL_Renderer* r, bool hovered);
    static void drawOpenButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);

private:
    enum class Tab { Money, Time, Farm, Tech, Info, Count };
    enum class Kind { Button, Text, Field };
    struct Widget {
        Kind kind = Kind::Button;
        SDL_FRect rect{};
        std::string label;
        std::function<void()> onClick; // buttons: click; fields: Enter
        ui::Style style = ui::Style::Ghost;
        bool enabled = true;
        float scale = 1.75f;
        SDL_Color color{255, 255, 255, 255};
        int field = -1;
    };
    enum Field { FieldAmount, FieldDayLength, FieldDay, FieldCount };

    void build();
    void buildMoney();
    void buildTime();
    void buildFarm();
    void buildTech();
    void buildInfo();

    // Layout helpers.
    void text(float x, float y, const std::string& s, float scale = 2.f, SDL_Color c = {235, 235, 225, 255});
    void button(float x, float y, float w, const std::string& label, std::function<void()> fn, bool enabled = true,
                ui::Style style = ui::Style::Ghost);
    void toggle(float x, float y, float w, const std::string& label, bool* value, bool affectsStats);
    void field(float x, float y, float w, int id, std::function<void()> onEnter, bool enabled = true);

    void toast(const std::string& msg);
    void addCoins(double amount);
    void statsChanged();
    void focusField(int id);
    static bool parseNumber(const std::string& s, double& out);

    Hooks hooks_;
    bool open_ = false;
    SDL_Window* window_ = nullptr;
    Tab tab_ = Tab::Money;
    std::vector<Widget> widgets_;
    std::array<std::string, FieldCount> buffers_{};
    int focused_ = -1;
    int pressed_ = -1;
    float mouseX_ = -1000.f, mouseY_ = -1000.f;
    int techPage_ = 0;
    std::string toast_;
    float toastTime_ = 0.f;
    float clock_ = 0.f;
};
