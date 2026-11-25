#pragma once
#include "core/types.hpp"
#include "game/balance.hpp"
#include <array>
#include <cmath>

namespace ls {

inline constexpr int TURRET_MAX_LEVEL = 5;
inline constexpr int TURRET_KINDS = static_cast<int>(TurretType::Count) - 1;

struct TurretDef {
    TurretType type;
    const char* name;
    const char* blurb;
    int base_cost;
    float range;
    float interval;
    float damage;
    Color color;
};

inline constexpr std::array<TurretDef, TURRET_KINDS> TURRET_DEFS{{
    {TurretType::Cannon, "CANNON", "Splash shells", 60, 820.0f, 1.5f, 48.0f, {230, 120, 60, 255}},
    {TurretType::Tesla, "TESLA", "Chain lightning", 90, 560.0f, 0.95f, 24.0f, {120, 200, 255, 255}},
    {TurretType::Flamer, "FLAMER", "Burns the front", 75, 330.0f, 0.1f, 5.5f, {255, 150, 40, 255}},
    {TurretType::Laser, "RAILBEAM", "Pierces a line", 140, 1000.0f, 2.6f, 95.0f, {255, 70, 140, 255}},
}};

inline const TurretDef& turret_def(TurretType t) {
    return TURRET_DEFS[static_cast<size_t>(t) - 1];
}

inline int turret_upgrade_cost(TurretType t, int current_level) {
    const auto& d = turret_def(t);
    if (current_level <= 0) return d.base_cost;
    return static_cast<int>(std::round(d.base_cost * 0.9f * std::pow(1.7f, static_cast<float>(current_level))));
}

inline int turret_refund(TurretType t, int level) {
    int spent = 0;
    for (int l = 0; l < level; ++l) spent += turret_upgrade_cost(t, l);
    return spent / 2;
}

inline float turret_damage(TurretType t, int level, int stage) {
    return turret_def(t).damage * std::pow(1.6f, static_cast<float>(level - 1)) * stage_hp_mult(stage);
}

inline float turret_interval(TurretType t, int level) {
    return turret_def(t).interval * (1.0f - 0.06f * static_cast<float>(level - 1));
}

inline int tesla_chains(int level) {
    return 4 + level;
}

} // namespace ls
