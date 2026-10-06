#include "Farm.h"

#include "Art.h"
#include "Draw.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kBedCenterX = 640.f;
constexpr float kBedCenterY = 398.f;
constexpr float kMaxBedRX = 590.f;     // the bed never gets wider than 2 x this...
constexpr float kMaxBedRY = 296.f;     // ...or taller than 2 x this
constexpr float kMaxPlantPx = 72.f;    // plants never get bigger than this
constexpr float kBedAspect = 1.6f;     // the bed is a rectangle 1.6 times wider than tall
constexpr float kSpreadGap = 1.0f;     // normally plants don't overlap at all...
constexpr float kMinGap = 0.9f;        // ...and never by more than 10% (centres 0.9 plant-widths apart)
constexpr float kBedMargin = 0.2f;    // soil around the outermost plants, in plant-widths

// Scatters points inside a rectangle (half-width hw, half-height hh) so no
// two are closer than minDist (Bridson's Poisson-disc sampling). Looks
// natural rather than grid-like, and never clumps.
std::vector<SDL_FPoint> scatterInRect(float hw, float hh, float minDist, std::mt19937& g) {
    const float rx = hw, ry = hh;
    std::uniform_real_distribution<float> u(0.f, 1.f);
    const float cell = minDist / std::sqrt(2.f);
    const int gw = static_cast<int>(std::ceil(2.f * rx / cell)) + 1;
    const int gh = static_cast<int>(std::ceil(2.f * ry / cell)) + 1;
    std::vector<int> grid(static_cast<size_t>(gw) * gh, -1);
    std::vector<SDL_FPoint> pts, active;
    auto inside = [&](float x, float y) { return std::fabs(x) <= rx && std::fabs(y) <= ry; };
    auto cellOf = [&](float x, float y) {
        return std::pair<int, int>{static_cast<int>((x + rx) / cell), static_cast<int>((y + ry) / cell)};
    };
    auto fits = [&](float x, float y) {
        auto [cx, cy] = cellOf(x, y);
        for (int j = std::max(0, cy - 2); j <= std::min(gh - 1, cy + 2); ++j)
            for (int i = std::max(0, cx - 2); i <= std::min(gw - 1, cx + 2); ++i) {
                int k = grid[static_cast<size_t>(j) * gw + i];
                if (k >= 0) {
                    float dx = pts[k].x - x, dy = pts[k].y - y;
                    if (dx * dx + dy * dy < minDist * minDist) return false;
                }
            }
        return true;
    };
    auto addPoint = [&](float x, float y) {
        auto [cx, cy] = cellOf(x, y);
        grid[static_cast<size_t>(cy) * gw + cx] = static_cast<int>(pts.size());
        pts.push_back({x, y});
        active.push_back({x, y});
    };
    addPoint((u(g) - 0.5f) * minDist * 0.5f, (u(g) - 0.5f) * minDist * 0.5f);
    while (!active.empty()) {
        size_t idx = static_cast<size_t>(u(g) * active.size()) % active.size();
        SDL_FPoint p = active[idx];
        bool placed = false;
        for (int k = 0; k < 30; ++k) {
            float a = u(g) * 2.f * kPi, dist = minDist * (1.f + 0.35f * u(g)); // close neighbours = a full-looking bed
            float x = p.x + std::cos(a) * dist, y = p.y + std::sin(a) * dist;
            if (inside(x, y) && fits(x, y)) {
                addPoint(x, y);
                placed = true;
                break;
            }
        }
        if (!placed) {
            active[idx] = active.back();
            active.pop_back();
        }
    }
    return pts;
}

const CropInfo kCrops[] = {
    {"Lettuce", 1.0, 1.0f, 1.0f},
    {"Carrot", 4.0, 1.6f, 1.25f},
    {"Pumpkin", 15.0, 2.6f, 1.6f},
};

const SDL_Color kSoil{112, 74, 46, 255};
const SDL_Color kSoilDark{88, 56, 34, 255};
const SDL_Color kSoilHover{135, 92, 58, 255};
const SDL_Color kLeaf{70, 160, 60, 255};
const SDL_Color kLeafDark{44, 120, 44, 255};
const SDL_Color kLeafLight{140, 210, 100, 255};
const SDL_Color kOrange{240, 130, 30, 255};
const SDL_Color kOrangeDark{200, 95, 20, 255};
const SDL_Color kPumpkin{235, 120, 25, 255};
const SDL_Color kPumpkinDark{190, 85, 15, 255};
} // namespace

