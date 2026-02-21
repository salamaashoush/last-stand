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
        return 1.0f + (wave - 1) * 0.2f;
    }

private:
    void generate_waves() {
        waves_.reserve(MAX_WAVES);
        for (WaveNum w = 1; w <= MAX_WAVES; ++w) {
            WaveData wave;
            wave.number = w;
            wave.is_boss_wave = (w % BOSS_WAVE_INTERVAL == 0);

            int base_count = 6 + static_cast<int>(w * 1.2f);
            float interval = std::max(0.3f, SPAWN_INTERVAL - w * 0.008f);

            if (wave.is_boss_wave) {
                // Regular enemies before boss
                wave.spawns.push_back({EnemyType::Grunt, base_count / 2 + 2, interval});
                if (w >= 10) wave.spawns.push_back({EnemyType::Tank, 3, interval * 1.5f});
                if (w >= 15) wave.spawns.push_back({EnemyType::Runner, 4, interval * 0.7f});
                wave.spawns.push_back({EnemyType::Boss, 1, 2.0f});
            } else {
                wave.spawns.push_back({EnemyType::Grunt, base_count, interval});

                if (w >= 2) wave.spawns.push_back({EnemyType::Runner, static_cast<int>(w / 2), interval * 0.7f});
                if (w >= 4) wave.spawns.push_back({EnemyType::Tank, static_cast<int>(w / 4), interval * 1.4f});
                if (w >= 7) wave.spawns.push_back({EnemyType::Healer, static_cast<int>(w / 6), interval * 1.1f});
                if (w >= 9) wave.spawns.push_back({EnemyType::Flying, static_cast<int>(w / 4), interval * 0.9f});
            }

            waves_.push_back(std::move(wave));
        }
    }

    std::vector<WaveData> waves_;
};

} // namespace ls
