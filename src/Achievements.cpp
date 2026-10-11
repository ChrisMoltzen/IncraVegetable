#include "Achievements.h"

#include "Art.h"
#include "Draw.h"
#include "Lang.h"
#include "Palette.h"
#include "Screen.h"

#include <algorithm>
#include <cmath>
#include <sstream>

// ---------------------------------------------------------------------------
// The list. Order is the order on the achievements page.
// ---------------------------------------------------------------------------

const std::vector<AchievementDef>& Achievements::all() {
    using P = AchievementProgress;
    static const std::vector<AchievementDef> list = {
        {"first_harvest", N_("First Harvest"), N_("Pick your first vegetable."), [](const P& p) { return p.picked >= 1; }},
        {"green_thumb", N_("Green Thumb"), N_("Pick 1,000 vegetables."), [](const P& p) { return p.picked >= 1000; }},
        {"bumper_crop", N_("Bumper Crop"), N_("Pick 50,000 vegetables."), [](const P& p) { return p.picked >= 50000; }},
        {"pocket_money", N_("Pocket Money"), N_("Earn 1,000 coins."), [](const P& p) { return p.lifetimeCoins >= 1e3; }},
        {"market_trader", N_("Market Trader"), N_("Earn 100,000 coins."), [](const P& p) { return p.lifetimeCoins >= 1e5; }},
        {"vegetable_tycoon", N_("Vegetable Tycoon"), N_("Earn 5,000,000 coins."),
         [](const P& p) { return p.lifetimeCoins >= 5e6; }},
        {"market_day", N_("Market Day"), N_("Earn 1,000 coins in a single day."),
         [](const P& p) { return p.bestDayCoins >= 1e3; }},
        {"harvest_festival", N_("Harvest Festival"), N_("Earn 25,000 coins in a single day."),
         [](const P& p) { return p.bestDayCoins >= 25e3; }},
        {"into_the_barn", N_("Into the Barn"), N_("Buy your first upgrade."), [](const P& p) { return p.upgrades >= 1; }},
        {"well_equipped", N_("Well Equipped"), N_("Buy 25 upgrades."), [](const P& p) { return p.upgrades >= 25; }},
        {"barn_raiser", N_("Barn Raiser"), N_("Buy 100 upgrades."), [](const P& p) { return p.upgrades >= 100; }},
        {"lend_a_hand", N_("Lend a Hand"), N_("Get the Helping Hand to pick for you."),
         [](const P& p) { return p.helpingHand; }},
        {"extra_hands", N_("Extra Hands"), N_("Hire your first farmer."), [](const P& p) { return p.farmers >= 1; }},
        {"full_crew", N_("Full Crew"), N_("Hire every farmer."),
         [](const P& p) { return p.maxFarmers > 0 && p.farmers >= p.maxFarmers; }},
        {"mixed_veg", N_("Mixed Veg"), N_("Grow every kind of vegetable."), [](const P& p) { return p.allCrops; }},
        {"week_on_the_farm", N_("Week on the Farm"), N_("Reach day 7."), [](const P& p) { return p.day >= 7; }},
        {"seasoned_farmer", N_("Seasoned Farmer"), N_("Reach day 50."), [](const P& p) { return p.day >= 50; }},
        {"the_barn_is_full", N_("The Barn Is Full"), N_("Buy every upgrade."), [](const P& p) { return p.allUpgrades; }},
    };
    return list;
}

std::vector<int> Achievements::check(const AchievementProgress& p) {
    std::vector<int> fresh;
    const auto& list = all();
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        if (!unlocked(i) && list[i].done(p) && unlock(i)) fresh.push_back(i);
    return fresh;
}

bool Achievements::unlock(int index) {
    if (index < 0 || index >= static_cast<int>(all().size())) return false;
    if (!got_.insert(all()[index].id).second) return false;
    popups_.push_back(index);
    return true;
}

int Achievements::unlockedCount() const {
    int n = 0;
    for (int i = 0; i < static_cast<int>(all().size()); ++i) n += unlocked(i);
    return n;
}

std::string Achievements::serialize() const {
    std::string out = "achievements 1\n";
    for (const auto& id : got_) out += "got " + id + "\n";
    return out;
}

