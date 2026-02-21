#pragma once
#include <cmath>
#include <cstring>
#include <raylib.h>
#include <vector>

namespace ls {

class SoundManager {
  public:
    void init() {
        arrow_fire = gen_sweep(800, 400, 0.08f, WaveTriangle);
        cannon_fire = gen_cannon(0.15f);
        ice_fire = gen_sweep(2000, 500, 0.12f, WaveSine);
        lightning_fire = gen_noise_burst(0.06f);
        poison_fire = gen_am_sine(200, 0.1f);
        laser_hum = gen_sine(440, 0.05f);
        enemy_death = gen_noise_decay(0.1f);
        boss_death = gen_rumble(0.5f);
        tower_place = gen_sweep(400, 800, 0.1f, WaveSine);
        wave_start = gen_sine(600, 0.3f);
        hero_ability = gen_chord(0.15f);
        ui_click = gen_sine(1000, 0.03f);
        enemy_hit = gen_noise_burst(0.04f);
        initialized_ = true;
    }

    void cleanup() {
        if (!initialized_) return;
        auto unload = [](Sound& s) {
            if (s.frameCount > 0) UnloadSound(s);
            s.frameCount = 0;
        };
        unload(arrow_fire);
        unload(cannon_fire);
        unload(ice_fire);
        unload(lightning_fire);
        unload(poison_fire);
        unload(laser_hum);
        unload(enemy_death);
        unload(boss_death);
        unload(tower_place);
        unload(wave_start);
        unload(hero_ability);
        unload(ui_click);
        unload(enemy_hit);
        initialized_ = false;
    }

    void play(Sound& snd, float volume = 1.0f) {
        if (!initialized_ || snd.frameCount == 0) return;
        SetSoundVolume(snd, volume * master_volume);
        PlaySound(snd);
    }

    float master_volume{0.7f};
    bool initialized_{false};

    Sound arrow_fire{};
    Sound cannon_fire{};
    Sound ice_fire{};
    Sound lightning_fire{};
    Sound poison_fire{};
    Sound laser_hum{};
    Sound enemy_death{};
    Sound boss_death{};
    Sound tower_place{};
    Sound wave_start{};
    Sound hero_ability{};
    Sound ui_click{};
    Sound enemy_hit{};

  private:
    static constexpr int SAMPLE_RATE = 44100;

    enum WaveType { WaveSine, WaveTriangle };

    static float wave_sample(WaveType type, float phase) {
        switch (type) {
        case WaveSine:
            return std::sin(phase * 2.0f * PI);
        case WaveTriangle: {
            float t = std::fmod(phase, 1.0f);
            return (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
        }
        }
        return 0.0f;
    }

    Sound make_sound(const std::vector<float>& samples) {
        Wave wave{};
        wave.frameCount = static_cast<unsigned int>(samples.size());
        wave.sampleRate = SAMPLE_RATE;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = std::malloc(samples.size() * sizeof(short));
        auto* buf = static_cast<short*>(wave.data);
        for (size_t i = 0; i < samples.size(); ++i) {
            float s = std::clamp(samples[i], -1.0f, 1.0f);
            buf[i] = static_cast<short>(s * 32000.0f);
        }
        Sound snd = LoadSoundFromWave(wave);
        UnloadWave(wave);
        return snd;
    }

    Sound gen_sweep(float freq_start, float freq_end, float duration, WaveType type) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        float phase = 0;
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float freq = freq_start + (freq_end - freq_start) * t;
            phase += freq / SAMPLE_RATE;
            float envelope = 1.0f - t;
            samples[i] = wave_sample(type, phase) * envelope * 0.5f;
        }
        return make_sound(samples);
    }

    Sound gen_sine(float freq, float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float envelope = 1.0f - t;
            samples[i] = std::sin(2.0f * PI * freq * i / SAMPLE_RATE) * envelope * 0.4f;
        }
        return make_sound(samples);
    }

    Sound gen_noise_burst(float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float noise = (static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f);
            samples[i] = noise * (1.0f - t) * 0.3f;
        }
        return make_sound(samples);
    }

    Sound gen_noise_decay(float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float noise = (static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f);
            float envelope = std::exp(-t * 8.0f);
            samples[i] = noise * envelope * 0.4f;
        }
        return make_sound(samples);
    }

    Sound gen_cannon(float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float noise = (static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f);
            float sine = std::sin(2.0f * PI * 120.0f * i / SAMPLE_RATE);
            float envelope = std::exp(-t * 6.0f);
            samples[i] = (noise * 0.4f + sine * 0.6f) * envelope * 0.5f;
        }
        return make_sound(samples);
    }

    Sound gen_am_sine(float freq, float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float carrier = std::sin(2.0f * PI * freq * i / SAMPLE_RATE);
            float modulator = 0.5f + 0.5f * std::sin(2.0f * PI * 15.0f * i / SAMPLE_RATE);
            float envelope = 1.0f - t;
            samples[i] = carrier * modulator * envelope * 0.4f;
        }
        return make_sound(samples);
    }

    Sound gen_rumble(float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float sine = std::sin(2.0f * PI * 80.0f * i / SAMPLE_RATE);
            float noise = (static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f);
            float envelope = (1.0f - t) * (1.0f - t);
            samples[i] = (sine * 0.5f + noise * 0.5f) * envelope * 0.6f;
        }
        return make_sound(samples);
    }

    Sound gen_chord(float duration) {
        int count = static_cast<int>(SAMPLE_RATE * duration);
        std::vector<float> samples(count);
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / count;
            float s1 = std::sin(2.0f * PI * 300.0f * i / SAMPLE_RATE);
            float s2 = std::sin(2.0f * PI * 400.0f * i / SAMPLE_RATE);
            float s3 = std::sin(2.0f * PI * 500.0f * i / SAMPLE_RATE);
            float envelope = 1.0f - t;
            samples[i] = (s1 + s2 + s3) / 3.0f * envelope * 0.4f;
        }
        return make_sound(samples);
    }
};

} // namespace ls