const CropInfo& cropInfo(Crop c) { return kCrops[static_cast<int>(c)]; }

int Farm::plantCount(const Stats& stats) {
    int room = std::max(1, stats.patchSize) * std::max(1, stats.patchSize);
    return std::clamp(stats.maxCrops, 1, room);
}

void Farm::startDay(const Stats& stats, std::mt19937& rng) {
    stats_ = stats;
    n_ = std::max(1, stats.patchSize);
    tiles_.assign(plantCount(stats), Tile{});
    plant(static_cast<Uint32>(rng()));

    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (auto& t : tiles_) {
        t.crop = randomCrop(rng);
        // Stagger growth so everything doesn't ripen at the same moment.
        t.growth = unit(rng) * 0.6f;
        if (unit(rng) < stats.headStart) t.growth = 1.f;
        t.wobble = unit(rng) * 6.28f;
    }
    particles_.clear();
    floatTexts_.clear();
    harvests_.clear();
    timeLeft_ = stats.dayLength;
    clock_ = 0.f;
    earnedToday_ = 0.0;
    pickedToday_ = 0;
    pickedByCrop_[0] = pickedByCrop_[1] = pickedByCrop_[2] = 0;
}

void Farm::plant(Uint32 seed) {
    seed_ = seed;
    const int count = static_cast<int>(tiles_.size());
    // The bed's size comes from the patch size, not from how many crops are
    // growing, so buying more crops fills the same bed up. Lay out ~25% more
    // spots than the patch has room for: the spare ones are the gaps a
    // vegetable can move to when it regrows.
    const int room = n_ * n_;
    const int capacity = room + std::max(2, (room + 3) / 4);

    struct Layout {
        std::vector<SDL_FPoint> pts;
        float d = 0.f, rx = 0.f, ry = 0.f, scale = 1.f;
    };
    // Lays the plants out with centres at least `gap` plant-widths apart.
    auto layoutWith = [&](float gap) {
        std::mt19937 g(seed); // same seed = same layout, so saved games come back identical
        Layout L;
        // Start with plants as big as will comfortably fit, then make the bed
        // bigger until every plant has a spot.
        L.d = std::min(kMaxPlantPx, std::sqrt(kPi * kMaxBedRX * kMaxBedRY / (capacity * 2.1f)));
        float bedRX = 1e9f;
        float area = capacity * 1.1f * L.d * L.d * gap * gap;
        // Scatter a few times and keep the most compact layout.
        for (int tries = 0, found = 0; tries < 80 && found < 6; ++tries) {
            float hh = std::sqrt(area / (4.f * kBedAspect)), hw = hh * kBedAspect;
            std::vector<SDL_FPoint> cand = scatterInRect(hw, hh, L.d * gap, g);
            if (static_cast<int>(cand.size()) < capacity) {
                area *= 1.06f; // not enough room yet - grow the bed a little
                continue;
            }
            ++found;
            // Keep the ones nearest the middle so the planted area stays a tidy rectangle.
            auto rectDist = [hw, hh](const SDL_FPoint& p) { return std::max(std::fabs(p.x) / hw, std::fabs(p.y) / hh); };
            std::sort(cand.begin(), cand.end(),
                      [&rectDist](const SDL_FPoint& a, const SDL_FPoint& b) { return rectDist(a) < rectDist(b); });
            cand.resize(capacity);
            // Centre the group, then size the bed around it.
            float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            for (const auto& p : cand) {
                minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
            }
            float ox = (minX + maxX) * 0.5f, oy = (minY + maxY) * 0.5f;
            for (auto& p : cand) { p.x -= ox; p.y -= oy; }
            float edge = L.d * (0.5f + kBedMargin);
            // Smallest rectangle of the right shape that holds every plant with some soil around it.
            float need = 0.f; // half-width
            for (const auto& p : cand) {
                need = std::max({need, std::fabs(p.x) + edge, (std::fabs(p.y) + edge) * kBedAspect});
            }
            need /= 0.93f; // room for the bed's rim
            if (need < bedRX) {
                bedRX = need;
                L.pts = cand;
            }
        }
        if (L.pts.empty()) L.pts.assign(capacity, SDL_FPoint{0.f, 0.f}); // can't happen in practice
        L.rx = bedRX;
        L.ry = bedRX / kBedAspect;
        L.scale = std::min({1.f, kMaxBedRX / L.rx, kMaxBedRY / L.ry});
        return L;
    };

    // Keep every vegetable fully separate while the bed fits on screen. Once
    // it's as big as it can get, let neighbours overlap by up to 10% rather
    // than shrinking every plant further.
    Layout L = layoutWith(kSpreadGap);
    if (L.scale < 1.f) {
        Layout tight = layoutWith(kMinGap);
        if (tight.d * tight.scale > L.d * L.scale) L = std::move(tight);
    }

    plantSize_ = L.d * L.scale;
    bedRX_ = L.rx * L.scale;
    bedRY_ = L.ry * L.scale;
    bedX_ = kBedCenterX;
    bedY_ = kBedCenterY;
    spots_.resize(capacity);
    for (int i = 0; i < capacity; ++i) spots_[i] = {bedX_ + L.pts[i].x * L.scale, bedY_ + L.pts[i].y * L.scale};

    // Plants start on a random selection of the spots, so the gaps are scattered.
    std::vector<int> order(capacity);
    for (int i = 0; i < capacity; ++i) order[i] = i;
    std::mt19937 g(seed ^ 0x9E3779B9u);
    std::shuffle(order.begin(), order.end(), g);
    for (int i = 0; i < count; ++i) placeOn(i, order[i]);
    sortDrawOrder();
}

