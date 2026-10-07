#include "Menus.h"

#include "Art.h"
#include "Draw.h"
#include "Farm.h"
#include "Platform.h"

#include <algorithm>
#include <cmath>

namespace {
const SDL_Color kWhite{255, 255, 255, 255};
const SDL_Color kInk{70, 50, 35, 255};
const SDL_Color kInkSoft{125, 100, 75, 255};

bool isBack(const SDL_Event& e) {
    return e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK);
}
} // namespace

// ===========================================================================
// Main menu
// ===========================================================================

void MainMenu::refresh(bool hasSave, const std::string& continueInfo) {
    buttons_.buttons.clear();
    actions_.clear();
    const float w = 380.f, h = 62.f, x = 640.f - w * 0.5f;
    float y = 236.f;
    auto add = [&](const std::string& label, Action a, ui::Style style, bool enabled = true,
                   const std::string& sub = "") {
        ui::Button b;
        b.rect = {x, y, w, h};
        b.label = label;
        b.subLabel = sub;
        b.style = style;
        b.enabled = enabled;
        buttons_.buttons.push_back(b);
        actions_.push_back(a);
        y += h + 16.f;
    };
    add("Continue", Action::Continue, ui::Style::Secondary, hasSave, hasSave ? continueInfo : "No saved farm yet");
    add("New Game", Action::NewGame, ui::Style::Primary);
    add("Load Game", Action::LoadGame, ui::Style::Primary, hasSave);
    add("Settings", Action::Settings, ui::Style::Ghost);
    if (kIsDesktop) add("Quit", Action::Quit, ui::Style::Ghost);
}

MainMenu::Action MainMenu::handleEvent(const SDL_Event& e) {
    if (kIsDesktop && isBack(e)) return Action::Quit;
    int i = buttons_.handleEvent(e);
    return i >= 0 ? actions_[i] : Action::None;
}

void MainMenu::drawBackgroundBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float sx = rc.w / 1280.f, sy = rc.h / 720.f;
    // Sunset sky.
    const SDL_Color top{255, 170, 90, 255}, bottom{255, 220, 150, 255};
    for (int y = 0; y < 440; y += 8) {
        draw::fillRect(r, rc.x, rc.y + y * sy, rc.w, 8 * sy + 1, draw::lerp(top, bottom, y / 440.f));
    }
    draw::fillCircle(r, rc.x + 1040 * sx, rc.y + 300 * sy, 90 * sy, SDL_Color{255, 240, 190, 255}, 48);
    // Distant hills.
    draw::fillEllipse(r, rc.x + 250 * sx, rc.y + 470 * sy, 520 * sx, 110 * sy, SDL_Color{120, 175, 90, 255}, 48);
    draw::fillEllipse(r, rc.x + 1050 * sx, rc.y + 480 * sy, 560 * sx, 120 * sy, SDL_Color{105, 165, 80, 255}, 48);
    // Field.
    draw::fillRect(r, rc.x, rc.y + 440 * sy, rc.w, 280 * sy, SDL_Color{86, 150, 70, 255});
}

void MainMenu::drawLogoBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float scale = std::min(7.f, rc.w / draw::textWidth("IncraVegetable", 1.f));
    float total = draw::textWidth("IncraVegetable", scale);
    float x = rc.x + (rc.w - total) * 0.5f, y = rc.y + 10.f * scale / 7.f;
    draw::text(r, x + 6, y + 6, "IncraVegetable", scale, SDL_Color{90, 40, 20, 160});
    draw::text(r, x, y, "Incra", scale, SDL_Color{70, 165, 60, 255});
    draw::text(r, x + draw::textWidth("Incra", scale), y, "Vegetable", scale, SDL_Color{235, 110, 30, 255});
    draw::text(r, rc.x + rc.w * 0.5f, rc.y + 86.f * scale / 7.f, "an incremental farming game", 2.f * scale / 7.f,
               SDL_Color{120, 60, 30, 255}, draw::Align::Center);
}

