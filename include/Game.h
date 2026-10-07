// Game.h - owns the window, the state machine, audio and saving/loading.
#pragma once

#include "Audio.h"
#include "DebugMenu.h"
#include "Farm.h"
#include "Menus.h"
#include "SaveSystem.h"
#include "TechTree.h"
#include "TechTreeScreen.h"

#include <SDL3/SDL.h>
#include <random>
#include <string>
#include <vector>

class Game {
public:
    static constexpr int kWidth = 1280;  // the game always draws at this size;
    static constexpr int kHeight = 720;  // SDL scales it to fit the window/screen

    ~Game();
    bool init();
    void handleEvent(SDL_Event& e); // converts e to game coordinates
    void iterate();                 // one frame: update + render, timing handled here
    void update(float dt);
    void render(bool present = true); // present=false lets tools read the frame back first
    bool running() const { return running_; }
    void shutdown(); // saves everything

    SDL_Renderer* renderer() const { return renderer_; }

    // Writes the built-in art as PNG templates (see Art.h). Returns files written.
    int exportArtTemplates(const std::string& folder);

private:
    enum class State { MainMenu, SlotSelect, Settings, Farming, DaySummary, TechTree, Paused };

    // Flow
    void goToMainMenu();
    void newGame(int slot);
    bool loadGame(int slot);
    void loadTechLevel(const std::string& id, int level);
    void startDay();
    void endDay();
    void openTechTree();
    void pause();
    void resume();
    void openSettings();
    void closeSettings();
    bool inGame() const;

    // Saving
    std::string serialize() const;
    const char* stateName(State s) const;
    void saveGame();
    void saveSettings();

    // Display
    std::vector<SettingsMenu::Resolution> availableResolutions() const;
    void applyDisplaySettings();

    // Debug screen
    Stats currentStats() const; // tech tree stats with any debug overrides applied
    bool dayRunning() const;
    void setupDebugMenu();
    bool handleDebugInput(const SDL_Event& e);
    std::vector<std::string> debugInfo() const;

    // Input helpers
    bool handlePauseButton(const SDL_Event& e);
    bool handleEndDayButton(const SDL_Event& e);
    bool pointerActive() const;

    // Rendering
    void renderPlayScene(State s);
    void renderBackground();
    void renderHud();
    void renderSummary();

    void playHarvestSounds();
    void showToast(const std::string& msg);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = true;
    Uint64 lastTicks_ = 0;

    State state_ = State::MainMenu;
    State pausedFrom_ = State::Farming;   // what's under the pause menu
    State settingsFrom_ = State::MainMenu; // where Settings returns to

    TechTree tree_;
    TechTreeScreen treeScreen_;
    Farm farm_;
    MainMenu mainMenu_;
    SlotMenu slotMenu_;
    SettingsMenu settingsMenu_;
    PauseMenu pauseMenu_;
    DebugMenu debugMenu_;
    DebugOptions debug_;
    bool debugButtonDown_ = false;
    float fps_ = 60.f;
    Audio audio_;
    SaveSystem saves_;
    Settings settings_;
    std::mt19937 rng_{std::random_device{}()};

    SlotMenu::Mode slotMode_ = SlotMenu::Mode::NewGame;
    int currentSlot_ = -1;
    double coins_ = 0.0;
    double lifetimeCoins_ = 0.0;
    int day_ = 1;
    float summaryTimer_ = 0.f;
    float autosaveTimer_ = 0.f;
    std::string toast_;
    float toastTime_ = 0.f;

    // Pointer state. With touch, vegetables are picked only while a finger is down.
    float mouseX_ = -1000.f, mouseY_ = -1000.f;
    bool mouseInside_ = false;
    bool usingTouch_ = false;
    bool touchDown_ = false;
    bool pauseButtonDown_ = false;
    bool endDayButtonDown_ = false;

    SDL_FRect summaryButton_{640.f - 150.f, 470.f, 300.f, 62.f};
    SDL_FRect pauseButton_{1280.f - 62.f, 12.f, 48.f, 48.f};
    static constexpr float kDialX = 640.f, kDialHorizon = 56.f, kDialRadius = 48.f; // day/night dial in the HUD
    SDL_FRect endDayButton_{1280.f - 196.f, 720.f - 60.f, 180.f, 46.f}; // bottom-right while farming
};