void Farm::placeOn(int index, int spot) {
    Tile& t = tiles_[index];
    t.spot = spot;
    t.x = spots_[spot].x;
    t.y = spots_[spot].y;
}

void Farm::sortDrawOrder() {
    const int count = static_cast<int>(tiles_.size());
    drawOrder_.resize(count);
    for (int i = 0; i < count; ++i) drawOrder_[i] = i;
    std::sort(drawOrder_.begin(), drawOrder_.end(), [this](int a, int b) { return tiles_[a].y < tiles_[b].y; });
}

// Moves a plant to a random free spot somewhere else in the bed.
void Farm::moveToFreeSpot(int index, std::mt19937& rng) {
    std::vector<char> used(spots_.size(), 0);
    for (const Tile& t : tiles_)
        if (t.spot >= 0 && t.spot < static_cast<int>(spots_.size())) used[t.spot] = 1;
    std::vector<int> free;
    for (int i = 0; i < static_cast<int>(spots_.size()); ++i)
        if (!used[i]) free.push_back(i);
    if (free.empty()) return; // nowhere else to go: regrow where it was
    std::uniform_int_distribution<size_t> pick(0, free.size() - 1);
    placeOn(index, free[pick(rng)]);
    sortDrawOrder();
}

std::string Farm::serialize() const {
    std::ostringstream out;
    out.precision(9);
    out << "farm " << n_ << " " << timeLeft_ << " " << earnedToday_ << " " << pickedToday_ << " "
        << pickedByCrop_[0] << " " << pickedByCrop_[1] << " " << pickedByCrop_[2] << " " << seed_ << "\n";
    for (const Tile& t : tiles_) {
        out << "tile " << static_cast<int>(t.crop) << " " << t.growth << " " << t.pick << " " << t.spot << "\n";
    }
    return out.str();
}

