#include "TechData.h"

#include "CropLooks.h"
#include "TileShapes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace techdata {

namespace {

std::string fmt(const char* f, double a, double b = 0.0) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), f, a, b);
    return buf;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Keeps a text field on one line and out of the way of the raw-string delimiter.
std::string clean(std::string s) {
    for (char& c : s)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    for (size_t p; (p = s.find(")TECHTREE")) != std::string::npos;) s.erase(p, 9);
    return trim(s);
}

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.10g", v);
    return buf;
}

// Floats are written with the fewest digits that read back as the same
// float, so 0.82 is saved as "0.82" rather than "0.8199999928".
std::string num(float v) {
    char buf[64];
    for (int digits = 6; digits <= 9; ++digits) {
        std::snprintf(buf, sizeof(buf), "%.*g", digits, static_cast<double>(v));
        if (std::strtof(buf, nullptr) == v) break;
    }
    return buf;
}

const char* kDelimOpen = "R\"TECHTREE(";
const char* kDelimClose = ")TECHTREE\"";

} // namespace

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

const std::vector<StatInfo>& stats() {
    static const std::vector<StatInfo> list = {
        {"patchSize", "Patch size", "Room in the bed = size x size plants. Starts at 2 (room for 4).", true},
        {"maxCrops", "Crops at once", "Vegetables growing at the same time (up to the patch's room). Starts at 4.", true},
        {"dayLength", "Day length", "Seconds of picking per day. Starts at 12.", false},
        {"pickTime", "Pick time", "Seconds of hovering needed to pick a vegetable. Starts at 1.", false},
        {"growTime", "Grow time", "Seconds for a lettuce to grow (other crops take longer). Starts at 3.", false},
        {"valueMult", "Coin value", "Multiplier on the coins from every vegetable. Starts at 1.", false},
        {"reach", "Reach", "Picks everything within this radius of the pointer. Starts at 0 (one plant).", true},
        {"cropTier", "Crops", "Which crops get planted: each crop unlocks at its own level of this (see Crops). Starts at 0.", true},
        {"headStart", "Head start", "Fraction of the bed already ripe when a day starts (0 to 1). Starts at 0.", false},
        {"autoPickChance", "Auto-pick chance", "% chance that picking a crop also picks ripe crops near it. Starts at 0.", false},
        {"autoPickCount", "Auto-pick crops", "How many nearby ripe crops an auto-pick picks. Starts at 0 (off).", true},
        {"autoPickRadius", "Auto-pick radius", "How near counts as nearby for auto-pick, in plant widths. Starts at 0.", false},
        {"farmers", "Farmers", "Helpers walking the patch and picking ripe crops. Starts at 0.", true},
        {"farmerSpeed", "Farmer speed", "How fast farmers walk, in plant widths per second. Starts at 2.", false},
        {"farmerPickTime", "Farmer pick time", "Seconds for a farmer to pick a lettuce (others take longer). Starts at 1.5.", false},
    };
    return list;
}

int statIndex(const std::string& key) {
    const auto& list = stats();
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        if (key == list[i].key) return i;
    return -1;
}

float getStat(const Stats& s, int index) {
    switch (index) {
    case 0: return static_cast<float>(s.patchSize);
    case 1: return static_cast<float>(s.maxCrops);
    case 2: return s.dayLength;
    case 3: return s.pickTime;
    case 4: return s.growTime;
    case 5: return s.valueMult;
    case 6: return static_cast<float>(s.reach);
    case 7: return static_cast<float>(s.cropTier);
    case 8: return s.headStart;
    case 9: return s.autoPickChance;
    case 10: return static_cast<float>(s.autoPickCount);
    case 11: return s.autoPickRadius;
    case 12: return static_cast<float>(s.farmers);
    case 13: return s.farmerSpeed;
    case 14: return s.farmerPickTime;
    }
    return 0.f;
}

