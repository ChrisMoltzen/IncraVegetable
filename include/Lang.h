// Lang.h - translating the game's text.
//
// The game is written in English, and the English text is the key: every
// piece of text the player sees goes through tr() (or trf() when it has
// numbers in it), which looks it up in the chosen language's file and falls
// back to the English if that language hasn't translated it yet.
//
//   draw::text(r, x, y, tr("Settings"), ...);
//   draw::text(r, x, y, trf("Day %d", day), ...);
//
// Languages are files in assets/lang/, one per language, in UTF-8:
//
//   @name Español          <- shown in Settings > Language (in that language)
//   @locale es             <- the device language it's picked for automatically
//   # comments start with #
//   Settings = Ajustes
//   Day %d = Día %d
//
// Each line is the English, " = ", then the translation. Keep any %d, %s,
// %.1f etc. in the translation, in the same order. Use \n for a line break.
// assets/lang/template.txt lists every piece of text in the game; run
// tools/lang/update.py after changing text to bring it and every language
// file up to date (it adds what's missing, marked "# TODO").
//
// Tech names ending in a number ("Bigger Patch 12") only need the name
// ("Bigger Patch") translated: the number is kept.
#pragma once

#include <string>
#include <vector>

namespace lang {

struct Language {
    std::string code;  // file name without .txt, e.g. "es"
    std::string name;  // @name, e.g. "Español"
    std::string locale; // @locale, e.g. "es" or "pt-BR"
};

// Uses the language in `fileText` (a whole language file). Empty = English.
void use(const std::string& fileText, const std::string& code);
const std::string& current(); // code of the language in use, "en" for English

// Reads the @name and @locale lines of a language file.
Language describe(const std::string& fileText, const std::string& code);

// The best of `languages` for the device's preferred languages (SDL_GetPreferredLocales),
// e.g. "pt-BR" picks a pt-BR file, else a pt one. "" = English.
std::string pickForDevice(const std::vector<Language>& languages);

const std::string& tr(const std::string& english);
// Marks English text in a list for translation (tools/lang/update.py finds it) without
// translating it yet: translate it with tr() where it's shown.
constexpr const char* N_(const char* english) { return english; }
const char* trc(const char* english); // the same, for C-style strings that must stay valid
std::string trf(const char* englishFormat, ...);

} // namespace lang

using lang::tr;
using lang::N_;
using lang::trc;
using lang::trf;