bool Farm::restore(const std::vector<std::string>& lines, const Stats& stats) {
    if (lines.empty()) return false;
    std::istringstream head(lines[0]);
    std::string key;
    int n = 0, picked = 0, p0 = 0, p1 = 0, p2 = 0;
    float timeLeft = 0.f;
    double earned = 0.0;
    head >> key >> n >> timeLeft >> earned >> picked >> p0 >> p1 >> p2;
    if (!head || key != "farm" || n != stats.patchSize) return false;
    Uint32 seed = 1;
    if (!(head >> seed)) seed = 1; // saves from before the garden layout have no seed
    // One tile line per growing vegetable. If the number of crops changed
    // since the save (e.g. the tech tree was rebalanced), start a fresh day.
    const int count = static_cast<int>(lines.size()) - 1;
    if (count != plantCount(stats)) return false;

    std::vector<Tile> tiles(count);
    std::vector<int> savedSpots(count, -1);
    for (int i = 0; i < count; ++i) {
        std::istringstream ss(lines[1 + i]);
        int crop = 0;
        ss >> key >> crop >> tiles[i].growth >> tiles[i].pick;
        if (!ss || key != "tile" || crop < 0 || crop > stats.cropTier) return false;
        if (!(ss >> savedSpots[i])) savedSpots[i] = -1; // older saves don't record spots
        tiles[i].crop = static_cast<Crop>(crop);
        tiles[i].growth = std::clamp(tiles[i].growth, 0.f, 1.f);
        tiles[i].pick = std::clamp(tiles[i].pick, 0.f, 0.99f);
        tiles[i].wobble = static_cast<float>(i) * 1.7f;
    }

    stats_ = stats;
    n_ = n;
    tiles_ = std::move(tiles);
    plant(seed); // same seed = the same bed and the same spots as when it was saved
    // Put each plant back on the spot it was on (if the save says, and it makes sense).
    std::vector<char> taken(spots_.size(), 0);
    bool valid = true;
    for (int s : savedSpots) {
        if (s < 0 || s >= static_cast<int>(spots_.size()) || taken[s]) {
            valid = false;
            break;
        }
        taken[s] = 1;
    }
    if (valid) {
        for (int i = 0; i < count; ++i) placeOn(i, savedSpots[i]);
        sortDrawOrder();
    }
    particles_.clear();
    floatTexts_.clear();
    harvests_.clear();
    timeLeft_ = std::clamp(timeLeft, 0.f, stats.dayLength);
    earnedToday_ = earned;
    pickedToday_ = picked;
    pickedByCrop_[0] = p0;
    pickedByCrop_[1] = p1;
    pickedByCrop_[2] = p2;
    return true;
}

void Farm::applyStats(const Stats& stats, std::mt19937& rng) {
    bool resized = stats.patchSize != n_ || tiles_.empty();
    // A longer day also lengthens today; a shorter one cuts it down.
    float left = timeLeft_ + std::max(0.f, stats.dayLength - stats_.dayLength);
    if (resized) {
        double earned = earnedToday_;
        int picked = pickedToday_;
        int byCrop[3] = {pickedByCrop_[0], pickedByCrop_[1], pickedByCrop_[2]};
        startDay(stats, rng); // replants a patch of the new size
        earnedToday_ = earned;
        pickedToday_ = picked;
        for (int i = 0; i < 3; ++i) pickedByCrop_[i] = byCrop[i];
    } else {
        stats_ = stats;
        // More (or fewer) crops at once: sprout new ones in empty spots, or pull some up.
        const int want = plantCount(stats);
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        while (static_cast<int>(tiles_.size()) > want) tiles_.pop_back();
        while (static_cast<int>(tiles_.size()) < want) {
            Tile t;
            t.crop = randomCrop(rng);
            t.wobble = unit(rng) * 6.28f;
            t.pop = 1.f;
            tiles_.push_back(t);
            moveToFreeSpot(static_cast<int>(tiles_.size()) - 1, rng);
        }
        if (hovered_ >= static_cast<int>(tiles_.size())) hovered_ = -1;
        sortDrawOrder();
        // Crops that are no longer unlocked get replanted.
        for (auto& t : tiles_) {
            if (static_cast<int>(t.crop) > stats.cropTier) {
                t.crop = randomCrop(rng);
                t.growth = 0.f;
                t.pick = 0.f;
            }
        }
    }
    timeLeft_ = std::min(left, stats.dayLength);
}

void Farm::ripenAll() {
    for (auto& t : tiles_) t.growth = 1.f;
}

void Farm::setTimeLeft(float seconds) { timeLeft_ = std::max(0.f, seconds); }

std::vector<Crop> Farm::takeHarvests() {
    std::vector<Crop> out;
    out.swap(harvests_);
    return out;
}

Crop Farm::randomCrop(std::mt19937& rng) const {
    std::uniform_int_distribution<int> roll(0, 99);
    int r = roll(rng);
    switch (stats_.cropTier) {
    case 0: return Crop::Lettuce;
    case 1: return r < 60 ? Crop::Lettuce : Crop::Carrot;
    default: return r < 45 ? Crop::Lettuce : (r < 80 ? Crop::Carrot : Crop::Pumpkin);
    }
}

