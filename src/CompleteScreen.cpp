#include "CompleteScreen.h"

#include "Art.h"
#include "Draw.h"
#include "Farm.h"
#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPanelW = 640.f;
} // namespace

float CompleteScreen::rnd() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return static_cast<float>(seed_ >> 8) / static_cast<float>(1u << 24);
}

void CompleteScreen::open(const std::string& title, const std::string& message, std::vector<std::string> facts) {
    title_ = title;
    message_ = message;
    facts_ = std::move(facts);
    time_ = 0.f;
    seed_ = static_cast<Uint32>(SDL_GetTicks()) | 1u;
    bits_.clear();
    for (int i = 0; i < 46; ++i) {
        Bit b;
        b.x = rnd() * 1280.f;
        b.y = -40.f - rnd() * 760.f; // spread out so they keep coming for a while
        b.vy = 70.f + rnd() * 90.f;
        b.spin = rnd() * 6.2832f;
        b.size = 34.f + rnd() * 26.f;
        b.kind = rnd() < 0.35f ? -1 : static_cast<int>(rnd() * std::max(1, cropCount()));
        bits_.push_back(b);
    }

    buttons_.buttons.clear();
    const float y = 0.f; // placed in render()
    ui::Button keep;
    keep.label = "Keep Playing";
    keep.style = ui::Style::Secondary;
    keep.textScale = 2.f;
    keep.rect = {640.f - 260.f, y, 250.f, 58.f};
    ui::Button menu = keep;
    menu.label = "Main Menu";
    menu.style = ui::Style::Primary;
    menu.rect = {640.f + 10.f, y, 250.f, 58.f};
    buttons_.buttons = {keep, menu};
}

CompleteScreen::Action CompleteScreen::handleEvent(const SDL_Event& e) {
    if (time_ < 0.8f) return Action::None; // don't let a click from buying the last upgrade go straight through
    if (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE))
        return Action::KeepPlaying;
    switch (buttons_.handleEvent(e)) {
    case 0: return Action::KeepPlaying;
    case 1: return Action::MainMenu;
    default: return Action::None;
    }
}

void CompleteScreen::update(float dt) {
    time_ += dt;
    for (Bit& b : bits_) {
        b.y += b.vy * dt;
        b.spin += dt * 1.5f;
        if (b.y > 760.f) { // round again from the top
            b.y = -50.f - rnd() * 120.f;
            b.x = rnd() * 1280.f;
        }
    }
}

void CompleteScreen::render(SDL_Renderer* r) const {
    const float in = std::min(1.f, time_ * 2.5f);
    ui::dim(r, static_cast<Uint8>(160 * in));

    // The harvest raining down behind the panel.
    for (const Bit& b : bits_) {
        const float wob = std::sin(b.spin) * 10.f;
        if (b.kind < 0) art::draw(r, "ui/coin", SDL_FRect{b.x + wob - b.size * 0.35f, b.y - b.size * 0.35f, b.size * 0.7f, b.size * 0.7f});
        else Farm::drawCrop(r, b.kind, b.x + wob, b.y, b.size);
    }

    const float lines = static_cast<float>(facts_.size());
    const float h = 250.f + lines * 26.f;
    const float slide = (1.f - in) * 40.f;
    const float w = std::max(kPanelW, draw::textWidth(title_, 3.f) + 90.f);
    const SDL_FRect panel{640.f - w * 0.5f, (720.f - h) * 0.5f + slide, w, h};
    ui::drawPanel(r, panel);
    art::draw(r, "ui/panel_header", SDL_FRect{panel.x, panel.y, panel.w, 74});
    draw::textShadow(r, 640.f, panel.y + 24, title_, 3.f, pal::Cream, draw::Align::Center);
    float y = panel.y + 100;
    draw::text(r, 640.f, y, message_, 2.f, pal::DeepSoil, draw::Align::Center);
    y += 42;
    for (const auto& f : facts_) {
        draw::text(r, 640.f, y, f, 1.5f, pal::Soil, draw::Align::Center);
        y += 26.f;
    }

    auto& list = const_cast<ui::ButtonList&>(buttons_);
    const float by = panel.y + panel.h - 84.f;
    list.buttons[0].rect.y = by;
    list.buttons[1].rect.y = by;
    list.buttons[0].enabled = list.buttons[1].enabled = time_ >= 0.8f;
    buttons_.render(r);
}
