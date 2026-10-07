// TechData.h - the tech tree as data.
//
// The tech tree lives in include/TechTreeData.h, which is written by the
// TechTreeEditor tool (tools/TechTreeEditor). That header is compiled into the
// game, so the tree is part of the executable - nothing is loaded at runtime.
//
// This file is shared by the game and the editor: it reads and writes the tree
// format, and turns each tech's effects into changes to Stats.
//
// The format is plain text, one setting per line:
//
//   tech patch                      <- id: letters, numbers and _ only; never rename once players have saves
//     name Bigger Patch
//     desc Dig out a bigger patch with room for more vegetables.
//     max 7                         <- maximum level
//     cost 10                       <- price of level 1
//     growth 2.1                    <- each level costs this many times more than the last
//     pos 0 0                       <- column and row on the tech tree screen (halves allowed)
//     icon seeds                    <- optional: which icon to show (default: the tech's own id)
//     requires daylength 2          <- needs "daylength" at level 2 (any number of these lines)
//     effect patchSize add 1        <- stat, operation, amount (any number of these lines)
//   end
//
// Crops are in the same file, before the techs:
//
//   crop carrot                     <- id: letters, numbers and _ only
//     name Carrot
//     value 4                       <- coins per crop (before Coin value upgrades)
//     grow 1.6                      <- grow time = this x the Grow time stat
//     pick 1.25                     <- pick time = this x the Pick time stat
//     weight 35                     <- how often it's planted, compared with the other unlocked crops
//     tier 1                        <- planted once the Crops stat reaches this (0 = from the start)
//     look carrot                   <- built-in drawing: lettuce, carrot, pumpkin or round
//     color e07020                  <- optional colour for the built-in drawing (hex)
//   end
//
// Art: assets/crops/<id>.png (ripe) and assets/crops/<id>_sprout.png replace the built-in look.
//
// Operations, for a tech at level L:
//   add       stat += amount x L                    e.g. day length + 5 seconds per level
//   percent   stat x (1 + amount/100 x L)           e.g. +25% coins per level
//   multiply  stat x amount^L                       e.g. pick time x 0.82 per level
//   atleast   stat = max(stat, amount)              e.g. unlock carrots (crop tier 1)
#pragma once

#include "TechTree.h" // Stats

#include <string>
#include <vector>

namespace techdata {

// ---- Stats a tech can change -------------------------------------------------

struct StatInfo {
    const char* key;   // name used in the file, e.g. "dayLength"
    const char* label; // shown in the editor, e.g. "Day length"
    const char* help;  // one line explaining it
    bool isInt;
};

const std::vector<StatInfo>& stats();
int statIndex(const std::string& key); // -1 if unknown
float getStat(const Stats& s, int index);
void setStat(Stats& s, int index, float value);
// Human-readable value, e.g. "16 plants", "25 second days", "x1.50 coins".
std::string formatStat(int index, const Stats& s);

// ---- Operations --------------------------------------------------------------

enum class Op { Add, Percent, Multiply, AtLeast, Count };
const char* opKey(Op op);   // "add", "percent", "multiply", "atleast"
const char* opLabel(Op op); // "+ per level", "+% per level", "x per level", "at least"
bool parseOp(const std::string& key, Op& out);

// ---- A tech ------------------------------------------------------------------

struct Effect {
    std::string stat = "dayLength";
    Op op = Op::Add;
    float amount = 1.f;
};

struct Requirement {
    std::string id;
    int level = 1;
};

struct TechDef {
    std::string id;
    std::string name;
    std::string description;
    int maxLevel = 1;
    double baseCost = 10.0;
    double costGrowth = 1.5;
    float gridX = 0.f, gridY = 0.f;
    std::string icon; // icon name; empty = the tech's id. Art: assets/tree/icons/<name>.png
    std::vector<Requirement> needs; // ("requires" is a C++20 keyword)
    std::vector<Effect> effects;
};

// Applies a tech's effects at `level` (>= 1) to `s`.
void applyEffects(const TechDef& t, Stats& s, int level);
// What the tech gives at `level` (0 = not bought), e.g. "16 plants".
std::string describe(const TechDef& t, int level);
// Price of the next level when `owned` levels are already bought.
double costAt(const TechDef& t, int owned);

// ---- Reading and writing -----------------------------------------------------

// ---- Crops -------------------------------------------------------------------

struct CropDef {
    std::string id;
    std::string name;
    double value = 1.0; // coins
    float grow = 1.f;   // x the Grow time stat
    float pick = 1.f;   // x the Pick time stat
    float weight = 10.f;
    int tier = 0;       // needs the Crops stat at least this
    std::string look = "round";
    std::string color;  // hex, "" = the look's usual colour
};

// The crops the game plants. The game sets these from the tree file; the
// editor sets them as you edit (the Crops stat's description uses them).
// If nothing was set, it's lettuce, carrot and pumpkin.
void setCrops(const std::vector<CropDef>& crops);
const std::vector<CropDef>& crops();
const std::vector<CropDef>& defaultCrops();
int cropIndex(const std::string& id); // -1 if unknown

struct ParseResult {
    std::vector<TechDef> techs;
    std::vector<CropDef> crops;      // empty if the file has none (use defaultCrops())
    std::vector<std::string> errors; // "line 12: unknown stat 'speed'"
};
ParseResult parse(const std::string& text);
std::string serialize(const std::vector<TechDef>& techs, const std::vector<CropDef>& crops = {});

// Problems that would make the tree misbehave (duplicate ids, missing
// requirements, loops, ...). Empty = all good. If `forTech` is not empty, only
// problems about that tech are returned.
std::vector<std::string> validate(const std::vector<TechDef>& techs, const std::string& forTech = "");
// Problems with the crops ("crop <id>: ..."), e.g. one no tech ever unlocks.
std::vector<std::string> validateCrops(const std::vector<CropDef>& crops, const std::vector<TechDef>& techs);

bool isValidId(const std::string& id);

// ---- Icons -------------------------------------------------------------------

// The icon a tech shows: its `icon` setting, or its id if that's empty.
std::string iconOf(const TechDef& t);

struct BuiltinIcon {
    const char* key;   // icon name, e.g. "carrots"
    const char* label; // shown in the editor's picker
};
// Icons the game can draw itself (any of these works without an image file).
const std::vector<BuiltinIcon>& builtinIcons();
bool isBuiltinIcon(const std::string& name);

// TechTreeData.h is a C++ header holding the text above in raw string
// literals. These convert between the two.
std::string toHeader(const std::vector<TechDef>& techs, const std::vector<CropDef>& crops = {});
bool fromHeader(const std::string& header, std::string& textOut);
std::string joinChunks(const char* const* chunks); // for the compiled-in copy

} // namespace techdata
