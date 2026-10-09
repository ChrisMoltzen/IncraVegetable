#include "SaveSystem.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "Draw.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace {
// Save file layout: "IVSV" | version (1 byte) | nonce (8) | scrambled text | check (8).
constexpr char kMagic[4] = {'I', 'V', 'S', 'V'};
constexpr Uint8 kVersion = 1;
constexpr Uint64 kKey = 0x6A09E667F3BCC908ull;  // scramble key
constexpr Uint64 kSalt = 0xBB67AE8584CAA73Bull; // mixed into the check value

Uint64 splitmix(Uint64& state) {
    Uint64 z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void scrambleText(std::string& bytes, Uint64 nonce) {
    Uint64 state = kKey ^ nonce;
    Uint64 word = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i % 8 == 0) word = splitmix(state);
        bytes[i] = static_cast<char>(bytes[i] ^ static_cast<char>(word >> (8 * (i % 8))));
    }
}

Uint64 checkValue(const std::string& text, Uint64 nonce) {
    Uint64 h = 0xCBF29CE484222325ull ^ kSalt ^ nonce; // FNV-1a, salted
    for (unsigned char c : text) {
        h ^= c;
        h *= 0x100000001B3ull;
    }
    Uint64 state = h;
    return splitmix(state);
}

void putU64(std::string& out, Uint64 v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>(v >> (8 * i)));
}
Uint64 getU64(const std::string& in, size_t at) {
    Uint64 v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<Uint64>(static_cast<unsigned char>(in[at + i])) << (8 * i);
    return v;
}
} // namespace

std::string SaveSystem::encodeSave(const std::string& text) {
    Uint64 nonce = (static_cast<Uint64>(SDL_GetTicksNS()) * 0x9E3779B97F4A7C15ull) ^ (static_cast<Uint64>(SDL_rand_bits()) << 32);
    std::string out(kMagic, 4);
    out.push_back(static_cast<char>(kVersion));
    putU64(out, nonce);
    std::string body = text;
    scrambleText(body, nonce);
    out += body;
    putU64(out, checkValue(text, nonce));
    return out;
}

bool SaveSystem::decodeSave(const std::string& file, std::string& text) {
    if (file.size() >= 4 && std::memcmp(file.data(), kMagic, 4) == 0) {
        if (file.size() < 4 + 1 + 8 + 8 || static_cast<Uint8>(file[4]) != kVersion) return false;
        const Uint64 nonce = getU64(file, 5);
        text = file.substr(13, file.size() - 13 - 8);
        scrambleText(text, nonce);
        return checkValue(text, nonce) == getU64(file, file.size() - 8); // edited or damaged otherwise
    }
#ifdef INCRA_RELEASE
    return false; // release builds only accept scrambled saves
#else
    text = file; // a plain-text save from before saves were scrambled
    return true;
#endif
}

bool SaveSystem::init() {
    char* pref = SDL_GetPrefPath("IncraVegetable", "IncraVegetable");
    if (!pref) {
        SDL_Log("No save folder available: %s", SDL_GetError());
        return false;
    }
    dir_ = pref;
    SDL_free(pref);

    // Version 0.1 kept a single save.txt. Move it into slot 1 so nobody loses progress.
    std::string legacy = dir_ + "save.txt";
    std::string contents;
    if (!slotInfo(0).exists && readFile(legacy, contents) && looksValid(contents)) {
        if (writeFileAtomic(slotPath(0), encodeSave(contents), false)) {
            SDL_RemovePath(legacy.c_str());
            Settings s = loadSettings();
            s.lastSlot = 0;
            saveSettings(s);
        }
    }
    return true;
}

std::string SaveSystem::slotPath(int slot) const {
    return dir_ + "slot" + std::to_string(slot + 1) + ".sav";
}

bool SaveSystem::readFile(const std::string& path, std::string& out) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) return false;
    out.assign(static_cast<const char*>(data), size);
    SDL_free(data);
    return true;
}

bool SaveSystem::writeFileAtomic(const std::string& path, const std::string& contents, bool keepBackup) {
    std::string tmp = path + ".tmp";
    if (!SDL_SaveFile(tmp.c_str(), contents.data(), contents.size())) {
        SDL_Log("Could not write %s: %s", tmp.c_str(), SDL_GetError());
        return false;
    }
    SDL_PathInfo info;
    if (keepBackup && SDL_GetPathInfo(path.c_str(), &info)) {
        SDL_CopyFile(path.c_str(), (path + ".bak").c_str());
    }
    if (!SDL_RenamePath(tmp.c_str(), path.c_str())) {
        // Some platforms refuse to rename over an existing file.
        SDL_RemovePath(path.c_str());
        if (!SDL_RenamePath(tmp.c_str(), path.c_str())) {
            SDL_Log("Could not replace %s: %s", path.c_str(), SDL_GetError());
            return false;
        }
    }
#ifdef __EMSCRIPTEN__
    // In a browser the save folder lives in the page's IndexedDB storage (web/pre.js
    // mounts it); copy the change out there so it's still there next visit.
    EM_ASM({
        if (typeof FS !== 'undefined' && FS.syncfs)
            FS.syncfs(false, function(err) { if (err) console.warn('IncraVegetable: saving failed', err); });
    });
#endif
    return true;
}

