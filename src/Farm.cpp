#include "Farm.h"

#include "Art.h"
#include "CropLooks.h"
#include "Draw.h"
#include "Palette.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <sstream>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kBedCenterX = 640.f;
constexpr float kBedCenterY = 366.f;
// The camera zooms out so the bed always fits in this space (between the
// HUD at the top and the End Day button at the bottom).
constexpr float kMaxBedRX = 590.f;     // the bed is never shown wider than 2 x this...
constexpr float kMaxBedRY = 280.f;     // ...or taller than 2 x this
constexpr float kMaxPlantPx = 72.f;    // a plant's size in the world (on screen when the camera isn't zoomed out)
constexpr float kZoomSeconds = 1.4f;   // camera zoom when the patch has changed size since yesterday
constexpr float kBedAspect = 1.6f;     // small beds are 1.6 times wider than tall; big ones stretch to fill the screen
constexpr float kSpreadGap = 1.0f;     // normally plants don't overlap at all...
constexpr float kMinGap = 0.9f;        // ...and never by more than 10% (centres 0.9 plant-widths apart)
constexpr float kBedMargin = 0.2f;    // soil around the outermost plants, in plant-widths
constexpr float kFenceTile = 46.f;     // fence tiles, in world pixels (a plant is kMaxPlantPx)
constexpr float kFenceGap = 14.f;      // grass between the bed and the fence, in world pixels

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

const SDL_Color kSoil{112, 74, 46, 255};
const SDL_Color kSoilDark{88, 56, 34, 255};
const SDL_Color kSoilHover{135, 92, 58, 255};
} // namespace

int cropCount() { return static_cast<int>(techdata::crops().size()); }

const techdata::CropDef& cropDef(Crop c) {
    const auto& list = techdata::crops();
    return list[c >= 0 && c < static_cast<int>(list.size()) ? c : 0];
}

std::string cropArtName(Crop c) { return "crops/" + cropDef(c).id; }

SDL_Color cropColor(Crop c) {
    const auto& d = cropDef(c);
    return croplook::parseColor(d.color, d.look);
}

int Farm::plantCount(const Stats& stats) {
    int room = std::max(1, stats.patchSize) * std::max(1, stats.patchSize);
    return std::clamp(stats.maxCrops, 1, room);
}

