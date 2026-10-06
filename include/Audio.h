// Audio.h - music and sound effects, all synthesised in code (no audio files).
//
// The music is a small looping tune generated note by note; the sound effects
// are rendered into buffers once at start-up. Swapping in real recordings
// later only means filling the sfx buffers from files (e.g. SDL_LoadWAV)
// instead of generating them.
#pragma once

#include <SDL3/SDL.h>
#include <vector>

enum class Sfx { Pick, Coin, Buy, Deny, Click, Sunset, Count };

class Audio {
public:
    ~Audio();
    bool init();
    void shutdown();

    // pitch 1 = normal; 2 = an octave up.
    void play(Sfx sfx, float pitch = 1.f, float gain = 1.f);
    void setMusicVolume(float v); // 0..1
    void setSfxVolume(float v);   // 0..1
    void pauseDevice(bool paused); // used when a phone app goes to the background

private:
    static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional, int total);
    void mix(float* out, int frames);
    float musicSample();
    void buildSfx();

    struct Voice {
        int sfx;
        double pos;
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
    std::vector<std::vector<float>> sfx_;
    std::vector<Voice> voices_;
    std::vector<float> scratch_;
    float musicVolume_ = 0.6f;
    float sfxVolume_ = 0.8f;

    // Music sequencer state (only touched on the audio thread).
    double musicClock_ = 0.0;
    int lastStep_ = -1;
    Synth lead_, bass_, pad_;
    float hatAge_ = 10.f;
    float leadFilter_ = 0.f;
    float leadLevel_ = 0.f;
    Uint32 noise_ = 0x12345678u;
};
