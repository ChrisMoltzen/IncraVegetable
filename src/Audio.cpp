#include "Audio.h"

#include "AssetPack.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

// Decoders for OGG and MP3 (single-file libraries, public domain / MIT-0).
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"
#undef L
#undef C
#undef R
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace {
constexpr int kRate = 44100;
constexpr float kTwoPi = 6.28318530718f;
constexpr size_t kMaxVoices = 24;
constexpr float kCrossfadeSeconds = 1.5f;

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

std::vector<float> monoToStereo(const std::vector<float>& m) {
    std::vector<float> s(m.size() * 2);
    for (size_t i = 0; i < m.size(); ++i) s[2 * i] = s[2 * i + 1] = m[i];
    return s;
}

// Any sample format, rate and channel count -> 44.1 kHz float stereo.
bool convertToGame(const SDL_AudioSpec& from, const void* data, int bytes, std::vector<float>& out) {
    SDL_AudioSpec to{SDL_AUDIO_F32, 2, kRate};
    Uint8* conv = nullptr;
    int len = 0;
    if (!SDL_ConvertAudioSamples(&from, static_cast<const Uint8*>(data), bytes, &to, &conv, &len)) return false;
    out.assign(reinterpret_cast<float*>(conv), reinterpret_cast<float*>(conv) + len / sizeof(float));
    SDL_free(conv);
    return !out.empty();
}

bool endsWith(const std::string& s, const char* ext) {
    size_t n = std::char_traits<char>::length(ext);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (std::tolower(static_cast<unsigned char>(s[s.size() - n + i])) != ext[i]) return false;
    return true;
}

// Reads a WAV, OGG or MP3 file into 44.1 kHz stereo. Returns false (with a reason) if it can't.
// (From disk, or from the assets compiled into a release build.)
bool decodeFile(const std::string& path, std::vector<float>& out, std::string& why) {
    size_t fileSize = 0;
    void* file = assetpack::load(path, &fileSize);
    if (!file) {
        why = "can't read the file";
        return false;
    }
    struct Free {
        void* p;
        ~Free() { SDL_free(p); }
    } freeFile{file};
    if (endsWith(path, ".wav")) {
        SDL_AudioSpec spec;
        Uint8* buf = nullptr;
        Uint32 len = 0;
        if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(file, fileSize), true, &spec, &buf, &len)) {
            why = SDL_GetError();
            return false;
        }
        bool ok = convertToGame(spec, buf, static_cast<int>(len), out);
        SDL_free(buf);
        if (!ok) why = SDL_GetError();
        return ok;
    }
    if (endsWith(path, ".ogg")) {
        int channels = 0, rate = 0;
        short* pcm = nullptr;
        int frames = stb_vorbis_decode_memory(static_cast<const unsigned char*>(file), static_cast<int>(fileSize),
                                              &channels, &rate, &pcm);
        if (frames <= 0 || !pcm) {
            why = "not a readable OGG Vorbis file";
            return false;
        }
        SDL_AudioSpec spec{SDL_AUDIO_S16, channels, rate};
        bool ok = convertToGame(spec, pcm, frames * channels * static_cast<int>(sizeof(short)), out);
        free(pcm);
        if (!ok) why = SDL_GetError();
        return ok;
    }
    if (endsWith(path, ".mp3")) {
        drmp3_config cfg{};
        drmp3_uint64 frames = 0;
        float* pcm = drmp3_open_memory_and_read_pcm_frames_f32(file, fileSize, &cfg, &frames, nullptr);
        if (!pcm || frames == 0) {
            why = "not a readable MP3 file";
            return false;
        }
        SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(cfg.channels), static_cast<int>(cfg.sampleRate)};
        bool ok = convertToGame(spec, pcm, static_cast<int>(frames * cfg.channels * sizeof(float)), out);
        drmp3_free(pcm, nullptr);
        if (!ok) why = SDL_GetError();
        return ok;
    }
    why = "unknown file type";
    return false;
}

// assets/audio/<name>.wav / .ogg / .mp3 - the first that exists, or "".
std::string findSound(const std::string& folder, const std::string& name) {
    for (const char* ext : {".wav", ".ogg", ".mp3"}) {
        std::string p = folder + name + ext;
        if (assetpack::exists(p)) return p;
    }
    return "";
}

// A 16-bit mono WAV file (the built-in sounds are mono, so the templates are too).
bool writeWav(const std::string& path, const std::vector<float>& stereo) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    auto u32 = [&](Uint32 v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](Uint16 v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    const size_t frames = stereo.size() / 2;
    const Uint32 dataBytes = static_cast<Uint32>(frames * 2);
    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
    f.write("data", 4); u32(dataBytes);
    for (size_t i = 0; i < frames; ++i) {
        float v = 0.5f * (stereo[2 * i] + stereo[2 * i + 1]);
        Sint16 s = static_cast<Sint16>(std::lround(std::clamp(v, -1.f, 1.f) * 32767.f));
        f.write(reinterpret_cast<const char*>(&s), 2);
    }
    return static_cast<bool>(f);
}
} // namespace

