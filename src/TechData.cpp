#include "TechData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>

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

const char* kDelimOpen = "R\"TECHTREE(";
const char* kDelimClose = ")TECHTREE\"";

} // namespace

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

const std::vector<StatInfo>& stats() {
    static const std::vector<StatInfo> list = {
        {"patchSize", "Patch size", "Vegetables in the bed = size x size. Starts at 3 (9 vegetables).", true},
        {"dayLength", "Day length", "Seconds of picking per day. Starts at 20.", false},
        {"pickTime", "Pick time", "Seconds of hovering needed to pick a vegetable. Starts at 1.", false},
        {"growTime", "Grow time", "Seconds for a lettuce to grow (other crops take longer). Starts at 3.", false},
        {"valueMult", "Coin value", "Multiplier on the coins from every vegetable. Starts at 1.", false},
        {"reach", "Reach", "Picks everything within this radius of the pointer. Starts at 0 (one plant).", true},
        {"cropTier", "Crops", "Which crops get planted: 0 lettuce, 1 + carrots, 2 + pumpkins.", true},
        {"headStart", "Head start", "Fraction of the bed already ripe when a day starts (0 to 1). Starts at 0.", false},
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
    case 1: return s.dayLength;
    case 2: return s.pickTime;
    case 3: return s.growTime;
    case 4: return s.valueMult;
    case 5: return static_cast<float>(s.reach);
    case 6: return static_cast<float>(s.cropTier);
    case 7: return s.headStart;
    }
    return 0.f;
}

void setStat(Stats& s, int index, float v) {
    switch (index) {
    case 0: s.patchSize = static_cast<int>(std::lround(v)); break;
    case 1: s.dayLength = v; break;
    case 2: s.pickTime = v; break;
    case 3: s.growTime = v; break;
    case 4: s.valueMult = v; break;
    case 5: s.reach = static_cast<int>(std::lround(v)); break;
    case 6: s.cropTier = static_cast<int>(std::lround(v)); break;
    case 7: s.headStart = v; break;
    }
}

std::string formatStat(int index, const Stats& s) {
    float v = getStat(s, index);
    switch (index) {
    case 0: return fmt("%.0f plants", static_cast<double>(s.patchSize) * s.patchSize);
    case 1: return fmt(std::fabs(v - std::round(v)) < 0.01f ? "%.0f second days" : "%.1f second days", v);
    case 2: return fmt("%.2fs to pick", v);
    case 3: return fmt("%.2fs to grow", v);
    case 4: return fmt("x%.2f coins", v);
    case 5: return s.reach <= 0 ? std::string("1 plant at a time") : fmt("reach radius %.0f", v);
    case 6: return s.cropTier <= 0 ? std::string("lettuce only")
                   : s.cropTier == 1 ? std::string("+ carrots")
                                     : std::string("+ carrots & pumpkins");
    case 7: return fmt("%.0f%% ripe at dawn", std::clamp(v, 0.f, 1.f) * 100.0);
    }
    return "";
}

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

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
    return r;
}

std::string serialize(const std::vector<TechDef>& techs) {
    std::ostringstream o;
    o << "# IncraVegetable tech tree. Made with TechTreeEditor (tools/TechTreeEditor).\n"
      << "# Format: see include/TechData.h\n"
      << "version 1\n\n";
    for (const auto& t : techs) {
        o << "tech " << clean(t.id) << "\n";
        o << "  name " << clean(t.name) << "\n";
        if (!clean(t.description).empty()) o << "  desc " << clean(t.description) << "\n";
        o << "  max " << t.maxLevel << "\n";
        o << "  cost " << num(t.baseCost) << "\n";
        o << "  growth " << num(t.costGrowth) << "\n";
        o << "  pos " << num(t.gridX) << " " << num(t.gridY) << "\n";
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

bool isValidId(const std::string& id) {
    if (id.empty() || id.size() > 40) return false;
    for (char c : id)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

std::vector<std::string> validate(const std::vector<TechDef>& techs, const std::string& forTech) {
    std::vector<std::pair<std::string, std::string>> found; // (tech id, problem)
    std::map<std::string, int> index;
    for (int i = 0; i < static_cast<int>(techs.size()); ++i) {
        if (index.count(techs[i].id)) found.push_back({techs[i].id, "id '" + techs[i].id + "' is used more than once"});
        else index[techs[i].id] = i;
    }
    for (const auto& t : techs) {
        const std::string& id = t.id;
        auto add = [&](const std::string& msg) { found.push_back({id, msg}); };
        if (!isValidId(id)) add("id must be letters, numbers or _ (no spaces)");
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
            else if (q.level < 1 || q.level > techs[it->second].maxLevel)
                add("requires " + q.id + " level " + std::to_string(q.level) + ", but it only goes to " +
                    std::to_string(techs[it->second].maxLevel));
        }
        for (const auto& o : techs) {
            if (&o != &t && o.gridX == t.gridX && o.gridY == t.gridY && &o < &t)
                add("is in the same spot as '" + o.id + "'");
        }
    }
    // Loops: A needs B needs ... needs A means none of them can ever be bought.
    std::map<std::string, int> state; // 0 new, 1 visiting, 2 done
    std::set<std::string> inLoop;
    std::function<void(const std::string&, std::vector<std::string>&)> dfs = [&](const std::string& id,
                                                                                  std::vector<std::string>& path) {
        state[id] = 1;
        path.push_back(id);
        auto it = index.find(id);
        if (it != index.end()) {
            for (const auto& q : techs[it->second].needs) {
                if (!index.count(q.id) || q.id == id) continue;
                if (state[q.id] == 1) {
                    auto start = std::find(path.begin(), path.end(), q.id);
                    for (auto p = start; p != path.end(); ++p) inLoop.insert(*p);
                } else if (state[q.id] == 0) {
                    dfs(q.id, path);
                }
            }
        }
        path.pop_back();
        state[id] = 2;
    };
    for (const auto& t : techs) {
        if (state[t.id] == 0) {
            std::vector<std::string> path;
            dfs(t.id, path);
        }
    }
    for (const auto& id : inLoop) found.push_back({id, "is part of a requirement loop, so it can never be bought"});

    std::vector<std::string> out;
    for (const auto& [id, msg] : found) {
        if (!forTech.empty() && id != forTech) continue;
        out.push_back(forTech.empty() ? (id.empty() ? "(no id)" : id) + ": " + msg : msg);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Embedding in a C++ header
// ---------------------------------------------------------------------------

std::string toHeader(const std::vector<TechDef>& techs) {
    std::string text = serialize(techs);
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
