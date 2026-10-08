#include "TechTreeScreen.h"

#include "Art.h"
#include "CropLooks.h"
#include "TileShapes.h"
#include "Draw.h"
#include "Palette.h"
#include "Farm.h"
#include "UI.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
// Each upgrade is a square tile with its icon; everything else is in the
// pop-up shown on hover (or on the first tap with touch).
constexpr float kTile = 84.f;      // tile size
constexpr float kIcon = 60.f;      // icon size inside it
constexpr float kSpacingX = 140.f; // one grid column / row on screen
constexpr float kSpacingY = 112.f;
constexpr float kTop = 64.f + 24.f;      // keep tiles below the header...
constexpr float kBottom = 720.f - 96.f;  // ...and above the Start Day button row
constexpr float kDragThreshold = 6.f;
constexpr float kMinZoom = 0.12f, kMaxZoom = 1.6f; // far enough out to fit a fully grown tree
constexpr float kPivotX = 640.f;
constexpr float kPivotY = kTop + kTile * 0.5f; // the top row stays put when zooming from Home

// `rc` (laid out at zoom 1) scaled by z about its own centre.
SDL_FRect scaled(const SDL_FRect& rc, float cx, float cy, float z) {
    return SDL_FRect{cx + (rc.x - cx) * z, cy + (rc.y - cy) * z, rc.w * z, rc.h * z};
}

// Text colours, from the palette (Palette.h).
const SDL_Color kWhite = pal::Cream;
const SDL_Color kGold = pal::Coin;
const SDL_Color kGreen = pal::FreshLeaf;
const SDL_Color kRed = pal::Rosehip;
const SDL_Color kGrey = pal::Stone;
// Tooltip text: the lightest shade of each colour, drawn with a shadow, so it
// reads on the built-in dark box and on lighter art (e.g. a wooden one).
const SDL_Color kTipTitle = pal::Cream;
const SDL_Color kTipText = pal::Parchment;
const SDL_Color kTipDim = pal::SageMist;
const SDL_Color kTipNow = pal::Mist;
const SDL_Color kTipGood = pal::PaleShoot;
const SDL_Color kTipGold = pal::PaleGold;
const SDL_Color kTipBad = pal::Skin; // the palette's reds are too dark to read on a box, so warnings are a warm pink-beige
} // namespace

void TechTreeScreen::open(const TechTree& tree) {
    flash_.assign(tree.nodes().size(), 0.f);
    pressing_ = dragged_ = false;
    pinching_ = false;
    fingers_ = 0;
    pressedZoom_ = 0;
    selected_ = -1;
    fitView(tree);
}

// Zooms and pans so every tech the player can see fits on screen (between the
// header and the Start Day row), centred. Never zooms in past 100%; if even
// the furthest zoom-out can't fit them, shows the top of the tree.
void TechTreeScreen::fitView(const TechTree& tree) {
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const auto& n : tree.nodes()) {
        if (!isVisible(tree, n)) continue;
        minX = std::min(minX, n.gridX); maxX = std::max(maxX, n.gridX);
        minY = std::min(minY, n.gridY); maxY = std::max(maxY, n.gridY);
    }
    if (minX > maxX) minX = maxX = minY = maxY = 0.f;
    // Lay the tree out with the visible part's middle at the screen's middle (at 100%)...
    const float midY = (kTop + kBottom) * 0.5f;
    originX_ = kPivotX - (minX + maxX) * 0.5f * kSpacingX;
    originY_ = midY - (minY + maxY) * 0.5f * kSpacingY;
    // ...then zoom out until it all fits (tiles' edges and the frame included, plus a little room).
    const float margin = 24.f;
    const float w = (maxX - minX) * kSpacingX + kTile + 2.f * margin;
    const float h = (maxY - minY) * kSpacingY + kTile + 2.f * margin;
    const float availW = 1280.f, availH = kBottom - kTop;
    zoom_ = std::clamp(std::min(availW / w, availH / h), kMinZoom, 1.f);
    // Keep the middle of the visible techs at the middle of the screen.
    const float wy = originY_ + (minY + maxY) * 0.5f * kSpacingY;
    camX_ = 0.f; // x: the layout's middle is already the pivot
    camY_ = midY - kPivotY - (wy - kPivotY) * zoom_;
    // Still too tall at the furthest zoom? Start at the top; drag to see the rest.
    const float topY = kPivotY + (originY_ + minY * kSpacingY - kPivotY) * zoom_ + camY_ - kTile * 0.5f * zoom_;
    if (topY < kTop) camY_ += kTop - topY;
}

