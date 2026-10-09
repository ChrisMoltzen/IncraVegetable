// Menus.h - main menu, save-slot picker, settings and pause menu.
//
// Each menu turns input into an Action; Game decides what the action does
// (load a slot, change the window size, ...). Events must already be in
// game coordinates (1280x720).
#pragma once

#include "SaveSystem.h"
#include "UI.h"

#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
class MainMenu {
public:
    enum class Action { None, Continue, NewGame, LoadGame, Settings, Quit };

    // continueInfo is shown under the Continue button, e.g. "Slot 2 - Day 5".
    void refresh(bool hasSave, const std::string& continueInfo);
    Action handleEvent(const SDL_Event& e);
    void update(float dt) { clock_ += dt; }
    void render(SDL_Renderer* r) const;
    void renderBackground(SDL_Renderer* r) const; // also used behind the settings screen
    // Which vegetables drift across the background: the ones your save has unlocked.
    void setCrops(std::vector<int> crops) { crops_ = std::move(crops); }

    static void drawBackgroundBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
    static void drawLogoBuiltin(SDL_Renderer* r, const SDL_FRect& rc);

private:
    ui::ButtonList buttons_;
    std::vector<Action> actions_;
    std::vector<int> crops_{0}; // crop indices (see Farm.h)
    float clock_ = 0.f;
};

// ---------------------------------------------------------------------------
class SlotMenu {
public:
    enum class Mode { NewGame, LoadGame };
    enum class Action { None, Back, Chosen, Renamed };

    void open(Mode mode, const std::array<SlotInfo, SaveSystem::kSlotCount>& slots);
    void refresh(const std::array<SlotInfo, SaveSystem::kSlotCount>& slots); // after a rename
    Action handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* r) const;
    int chosenSlot() const { return chosen_; }
    // The name typed for a new farm (Chosen, in New Game) or a rename (Renamed). "" = unnamed.
    const std::string& chosenName() const { return chosenName_; }

private:
    void buildButtons();

    Mode mode_ = Mode::NewGame;
    std::array<SlotInfo, SaveSystem::kSlotCount> slots_{};
    ui::ButtonList buttons_;       // the slot cards + Back
    ui::ButtonList confirmButtons_; // Overwrite / Cancel
    int confirming_ = -1;          // slot waiting for "overwrite?" confirmation
    int chosen_ = -1;
    // Naming a farm: a new one (after picking its slot) or renaming one.
    void startNaming(int slot, bool forNewFarm);
    void stopNaming();
    int naming_ = -1;              // slot being named, -1 = not naming
    bool namingNew_ = false;
    std::string nameText_, chosenName_;
    ui::ButtonList nameButtons_;   // OK / Cancel
    std::vector<int> renameSlotOf_; // buttons_ index -> slot, for the Rename buttons (-1 = not one)
};

// ---------------------------------------------------------------------------
class SettingsMenu {
public:
    enum class Action { None, Back, MusicChanged, SfxChanged, SfxReleased, DisplayChanged };
    struct Resolution {
        int w, h;
    };

    // Edits *settings directly. resolutions is the list offered on desktop.
    void open(Settings* settings, std::vector<Resolution> resolutions);
    Action handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* r) const;

    static void drawSliderBarBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool fill);
    static void drawKnobBuiltin(SDL_Renderer* r, const SDL_FRect& rc);

private:
    enum Slider { None = -1, Music = 0, Sfx = 1 };
    SDL_FRect sliderTrack(int which) const;
    float* sliderValue(int which) const;
    int currentResolution() const;
    void buildButtons();

    Settings* settings_ = nullptr;
    std::vector<Resolution> resolutions_;
    ui::ButtonList buttons_; // [prev res, next res, fullscreen, back] or just [back]
    int dragging_ = None;
    float mouseX_ = -1000.f, mouseY_ = -1000.f;
};

// ---------------------------------------------------------------------------
class PauseMenu {
public:
    enum class Action { None, Resume, Settings, MainMenu, Quit };

    void open();
    Action handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* r) const;

private:
    ui::ButtonList buttons_;
    std::vector<Action> actions_;
};
