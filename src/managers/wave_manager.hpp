#pragma once
#include "core/constants.hpp"
#include "core/types.hpp"
#include <cmath>
#include <vector>

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
        // Steeper scaling: enemies get noticeably tougher each wave
        return 1.0f + (wave - 1) * 0.3f + (wave > 15 ? (wave - 15) * 0.15f : 0.0f);
    }

  private:
    void generate_waves() {
        waves_.reserve(MAX_WAVES);
        for (WaveNum w = 1; w <= MAX_WAVES; ++w) {
            WaveData wave;
            wave.number = w;
            wave.is_boss_wave = (w % BOSS_WAVE_INTERVAL == 0);

            int base_count = 10 + static_cast<int>(w * 2.5f);
            float interval = std::max(0.2f, SPAWN_INTERVAL - w * 0.01f);

            if (wave.is_boss_wave) {
                // Large escort before boss
                wave.spawns.push_back({EnemyType::Grunt, base_count / 2 + 5, interval});
                wave.spawns.push_back({EnemyType::Runner, 3 + static_cast<int>(w / 3), interval * 0.6f});
                if (w >= 10) wave.spawns.push_back({EnemyType::Tank, 3 + static_cast<int>(w / 5), interval * 1.2f});
                if (w >= 15) wave.spawns.push_back({EnemyType::Flying, 4 + static_cast<int>(w / 5), interval * 0.8f});
                wave.spawns.push_back({EnemyType::Boss, 1 + static_cast<int>(w / 15), 1.5f});
            } else {
                wave.spawns.push_back({EnemyType::Grunt, base_count, interval});

                if (w >= 2) wave.spawns.push_back({EnemyType::Runner, 2 + static_cast<int>(w * 0.8f), interval * 0.6f});
                if (w >= 4) wave.spawns.push_back({EnemyType::Tank, 1 + static_cast<int>(w / 3), interval * 1.2f});
                if (w >= 6) wave.spawns.push_back({EnemyType::Healer, 1 + static_cast<int>(w / 4), interval});
                if (w >= 8) wave.spawns.push_back({EnemyType::Flying, 2 + static_cast<int>(w / 3), interval * 0.7f});
            }

            waves_.push_back(std::move(wave));
        }
    }

    std::vector<WaveData> waves_;
};

} // namespace ls