void Achievements::parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key, id;
        ss >> key >> id;
        if (key == "got" && !id.empty()) got_.insert(id); // ids no longer in the list are kept, harmlessly
    }
}

// ---------------------------------------------------------------------------
// Banner
// ---------------------------------------------------------------------------

namespace {
constexpr float kBannerTime = 4.f;  // on screen, including sliding in and out
constexpr float kQuickTime = 2.2f; // when more are waiting
constexpr float kSlide = 0.35f;
constexpr float kBannerW = 400.f, kBannerH = 70.f;

const SDL_Color kInk = pal::DeepSoil;
const SDL_Color kInkSoft = pal::Soil;
const SDL_Color kHeading = pal::Burnt;
} // namespace

void AchievementBanner::update(float dt, Achievements& a) {
    if (current_ >= 0) {
        time_ += dt;
        // A burst of unlocks (e.g. an old farm loaded) goes by quicker.
        if (time_ >= (a.popups().empty() ? kBannerTime : kQuickTime)) current_ = -1;
    }
    if (current_ < 0 && !a.popups().empty()) {
        current_ = a.popups().front();
        a.popups().pop_front();
        time_ = 0.f;
    }
    quick_ = !a.popups().empty();
}

void AchievementBanner::render(SDL_Renderer* r) const {
    if (current_ < 0) return;
    screen::Pinned top(r, screen::Edge::Top); // just under the top bar, in the top-right corner
    // Slides in from the right, waits, slides back out.
    const float total = quick_ ? kQuickTime : kBannerTime;
    float in = std::min(1.f, time_ / kSlide), out = std::min(1.f, (total - time_) / kSlide);
    float t = std::clamp(std::min(in, out), 0.f, 1.f);
    t = 1.f - (1.f - t) * (1.f - t);
    const SDL_FRect rc{1280.f - 16.f - kBannerW * t, 84.f, kBannerW, kBannerH};
    ui::drawPanel(r, rc);
    AchievementsScreen::drawTrophy(r, SDL_FRect{rc.x + 12, rc.y + 11, 48, 48}, true, current_);
    const auto& def = Achievements::all()[current_];
    draw::text(r, rc.x + 74, rc.y + 14, tr("Achievement unlocked"), 1.25f, kHeading);
    draw::text(r, rc.x + 74, rc.y + 34, tr(def.name), 2.f, kInk);
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

void AchievementsScreen::open() {
    buttons_.buttons.clear();
    ui::Button b;
    b.label = tr("Back");
    b.style = ui::Style::Secondary;
    b.textScale = 2.f;
    buttons_.buttons.push_back(b);
}

bool AchievementsScreen::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK)) return true;
    return buttons_.handleEvent(e) == 0;
}

void AchievementsScreen::render(SDL_Renderer* r, const Achievements& a) const {
    ui::dim(r, 150);
    const auto& list = Achievements::all();
    const int n = static_cast<int>(list.size()), rows = (n + 1) / 2;
    // Two columns of cards; they squeeze together if there are lots.
    const float cardW = 470.f, gapX = 16.f, rowH = std::min(56.f, 470.f / std::max(1, rows));
    const float panelW = cardW * 2 + gapX + 64.f, panelH = 96.f + rowH * rows + 92.f;
    const SDL_FRect panel{640.f - panelW * 0.5f, std::max(8.f, 360.f - panelH * 0.5f), panelW, panelH};
    ui::drawPanel(r, panel);
    draw::text(r, 640, panel.y + 26, tr("Achievements"), 4.f, kInk, draw::Align::Center);
    draw::text(r, 640, panel.y + 66, trf("%d of %d", a.unlockedCount(), n), 1.5f, kInkSoft, draw::Align::Center);

    const float x0 = panel.x + 32.f, y0 = panel.y + 92.f;
    for (int i = 0; i < n; ++i) {
        const bool got = a.unlocked(i);
        const float x = x0 + (i % 2) * (cardW + gapX), y = y0 + (i / 2) * rowH;
        const float h = rowH - 6.f, icon = std::min(40.f, h - 6.f);
        draw::fillRoundRect(r, x, y, cardW, h, 8, got ? pal::alpha(pal::PaleGold, 150) : pal::alpha(pal::Straw, 70));
        drawTrophy(r, SDL_FRect{x + 8, y + (h - icon) * 0.5f, icon, icon}, got, i);
        const float tx = x + 16 + icon;
        const float nameScale = h >= 44.f ? 1.75f : 1.5f;
        const float textW = cardW - (tx - x) - 8.f; // long translations shrink to fit the card
        const std::string& name = tr(list[i].name);
        const std::string& desc = tr(list[i].description);
        const float ns = std::min(nameScale, textW / std::max(1.f, draw::textWidth(name, 1.f)));
        const float ds = std::min(1.25f, textW / std::max(1.f, draw::textWidth(desc, 1.f)));
        draw::text(r, tx, y + h * 0.5f - 15.f * nameScale / 1.75f, name, ns, got ? kInk : kInkSoft);
        draw::text(r, tx, y + h * 0.5f + 4.f, desc, ds, got ? kInkSoft : pal::alpha(kInkSoft, 170));
    }

    auto& back = const_cast<ui::ButtonList&>(buttons_).buttons[0];
    back.rect = SDL_FRect{640.f - 120.f, panel.y + panel.h - 74.f, 240.f, 54.f};
    buttons_.render(r);
}