const char* Audio::sfxName(Sfx s) {
    switch (s) {
    case Sfx::Pick: return "pick";
    case Sfx::Coin: return "coin";
    case Sfx::Buy: return "buy";
    case Sfx::Deny: return "deny";
    case Sfx::Click: return "click";
    case Sfx::Sunset: return "sunset";
    case Sfx::Count: break;
    }
    return "";
}

const char* Audio::musicName(Music m) {
    switch (m) {
    case Music::Farm: return "farm_1";
    case Music::Menu: return "menu";
    case Music::Barn: return "barn";
    case Music::Count: break;
    }
    return "";
}

Audio::~Audio() { shutdown(); }

bool Audio::init() {
    buildSfx();
    sfxFromFile_.assign(sfx_.size(), false);
    music_.clear();
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kRate};
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

int Audio::loadFiles(const std::string& folder) {
    errors_.clear();
    // Decode everything first (slow), then swap it in under the lock (quick).
    std::vector<std::vector<std::vector<float>>> sfx = builtinSfx();
    std::vector<bool> fromFile(sfx.size(), false);
    std::vector<std::vector<float>> music;
    std::vector<int> farmTracks;
    std::vector<std::string> farmNames;
    int menuTrack = -1, barnTrack = -1;
    int loaded = 0;
    auto load = [&](const std::string& name, std::vector<float>& out) {
        std::string path = findSound(folder, name);
        if (path.empty()) return false;
        std::string why;
        if (decodeFile(path, out, why)) {
            ++loaded;
            return true;
        }
        errors_.push_back(path + ": " + why);
        SDL_Log("Audio: couldn't load %s (%s) - using the built-in sound", path.c_str(), why.c_str());
        return false;
    };
    if (!folder.empty()) {
        for (int i = 0; i < static_cast<int>(Sfx::Count); ++i) {
            std::vector<std::vector<float>> takes;
            for (int k = 1; k <= 9; ++k) {
                std::vector<float> buf;
                std::string name = std::string(sfxName(static_cast<Sfx>(i))) + (k == 1 ? "" : std::to_string(k));
                if (load(name, buf)) takes.push_back(std::move(buf));
            }
            if (!takes.empty()) {
                sfx[i] = std::move(takes);
                fromFile[i] = true;
            }
        }
        // Music: farm_1, farm_2 ... (gaps are fine), then menu and barn. The old single-track
        // names (music, music_menu, music_barn) still work when the new ones aren't there.
        auto loadTrack = [&](const std::string& name) {
            std::vector<float> buf;
            if (!load(name, buf)) return -1;
            music.push_back(std::move(buf));
            return static_cast<int>(music.size()) - 1;
        };
        for (int k = 1; k <= 99; ++k) {
            const std::string name = "farm_" + std::to_string(k);
            if (findSound(folder, name).empty()) continue;
            int t = loadTrack(name);
            if (t >= 0) { farmTracks.push_back(t); farmNames.push_back(name); }
        }
        if (farmTracks.empty()) {
            int t = loadTrack("music");
            if (t >= 0) { farmTracks.push_back(t); farmNames.push_back("music"); }
        }
        menuTrack = loadTrack("menu");
        if (menuTrack < 0) menuTrack = loadTrack("music_menu");
        barnTrack = loadTrack("barn");
        if (barnTrack < 0) barnTrack = loadTrack("music_barn");
    }
    if (stream_) SDL_LockAudioStream(stream_);
    sfx_ = std::move(sfx);
    sfxFromFile_ = std::move(fromFile);
    music_ = std::move(music);
    farmTracks_ = std::move(farmTracks);
    farmNames_ = std::move(farmNames);
    menuTrack_ = menuTrack;
    barnTrack_ = barnTrack;
    if (farmChoice_ >= static_cast<int>(farmTracks_.size())) farmChoice_ = 0;
    voices_.clear();
    current_ = previous_ = sourceFor(wanted_);
    currentPos_ = previousPos_ = 0.0;
    fade_ = 1.f;
    if (stream_) SDL_UnlockAudioStream(stream_);
    if (loaded) SDL_Log("Audio: %d sound file(s) loaded from %s", loaded, folder.c_str());
    return loaded;
}

