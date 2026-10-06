#include "SaveSystem.h"

#include "Draw.h"

#include <algorithm>
#include <sstream>

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
        if (writeFileAtomic(slotPath(0), contents, false)) {
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
    return true;
}

bool SaveSystem::looksValid(const std::string& contents) {
    return contents.find("coins ") != std::string::npos && contents.find("day ") != std::string::npos;
}

bool SaveSystem::writeSlot(int slot, const std::string& contents) const {
    if (dir_.empty() || slot < 0 || slot >= kSlotCount) return false;
    return writeFileAtomic(slotPath(slot), contents, true);
}

bool SaveSystem::readSlot(int slot, std::string& out) const {
    if (dir_.empty() || slot < 0 || slot >= kSlotCount) return false;
    std::string path = slotPath(slot);
    if (readFile(path, out) && looksValid(out)) return true;
    if (readFile(path + ".bak", out) && looksValid(out)) {
        SDL_Log("Slot %d was damaged; loaded the backup instead", slot + 1);
        return true;
    }
    return false;
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
