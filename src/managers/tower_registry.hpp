#pragma once
#include "components/components.hpp"
#include "core/types.hpp"
#include <array>
#include <unordered_map>

namespace ls {

struct TowerStats {
    TowerType type;
    int level;
    int cost;
    int damage;
    float range;
    float fire_rate;
    float aoe_radius;
    int chain_count;
    EffectType effect;
    float effect_duration;
    float slow_factor;
    Color color;
    std::string name;
};

class TowerRegistry {
  public:
    TowerRegistry() { init(); }

    const TowerStats& get(TowerType type, int level) const { return stats_.at(key(type, level)); }

    int upgrade_cost(TowerType type, int level) const {
        if (level >= 3) return 0;
        return get(type, level + 1).cost;
    }

    static constexpr int MAX_LEVEL = 3;

  private:
    static uint32_t key(TowerType t, int l) { return static_cast<uint32_t>(t) * 10 + l; }

    void add(TowerStats s) { stats_[key(s.type, s.level)] = s; }

    void init() {
        // fire_rate = shots per second for projectile towers
        // fire_rate = seconds between ticks for laser tower

        // clang-format off
        // Arrow Tower - reliable single target DPS
        // L1: 15 DPS, L2: 31 DPS, L3: 53 DPS
        add({TowerType::Arrow, 1, 50,  15, 150, 1.0f, 0, 0, EffectType::None, 0, 1.0f, {200, 150, 50, 255}, "Arrow"});
        add({TowerType::Arrow, 2, 75,  25, 170, 1.25f, 0, 0, EffectType::None, 0, 1.0f, {220, 170, 60, 255}, "Arrow II"});
        add({TowerType::Arrow, 3, 125, 40, 200, 1.33f, 0, 0, EffectType::None, 0, 1.0f, {240, 190, 70, 255}, "Arrow III"});

        // Cannon Tower - slow but AoE, lower single-target DPS
        // L1: 15 DPS single (AoE), L2: 22 DPS, L3: 35 DPS
        add({TowerType::Cannon, 1, 100, 40, 120, 0.4f, 48, 0, EffectType::None, 0, 1.0f, {100, 100, 100, 255}, "Cannon"});
        add({TowerType::Cannon, 2, 150, 55, 130, 0.4f, 56, 0, EffectType::None, 0, 1.0f, {120, 120, 120, 255}, "Cannon II"});
        add({TowerType::Cannon, 3, 250, 70, 150, 0.5f, 64, 0, EffectType::None, 0, 1.0f, {140, 140, 140, 255}, "Cannon III"});

        // Ice Tower - low DPS but strong slow
        // L1: 8 DPS + 50% slow, L2: 13 DPS + 60% slow, L3: 20 DPS + 70% slow
        add({TowerType::Ice, 1, 75,  10, 130, 0.8f, 0, 0, EffectType::Slow, 2.0f, 0.5f, {100, 180, 255, 255}, "Ice"});
        add({TowerType::Ice, 2, 100, 15, 150, 0.9f, 0, 0, EffectType::Slow, 2.5f, 0.4f, {120, 200, 255, 255}, "Ice II"});
        add({TowerType::Ice, 3, 175, 20, 170, 1.0f, 0, 0, EffectType::Slow, 3.0f, 0.3f, {140, 220, 255, 255}, "Ice III"});

        // Lightning Tower - chain hits multiple enemies
        // L1: 14 DPS + 2 chains, L2: 24 DPS + 3 chains, L3: 40 DPS + 4 chains + stun
        add({TowerType::Lightning, 1, 125, 20, 140, 0.7f, 0, 2, EffectType::None, 0, 1.0f, {255, 255, 100, 255}, "Lightning"});
        add({TowerType::Lightning, 2, 175, 30, 160, 0.8f, 0, 3, EffectType::None, 0, 1.0f, {255, 255, 130, 255}, "Lightning II"});
        add({TowerType::Lightning, 3, 275, 45, 180, 0.9f, 0, 4, EffectType::Stun, 0.5f, 1.0f, {255, 255, 160, 255}, "Lightning III"});

        // Poison Tower - low direct DPS but strong DoT over time
        // L1: 5 DPS direct + 10 DoT/sec, L2: 8 + 10, L3: 12 + 10
        add({TowerType::Poison, 1, 75,  8,  130, 0.6f, 0, 0, EffectType::Poison, 3.0f, 1.0f, {100, 200, 50, 255}, "Poison"});
        add({TowerType::Poison, 2, 100, 12, 150, 0.7f, 0, 0, EffectType::Poison, 4.0f, 1.0f, {120, 220, 60, 255}, "Poison II"});
        add({TowerType::Poison, 3, 175, 18, 170, 0.8f, 0, 0, EffectType::Poison, 5.0f, 1.0f, {140, 240, 70, 255}, "Poison III"});

        // Laser Tower - continuous beam, high sustained DPS + burn
        // L1: 160 DPS, L2: 240 DPS, L3: 360 DPS (but expensive)
        add({TowerType::Laser, 1, 150, 8,  160, 0.05f, 0, 0, EffectType::Burn, 1.0f, 1.0f, {255, 50, 50, 255}, "Laser"});
        add({TowerType::Laser, 2, 225, 12, 180, 0.05f, 0, 0, EffectType::Burn, 1.5f, 1.0f, {255, 80, 80, 255}, "Laser II"});
        add({TowerType::Laser, 3, 375, 18, 200, 0.05f, 0, 0, EffectType::Burn, 2.0f, 1.0f, {255, 110, 110, 255}, "Laser III"});
        // clang-format on
    }

    std::unordered_map<uint32_t, TowerStats> stats_;
};

} // namespace ls