void Audio::play(Sfx sfx, float pitch, float gain) {
    if (!stream_) return;
    SDL_LockAudioStream(stream_);
    const int i = static_cast<int>(sfx);
    if (i >= 0 && i < static_cast<int>(sfx_.size()) && !sfx_[i].empty()) {
        if (voices_.size() >= kMaxVoices) voices_.erase(voices_.begin());
        rng_ = rng_ * 1664525u + 1013904223u;
        const int take = static_cast<int>((rng_ >> 8) % sfx_[i].size());
        if (!pitchVariation_ && i < static_cast<int>(sfxFromFile_.size()) && sfxFromFile_[i]) pitch = 1.f;
        voices_.push_back({i, take, 0.0, pitch, gain});
    }
    SDL_UnlockAudioStream(stream_);
}

int Audio::sourceFor(Music m) const {
    if (m == Music::Menu && menuTrack_ >= 0) return menuTrack_;
    if (m == Music::Barn && barnTrack_ >= 0) return barnTrack_;
    // The farm, and anywhere without its own track: today's farm track.
    if (!farmTracks_.empty()) return farmTracks_[std::clamp(farmChoice_, 0, static_cast<int>(farmTracks_.size()) - 1)];
    return -1; // the built-in tune
}

void Audio::setFarmTrack(int index) {
    if (stream_) SDL_LockAudioStream(stream_);
    if (!farmTracks_.empty()) farmChoice_ = std::clamp(index, 0, static_cast<int>(farmTracks_.size()) - 1);
    // If the farm track is what's playing, crossfade to the new one.
    int src = sourceFor(wanted_);
    if (src != current_) {
        previous_ = current_;
        previousPos_ = currentPos_;
        current_ = src;
        currentPos_ = 0.0;
        fade_ = 0.f;
    }
    if (stream_) SDL_UnlockAudioStream(stream_);
}

std::string Audio::farmTrackName() const {
    if (farmNames_.empty()) return "built-in";
    return farmNames_[std::clamp(farmChoice_, 0, static_cast<int>(farmNames_.size()) - 1)];
}

void Audio::setMusic(Music m) {
    if (stream_) SDL_LockAudioStream(stream_);
    wanted_ = m;
    int src = sourceFor(m);
    if (src != current_) {
        previous_ = current_;
        previousPos_ = currentPos_;
        current_ = src;
        currentPos_ = 0.0; // a new track starts from the top
        fade_ = 0.f;
    }
    if (stream_) SDL_UnlockAudioStream(stream_);
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
    int frames = additional / static_cast<int>(2 * sizeof(float));
    while (frames > 0) {
        int chunk = std::min(frames, 1024);
        self->scratch_.resize(2 * chunk);
        self->mix(self->scratch_.data(), chunk);
        SDL_PutAudioStreamData(stream, self->scratch_.data(), chunk * static_cast<int>(2 * sizeof(float)));
        frames -= chunk;
    }
}

void Audio::sourceFrame(int source, double& pos, float& l, float& r) {
    if (source < 0) {
        l = r = 0.f; // the built-in tune is added separately (it always runs)
        return;
    }
    const auto& buf = music_[source];
    const size_t frames = buf.size() / 2;
    size_t i = static_cast<size_t>(pos);
    if (i >= frames) i = 0, pos = 0.0;
    l = buf[2 * i];
    r = buf[2 * i + 1];
    pos += 1.0;
    if (pos >= static_cast<double>(frames)) pos -= static_cast<double>(frames); // loop
}

void Audio::mix(float* out, int frames) {
    const float musicGain = 0.5f * musicVolume_ * musicVolume_; // squared feels more natural on a slider
    const float sfxGain = 0.85f * sfxVolume_ * sfxVolume_;
    const float fadeStep = 1.f / (kCrossfadeSeconds * kRate);
    for (int i = 0; i < frames; ++i) {
        // Music: the current track, crossfading from the previous one.
        float synth = synthSample(); // always advance the built-in tune, even when it isn't heard
        float cl, cr, pl = 0.f, pr = 0.f;
        sourceFrame(current_, currentPos_, cl, cr);
        if (current_ < 0) cl = cr = synth;
        if (fade_ < 1.f) {
            sourceFrame(previous_, previousPos_, pl, pr);
            if (previous_ < 0) pl = pr = synth;
            fade_ = std::min(1.f, fade_ + fadeStep);
        }
        float l = (cl * fade_ + pl * (1.f - fade_)) * musicGain;
        float r = (cr * fade_ + pr * (1.f - fade_)) * musicGain;
        // Sound effects.
        for (auto& v : voices_) {
            const auto& buf = sfx_[v.sfx][v.take];
            size_t idx = static_cast<size_t>(v.pos);
            if (2 * (idx + 1) + 1 >= buf.size()) continue;
            float frac = static_cast<float>(v.pos - idx), g = v.gain * sfxGain;
            l += (buf[2 * idx] + (buf[2 * idx + 2] - buf[2 * idx]) * frac) * g;
            r += (buf[2 * idx + 1] + (buf[2 * idx + 3] - buf[2 * idx + 1]) * frac) * g;
            v.pos += v.rate;
        }
        out[2 * i] = std::clamp(l, -1.f, 1.f);
        out[2 * i + 1] = std::clamp(r, -1.f, 1.f);
    }
    std::erase_if(voices_, [this](const Voice& v) {
        return 2 * (static_cast<size_t>(v.pos) + 1) + 1 >= sfx_[v.sfx][v.take].size();
    });
}

