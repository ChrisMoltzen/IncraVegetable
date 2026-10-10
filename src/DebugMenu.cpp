#include "DebugMenu.h"

#include "Art.h"
#include "Draw.h"
#include "Screen.h"
#include "Palette.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float kPanelX = 150.f, kPanelY = 56.f, kPanelW = 980.f, kPanelH = 628.f;
constexpr float kLeft = 182.f;      // content left edge
constexpr float kTop = 196.f;       // content top edge
constexpr float kRowH = 46.f;       // button height
constexpr int kTechRowsPerPage = 7;

// Text colours, from the palette (Palette.h).
const SDL_Color kText = pal::Parchment;
const SDL_Color kDim = pal::Stone;
const SDL_Color kAccent = pal::Mist;
const SDL_Color kWarn = pal::Pumpkin;

const char* tabName(int t) {
    static const char* names[] = {"Money", "Time", "Farm", "Tech", "Info"};
    return names[t];
}
} // namespace

// ---------------------------------------------------------------------------
// Opening / closing
// ---------------------------------------------------------------------------

void DebugMenu::open(SDL_Window* window) {
    window_ = window;
    open_ = true;
    pressed_ = -1;
    focused_ = -1;
}

void DebugMenu::close() {
    focusField(-1);
    open_ = false;
}

void DebugMenu::focusField(int id) {
    if (focused_ == id) return;
    focused_ = id;
    if (!window_) return;
    if (id >= 0) {
        SDL_StartTextInput(window_); // shows the on-screen keyboard on phones
    } else {
        SDL_StopTextInput(window_);
    }
}

void DebugMenu::toast(const std::string& msg) {
    toast_ = msg;
    toastTime_ = 2.5f;
}

void DebugMenu::statsChanged() {
    if (hooks_.statsChanged) hooks_.statsChanged();
}

void DebugMenu::addCoins(double amount) {
    *hooks_.coins = std::max(0.0, *hooks_.coins + amount);
    toast((amount >= 0 ? "Added " : "Removed ") + draw::number(std::fabs(amount)) + " coins");
}

// Accepts 1500, 1,500, 2.5k, 3M, 1b, 4t, 1e12.
bool DebugMenu::parseNumber(const std::string& s, double& out) {
    std::string clean;
    for (char c : s)
        if (c != ',' && c != ' ' && c != '_') clean += c;
    if (clean.empty()) return false;
    double mult = 1.0;
    char last = static_cast<char>(std::tolower(static_cast<unsigned char>(clean.back())));
    if (last == 'k') mult = 1e3;
    else if (last == 'm') mult = 1e6;
    else if (last == 'b') mult = 1e9;
    else if (last == 't') mult = 1e12;
    if (mult != 1.0) clean.pop_back();
    char* end = nullptr;
    double v = std::strtod(clean.c_str(), &end);
    if (end == clean.c_str() || *end != '\0' || !std::isfinite(v)) return false;
    out = v * mult;
    return true;
}

// ---------------------------------------------------------------------------
// Layout helpers. The whole screen is rebuilt from scratch every frame, so
// the numbers shown are always live.
// ---------------------------------------------------------------------------

void DebugMenu::text(float x, float y, const std::string& s, float scale, SDL_Color c) {
    Widget w;
    w.kind = Kind::Text;
    w.rect = {x, y, 0, 0};
    w.label = s;
    w.scale = scale;
    w.color = c;
    widgets_.push_back(std::move(w));
}

void DebugMenu::button(float x, float y, float width, const std::string& label, std::function<void()> fn, bool enabled,
                       ui::Style style) {
    Widget w;
    w.rect = {x, y, width, kRowH};
    w.label = label;
    w.onClick = std::move(fn);
    w.enabled = enabled;
    w.style = style;
    widgets_.push_back(std::move(w));
}

void DebugMenu::toggle(float x, float y, float width, const std::string& label, bool* value, bool affectsStats) {
    button(x, y, width, label + (*value ? ": ON" : ": off"),
           [this, value, label, affectsStats] {
               *value = !*value;
               if (affectsStats) statsChanged();
               toast(label + (*value ? " on" : " off"));
           },
           true, *value ? ui::Style::Secondary : ui::Style::Ghost);
}

void DebugMenu::field(float x, float y, float width, int id, std::function<void()> onEnter, bool enabled) {
    Widget w;
    w.kind = Kind::Field;
    w.rect = {x, y, width, kRowH};
    w.field = id;
    w.onClick = std::move(onEnter);
    w.enabled = enabled;
    widgets_.push_back(std::move(w));
}

