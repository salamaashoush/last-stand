#include "managers/sound_manager.hpp"
#include "core/rng.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ls {

namespace {

constexpr int RATE = 44100;
constexpr float TAU = 6.28318530718f;

std::vector<float> buffer(float seconds) {
    return std::vector<float>(static_cast<size_t>(RATE * seconds), 0.0f);
}

float sq(float phase) {
    return std::fmod(phase, 1.0f) < 0.5f ? 1.0f : -1.0f;
}
float tri(float phase) {
    float t = std::fmod(phase, 1.0f);
    return t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
}

// Exponential pitch sweep with an exponential amplitude decay; the workhorse for blips and booms.
void add_sweep(std::vector<float>& b, float f0, float f1, float decay, float amp, int wave, float start = 0.0f) {
    float phase = 0.0f;
    auto n = b.size();
    auto s0 = static_cast<size_t>(start * RATE);
    for (size_t i = s0; i < n; ++i) {
        float t = static_cast<float>(i - s0) / RATE;
        float k = static_cast<float>(i - s0) / static_cast<float>(n - s0);
        float f = f0 * std::pow(f1 / f0, k);
        phase += f / RATE;
        float v = wave == 0 ? std::sin(TAU * phase) : wave == 1 ? sq(phase) : tri(phase);
        b[i] += v * amp * std::exp(-t * decay);
    }
}

void add_noise(std::vector<float>& b, float decay, float amp, float lowpass, Rng& rng, float start = 0.0f) {
    float y = 0.0f;
    auto s0 = static_cast<size_t>(start * RATE);
    for (size_t i = s0; i < b.size(); ++i) {
        float t = static_cast<float>(i - s0) / RATE;
        float x = rng.uniform(-1.0f, 1.0f);
        y += lowpass * (x - y);
        b[i] += y * amp * std::exp(-t * decay);
    }
}

void add_notes(std::vector<float>& b, std::initializer_list<float> freqs, float step, float decay, float amp) {
    float start = 0.0f;
    for (float f : freqs) {
        auto s0 = static_cast<size_t>(start * RATE);
        for (size_t i = s0; i < b.size(); ++i) {
            float t = static_cast<float>(i - s0) / RATE;
            float env = std::exp(-t * decay) * std::min(1.0f, t * 200.0f);
            b[i] += (std::sin(TAU * f * t) * 0.7f + tri(f * 2.0f * t) * 0.3f) * amp * env;
        }
        start += step;
    }
}

void fade_tail(std::vector<float>& b) {
    size_t tail = std::min<size_t>(b.size(), 400);
    for (size_t i = 0; i < tail; ++i) b[b.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(tail);
}

} // namespace

void SoundManager::set(Sfx s, const std::vector<float>& samples, int max_per_frame) {
    std::vector<float> data = samples;
    fade_tail(data);
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(data.size());
    wave.sampleRate = RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = std::malloc(data.size() * sizeof(short));
    auto* out = static_cast<short*>(wave.data);
    for (size_t i = 0; i < data.size(); ++i) out[i] = static_cast<short>(std::clamp(data[i], -1.0f, 1.0f) * 30000.0f);
    auto& slot = slots_[static_cast<size_t>(s)];
    slot.base = LoadSoundFromWave(wave);
    UnloadWave(wave);
    for (auto& v : slot.voices) v = LoadSoundAlias(slot.base);
    slot.max_per_frame = max_per_frame;
    slot.loaded = true;
}

void SoundManager::init() {
    Rng rng{1234};
    {
        auto b = buffer(0.09f);
        add_noise(b, 45.0f, 0.35f, 0.5f, rng);
        add_sweep(b, 900, 180, 40.0f, 0.25f, 1);
        set(Sfx::Shot, b, 2);
    }
    {
        auto b = buffer(0.22f);
        add_noise(b, 18.0f, 0.5f, 0.25f, rng);
        add_sweep(b, 220, 50, 14.0f, 0.5f, 0);
        set(Sfx::ShotHeavy, b, 2);
    }
    {
        auto b = buffer(0.05f);
        add_noise(b, 70.0f, 0.3f, 0.7f, rng);
        set(Sfx::Hit, b, 3);
    }
    {
        auto b = buffer(0.16f);
        add_noise(b, 25.0f, 0.45f, 0.12f, rng);
        add_sweep(b, 300, 80, 20.0f, 0.25f, 0);
        set(Sfx::Splat, b, 3);
    }
    {
        auto b = buffer(0.55f);
        add_noise(b, 7.0f, 0.7f, 0.08f, rng);
        add_sweep(b, 140, 35, 6.0f, 0.6f, 0);
        set(Sfx::Explosion, b, 2);
    }
    {
        auto b = buffer(1.3f);
        add_noise(b, 3.2f, 0.8f, 0.05f, rng);
        add_sweep(b, 90, 25, 2.5f, 0.8f, 0);
        add_noise(b, 9.0f, 0.4f, 0.4f, rng);
        set(Sfx::BigExplosion, b, 1);
    }
    {
        auto b = buffer(0.5f);
        add_notes(b, {523.25f, 659.25f, 783.99f, 1046.5f}, 0.07f, 7.0f, 0.3f);
        set(Sfx::GateGood, b, 1);
    }
    {
        auto b = buffer(0.5f);
        add_notes(b, {392.0f, 311.13f, 233.08f}, 0.09f, 6.0f, 0.32f);
        add_sweep(b, 300, 90, 5.0f, 0.15f, 1);
        set(Sfx::GateBad, b, 1);
    }
    {
        auto b = buffer(0.05f);
        add_sweep(b, 1500, 1800, 50.0f, 0.18f, 2);
        set(Sfx::GateTick, b, 1);
    }
    {
        auto b = buffer(0.9f);
        add_notes(b, {523.25f, 659.25f, 783.99f, 1046.5f, 1318.5f}, 0.06f, 4.0f, 0.24f);
        set(Sfx::LevelUp, b, 1);
    }
    {
        auto b = buffer(0.35f);
        add_sweep(b, 400, 1600, 8.0f, 0.3f, 2);
        add_noise(b, 20.0f, 0.12f, 0.6f, rng);
        set(Sfx::CardPick, b, 1);
    }
    {
        auto b = buffer(0.12f);
        add_notes(b, {1567.98f, 2093.0f}, 0.04f, 25.0f, 0.18f);
        set(Sfx::Coin, b, 2);
    }
    {
        auto b = buffer(0.25f);
        add_noise(b, 16.0f, 0.5f, 0.1f, rng);
        add_sweep(b, 120, 60, 12.0f, 0.4f, 0);
        set(Sfx::WallHit, b, 1);
    }
    {
        auto b = buffer(0.4f);
        add_noise(b, 10.0f, 0.5f, 0.15f, rng);
        add_sweep(b, 180, 45, 9.0f, 0.55f, 0);
        set(Sfx::Cannon, b, 2);
    }
    {
        auto b = buffer(0.22f);
        Rng z{77};
        float phase = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / RATE;
            phase += (900.0f + z.uniform(-600.0f, 600.0f)) / RATE;
            b[i] = sq(phase) * 0.22f * std::exp(-t * 14.0f);
        }
        set(Sfx::Zap, b, 2);
    }
    {
        auto b = buffer(0.45f);
        add_sweep(b, 1800, 200, 6.0f, 0.3f, 1);
        add_sweep(b, 900, 120, 6.0f, 0.25f, 0);
        set(Sfx::Laser, b, 1);
    }
    {
        auto b = buffer(0.25f);
        add_noise(b, 6.0f, 0.25f, 0.2f, rng);
        set(Sfx::Flame, b, 1);
    }
    {
        auto b = buffer(1.6f);
        float phase = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            float t = static_cast<float>(i) / RATE;
            float f = 70.0f + 25.0f * std::sin(t * 9.0f) - 20.0f * t;
            phase += f / RATE;
            float env = std::min(1.0f, t * 6.0f) * std::exp(-t * 1.6f);
            b[i] = (sq(phase) * 0.35f + std::sin(TAU * phase * 0.5f) * 0.4f) * env;
        }
        add_noise(b, 2.0f, 0.25f, 0.05f, rng);
        set(Sfx::BossRoar, b, 1);
    }
    {
        auto b = buffer(1.8f);
        add_notes(b, {523.25f, 659.25f, 783.99f, 1046.5f, 783.99f, 1046.5f, 1318.5f}, 0.13f, 2.5f, 0.22f);
        set(Sfx::Victory, b, 1);
    }
    {
        auto b = buffer(1.8f);
        add_notes(b, {392.0f, 369.99f, 349.23f, 329.63f, 261.63f}, 0.22f, 2.0f, 0.26f);
        set(Sfx::Defeat, b, 1);
    }
    {
        auto b = buffer(0.05f);
        add_sweep(b, 1200, 900, 60.0f, 0.25f, 0);
        set(Sfx::Click, b, 1);
    }
    {
        auto b = buffer(0.4f);
        add_noise(b, 20.0f, 0.3f, 0.3f, rng);
        add_notes(b, {659.25f, 987.77f}, 0.08f, 8.0f, 0.25f);
        set(Sfx::Build, b, 1);
    }
    {
        auto b = buffer(0.5f);
        add_notes(b, {1318.5f, 1975.5f, 2637.0f}, 0.05f, 6.0f, 0.18f);
        set(Sfx::Gem, b, 1);
    }
    {
        auto b = buffer(0.06f);
        add_sweep(b, 2600, 3200, 45.0f, 0.12f, 2);
        add_noise(b, 60.0f, 0.08f, 0.9f, rng);
        set(Sfx::IceHit, b, 1);
    }
    {
        auto b = buffer(0.8f);
        add_noise(b, 9.0f, 0.5f, 0.85f, rng);
        add_notes(b, {1760.0f, 2349.3f, 2793.8f, 3520.0f}, 0.04f, 6.0f, 0.18f);
        set(Sfx::IceBreak, b, 1);
    }
}

void SoundManager::cleanup() {
    for (auto& slot : slots_) {
        if (!slot.loaded) continue;
        for (auto& v : slot.voices) UnloadSoundAlias(v);
        UnloadSound(slot.base);
        slot.loaded = false;
    }
}

void SoundManager::play(Sfx s, float vol, float pitch) {
    auto& slot = slots_[static_cast<size_t>(s)];
    if (!slot.loaded || volume <= 0.0f) return;
    if (slot.plays_this_frame >= slot.max_per_frame) return;
    slot.plays_this_frame++;
    Sound& v = slot.voices[static_cast<size_t>(slot.next)];
    slot.next = (slot.next + 1) % VOICES;
    SetSoundVolume(v, std::clamp(vol * volume, 0.0f, 1.0f));
    SetSoundPitch(v, pitch);
    PlaySound(v);
}

void SoundManager::end_frame() {
    for (auto& slot : slots_) slot.plays_this_frame = 0;
}

} // namespace ls
