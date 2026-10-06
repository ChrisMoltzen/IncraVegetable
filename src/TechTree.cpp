#include "TechTree.h"

#include "Draw.h"

#include <algorithm>
#include <cmath>

TechTree::TechTree() {
    // ------------------------------------------------------------------
    // The tech tree. Add new upgrades here - one addNode per upgrade.
    // gridX / gridY place the node on the tech tree screen (0,0 is the
    // middle-top). Fields must stay in the same order as in TechNode.
    // ------------------------------------------------------------------

    // ---- Roots: always visible ----
    addNode({
        .id = "patch",
        .name = "Bigger Patch",
        .description = "Dig out a bigger patch with room for more vegetables.",
        .maxLevel = 7,
        .baseCost = 10,
        .costGrowth = 2.1,
        .prereqs = {},
        .gridX = 0, .gridY = 0,
        .apply = [](Stats& s, int lv) { s.patchSize += lv; },
        .describe = [](int lv) { int n = 3 + lv; return draw::strf("%d plants", n * n); },
    });

    addNode({
        .id = "daylength",
        .name = "Longer Days",
        .description = "Wake up earlier. Each level adds 5 seconds to every day.",
        .maxLevel = 10,
        .baseCost = 8,
        .costGrowth = 1.65,
        .prereqs = {},
        .gridX = -1, .gridY = 0,
        .apply = [](Stats& s, int lv) { s.dayLength += 5.f * lv; },
        .describe = [](int lv) { return draw::strf("%d second days", 20 + 5 * lv); },
    });

    addNode({
        .id = "pickspeed",
        .name = "Quick Hands",
        .description = "Practice makes perfect. Pick vegetables 18% faster per level.",
        .maxLevel = 10,
        .baseCost = 8,
        .costGrowth = 1.7,
        .prereqs = {},
        .gridX = 1, .gridY = 0,
        .apply = [](Stats& s, int lv) { s.pickTime *= std::pow(0.82f, static_cast<float>(lv)); },
        .describe = [](int lv) { return draw::strf("%.2fs to pick", std::pow(0.82, lv)); },
    });

    // ---- Second row ----
    addNode({
        .id = "headstart",
        .name = "Head Start",
        .description = "Crops keep growing overnight. Part of the patch is already ripe when the day begins.",
        .maxLevel = 5,
        .baseCost = 60,
        .costGrowth = 2.0,
        .prereqs = {{"daylength", 2}},
        .gridX = -1, .gridY = 1,
        .apply = [](Stats& s, int lv) { s.headStart += 0.2f * lv; },
        .describe = [](int lv) { return draw::strf("%d%% ripe at dawn", 20 * lv); },
    });

    addNode({
        .id = "growspeed",
        .name = "Fertile Soil",
        .description = "Richer compost. Vegetables grow 12% faster per level.",
        .maxLevel = 8,
        .baseCost = 25,
        .costGrowth = 1.8,
        .prereqs = {{"patch", 1}},
        .gridX = 0, .gridY = 1,
        .apply = [](Stats& s, int lv) { s.growTime *= std::pow(0.88f, static_cast<float>(lv)); },
        .describe = [](int lv) { return draw::strf("%.0f%% grow time", 100.0 * std::pow(0.88, lv)); },
    });

    addNode({
        .id = "value",
        .name = "Prize Produce",
        .description = "Bigger, shinier vegetables. Every vegetable sells for 25% more per level.",
        .maxLevel = 10,
        .baseCost = 30,
        .costGrowth = 1.75,
        .prereqs = {{"pickspeed", 1}},
        .gridX = 1, .gridY = 1,
        .apply = [](Stats& s, int lv) { s.valueMult *= 1.f + 0.25f * lv; },
        .describe = [](int lv) { return draw::strf("x%.2f coins", 1.0 + 0.25 * lv); },
    });

    addNode({
        .id = "reach",
        .name = "Wide Reach",
        .description = "Long arms. Pick every ripe vegetable inside a circle around the mouse.",
        .maxLevel = 3,
        .baseCost = 150,
        .costGrowth = 3.5,
        .prereqs = {{"pickspeed", 3}},
        .gridX = 2, .gridY = 1,
        .apply = [](Stats& s, int lv) { s.reach += lv; },
        .describe = [](int lv) { return lv == 0 ? std::string("1 plant") : draw::strf("reach radius %d", lv); },
    });

    // ---- New crops ----
    addNode({
        .id = "carrots",
        .name = "Carrots",
        .description = "Unlocks carrots. They grow slower but sell for 4 coins each.",
        .maxLevel = 1,
        .baseCost = 180,
        .costGrowth = 1.0,
        .prereqs = {{"growspeed", 2}},
        .gridX = 0, .gridY = 2,
        .apply = [](Stats& s, int) { s.cropTier = std::max(s.cropTier, 1); },
        .describe = [](int lv) { return lv ? std::string("carrots planted") : std::string("no carrots"); },
    });

    addNode({
        .id = "pumpkins",
        .name = "Pumpkins",
        .description = "Unlocks pumpkins. Slow to grow and hard to pick, but worth 15 coins each.",
        .maxLevel = 1,
        .baseCost = 1500,
        .costGrowth = 1.0,
        .prereqs = {{"carrots", 1}, {"value", 3}},
        .gridX = 0.5f, .gridY = 3,
        .apply = [](Stats& s, int) { s.cropTier = std::max(s.cropTier, 2); },
        .describe = [](int lv) { return lv ? std::string("pumpkins planted") : std::string("no pumpkins"); },
    });
}

void TechTree::addNode(TechNode node) {
    index_[node.id] = nodes_.size();
    nodes_.push_back(std::move(node));
}

TechNode* TechTree::find(const std::string& id) {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &nodes_[it->second];
}

const TechNode* TechTree::find(const std::string& id) const {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &nodes_[it->second];
}

bool TechTree::prereqsMet(const TechNode& n) const {
    for (const auto& p : n.prereqs) {
        const TechNode* other = find(p.id);
        if (!other || other->level < p.level) return false;
    }
    return true;
}

double TechTree::cost(const TechNode& n) const {
    return std::ceil(n.baseCost * std::pow(n.costGrowth, n.level));
}

bool TechTree::canBuy(const TechNode& n, double coins) const {
    return !isMaxed(n) && prereqsMet(n) && coins >= cost(n);
}

bool TechTree::tryBuy(size_t index, double& coins) {
    if (index >= nodes_.size()) return false;
    TechNode& n = nodes_[index];
    if (!canBuy(n, coins)) return false;
    coins -= cost(n);
    ++n.level;
    return true;
}

Stats TechTree::computeStats() const {
    Stats s;
    for (const auto& n : nodes_) {
        if (n.level > 0 && n.apply) n.apply(s, n.level);
    }
    s.headStart = std::clamp(s.headStart, 0.f, 1.f);
    return s;
}

void TechTree::resetLevels() {
    for (auto& n : nodes_) n.level = 0;
}