void MainMenu::renderBackground(SDL_Renderer* r) const {
    art::draw(r, "menu/background", SDL_FRect{0, 0, 1280, 720});
    if (!art::option("menu_crop_rows", true)) return;

    // Rows of vegetables slowly drifting past, bigger ones nearer the front.
    struct Row {
        float y, size, speed;
        SDL_Color soil;
    };
    const Row rows[] = {
        {470.f, 38.f, 14.f, SDL_Color{120, 82, 50, 255}},
        {540.f, 56.f, 22.f, SDL_Color{112, 74, 46, 255}},
        {640.f, 84.f, 34.f, SDL_Color{100, 66, 40, 255}},
    };
    const Crop pattern[] = {0, 1, 0, 2, 1, 0}; // (wrapped to however many crops there are)
    for (int ri = 0; ri < 3; ++ri) {
        const Row& row = rows[ri];
        float spacing = row.size * 1.5f;
        draw::fillRect(r, 0, row.y + row.size * 0.25f, 1280, row.size * 0.3f, row.soil);
        float offset = std::fmod(clock_ * row.speed, spacing);
        int first = static_cast<int>(std::floor(clock_ * row.speed / spacing));
        for (int k = -1; k * spacing < 1280 + spacing; ++k) {
            float x = k * spacing - offset + spacing * 0.5f;
            int idx = ((k + first + ri * 2) % 6 + 6) % 6;
            float bob = std::sin(clock_ * 2.f + k * 1.3f + ri) * row.size * 0.03f;
            Farm::drawCrop(r, pattern[idx] % std::max(1, cropCount()), x, row.y + bob, row.size);
        }
    }
}

void MainMenu::render(SDL_Renderer* r) const {
    renderBackground(r);

    // Title with a little bounce.
    float bounce = std::sin(clock_ * 1.8f) * 4.f;
    art::draw(r, "menu/logo", SDL_FRect{190.f, 64.f + bounce, 900.f, 112.f});

    buttons_.render(r);
    draw::text(r, 1270, 704, std::string("v") + kGameVersion, 1.5f, SDL_Color{255, 255, 255, 180}, draw::Align::Right);
}

// ===========================================================================
// Save slot picker
// ===========================================================================

void SlotMenu::open(Mode mode, const std::array<SlotInfo, SaveSystem::kSlotCount>& slots) {
    mode_ = mode;
    slots_ = slots;
    confirming_ = -1;
    chosen_ = -1;
    buildButtons();
}

void SlotMenu::buildButtons() {
    buttons_.buttons.clear();
    for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
        const SlotInfo& s = slots_[i];
        ui::Button b;
        b.rect = {290.f, 160.f + i * 135.f, 700.f, 112.f};
        b.style = s.exists ? ui::Style::Secondary : ui::Style::Ghost;
        b.enabled = s.exists || mode_ == Mode::NewGame;
        b.textScale = 3.f;
        b.subScale = 1.75f;
        b.label = draw::strf("Slot %d", i + 1);
        if (s.exists) {
            b.subLabel = draw::strf("Day %d   -   %s coins", s.day, draw::number(s.coins).c_str());
            std::string when = SaveSystem::formatTime(s.savedAt);
            if (!when.empty()) b.subLabel += "   -   saved " + when;
        } else {
            b.subLabel = mode_ == Mode::NewGame ? "Empty - start a new farm here" : "Empty";
        }
        buttons_.buttons.push_back(b);
    }
    ui::Button back;
    back.rect = {64.f, 630.f, 200.f, 58.f};
    back.label = "< Back";
    back.style = ui::Style::Ghost;
    buttons_.buttons.push_back(back);

    confirmButtons_.buttons.clear();
    ui::Button yes;
    yes.rect = {400.f, 420.f, 220.f, 60.f};
    yes.label = "Overwrite";
    yes.style = ui::Style::Danger;
    ui::Button no;
    no.rect = {660.f, 420.f, 220.f, 60.f};
    no.label = "Cancel";
    no.style = ui::Style::Ghost;
    confirmButtons_.buttons = {yes, no};
}

SlotMenu::Action SlotMenu::handleEvent(const SDL_Event& e) {
    if (confirming_ >= 0) {
        if (isBack(e)) {
            confirming_ = -1;
            return Action::None;
        }
        int i = confirmButtons_.handleEvent(e);
        if (i == 0) {
            chosen_ = confirming_;
            confirming_ = -1;
            return Action::Chosen;
        }
        if (i == 1) confirming_ = -1;
        return Action::None;
    }

    if (isBack(e)) return Action::Back;
    int i = buttons_.handleEvent(e);
    if (i < 0) return Action::None;
    if (i == SaveSystem::kSlotCount) return Action::Back;
    if (mode_ == Mode::NewGame && slots_[i].exists) {
        confirming_ = i; // ask before wiping an existing farm
        confirmButtons_.clearPress();
        return Action::None;
    }
    chosen_ = i;
    return Action::Chosen;
}

