#pragma once

namespace ls {

struct HeroUpgrades {
    int upgrade_xp{0};
    int attack_range_level{0};
    int magnet_level{0};
    int attack_damage_level{0};
    int attack_speed_level{0};
    int max_hp_level{0};
    static constexpr int MAX_LEVEL = 5;
    float bonus_range() const { return attack_range_level * 30.0f; }
    float bonus_pickup() const { return magnet_level * 40.0f; }
    int bonus_damage() const { return attack_damage_level * 5; }
    float bonus_cooldown() const { return attack_speed_level * 0.04f; }
    int bonus_hp() const { return max_hp_level * 40; }
    int cost(int level) const {
        constexpr int costs[] = {100, 200, 400, 700, 1000};
        return (level >= 0 && level < MAX_LEVEL) ? costs[level] : 0;
    }
};

} // namespace ls
