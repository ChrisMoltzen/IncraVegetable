#include "Game.h"

#include "Art.h"
#include "Draw.h"
#include "Platform.h"
#include "UI.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {
const SDL_Color kWhite{255, 255, 255, 255};
const SDL_Color kGold{255, 205, 70, 255};
const SDL_Color kGrey{190, 195, 190, 255};

constexpr float kAutosaveSeconds = 15.f;


bool isMouseUp(const SDL_Event& e) { return e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT; }
} // namespace

Game::~Game() {
    audio_.shutdown();
    art::shutdown();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
}

bool Game::init() {
    SDL_SetAppMetadata("IncraVegetable", kGameVersion, "com.incravegetable.game");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    saves_.init();
    settings_ = saves_.loadSettings();

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (kIsMobile) flags |= SDL_WINDOW_FULLSCREEN;
    else flags |= SDL_WINDOW_RESIZABLE;
    if (!SDL_CreateWindowAndRenderer("IncraVegetable", settings_.windowWidth, settings_.windowHeight, flags, &window_,
                                     &renderer_)) {
        SDL_Log("Could not create window: %s", SDL_GetError());
        return false;
    }
    // The game always thinks in 1280x720; SDL scales it to whatever the window or screen is.
    SDL_SetRenderLogicalPresentation(renderer_, kWidth, kHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer_, 1);

    std::vector<std::pair<std::string, std::string>> techNodes;
    for (const auto& n : tree_.nodes()) techNodes.push_back({n.id, n.name});
    art::init(renderer_, techNodes);
    if (kIsDesktop && settings_.fullscreen) applyDisplaySettings();

    audio_.init(); // the game still runs if there's no audio device
    audio_.setMusicVolume(settings_.musicVolume);
    audio_.setSfxVolume(settings_.sfxVolume);


    lastTicks_ = SDL_GetTicksNS();
    setupDebugMenu();
    goToMainMenu();
    return true;
}

void Game::shutdown() {
    saveGame();
    saveSettings();
}

// ---------------------------------------------------------------------------
// Flow between screens
// ---------------------------------------------------------------------------

bool Game::inGame() const {
    return state_ == State::Farming || state_ == State::DaySummary || state_ == State::TechTree ||
           state_ == State::Paused || (state_ == State::Settings && settingsFrom_ == State::Paused);
}

void Game::goToMainMenu() {
    std::string info;
    int slot = settings_.lastSlot >= 0 && saves_.slotInfo(settings_.lastSlot).exists ? settings_.lastSlot
                                                                                        : saves_.mostRecentSlot();
    if (slot >= 0) {
        SlotInfo s = saves_.slotInfo(slot);
        info = draw::strf("Slot %d  -  Day %d  -  %s coins", slot + 1, s.day, draw::number(s.coins).c_str());
    }
    mainMenu_.refresh(slot >= 0, info);
    state_ = State::MainMenu;
}

void Game::newGame(int slot) {
    currentSlot_ = slot;
    coins_ = 0.0;
    lifetimeCoins_ = 0.0;
    day_ = 1;
    tree_.resetLevels();
    settings_.lastSlot = slot;
    saveSettings();
    startDay();
    saveGame();
}

bool Game::loadGame(int slot) {
    std::string contents;
    if (!saves_.readSlot(slot, contents)) return false;

    tree_.resetLevels();
    coins_ = 0.0;
    lifetimeCoins_ = 0.0;
    day_ = 1;
    std::string phase = "farming";
    std::vector<std::string> farmLines;

    std::istringstream in(contents);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key == "coins") ss >> coins_;
        else if (key == "lifetime") ss >> lifetimeCoins_;
        else if (key == "day") ss >> day_;
        else if (key == "phase") ss >> phase;
        else if (key == "tech") {
            std::string id;
            int level = 0;
            ss >> id >> level;
            // Unknown ids are ignored, so old saves keep working as the tree changes.
            if (TechNode* n = tree_.find(id)) n->level = std::clamp(level, 0, n->maxLevel);
        } else if (key == "farm" || key == "tile") {
            farmLines.push_back(line);
        }
    }
    day_ = std::max(1, day_);
    coins_ = std::max(0.0, coins_);

    currentSlot_ = slot;
    settings_.lastSlot = slot;
    saveSettings();

    if (phase == "techtree") {
        openTechTree();
    } else if (farm_.restore(farmLines, currentStats())) {
        state_ = State::Farming; // pick up exactly where the day was left
        autosaveTimer_ = 0.f;
    } else {
        startDay();
    }
    return true;
}

