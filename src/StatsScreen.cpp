#include "StatsScreen.h"

#include "Art.h"
#include "Draw.h"
#include "Palette.h"

#include <algorithm>

namespace {
const SDL_Color kInk = pal::DeepSoil;
const SDL_Color kInkSoft = pal::Soil;
const SDL_Color kHeading = pal::Burnt;

constexpr float kPanelW = 620.f, kMaxRowH = 27.f;
} // namespace

void StatsScreen::open(std::vector<Row> rows) {
    rows_ = std::move(rows);
    buttons_.buttons.clear();
    ui::Button b;
    b.label = "Back";
    b.style = ui::Style::Secondary;
    b.textScale = 2.f;
    buttons_.buttons.push_back(b);
}

bool StatsScreen::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK || e.key.key == SDLK_TAB))
        return true;
    return buttons_.handleEvent(e) == 0;
}

void StatsScreen::render(SDL_Renderer* r) const {
    ui::dim(r, 150);
    // Rows squeeze together a little if there are lots of them, so the page always fits.
    const float n = static_cast<float>(std::max<size_t>(1, rows_.size()));
    const float rowH = std::min(kMaxRowH, (700.f - 186.f) / n);
    const float h = 96.f + rowH * n + 90.f;
    const SDL_FRect panel{640.f - kPanelW * 0.5f, std::max(10.f, 86.f - std::max(0.f, h - 620.f)), kPanelW, h};
    ui::drawPanel(r, panel);
    draw::text(r, 640, panel.y + 30, "Stats", 4.f, kInk, draw::Align::Center);

    const float left = panel.x + 56.f, right = panel.x + panel.w - 56.f;
    float y = panel.y + 92.f;
    for (const Row& row : rows_) {
        if (row.heading) {
            draw::text(r, left, y + 6, row.label, 1.5f, kHeading);
        } else {
            const float scale = row.indent ? 1.5f : 2.f;
            const SDL_Color c = row.indent ? kInkSoft : kInk;
            draw::text(r, left + (row.indent ? 24.f : 0.f), y + (row.indent ? 4.f : 0.f), row.label, scale, c);
            draw::text(r, right, y + (row.indent ? 4.f : 0.f), row.value, scale, c, draw::Align::Right);
        }
        y += rowH;
    }

    auto& back = const_cast<ui::ButtonList&>(buttons_).buttons[0];
    back.rect = SDL_FRect{640.f - 120.f, panel.y + panel.h - 76.f, 240.f, 54.f};
    buttons_.render(r);
}

void StatsScreen::drawButton(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    art::drawVariant(r, "ui/stats_button", hovered ? "_hover" : "", rc);
}

// Matches the built-in pause button: a rounded square with three rising bars.
void StatsScreen::drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rc.w * 0.25f,
                        hovered ? SDL_Color{90, 100, 90, 230} : SDL_Color{50, 58, 50, 210});
    const float bw = rc.w * 0.14f, base = rc.y + rc.h * 0.74f, gap = rc.w * 0.06f;
    const float x0 = rc.x + rc.w * 0.5f - bw * 1.5f - gap;
    const float heights[3] = {0.22f, 0.36f, 0.5f};
    for (int i = 0; i < 3; ++i) {
        const float bh = rc.h * heights[i];
        draw::fillRoundRect(r, x0 + i * (bw + gap), base - bh, bw, bh, 2, SDL_Color{240, 240, 230, 255});
    }
}