std::string AchievementsScreen::artName(int index, bool got) {
    return std::string("achievements/") + Achievements::all()[index].id + (got ? "" : "_locked");
}

void AchievementsScreen::drawTrophy(SDL_Renderer* r, const SDL_FRect& rc, bool got, int index) {
    if (index >= 0 && index < static_cast<int>(Achievements::all().size())) {
        const std::string own = artName(index, true);
        if (got && art::has(own)) return art::draw(r, own, rc);
        if (!got) {
            const std::string locked = artName(index, false);
            if (art::has(locked)) return art::draw(r, locked, rc);
            // Only the unlocked picture: shown dark and see-through, like a silhouette.
            if (art::drawTinted(r, own, rc, SDL_Color{70, 74, 70, 170})) return;
        }
    }
    art::draw(r, got ? "ui/trophy" : "ui/trophy_locked", rc);
}

// A little gold cup on a stand (grey and faded while locked).
void AchievementsScreen::drawTrophyBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool got) {
    const float u = rc.w / 16.f;
    auto X = [&](float v) { return rc.x + v * u; };
    auto Y = [&](float v) { return rc.y + v * (rc.h / 16.f); };
    const SDL_Color body = got ? pal::Coin : pal::Stone, dark = got ? pal::Mustard : pal::Slate,
                    shine = got ? pal::PaleGold : pal::SageMist;
    // Handles.
    draw::fillRoundRect(r, X(1.5f), Y(3), 3.5f * u, 5 * u, 1.5f * u, dark);
    draw::fillRoundRect(r, X(11), Y(3), 3.5f * u, 5 * u, 1.5f * u, dark);
    // Cup, stem and base.
    draw::fillRoundRect(r, X(4), Y(2), 8 * u, 8 * u, 3 * u, body);
    draw::fillRect(r, X(4), Y(2), 8 * u, 2 * u, body);
    draw::fillRect(r, X(7), Y(9.5f), 2 * u, 2.5f * u, dark);
    draw::fillRoundRect(r, X(4.5f), Y(12), 7 * u, 2.5f * u, u, dark);
    draw::fillRect(r, X(5.3f), Y(3), 1.2f * u, 4.5f * u, shine);
}

void AchievementsScreen::drawButton(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    art::drawVariant(r, "ui/achievements_button", hovered ? "_hover" : "", rc);
}

// Matches the pause and stats buttons: a rounded square with a trophy on it.
void AchievementsScreen::drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rc.w * 0.25f,
                        hovered ? SDL_Color{90, 100, 90, 230} : SDL_Color{50, 58, 50, 210});
    const float m = rc.w * 0.18f;
    drawTrophyBuiltin(r, SDL_FRect{rc.x + m, rc.y + m, rc.w - 2 * m, rc.h - 2 * m}, true);
}
