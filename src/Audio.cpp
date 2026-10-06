#include "Audio.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kRate = 44100;
constexpr float kTwoPi = 6.28318530718f;
constexpr size_t kMaxVoices = 24;

float midiToHz(int note) { return 440.f * std::pow(2.f, (note - 69) / 12.f); }

// ---------------------------------------------------------------------------
// The tune. One step = one eighth note. -1 = rest.
// Chords: C - G - Am - F, played twice with two different melodies.
// ---------------------------------------------------------------------------
constexpr double kTempoBpm = 104.0;
constexpr double kStepSeconds = 60.0 / kTempoBpm / 2.0;
constexpr int kStepsPerBar = 8;
constexpr int kBars = 8;
constexpr int kMelody[kBars * kStepsPerBar] = {
    // A section
    72, -1, 76, 79, 76, -1, 74, 72,
    71, 74, 79, -1, 74, -1, 71, -1,
    72, 76, 81, -1, 79, 76, -1, 74,
    72, -1, 69, 72, 77, -1, 76, 74,
    // B section
    79, -1, 76, -1, 72, 74, 76, -1,
    74, -1, 71, -1, 67, 71, 74, -1,
    76, -1, 72, 76, 81, -1, 79, -1,
    77, 76, 74, 72, -1, 72, -1, -1,
};
constexpr int kBassRoot[4] = {48, 43, 45, 41}; // C, G, A, F
constexpr int kPadNote[4] = {64, 62, 60, 65};  // a chord tone held under each bar
constexpr int kBassPattern[4] = {0, 12, 7, 12};

float triangle(float phase) { // phase 0..1
    return phase < 0.5f ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
}

void normalize(std::vector<float>& buf, float peak) {
    float m = 0.f;
    for (float v : buf) m = std::max(m, std::fabs(v));
    if (m > 0.f)
        for (float& v : buf) v *= peak / m;
}

std::vector<float> makeBuffer(float seconds) { return std::vector<float>(static_cast<size_t>(seconds * kRate), 0.f); }
} // namespace

Audio::~Audio() { shutdown(); }

