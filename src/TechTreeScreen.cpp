#include "TechTreeScreen.h"

#include "Art.h"
#include "Draw.h"
#include "UI.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kNodeW = 230.f;
constexpr float kNodeH = 72.f;
constexpr float kSpacingX = 270.f;
constexpr float kSpacingY = 118.f;
constexpr float kOriginX = 560.f; // screen position of grid (0,0) before panning
constexpr float kOriginY = 175.f;
constexpr float kDragThreshold = 6.f;

const SDL_Color kWhite{255, 255, 255, 255};
const SDL_Color kGold{255, 205, 70, 255};
const SDL_Color kGreen{110, 220, 110, 255};
const SDL_Color kRed{240, 100, 90, 255};
const SDL_Color kGrey{150, 150, 155, 255};
} // namespace

void TechTreeScreen::open(const TechTree& tree) {
    flash_.assign(tree.nodes().size(), 0.f);
    pressing_ = dragged_ = false;
    selected_ = -1;
}

SDL_FRect TechTreeScreen::nodeRect(const TechNode& n) const {
    float cx = kOriginX + camX_ + n.gridX * kSpacingX;
    float cy = kOriginY + camY_ + n.gridY * kSpacingY;
    return SDL_FRect{cx - kNodeW * 0.5f, cy - kNodeH * 0.5f, kNodeW, kNodeH};
}

// A node shows up once any of its prerequisites has been bought at least
// once, so the tree reveals itself as you go. Roots are always visible.
bool TechTreeScreen::isVisible(const TechTree& tree, const TechNode& n) const {
    if (n.prereqs.empty() || n.level > 0) return true;
    for (const auto& p : n.prereqs) {
        const TechNode* other = tree.find(p.id);
        if (other && other->level > 0) return true;
    }
    return false;
}

int TechTreeScreen::nodeAt(const TechTree& tree, float x, float y) const {
    const auto& nodes = tree.nodes();
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (isVisible(tree, nodes[i]) && draw::pointInRect(x, y, nodeRect(nodes[i]))) return i;
    }
    return -1;
}

TechTreeScreen::Action TechTreeScreen::handleEvent(const SDL_Event& e, TechTree& tree, double& coins) {
    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        touchMode_ = e.motion.which == SDL_TOUCH_MOUSEID;
        if (pressing_) {
            if (!dragged_ && std::hypot(mouseX_ - pressX_, mouseY_ - pressY_) > kDragThreshold) dragged_ = true;
            if (dragged_) {
                camX_ += e.motion.xrel;
                camY_ += e.motion.yrel;
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT) {
            pressing_ = true;
            dragged_ = false;
            touchMode_ = e.button.which == SDL_TOUCH_MOUSEID;
            pressX_ = e.button.x;
            pressY_ = e.button.y;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!pressing_) break;
        bool wasDrag = dragged_;
        pressing_ = dragged_ = false;
        if (wasDrag || e.button.button != SDL_BUTTON_LEFT) break;
        if (draw::pointInRect(e.button.x, e.button.y, startButton_)) return Action::StartDay;
        int idx = nodeAt(tree, e.button.x, e.button.y);
        if (idx < 0) {
            selected_ = -1;
            break;
        }
        // With touch there's no hover, so the first tap shows the details
        // and a second tap on the same node buys it.
        if (touchMode_ && selected_ != idx) {
            selected_ = idx;
            return Action::Selected;
        }
        selected_ = idx;
        if (tree.tryBuy(static_cast<size_t>(idx), coins)) {
            if (flash_.size() != tree.nodes().size()) flash_.assign(tree.nodes().size(), 0.f);
            flash_[idx] = 1.f;
            return Action::Purchased;
        }
        return Action::Denied;
    }
    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) return Action::StartDay;
        if (e.key.key == SDLK_HOME) camX_ = camY_ = 0.f;
        break;
    default:
        break;
    }
    return Action::None;
}

void TechTreeScreen::update(float dt) {
    clock_ += dt;
    for (auto& f : flash_) f = std::max(0.f, f - dt * 2.5f);
}