SDL_FRect Farm::plantRect(int index) const {
    const Tile& t = tiles_[index];
    return SDL_FRect{t.x - plantSize_ * 0.5f, t.y - plantSize_ * 0.5f, plantSize_, plantSize_};
}

int Farm::plantAt(float x, float y) const {
    int best = -1;
    float bestD2 = plantSize_ * plantSize_ * 0.25f; // inside the plant's circle
    for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
        float dx = tiles_[i].x - x, dy = tiles_[i].y - y, d2 = dx * dx + dy * dy;
        if (d2 <= bestD2) {
            best = i;
            bestD2 = d2;
        }
    }
    return best;
}

float Farm::pickRadius() const {
    // Reach 0 means "only the plant directly under the pointer".
    return stats_.reach <= 0 ? 0.f : plantSize_ * (0.55f + 0.75f * stats_.reach);
}

bool Farm::tileInReach(int index, float mx, float my) const {
    if (index == hovered_) return true;
    float radius = pickRadius();
    if (radius <= 0.f) return false;
    float dx = tiles_[index].x - mx, dy = tiles_[index].y - my;
    return dx * dx + dy * dy <= radius * radius;
}

void Farm::update(float dt, float mouseX, float mouseY, bool mouseInside, double& coins, std::mt19937& rng) {
    mouseX_ = mouseX;
    mouseY_ = mouseY;
    mouseInside_ = mouseInside;
    clock_ += dt;
    hovered_ = mouseInside ? plantAt(mouseX, mouseY) : -1;

    if (timeLeft_ > 0.f) {
        if (!timerFrozen_) timeLeft_ = std::max(0.f, timeLeft_ - dt);
        for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
            Tile& t = tiles_[i];
            const CropInfo& info = cropInfo(t.crop);
            t.pop = std::max(0.f, t.pop - dt * 3.f);
            if (t.growth < 1.f) {
                t.growth = std::min(1.f, t.growth + dt / (stats_.growTime * info.growMult));
                continue;
            }
            if (mouseInside && tileInReach(i, mouseX, mouseY)) {
                t.pick += dt / (stats_.pickTime * info.pickMult);
                if (t.pick >= 1.f) harvest(i, coins, rng);
            } else {
                // Progress drains away if you move off before finishing.
                t.pick = std::max(0.f, t.pick - dt * 1.5f);
            }
        }
    }

    for (auto& p : particles_) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += 500.f * dt;
        p.life -= dt;
    }
    std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.f; });

    for (auto& f : floatTexts_) {
        f.y -= 45.f * dt;
        f.life -= dt;
    }
    std::erase_if(floatTexts_, [](const FloatText& f) { return f.life <= 0.f; });
}

void Farm::harvest(int index, double& coins, std::mt19937& rng) {
    Tile& t = tiles_[index];
    const CropInfo& info = cropInfo(t.crop);
    double gain = info.value * stats_.valueMult;
    coins += gain;
    earnedToday_ += gain;
    ++pickedToday_;
    ++pickedByCrop_[static_cast<int>(t.crop)];
    harvests_.push_back(t.crop);

    float cx = t.x, cy = t.y;

    SDL_Color burst = t.crop == Crop::Lettuce ? kLeafLight : (t.crop == Crop::Carrot ? kOrange : kPumpkin);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (int k = 0; k < 10; ++k) {
        float a = unit(rng) * 6.2832f;
        float speed = 80.f + unit(rng) * 160.f;
        particles_.push_back({cx, cy, std::cos(a) * speed, std::sin(a) * speed - 140.f,
                              0.4f + unit(rng) * 0.4f, 2.f + unit(rng) * 3.f,
                              k % 3 == 0 ? SDL_Color{255, 225, 90, 255} : burst});
    }
    floatTexts_.push_back({cx, cy - plantSize_ * 0.3f, 1.0f, "+" + draw::number(gain), SDL_Color{255, 230, 90, 255}});

    // Replant straight away, in a new random spot.
    t.crop = randomCrop(rng);
    t.growth = 0.f;
    t.pick = 0.f;
    t.pop = 1.f;
    moveToFreeSpot(index, rng);
}