float Audio::synthSample() {
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

void Audio::buildSfx() { sfx_ = builtinSfx(); }

// The built-in sound effects, one take each.
std::vector<std::vector<std::vector<float>>> Audio::builtinSfx() {
    std::vector<std::vector<std::vector<float>>> out(static_cast<size_t>(Sfx::Count));
    auto set = [&out](Sfx s, std::vector<float>&& mono) { out[static_cast<int>(s)] = {monoToStereo(mono)}; };

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
        set(Sfx::Pick, std::move(b));
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
        set(Sfx::Coin, std::move(b));
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
        set(Sfx::Buy, std::move(b));
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
        set(Sfx::Deny, std::move(b));
    }
    { // Click: a tiny tick for buttons.
        auto b = makeBuffer(0.05f);
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / kRate;
            b[i] = std::sin(1500.f * t * kTwoPi) * std::exp(-t * 90.f);
        }
        normalize(b, 0.45f);
        set(Sfx::Click, std::move(b));
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
        set(Sfx::Sunset, std::move(b));
    }
    return out;
}

int Audio::exportTemplates(const std::string& folderIn) {
    std::string folder = folderIn;
    if (!folder.empty() && folder.back() != '/' && folder.back() != '\\') folder += '/';
    const std::string dir = folder + "audio/";
    SDL_CreateDirectory(dir.c_str());
    const auto builtin = builtinSfx();
    int written = 0;
    for (int i = 0; i < static_cast<int>(Sfx::Count); ++i)
        if (writeWav(dir + sfxName(static_cast<Sfx>(i)) + ".wav", builtin[i][0])) ++written;
    // One loop of the built-in tune (the audio thread is paused while we borrow its synth).
    {
        if (stream_) SDL_LockAudioStream(stream_);
        const int frames = static_cast<int>(kStepSeconds * kBars * kStepsPerBar * kRate);
        double clock = musicClock_;
        int step = lastStep_;
        musicClock_ = 0.0;
        lastStep_ = -1;
        std::vector<float> mono(frames);
        for (int i = 0; i < frames; ++i) mono[i] = synthSample() * 0.5f;
        musicClock_ = clock;
        lastStep_ = step;
        if (stream_) SDL_UnlockAudioStream(stream_);
        if (writeWav(dir + "farm_1.wav", monoToStereo(mono))) ++written;
    }
    // Add them to the art list.
    std::ofstream list(folder + "ART_LIST.md", std::ios::app);
    list << "\n## Sounds\n\n"
         << "Put your sounds in `assets/audio/` using these names. WAV, OGG or MP3, any sample rate, mono or stereo.\n"
         << "Anything you leave out uses the built-in sound. The built-in ones are in `audio/` here to listen to or replace.\n"
         << "Sound effects can have up to 9 takes, picked at random: `pick.wav`, `pick2.wav`, `pick3.wav`...\n"
         << "Music loops; when the screen changes it crossfades to that screen's track. F5 in game reloads your sounds.\n\n"
         << "| File | What it is |\n|---|---|\n"
         << "| `audio/farm_1`, `audio/farm_2`, ... | Music on the farm: as many tracks as you like. One is picked at "
            "random and changed every couple of days (loops) |\n"
         << "| `audio/menu` | Main menu, save slots and settings (optional: else the farm music) |\n"
         << "| `audio/barn` | The Barn (optional: else the farm music) |\n"
         << "| `audio/pick` | Picking a vegetable (played slightly higher or lower each time) |\n"
         << "| `audio/coin` | Coins for a pick (quietly, under `pick`), and the sound-volume preview |\n"
         << "| `audio/buy` | Buying an upgrade in The Barn |\n"
         << "| `audio/deny` | Clicking an upgrade you can't buy yet |\n"
         << "| `audio/click` | Buttons |\n"
         << "| `audio/sunset` | The day ending |\n\n"
         << "Your own effects get the same small random pitch changes as the built-in ones. To turn that off,\n"
         << "put `sound_pitch_variation off` in `assets/art.txt`.\n";
    return written;
}