SDL_FPoint TechTreeScreen::toScreen(float x, float y) const {
    return SDL_FPoint{kPivotX + (x - kPivotX) * zoom_ + camX_, kPivotY + (y - kPivotY) * zoom_ + camY_};
}

SDL_FRect TechTreeScreen::nodeRect(const TechNode& n) const {
    SDL_FPoint c = toScreen(originX_ + n.gridX * kSpacingX, originY_ + n.gridY * kSpacingY);
    const float t = kTile * zoom_;
    return SDL_FRect{c.x - t * 0.5f, c.y - t * 0.5f, t, t};
}

void TechTreeScreen::zoomAt(float sx, float sy, float newZoom) {
    newZoom = std::clamp(newZoom, kMinZoom, kMaxZoom);
    // The layout point under (sx, sy) before...
    float wx = (sx - kPivotX - camX_) / zoom_ + kPivotX;
    float wy = (sy - kPivotY - camY_) / zoom_ + kPivotY;
    zoom_ = newZoom;
    // ...stays under it after.
    camX_ = sx - kPivotX - (wx - kPivotX) * zoom_;
    camY_ = sy - kPivotY - (wy - kPivotY) * zoom_;
}

// Small + and - buttons above Start Day, for players without a wheel or a pinch.
SDL_FRect TechTreeScreen::zoomInButton() const { return SDL_FRect{1250.f - 52.f, 634.f - 64.f, 52.f, 52.f}; }
SDL_FRect TechTreeScreen::zoomOutButton() const { return SDL_FRect{1250.f - 52.f - 60.f, 634.f - 64.f, 52.f, 52.f}; }

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
    case SDL_EVENT_MOUSE_WHEEL: {
        float steps = e.wheel.y;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) steps = -steps;
        if (steps != 0.f) zoomAt(e.wheel.mouse_x, e.wheel.mouse_y, zoom_ * std::pow(1.15f, steps));
        break;
    }
    case SDL_EVENT_FINGER_DOWN: {
        // Resync if a finger went up while another screen was showing.
        int down = 0;
        if (SDL_Finger** list = SDL_GetTouchFingers(e.tfinger.touchID, &down)) SDL_free(list);
        if (down == 1) { // this is the only finger on the screen
            fingers_ = 0;
            pinching_ = false;
        }
        if (fingers_ == 0) {
            fingerA_ = e.tfinger.fingerID;
            fingerPosA_ = {e.tfinger.x, e.tfinger.y};
            fingers_ = 1;
        } else if (fingers_ == 1 && e.tfinger.fingerID != fingerA_) {
            fingerB_ = e.tfinger.fingerID;
            fingerPosB_ = {e.tfinger.x, e.tfinger.y};
            fingers_ = 2;
            pinching_ = true;
            dragged_ = true; // the first finger's press is a pinch now, not a tap
        }
        break;
    }
    case SDL_EVENT_FINGER_MOTION:
        if (fingers_ == 2 && (e.tfinger.fingerID == fingerA_ || e.tfinger.fingerID == fingerB_)) {
            SDL_FPoint a = fingerPosA_, b = fingerPosB_;
            float before = std::hypot(b.x - a.x, b.y - a.y);
            if (e.tfinger.fingerID == fingerA_) fingerPosA_ = {e.tfinger.x, e.tfinger.y};
            else fingerPosB_ = {e.tfinger.x, e.tfinger.y};
            float after = std::hypot(fingerPosB_.x - fingerPosA_.x, fingerPosB_.y - fingerPosA_.y);
            if (before > 1.f && after > 1.f) {
                // Zoom about the middle of the two fingers, and move with them.
                float mx = (fingerPosA_.x + fingerPosB_.x) * 0.5f, my = (fingerPosA_.y + fingerPosB_.y) * 0.5f;
                float pmx = (a.x + b.x) * 0.5f, pmy = (a.y + b.y) * 0.5f;
                zoomAt(mx, my, zoom_ * after / before);
                camX_ += mx - pmx;
                camY_ += my - pmy;
            }
        } else if (e.tfinger.fingerID == fingerA_) {
            fingerPosA_ = {e.tfinger.x, e.tfinger.y};
        }
        break;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (e.tfinger.fingerID == fingerA_ && fingers_ == 2) {
            fingerA_ = fingerB_;
            fingerPosA_ = fingerPosB_;
            fingers_ = 1;
        } else if (e.tfinger.fingerID == fingerA_ || e.tfinger.fingerID == fingerB_) {
            fingers_ = std::max(0, fingers_ - 1);
        }
        if (fingers_ == 0) pinching_ = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        touchMode_ = e.motion.which == SDL_TOUCH_MOUSEID;
        if (pinching_) break; // the pinch moves the view
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
            pressedZoom_ = 0;
            if (e.button.button == SDL_BUTTON_LEFT) {
                if (draw::pointInRect(e.button.x, e.button.y, zoomInButton())) pressedZoom_ = 1;
                else if (draw::pointInRect(e.button.x, e.button.y, zoomOutButton())) pressedZoom_ = -1;
            }
            if (pinching_) dragged_ = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!pressing_) break;
        bool wasDrag = dragged_ || pinching_;
        int zoomButton = pressedZoom_;
        pressing_ = dragged_ = false;
        pressedZoom_ = 0;
        if (wasDrag || e.button.button != SDL_BUTTON_LEFT) break;
        if (draw::pointInRect(e.button.x, e.button.y, startButton_)) return Action::StartDay;
        // Zoom buttons zoom about the middle of the screen.
        if (zoomButton > 0 && draw::pointInRect(e.button.x, e.button.y, zoomInButton())) {
            zoomAt(640.f, 360.f, zoom_ * 1.25f);
            break;
        }
        if (zoomButton < 0 && draw::pointInRect(e.button.x, e.button.y, zoomOutButton())) {
            zoomAt(640.f, 360.f, zoom_ / 1.25f);
            break;
        }
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
        if (e.key.key == SDLK_HOME) fitView(tree); // back to showing everything you can see
        if (e.key.key == SDLK_EQUALS || e.key.key == SDLK_PLUS || e.key.key == SDLK_KP_PLUS)
            zoomAt(640.f, 360.f, zoom_ * 1.25f);
        if (e.key.key == SDLK_MINUS || e.key.key == SDLK_KP_MINUS) zoomAt(640.f, 360.f, zoom_ / 1.25f);
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
    else drawBackgroundBuiltin(r, SDL_FRect{0, 0, 1280, 720}, camX_ + kPivotX * (1.f - zoom_),
                               camY_ + kPivotY * (1.f - zoom_), zoom_);

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
            SDL_Color c = met ? pal::FreshLeaf : pal::Slate; // palette: green once the requirement is met
            float x1 = fr.x + fr.w * 0.5f, y1 = fr.y + fr.h * 0.5f;
            float x2 = to.x + to.w * 0.5f, y2 = to.y + to.h * 0.5f;
            draw::thickLine(r, x1, y1, x2, y2, std::max(2.f, 4.f * zoom_), c);
            // Show the level needed at the midpoint of the line when it isn't met yet.
            if (!met && zoom_ >= 0.6f) {
                float mx = (x1 + x2) * 0.5f, my = (y1 + y2) * 0.5f;
                std::string need = draw::strf("Lv%d", p.level);
                draw::fillRoundRect(r, mx - 22, my - 10, 44, 20, 8, pal::Panel);
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
        SDL_Color fill, border;
        if (maxed) {
            fill = {85, 68, 25, 255}; border = kGold;
        } else if (affordable) {
            float pulse = 0.5f + 0.5f * std::sin(clock_ * 4.f);
            fill = {32, 72, 38, 255};
            border = draw::lerp(SDL_Color{80, 170, 80, 255}, SDL_Color{170, 255, 150, 255}, pulse);
        } else if (unlocked) {
            fill = {44, 50, 56, 255}; border = {120, 128, 135, 255};
        } else {
            fill = {32, 33, 37, 255}; border = {70, 72, 78, 255};
        }
        if (i == hovered) border = kWhite;

        // The tech's own shape and colours (set in the editor). The frame
        // colour still shows its state: pulsing green = can buy, gold = maxed.
        const std::string shape = n.shape.empty() ? std::string("square") : n.shape;
        const bool locked = !unlocked && !maxed;
        SDL_Color custom;
        bool customFill = tileshape::parseHex(locked ? n.lockedColor : n.color, custom);
        if (customFill) fill = custom;

        const float z = zoom_, bw = std::max(1.5f, 3.f * z);
        const float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
        const SDL_FRect base{cx - kTile * 0.5f, cy - kTile * 0.5f, kTile, kTile}; // the tile at zoom 1
        SDL_FRect outer{rc.x - bw, rc.y - bw, rc.w + 2 * bw, rc.h + 2 * bw};
        if (shape == "square" && !customFill && art::has(artName)) {
            // Your tile art (square, usual colours), 9-sliced: the corners and edges keep their shape
            // and scale with the zoom, and only the middle stretches.
            art::SliceScale zoomed(z);
            art::drawVariant(r, artName, i == hovered ? "_hover" : "", outer);
        } else {
            tileshape::fill(r, shape, SDL_FRect{rc.x + 3 * z, rc.y + 5 * z, rc.w, rc.h}, SDL_Color{0, 0, 0, 90}); // shadow
            tileshape::draw(r, shape, outer, fill, border, bw);
        }

        // The icon (assets/tree/icons/<id>.png, or the built-in picture). Locked: faded into the tile.
        SDL_FRect ic = scaled(tileshape::iconRect(shape, base, kIcon), cx, cy, z);
        art::draw(r, "tree/icons/" + (n.icon.empty() ? n.id : n.icon), ic);
        if (locked) draw::fillRoundRect(r, ic.x - 2, ic.y - 2, ic.w + 4, ic.h + 4, 8, draw::withAlpha(fill, 165));

        // A thin bar near the bottom shows how many levels are bought (techs with
        // more than one level only; a one-level tech's frame already shows if it's bought).
        if (n.maxLevel > 1) {
            SDL_FRect bar = scaled(tileshape::barRect(shape, base), cx, cy, z);
            float f = static_cast<float>(n.level) / static_cast<float>(n.maxLevel);
            draw::fillRoundRect(r, bar.x, bar.y, bar.w, bar.h, 2, SDL_Color{0, 0, 0, 120});
            if (f > 0.f) draw::fillRoundRect(r, bar.x, bar.y, std::max(4.f, bar.w * f), bar.h, 2, maxed ? kGold : kGreen);
        }

        if (i < static_cast<int>(flash_.size()) && flash_[i] > 0.f) {
            float f = flash_[i], g = 8.f * z * (1.f - f);
            tileshape::fill(r, shape, SDL_FRect{outer.x - g, outer.y - g, outer.w + 2 * g, outer.h + 2 * g},
                            SDL_Color{255, 255, 200, static_cast<Uint8>(160 * f)});
        }
    }

    // Header.
    art::draw(r, "tree/header_bar", SDL_FRect{0, 0, 1280, 64});
    draw::textShadow(r, 24, 18, "THE BARN", 3.f, kWhite);
    std::string coinText = draw::number(coins) + " coins";
    float coinX = 1136.f - draw::textWidth(coinText, 3.f); // clear of the stats and pause buttons
    ui::drawCoin(r, coinX - 22, 32, 12);
    draw::textShadow(r, coinX, 21, coinText, 3.f, kGold);
    draw::text(r, 64, 690,
               touchMode_ ? "Tap to see an upgrade, tap again to buy.  Drag to move, pinch to zoom."
                          : "Hover to see, click to buy.  Drag to move, wheel to zoom, Home to fit.",
               1.5f, kGrey);

    // Start day button.
    ui::Button start;
    start.rect = startButton_;
    start.label = draw::strf("Start Day %d", nextDay);
    ui::drawButton(r, start, !touchMode_ && draw::pointInRect(mouseX_, mouseY_, startButton_), false);

    // Zoom buttons.
    for (int k = 0; k < 2; ++k) {
        ui::Button zb;
        zb.rect = k == 0 ? zoomOutButton() : zoomInButton();
        zb.label = k == 0 ? "-" : "+";
        zb.style = ui::Style::Secondary;
        zb.textScale = 3.f;
        zb.enabled = k == 0 ? zoom_ > kMinZoom + 0.001f : zoom_ < kMaxZoom - 0.001f;
        bool over = !touchMode_ && draw::pointInRect(mouseX_, mouseY_, zb.rect);
        ui::drawButton(r, zb, over, over && pressing_ && pressedZoom_ == (k == 0 ? -1 : 1));
    }
    {
        std::string pct = draw::strf("%d%%", static_cast<int>(std::lround(zoom_ * 100)));
        float mx = (zoomOutButton().x + zoomInButton().x + zoomInButton().w) * 0.5f, y = zoomInButton().y - 24;
        draw::fillRoundRect(r, mx - 30, y, 60, 20, 8, SDL_Color{20, 24, 23, 220});
        draw::text(r, mx, y + 6, pct, 1.25f, kGrey, draw::Align::Center);
    }

    if (hovered >= 0) renderTooltip(r, tree, nodes[hovered], coins);
}