void setStat(Stats& s, int index, float v) {
    switch (index) {
    case 0: s.patchSize = static_cast<int>(std::lround(v)); break;
    case 1: s.maxCrops = static_cast<int>(std::lround(v)); break;
    case 2: s.dayLength = v; break;
    case 3: s.pickTime = v; break;
    case 4: s.growTime = v; break;
    case 5: s.valueMult = v; break;
    case 6: s.reach = static_cast<int>(std::lround(v)); break;
    case 7: s.cropTier = static_cast<int>(std::lround(v)); break;
    case 8: s.headStart = v; break;
    case 9: s.autoPickChance = std::clamp(v, 0.f, 100.f); break;
    case 10: s.autoPickCount = std::max(0, static_cast<int>(std::lround(v))); break;
    case 11: s.autoPickRadius = std::max(0.f, v); break;
    case 12: s.farmers = std::clamp(static_cast<int>(std::lround(v)), 0, 50); break;
    case 13: s.farmerSpeed = std::max(0.1f, v); break;
    case 14: s.farmerPickTime = std::max(0.001f, v); break;
    }
}

std::string formatStat(int index, const Stats& s) {
    float v = getStat(s, index);
    switch (index) {
    case 0: return fmt("room for %.0f plants", static_cast<double>(s.patchSize) * s.patchSize);
    case 1: return fmt(s.maxCrops == 1 ? "%.0f crop at once" : "%.0f crops at once", v);
    case 2: return fmt(std::fabs(v - std::round(v)) < 0.01f ? "%.0f second days" : "%.1f second days", v);
    case 3: return fmt("%.2fs to pick", v);
    case 4: return fmt("%.2fs to grow", v);
    case 5: return fmt("x%.2f coins", v);
    case 6: return s.reach <= 0 ? std::string("1 plant at a time") : fmt("reach radius %.0f", v);
    case 7: {
        // Names of the crops this level plants, e.g. "lettuce, carrot & pumpkin".
        std::vector<std::string> names;
        for (const auto& c : crops())
            if (c.tier <= s.cropTier) {
                std::string n = c.name;
                for (char& ch : n) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                names.push_back(n);
            }
        if (names.empty()) return std::string("no crops");
        if (names.size() > 4) return names[0] + ", " + names[1] + " + " + std::to_string(names.size() - 2) + " more";
        std::string out;
        for (size_t k = 0; k < names.size(); ++k)
            out += (k == 0 ? "" : k + 1 == names.size() ? " & " : ", ") + names[k];
        return out;
    }
    case 8: return fmt("%.0f%% ripe at dawn", std::clamp(v, 0.f, 1.f) * 100.0);
    case 9: return fmt(std::fabs(v - std::round(v)) < 0.01f ? "%.0f%% auto-pick chance" : "%.1f%% auto-pick chance", v);
    case 10: return s.autoPickCount <= 0 ? std::string("no auto-pick")
                   : fmt(s.autoPickCount == 1 ? "auto-picks %.0f crop" : "auto-picks up to %.0f crops", v);
    case 11: return v <= 0.f ? std::string("auto-pick off") : fmt("auto-pick within %.1f plants", v);
    case 12: return s.farmers <= 0 ? std::string("no farmers") : fmt(s.farmers == 1 ? "%.0f farmer" : "%.0f farmers", v);
    case 13: return fmt("farmers walk %.2f plants/s", v);
    case 14: return fmt("farmers pick in %.2fs", v);
    }
    return "";
}

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Crops
// ---------------------------------------------------------------------------

namespace {
std::vector<CropDef>& cropRegistry() {
    static std::vector<CropDef> list;
    return list;
}
} // namespace

const std::vector<CropDef>& defaultCrops() {
    static const std::vector<CropDef> list = {
        {"lettuce", "Lettuce", 1.0, 1.0f, 1.0f, 45.f, 0, "lettuce", ""},
        {"carrot", "Carrot", 4.0, 1.6f, 1.25f, 35.f, 1, "carrot", ""},
        {"pumpkin", "Pumpkin", 15.0, 2.6f, 1.6f, 20.f, 2, "pumpkin", ""},
    };
    return list;
}

void setCrops(const std::vector<CropDef>& list) { cropRegistry() = list; }

const std::vector<CropDef>& crops() { return cropRegistry().empty() ? defaultCrops() : cropRegistry(); }

int cropIndex(const std::string& id) {
    const auto& list = crops();
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        if (list[i].id == id) return i;
    return -1;
}

const char* opKey(Op op) {
    switch (op) {
    case Op::Add: return "add";
    case Op::Percent: return "percent";
    case Op::Multiply: return "multiply";
    case Op::AtLeast: return "atleast";
    case Op::Count: break;
    }
    return "add";
}

