// TechTree.h - the rules of the tech tree (costs, requirements, buying).
//
// The techs themselves are data in include/TechTreeData.h, which you edit with
// the TechTreeEditor tool (`make editor`, see README). It's compiled into the
// game, so there are no files to ship. Each tech's effects change the Stats
// below; to make a brand-new KIND of effect:
//   1. Add a field to Stats (with its starting value).
//   2. Add it to the list in TechData.cpp (stats(), getStat, setStat, formatStat)
//      so the editor and the game know about it.
//   3. Use the new Stats field wherever the game needs it (usually Farm.cpp).
#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Every number the upgrades can change. Defaults are the values at the
// very start of the game, before anything has been bought.
struct Stats {
    int patchSize = 2;        // the patch has room for patchSize x patchSize plants
    int maxCrops = 4;         // vegetables growing at any one time (never more than the patch has room for)
    float dayLength = 12.f;   // seconds of picking per day
    float pickTime = 1.0f;    // seconds of hovering needed to pick a vegetable
    float growTime = 3.0f;    // seconds for a lettuce to grow (other crops are multiples)
    float valueMult = 1.0f;   // multiplier on coins from every vegetable
    int reach = 0;            // 0 = pick only the tile under the mouse; higher = bigger picking circle
    // Which crops get planted. Each crop has its own unlock number (its "tier" in the tree file);
    // a tech with "effect cropTier atleast N" unlocks just the crops numbered N, not the ones
    // below it. Bit N set = unlocked; bit 0 (crops from the start) is always set.
    std::uint64_t cropsUnlocked = 1;
    bool cropUnlocked(int tier) const { return tier <= 0 || (tier < 64 && ((cropsUnlocked >> tier) & 1u)); }
    float headStart = 0.f;    // fraction of the patch that is already ripe when a day starts
    // Auto-pick: picking a crop has a chance to also pick ripe crops near it.
    float autoPickChance = 0.f; // percent chance (0-100) each time you pick a crop
    int autoPickCount = 0;      // how many nearby ripe crops it picks
    float autoPickRadius = 0.f; // how far counts as "nearby", in plant widths (centre to centre)
    // Farmers: helpers that walk around the patch picking ripe crops.
    int farmers = 0;            // how many are working the patch
    float farmerSpeed = 2.0f;   // walking speed, in plant widths per second
    float farmerPickTime = 1.5f; // seconds for a farmer to pick a lettuce (other crops take longer, as for you)
};

struct Prereq {
    std::string id; // id of the node that must be bought first
    int level;      // level that node must reach
};

struct TechNode {
    std::string id;
    std::string name;
    std::string description;
    std::string icon; // art name under tree/icons/ (the tech's id unless the tree picks another)
    std::string shape;       // tile shape ("" = square)
    std::string color;       // tile colour once unlocked (hex, "" = the usual)
    std::string lockedColor; // tile colour while locked (hex, "" = the usual)
    int maxLevel = 1;
    double baseCost = 10;     // cost of level 1
    double costGrowth = 1.5;  // each level costs this many times more than the last
    std::vector<Prereq> prereqs;
    float gridX = 0, gridY = 0; // position on the tech tree screen (in node slots)

    // Applies this node's effect at `level` (always >= 1) to the stats.
    std::function<void(Stats&, int level)> apply;
    // Short text describing the effect at a given level, e.g. "4x4 patch".
    std::function<std::string(int level)> describe;
    // The stats this tech changes, read from a full set of Stats (so other
    // techs that change the same stat are included), e.g. "2 farmers".
    std::function<std::string(const Stats&)> describeStats;

    int level = 0; // how many levels the player owns
};

class TechTree {
public:
    TechTree();

    std::vector<TechNode>& nodes() { return nodes_; }
    const std::vector<TechNode>& nodes() const { return nodes_; }

    TechNode* find(const std::string& id);
    const TechNode* find(const std::string& id) const;

    bool prereqsMet(const TechNode& n) const;
    bool isMaxed(const TechNode& n) const { return n.level >= n.maxLevel; }
    double cost(const TechNode& n) const;
    bool canBuy(const TechNode& n, double coins) const;
    // Demo builds only: true for techs outside the demo's radius around The Barn
    // (they show, but can't be bought). Always false in the full game.
    bool demoLocked(const TechNode& n) const;
    // How far a tech is from The Barn, in tech tree rows (see Platform.h).
    float distanceFromRoot(const TechNode& n) const;
    // Demo builds: every tech the demo allows has been bought.
    bool demoComplete() const;
    // Every tech bought to its max level.
    bool allBought() const;

    // Buys one level of node `index` if possible. Returns true on success.
    bool tryBuy(size_t index, double& coins);

    // Builds the current Stats from the defaults plus every owned upgrade.
    Stats computeStats() const;
    // The same, as if `node` were at `level` (for "next level" previews).
    Stats computeStatsWith(const TechNode& node, int level) const;

    void resetLevels();

private:
    void addNode(TechNode node);

    std::vector<TechNode> nodes_;
    std::unordered_map<std::string, size_t> index_;
};