const char* cropArtName(Crop c) {
    switch (c) {
    case Crop::Lettuce: return "crops/lettuce";
    case Crop::Carrot: return "crops/carrot";
    case Crop::Pumpkin: return "crops/pumpkin";
    }
    return "crops/lettuce";
}

void Farm::drawVegetable(SDL_Renderer* r, const Tile& t, float cx, float cy, float s) const {
    std::string name = cropArtName(t.crop);
    if (t.growth < 1.f) {
        // Growing: a sprout that gets bigger as it grows, anchored at its base.
        float size = s * (0.25f + 0.75f * t.growth) * (1.f - t.pop * 0.5f);
        float base = cy + s * 0.18f;
        art::drawFirst(r, {name + "_sprout", "crops/sprout"}, SDL_FRect{cx - size * 0.5f, base - size * 0.68f, size, size});
        return;
    }

    // Ripe: gentle bob, and a shake while being picked.
    float bob = std::sin(clock_ * 3.f + t.wobble) * s * 0.015f;
    float shake = t.pick > 0.f ? std::sin(clock_ * 45.f) * s * 0.035f * t.pick : 0.f;
    drawCrop(r, t.crop, cx + shake, cy + bob, s);
}

void Farm::drawCrop(SDL_Renderer* r, Crop crop, float cx, float cy, float s) {
    art::draw(r, cropArtName(crop), SDL_FRect{cx - s * 0.5f, cy - s * 0.5f, s, s});
}

// ---------------------------------------------------------------------------
// Built-in art (used when there's no image in assets/, and for the templates)
// ---------------------------------------------------------------------------

void Farm::drawSproutBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float s = rc.w, cx = rc.x + rc.w * 0.5f;
    float h = s * 0.32f;
    float base = rc.y + rc.h * 0.68f;
    draw::thickLine(r, cx, base, cx, base - h, std::max(2.f, s * 0.04f), kLeafDark);
    draw::fillEllipse(r, cx - h * 0.45f, base - h * 0.85f, h * 0.45f, h * 0.22f, kLeaf);
    draw::fillEllipse(r, cx + h * 0.45f, base - h * 0.95f, h * 0.45f, h * 0.22f, kLeafLight);
}

// A small mound of loose soil under a plant (drawn in the plant's square).
void Farm::drawSoilBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.70f;
    draw::fillEllipse(r, cx, cy + rc.h * 0.02f, rc.w * 0.44f, rc.h * 0.18f, kSoilDark);
    draw::fillEllipse(r, cx, cy, rc.w * 0.40f, rc.h * 0.14f, hovered ? kSoilHover : kSoil);
}

// The garden bed: a rectangular raised bed - a wooden frame filled with soil.
void Farm::drawBedBuiltin(SDL_Renderer* r, const SDL_FRect& rc, Uint32 seed) {
    const SDL_Color wood{150, 105, 60, 255}, woodDark{118, 80, 44, 255}, soil{104, 70, 42, 255};
    float rim = std::max(6.f, std::min(rc.w, rc.h) * 0.045f);
    // Frame, with plank seams and corner posts.
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rim * 0.6f, wood);
    float plank = std::max(rim * 5.f, 60.f);
    for (float x = rc.x + plank; x < rc.x + rc.w - rim; x += plank) {
        draw::fillRect(r, x, rc.y, 2.f, rim, woodDark);
        draw::fillRect(r, x, rc.y + rc.h - rim, 2.f, rim, woodDark);
    }
    for (float y = rc.y + plank; y < rc.y + rc.h - rim; y += plank) {
        draw::fillRect(r, rc.x, y, rim, 2.f, woodDark);
        draw::fillRect(r, rc.x + rc.w - rim, y, rim, 2.f, woodDark);
    }
    for (float cx : {rc.x, rc.x + rc.w - rim * 1.3f})
        for (float cy : {rc.y, rc.y + rc.h - rim * 1.3f})
            draw::fillRoundRect(r, cx, cy, rim * 1.3f, rim * 1.3f, rim * 0.3f, woodDark);
    // Soil.
    SDL_FRect in{rc.x + rim, rc.y + rim, rc.w - rim * 2.f, rc.h - rim * 2.f};
    draw::fillRect(r, in.x, in.y, in.w, in.h, soil);
    draw::fillRect(r, in.x, in.y, in.w, std::max(2.f, rim * 0.35f), SDL_Color{80, 52, 30, 255}); // shadow under the top plank
    // A scattering of darker clods for texture.
    Uint32 g = seed * 2654435761u + 1u;
    auto rnd = [&g] {
        g = g * 1664525u + 1013904223u;
        return static_cast<float>(g >> 8) / static_cast<float>(1u << 24);
    };
    int clods = static_cast<int>(in.w * in.h / 900.f);
    for (int i = 0; i < clods; ++i) {
        float s = 1.5f + rnd() * 2.5f;
        float x = in.x + s * 2.f + rnd() * (in.w - s * 4.f), y = in.y + s * 2.f + rnd() * (in.h - s * 4.f);
        draw::fillEllipse(r, x, y, s * 1.6f, s, i % 3 ? kSoilDark : SDL_Color{126, 86, 54, 255}, 10);
    }
}