void Game::startDay() {
    farm_.startDay(currentStats(), rng_);
    state_ = State::Farming;
    autosaveTimer_ = 0.f;
}

void Game::endDay() {
    farm_.setTimeLeft(0.f); // ended early: stop the clock so nothing more is picked behind the summary
    lifetimeCoins_ += farm_.earnedToday();
    ++day_;
    state_ = State::DaySummary;
    summaryTimer_ = 0.f;
    audio_.play(Sfx::Sunset);
    saveGame();
}

void Game::openTechTree() {
    treeScreen_.open(tree_);
    state_ = State::TechTree;
}

void Game::pause() {
    if (state_ != State::Farming && state_ != State::TechTree && state_ != State::DaySummary) return;
    pausedFrom_ = state_;
    pauseMenu_.open();
    state_ = State::Paused;
    touchDown_ = false;
    saveGame();
}

void Game::resume() {
    state_ = pausedFrom_;
    lastTicks_ = SDL_GetTicksNS(); // don't count the time spent paused
}

void Game::openSettings() {
    settingsFrom_ = state_;
    settingsMenu_.open(&settings_, availableResolutions());
    state_ = State::Settings;
}

void Game::closeSettings() {
    saveSettings();
    state_ = settingsFrom_;
    if (state_ == State::MainMenu) goToMainMenu();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

std::string Game::serialize() const {
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    State playing = state_ == State::Paused ? pausedFrom_ : state_;
    if (state_ == State::Settings && settingsFrom_ == State::Paused) playing = pausedFrom_;
    bool midDay = playing == State::Farming;

    std::ostringstream out;
    out.precision(17);
    out << "incravegetable-save 2\n";
    // The summary fields come first; the slot picker only reads this far.
    out << "day " << day_ << "\n";
    out << "coins " << coins_ << "\n";
    out << "saved " << static_cast<long long>(now) << "\n";
    out << "lifetime " << lifetimeCoins_ << "\n";
    out << "phase " << (midDay ? "farming" : "techtree") << "\n";
    for (const auto& n : tree_.nodes()) out << "tech " << n.id << " " << n.level << "\n";
    if (midDay) out << farm_.serialize();
    return out.str();
}

void Game::saveGame() {
    if (currentSlot_ < 0 || !inGame()) return;
    saves_.writeSlot(currentSlot_, serialize());
    autosaveTimer_ = 0.f;
}

void Game::saveSettings() { saves_.saveSettings(settings_); }

// ---------------------------------------------------------------------------
// Display settings (desktop only)
// ---------------------------------------------------------------------------

std::vector<SettingsMenu::Resolution> Game::availableResolutions() const {
    static const SettingsMenu::Resolution candidates[] = {
        {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160},
    };
    SDL_Rect usable{0, 0, 1920, 1080};
    SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    if (display) SDL_GetDisplayUsableBounds(display, &usable);

    std::vector<SettingsMenu::Resolution> list;
    for (const auto& c : candidates) {
        if (c.w <= usable.w && c.h <= usable.h) list.push_back(c);
    }
    if (list.empty()) list.push_back(candidates[0]);
    bool hasCurrent = std::any_of(list.begin(), list.end(), [&](const SettingsMenu::Resolution& r) {
        return r.w == settings_.windowWidth && r.h == settings_.windowHeight;
    });
    if (!hasCurrent) list.push_back({settings_.windowWidth, settings_.windowHeight});
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.w * a.h < b.w * b.h; });
    return list;
}

