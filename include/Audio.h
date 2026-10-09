// Audio.h - music and sound effects.
//
// Everything has a built-in version made in code (a small looping tune and
// synthesised effects), so the game always has sound. Put your own files in
// assets/audio/ to replace any of them (WAV, OGG or MP3, any sample rate,
// mono or stereo):
//
//   farm_1, farm_2, farm_3 ...  music while farming: as many as you like. The
//                               game picks one at random and changes it every
//                               couple of days (Game::kDaysPerFarmTrack)
//   menu         main menu, save slots and settings   (optional: else the farm music)
//   barn         The Barn (tech tree)                 (optional: else the farm music)
//   pick, coin, buy, deny, click, sunset              sound effects
//
// (The old names still work: music = a farm track, music_menu, music_barn.)
// Sound effects can have up to 9 takes, picked at random each time:
// pick.wav, pick2.wav, pick3.wav ... Music loops, and crossfades when the
// screen changes. F5 in game reloads the files.
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

enum class Sfx { Pick, Coin, Buy, Deny, Click, Sunset, Count };
enum class Music { Farm, Menu, Barn, Count };

class Audio {
public:
    ~Audio();
    bool init();
    void shutdown();

    // Loads assets/audio/ (pass that folder, ending in /). Files that aren't
    // there keep the built-in sound. Returns how many files were loaded.
    int loadFiles(const std::string& folder);
    const std::vector<std::string>& loadErrors() const { return errors_; }

    // pitch 1 = normal; 2 = an octave up.
    void play(Sfx sfx, float pitch = 1.f, float gain = 1.f);
    void setMusic(Music m); // crossfades if it's a different track
    void setMusicVolume(float v); // 0..1
    void setSfxVolume(float v);   // 0..1
    // Whether your own sound effects get the game's small random pitch changes (built-in ones always do).
    void setPitchVariation(bool on) { pitchVariation_ = on; }
    void pauseDevice(bool paused); // used when a phone app goes to the background

    // Writes the built-in sounds as WAV files (to paint over, as it were) into
    // <folder>/audio/, and adds them to <folder>/ART_LIST.md. Returns how many.
    int exportTemplates(const std::string& folder);

    static const char* sfxName(Sfx s);
    static const char* musicName(Music m);

    // Farm music: how many farm_N tracks were loaded (0 = the built-in tune), and
    // which one to play (0-based; crossfades if the farm music is playing).
    int farmTrackCount() const { return static_cast<int>(farmTracks_.size()); }
    void setFarmTrack(int index);
    int farmTrack() const { return farmChoice_; }
    std::string farmTrackName() const; // "farm_2", or "built-in" (for the debug screen)

private:
    static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional, int total);
    void mix(float* out, int frames); // stereo, interleaved
    float synthSample();
    void buildSfx();
    static std::vector<std::vector<std::vector<float>>> builtinSfx();
    // A music source: a loaded track, or -1 for the built-in tune.
    int sourceFor(Music m) const;
    void sourceFrame(int source, double& pos, float& l, float& r);

    struct Voice {
        int sfx, take;
        double pos; // in frames
        float rate;
        float gain;
    };
    struct Synth {
        float freq = 0.f;
        float phase = 0.f;
        float age = 10.f; // seconds since the note started
        bool on = false;
    };

    SDL_AudioStream* stream_ = nullptr;
    // All sound data is 44.1 kHz stereo, interleaved (L, R, L, R...).
    std::vector<std::vector<std::vector<float>>> sfx_; // [sfx][take]
    std::vector<bool> sfxFromFile_;
    std::vector<std::vector<float>> music_; // every loaded track (farm ones, menu, barn)
    std::vector<int> farmTracks_;            // indices into music_, in farm_1, farm_2 ... order
    std::vector<std::string> farmNames_;     // their file names
    int menuTrack_ = -1, barnTrack_ = -1;    // indices into music_, -1 = not provided
    int farmChoice_ = 0;                     // which farm track (into farmTracks_)
    std::vector<Voice> voices_;
    std::vector<float> scratch_;
    std::vector<std::string> errors_;
    float musicVolume_ = 0.6f;
    float sfxVolume_ = 0.8f;
    bool pitchVariation_ = true;
    Uint32 rng_ = 0x2545F491u;

    // Music playback (touched on the audio thread, under the stream lock).
    Music wanted_ = Music::Menu;
    int current_ = -1, previous_ = -1; // sources
    double currentPos_ = 0.0, previousPos_ = 0.0;
    float fade_ = 1.f; // 0..1 from previous_ to current_

    // Built-in tune's sequencer state.
    double musicClock_ = 0.0;
    int lastStep_ = -1;
    Synth lead_, bass_, pad_;
    float hatAge_ = 10.f;
    float leadFilter_ = 0.f;
    float leadLevel_ = 0.f;
    Uint32 noise_ = 0x12345678u;
};