void SlotMenu::render(SDL_Renderer* r) const {
    art::draw(r, "menu/slots_background", SDL_FRect{0, 0, 1280, 720});

    const char* title = mode_ == Mode::NewGame ? "New Game" : "Load Game";
    draw::textShadow(r, 640, 50, title, 5.f, kWhite, draw::Align::Center);
    draw::text(r, 640, 108, mode_ == Mode::NewGame ? "Choose a slot for your new farm" : "Choose a farm to load", 2.f,
               SDL_Color{200, 225, 190, 255}, draw::Align::Center);
    buttons_.render(r);

    if (confirming_ >= 0) {
        ui::dim(r, 150);
        SDL_FRect panel{340.f, 220.f, 600.f, 290.f};
        ui::drawPanel(r, panel);
        draw::text(r, 640, 255, draw::strf("Overwrite Slot %d?", confirming_ + 1), 3.f, kInk, draw::Align::Center);
        const SlotInfo& s = slots_[confirming_];
        draw::text(r, 640, 315, draw::strf("The farm on day %d with %s coins", s.day, draw::number(s.coins).c_str()),
                   1.75f, kInkSoft, draw::Align::Center);
        draw::text(r, 640, 345, "will be lost for good.", 1.75f, kInkSoft, draw::Align::Center);
        confirmButtons_.render(r);
    }
}

// ===========================================================================
// Settings
// ===========================================================================

void SettingsMenu::open(Settings* settings, std::vector<Resolution> resolutions) {
    settings_ = settings;
    resolutions_ = std::move(resolutions);
    dragging_ = None;
    buildButtons();
}

void SettingsMenu::buildButtons() {
    buttons_.buttons.clear();
    if (kIsDesktop) {
        bool windowed = !settings_->fullscreen;
        int cur = currentResolution();
        ui::Button prev;
        prev.rect = {560.f, 372.f, 56.f, 52.f};
        prev.label = "<";
        prev.style = ui::Style::Ghost;
        prev.enabled = windowed && cur > 0;
        ui::Button next = prev;
        next.rect = {880.f, 372.f, 56.f, 52.f};
        next.label = ">";
        next.enabled = windowed && cur + 1 < static_cast<int>(resolutions_.size());
        ui::Button fs;
        fs.rect = {560.f, 452.f, 376.f, 52.f};
        fs.label = settings_->fullscreen ? "On" : "Off";
        fs.style = settings_->fullscreen ? ui::Style::Secondary : ui::Style::Ghost;
        buttons_.buttons = {prev, next, fs};
    }
    ui::Button back;
    back.rect = {640.f - 130.f, kIsDesktop ? 548.f : 420.f, 260.f, 60.f};
    back.label = "Done";
    back.style = ui::Style::Primary;
    buttons_.buttons.push_back(back);
}

SDL_FRect SettingsMenu::sliderTrack(int which) const {
    return SDL_FRect{560.f, which == Music ? 233.f : 313.f, 300.f, 14.f};
}

float* SettingsMenu::sliderValue(int which) const {
    return which == Music ? &settings_->musicVolume : &settings_->sfxVolume;
}

int SettingsMenu::currentResolution() const {
    for (int i = 0; i < static_cast<int>(resolutions_.size()); ++i) {
        if (resolutions_[i].w == settings_->windowWidth && resolutions_[i].h == settings_->windowHeight) return i;
    }
    return 0;
}