void Game::applyDisplaySettings() {
    if (!kIsDesktop) return;
    if (settings_.fullscreen) {
        SDL_SetWindowFullscreenMode(window_, nullptr); // borderless, at the desktop resolution
        SDL_SetWindowFullscreen(window_, true);
    } else {
        SDL_SetWindowFullscreen(window_, false);
        SDL_SetWindowSize(window_, settings_.windowWidth, settings_.windowHeight);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool Game::pointerActive() const { return usingTouch_ ? touchDown_ : mouseInside_; }

// The pause button sits in the top-right corner during play. Returns true if
// the event was used by it.
bool Game::handlePauseButton(const SDL_Event& e) {
    if (state_ != State::Farming && state_ != State::TechTree) return false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
        draw::pointInRect(e.button.x, e.button.y, pauseButton_)) {
        pauseButtonDown_ = true;
        return true;
    }
    if (isMouseUp(e) && pauseButtonDown_) {
        pauseButtonDown_ = false;
        if (draw::pointInRect(e.button.x, e.button.y, pauseButton_)) {
            audio_.play(Sfx::Click);
            pause();
        }
        return true;
    }
    return false;
}

// The End Day button sits in the bottom-right corner while farming, so you
// don't have to wait for the timer. Returns true if the event was used by it.
bool Game::handleEndDayButton(const SDL_Event& e) {
    if (state_ != State::Farming) return false;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
        draw::pointInRect(e.button.x, e.button.y, endDayButton_)) {
        endDayButtonDown_ = true;
        return true;
    }
    if (isMouseUp(e) && endDayButtonDown_) {
        endDayButtonDown_ = false;
        if (draw::pointInRect(e.button.x, e.button.y, endDayButton_)) {
            audio_.play(Sfx::Click);
            endDay();
        }
        return true;
    }
    return false;
}