bool SaveSystem::looksValid(const std::string& contents) {
    return contents.find("coins ") != std::string::npos && contents.find("day ") != std::string::npos;
}

bool SaveSystem::writeSlot(int slot, const std::string& contents) const {
    if (dir_.empty() || slot < 0 || slot >= kSlotCount) return false;
    return writeFileAtomic(slotPath(slot), encodeSave(contents), true);
}

bool SaveSystem::readSlot(int slot, std::string& out) const {
    if (dir_.empty() || slot < 0 || slot >= kSlotCount) return false;
    std::string path = slotPath(slot), file;
    if (readFile(path, file) && decodeSave(file, out) && looksValid(out)) return true;
    if (readFile(path + ".bak", file) && decodeSave(file, out) && looksValid(out)) {
        SDL_Log("Slot %d was damaged; loaded the backup instead", slot + 1);
        return true;
    }
    return false;
}

std::string SaveSystem::cleanName(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        if (c >= 32 && c < 127) out += c; // the pixel font is plain ASCII
        if (static_cast<int>(out.size()) >= kMaxNameLength) break;
    }
    const size_t a = out.find_first_not_of(' '), b = out.find_last_not_of(' ');
    return a == std::string::npos ? std::string() : out.substr(a, b - a + 1);
}

bool SaveSystem::renameSlot(int slot, const std::string& rawName) const {
    std::string contents;
    if (!readSlot(slot, contents)) return false;
    const std::string name = cleanName(rawName);
    std::istringstream in(contents);
    std::ostringstream out;
    std::string line;
    bool first = true, done = false;
    while (std::getline(in, line)) {
        if (line.rfind("name ", 0) == 0 || line == "name") continue; // the old name
        out << line << "\n";
        if (first && !name.empty()) { // right after the header line, with the other summary fields
            out << "name " << name << "\n";
            done = true;
        }
        first = false;
    }
    (void)done;
    return writeSlot(slot, out.str());
}

SlotInfo SaveSystem::slotInfo(int slot) const {
    SlotInfo info;
    std::string contents;
    if (!readSlot(slot, contents)) return info;
    info.exists = true;
    std::istringstream in(contents);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key == "day") ss >> info.day;
        else if (key == "name") std::getline(ss >> std::ws, info.name);
        else if (key == "coins") ss >> info.coins;
        else if (key == "saved") {
            long long t = 0;
            ss >> t;
            info.savedAt = static_cast<SDL_Time>(t);
        } else if (key == "tech" || key == "farm") {
            break; // the summary fields all come first
        }
    }
    if (info.savedAt == 0) {
        SDL_PathInfo pi;
        if (SDL_GetPathInfo(slotPath(slot).c_str(), &pi)) info.savedAt = pi.modify_time;
    }
    return info;
}

bool SaveSystem::anySaves() const { return mostRecentSlot() >= 0; }

int SaveSystem::mostRecentSlot() const {
    int best = -1;
    SDL_Time bestTime = 0;
    for (int i = 0; i < kSlotCount; ++i) {
        SlotInfo s = slotInfo(i);
        if (s.exists && (best < 0 || s.savedAt > bestTime)) {
            best = i;
            bestTime = s.savedAt;
        }
    }
    return best;
}

Settings SaveSystem::loadSettings() const {
    Settings s;
    std::string contents;
    if (dir_.empty() || !readFile(dir_ + "settings.txt", contents)) return s;
    std::istringstream in(contents);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key == "music") ss >> s.musicVolume;
        else if (key == "sfx") ss >> s.sfxVolume;
        else if (key == "window") ss >> s.windowWidth >> s.windowHeight;
        else if (key == "fullscreen") ss >> s.fullscreen;
        else if (key == "lastSlot") ss >> s.lastSlot;
    }
    s.musicVolume = std::clamp(s.musicVolume, 0.f, 1.f);
    s.sfxVolume = std::clamp(s.sfxVolume, 0.f, 1.f);
    if (s.windowWidth < 640 || s.windowHeight < 360) {
        s.windowWidth = 1280;
        s.windowHeight = 720;
    }
    if (s.lastSlot < -1 || s.lastSlot >= kSlotCount) s.lastSlot = -1;
    return s;
}

void SaveSystem::saveSettings(const Settings& s) const {
    if (dir_.empty()) return;
    std::ostringstream out;
    out << "music " << s.musicVolume << "\n";
    out << "sfx " << s.sfxVolume << "\n";
    out << "window " << s.windowWidth << " " << s.windowHeight << "\n";
    out << "fullscreen " << (s.fullscreen ? 1 : 0) << "\n";
    out << "lastSlot " << s.lastSlot << "\n";
    writeFileAtomic(dir_ + "settings.txt", out.str(), false);
}

std::string SaveSystem::formatTime(SDL_Time t) {
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    SDL_DateTime dt;
    if (t == 0 || !SDL_TimeToDateTime(t, &dt, true)) return "";
    return draw::strf("%d %s %02d:%02d", dt.day, months[std::clamp(dt.month, 1, 12) - 1], dt.hour, dt.minute);
}