void TechTreeScreen::renderTooltip(SDL_Renderer* r, const TechTree& tree, const TechNode& n, double coins) const {
    struct Line {
        std::string text;
        float scale;
        SDL_Color color;
    };
    std::vector<Line> lines;
    const bool maxed = tree.isMaxed(n), unlocked = tree.prereqsMet(n);
    lines.push_back({n.name, 2.5f, maxed ? kTipGold : kTipTitle});
    if (n.maxLevel > 1) // a one-level tech is either bought or not; no level line
        lines.push_back({maxed ? draw::strf("Level %d / %d  -  MAX", n.level, n.maxLevel)
                               : draw::strf("Level %d / %d", n.level, n.maxLevel),
                         1.5f, maxed ? kTipGold : kTipDim});
    lines.push_back({"", 0.6f, kWhite});
    for (const auto& w : draw::wrap(n.description, 36)) lines.push_back({w, 1.5f, kTipText});
    lines.push_back({"", 0.8f, kWhite});
    if (n.describeStats) {
        // Worked out from all your upgrades, so e.g. a second farmer reads "2 farmers".
        lines.push_back({"Now:  " + n.describeStats(tree.computeStats()), 1.5f, kTipNow});
        if (!maxed) lines.push_back({"Next: " + n.describeStats(tree.computeStatsWith(n, n.level + 1)), 1.5f, kTipGood});
        if (n.maxLevel > 1 && n.level + 1 < n.maxLevel)
            lines.push_back({"At max: " + n.describeStats(tree.computeStatsWith(n, n.maxLevel)), 1.5f, kTipDim});
    }
    if (!n.describeStats) {
        // A tech that only unlocks others: say which.
        std::string names;
        for (const auto& o : tree.nodes())
            for (const auto& p : o.prereqs)
                if (p.id == n.id) names += (names.empty() ? "" : ", ") + o.name;
        if (!names.empty()) {
            bool first = true;
            for (const auto& w : draw::wrap("Unlocks: " + names, 36)) {
                lines.push_back({(first ? "" : "  ") + w, 1.5f, kTipNow});
                first = false;
            }
        }
    }
    for (const auto& p : n.prereqs) {
        const TechNode* other = tree.find(p.id);
        if (other && other->level < p.level)
            lines.push_back({draw::strf("Needs %s Lv %d", other->name.c_str(), p.level), 1.5f, kTipBad});
    }
    lines.push_back({"", 0.8f, kWhite});
    if (maxed) {
        lines.push_back({n.maxLevel > 1 ? "Fully upgraded!" : "UNLOCKED", 2.f, kTipGold});
    } else {
        double c = tree.cost(n);
        lines.push_back({"Price: " + draw::number(c) + " coins", 2.f, coins >= c ? kTipGold : kTipBad});
        const char* hint = !unlocked      ? "Locked"
                           : coins < c    ? "Not enough coins"
                           : touchMode_   ? "Tap again to buy"
                                          : "Click to buy";
        lines.push_back({hint, 1.25f, unlocked && coins >= c ? kTipGood : kTipDim});
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
        if (!l.text.empty()) draw::textShadow(r, x + 14, ty, l.text, l.scale, l.color); // shadowed: reads on light or dark boxes
        ty += 8.f * l.scale + 6.f;
    }
}