void Farm::startDay(const Stats& stats, std::mt19937& rng) {
    const float oldPlantSize = tiles_.empty() ? 0.f : plantSize_;
    stats_ = stats;
    n_ = std::max(1, stats.patchSize);
    tiles_.assign(plantCount(stats), Tile{});
    plant(static_cast<Uint32>(rng()));
    // If the camera had to zoom out (or in) since yesterday, start at
    // yesterday's zoom and glide to the new one.
    if (oldPlantSize > 0.f && std::fabs(oldPlantSize - plantSize_) > 0.5f) {
        zoomFrom_ = oldPlantSize / plantSize_;
        zoomT_ = 0.f;
    } else {
        zoomT_ = 1.f;
    }

    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (auto& t : tiles_) {
        t.crop = randomCrop(rng);
        // Stagger growth so everything doesn't ripen at the same moment.
        t.growth = unit(rng) * 0.6f;
        if (unit(rng) < stats.headStart) t.growth = 1.f;
        t.wobble = unit(rng) * 6.28f;
    }
    farmers_.clear();
    syncFarmers(rng);
    particles_.clear();
    floatTexts_.clear();
    harvests_.clear();
    timeLeft_ = stats.dayLength;
    clock_ = 0.f;
    earnedToday_ = 0.0;
    pickedToday_ = 0;
    pickedByCrop_.assign(cropCount(), 0);
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
    // Big beds take the screen's shape (wider), so the camera needn't zoom out as far.
    const float aspect = kBedAspect + (kMaxBedRX / kMaxBedRY - kBedAspect) * std::clamp((room - 25.f) / 75.f, 0.f, 1.f);
    const int layouts = capacity > 200 ? 2 : 6; // big beds: fewer tries, so the day starts without a pause

    struct Layout {
        std::vector<SDL_FPoint> pts;
        float d = 0.f, rx = 0.f, ry = 0.f, scale = 1.f;
    };
    // Lays the plants out with centres at least `gap` plant-widths apart.
    auto layoutWith = [&](float gap) {
        std::mt19937 g(seed); // same seed = same layout, so saved games come back identical
        Layout L;
        // Start with a bed that should just about fit, then make it bigger
        // until every plant has a spot.
        L.d = kMaxPlantPx; // plants are always the same size in the world; the camera zooms out instead
        float bedRX = 1e9f;
        float area = capacity * 1.45f * L.d * L.d * gap * gap; // about what a scatter needs, so it rarely has to retry
        // Scatter a few times and keep the most compact layout.
        for (int tries = 0, found = 0; tries < 80 && found < layouts; ++tries) {
            float hh = std::sqrt(area / (4.f * aspect)), hw = hh * aspect;
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
                need = std::max({need, std::fabs(p.x) + edge, (std::fabs(p.y) + edge) * aspect});
            }
            need /= 0.93f; // room for the bed's rim
            if (need < bedRX) {
                bedRX = need;
                L.pts = cand;
            }
        }
        if (L.pts.empty()) L.pts.assign(capacity, SDL_FPoint{0.f, 0.f}); // can't happen in practice
        L.rx = bedRX;
        L.ry = bedRX / aspect;
        // Camera zoom that fits the whole bed and the fence around it.
        const float fence = kFenceGap + kFenceTile;
        L.scale = std::min({1.f, kMaxBedRX / (L.rx + fence), kMaxBedRY / (L.ry + fence)});
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
    fenceTile_ = kFenceTile * L.scale;
    fenceGap_ = kFenceGap * L.scale;
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
    // "0 0 0" is where older versions kept per-crop counts; they're now id=count after the seed.
    out << "farm " << n_ << " " << timeLeft_ << " " << earnedToday_ << " " << pickedToday_ << " 0 0 0 " << seed_;
    for (int c = 0; c < static_cast<int>(pickedByCrop_.size()); ++c)
        if (pickedByCrop_[c] > 0) out << " " << cropDef(c).id << "=" << pickedByCrop_[c];
    out << "\n";
    for (const Tile& t : tiles_) {
        out << "tile " << cropDef(t.crop).id << " " << t.growth << " " << t.pick << " " << t.spot << "\n";
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
    std::vector<int> byCrop(cropCount(), 0);
    bool namedCounts = false;
    for (std::string tok; head >> tok;) { // per-crop counts: carrot=12
        size_t eq = tok.find('=');
        int c = eq == std::string::npos ? -1 : techdata::cropIndex(tok.substr(0, eq));
        if (c >= 0) byCrop[c] = std::atoi(tok.c_str() + eq + 1);
        namedCounts = true;
    }
    if (!namedCounts) { // older saves: counts for lettuce, carrot, pumpkin
        const int old[3] = {p0, p1, p2};
        for (int c = 0; c < 3 && c < cropCount(); ++c) byCrop[c] = old[c];
    }
    // One tile line per growing vegetable. If the number of crops changed
    // since the save (e.g. the tech tree was rebalanced), start a fresh day.
    const int count = static_cast<int>(lines.size()) - 1;
    if (count != plantCount(stats)) return false;

    std::vector<Tile> tiles(count);
    std::vector<int> savedSpots(count, -1);
    for (int i = 0; i < count; ++i) {
        std::istringstream ss(lines[1 + i]);
        std::string cropTok;
        ss >> key >> cropTok >> tiles[i].growth >> tiles[i].pick;
        // The crop by id (older saves used a number). A crop that's gone, or isn't
        // unlocked any more, means the tree changed: start a fresh day instead.
        int crop = !cropTok.empty() && std::isdigit(static_cast<unsigned char>(cropTok[0])) ? std::atoi(cropTok.c_str())
                                                                                              : techdata::cropIndex(cropTok);
        if (!ss || key != "tile" || crop < 0 || crop >= cropCount() || cropDef(crop).tier > stats.cropTier) return false;
        if (!(ss >> savedSpots[i])) savedSpots[i] = -1; // older saves don't record spots
        tiles[i].crop = crop;
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
    zoomT_ = 1.f;
    farmers_.clear(); // farmers aren't saved: they start again from the edge of the bed
    std::mt19937 frng(seed);
    syncFarmers(frng);
    particles_.clear();
    floatTexts_.clear();
    harvests_.clear();
    timeLeft_ = std::clamp(timeLeft, 0.f, stats.dayLength);
    earnedToday_ = earned;
    pickedToday_ = picked;
    pickedByCrop_ = byCrop;
    return true;
}

void Farm::applyStats(const Stats& stats, std::mt19937& rng) {
    bool resized = stats.patchSize != n_ || tiles_.empty();
    // A longer day also lengthens today; a shorter one cuts it down.
    float left = timeLeft_ + std::max(0.f, stats.dayLength - stats_.dayLength);
    if (resized) {
        double earned = earnedToday_;
        int picked = pickedToday_;
        std::vector<int> byCrop = pickedByCrop_;
        startDay(stats, rng); // replants a patch of the new size
        earnedToday_ = earned;
        pickedToday_ = picked;
        pickedByCrop_ = byCrop;
        pickedByCrop_.resize(cropCount(), 0);
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
        for (auto& f : farmers_)
            if (f.target >= static_cast<int>(tiles_.size())) f.target = -1;
        syncFarmers(rng);
        // Crops that are no longer unlocked get replanted.
        for (auto& t : tiles_) {
            if (cropDef(t.crop).tier > stats.cropTier) {
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

std::vector<Farm::Harvest> Farm::takeHarvests() {
    std::vector<Harvest> out;
    out.swap(harvests_);
    return out;
}

Crop Farm::randomCrop(std::mt19937& rng) const {
    // Any unlocked crop, more or less often by its weight.
    const auto& list = techdata::crops();
    float total = 0.f;
    for (const auto& c : list)
        if (c.tier <= stats_.cropTier) total += std::max(0.f, c.weight);
    if (total <= 0.f) return 0;
    std::uniform_real_distribution<float> roll(0.f, total);
    float r = roll(rng);
    int last = 0;
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        if (list[i].tier > stats_.cropTier || list[i].weight <= 0.f) continue;
        last = i;
        r -= list[i].weight;
        if (r < 0.f) return i;
    }
    return last;
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

float Farm::viewZoom() const {
    if (zoomT_ >= 1.f) return 1.f;
    float t = zoomT_ * zoomT_ * (3.f - 2.f * zoomT_); // ease in and out
    return zoomFrom_ + (1.f - zoomFrom_) * t;
}

void Farm::update(float dt, float mouseX, float mouseY, bool mouseInside, double& coins, std::mt19937& rng) {
    zoomT_ = std::min(1.f, zoomT_ + dt / kZoomSeconds);
    // While the camera is still zooming, map the pointer into the bed's own coordinates.
    const float z = viewZoom();
    if (z != 1.f) {
        mouseX = bedX_ + (mouseX - bedX_) / z;
        mouseY = bedY_ + (mouseY - bedY_) / z;
    }
    mouseX_ = mouseX;
    mouseY_ = mouseY;
    mouseInside_ = mouseInside;
    clock_ += dt;
    hovered_ = mouseInside ? plantAt(mouseX, mouseY) : -1;

    if (timeLeft_ > 0.f) {
        if (!timerFrozen_) timeLeft_ = std::max(0.f, timeLeft_ - dt);
        for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
            Tile& t = tiles_[i];
            const techdata::CropDef& info = cropDef(t.crop);
            t.pop = std::max(0.f, t.pop - dt * 3.f);
            if (t.growth < 1.f) {
                t.growth = std::min(1.f, t.growth + dt / (stats_.growTime * info.grow));
                continue;
            }
            if (mouseInside && tileInReach(i, mouseX, mouseY)) {
                t.pick += dt / (stats_.pickTime * info.pick);
                if (t.pick >= 1.f) harvest(i, coins, rng);
            } else {
                // Progress drains away if you move off before finishing.
                t.pick = std::max(0.f, t.pick - dt * 1.5f);
            }
        }
        updateFarmers(dt, coins, rng);
    } else {
        for (auto& f : farmers_) f.walking = f.picking = false; // day over: everyone stops
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

// ---------------------------------------------------------------------------
// Farmers
// ---------------------------------------------------------------------------

void Farm::syncFarmers(std::mt19937& rng) {
    const int want = std::clamp(stats_.farmers, 0, 50);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    while (static_cast<int>(farmers_.size()) > want) farmers_.pop_back();
    while (static_cast<int>(farmers_.size()) < want) {
        // New farmers walk in from the front edge of the bed.
        Farmer f;
        f.x = bedX_ + (unit(rng) * 1.6f - 0.8f) * bedRX_;
        f.y = bedY_ + bedRY_ * 0.82f;
        f.facingLeft = unit(rng) < 0.5f;
        f.wait = unit(rng) * 0.5f;
        farmers_.push_back(f);
    }
}

SDL_FPoint Farm::farmerStand(int plant) const {
    // Just in front of the plant, so the farmer is drawn over it while picking.
    return {tiles_[plant].x, tiles_[plant].y + plantSize_ * 0.3f};
}

void Farm::updateFarmers(float dt, double& coins, std::mt19937& rng) {
    if (farmers_.empty()) return;
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    const float speed = std::max(0.1f, stats_.farmerSpeed) * plantSize_; // pixels per second
    const float minX = bedX_ - bedRX_ * 0.9f, maxX = bedX_ + bedRX_ * 0.9f;
    const float minY = bedY_ - bedRY_ * 0.8f, maxY = bedY_ + bedRY_ * 0.9f;

    // Walks f towards (tx, ty) at `pace`. Returns true once it's there.
    auto walkTo = [&](Farmer& f, float tx, float ty, float pace) {
        float dx = tx - f.x, dy = ty - f.y, dist = std::sqrt(dx * dx + dy * dy);
        float move = pace * dt;
        if (std::fabs(dx) > 0.5f) f.facingLeft = dx < 0.f;
        if (dist <= move || dist < 0.5f) {
            f.x = tx;
            f.y = ty;
            f.walking = false;
            return true;
        }
        f.x += dx / dist * move;
        f.y += dy / dist * move;
        f.step += move / (plantSize_ * 0.45f); // one stride per ~half a plant
        f.walking = true;
        return false;
    };

    for (size_t k = 0; k < farmers_.size(); ++k) {
        Farmer& f = farmers_[k];
        // Drop the target if it was picked (by you or anyone) or isn't ripe any more.
        if (f.target >= 0 && (f.target >= static_cast<int>(tiles_.size()) || tiles_[f.target].spot != f.targetSpot ||
                              tiles_[f.target].growth < 1.f)) {
            f.target = -1;
            f.picking = false;
            f.work = 0.f;
        }
        // Look for the nearest ripe plant no other farmer is going for.
        if (f.target < 0) {
            float best = 1e30f;
            for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
                if (tiles_[i].growth < 1.f) continue;
                bool taken = false;
                for (size_t o = 0; o < farmers_.size() && !taken; ++o) taken = o != k && farmers_[o].target == i;
                if (taken) continue;
                SDL_FPoint p = farmerStand(i);
                float d = (p.x - f.x) * (p.x - f.x) + (p.y - f.y) * (p.y - f.y);
                if (d < best) {
                    best = d;
                    f.target = i;
                }
            }
            if (f.target >= 0) f.targetSpot = tiles_[f.target].spot;
        }

        if (f.target >= 0) {
            SDL_FPoint p = farmerStand(f.target);
            if (!f.picking && walkTo(f, p.x, p.y, speed)) {
                f.picking = true;
                f.work = 0.f;
            }
            if (f.picking) {
                f.walking = false;
                const techdata::CropDef& info = cropDef(tiles_[f.target].crop);
                f.work += dt / (std::max(0.001f, stats_.farmerPickTime) * info.pick);
                if (f.work >= 1.f) {
                    int t = f.target;
                    f.target = -1;
                    f.picking = false;
                    f.work = 0.f;
                    harvest(t, coins, rng, PickedBy::Farmer);
                }
            }
        } else {
            // Nothing ripe: stroll about the bed, pausing now and then.
            f.picking = false;
            if (f.wait > 0.f) {
                f.wait -= dt;
                f.walking = false;
            } else if (walkTo(f, f.wanderX, f.wanderY, speed * 0.4f)) {
                f.wanderX = minX + unit(rng) * (maxX - minX);
                f.wanderY = minY + unit(rng) * (maxY - minY);
                f.wait = 0.6f + unit(rng) * 1.4f;
            }
        }
        f.x = std::clamp(f.x, bedX_ - bedRX_, bedX_ + bedRX_);
        f.y = std::clamp(f.y, bedY_ - bedRY_, bedY_ + bedRY_);
    }
}

SDL_FRect Farm::farmerRect(const Farmer& f) const {
    float size = plantSize_ * 1.35f;
    float bob = f.walking ? std::fabs(std::sin(f.step * kPi)) * size * 0.04f : 0.f;
    return {f.x - size * 0.5f, f.y - size * 0.92f - bob, size, size}; // feet a little above the bottom edge
}

void Farm::drawFarmer(SDL_Renderer* r, const Farmer& f) const {
    // Pose: picking, a walking step, or standing. Images fall back to farm/farmer,
    // then to the built-in drawing.
    int pose = f.picking ? 3 : (f.walking ? (std::fmod(f.step, 2.f) < 1.f ? 1 : 2) : 0);
    static const char* kNames[4] = {"farm/farmer", "farm/farmer_walk1", "farm/farmer_walk2", "farm/farmer_pick"};
    SDL_FRect rc = farmerRect(f);
    if (art::has(kNames[pose])) art::drawFlipped(r, kNames[pose], rc, f.facingLeft);
    else if (art::has("farm/farmer")) art::drawFlipped(r, "farm/farmer", rc, f.facingLeft);
    else drawFarmerBuiltin(r, rc, pose);
}

float Farm::autoPickRange() const {
    if (stats_.autoPickChance <= 0.f || stats_.autoPickCount <= 0 || stats_.autoPickRadius <= 0.f) return 0.f;
    return stats_.autoPickRadius * plantSize_;
}

void Farm::harvest(int index, double& coins, std::mt19937& rng, PickedBy by) {
    Tile& t = tiles_[index];
    const techdata::CropDef& info = cropDef(t.crop);
    double gain = info.value * stats_.valueMult;
    coins += gain;
    earnedToday_ += gain;
    ++pickedToday_;
    if (static_cast<int>(pickedByCrop_.size()) < cropCount()) pickedByCrop_.resize(cropCount(), 0);
    ++pickedByCrop_[t.crop];
    harvests_.push_back({t.crop, by, gain});

    float cx = t.x, cy = t.y;

    SDL_Color burst = cropColor(t.crop);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (int k = 0; k < 10; ++k) {
        float a = unit(rng) * 6.2832f;
        float speed = 80.f + unit(rng) * 160.f;
        particles_.push_back({cx, cy, std::cos(a) * speed, std::sin(a) * speed - 140.f,
                              0.4f + unit(rng) * 0.4f, 2.f + unit(rng) * 3.f,
                              k % 3 == 0 ? SDL_Color{255, 225, 90, 255} : burst});
    }
    floatTexts_.push_back({cx, cy - plantSize_ * 0.3f, 1.0f, "+" + draw::number(gain),
                           by == PickedBy::AutoPick ? pal::PaleShoot
                           : by == PickedBy::Farmer ? pal::Mist
                                                    : pal::PaleGold});

    // Auto-pick: a chance to also pick the nearest ripe crops within range.
    std::vector<int> extra;
    const float range = autoPickRange();
    if (by == PickedBy::Player && range > 0.f && unit(rng) * 100.f < stats_.autoPickChance) {
        std::vector<std::pair<float, int>> near;
        for (int j = 0; j < static_cast<int>(tiles_.size()); ++j) {
            if (j == index || tiles_[j].growth < 1.f) continue;
            float dx = tiles_[j].x - cx, dy = tiles_[j].y - cy, d2 = dx * dx + dy * dy;
            if (d2 <= range * range) near.push_back({d2, j});
        }
        std::sort(near.begin(), near.end());
        for (int k = 0; k < static_cast<int>(near.size()) && k < stats_.autoPickCount; ++k) extra.push_back(near[k].second);
        if (!extra.empty()) floatTexts_.push_back({cx, cy - plantSize_ * 0.75f, 1.1f, "Auto-pick!", pal::PaleShoot});
    }

    // Replant straight away, in a new random spot.
    t.crop = randomCrop(rng);
    t.growth = 0.f;
    t.pick = 0.f;
    t.pop = 1.f;
    moveToFreeSpot(index, rng);

    for (int j : extra) {
        // A trail of sparkles from the crop you picked to each one it picked for you.
        float tx = tiles_[j].x, ty = tiles_[j].y;
        for (int k = 1; k <= 8; ++k) {
            float f = k / 9.f;
            particles_.push_back({cx + (tx - cx) * f, cy + (ty - cy) * f, (unit(rng) - 0.5f) * 30.f, -40.f - unit(rng) * 40.f,
                                  0.35f + 0.04f * k, 2.5f, SDL_Color{170, 255, 150, 255}});
        }
        harvest(j, coins, rng, PickedBy::AutoPick);
    }
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
    std::string name = cropArtName(crop);
    if (art::has(name)) art::draw(r, name, SDL_FRect{cx - s * 0.5f, cy - s * 0.5f, s, s});
    else drawCropBuiltin(r, crop, cx, cy, s); // (also covers crops added after the art list was made)
}

// ---------------------------------------------------------------------------
// Built-in art (used when there's no image in assets/, and for the templates)
// ---------------------------------------------------------------------------

void Farm::drawSproutBuiltin(SDL_Renderer* r, const SDL_FRect& rc) { croplook::drawSprout(r, rc); }

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

// Built-in fence tiles: weathered wooden posts and two rails, seen from above
// like the bed. Each tile joins up with its neighbours at the edges.
namespace {
const SDL_Color kFenceWood{164, 128, 86, 255}, kFenceDark{112, 84, 54, 255}, kFenceLight{196, 162, 116, 255};
void fencePost(SDL_Renderer* r, float cx, float cy, float s) {
    draw::fillRoundRect(r, cx - s * 0.5f + s * 0.08f, cy - s * 0.5f + s * 0.12f, s, s, s * 0.2f, SDL_Color{0, 0, 0, 50}); // shadow
    draw::fillRoundRect(r, cx - s * 0.5f, cy - s * 0.5f, s, s, s * 0.2f, kFenceDark);
    draw::fillRoundRect(r, cx - s * 0.36f, cy - s * 0.36f, s * 0.72f, s * 0.72f, s * 0.14f, kFenceWood);
    draw::fillRoundRect(r, cx - s * 0.2f, cy - s * 0.26f, s * 0.36f, s * 0.14f, s * 0.07f, kFenceLight);
}
// Two rails across the tile: horizontal (along the top and bottom) or vertical (down the sides).
void fenceRails(SDL_Renderer* r, const SDL_FRect& rc, bool horizontal) {
    const float t = std::max(2.f, std::min(rc.w, rc.h) * 0.14f);
    for (float f : {0.36f, 0.64f}) {
        if (horizontal) {
            float y = rc.y + rc.h * f - t * 0.5f;
            draw::fillRect(r, rc.x, y + t * 0.4f, rc.w, t, SDL_Color{0, 0, 0, 45});
            draw::fillRect(r, rc.x, y, rc.w, t, kFenceWood);
            draw::fillRect(r, rc.x, y, rc.w, std::max(1.f, t * 0.3f), kFenceLight);
        } else {
            float x = rc.x + rc.w * f - t * 0.5f;
            draw::fillRect(r, x + t * 0.4f, rc.y, t, rc.h, SDL_Color{0, 0, 0, 45});
            draw::fillRect(r, x, rc.y, t, rc.h, kFenceWood);
            draw::fillRect(r, x, rc.y, std::max(1.f, t * 0.3f), rc.h, kFenceLight);
        }
    }
}
} // namespace

void Farm::drawFenceBuiltin(SDL_Renderer* r, const SDL_FRect& rc, FencePiece piece) {
    const float s = std::min(rc.w, rc.h);
    const float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
    switch (piece) {
    case FencePiece::Horizontal:
        fenceRails(r, rc, true);
        fencePost(r, cx, cy, s * 0.42f);
        break;
    case FencePiece::Vertical:
        fenceRails(r, rc, false);
        fencePost(r, cx, cy, s * 0.42f);
        break;
    case FencePiece::Corner:
        // One picture for all four corners, so just a big post (the rails come from the neighbours).
        fencePost(r, cx, cy, s * 0.7f);
        break;
    case FencePiece::Gate: {
        // Hinge posts at each side, a lighter gate between them with a diagonal brace.
        const float t = std::max(2.f, s * 0.16f);
        SDL_FRect g{rc.x + rc.w * 0.16f, rc.y + rc.h * 0.24f, rc.w * 0.68f, rc.h * 0.52f};
        draw::fillRect(r, g.x, g.y + t * 0.5f, g.w, g.h, SDL_Color{0, 0, 0, 45});
        draw::fillRect(r, g.x, g.y, g.w, t, kFenceLight);                 // top rail
        draw::fillRect(r, g.x, g.y + g.h - t, g.w, t, kFenceLight);       // bottom rail
        draw::fillRect(r, g.x + g.w - t, g.y, t, g.h, kFenceLight);       // latch side
        draw::thickLine(r, g.x + t * 0.5f, g.y + g.h - t * 0.5f, g.x + g.w - t, g.y + t, t * 0.9f, kFenceWood); // brace
        draw::fillRect(r, g.x + g.w - t * 1.6f, cy - t * 0.3f, t * 0.6f, t * 0.6f, kFenceDark); // latch
        fencePost(r, rc.x + rc.w * 0.1f, cy, s * 0.42f);
        fencePost(r, rc.x + rc.w * 0.9f, cy, s * 0.42f);
        break;
    }
    }
}

void Farm::drawCropBuiltin(SDL_Renderer* r, Crop crop, float cx, float cy, float s) {
    croplook::draw(r, cropDef(crop).look, cropColor(crop), cx, cy, s);
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
    const float z = viewZoom();
    if (z == 1.f) return renderScene(r);
    // Camera mid-zoom: draw the bed to an off-screen image and show it scaled
    // around the middle of the bed.
    if (!zoomTex_ || zoomTexRenderer_ != r) {
        zoomTex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1280, 720);
        zoomTexRenderer_ = r;
        if (zoomTex_) SDL_SetTextureBlendMode(zoomTex_, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    }
    if (!zoomTex_) return renderScene(r);
    SDL_Texture* previous = SDL_GetRenderTarget(r);
    SDL_SetRenderTarget(r, zoomTex_);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);
    renderScene(r);
    SDL_SetRenderTarget(r, previous);
    SDL_FRect dst{bedX_ - bedX_ * z, bedY_ - bedY_ * z, 1280.f * z, 720.f * z};
    SDL_RenderTexture(r, zoomTex_, nullptr, &dst);
}

// The fence around the bed: a ring of tiles. Corners are posts, the long sides
// are fence runs, and the middle of the bottom side is a gate.
Farm::FenceRing Farm::fenceRing() const {
    FenceRing f;
    const float m = fenceGap_ + fenceTile_;
    f.outer = SDL_FRect{bedX_ - bedRX_ - m, bedY_ - bedRY_ - m, (bedRX_ + m) * 2.f, (bedRY_ + m) * 2.f};
    // Whole tiles along each side, stretched a touch so they meet exactly at the corners.
    f.nx = std::max(3, static_cast<int>(std::lround(f.outer.w / fenceTile_)));
    f.ny = std::max(3, static_cast<int>(std::lround(f.outer.h / fenceTile_)));
    f.tw = f.outer.w / f.nx;
    f.th = f.outer.h / f.ny;
    return f;
}

namespace {
// Part of a fence image, `src` given as fractions of the image (0..1), drawn into the matching part of `tile`.
void drawPart(SDL_Renderer* r, const char* name, const SDL_FRect& tile, float x0, float y0, float x1, float y1) {
    SDL_Texture* tex = art::texture(name);
    if (!tex) return;
    float w = 0, h = 0;
    SDL_GetTextureSize(tex, &w, &h);
    SDL_FRect src{std::round(w * x0), std::round(h * y0), std::round(w * x1) - std::round(w * x0),
                  std::round(h * y1) - std::round(h * y0)};
    SDL_FRect dst{tile.x + tile.w * src.x / w, tile.y + tile.h * src.y / h, tile.w * src.w / w, tile.h * src.h / h};
    SDL_RenderTexture(r, tex, &src, &dst);
}
} // namespace

// The fence is drawn in two halves so plants can stand in front of the back
// fence and behind the front one: back = top run and both sides, front = bottom run.
void Farm::drawFence(SDL_Renderer* r, bool front) const {
    if (fenceTile_ <= 0.f) return;
    const FenceRing f = fenceRing();
    const int gate = f.nx / 2;
    auto tile = [&](int i, int j) { return SDL_FRect{f.outer.x + i * f.tw, f.outer.y + j * f.th, f.tw, f.th}; };
    // A corner post, with the rails that run into it from its neighbours tucked in behind, so the
    // fence meets the post instead of stopping short of it.
    auto corner = [&](int i, int j) {
        const SDL_FRect t = tile(i, j);
        if (i == 0) drawPart(r, "farm/fence_h", t, 0.5f, 0.f, 1.f, 1.f);   // rails going right
        else drawPart(r, "farm/fence_h", t, 0.f, 0.f, 0.5f, 1.f);          // rails going left
        if (j > 0) drawPart(r, "farm/fence_v", t, 0.f, 0.f, 1.f, 0.25f);   // rail coming down from above
        art::draw(r, "farm/fence_corner", t);
    };
    const int j = front ? f.ny - 1 : 0;
    if (!front) {
        for (int k = 1; k < f.ny - 1; ++k) {
            art::draw(r, "farm/fence_v", tile(0, k));
            art::draw(r, "farm/fence_v", tile(f.nx - 1, k));
        }
    }
    for (int i = 1; i < f.nx - 1; ++i)
        art::draw(r, front && i == gate ? "farm/fence_gate" : "farm/fence_h", tile(i, j));
    corner(0, j);
    corner(f.nx - 1, j);
}

void Farm::renderScene(SDL_Renderer* r) const {
    // The bed fills the whole fenced area: from the middle of the side fences, and from the foot of
    // the back fence to under the front one, so there's no grass between the soil and the fence.
    const SDL_FRect bed{bedX_ - bedRX_, bedY_ - bedRY_, bedRX_ * 2.f, bedRY_ * 2.f}; // where plants go
    SDL_FRect soil = bed;
    if (fenceTile_ > 0.f) {
        const FenceRing f = fenceRing();
        soil = SDL_FRect{f.outer.x + f.tw * 0.5f, f.outer.y + f.th * 0.75f, f.outer.w - f.tw, f.outer.h - f.th * 1.25f};
    }
    if (art::has("farm/bed")) {
        art::draw(r, "farm/bed", soil);
    } else {
        draw::fillRoundRect(r, soil.x + 4.f, soil.y + 8.f, soil.w, soil.h, 8.f, SDL_Color{0, 0, 0, 55}); // shadow
        drawBedBuiltin(r, soil, seed_);
    }
    drawFence(r, false);

    // Plants, back to front so nearer ones overlap the ones behind.
    bool active = mouseInside_ && timeLeft_ > 0.f;
    // Farmers are drawn in between, by how far down the screen their feet
    // are, so they walk behind plants further down and in front of the rest.
    std::vector<int> farmerOrder(farmers_.size());
    for (size_t k = 0; k < farmers_.size(); ++k) farmerOrder[k] = static_cast<int>(k);
    std::sort(farmerOrder.begin(), farmerOrder.end(), [this](int a, int b) { return farmers_[a].y < farmers_[b].y; });
    size_t nextFarmer = 0;
    for (int i : drawOrder_) {
        const Tile& t = tiles_[i];
        while (nextFarmer < farmerOrder.size() && farmers_[farmerOrder[nextFarmer]].y < t.y + plantSize_ * 0.25f)
            drawFarmer(r, farmers_[farmerOrder[nextFarmer++]]);
        art::drawVariant(r, "farm/soil", active && i == hovered_ ? "_hover" : "", plantRect(i));
        drawVegetable(r, t, t.x, t.y, plantSize_);
    }
    while (nextFarmer < farmerOrder.size()) drawFarmer(r, farmers_[farmerOrder[nextFarmer++]]);
    drawFence(r, true);

    // Picking bars on top of everything else in the bed.
    std::vector<float> farmerWork(tiles_.size(), 0.f);
    for (const auto& f : farmers_)
        if (f.picking && f.target >= 0 && f.target < static_cast<int>(tiles_.size())) farmerWork[f.target] = f.work;
    for (int i : drawOrder_) {
        const Tile& t = tiles_[i];
        const float progress = std::max(t.pick, farmerWork[i]);
        if (t.growth < 1.f || progress <= 0.01f) continue;
        float bw = plantSize_ * 0.7f, bh = std::max(4.f, plantSize_ * 0.08f);
        float bx = t.x - bw * 0.5f, by = t.y + plantSize_ * 0.36f;
        art::draw(r, "ui/pick_bar_back", SDL_FRect{bx - 1, by - 1, bw + 2, bh + 2});
        float f = std::min(1.f, progress);
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
            std::string name = cropDef(t.crop).name;
            if (draw::textWidth(name, 1.f) > plantSize_ - 6) name = name.substr(0, 1); // small plants: just the initial
            draw::text(r, t.x, t.y + 10, name, 1.f, SDL_Color{255, 255, 255, 200}, draw::Align::Center);
        }
        draw::outlineRect(r, bed.x, bed.y, bed.w, bed.h, SDL_Color{255, 255, 0, 120});
        float rad = pickRadius();
        if (rad > 0.f && mouseInside_) draw::circleOutline(r, mouseX_, mouseY_, rad, SDL_Color{0, 255, 255, 200});
        // Auto-pick range around the plant under the pointer.
        float ap = autoPickRange();
        if (ap > 0.f && hovered_ >= 0)
            draw::circleOutline(r, tiles_[hovered_].x, tiles_[hovered_].y, ap, SDL_Color{150, 255, 140, 200});
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

// A farmer in a straw hat, red shirt and blue overalls, drawn in a 128x128
// box scaled to rc. pose: 0 standing, 1 / 2 walking steps, 3 bending to pick.
void Farm::drawFarmerBuiltin(SDL_Renderer* r, const SDL_FRect& rc, int pose) {
    const float u = rc.w / 128.f;
    auto X = [&](float v) { return rc.x + v * u; };
    auto Y = [&](float v) { return rc.y + v * (rc.h / 128.f); };
    const SDL_Color skin{240, 198, 156, 255}, shirt{200, 70, 58, 255}, shirtDark{165, 52, 44, 255};
    const SDL_Color denim{62, 102, 170, 255}, denimDark{46, 78, 135, 255}, boot{92, 60, 36, 255};
    const SDL_Color straw{236, 200, 112, 255}, strawDark{205, 165, 80, 255}, band{180, 60, 50, 255};
    const float c = pose == 3 ? 10.f : 0.f;                        // crouch
    const float lift1 = pose == 1 ? 5.f : 0.f, lift2 = pose == 2 ? 5.f : 0.f; // which foot is up
    const float swing = pose == 1 ? 5.f : (pose == 2 ? -5.f : 0.f); // arm swing

    draw::fillEllipse(r, X(64), Y(118), 28 * u, 6 * u, SDL_Color{0, 0, 0, 70});
    // Legs and boots.
    draw::fillRect(r, X(49), Y(96 + c * 0.5f - lift1), 12 * u, (18 - c * 0.5f) * u, denimDark);
    draw::fillRect(r, X(67), Y(96 + c * 0.5f - lift2), 12 * u, (18 - c * 0.5f) * u, denimDark);
    draw::fillRoundRect(r, X(46), Y(110 - lift1), 17 * u, 9 * u, 3 * u, boot);
    draw::fillRoundRect(r, X(65), Y(110 - lift2), 17 * u, 9 * u, 3 * u, boot);
    // Shirt, arms and hands.
    draw::fillRoundRect(r, X(40), Y(58 + c), 48 * u, 30 * u, 10 * u, shirt);
    if (pose == 3) {
        // Reaching down in front.
        draw::fillRoundRect(r, X(40), Y(70 + c), 12 * u, 26 * u, 5 * u, shirtDark);
        draw::fillRoundRect(r, X(76), Y(70 + c), 12 * u, 26 * u, 5 * u, shirtDark);
        draw::fillCircle(r, X(48), Y(100 + c), 6 * u, skin);
        draw::fillCircle(r, X(80), Y(100 + c), 6 * u, skin);
    } else {
        draw::fillRoundRect(r, X(31), Y(62 + swing), 11 * u, 26 * u, 5 * u, shirtDark);
        draw::fillRoundRect(r, X(86), Y(62 - swing), 11 * u, 26 * u, 5 * u, shirtDark);
        draw::fillCircle(r, X(36.5f), Y(91 + swing), 6 * u, skin);
        draw::fillCircle(r, X(91.5f), Y(91 - swing), 6 * u, skin);
    }
    // Overalls: trousers, bib and straps.
    draw::fillRoundRect(r, X(44), Y(80 + c), 40 * u, (20 - c * 0.5f) * u, 6 * u, denim);
    draw::fillRoundRect(r, X(50), Y(66 + c), 28 * u, 20 * u, 4 * u, denim);
    draw::fillRect(r, X(50), Y(58 + c), 5 * u, 10 * u, denim);
    draw::fillRect(r, X(73), Y(58 + c), 5 * u, 10 * u, denim);
    draw::fillCircle(r, X(55), Y(70 + c), 2 * u, straw);
    draw::fillCircle(r, X(73), Y(70 + c), 2 * u, straw);
    // Head.
    draw::fillCircle(r, X(64), Y(45 + c), 15 * u, skin);
    if (pose == 3) {
        draw::fillRect(r, X(57), Y(50 + c), 4 * u, 2 * u, SDL_Color{60, 40, 30, 255}); // looking down
        draw::fillRect(r, X(67), Y(50 + c), 4 * u, 2 * u, SDL_Color{60, 40, 30, 255});
    } else {
        draw::fillCircle(r, X(58), Y(46 + c), 2 * u, SDL_Color{50, 35, 25, 255});
        draw::fillCircle(r, X(70), Y(46 + c), 2 * u, SDL_Color{50, 35, 25, 255});
        draw::fillRect(r, X(60), Y(53 + c), 8 * u, 2 * u, SDL_Color{170, 90, 70, 255});
    }
    // Straw hat.
    draw::fillEllipse(r, X(64), Y(35 + c), 30 * u, 7 * u, strawDark);
    draw::fillEllipse(r, X(64), Y(33 + c), 29 * u, 6 * u, straw);
    draw::fillRoundRect(r, X(49), Y(17 + c), 30 * u, 17 * u, 7 * u, straw);
    draw::fillRect(r, X(49), Y(28 + c), 30 * u, 4 * u, band);
}
