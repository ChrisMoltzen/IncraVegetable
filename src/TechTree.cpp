#include "TechTree.h"

#include "TechData.h"
#include "TechTreeData.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

TechTree::TechTree() {
    // The tree itself is data: include/TechTreeData.h, made with the
    // TechTreeEditor tool and compiled into the game.
    techdata::ParseResult parsed = techdata::parse(techdata::joinChunks(kTechTreeData));
    for (const auto& e : parsed.errors) SDL_Log("Tech tree: %s", e.c_str());
    if (!parsed.crops.empty()) techdata::setCrops(parsed.crops); // (none in the file: lettuce, carrot, pumpkin)
    for (const auto& p : techdata::validate(parsed.techs)) SDL_Log("Tech tree problem: %s", p.c_str());
    for (const auto& p : techdata::validateCrops(techdata::crops(), parsed.techs)) SDL_Log("Tech tree problem: %s", p.c_str());

    for (const techdata::TechDef& def : parsed.techs) {
        if (find(def.id)) continue; // duplicate id - keep the first one
        TechNode n;
        n.id = def.id;
        n.name = def.name;
        n.description = def.description;
        n.icon = techdata::iconOf(def);
        n.shape = def.shape;
        n.color = def.color;
        n.lockedColor = def.lockedColor;
        n.maxLevel = std::max(1, def.maxLevel);
        n.baseCost = def.baseCost;
        n.costGrowth = def.costGrowth;
        for (const auto& q : def.needs) n.prereqs.push_back({q.id, q.level});
        n.gridX = def.gridX;
        n.gridY = def.gridY;
        n.apply = [def](Stats& s, int lv) { techdata::applyEffects(def, s, lv); };
        n.describe = [def](int lv) { return techdata::describe(def, lv); };
        std::vector<int> touched;
        for (const auto& e : def.effects) {
            int idx = techdata::statIndex(e.stat);
            if (idx >= 0 && std::find(touched.begin(), touched.end(), idx) == touched.end()) touched.push_back(idx);
        }
        // Techs with no effects (they only unlock others, like The Barn) leave this empty.
        if (!touched.empty()) n.describeStats = [touched](const Stats& s) {
            std::string out;
            for (int idx : touched) out += (out.empty() ? "" : ", ") + techdata::formatStat(idx, s);
            return out.empty() ? std::string("no effect") : out;
        };
        addNode(std::move(n));
    }
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

Stats TechTree::computeStats() const { return computeStatsWith(TechNode{}, 0); }

Stats TechTree::computeStatsWith(const TechNode& node, int level) const {
    Stats s;
    for (const auto& n : nodes_) {
        int lv = &n == &node ? level : n.level;
        if (lv > 0 && n.apply) n.apply(s, lv);
    }
    s.headStart = std::clamp(s.headStart, 0.f, 1.f);
    return s;
}

void TechTree::resetLevels() {
    for (auto& n : nodes_) n.level = 0;
}