// ---------------------------------------------------------------------------
// Built-in art (used when there's no image in assets/, and for the templates)
// ---------------------------------------------------------------------------

void TechTreeScreen::drawBackgroundBuiltin(SDL_Renderer* r, const SDL_FRect& rc, float panX, float panY, float zoom) {
    draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{24, 30, 28, 255});
    float grid = 40.f * zoom;
    while (grid < 16.f) grid *= 2.f; // zoomed far out: fewer lines, not a solid fill
    float ox = std::fmod(panX, grid), oy = std::fmod(panY, grid);
    if (ox < 0) ox += grid;
    if (oy < 0) oy += grid;
    for (float x = rc.x + ox - grid; x < rc.x + rc.w; x += grid)
        if (x >= rc.x) draw::fillRect(r, x, rc.y, 1, rc.h, SDL_Color{34, 42, 39, 255});
    for (float y = rc.y + oy - grid; y < rc.y + rc.h; y += grid)
        if (y >= rc.y) draw::fillRect(r, rc.x, y, rc.w, 1, SDL_Color{34, 42, 39, 255});
}

void TechTreeScreen::drawNodeBuiltin(SDL_Renderer* r, const SDL_FRect& outer, SDL_Color fill, SDL_Color border) {
    // Corners and border keep their size however big the tile is (like 9-slice art), and follow the zoom.
    const float k = art::sliceScale();
    const float radius = std::min(14.f * k, std::min(outer.w, outer.h) * 0.5f), b = std::max(1.f, 3.f * k);
    draw::fillRoundRect(r, outer.x, outer.y, outer.w, outer.h, radius, border);
    draw::fillRoundRect(r, outer.x + b, outer.y + b, outer.w - 2 * b, outer.h - 2 * b, std::max(0.f, radius - b), fill);
}