void Farm::drawCropBuiltin(SDL_Renderer* r, Crop crop, float cx, float cy, float s) {
    switch (crop) {
    case Crop::Lettuce:
        draw::fillCircle(r, cx, cy + s * 0.04f, s * 0.30f, kLeafDark);
        draw::fillCircle(r, cx - s * 0.12f, cy, s * 0.18f, kLeaf);
        draw::fillCircle(r, cx + s * 0.12f, cy, s * 0.18f, kLeaf);
        draw::fillCircle(r, cx, cy - s * 0.08f, s * 0.18f, kLeaf);
        draw::fillCircle(r, cx, cy + s * 0.01f, s * 0.11f, kLeafLight);
        break;
    case Crop::Carrot: {
        float line = std::max(2.f, s * 0.026f);
        // Leafy top...
        draw::fillEllipse(r, cx - s * 0.10f, cy - s * 0.24f, s * 0.06f, s * 0.16f, kLeafDark);
        draw::fillEllipse(r, cx + s * 0.10f, cy - s * 0.24f, s * 0.06f, s * 0.16f, kLeafDark);
        draw::fillEllipse(r, cx, cy - s * 0.28f, s * 0.06f, s * 0.18f, kLeaf);
        // ...and the orange root.
        draw::fillTriangle(r, {cx - s * 0.15f, cy - s * 0.10f}, {cx + s * 0.15f, cy - s * 0.10f},
                           {cx, cy + s * 0.34f}, kOrange);
        draw::fillEllipse(r, cx, cy - s * 0.10f, s * 0.15f, s * 0.06f, kOrange);
        draw::thickLine(r, cx - s * 0.08f, cy + s * 0.02f, cx - s * 0.01f, cy + s * 0.02f, line, kOrangeDark);
        draw::thickLine(r, cx + s * 0.02f, cy + s * 0.12f, cx + s * 0.07f, cy + s * 0.12f, line, kOrangeDark);
        break;
    }
    case Crop::Pumpkin:
        draw::fillEllipse(r, cx, cy + s * 0.06f, s * 0.36f, s * 0.27f, kPumpkinDark);
        draw::fillEllipse(r, cx - s * 0.14f, cy + s * 0.06f, s * 0.17f, s * 0.25f, kPumpkin);
        draw::fillEllipse(r, cx + s * 0.14f, cy + s * 0.06f, s * 0.17f, s * 0.25f, kPumpkin);
        draw::fillEllipse(r, cx, cy + s * 0.06f, s * 0.13f, s * 0.27f, SDL_Color{250, 145, 45, 255});
        draw::fillRect(r, cx - s * 0.03f, cy - s * 0.30f, s * 0.06f, s * 0.12f, SDL_Color{110, 80, 30, 255});
        draw::fillEllipse(r, cx + s * 0.11f, cy - s * 0.24f, s * 0.09f, s * 0.04f, kLeaf);
        break;
    }
}

void Farm::drawPickBarBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool fill) {
    if (fill) draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rc.h * 0.5f, SDL_Color{255, 220, 70, 255});
    else draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rc.h * 0.5f, SDL_Color{30, 20, 10, 220});
}

void Farm::drawReachBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
    draw::fillCircle(r, cx, cy, rc.w * 0.5f, SDL_Color{255, 240, 160, 35}, 48);
    draw::fillCircle(r, cx, cy, std::max(3.f, rc.w * 0.015f), SDL_Color{255, 240, 160, 160});
}