void TechTreeScreen::render(SDL_Renderer* r, const TechTree& tree, double coins, int nextDay) const {
    // Background. The built-in one is a grid that moves when you pan.
    if (art::has("tree/background")) art::draw(r, "tree/background", SDL_FRect{0, 0, 1280, 720});
    else drawBackgroundBuiltin(r, SDL_FRect{0, 0, 1280, 720}, camX_, camY_);

    const auto& nodes = tree.nodes();

    // Connection lines first so nodes sit on top of them.
    for (const auto& n : nodes) {
        if (!isVisible(tree, n)) continue;
        SDL_FRect to = nodeRect(n);
        for (const auto& p : n.prereqs) {
            const TechNode* from = tree.find(p.id);
            if (!from) continue;
            SDL_FRect fr = nodeRect(*from);
            bool met = from->level >= p.level;
            SDL_Color c = met ? SDL_Color{110, 200, 110, 255} : SDL_Color{80, 85, 90, 255};
            float x1 = fr.x + fr.w * 0.5f, y1 = fr.y + fr.h * 0.5f;
            float x2 = to.x + to.w * 0.5f, y2 = to.y + to.h * 0.5f;
            draw::thickLine(r, x1, y1, x2, y2, 4.f, c);
            // Show the level needed at the midpoint of the line when it isn't met yet.
            if (!met) {
                float mx = (x1 + x2) * 0.5f, my = (y1 + y2) * 0.5f;
                std::string need = draw::strf("Lv%d", p.level);
                draw::fillRoundRect(r, mx - 22, my - 10, 44, 20, 8, SDL_Color{50, 55, 60, 255});
                draw::text(r, mx, my - 4, need, 1.f, kGrey, draw::Align::Center);
            }
        }
    }

    int hovered = touchMode_ ? selected_ : nodeAt(tree, mouseX_, mouseY_);
    if (pressing_ && dragged_) hovered = -1;

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        const TechNode& n = nodes[i];
        if (!isVisible(tree, n)) continue;
        SDL_FRect rc = nodeRect(n);
        bool maxed = tree.isMaxed(n);
        bool unlocked = tree.prereqsMet(n);
        bool affordable = tree.canBuy(n, coins);

        const char* artName = maxed ? "tree/node_maxed"
                              : affordable ? "tree/node_affordable"
                              : unlocked ? "tree/node"
                                         : "tree/node_locked";
        SDL_Color fill, border, title;
        if (maxed) {
            fill = {85, 68, 25, 255}; border = kGold; title = kGold;
        } else if (affordable) {
            float pulse = 0.5f + 0.5f * std::sin(clock_ * 4.f);
            fill = {32, 72, 38, 255};
            border = draw::lerp(SDL_Color{80, 170, 80, 255}, SDL_Color{170, 255, 150, 255}, pulse);
            title = kWhite;
        } else if (unlocked) {
            fill = {44, 50, 56, 255}; border = {120, 128, 135, 255}; title = kWhite;
        } else {
            fill = {32, 33, 37, 255}; border = {70, 72, 78, 255}; title = kGrey;
        }
        if (i == hovered) border = kWhite;

        SDL_FRect outer{rc.x - 3, rc.y - 3, rc.w + 6, rc.h + 6};
        if (art::has(artName)) {
            art::drawVariant(r, artName, i == hovered ? "_hover" : "", outer);
        } else {
            draw::fillRoundRect(r, rc.x + 3, rc.y + 5, rc.w, rc.h, 12, SDL_Color{0, 0, 0, 90});
            drawNodeBuiltin(r, outer, fill, border);
        }

        // Optional icon on the left (assets/tree/icons/<id>.png).
        std::string icon = "tree/icons/" + n.id;
        float textLeft = rc.x + 12;
        if (art::has(icon)) {
            art::draw(r, icon, SDL_FRect{rc.x + 10, rc.y + (rc.h - 48) * 0.5f, 48, 48});
            textLeft = rc.x + 66;
        }
        float avail = rc.x + rc.w - 10 - textLeft;
        float nameScale = std::min(2.f, avail / std::max(1.f, draw::textWidth(n.name, 1.f)));
        draw::text(r, textLeft + avail * 0.5f, rc.y + 12 + (2.f - nameScale) * 4.f, n.name, nameScale, title,
                   draw::Align::Center);

        std::string lvl = draw::strf("Lv %d/%d", n.level, n.maxLevel);
        draw::text(r, textLeft, rc.y + rc.h - 22, lvl, 1.5f, maxed ? kGold : kGrey);

        std::string right;
        SDL_Color rightCol = kGrey;
        if (maxed) {
            right = "MAX";
            rightCol = kGold;
        } else if (!unlocked) {
            right = "LOCKED";
        } else {
            right = draw::number(tree.cost(n));
            rightCol = coins >= tree.cost(n) ? kGreen : kRed;
        }
        float tw = draw::textWidth(right, 1.5f);
        draw::text(r, rc.x + rc.w - 12, rc.y + rc.h - 22, right, 1.5f, rightCol, draw::Align::Right);
        if (!maxed && unlocked) ui::drawCoin(r, rc.x + rc.w - 26 - tw, rc.y + rc.h - 17, 6);

        if (i < static_cast<int>(flash_.size()) && flash_[i] > 0.f) {
            float f = flash_[i];
            draw::fillRoundRect(r, rc.x - 3 - 8 * (1 - f), rc.y - 3 - 8 * (1 - f), rc.w + 6 + 16 * (1 - f),
                                rc.h + 6 + 16 * (1 - f), 14, SDL_Color{255, 255, 200, static_cast<Uint8>(160 * f)});
        }
    }

    // Header.
    art::draw(r, "tree/header_bar", SDL_FRect{0, 0, 1280, 64});
    draw::textShadow(r, 24, 18, "TECH TREE", 3.f, kWhite);
    std::string coinText = draw::number(coins) + " coins";
    float coinX = 1192.f - draw::textWidth(coinText, 3.f);
    ui::drawCoin(r, coinX - 22, 32, 12);
    draw::textShadow(r, coinX, 21, coinText, 3.f, kGold);
    draw::text(r, 64, 690,
               touchMode_ ? "Tap to see an upgrade, tap again to buy.  Drag to look around."
                          : "Click to buy.  Drag to look around.  Home to recentre.",
               1.5f, kGrey);

    // Start day button.
    ui::Button start;
    start.rect = startButton_;
    start.label = draw::strf("Start Day %d", nextDay);
    ui::drawButton(r, start, !touchMode_ && draw::pointInRect(mouseX_, mouseY_, startButton_), false);

    if (hovered >= 0) renderTooltip(r, tree, nodes[hovered], coins);
}

