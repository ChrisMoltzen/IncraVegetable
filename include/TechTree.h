// TechTree.h - the data and rules of the tech tree.
//
// Every upgrade is one TechNode. A node changes the game by modifying a
// Stats struct in its `apply` function. To add a new upgrade:
//   1. If it needs a new number, add a field to Stats (with its default value).
//   2. Add one addNode(...) entry in TechTree::TechTree() in TechTree.cpp.
//   3. Use the new Stats field wherever the game needs it (usually Farm.cpp).
// The tech tree screen picks the new node up automatically.
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Every number the upgrades can change. Defaults are the values at the
// very start of the game, before anything has been bought.
struct Stats {
    int patchSize = 3;        // patch is patchSize x patchSize tiles
    float dayLength = 20.f;   // seconds of picking per day
    float pickTime = 1.0f;    // seconds of hovering needed to pick a vegetable
    float growTime = 3.0f;    // seconds for a lettuce to grow (other crops are multiples)
    float valueMult = 1.0f;   // multiplier on coins from every vegetable
    int reach = 0;            // 0 = pick only the tile under the mouse; higher = bigger picking circle
    int cropTier = 0;         // 0 = lettuce, 1 = + carrots, 2 = + pumpkins
    float headStart = 0.f;    // fraction of the patch that is already ripe when a day starts
};

struct Prereq {
    std::string id; // id of the node that must be bought first
    int level;      // level that node must reach
};

struct TechNode {
    std::string id;
    std::string name;
    std::string description;
    int maxLevel = 1;
    double baseCost = 10;     // cost of level 1
    double costGrowth = 1.5;  // each level costs this many times more than the last
    std::vector<Prereq> prereqs;
    float gridX = 0, gridY = 0; // position on the tech tree screen (in node slots)

    // Applies this node's effect at `level` (always >= 1) to the stats.
    std::function<void(Stats&, int level)> apply;
    // Short text describing the effect at a given level, e.g. "4x4 patch".
    std::function<std::string(int level)> describe;

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

    // Buys one level of node `index` if possible. Returns true on success.
    bool tryBuy(size_t index, double& coins);

    // Builds the current Stats from the defaults plus every owned upgrade.
    Stats computeStats() const;

    void resetLevels();

private:
    void addNode(TechNode node);

    std::vector<TechNode> nodes_;
    std::unordered_map<std::string, size_t> index_;
};