void Farm::render(SDL_Renderer* r) const {
    // The bed.
    SDL_FRect bed{bedX_ - bedRX_, bedY_ - bedRY_, bedRX_ * 2.f, bedRY_ * 2.f};
    if (art::has("farm/bed")) {
        art::draw(r, "farm/bed", bed);
    } else {
        draw::fillRoundRect(r, bed.x + 4.f, bed.y + 8.f, bed.w, bed.h, 8.f, SDL_Color{0, 0, 0, 55}); // shadow
        drawBedBuiltin(r, bed, seed_);
    }

    // Plants, back to front so nearer ones overlap the ones behind.
    bool active = mouseInside_ && timeLeft_ > 0.f;
    for (int i : drawOrder_) {
        const Tile& t = tiles_[i];
        art::drawVariant(r, "farm/soil", active && i == hovered_ ? "_hover" : "", plantRect(i));
        drawVegetable(r, t, t.x, t.y, plantSize_);
    }

    // Picking bars on top of everything else in the bed.
    for (int i : drawOrder_) {
        const Tile& t = tiles_[i];
        if (t.growth < 1.f || t.pick <= 0.01f) continue;
        float bw = plantSize_ * 0.7f, bh = std::max(4.f, plantSize_ * 0.08f);
        float bx = t.x - bw * 0.5f, by = t.y + plantSize_ * 0.36f;
        art::draw(r, "ui/pick_bar_back", SDL_FRect{bx - 1, by - 1, bw + 2, bh + 2});
        float f = std::min(1.f, t.pick);
        if (!art::drawFill(r, "ui/pick_bar_fill", SDL_FRect{bx, by, bw, bh}, f))
            drawPickBarBuiltin(r, SDL_FRect{bx, by, bw * f, bh}, true);
    }

    // Picking circle for the Wide Reach upgrade.
    float radius = pickRadius();
    if (radius > 0.f && active) {
        art::draw(r, "fx/reach_circle", SDL_FRect{mouseX_ - radius, mouseY_ - radius, radius * 2, radius * 2});
    }

    if (debugView_) {
        // Plant circles, growth / pick progress, and the picking area.
        for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
            const Tile& t = tiles_[i];
            draw::circleOutline(r, t.x, t.y, plantSize_ * 0.5f,
                                i == hovered_ ? SDL_Color{0, 255, 255, 255} : SDL_Color{255, 0, 255, 170});
            std::string g = t.growth >= 1.f ? "RIPE" : draw::strf("%d%%", static_cast<int>(t.growth * 100));
            draw::textShadow(r, t.x, t.y - 10, g, 1.f, SDL_Color{255, 255, 255, 255}, draw::Align::Center);
            if (t.pick > 0.f)
                draw::textShadow(r, t.x, t.y, draw::strf("pick %d%%", static_cast<int>(t.pick * 100)), 1.f,
                                 SDL_Color{255, 230, 90, 255}, draw::Align::Center);
            std::string name = cropInfo(t.crop).name;
            if (draw::textWidth(name, 1.f) > plantSize_ - 6) name = name.substr(0, 1); // small plants: just the initial
            draw::text(r, t.x, t.y + 10, name, 1.f, SDL_Color{255, 255, 255, 200}, draw::Align::Center);
        }
        draw::outlineRect(r, bed.x, bed.y, bed.w, bed.h, SDL_Color{255, 255, 0, 120});
        float rad = pickRadius();
        if (rad > 0.f && mouseInside_) draw::circleOutline(r, mouseX_, mouseY_, rad, SDL_Color{0, 255, 255, 200});
    }

    for (const auto& p : particles_) {
        Uint8 a = static_cast<Uint8>(255.f * std::clamp(p.life / 0.5f, 0.f, 1.f));
        SDL_Color c = draw::withAlpha(p.color, a);
        if (!art::drawTinted(r, "fx/particle", SDL_FRect{p.x - p.size, p.y - p.size, p.size * 2, p.size * 2}, c))
            draw::fillCircle(r, p.x, p.y, p.size, c, 10);
    }
    for (const auto& f : floatTexts_) {
        Uint8 a = static_cast<Uint8>(255.f * std::clamp(f.life, 0.f, 1.f));
        draw::textShadow(r, f.x, f.y, f.text, 2.f, draw::withAlpha(f.color, a), draw::Align::Center);
    }
}
