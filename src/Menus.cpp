#include "Menus.h"

#include "Art.h"
#include "Draw.h"
#include "Screen.h"
#include "Palette.h"
#include "Farm.h"
#include "Grass.h"
#include "Platform.h"

#include <algorithm>
#include <cmath>

namespace {
// Text colours, from the palette (Palette.h).
const SDL_Color kWhite = pal::Cream;
const SDL_Color kInk = pal::DeepSoil;
const SDL_Color kInkSoft = pal::Tilled;

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
    draw::text(r, x + 6, y + 6, "IncraVegetable", scale, pal::alpha(pal::DeepSoil, 160));
    draw::text(r, x, y, "Incra", scale, pal::FreshLeaf);
    draw::text(r, x + draw::textWidth("Incra", scale), y, "Vegetable", scale, pal::Pumpkin);
    draw::text(r, rc.x + rc.w * 0.5f, rc.y + 86.f * scale / 7.f, "an incremental farming game", 2.f * scale / 7.f,
               SDL_Color{120, 60, 30, 255}, draw::Align::Center);
}

void MainMenu::renderBackground(SDL_Renderer* r) const {
    screen::background(r, "menu/background");
    if (!art::option("menu_crop_rows", true)) return;
    screen::Whole all(r); // the rows run right across the screen

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
    // A mix of the unlocked vegetables, the same every time.
    auto cropAt = [this](int k, int ri) {
        const int n = static_cast<int>(crops_.size());
        if (n <= 1) return crops_.empty() ? 0 : crops_[0];
        Uint32 h = static_cast<Uint32>(k) * 2654435761u ^ static_cast<Uint32>(ri + 1) * 40503u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        // Cheaper crops turn up more often, like in the patch.
        float best = -1.f;
        int pick = 0;
        for (int i = 0; i < n; ++i) {
            Uint32 hi = (h + static_cast<Uint32>(i) * 0x9e3779b9u) * 0x85ebca6bu;
            hi ^= hi >> 16;
            float roll = static_cast<float>(hi % 1000u) / 1000.f * cropDef(crops_[i]).weight;
            if (roll > best) { best = roll; pick = i; }
        }
        return crops_[pick];
    };
    for (int ri = 0; ri < 3; ++ri) {
        const Row& row = rows[ri];
        float spacing = row.size * 1.5f;
        const float y = row.y + all.oy;
        draw::fillRect(r, 0, y + row.size * 0.25f, all.w, row.size * 0.3f, row.soil);
        float offset = std::fmod(clock_ * row.speed, spacing);
        int first = static_cast<int>(std::floor(clock_ * row.speed / spacing));
        // Crops line up with the stage, so the screen's edges just show more of the row.
        const int k0 = -1 - static_cast<int>(std::ceil(all.ox / spacing));
        for (int k = k0; (k - k0 - 1) * spacing < all.w + spacing; ++k) {
            float x = k * spacing - offset + spacing * 0.5f + all.ox;
            float bob = std::sin(clock_ * 2.f + k * 1.3f + ri) * row.size * 0.03f;
            Farm::drawCrop(r, cropAt(k + first, ri), x, y + bob, row.size);
        }
    }
}

void MainMenu::render(SDL_Renderer* r) const {
    renderBackground(r);

    // Title with a little bounce.
    float bounce = std::sin(clock_ * 1.8f) * 4.f;
    art::draw(r, "menu/logo", SDL_FRect{190.f, 64.f + bounce, 900.f, 112.f});

    buttons_.render(r);
    {
        screen::Pinned bottom(r, screen::Edge::Bottom); // in the screen's corner
        draw::text(r, 1270, 704, std::string("v") + kGameVersion + (kIsDemo ? " demo" : ""), 1.5f,
                   pal::alpha(pal::Cream, 180), draw::Align::Right);
    }
    if (kIsDemo) { // a little "DEMO" tag between the logo and the buttons
        const float x = 640.f, y = 184.f + bounce;
        draw::fillRoundRect(r, x - 70, y, 140, 40, 10, pal::Carrot);
        draw::fillRoundRect(r, x - 66, y + 4, 132, 32, 8, pal::Pumpkin);
        draw::text(r, x + 2, y + 10, "DEMO", 2.5f, pal::DeepSoil, draw::Align::Center);
        draw::text(r, x, y + 8, "DEMO", 2.5f, pal::Cream, draw::Align::Center);
    }
}

// ===========================================================================
// Save slot picker
// ===========================================================================

void SlotMenu::open(Mode mode, const std::array<SlotInfo, SaveSystem::kSlotCount>& slots) {
    mode_ = mode;
    slots_ = slots;
    confirming_ = -1;
    chosen_ = -1;
    stopNaming();
    buildButtons();
}

void SlotMenu::refresh(const std::array<SlotInfo, SaveSystem::kSlotCount>& slots) {
    slots_ = slots;
    buildButtons();
}

void SlotMenu::startNaming(int slot, bool forNewFarm) {
    naming_ = slot;
    namingNew_ = forNewFarm;
    nameText_ = forNewFarm ? std::string() : slots_[slot].name;
    nameButtons_.buttons.clear();
    ui::Button ok;
    ok.rect = {400.f, 430.f, 220.f, 60.f};
    ok.label = forNewFarm ? "Start" : "Rename";
    ok.style = ui::Style::Secondary;
    ui::Button cancel;
    cancel.rect = {660.f, 430.f, 220.f, 60.f};
    cancel.label = "Cancel";
    cancel.style = ui::Style::Ghost;
    nameButtons_.buttons = {ok, cancel};
    nameButtons_.clearPress();
    SDL_StartTextInput(SDL_GetKeyboardFocus()); // (shows the on-screen keyboard on phones)
}