void DebugMenu::build() {
    widgets_.clear();

    // Close button and tabs.
    button(kPanelX + kPanelW - 58, kPanelY + 14, 44, "X", [this] { close(); });
    for (int t = 0; t < static_cast<int>(Tab::Count); ++t) {
        button(kLeft + t * 186.f, 124.f, 176.f, tabName(t),
               [this, t] {
                   tab_ = static_cast<Tab>(t);
                   focusField(-1);
               },
               true, tab_ == static_cast<Tab>(t) ? ui::Style::Primary : ui::Style::Ghost);
    }

    bool inGame = hooks_.inGame && hooks_.inGame();
    if (!inGame && tab_ != Tab::Info) {
        text(kLeft, kTop + 10, "No farm loaded.", 2.5f, kWarn);
        text(kLeft, kTop + 50, "Start or continue a game to use this tab.", 2.f, kDim);
        text(kLeft, kTop + 80, "(The Info tab works everywhere.)", 2.f, kDim);
        return;
    }

    switch (tab_) {
    case Tab::Money: buildMoney(); break;
    case Tab::Time: buildTime(); break;
    case Tab::Farm: buildFarm(); break;
    case Tab::Tech: buildTech(); break;
    case Tab::Info: buildInfo(); break;
    case Tab::Count: break;
    }
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

void DebugMenu::buildMoney() {
    double coins = *hooks_.coins;
    text(kLeft, kTop, "Coins: " + draw::number(coins), 3.f, SDL_Color{255, 210, 80, 255});
    text(kLeft, kTop + 34, draw::strf("exact: %.2f     lifetime: %s", coins, draw::number(*hooks_.lifetimeCoins).c_str()),
         1.5f, kDim);

    struct Quick {
        const char* label;
        double add;
    };
    const Quick row1[] = {{"+1", 1}, {"+10", 10}, {"+100", 100}, {"+1K", 1e3}, {"+10K", 1e4}, {"+100K", 1e5}};
    const Quick row2[] = {{"+1M", 1e6}, {"+1B", 1e9}, {"+1T", 1e12}, {"-100", -100}, {"-1K", -1e3}};
    for (int i = 0; i < 6; ++i) button(kLeft + i * 150.f, kTop + 70, 140, row1[i].label, [this, a = row1[i].add] { addCoins(a); });
    for (int i = 0; i < 5; ++i) button(kLeft + i * 150.f, kTop + 126, 140, row2[i].label, [this, a = row2[i].add] { addCoins(a); });
    button(kLeft + 5 * 150.f, kTop + 126, 140, "x10", [this] {
        *hooks_.coins *= 10.0;
        toast("Coins x10");
    });

    text(kLeft, kTop + 214, "Amount", 2.f);
    auto setCoins = [this] {
        double v = 0;
        if (!parseNumber(buffers_[FieldAmount], v)) return toast("Type a number first, e.g. 2500 or 1.5M");
        *hooks_.coins = std::max(0.0, v);
        toast("Coins set to " + draw::number(*hooks_.coins));
    };
    field(kLeft + 110, kTop + 200, 300, FieldAmount, setCoins);
    button(kLeft + 425, kTop + 200, 180, "Set coins", setCoins, true, ui::Style::Primary);
    button(kLeft + 615, kTop + 200, 180, "Add coins", [this] {
        double v = 0;
        if (!parseNumber(buffers_[FieldAmount], v)) return toast("Type a number first, e.g. 2500 or 1.5M");
        addCoins(v);
    });
    text(kLeft, kTop + 262, "Type 2500, 2.5K, 3M, 1B, 4T or 1e15, then Enter (or a button).", 1.5f, kDim);

    button(kLeft, kTop + 310, 220, "Coins = 0", [this] {
        *hooks_.coins = 0.0;
        toast("Coins set to 0");
    }, true, ui::Style::Danger);
    button(kLeft + 235, kTop + 310, 300, "Afford everything", [this] {
        // Enough to buy every remaining level of every upgrade.
        double total = 0.0;
        for (const auto& n : hooks_.tree->nodes())
            for (int lv = n.level; lv < n.maxLevel; ++lv) total += std::ceil(n.baseCost * std::pow(n.costGrowth, lv));
        *hooks_.coins = std::max(*hooks_.coins, total);
        toast("Coins set to " + draw::number(*hooks_.coins) + " (enough for the whole tree)");
    });
}

void DebugMenu::buildTime() {
    DebugOptions& o = *hooks_.options;
    Stats st = hooks_.stats();
    bool running = hooks_.dayRunning && hooks_.dayRunning();

    text(kLeft, kTop, draw::strf("Day length: %.1fs", st.dayLength), 2.5f, kAccent);
    text(kLeft + 380, kTop + 6, o.dayLengthOverride > 0 ? "(debug override)" : "(from upgrades)", 1.5f, kDim);

    auto setLength = [this](float v) {
        hooks_.options->dayLengthOverride = std::clamp(v, 1.f, 100000.f);
        statsChanged();
        toast(draw::strf("Day length set to %.1fs", hooks_.options->dayLengthOverride));
    };
    const float steps[] = {-10.f, -1.f, 1.f, 10.f, 60.f};
    const char* labels[] = {"-10s", "-1s", "+1s", "+10s", "+60s"};
    for (int i = 0; i < 5; ++i) {
        button(kLeft + i * 125.f, kTop + 34, 115, labels[i], [this, setLength, d = steps[i]] {
            setLength(hooks_.stats().dayLength + d);
        });
    }
    button(kLeft + 5 * 125.f, kTop + 34, 210, "Use upgrades", [this] {
        hooks_.options->dayLengthOverride = 0.f;
        statsChanged();
        toast("Day length back to the tech tree value");
    }, o.dayLengthOverride > 0);

    text(kLeft, kTop + 106, "Set length", 2.f);
    auto setFromField = [this, setLength] {
        double v = 0;
        if (!parseNumber(buffers_[FieldDayLength], v) || v <= 0) return toast("Type a number of seconds, e.g. 90");
        setLength(static_cast<float>(v));
    };
    field(kLeft + 180, kTop + 92, 200, FieldDayLength, setFromField);
    button(kLeft + 395, kTop + 92, 120, "Set", setFromField, true, ui::Style::Primary);

    // Today.
    Farm& farm = *hooks_.farm;
    text(kLeft, kTop + 158,
         running ? draw::strf("Today: %.1fs left", farm.timeLeft()) : std::string("Today: no day in progress"), 2.f,
         running ? kText : kDim);
    button(kLeft, kTop + 188, 120, "+10s", [this, &farm] { farm.setTimeLeft(farm.timeLeft() + 10.f); toast("+10 seconds"); }, running);
    button(kLeft + 130, kTop + 188, 120, "+60s", [this, &farm] { farm.setTimeLeft(farm.timeLeft() + 60.f); toast("+60 seconds"); }, running);
    button(kLeft + 260, kTop + 188, 150, "Refill", [this, &farm] { farm.setTimeLeft(farm.dayLength()); toast("Timer refilled"); }, running);
    button(kLeft + 420, kTop + 188, 190, "End day now", [this] { if (hooks_.endDay) hooks_.endDay(); }, running, ui::Style::Danger);
    toggle(kLeft + 620, kTop + 188, 170, "Freeze", &o.freezeTimer, false);

    // Speed.
    text(kLeft, kTop + 252, draw::strf("Game speed: %gx", o.gameSpeed), 2.f);
    const float speeds[] = {0.25f, 0.5f, 1.f, 2.f, 5.f, 10.f};
    for (int i = 0; i < 6; ++i) {
        float s = speeds[i];
        button(kLeft + i * 125.f, kTop + 280, 115, draw::strf("%gx", s), [this, s] {
            hooks_.options->gameSpeed = s;
            toast(draw::strf("Game speed %gx", s));
        }, true, std::fabs(o.gameSpeed - s) < 0.001f ? ui::Style::Secondary : ui::Style::Ghost);
    }

    // Day number.
    text(kLeft, kTop + 344, draw::strf("Day number: %d", *hooks_.day), 2.f);
    auto setDay = [this](int d) {
        *hooks_.day = std::max(1, d);
        toast(draw::strf("Day set to %d", *hooks_.day));
    };
    button(kLeft, kTop + 372, 100, "-1", [this, setDay] { setDay(*hooks_.day - 1); });
    button(kLeft + 110, kTop + 372, 100, "+1", [this, setDay] { setDay(*hooks_.day + 1); });
    button(kLeft + 220, kTop + 372, 100, "+10", [this, setDay] { setDay(*hooks_.day + 10); });
    auto dayFromField = [this, setDay] {
        double v = 0;
        if (!parseNumber(buffers_[FieldDay], v) || v < 1) return toast("Type a day number, e.g. 25");
        setDay(static_cast<int>(v));
    };
    field(kLeft + 340, kTop + 372, 200, FieldDay, dayFromField);
    button(kLeft + 555, kTop + 372, 110, "Set", dayFromField, true, ui::Style::Primary);
}

void DebugMenu::buildFarm() {
    DebugOptions& o = *hooks_.options;
    bool running = hooks_.dayRunning && hooks_.dayRunning();

    button(kLeft, kTop, 280, "Ripen everything", [this] {
        hooks_.farm->ripenAll();
        toast("Every crop is ripe");
    }, running, ui::Style::Primary);
    button(kLeft + 295, kTop, 280, "Restart today", [this] { if (hooks_.restartDay) hooks_.restartDay(); });
    if (!running) text(kLeft + 600, kTop + 14, "(no day in progress)", 1.5f, kDim);

    toggle(kLeft, kTop + 70, 380, "Instant grow", &o.instantGrow, true);
    toggle(kLeft + 400, kTop + 70, 380, "Instant pick", &o.instantPick, true);
    toggle(kLeft, kTop + 126, 380, "Show plant info", &o.showTileInfo, false);
    toggle(kLeft + 400, kTop + 126, 380, "Show FPS", &o.showFps, false);

    text(kLeft, kTop + 200, "Instant grow: crops ripen the moment they are planted.", 1.5f, kDim);
    text(kLeft, kTop + 222, "Instant pick: touching a ripe crop picks it straight away.", 1.5f, kDim);
    text(kLeft, kTop + 244, "Show plant info: growth %, pick %, crop name and hit circles.", 1.5f, kDim);
    text(kLeft, kTop + 266, "Restart today: fresh patch and a full timer, coins are kept.", 1.5f, kDim);

    Stats st = hooks_.stats();
    text(kLeft, kTop + 320, "Current farm stats", 2.f, kAccent);
    text(kLeft, kTop + 352,
         draw::strf("Crops %d (room %d)   Grow %.2fs   Pick %.2fs   Value x%.2f", Farm::plantCount(st), st.patchSize * st.patchSize, st.growTime,
                    st.pickTime, st.valueMult),
         1.5f);
    text(kLeft, kTop + 374,
         draw::strf("Reach %d   Crop unlocks %d   Head start %d%%   Day %.1fs", st.reach, __builtin_popcountll(st.cropsUnlocked) - 1,
                    static_cast<int>(st.headStart * 100), st.dayLength),
         1.5f);
    text(kLeft, kTop + 396,
         draw::strf("Auto-pick %.0f%% chance, up to %d crops within %.1f plants", st.autoPickChance, st.autoPickCount,
                    st.autoPickRadius),
         1.5f);
    text(kLeft, kTop + 418,
         draw::strf("Farmers %d   walk %.2f plants/s   pick %.2fs", st.farmers, st.farmerSpeed, st.farmerPickTime), 1.5f);
}

void DebugMenu::buildTech() {
    TechTree& tree = *hooks_.tree;
    auto& nodes = tree.nodes();

    button(kLeft, kTop, 200, "Max all", [this, &tree] {
        for (auto& n : tree.nodes()) n.level = n.maxLevel;
        statsChanged();
        toast("Every upgrade maxed");
    }, true, ui::Style::Primary);
    button(kLeft + 215, kTop, 200, "Reset all", [this, &tree] {
        tree.resetLevels();
        statsChanged();
        toast("Every upgrade reset to 0");
    }, true, ui::Style::Danger);
    text(kLeft + 440, kTop + 14, "Changes apply to today straight away.", 1.5f, kDim);

    int pages = std::max(1, (static_cast<int>(nodes.size()) + kTechRowsPerPage - 1) / kTechRowsPerPage);
    techPage_ = std::clamp(techPage_, 0, pages - 1);
    int first = techPage_ * kTechRowsPerPage;
    int last = std::min(static_cast<int>(nodes.size()), first + kTechRowsPerPage);

    float y = kTop + 62;
    for (int i = first; i < last; ++i) {
        TechNode& n = nodes[i];
        text(kLeft, y + 13, n.name, 2.f, n.level >= n.maxLevel ? SDL_Color{255, 205, 70, 255} : kText);
        text(kLeft + 300, y + 15, draw::strf("Lv %d / %d", n.level, n.maxLevel), 1.75f, kDim);
        auto setLevel = [this, &n](int lv) {
            n.level = std::clamp(lv, 0, n.maxLevel);
            statsChanged();
            toast(draw::strf("%s -> level %d", n.name.c_str(), n.level));
        };
        button(kLeft + 470, y, 70, "0", [setLevel] { setLevel(0); }, n.level > 0);
        button(kLeft + 548, y, 70, "-", [setLevel, &n] { setLevel(n.level - 1); }, n.level > 0);
        button(kLeft + 626, y, 70, "+", [setLevel, &n] { setLevel(n.level + 1); }, n.level < n.maxLevel);
        button(kLeft + 704, y, 90, "Max", [setLevel, &n] { setLevel(n.maxLevel); }, n.level < n.maxLevel);
        y += 50.f;
    }

    if (pages > 1) {
        button(kLeft + 810, kTop + 62, 110, "Up", [this] { --techPage_; }, techPage_ > 0);
        text(kLeft + 828, kTop + 124, draw::strf("%d / %d", techPage_ + 1, pages), 1.75f, kDim);
        button(kLeft + 810, kTop + 152, 110, "Down", [this] { ++techPage_; }, techPage_ < pages - 1);
    }
}

void DebugMenu::buildInfo() {
    float y = kTop;
    std::vector<std::string> lines = hooks_.info ? hooks_.info() : std::vector<std::string>{};
    for (const auto& l : lines) {
        text(kLeft, y, l, 1.75f);
        y += 26.f;
    }
    bool inGame = hooks_.inGame && hooks_.inGame();
    button(kLeft, kTop + 340, 220, "Save now", [this] {
        toast(hooks_.saveNow && hooks_.saveNow() ? "Saved" : "Nothing to save - no farm loaded");
    }, inGame, ui::Style::Secondary);
    button(kLeft + 235, kTop + 340, 260, "Reload last save", [this] {
        if (!(hooks_.reloadSave && hooks_.reloadSave())) toast("Couldn't reload - no save for this slot");
    }, inGame);
    toggle(kLeft + 510, kTop + 340, 260, "Show FPS", &hooks_.options->showFps, false);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void DebugMenu::handleEvent(const SDL_Event& e) {
    if (!open_) return;
    build();

    auto widgetAt = [this](float x, float y) {
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (w.kind != Kind::Text && w.enabled && draw::pointInRect(x, y, w.rect)) return i;
        }
        return -1;
    };

    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button != SDL_BUTTON_LEFT) break;
        mouseX_ = e.button.x;
        mouseY_ = e.button.y;
        pressed_ = widgetAt(e.button.x, e.button.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (e.button.button != SDL_BUTTON_LEFT) break;
        int hit = widgetAt(e.button.x, e.button.y);
        int was = pressed_;
        pressed_ = -1;
        if (e.button.which == SDL_TOUCH_MOUSEID) mouseX_ = mouseY_ = -1000.f;
        if (hit < 0 || hit != was) {
            if (hit < 0) focusField(-1);
            break;
        }
        Widget w = widgets_[hit]; // copy: the callback may rebuild widgets_
        if (w.kind == Kind::Field) {
            focusField(w.field);
        } else {
            focusField(-1);
            if (w.onClick) w.onClick();
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (tab_ == Tab::Tech) techPage_ += e.wheel.y < 0 ? 1 : (e.wheel.y > 0 ? -1 : 0);
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (focused_ >= 0 && buffers_[focused_].size() < 24) {
            for (const char* c = e.text.text; *c; ++c) {
                char ch = *c;
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == ',' || ch == '-' || ch == '+')
                    buffers_[focused_] += ch;
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (focused_ >= 0) {
            if (e.key.key == SDLK_BACKSPACE && !buffers_[focused_].empty()) buffers_[focused_].pop_back();
            else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                for (const auto& w : widgets_) {
                    if (w.kind == Kind::Field && w.field == focused_ && w.onClick) {
                        auto fn = w.onClick;
                        fn();
                        break;
                    }
                }
            } else if (e.key.key == SDLK_ESCAPE) {
                focusField(-1);
            }
        } else if (e.key.key == SDLK_ESCAPE) {
            close();
        } else if (e.key.key == SDLK_TAB) {
            tab_ = static_cast<Tab>((static_cast<int>(tab_) + 1) % static_cast<int>(Tab::Count));
        }
        break;
    default:
        break;
    }
}

void DebugMenu::update(float dt) {
    clock_ += dt;
    toastTime_ = std::max(0.f, toastTime_ - dt);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void DebugMenu::drawOpenButton(SDL_Renderer* r, bool hovered) {
    art::drawVariant(r, "ui/debug_button", hovered ? "_hover" : "", buttonRect());
}

void DebugMenu::drawOpenButtonBuiltin(SDL_Renderer* r, const SDL_FRect& b, bool hovered) {
    float k = b.w / 40.f;
    draw::fillRoundRect(r, b.x, b.y, b.w, b.h, 10 * k, hovered ? SDL_Color{120, 60, 140, 235} : SDL_Color{60, 30, 75, 190});
    // A little bug.
    float cx = b.x + b.w * 0.5f, cy = b.y + b.h * 0.5f + 2 * k;
    SDL_Color c{235, 220, 255, 255};
    for (int j = -1; j <= 1; ++j) {
        draw::thickLine(r, cx - 6 * k, cy + j * 5 * k, cx - 13 * k, cy + (j * 6 - 2) * k, 2 * k, c);
        draw::thickLine(r, cx + 6 * k, cy + j * 5 * k, cx + 13 * k, cy + (j * 6 - 2) * k, 2 * k, c);
    }
    draw::fillEllipse(r, cx, cy + 2 * k, 7 * k, 10 * k, c);
    draw::fillCircle(r, cx, cy - 10 * k, 5 * k, c);
    draw::thickLine(r, cx, cy - 8 * k, cx, cy + 12 * k, 1.5f * k, SDL_Color{60, 30, 75, 255});
}

void DebugMenu::render(SDL_Renderer* r) {
    if (!open_) return;
    build();

    screen::fillAll(r, SDL_Color{0, 0, 0, 140});
    draw::fillRoundRect(r, kPanelX + 4, kPanelY + 8, kPanelW, kPanelH, 18, SDL_Color{0, 0, 0, 120});
    draw::fillRoundRect(r, kPanelX - 2, kPanelY - 2, kPanelW + 4, kPanelH + 4, 20, SDL_Color{150, 90, 190, 255});
    draw::fillRoundRect(r, kPanelX, kPanelY, kPanelW, kPanelH, 18, SDL_Color{28, 24, 34, 248});
    draw::text(r, kLeft, kPanelY + 22, "DEBUG", 3.5f, pal::Lavender);
    draw::text(r, kLeft + 170, kPanelY + 32, "F1 or Esc to close   -   Tab to switch tabs", 1.5f, kDim);

    int hovered = -1;
    for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
        const Widget& w = widgets_[i];
        if (w.kind != Kind::Text && w.enabled && draw::pointInRect(mouseX_, mouseY_, w.rect)) {
            hovered = i;
            break;
        }
    }

    for (int i = 0; i < static_cast<int>(widgets_.size()); ++i) {
        const Widget& w = widgets_[i];
        switch (w.kind) {
        case Kind::Text:
            draw::text(r, w.rect.x, w.rect.y, w.label, w.scale, w.color);
            break;
        case Kind::Button: {
            ui::Button b;
            b.rect = w.rect;
            b.label = w.label;
            b.style = w.style;
            b.enabled = w.enabled;
            b.textScale = w.scale;
            ui::drawButton(r, b, i == hovered, i == pressed_ && i == hovered);
            break;
        }
        case Kind::Field: {
            bool focused = focused_ == w.field;
            draw::fillRoundRect(r, w.rect.x - 2, w.rect.y - 2, w.rect.w + 4, w.rect.h + 4, 10,
                                focused ? SDL_Color{150, 90, 190, 255} : SDL_Color{80, 75, 90, 255});
            draw::fillRoundRect(r, w.rect.x, w.rect.y, w.rect.w, w.rect.h, 8, SDL_Color{14, 12, 18, 255});
            const std::string& buf = buffers_[w.field];
            float tx = w.rect.x + 12, ty = w.rect.y + w.rect.h * 0.5f - 8;
            if (buf.empty() && !focused) {
                draw::text(r, tx, ty + 2, "type...", 1.5f, pal::Stone);
            } else {
                draw::text(r, tx, ty, buf, 2.f, kText);
                if (focused && std::fmod(clock_, 1.f) < 0.6f)
                    draw::fillRect(r, tx + draw::textWidth(buf, 2.f) + 2, ty - 2, 3, 20, kText);
            }
            break;
        }
        }
    }

    if (toastTime_ > 0.f) {
        Uint8 a = static_cast<Uint8>(255 * std::min(1.f, toastTime_ * 2.f));
        draw::text(r, kLeft, kPanelY + kPanelH - 26, toast_, 2.f, pal::alpha(pal::PaleShoot, a));
    }
}