void Game::handleEvent(SDL_Event& e) {
    SDL_ConvertEventToRenderCoordinates(renderer_, &e);

    // ---- App lifecycle (mostly matters on phones) ----
    switch (e.type) {
    case SDL_EVENT_QUIT:
        running_ = false;
        return;
    case SDL_EVENT_TERMINATING:
        shutdown();
        return;
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        if (state_ == State::Farming) pause();
        saveGame();
        saveSettings();
        audio_.pauseDevice(true);
        return;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        audio_.pauseDevice(false);
        lastTicks_ = SDL_GetTicksNS();
        return;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        // Stop the clock if the player clicks away from the game mid-day.
        if (state_ == State::Farming) pause();
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if (kIsDesktop && !(SDL_GetWindowFlags(window_) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
            settings_.windowWidth = e.window.data1;
            settings_.windowHeight = e.window.data2;
        }
        break;
    default:
        break;
    }

    // ---- Pointer tracking (mouse, or a finger acting as the mouse) ----
    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        usingTouch_ = e.motion.which == SDL_TOUCH_MOUSEID;
        mouseInside_ = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        usingTouch_ = e.button.which == SDL_TOUCH_MOUSEID;
        if (usingTouch_) touchDown_ = true;
        mouseX_ = e.button.x;
        mouseY_ = e.button.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.which == SDL_TOUCH_MOUSEID) touchDown_ = false;
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        mouseInside_ = false;
        break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        mouseInside_ = true;
        break;
    default:
        break;
    }

    // F5 reloads the artwork from assets/, so you can see changes without restarting.
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F5 && !e.key.repeat) {
        int n = art::reload();
        showToast(art::assetDir().empty() ? std::string("No assets folder found")
                                          : draw::strf("Artwork reloaded - %d image%s", n, n == 1 ? "" : "s"));
        return;
    }
    if (handleDebugInput(e)) return;
    if (handlePauseButton(e)) return;
    if (handleEndDayButton(e)) return;

    switch (state_) {
    case State::MainMenu: {
        MainMenu::Action action = mainMenu_.handleEvent(e);
        switch (action) {
        case MainMenu::Action::None:
            break;
        case MainMenu::Action::Continue: {
            audio_.play(Sfx::Click);
            int slot = settings_.lastSlot >= 0 && saves_.slotInfo(settings_.lastSlot).exists ? settings_.lastSlot
                                                                                                : saves_.mostRecentSlot();
            if (slot < 0 || !loadGame(slot)) goToMainMenu();
            break;
        }
        case MainMenu::Action::NewGame:
        case MainMenu::Action::LoadGame: {
            audio_.play(Sfx::Click);
            std::array<SlotInfo, SaveSystem::kSlotCount> slots;
            for (int i = 0; i < SaveSystem::kSlotCount; ++i) slots[i] = saves_.slotInfo(i);
            slotMenu_.open(action == MainMenu::Action::NewGame ? SlotMenu::Mode::NewGame : SlotMenu::Mode::LoadGame,
                           slots);
            slotMode_ = action == MainMenu::Action::NewGame ? SlotMenu::Mode::NewGame : SlotMenu::Mode::LoadGame;
            state_ = State::SlotSelect;
            break;
        }
        case MainMenu::Action::Settings:
            audio_.play(Sfx::Click);
            openSettings();
            break;
        case MainMenu::Action::Quit:
            running_ = false;
            break;
        }
        break;
    }

    case State::SlotSelect:
        switch (slotMenu_.handleEvent(e)) {
        case SlotMenu::Action::None:
            break;
        case SlotMenu::Action::Back:
            audio_.play(Sfx::Click);
            goToMainMenu();
            break;
        case SlotMenu::Action::Chosen:
            audio_.play(Sfx::Click);
            if (slotMode_ == SlotMenu::Mode::NewGame) newGame(slotMenu_.chosenSlot());
            else if (!loadGame(slotMenu_.chosenSlot())) goToMainMenu();
            break;
        }
        break;

    case State::Settings:
        switch (settingsMenu_.handleEvent(e)) {
        case SettingsMenu::Action::None:
            break;
        case SettingsMenu::Action::Back:
            audio_.play(Sfx::Click);
            closeSettings();
            break;
        case SettingsMenu::Action::MusicChanged:
            audio_.setMusicVolume(settings_.musicVolume);
            break;
        case SettingsMenu::Action::SfxChanged:
            audio_.setSfxVolume(settings_.sfxVolume);
            break;
        case SettingsMenu::Action::SfxReleased:
            audio_.play(Sfx::Coin); // preview the new volume
            break;
        case SettingsMenu::Action::DisplayChanged:
            audio_.play(Sfx::Click);
            applyDisplaySettings();
            break;
        }
        break;

    case State::Paused:
        switch (pauseMenu_.handleEvent(e)) {
        case PauseMenu::Action::None:
            break;
        case PauseMenu::Action::Resume:
            audio_.play(Sfx::Click);
            resume();
            break;
        case PauseMenu::Action::Settings:
            audio_.play(Sfx::Click);
            openSettings();
            break;
        case PauseMenu::Action::MainMenu:
            audio_.play(Sfx::Click);
            saveGame();
            goToMainMenu();
            break;
        case PauseMenu::Action::Quit:
            running_ = false;
            break;
        }
        break;

    case State::Farming:
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) pause();
        break; // picking is hover-based, handled in update()

    case State::DaySummary: {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            pause();
            break;
        }
        if (summaryTimer_ < 0.6f) break; // ignore frantic clicks right as the day ends
        bool clicked = isMouseUp(e) && draw::pointInRect(e.button.x, e.button.y, summaryButton_);
        bool key = e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE);
        if (clicked || key) {
            audio_.play(Sfx::Click);
            openTechTree();
        }
        break;
    }

    case State::TechTree: {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            pause();
            break;
        }
        switch (treeScreen_.handleEvent(e, tree_, coins_)) {
        case TechTreeScreen::Action::None:
            break;
        case TechTreeScreen::Action::Purchased:
            audio_.play(Sfx::Buy);
            saveGame();
            break;
        case TechTreeScreen::Action::Denied:
            audio_.play(Sfx::Deny);
            break;
        case TechTreeScreen::Action::Selected:
            audio_.play(Sfx::Click);
            break;
        case TechTreeScreen::Action::StartDay:
            audio_.play(Sfx::Click);
            startDay();
            saveGame();
            break;
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::iterate() {
    Uint64 now = SDL_GetTicksNS();
    float dt = static_cast<float>(now - lastTicks_) / 1e9f;
    lastTicks_ = now;
    if (dt > 0.f) fps_ += (1.f / dt - fps_) * 0.05f;
    update(std::min(dt, 0.1f)); // don't jump ahead after a stall
    render();
}

void Game::showToast(const std::string& msg) {
    toast_ = msg;
    toastTime_ = 2.5f;
}

int Game::exportArtTemplates(const std::string& folder) { return art::exportTemplates(renderer_, folder); }

void Game::update(float dt) {
    toastTime_ = std::max(0.f, toastTime_ - dt);
    farm_.setTimerFrozen(debug_.freezeTimer);
    farm_.setDebugView(debug_.showTileInfo);
    if (debugMenu_.isOpen()) {
        debugMenu_.update(dt); // the game is paused while the debug screen is open
        return;
    }
    if (state_ == State::Farming || state_ == State::DaySummary) dt *= debug_.gameSpeed;

    switch (state_) {
    case State::MainMenu:
    case State::SlotSelect:
        mainMenu_.update(dt);
        break;
    case State::Settings:
        if (settingsFrom_ == State::MainMenu) mainMenu_.update(dt);
        break;
    case State::Farming:
        farm_.update(dt, mouseX_, mouseY_, pointerActive(), coins_, rng_);
        playHarvestSounds();
        autosaveTimer_ += dt;
        if (farm_.dayOver()) endDay();
        else if (autosaveTimer_ >= kAutosaveSeconds) saveGame();
        break;
    case State::DaySummary:
        summaryTimer_ += dt;
        farm_.update(dt, -1000.f, -1000.f, false, coins_, rng_); // let particles finish
        break;
    case State::TechTree:
        treeScreen_.update(dt);
        break;
    case State::Paused:
        break;
    }
}

void Game::playHarvestSounds() {
    std::vector<Crop> picked = farm_.takeHarvests();
    std::uniform_real_distribution<float> jitter(0.94f, 1.06f);
    int played = 0;
    for (Crop c : picked) {
        if (played++ >= 3) break; // Wide Reach can pick lots at once; don't deafen anyone
        float pitch = c == Crop::Lettuce ? 1.f : (c == Crop::Carrot ? 0.86f : 0.72f);
        audio_.play(Sfx::Pick, pitch * jitter(rng_));
        audio_.play(Sfx::Coin, jitter(rng_), 0.45f);
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void Game::render(bool present) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    switch (state_) {
    case State::MainMenu:
        mainMenu_.render(renderer_);
        break;
    case State::SlotSelect:
        slotMenu_.render(renderer_);
        break;
    case State::Settings:
        if (settingsFrom_ == State::MainMenu) mainMenu_.renderBackground(renderer_);
        else renderPlayScene(pausedFrom_);
        settingsMenu_.render(renderer_);
        break;
    case State::Paused:
        renderPlayScene(pausedFrom_);
        pauseMenu_.render(renderer_);
        break;
    default:
        renderPlayScene(state_);
        break;
    }

    if (toastTime_ > 0.f) {
        Uint8 a = static_cast<Uint8>(255 * std::min(1.f, toastTime_ * 2.f));
        float w = draw::textWidth(toast_, 2.f) + 40;
        draw::fillRoundRect(renderer_, (kWidth - w) * 0.5f, 84, w, 40, 12, SDL_Color{0, 0, 0, static_cast<Uint8>(a * 0.7f)});
        draw::text(renderer_, kWidth * 0.5f, 96, toast_, 2.f, SDL_Color{255, 255, 255, a}, draw::Align::Center);
    }

    if (kDebugTools) {
        if (debug_.showFps) {
            draw::textShadow(renderer_, 56, 686, draw::strf("%.0f fps", fps_), 1.5f, SDL_Color{150, 255, 150, 255});
        }
        if (debugMenu_.isOpen()) {
            debugMenu_.render(renderer_);
        } else {
            bool hover = !usingTouch_ && draw::pointInRect(mouseX_, mouseY_, DebugMenu::buttonRect());
            DebugMenu::drawOpenButton(renderer_, hover);
        }
    }

    if (present) SDL_RenderPresent(renderer_);
}

void Game::renderPlayScene(State s) {
    if (s == State::TechTree) {
        treeScreen_.render(renderer_, tree_, coins_, day_);
    } else {
        renderBackground();
        farm_.render(renderer_);
        renderHud();
        if (s == State::DaySummary) renderSummary();
    }
    if (s == State::Farming || s == State::TechTree) {
        bool hover = !usingTouch_ && draw::pointInRect(mouseX_, mouseY_, pauseButton_);
        ui::drawPauseIcon(renderer_, pauseButton_, hover && state_ == s);
    }
    if (s == State::Farming) {
        bool live = state_ == s; // not while paused
        bool hover = live && !usingTouch_ && draw::pointInRect(mouseX_, mouseY_, endDayButton_);
        ui::Button b;
        b.rect = endDayButton_;
        b.label = "End Day";
        b.style = ui::Style::Secondary;
        b.textScale = 2.f;
        ui::drawButton(renderer_, b, hover, live && endDayButtonDown_ && hover);
    }
}

void Game::renderBackground() { art::draw(renderer_, "farm/background", SDL_FRect{0, 0, kWidth, kHeight}); }

void Game::renderHud() {
    art::draw(renderer_, "farm/hud_bar", SDL_FRect{0, 0, kWidth, 72});
    art::draw(renderer_, "ui/hud_logo", SDL_FRect{24, 10, 336, 30});
    draw::text(renderer_, 26, 48, draw::strf("Day %d   -   Slot %d", day_, currentSlot_ + 1), 1.5f, kGrey);

    // Day/night dial: the sun crosses from sunrise (left) to sunset (right) as the day runs out.
    const float dayT = farm_.dayLength() > 0 ? 1.f - std::clamp(farm_.timeLeft() / farm_.dayLength(), 0.f, 1.f) : 1.f;
    ui::drawDayDial(renderer_, kDialX, kDialHorizon, kDialRadius, dayT);
    draw::textShadow(renderer_, kDialX + kDialRadius + 20, 14, draw::strf("%.1fs", farm_.timeLeft()), 2.5f, kWhite);
    draw::text(renderer_, kDialX + kDialRadius + 22, 42, "until sunset", 1.f, kGrey);

    // Coins.
    ui::drawCoin(renderer_, 930, 30, 13);
    draw::textShadow(renderer_, 952, 19, draw::number(coins_), 3.f, kGold);
    draw::text(renderer_, 952, 50, "+" + draw::number(farm_.earnedToday()) + " today", 1.5f, kGrey);

    if (state_ == State::Farming && day_ <= 2) {
        const char* hint = usingTouch_ ? "Hold your finger on ripe vegetables to pick them!"
                                       : "Hover over ripe vegetables to pick them!";
        draw::textShadow(renderer_, kWidth * 0.5f, kHeight - 34, hint, 2.f, kWhite, draw::Align::Center);
    }
}

void Game::renderSummary() {
    Uint8 fade = static_cast<Uint8>(150 * std::min(1.f, summaryTimer_ * 3.f));
    draw::fillRect(renderer_, 0, 0, kWidth, kHeight, SDL_Color{0, 0, 0, fade});

    const float w = 560, h = 360, x = (kWidth - w) * 0.5f, y = 180;
    ui::drawPanel(renderer_, SDL_FRect{x, y, w, h});
    art::draw(renderer_, "ui/panel_header", SDL_FRect{x, y, w, 70});
    draw::textShadow(renderer_, kWidth * 0.5f, y + 22, draw::strf("Sunset - Day %d done!", day_ - 1), 3.f, kWhite,
                     draw::Align::Center);

    const SDL_Color ink{70, 50, 35, 255};
    float ty = y + 100;
    draw::text(renderer_, x + 50, ty, draw::strf("Vegetables picked: %d", farm_.pickedToday()), 2.f, ink);
    ty += 30;
    const Crop crops[] = {Crop::Lettuce, Crop::Carrot, Crop::Pumpkin};
    for (Crop c : crops) {
        if (farm_.pickedOf(c) == 0) continue;
        draw::text(renderer_, x + 80, ty, draw::strf("%s x%d", cropInfo(c).name, farm_.pickedOf(c)), 1.5f,
                   SDL_Color{120, 95, 70, 255});
        ty += 22;
    }
    ty += 10;
    draw::text(renderer_, x + 50, ty, "Coins earned: " + draw::number(farm_.earnedToday()), 2.f, ink);
    ty += 30;
    draw::text(renderer_, x + 50, ty, "Total coins:  " + draw::number(coins_), 2.f, ink);

    bool ready = summaryTimer_ >= 0.6f;
    bool over = ready && !usingTouch_ && draw::pointInRect(mouseX_, mouseY_, summaryButton_);
    ui::Button b;
    b.rect = summaryButton_;
    b.label = "Tech Tree >";
    b.style = ui::Style::Secondary;
    b.enabled = ready;
    ui::drawButton(renderer_, b, over, false);
}

// ---------------------------------------------------------------------------
// Debug screen
// ---------------------------------------------------------------------------

Stats Game::currentStats() const {
    Stats s = tree_.computeStats();
    if (debug_.dayLengthOverride > 0.f) s.dayLength = debug_.dayLengthOverride;
    if (debug_.instantGrow) s.growTime = 0.001f;
    if (debug_.instantPick) {
        s.pickTime = 0.001f;
        s.farmerPickTime = 0.001f;
    }
    return s;
}

bool Game::dayRunning() const {
    State playing = state_;
    if (state_ == State::Paused) playing = pausedFrom_;
    if (state_ == State::Settings && settingsFrom_ == State::Paused) playing = pausedFrom_;
    return playing == State::Farming && !farm_.dayOver();
}

const char* Game::stateName(State s) const {
    switch (s) {
    case State::MainMenu: return "Main menu";
    case State::SlotSelect: return "Slot select";
    case State::Settings: return "Settings";
    case State::Farming: return "Farming";
    case State::DaySummary: return "Day summary";
    case State::TechTree: return "Tech tree";
    case State::Paused: return "Paused";
    }
    return "?";
}

std::vector<std::string> Game::debugInfo() const {
    std::vector<std::string> lines;
    lines.push_back(draw::strf("FPS: %.0f", fps_));
    lines.push_back(std::string("Screen: ") + stateName(state_) +
                    (state_ == State::Paused ? std::string(" (over ") + stateName(pausedFrom_) + ")" : ""));
    if (inGame()) {
        lines.push_back(draw::strf("Slot %d   Day %d   Coins %.2f   Lifetime %.2f", currentSlot_ + 1, day_, coins_,
                                   lifetimeCoins_));
        Stats st = currentStats();
        lines.push_back(draw::strf("Crops %d of %d (patch room %d)   Day %.1fs   Grow %.3fs   Pick %.3fs", Farm::plantCount(st), st.maxCrops, st.patchSize * st.patchSize,
                                   st.dayLength, st.growTime, st.pickTime));
        lines.push_back(draw::strf("Value x%.2f   Reach %d   Crop tier %d   Head start %.0f%%", st.valueMult, st.reach,
                                   st.cropTier, st.headStart * 100.f));
        lines.push_back(draw::strf("Auto-pick %.0f%% chance, up to %d crops within %.1f plants", st.autoPickChance,
                                   st.autoPickCount, st.autoPickRadius));
        lines.push_back(draw::strf("Farmers %d   walk %.2f plants/s   pick %.2fs", st.farmers, st.farmerSpeed,
                                   st.farmerPickTime));
        if (dayRunning())
            lines.push_back(draw::strf("Today: %.1fs left, %d picked, %.2f coins earned", farm_.timeLeft(),
                                       farm_.pickedToday(), farm_.earnedToday()));
    } else {
        lines.push_back("No farm loaded");
    }
    lines.push_back(draw::strf("Game speed %gx%s%s%s", debug_.gameSpeed, debug_.freezeTimer ? "   timer frozen" : "",
                               debug_.instantGrow ? "   instant grow" : "", debug_.instantPick ? "   instant pick" : ""));
    lines.push_back(std::string("Platform: ") + SDL_GetPlatform() + "   Renderer: " +
                    (SDL_GetRendererName(renderer_) ? SDL_GetRendererName(renderer_) : "?"));
    if (art::assetDir().empty()) {
        lines.push_back("Artwork: no assets folder found (built-in art only)");
    } else {
        lines.push_back(draw::strf("Artwork: %d image(s) loaded - F5 reloads", art::loadedCount()));
        lines.push_back("  from " + art::assetDir());
        for (const auto& f : art::unknownFiles()) lines.push_back("  Unused (name typo?): " + f);
    }
    if (char* pref = SDL_GetPrefPath("IncraVegetable", "IncraVegetable")) {
        lines.push_back("Save folder:");
        std::string path = pref;
        for (size_t i = 0; i < path.size(); i += 62) lines.push_back("  " + path.substr(i, 62)); // wrap long paths
        SDL_free(pref);
    }
    return lines;
}

void Game::setupDebugMenu() {
    DebugMenu::Hooks h;
    h.coins = &coins_;
    h.lifetimeCoins = &lifetimeCoins_;
    h.day = &day_;
    h.tree = &tree_;
    h.farm = &farm_;
    h.options = &debug_;
    h.inGame = [this] { return inGame(); };
    h.dayRunning = [this] { return dayRunning(); };
    h.stats = [this] { return currentStats(); };
    h.statsChanged = [this] {
        if (dayRunning()) farm_.applyStats(currentStats(), rng_);
    };
    h.endDay = [this] {
        if (!dayRunning()) return;
        farm_.setTimeLeft(0.f);
        state_ = State::Farming; // leave any pause/settings screen so the day can finish
        debugMenu_.close();
    };
    h.restartDay = [this] {
        if (!inGame()) return;
        startDay();
        debugMenu_.close();
    };
    h.saveNow = [this] {
        if (currentSlot_ < 0 || !inGame()) return false;
        saveGame();
        return true;
    };
    h.reloadSave = [this] {
        if (currentSlot_ < 0) return false;
        if (!loadGame(currentSlot_)) return false;
        debugMenu_.close();
        return true;
    };
    h.info = [this] { return debugInfo(); };
    debugMenu_.setHooks(std::move(h));
}

// F1 toggles the debug screen; so does the bug button in the corner.
// Returns true if the event was used.
bool Game::handleDebugInput(const SDL_Event& e) {
    if (!kDebugTools) return false;
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F1 && !e.key.repeat) {
        if (debugMenu_.isOpen()) debugMenu_.close();
        else debugMenu_.open(window_);
        audio_.play(Sfx::Click);
        return true;
    }
    if (debugMenu_.isOpen()) {
        debugMenu_.handleEvent(e);
        if (!debugMenu_.isOpen()) lastTicks_ = SDL_GetTicksNS();
        return true;
    }
    SDL_FRect b = DebugMenu::buttonRect();
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
        draw::pointInRect(e.button.x, e.button.y, b)) {
        debugButtonDown_ = true;
        return true;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && debugButtonDown_) {
        debugButtonDown_ = false;
        if (draw::pointInRect(e.button.x, e.button.y, b)) {
            audio_.play(Sfx::Click);
            touchDown_ = false;
            debugMenu_.open(window_);
        }
        return true;
    }
    return false;
}