void SlotMenu::stopNaming() {
    if (naming_ >= 0) SDL_StopTextInput(SDL_GetKeyboardFocus());
    naming_ = -1;
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
        b.label = s.exists && !s.name.empty() ? s.name : draw::strf("Slot %d", i + 1);
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
    // A Rename button beside each farm.
    renameSlotOf_.assign(buttons_.buttons.size(), -1);
    for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
        if (!slots_[i].exists) continue;
        ui::Button rn;
        rn.rect = {1006.f, 160.f + i * 135.f + 30.f, 150.f, 52.f};
        rn.label = "Rename";
        rn.style = ui::Style::Ghost;
        rn.textScale = 2.f;
        buttons_.buttons.push_back(rn);
        renameSlotOf_.push_back(i);
    }

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
    if (naming_ >= 0) {
        auto finish = [&]() {
            chosen_ = naming_;
            chosenName_ = SaveSystem::cleanName(nameText_);
            const bool isNew = namingNew_;
            stopNaming();
            return isNew ? Action::Chosen : Action::Renamed;
        };
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (const char* c = e.text.text; *c; ++c) // plain letters, numbers and punctuation (what the font has)
                if (*c >= 32 && *c < 127 && static_cast<int>(nameText_.size()) < SaveSystem::kMaxNameLength)
                    nameText_ += *c;
            return Action::None;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !nameText_.empty()) nameText_.pop_back();
            else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) return finish();
            else if (isBack(e)) stopNaming();
            return Action::None;
        }
        int i = nameButtons_.handleEvent(e);
        if (i == 0) return finish();
        if (i == 1) stopNaming();
        return Action::None;
    }
    if (confirming_ >= 0) {
        if (isBack(e)) {
            confirming_ = -1;
            return Action::None;
        }
        int i = confirmButtons_.handleEvent(e);
        if (i == 0) { // overwrite: then name the new farm
            const int slot = confirming_;
            confirming_ = -1;
            startNaming(slot, true);
            return Action::None;
        }
        if (i == 1) confirming_ = -1;
        return Action::None;
    }

    if (isBack(e)) return Action::Back;
    int i = buttons_.handleEvent(e);
    if (i < 0) return Action::None;
    if (i == SaveSystem::kSlotCount) return Action::Back;
    if (i < static_cast<int>(renameSlotOf_.size()) && renameSlotOf_[i] >= 0) {
        startNaming(renameSlotOf_[i], false);
        return Action::None;
    }
    if (mode_ == Mode::NewGame && slots_[i].exists) {
        confirming_ = i; // ask before wiping an existing farm
        confirmButtons_.clearPress();
        return Action::None;
    }
    if (mode_ == Mode::NewGame) {
        startNaming(i, true); // name it first
        return Action::None;
    }
    chosen_ = i;
    return Action::Chosen;
}

void SlotMenu::render(SDL_Renderer* r) const {
    grass::drawField(r); // the farm's field, a little darker

    const char* title = mode_ == Mode::NewGame ? "New Game" : "Load Game";
    draw::textShadow(r, 640, 50, title, 5.f, kWhite, draw::Align::Center);
    draw::text(r, 640, 108, mode_ == Mode::NewGame ? "Choose a slot for your new farm" : "Choose a farm to load", 2.f,
               SDL_Color{200, 225, 190, 255}, draw::Align::Center);
    buttons_.render(r);

    if (confirming_ >= 0) {
        ui::dim(r, 150);
        SDL_FRect panel{340.f, 220.f, 600.f, 290.f};
        ui::drawPanel(r, panel);
        const std::string what = slots_[confirming_].name.empty() ? draw::strf("Slot %d", confirming_ + 1)
                                                                  : "\"" + slots_[confirming_].name + "\"";
        draw::text(r, 640, 255, "Overwrite " + what + "?", 3.f, kInk, draw::Align::Center);
        const SlotInfo& s = slots_[confirming_];
        draw::text(r, 640, 315, draw::strf("The farm on day %d with %s coins", s.day, draw::number(s.coins).c_str()),
                   1.75f, kInkSoft, draw::Align::Center);
        draw::text(r, 640, 345, "will be lost for good.", 1.75f, kInkSoft, draw::Align::Center);
        confirmButtons_.render(r);
    }

    if (naming_ >= 0) {
        ui::dim(r, 150);
        SDL_FRect panel{340.f, 200.f, 600.f, 320.f};
        ui::drawPanel(r, panel);
        draw::text(r, 640, 235, namingNew_ ? "Name your farm" : "Rename your farm", 3.f, kInk, draw::Align::Center);
        // The text box, with a blinking caret.
        SDL_FRect box{400.f, 300.f, 480.f, 60.f};
        draw::fillRoundRect(r, box.x, box.y, box.w, box.h, 10.f, pal::DeepSoil);
        draw::fillRoundRect(r, box.x + 3, box.y + 3, box.w - 6, box.h - 6, 8.f, pal::Parchment);
        const std::string placeholder = draw::strf("Slot %d", naming_ + 1);
        const bool empty = nameText_.empty();
        const float tx = box.x + 18, ty = box.y + 20;
        draw::text(r, tx, ty, empty ? placeholder : nameText_, 2.5f, empty ? pal::alpha(pal::Tilled, 140) : kInk);
        if ((SDL_GetTicks() / 500) % 2 == 0)
            draw::fillRect(r, tx + (empty ? 0.f : draw::textWidth(nameText_, 2.5f)) + 2, ty - 2, 3, 24, kInk);
        draw::text(r, 640, 380, draw::strf("Up to %d letters. Leave it blank for \"%s\".", SaveSystem::kMaxNameLength,
                                           placeholder.c_str()),
                   1.25f, kInkSoft, draw::Align::Center);
        nameButtons_.render(r);
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