bool Audio::init() {
    buildSfx();
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Audio::callback, this);
    if (!stream_) {
        SDL_Log("Could not open audio device: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void Audio::shutdown() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
}

void Audio::play(Sfx sfx, float pitch, float gain) {
    if (!stream_) return;
    SDL_LockAudioStream(stream_);
    if (voices_.size() >= kMaxVoices) voices_.erase(voices_.begin());
    voices_.push_back({static_cast<int>(sfx), 0.0, pitch, gain});
    SDL_UnlockAudioStream(stream_);
}

void Audio::setMusicVolume(float v) {
    if (stream_) SDL_LockAudioStream(stream_);
    musicVolume_ = std::clamp(v, 0.f, 1.f);
    if (stream_) SDL_UnlockAudioStream(stream_);
}

void Audio::setSfxVolume(float v) {
    if (stream_) SDL_LockAudioStream(stream_);
    sfxVolume_ = std::clamp(v, 0.f, 1.f);
    if (stream_) SDL_UnlockAudioStream(stream_);
}

void Audio::pauseDevice(bool paused) {
    if (!stream_) return;
    if (paused) SDL_PauseAudioStreamDevice(stream_);
    else SDL_ResumeAudioStreamDevice(stream_);
}

// Called by SDL on the audio thread, with the stream locked.
void SDLCALL Audio::callback(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/) {
    auto* self = static_cast<Audio*>(userdata);
    int frames = additional / static_cast<int>(sizeof(float));
    while (frames > 0) {
        int chunk = std::min(frames, 1024);
        self->scratch_.resize(chunk);
        self->mix(self->scratch_.data(), chunk);
        SDL_PutAudioStreamData(stream, self->scratch_.data(), chunk * static_cast<int>(sizeof(float)));
        frames -= chunk;
    }
}

void Audio::mix(float* out, int frames) {
    const float musicGain = 0.5f * musicVolume_ * musicVolume_; // squared feels more natural on a slider
    const float sfxGain = 0.85f * sfxVolume_ * sfxVolume_;
    for (int i = 0; i < frames; ++i) {
        float s = musicSample() * musicGain; // always advance the tune, even when muted
        for (auto& v : voices_) {
            const auto& buf = sfx_[v.sfx];
            size_t idx = static_cast<size_t>(v.pos);
            if (idx + 1 >= buf.size()) continue;
            float frac = static_cast<float>(v.pos - idx);
            s += (buf[idx] + (buf[idx + 1] - buf[idx]) * frac) * v.gain * sfxGain;
            v.pos += v.rate;
        }
        out[i] = std::clamp(s, -1.f, 1.f);
    }
    std::erase_if(voices_, [this](const Voice& v) { return static_cast<size_t>(v.pos) + 1 >= sfx_[v.sfx].size(); });
}

float Audio::musicSample() {
    const float dt = 1.f / kRate;
    int step = static_cast<int>(musicClock_ / kStepSeconds) % (kBars * kStepsPerBar);
    musicClock_ += dt;
    if (musicClock_ > kStepSeconds * kBars * kStepsPerBar) musicClock_ -= kStepSeconds * kBars * kStepsPerBar;

    if (step != lastStep_) {
        lastStep_ = step;
        int bar = (step / kStepsPerBar) % 4;
        int note = kMelody[step];
        if (note >= 0) {
            lead_.freq = midiToHz(note);
            lead_.age = 0.f;
            lead_.on = true;
        } else {
            lead_.on = false;
        }
        if (step % 2 == 0) {
            int beat = (step % kStepsPerBar) / 2;
            bass_.freq = midiToHz(kBassRoot[bar] + kBassPattern[beat]);
            bass_.age = 0.f;
            bass_.on = true;
        } else {
            hatAge_ = 0.f;
        }
        if (step % kStepsPerBar == 0) {
            pad_.freq = midiToHz(kPadNote[bar]);
            pad_.on = true;
        }
    }

    // Lead: triangle with a touch of square, softened by a one-pole filter.
    lead_.phase = std::fmod(lead_.phase + lead_.freq * dt, 1.f);
    lead_.age += dt;
    float env = std::min(1.f, lead_.age / 0.008f) * (0.35f + 0.65f * std::exp(-lead_.age * 5.f));
    // Smooth the volume toward its target so rests fade out instead of clicking.
    leadLevel_ += ((lead_.on ? env : 0.f) - leadLevel_) * 0.004f;
    float raw = 0.75f * triangle(lead_.phase) + 0.25f * (lead_.phase < 0.5f ? 1.f : -1.f);
    leadFilter_ += (raw * leadLevel_ - leadFilter_) * 0.25f;
    float lead = leadFilter_ * 0.55f;

    // Bass: plucked triangle.
    bass_.phase = std::fmod(bass_.phase + bass_.freq * dt, 1.f);
    bass_.age += dt;
    float bass = triangle(bass_.phase) * std::exp(-bass_.age * 3.f) * 0.6f;

    // Pad: quiet sine holding a chord tone.
    pad_.phase = std::fmod(pad_.phase + pad_.freq * dt, 1.f);
    float pad = std::sin(pad_.phase * kTwoPi) * 0.12f;

    // Hi-hat: a short burst of noise on the off-beats.
    noise_ = noise_ * 1664525u + 1013904223u;
    float white = static_cast<float>(noise_ >> 8) / static_cast<float>(1u << 24) * 2.f - 1.f;
    hatAge_ += dt;
    float hat = white * std::exp(-hatAge_ * 70.f) * 0.12f;

    return lead + bass + pad + hat;
}

void Audio::buildSfx() {
    sfx_.assign(static_cast<size_t>(Sfx::Count), {});

    { // Pick: a quick upward "pop".
        auto b = makeBuffer(0.10f);
        float phase = 0.f;
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / kRate;
            float f = 520.f * std::pow(2.1f, t / 0.10f);
            phase += f / kRate;
            float env = std::min(1.f, t / 0.003f) * std::exp(-t * 28.f);
            b[i] = (std::sin(phase * kTwoPi) + 0.3f * std::sin(phase * 2.f * kTwoPi)) * env;
        }
        normalize(b, 0.55f);
        sfx_[static_cast<int>(Sfx::Pick)] = std::move(b);
    }
    { // Coin: two bright tones.
        auto b = makeBuffer(0.40f);
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / kRate;
            float f = t < 0.065f ? 1568.f : 2093.f;
            float tt = t < 0.065f ? t : t - 0.065f;
            float env = std::min(1.f, tt / 0.002f) * std::exp(-tt * (t < 0.065f ? 20.f : 9.f));
            float x = f * t * kTwoPi;
            b[i] = (std::sin(x) + 0.25f * std::sin(3.f * x)) * env;
        }
        normalize(b, 0.35f);
        sfx_[static_cast<int>(Sfx::Coin)] = std::move(b);
    }
    { // Buy: a happy rising arpeggio.
        const float notes[4] = {523.25f, 659.25f, 783.99f, 1046.5f};
        auto b = makeBuffer(0.55f);
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / kRate;
            int n = std::min(3, static_cast<int>(t / 0.07f));
            float tt = t - n * 0.07f;
            float env = std::min(1.f, tt / 0.004f) * std::exp(-tt * (n == 3 ? 6.f : 16.f));
            float ph = std::fmod(notes[n] * t, 1.f);
            b[i] = triangle(ph) * env;
        }
        normalize(b, 0.6f);
        sfx_[static_cast<int>(Sfx::Buy)] = std::move(b);
    }
    { // Deny: two low soft buzzes.
        auto b = makeBuffer(0.22f);
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / kRate;
            float tt = std::fmod(t, 0.11f);
            float env = std::min(1.f, tt / 0.004f) * std::exp(-tt * 25.f);
            float ph = std::fmod(170.f * t, 1.f);
            b[i] = (0.6f * (ph < 0.5f ? 1.f : -1.f) + 0.4f * triangle(ph)) * env;
        }
        normalize(b, 0.4f);
        sfx_[static_cast<int>(Sfx::Deny)] = std::move(b);
    }
    { // Click: a tiny tick for buttons.
        auto b = makeBuffer(0.05f);
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / kRate;
            b[i] = std::sin(1500.f * t * kTwoPi) * std::exp(-t * 90.f);
        }
        normalize(b, 0.45f);
        sfx_[static_cast<int>(Sfx::Click)] = std::move(b);
    }
    { // Sunset: two bell strikes.
        auto b = makeBuffer(2.0f);
        auto bell = [&](float start, float f, float amp) {
            for (size_t i = static_cast<size_t>(start * kRate); i < b.size(); ++i) {
                float t = static_cast<float>(i) / kRate - start;
                float env = std::min(1.f, t / 0.003f);
                b[i] += amp * env *
                        (std::sin(f * t * kTwoPi) * std::exp(-t * 2.5f) +
                         0.4f * std::sin(2.76f * f * t * kTwoPi) * std::exp(-t * 5.f) +
                         0.2f * std::sin(5.4f * f * t * kTwoPi) * std::exp(-t * 9.f));
            }
        };
        bell(0.0f, 783.99f, 1.f);
        bell(0.35f, 587.33f, 0.9f);
        normalize(b, 0.5f);
        sfx_[static_cast<int>(Sfx::Sunset)] = std::move(b);
    }
}
