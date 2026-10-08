// SaveSystem.h - save slots and the settings file.
//
// Everything lives in SDL's per-user preference folder (see README):
//   slot1.sav, slot2.sav, slot3.sav   game saves (scrambled, with a tamper check: see encodeSave)
//   slotN.sav.bak                      the previous good save, used if the main one is damaged
//   settings.txt                       volumes, resolution, last played slot
//
// Saves are written to a temporary file first and then renamed over the old
// one, so a crash or the phone killing the app mid-save can't corrupt them.
#pragma once

#include <SDL3/SDL.h>
#include <string>

struct Settings {
    float musicVolume = 0.6f; // 0..1
    float sfxVolume = 0.8f;   // 0..1
    int windowWidth = 1280;   // desktop only
    int windowHeight = 720;   // desktop only
    bool fullscreen = false;  // desktop only
    int lastSlot = -1;        // slot used by "Continue"; -1 = none yet
};

struct SlotInfo {
    bool exists = false;
    int day = 0;
    double coins = 0.0;
    SDL_Time savedAt = 0; // nanoseconds since 1970
};

class SaveSystem {
public:
    static constexpr int kSlotCount = 3;

    bool init(); // finds the save folder and upgrades a v0.1 save.txt into slot 1

    bool writeSlot(int slot, const std::string& contents) const;
    bool readSlot(int slot, std::string& out) const; // falls back to the .bak file
    SlotInfo slotInfo(int slot) const;
    bool anySaves() const;
    int mostRecentSlot() const; // -1 if there are no saves

    Settings loadSettings() const;
    void saveSettings(const Settings& s) const;

    // "6 Oct 14:32" in local time.
    static std::string formatTime(SDL_Time t);

private:
    std::string slotPath(int slot) const;
    static bool readFile(const std::string& path, std::string& out);
    static bool writeFileAtomic(const std::string& path, const std::string& contents, bool keepBackup);
    static bool looksValid(const std::string& contents);

public:
    // Saves are stored scrambled, with a check value, so they can't simply be
    // opened in a text editor and changed: an edited file fails the check and
    // is treated as damaged (the .bak backup is used instead, if it's good).
    // Like the release build's asset packing, this keeps honest players honest;
    // it isn't encryption. Development builds still read old plain-text saves
    // (and re-save them scrambled); release builds don't.
    static std::string encodeSave(const std::string& text);
    static bool decodeSave(const std::string& file, std::string& text);

    std::string dir_;
};