const char* opLabel(Op op) {
    switch (op) {
    case Op::Add: return "+ per level";
    case Op::Percent: return "+% per level";
    case Op::Multiply: return "x per level";
    case Op::AtLeast: return "set at least";
    case Op::Count: break;
    }
    return "?";
}

bool parseOp(const std::string& key, Op& out) {
    for (int i = 0; i < static_cast<int>(Op::Count); ++i) {
        if (key == opKey(static_cast<Op>(i))) {
            out = static_cast<Op>(i);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

void applyEffects(const TechDef& t, Stats& s, int level) {
    if (level <= 0) return;
    for (const auto& e : t.effects) {
        int idx = statIndex(e.stat);
        if (idx < 0) continue;
        float v = getStat(s, idx);
        switch (e.op) {
        case Op::Add: v += e.amount * level; break;
        case Op::Percent: v *= 1.f + e.amount / 100.f * level; break;
        case Op::Multiply: v *= std::pow(e.amount, static_cast<float>(level)); break;
        case Op::AtLeast: v = std::max(v, e.amount); break;
        case Op::Count: break;
        }
        setStat(s, idx, v);
    }
}

std::string describe(const TechDef& t, int level) {
    Stats s;
    applyEffects(t, s, level);
    std::string out;
    std::set<int> shown;
    for (const auto& e : t.effects) {
        int idx = statIndex(e.stat);
        if (idx < 0 || !shown.insert(idx).second) continue;
        if (!out.empty()) out += ", ";
        out += formatStat(idx, s);
    }
    return out.empty() ? std::string("no effect") : out;
}

double costAt(const TechDef& t, int owned) { return std::ceil(t.baseCost * std::pow(t.costGrowth, owned)); }

// ---------------------------------------------------------------------------
// Text format
// ---------------------------------------------------------------------------

ParseResult parse(const std::string& text) {
    ParseResult r;
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    TechDef* cur = nullptr;
    CropDef* crop = nullptr;
    auto err = [&](const std::string& msg) { r.errors.push_back("line " + std::to_string(lineNo) + ": " + msg); };

    while (std::getline(in, raw)) {
        ++lineNo;
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);

        if (key == "version") continue;
        if (key == "crop") {
            if (cur || crop) err("'crop' inside another block - missing 'end'?");
            cur = nullptr;
            r.crops.push_back({});
            crop = &r.crops.back();
            crop->id = rest;
            crop->name = rest;
            continue;
        }
        if (crop) {
            std::istringstream args(rest);
            if (key == "end") crop = nullptr;
            else if (key == "name") crop->name = rest;
            else if (key == "value") { if (!(args >> crop->value)) err("value needs a number"); }
            else if (key == "grow") { if (!(args >> crop->grow)) err("grow needs a number"); }
            else if (key == "pick") { if (!(args >> crop->pick)) err("pick needs a number"); }
            else if (key == "weight") { if (!(args >> crop->weight)) err("weight needs a number"); }
            else if (key == "tier") { if (!(args >> crop->tier)) err("tier needs a whole number"); }
            else if (key == "look") {
                crop->look = rest;
                if (!croplook::isLook(rest)) err("unknown look '" + rest + "'");
            } else if (key == "color") {
                crop->color = rest;
                if (!croplook::isValidColor(rest)) err("color must be 6 hex digits, e.g. e05040");
            } else err("unknown crop setting '" + key + "'");
            continue;
        }
        if (key == "tech") {
            if (cur) err("'tech' inside another tech - missing 'end'?");
            r.techs.push_back({});
            cur = &r.techs.back();
            cur->id = rest;
            continue;
        }
        if (!cur) {
            err("'" + key + "' outside a tech");
            continue;
        }
        if (key == "end") {
            cur = nullptr;
            continue;
        }
        std::istringstream args(rest);
        if (key == "name") cur->name = rest;
        else if (key == "icon") {
            cur->icon = rest;
            if (!isValidId(rest)) err("icon must be letters, numbers or _");
        } else if (key == "shape") {
            cur->shape = rest;
            if (!tileshape::isShape(rest)) err("unknown shape '" + rest + "' (square, circle, triangle, pentagon)");
        } else if (key == "color" || key == "lockedcolor") {
            (key == "color" ? cur->color : cur->lockedColor) = rest;
            if (!tileshape::isValidHex(rest)) err(key + " must be 6 hex digits, e.g. 47613f");
        }
        else if (key == "desc") cur->description = rest;
        else if (key == "max") {
            if (!(args >> cur->maxLevel)) err("max needs a whole number");
        } else if (key == "cost") {
            if (!(args >> cur->baseCost)) err("cost needs a number");
        } else if (key == "growth") {
            if (!(args >> cur->costGrowth)) err("growth needs a number");
        } else if (key == "pos") {
            if (!(args >> cur->gridX >> cur->gridY)) err("pos needs two numbers");
        } else if (key == "requires") {
            Requirement q;
            args >> q.id;
            if (!(args >> q.level)) q.level = 1;
            if (q.id.empty()) err("requires needs a tech id");
            else cur->needs.push_back(q);
        } else if (key == "effect") {
            Effect e;
            std::string op;
            if (!(args >> e.stat >> op >> e.amount)) {
                err("effect needs: stat operation amount");
                continue;
            }
            if (statIndex(e.stat) < 0) err("unknown stat '" + e.stat + "'");
            if (!parseOp(op, e.op)) err("unknown operation '" + op + "'");
            cur->effects.push_back(e);
        } else {
            err("unknown setting '" + key + "'");
        }
    }
    if (cur) r.errors.push_back("end of file: tech '" + cur->id + "' has no 'end'");
    if (crop) r.errors.push_back("end of file: crop '" + crop->id + "' has no 'end'");
    return r;
}

std::string serialize(const std::vector<TechDef>& techs, const std::vector<CropDef>& cropList) {
    std::ostringstream o;
    o << "# IncraVegetable tech tree. Made with TechTreeEditor (tools/TechTreeEditor).\n"
      << "# Format: see include/TechData.h\n"
      << "version 1\n\n";
    for (const auto& c : cropList) {
        o << "crop " << clean(c.id) << "\n";
        o << "  name " << clean(c.name) << "\n";
        o << "  value " << num(c.value) << "\n";
        o << "  grow " << num(c.grow) << "\n";
        o << "  pick " << num(c.pick) << "\n";
        o << "  weight " << num(c.weight) << "\n";
        o << "  tier " << c.tier << "\n";
        o << "  look " << clean(c.look) << "\n";
        if (!clean(c.color).empty()) o << "  color " << clean(c.color) << "\n";
        o << "end\n\n";
    }
    for (const auto& t : techs) {
        o << "tech " << clean(t.id) << "\n";
        o << "  name " << clean(t.name) << "\n";
        if (!clean(t.description).empty()) o << "  desc " << clean(t.description) << "\n";
        o << "  max " << t.maxLevel << "\n";
        o << "  cost " << num(t.baseCost) << "\n";
        o << "  growth " << num(t.costGrowth) << "\n";
        o << "  pos " << num(t.gridX) << " " << num(t.gridY) << "\n";
        if (!t.icon.empty() && t.icon != t.id) o << "  icon " << clean(t.icon) << "\n";
        if (!t.shape.empty() && t.shape != "square") o << "  shape " << clean(t.shape) << "\n";
        if (!clean(t.color).empty()) o << "  color " << clean(t.color) << "\n";
        if (!clean(t.lockedColor).empty()) o << "  lockedcolor " << clean(t.lockedColor) << "\n";
        for (const auto& q : t.needs) o << "  requires " << clean(q.id) << " " << q.level << "\n";
        for (const auto& e : t.effects)
            o << "  effect " << clean(e.stat) << " " << opKey(e.op) << " " << num(e.amount) << "\n";
        o << "end\n\n";
    }
    return o.str();
}

// ---------------------------------------------------------------------------
// Checking
// ---------------------------------------------------------------------------

std::string iconOf(const TechDef& t) { return t.icon.empty() ? t.id : t.icon; }

const std::vector<BuiltinIcon>& builtinIcons() {
    static const std::vector<BuiltinIcon> list = {
        {"patch", "Garden plot"},     {"seeds", "Seed packet"},      {"daylength", "Sun"},
        {"headstart", "Sunrise"},     {"pickspeed", "Quick lettuce"}, {"growspeed", "Sprout"},
        {"value", "Coins"},           {"reach", "Reach ring"},       {"carrots", "Carrot"},
        {"pumpkins", "Pumpkin"},      {"autopick", "Sparkle"},       {"autopickchance", "Clover"},
        {"autopickcount", "Bunch"},   {"autopickradius", "Ring"},    {"farmhand", "Farmer"},
        {"farmcrew", "Two farmers"},  {"farmerspeed", "Boot"},       {"farmerpick", "Shears"},
    };
    return list;
}

bool isBuiltinIcon(const std::string& name) {
    for (const auto& b : builtinIcons())
        if (name == b.key) return true;
    return false;
}

bool isValidId(const std::string& id) {
    if (id.empty() || id.size() > 40) return false;
    for (char c : id)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

// Runs every frame in the editor, so it stays quick on big trees: lookups go
// through hash maps and the loop search works on indexes, not names.
std::vector<Problem> findProblems(const std::vector<TechDef>& techs) {
    std::vector<Problem> found; // (tech id, problem)
    const int n = static_cast<int>(techs.size());
    std::unordered_map<std::string, int> index;
    index.reserve(n * 2);
    for (int i = 0; i < n; ++i) {
        if (!index.emplace(techs[i].id, i).second)
            found.push_back({techs[i].id, "id '" + techs[i].id + "' is used more than once"});
    }
    // Techs at each spot, to spot overlaps without comparing every pair.
    std::map<std::pair<float, float>, std::vector<int>> spots;
    for (int i = 0; i < n; ++i) spots[{techs[i].gridX, techs[i].gridY}].push_back(i);

    // Each requirement as an index (-1 = missing or itself), for the loop search.
    std::vector<std::vector<int>> needIdx(n);
    for (int i = 0; i < n; ++i) {
        const TechDef& t = techs[i];
        const std::string& id = t.id;
        auto add = [&](const std::string& msg) { found.push_back({id, msg}); };
        if (!isValidId(id)) add("id must be letters, numbers or _ (no spaces)");
        if (!t.icon.empty() && !isValidId(t.icon)) add("icon must be letters, numbers or _");
        if (!tileshape::isShape(t.shape)) add("unknown shape '" + t.shape + "'");
        if (!tileshape::isValidHex(t.color)) add("colour must be 6 hex digits, e.g. 47613f");
        if (!tileshape::isValidHex(t.lockedColor)) add("locked colour must be 6 hex digits, e.g. 2e3530");
        if (trim(t.name).empty()) add("has no name");
        if (t.maxLevel < 1) add("max level must be at least 1");
        if (t.baseCost < 0) add("cost can't be negative");
        if (t.costGrowth <= 0) add("cost growth must be more than 0");
        if (t.effects.empty()) add("has no effects (buying it does nothing)");
        for (const auto& e : t.effects) {
            if (statIndex(e.stat) < 0) add("unknown stat '" + e.stat + "'");
            if (e.op == Op::Multiply && e.amount <= 0) add("multiply amount must be more than 0");
        }
        for (const auto& q : t.needs) {
            auto it = index.find(q.id);
            if (q.id == id) add("requires itself");
            else if (it == index.end()) add("requires '" + q.id + "', which doesn't exist");
            else {
                if (q.level < 1 || q.level > techs[it->second].maxLevel)
                    add("requires " + q.id + " level " + std::to_string(q.level) + ", but it only goes to " +
                        std::to_string(techs[it->second].maxLevel));
                needIdx[i].push_back(it->second);
            }
        }
        for (int o : spots[{t.gridX, t.gridY}]) {
            if (o >= i) break;
            add("is in the same spot as '" + techs[o].id + "'");
        }
    }

    // Loops: A needs B needs ... needs A means none of them can ever be bought.
    // (Duplicate ids all point at the first tech with that id, as above.)
    std::vector<char> state(n, 0); // 0 new, 1 visiting, 2 done
    std::vector<char> inLoop(n, 0);
    std::vector<int> path;
    std::vector<int> posInPath(n, -1);
    std::function<void(int)> dfs = [&](int v) {
        state[v] = 1;
        posInPath[v] = static_cast<int>(path.size());
        path.push_back(v);
        for (int w : needIdx[v]) {
            if (w == v) continue;
            if (state[w] == 1) {
                for (size_t p = posInPath[w]; p < path.size(); ++p) inLoop[path[p]] = 1;
            } else if (state[w] == 0) {
                dfs(w);
            }
        }
        path.pop_back();
        posInPath[v] = -1;
        state[v] = 2;
    };
    for (int i = 0; i < n; ++i) {
        int first = index[techs[i].id];
        if (state[first] == 0) dfs(first);
    }
    // In id order, as before.
    std::set<std::string> loopIds;
    for (int i = 0; i < n; ++i)
        if (inLoop[i]) loopIds.insert(techs[i].id);
    for (const auto& id : loopIds) found.push_back({id, "is part of a requirement loop, so it can never be bought"});
    return found;
}

std::vector<std::string> validate(const std::vector<TechDef>& techs, const std::string& forTech) {
    std::vector<std::string> out;
    for (const auto& [id, msg] : findProblems(techs)) {
        if (!forTech.empty() && id != forTech) continue;
        out.push_back(forTech.empty() ? (id.empty() ? "(no id)" : id) + ": " + msg : msg);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Embedding in a C++ header
// ---------------------------------------------------------------------------

std::vector<std::string> validateCrops(const std::vector<CropDef>& list, const std::vector<TechDef>& techs) {
    std::vector<std::string> out;
    if (list.empty()) out.push_back("crops: there are no crops - add at least one");
    // The highest Crops level the tree can reach (everything bought).
    Stats full;
    for (const auto& t : techs) applyEffects(t, full, std::max(1, t.maxLevel));
    std::set<std::string> seen;
    bool startCrop = false;
    for (const auto& c : list) {
        auto add = [&](const std::string& m) { out.push_back("crop " + c.id + ": " + m); };
        if (!isValidId(c.id)) add("id must be letters, numbers or _ (no spaces)");
        if (!seen.insert(c.id).second) add("id is used more than once");
        if (trim(c.name).empty()) add("has no name");
        if (c.value < 0) add("value can't be negative");
        if (c.grow <= 0) add("grow must be more than 0");
        if (c.pick <= 0) add("pick must be more than 0");
        if (c.weight <= 0) add("weight must be more than 0, or it's never planted");
        if (c.tier < 0) add("tier can't be negative");
        if (!croplook::isLook(c.look)) add("unknown look '" + c.look + "'");
        if (!croplook::isValidColor(c.color)) add("color must be 6 hex digits, e.g. e05040");
        if (c.tier <= 0) startCrop = true;
        else if (c.tier > full.cropTier)
            add("nothing unlocks it - it needs a tech with effect Crops 'set at least' " + std::to_string(c.tier));
    }
    if (!list.empty() && !startCrop) out.push_back("crops: no crop has tier 0, so nothing grows at the start");
    return out;
}

std::string toHeader(const std::vector<TechDef>& techs, const std::vector<CropDef>& cropList) {
    std::string text = serialize(techs, cropList);
    std::ostringstream o;
    o << "// TechTreeData.h - the tech tree, compiled into the game.\n"
      << "//\n"
      << "// GENERATED by TechTreeEditor (tools/TechTreeEditor). Open this file in the\n"
      << "// editor to change the tree. Hand edits work too if you keep the format\n"
      << "// (see include/TechData.h) - the editor will read them back.\n"
      << "#pragma once\n\n"
      << "// The text is split into chunks to stay under compiler string-length limits.\n"
      << "inline constexpr const char* kTechTreeData[] = {\n";
    // Chunk on line boundaries.
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = std::min(text.size(), pos + 8000);
        if (end < text.size()) {
            size_t nl = text.rfind('\n', end);
            if (nl != std::string::npos && nl > pos) end = nl + 1;
        }
        o << kDelimOpen << text.substr(pos, end - pos) << kDelimClose << ",\n";
        pos = end;
    }
    o << "    nullptr,\n};\n";
    return o.str();
}

bool fromHeader(const std::string& header, std::string& textOut) {
    textOut.clear();
    bool any = false;
    size_t pos = 0;
    const std::string open = kDelimOpen, close = kDelimClose;
    while ((pos = header.find(open, pos)) != std::string::npos) {
        size_t start = pos + open.size();
        size_t end = header.find(close, start);
        if (end == std::string::npos) return false;
        textOut += header.substr(start, end - start);
        pos = end + close.size();
        any = true;
    }
    return any;
}

std::string joinChunks(const char* const* chunks) {
    std::string out;
    for (; chunks && *chunks; ++chunks) out += *chunks;
    return out;
}

} // namespace techdata