void TechTreeScreen::renderTooltip(SDL_Renderer* r, const TechTree& tree, const TechNode& n, double coins) const {
    struct Line {
        std::string text;
        float scale;
        SDL_Color color;
    };
    std::vector<Line> lines;
    lines.push_back({n.name, 2.f, kWhite});
    for (const auto& w : draw::wrap(n.description, 34)) lines.push_back({w, 1.5f, SDL_Color{200, 205, 200, 255}});
    lines.push_back({"", 0.8f, kWhite});
    if (n.describe) {
        lines.push_back({"Now:  " + n.describe(n.level), 1.5f, SDL_Color{190, 220, 255, 255}});
        if (!tree.isMaxed(n)) lines.push_back({"Next: " + n.describe(n.level + 1), 1.5f, kGreen});
    }
    for (const auto& p : n.prereqs) {
        const TechNode* other = tree.find(p.id);
        if (other && other->level < p.level)
            lines.push_back({draw::strf("Needs %s Lv %d", other->name.c_str(), p.level), 1.5f, kRed});
    }
    if (tree.isMaxed(n)) {
        lines.push_back({"Fully upgraded!", 1.5f, kGold});
    } else {
        double c = tree.cost(n);
        lines.push_back({"Cost: " + draw::number(c) + " coins", 1.5f, coins >= c ? kGold : kRed});
    }

    float w = 0.f, h = 0.f;
    for (const auto& l : lines) {
        w = std::max(w, draw::textWidth(l.text, l.scale));
        h += 8.f * l.scale + 6.f;
    }
    w += 28.f;
    h += 22.f;
    float ax = mouseX_, ay = mouseY_;
    if (touchMode_) { // anchor under the node instead of under a finger
        SDL_FRect rc = nodeRect(n);
        ax = rc.x + rc.w * 0.5f;
        ay = rc.y + rc.h;
    }
    float x = ax + 18.f, y = ay + 18.f;
    if (x + w > 1270.f) x = ax - w - 12.f;
    if (y + h > 710.f) y = 710.f - h;
    x = std::max(10.f, x);
    y = std::max(70.f, y);

    art::draw(r, "ui/tooltip", SDL_FRect{x - 2, y - 2, w + 4, h + 4});
    float ty = y + 12.f;
    for (const auto& l : lines) {
        if (!l.text.empty()) draw::text(r, x + 14, ty, l.text, l.scale, l.color);
        ty += 8.f * l.scale + 6.f;
    }
}

// ---------------------------------------------------------------------------
// Built-in art (used when there's no image in assets/, and for the templates)
// ---------------------------------------------------------------------------

void TechTreeScreen::drawBackgroundBuiltin(SDL_Renderer* r, const SDL_FRect& rc, float panX, float panY) {
    draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{24, 30, 28, 255});
    const float grid = 40.f;
    float ox = std::fmod(panX, grid), oy = std::fmod(panY, grid);
    for (float x = rc.x + ox - grid; x < rc.x + rc.w; x += grid)
        if (x >= rc.x) draw::fillRect(r, x, rc.y, 1, rc.h, SDL_Color{34, 42, 39, 255});
    for (float y = rc.y + oy - grid; y < rc.y + rc.h; y += grid)
        if (y >= rc.y) draw::fillRect(r, rc.x, y, rc.w, 1, SDL_Color{34, 42, 39, 255});
}

void TechTreeScreen::drawNodeBuiltin(SDL_Renderer* r, const SDL_FRect& outer, SDL_Color fill, SDL_Color border) {
    draw::fillRoundRect(r, outer.x, outer.y, outer.w, outer.h, 14, border);
    draw::fillRoundRect(r, outer.x + 3, outer.y + 3, outer.w - 6, outer.h - 6, 11, fill);
}

void TechTreeScreen::drawTooltipBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, 12, SDL_Color{150, 160, 150, 255});
    draw::fillRoundRect(r, rc.x + 2, rc.y + 2, rc.w - 4, rc.h - 4, 10, SDL_Color{20, 24, 23, 250});
}

void TechTreeScreen::drawHeaderBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{16, 20, 19, 235});
}