void TechTreeScreen::drawTooltipBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, 12, SDL_Color{150, 160, 150, 255});
    draw::fillRoundRect(r, rc.x + 2, rc.y + 2, rc.w - 4, rc.h - 4, 10, SDL_Color{20, 24, 23, 250});
}

void TechTreeScreen::drawHeaderBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{16, 20, 19, 235});
}

namespace {
// Initials on a badge, for techs that have no picture of their own.
void drawInitialsIcon(SDL_Renderer* r, const SDL_FRect& rc, const std::string& title) {
    draw::fillRoundRect(r, rc.x + 2, rc.y + 2, rc.w - 4, rc.h - 4, rc.w * 0.2f, SDL_Color{70, 110, 160, 255});
    std::string initials;
    bool next = true;
    for (char c : title) {
        if (next && std::isalpha(static_cast<unsigned char>(c))) initials += static_cast<char>(std::toupper(c));
        next = c == ' ';
    }
    initials = initials.substr(0, 2);
    float scale = rc.w / 32.f;
    draw::text(r, rc.x + rc.w * 0.5f, rc.y + rc.h * 0.5f - 4 * scale, initials, scale, SDL_Color{255, 255, 255, 255},
               draw::Align::Center);
}
} // namespace

// Simple pictures for the shipped techs, drawn in a 64x64 box scaled to rc.
void TechTreeScreen::drawIconBuiltin(SDL_Renderer* r, const SDL_FRect& rc, const std::string& id, const std::string& name) {
    const float u = rc.w / 64.f;
    auto X = [&](float v) { return rc.x + v * u; };
    auto Y = [&](float v) { return rc.y + v * (rc.h / 64.f); };
    auto R = [&](float x, float y, float w, float h) { return SDL_FRect{X(x), Y(y), w * u, h * u}; };
    const SDL_Color sun{244, 196, 88, 255}, sunCore{252, 226, 140, 255}, soil{112, 74, 46, 255}, wood{170, 120, 70, 255};
    const SDL_Color leaf{110, 180, 80, 255}, leafDark{60, 130, 60, 255}, white{240, 240, 230, 255}, sky{120, 170, 215, 255};
    const SDL_Color metal{190, 195, 200, 255}, red{200, 70, 58, 255};
    auto sunAt = [&](float cx, float cy, float rad) {
        for (int k = 0; k < 8; ++k) {
            float a = 3.14159265f * k / 4.f;
            draw::thickLine(r, X(cx + std::cos(a) * rad * 1.3f), Y(cy + std::sin(a) * rad * 1.3f),
                            X(cx + std::cos(a) * rad * 1.75f), Y(cy + std::sin(a) * rad * 1.75f), 3.f * u, sun);
        }
        draw::fillCircle(r, X(cx), Y(cy), rad * u, sun);
        draw::fillCircle(r, X(cx - rad * 0.15f), Y(cy - rad * 0.15f), rad * 0.62f * u, sunCore);
    };
    auto speedLines = [&](float x, float y) {
        for (int k = 0; k < 3; ++k) draw::thickLine(r, X(x - 4.f * k), Y(y + 8.f * k), X(x + 10.f - 4.f * k), Y(y + 8.f * k), 3.f * u, white);
    };
    auto dashedRing = [&](float cx, float cy, float rad, SDL_Color c) {
        for (int k = 0; k < 16; k += 2) {
            float a0 = 6.2831853f * k / 16.f, a1 = 6.2831853f * (k + 1) / 16.f;
            draw::thickLine(r, X(cx + std::cos(a0) * rad), Y(cy + std::sin(a0) * rad), X(cx + std::cos(a1) * rad),
                            Y(cy + std::sin(a1) * rad), 3.f * u, c);
        }
    };

    if (id == "barn") {
        // A red barn: gable roof, white trim, big cross-braced doors, a hayloft window.
        const SDL_Color barnRed{176, 62, 50, 255}, roof{120, 48, 40, 255}, trim{236, 228, 214, 255};
        draw::fillRect(r, X(4), Y(56), 56 * u, 4 * u, SDL_Color{90, 140, 70, 255});           // grass
        draw::fillTriangle(r, {X(32), Y(6)}, {X(4), Y(28)}, {X(60), Y(28)}, roof);           // roof
        draw::fillTriangle(r, {X(32), Y(11)}, {X(10), Y(28)}, {X(54), Y(28)}, barnRed);      // gable
        draw::fillRect(r, X(10), Y(28), 44 * u, 29 * u, barnRed);                           // walls
        draw::thickLine(r, X(32), Y(6), X(4), Y(28), 2.5f * u, trim);
        draw::thickLine(r, X(32), Y(6), X(60), Y(28), 2.5f * u, trim);
        draw::fillRect(r, X(27), Y(16), 10 * u, 8 * u, trim);                               // hayloft
        draw::fillRect(r, X(29), Y(18), 6 * u, 4 * u, SDL_Color{70, 40, 30, 255});
        draw::fillRect(r, X(20), Y(34), 24 * u, 23 * u, trim);                              // doors
        draw::fillRect(r, X(22), Y(36), 9 * u, 21 * u, barnRed);
        draw::fillRect(r, X(33), Y(36), 9 * u, 21 * u, barnRed);
        draw::thickLine(r, X(22), Y(36), X(31), Y(57), 2.f * u, trim);
        draw::thickLine(r, X(31), Y(36), X(22), Y(57), 2.f * u, trim);
        draw::thickLine(r, X(33), Y(36), X(42), Y(57), 2.f * u, trim);
        draw::thickLine(r, X(42), Y(36), X(33), Y(57), 2.f * u, trim);
    } else if (id == "patch") {
        draw::fillRoundRect(r, X(6), Y(14), 52 * u, 38 * u, 6 * u, wood);
        draw::fillRoundRect(r, X(10), Y(18), 44 * u, 30 * u, 4 * u, soil);
        for (int k = 0; k < 4; ++k) Farm::drawSproutBuiltin(r, R(12.f + (k % 2) * 22.f, 18.f + (k / 2) * 14.f, 18, 16));
        draw::thickLine(r, X(52), Y(6), X(52), Y(18), 3.5f * u, white);
        draw::thickLine(r, X(46), Y(12), X(58), Y(12), 3.5f * u, white);
    } else if (id == "seeds") {
        draw::fillRoundRect(r, X(14), Y(10), 36 * u, 46 * u, 4 * u, SDL_Color{222, 196, 140, 255});
        draw::fillRect(r, X(14), Y(10), 36 * u, 6 * u, SDL_Color{190, 160, 100, 255});
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(32), Y(34), 26 * u);
        for (int k = 0; k < 3; ++k) draw::fillEllipse(r, X(46.f + k * 4.f), Y(54.f - k * 3.f), 2.2f * u, 1.5f * u, SDL_Color{150, 110, 60, 255});
    } else if (id == "daylength") {
        sunAt(32, 32, 13);
    } else if (id == "headstart") {
        sunAt(32, 40, 12);
        draw::fillRect(r, X(4), Y(40), 56 * u, 18 * u, SDL_Color{90, 130, 70, 255});
    } else if (id == "pickspeed") {
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(38), Y(34), 34 * u);
        speedLines(10, 22);
    } else if (id == "growspeed") {
        draw::fillEllipse(r, X(32), Y(52), 20 * u, 6 * u, soil);
        draw::thickLine(r, X(32), Y(52), X(32), Y(26), 3.5f * u, leafDark);
        draw::fillEllipse(r, X(23), Y(30), 10 * u, 5 * u, leaf);
        draw::fillEllipse(r, X(41), Y(24), 10 * u, 5 * u, leaf);
        draw::fillCircle(r, X(48), Y(46), 5 * u, sky);
        draw::fillTriangle(r, {X(43.6f), Y(44)}, {X(52.4f), Y(44)}, {X(48), Y(36)}, sky);
    } else if (id == "value") {
        ui::drawCoinBuiltin(r, R(8, 22, 34, 34));
        ui::drawCoinBuiltin(r, R(24, 8, 34, 34));
    } else if (id == "reach") {
        dashedRing(32, 32, 24, white);
        draw::fillTriangle(r, {X(28), Y(22)}, {X(28), Y(44)}, {X(42), Y(36)}, white); // pointer
    } else if (id == "carrots") {
        croplook::draw(r, "carrot", croplook::defaultColor("carrot"), X(32), Y(32), 52 * u);
    } else if (id == "pumpkins") {
        croplook::draw(r, "pumpkin", croplook::defaultColor("pumpkin"), X(32), Y(34), 52 * u);
    } else if (id == "autopick") {
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(28), Y(36), 36 * u);
        draw::fillTriangle(r, {X(50), Y(6)}, {X(46), Y(18)}, {X(54), Y(18)}, sunCore); // sparkle
        draw::fillTriangle(r, {X(50), Y(30)}, {X(46), Y(18)}, {X(54), Y(18)}, sunCore);
        draw::fillTriangle(r, {X(38), Y(18)}, {X(50), Y(14)}, {X(50), Y(22)}, sunCore);
        draw::fillTriangle(r, {X(62), Y(18)}, {X(50), Y(14)}, {X(50), Y(22)}, sunCore);
    } else if (id == "autopickchance") {
        for (int k = 0; k < 4; ++k) {
            float a = 1.5707963f * k + 0.785f;
            draw::fillCircle(r, X(32 + std::cos(a) * 10), Y(28 + std::sin(a) * 10), 10 * u, k % 2 ? leaf : leafDark);
        }
        draw::thickLine(r, X(32), Y(30), X(40), Y(58), 3.5f * u, leafDark);
    } else if (id == "autopickcount") {
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(20), Y(24), 26 * u);
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(44), Y(24), 26 * u);
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(32), Y(44), 26 * u);
    } else if (id == "autopickradius") {
        dashedRing(32, 32, 26, leaf);
        croplook::draw(r, "lettuce", croplook::defaultColor("lettuce"), X(32), Y(32), 28 * u);
    } else if (id == "farmhand") {
        Farm::drawFarmerBuiltin(r, R(0, 2, 64, 64), 0);
    } else if (id == "farmcrew") {
        Farm::drawFarmerBuiltin(r, R(-8, 0, 52, 52), 0);
        Farm::drawFarmerBuiltin(r, R(20, 12, 52, 52), 0);
    } else if (id == "farmerspeed") {
        draw::fillRoundRect(r, X(24), Y(16), 16 * u, 30 * u, 4 * u, SDL_Color{92, 60, 36, 255});  // boot leg
        draw::fillRoundRect(r, X(24), Y(36), 32 * u, 14 * u, 6 * u, SDL_Color{92, 60, 36, 255});  // foot
        draw::fillRect(r, X(22), Y(48), 36 * u, 4 * u, SDL_Color{50, 34, 22, 255});              // sole
        speedLines(8, 22);
    } else if (id == "farmerpick") {
        draw::thickLine(r, X(14), Y(12), X(44), Y(44), 6.f * u, metal);
        draw::thickLine(r, X(50), Y(12), X(20), Y(44), 6.f * u, metal);
        draw::fillCircle(r, X(32), Y(28), 3 * u, SDL_Color{90, 95, 100, 255});
        draw::circleOutline(r, X(17), Y(50), 8 * u, red);
        draw::circleOutline(r, X(47), Y(50), 8 * u, red);
        draw::fillCircle(r, X(17), Y(50), 7 * u, red);
        draw::fillCircle(r, X(47), Y(50), 7 * u, red);
        draw::fillCircle(r, X(17), Y(50), 4 * u, SDL_Color{44, 50, 56, 255});
        draw::fillCircle(r, X(47), Y(50), 4 * u, SDL_Color{44, 50, 56, 255});
    } else if (id.rfind("crop_", 0) == 0 && techdata::cropIndex(id.substr(5)) >= 0) {
        // A crop as an icon: its artwork if there is any, else its built-in look.
        Farm::drawCrop(r, techdata::cropIndex(id.substr(5)), X(32), Y(32), 56 * u);
    } else {
        drawInitialsIcon(r, rc, name);
    }
}

