#include "Lang.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace lang {

namespace {
std::unordered_map<std::string, std::string> gTable; // English -> translation
std::unordered_map<std::string, std::string> gMade;  // looked-up strings kept alive (English fallbacks, "Name 12")
std::string gCode = "en";

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') {
            out += '\n';
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

// Calls f(english, translation) for each translated line, and f("@key", value) for headers.
template <typename F> void eachLine(const std::string& text, F f) {
    std::istringstream in(text);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line = line.substr(3); // BOM
        first = false;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t[0] == '@') {
            size_t sp = t.find(' ');
            f(t.substr(0, sp), sp == std::string::npos ? std::string() : trim(t.substr(sp + 1)));
            continue;
        }
        size_t eq = t.find(" = ");
        if (eq == std::string::npos) continue;
        std::string en = unescape(trim(t.substr(0, eq))), tx = unescape(trim(t.substr(eq + 3)));
        if (!en.empty() && !tx.empty()) f(en, tx);
    }
}
// The printf conversions in s, e.g. "Day %d of %s" -> "d s " (a translation must have the same).
// A % that isn't one ("18% faster") doesn't count.
std::string conversions(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '%') continue;
        if (i + 1 < s.size() && s[i + 1] == '%') { ++i; continue; }
        size_t j = i + 1;
        while (j < s.size() && std::strchr("-+#0123456789.", s[j])) ++j;
        while (j < s.size() && std::strchr("hlLqjzt", s[j])) ++j;
        if (j < s.size() && std::strchr("diouxXeEfFgGaAcsp", s[j])) {
            out += s.substr(i + 1, j - i) + " ";
            i = j;
        }
    }
    return out;
}
} // namespace

void use(const std::string& fileText, const std::string& code) {
    gTable.clear();
    gMade.clear();
    gCode = code.empty() ? "en" : code;
    int bad = 0;
    eachLine(fileText, [&bad](const std::string& en, const std::string& tx) {
        if (en[0] == '@') return;
        if (conversions(en) != conversions(tx)) { // a missing or extra %d would crash the game
            SDL_Log("Language %s: \"%s\" has different %%-codes from the English, so it's skipped", gCode.c_str(), tx.c_str());
            ++bad;
            return;
        }
        gTable[en] = tx;
    });
}

const std::string& current() { return gCode; }

Language describe(const std::string& fileText, const std::string& code) {
    Language l;
    l.code = code;
    l.name = code;
    l.locale = code;
    eachLine(fileText, [&l](const std::string& key, const std::string& value) {
        if (key == "@name") l.name = value;
        else if (key == "@locale") l.locale = value;
    });
    return l;
}

std::string pickForDevice(const std::vector<Language>& languages) {
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (!locales) return "";
    std::string found;
    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    for (int i = 0; i < count && found.empty(); ++i) {
        const std::string want = lower(locales[i]->language ? locales[i]->language : "");
        const std::string region = lower(locales[i]->country ? locales[i]->country : "");
        if (want == "en") break; // English is first choice: nothing to load
        for (const auto& l : languages) // exact language + region first ("pt-br")
            if (!region.empty() && lower(l.locale) == want + "-" + region) found = l.code;
        for (const auto& l : languages) // then the language alone ("pt", or "pt-PT" for any pt)
            if (found.empty() && lower(l.locale).substr(0, want.size()) == want &&
                (l.locale.size() == want.size() || l.locale[want.size()] == '-'))
                found = l.code;
    }
    SDL_free(locales);
    return found;
}

const std::string& tr(const std::string& english) {
    if (auto it = gTable.find(english); it != gTable.end()) return it->second;
    if (auto it = gMade.find(english); it != gMade.end()) return it->second;
    // "Bigger Patch 12": translate "Bigger Patch" and keep the number.
    std::string made = english;
    size_t sp = english.find_last_of(' ');
    if (!gTable.empty() && sp != std::string::npos && sp + 1 < english.size()) {
        bool digits = true;
        for (size_t i = sp + 1; i < english.size(); ++i) digits = digits && std::isdigit(static_cast<unsigned char>(english[i]));
        if (digits) {
            auto it = gTable.find(english.substr(0, sp));
            if (it != gTable.end()) made = it->second + english.substr(sp);
        }
    }
    return gMade.emplace(english, made).first->second;
}

const char* trc(const char* english) { return tr(english).c_str(); }

std::string trf(const char* englishFormat, ...) {
    const std::string& fmt = tr(englishFormat);
    char buf[1024];
    va_list args;
    va_start(args, englishFormat);
    std::vsnprintf(buf, sizeof buf, fmt.c_str(), args);
    va_end(args);
    return buf;
}

} // namespace lang
