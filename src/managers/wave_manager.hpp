#pragma once
#include <vector>
#include <cmath>
#include "core/types.hpp"
#include "core/constants.hpp"

namespace ls {

struct SpawnEntry {
    EnemyType type;
    int count;
    float delay;
};

struct WaveData {
    WaveNum number;
    std::vector<SpawnEntry> spawns;
    bool is_boss_wave;
};

class WaveManager {
public:
    WaveManager() { generate_waves(); }

    const WaveData& get_wave(WaveNum n) const {
        return waves_.at(std::min(n, static_cast<WaveNum>(waves_.size()) - 1));
    }

    WaveNum total_waves() const { return static_cast<WaveNum>(waves_.size()); }

    float scaling(WaveNum wave) const {
        return 1.0f + (wave - 1) * 0.15f;
    }

private:
    void generate_waves() {
        waves_.reserve(MAX_WAVES);
        for (WaveNum w = 1; w <= MAX_WAVES; ++w) {
            WaveData wave;
            wave.number = w;
            wave.is_boss_wave = (w % BOSS_WAVE_INTERVAL == 0);

            int base_count = 5 + w;

            if (wave.is_boss_wave) {
                // Regular enemies before boss
                wave.spawns.push_back({EnemyType::Grunt, base_count / 2, SPAWN_INTERVAL});
                if (w >= 10) wave.spawns.push_back({EnemyType::Tank, 2, SPAWN_INTERVAL * 1.5f});
                wave.spawns.push_back({EnemyType::Boss, 1, 2.0f});
            } else {
                wave.spawns.push_back({EnemyType::Grunt, base_count, SPAWN_INTERVAL});

                if (w >= 3) wave.spawns.push_back({EnemyType::Runner, static_cast<int>(w / 3), SPAWN_INTERVAL * 0.8f});
                if (w >= 6) wave.spawns.push_back({EnemyType::Tank, static_cast<int>(w / 6), SPAWN_INTERVAL * 1.5f});
                if (w >= 9) wave.spawns.push_back({EnemyType::Healer, static_cast<int>(w / 9), SPAWN_INTERVAL * 1.2f});
                if (w >= 12) wave.spawns.push_back({EnemyType::Flying, static_cast<int>(w / 6), SPAWN_INTERVAL});
            }

            waves_.push_back(std::move(wave));
        }
    }

    std::vector<WaveData> waves_;
};

} // namespace ls