SettingsMenu::Action SettingsMenu::handleEvent(const SDL_Event& e) {
    if (!settings_) return Action::None;
    if (isBack(e)) return Action::Back;

    // Sliders.
    auto valueAt = [&](int which, float x) {
        SDL_FRect t = sliderTrack(which);
        return std::clamp((x - t.x) / t.w, 0.f, 1.f);
    };
    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        if (dragging_ != None) {
            *sliderValue(dragging_) = valueAt(dragging_, e.motion.x);
            buttons_.handleEvent(e);
            return dragging_ == Music ? Action::MusicChanged : Action::SfxChanged;
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        for (int which : {Music, Sfx}) {
            SDL_FRect t = sliderTrack(which);
            SDL_FRect hit{t.x - 20.f, t.y - 22.f, t.w + 40.f, t.h + 44.f};
            if (draw::pointInRect(e.button.x, e.button.y, hit)) {
                dragging_ = which;
                *sliderValue(which) = valueAt(which, e.button.x);
                return which == Music ? Action::MusicChanged : Action::SfxChanged;
            }
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && dragging_ != None) {
        int was = dragging_;
        dragging_ = None;
        buttons_.handleEvent(e);
        return was == Sfx ? Action::SfxReleased : Action::None;
    }

    int i = buttons_.handleEvent(e);
    if (i < 0) return Action::None;
    if (i == static_cast<int>(buttons_.buttons.size()) - 1) return Action::Back;

    // Desktop-only buttons: 0 = previous resolution, 1 = next, 2 = fullscreen.
    if (i == 0 || i == 1) {
        int cur = std::clamp(currentResolution() + (i == 0 ? -1 : 1), 0, static_cast<int>(resolutions_.size()) - 1);
        settings_->windowWidth = resolutions_[cur].w;
        settings_->windowHeight = resolutions_[cur].h;
    } else if (i == 2) {
        settings_->fullscreen = !settings_->fullscreen;
    }
    buildButtons();
    return Action::DisplayChanged;
}

void SettingsMenu::render(SDL_Renderer* r) const {
    if (!settings_) return;
    ui::dim(r, 140);
    SDL_FRect panel{290.f, 90.f, 700.f, kIsDesktop ? 550.f : 420.f};
    ui::drawPanel(r, panel);
    draw::text(r, 640, 128, "Settings", 4.f, kInk, draw::Align::Center);

    auto label = [&](float y, const char* text) { draw::text(r, 340, y, text, 2.25f, kInk); };

    for (int which : {Music, Sfx}) {
        SDL_FRect t = sliderTrack(which);
        float v = *sliderValue(which);
        label(t.y - 2, which == Music ? "Music" : "Sound FX");
        art::draw(r, "ui/slider_track", t);
        if (v > 0.f && !art::drawFill(r, "ui/slider_fill", t, v))
            drawSliderBarBuiltin(r, SDL_FRect{t.x, t.y, std::max(t.h, t.w * v), t.h}, true);
        float hx = t.x + t.w * v, hy = t.y + t.h * 0.5f;
        bool active = dragging_ == which;
        float knob = active ? 22.f : 20.f;
        art::draw(r, "ui/slider_knob", SDL_FRect{hx - knob, hy - knob, knob * 2, knob * 2});
        draw::text(r, 886, t.y - 2, v <= 0.001f ? "Off" : draw::strf("%d%%", static_cast<int>(std::lround(v * 100))),
                   2.f, kInkSoft);
    }

    if (kIsDesktop) {
        label(388, "Resolution");
        const auto& res = resolutions_[std::clamp(currentResolution(), 0, static_cast<int>(resolutions_.size()) - 1)];
        std::string text = settings_->fullscreen ? "Full screen" : draw::strf("%d x %d", res.w, res.h);
        draw::text(r, 748, 389, text, 2.f, settings_->fullscreen ? kInkSoft : kInk, draw::Align::Center);
        label(468, "Fullscreen");
    }
    buttons_.render(r);
}

// ===========================================================================
// Pause menu
// ===========================================================================

void PauseMenu::open() {
    buttons_.buttons.clear();
    actions_.clear();
    float y = 250.f;
    auto add = [&](const char* label, Action a, ui::Style style) {
        ui::Button b;
        b.rect = {640.f - 170.f, y, 340.f, 60.f};
        b.label = label;
        b.style = style;
        buttons_.buttons.push_back(b);
        actions_.push_back(a);
        y += 76.f;
    };
    add("Resume", Action::Resume, ui::Style::Secondary);
    add("Settings", Action::Settings, ui::Style::Ghost);
    add("Main Menu", Action::MainMenu, ui::Style::Primary);
    if (kIsDesktop) add("Quit Game", Action::Quit, ui::Style::Danger);
}

PauseMenu::Action PauseMenu::handleEvent(const SDL_Event& e) {
    if (isBack(e)) return Action::Resume;
    int i = buttons_.handleEvent(e);
    return i >= 0 ? actions_[i] : Action::None;
}

void PauseMenu::render(SDL_Renderer* r) const {
    ui::dim(r, 150);
    SDL_FRect panel{640.f - 230.f, 140.f, 460.f, kIsDesktop ? 450.f : 374.f};
    ui::drawPanel(r, panel);
    draw::text(r, 640, 175, "Paused", 4.f, kInk, draw::Align::Center);
    draw::text(r, 640, 218, "Your farm is saved automatically", 1.5f, kInkSoft, draw::Align::Center);
    buttons_.render(r);
}

// ===========================================================================
// Built-in art (used when there's no image in assets/, and for the templates)
// ===========================================================================

void SlotMenu::drawBackgroundBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{40, 70, 38, 255});
    float sy = rc.h / 720.f;
    for (int y = 0; y < 720; y += 48)
        draw::fillRect(r, rc.x, rc.y + y * sy, rc.w, 22 * sy, SDL_Color{46, 78, 43, 255});
}

void SettingsMenu::drawSliderBarBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool fill) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rc.h * 0.5f,
                        fill ? SDL_Color{240, 150, 50, 255} : SDL_Color{200, 180, 150, 255});
}

void SettingsMenu::drawKnobBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f, k = rc.w / 40.f;
    draw::fillCircle(r, cx, cy + 2 * k, 17 * k, SDL_Color{0, 0, 0, 60});
    draw::fillCircle(r, cx, cy, 16 * k, SDL_Color{255, 255, 255, 255});
    draw::fillCircle(r, cx, cy, 9 * k, SDL_Color{240, 150, 50, 255});
}
