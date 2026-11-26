#pragma once
#include "core/types.hpp"
#include "game/balance.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace ls {

inline constexpr int TURRET_MAX_LEVEL = 5;
inline constexpr int TURRET_KINDS = static_cast<int>(TurretType::Count) - 1;

struct TurretDef {
    TurretType type;
    const char* name;
    const char* blurb;
    const char* perk_lv3;
    const char* perk_lv5;
    int unlock_stage;
    int base_cost;
    float range;
    float interval;
    float damage;
    Color color;
};

inline constexpr std::array<TurretDef, TURRET_KINDS> TURRET_DEFS{{
    {TurretType::Cannon, "CANNON", "Splash artillery", "LV3: Shreds armor + big blast", "LV5: Twin Siege Shells", 1, 60,
     820.0f, 1.5f, 48.0f, {230, 120, 60, 255}},
    {TurretType::Tesla, "TESLA", "Chain lightning", "LV3: +2 arcs & shocks foes", "LV5: Final-hit Storm Nova", 2, 90,
     560.0f, 0.95f, 24.0f, {120, 200, 255, 255}},
    {TurretType::Flamer, "FLAMER", "Burns the front", "LV3: Wider cone + fierce burn", "LV5: Ignites Napalm Pools", 1,
     75, 360.0f, 0.1f, 5.8f, {255, 150, 40, 255}},
    {TurretType::Laser, "RAILBEAM", "Pierces a line", "LV3: Wider armor-melt beam", "LV5: Hyper-Crit Dual Beam", 4, 140,
     1000.0f, 2.6f, 95.0f, {255, 70, 140, 255}},
    {TurretType::Frost, "CRYO NOVA", "Freezes & shreds", "LV3: Instant Deep-Freeze", "LV5: Twin Frost + Ice Shards", 3,
     85, 640.0f, 1.35f, 34.0f, {110, 232, 255, 255}},
    {TurretType::Missile, "MISSILE POD", "Homing rocket swarm", "LV3: Quad Swarm + Shred", "LV5: 6-Rocket Cluster", 5,
     120, 980.0f, 1.85f, 52.0f, {255, 105, 75, 255}},
}};

inline constexpr std::array<TurretType, TURRET_KINDS> TURRET_DISPLAY_ORDER{{
    TurretType::Cannon,
    TurretType::Flamer,
    TurretType::Tesla,
    TurretType::Frost,
    TurretType::Laser,
    TurretType::Missile,
}};

inline const TurretDef& turret_def(TurretType t) {
    return TURRET_DEFS[static_cast<size_t>(t) - 1];
}

inline bool turret_unlocked(TurretType t, int unlocked_stage) {
    if (t == TurretType::None || t == TurretType::Count) return false;
    return unlocked_stage >= turret_def(t).unlock_stage;
}

inline int turret_upgrade_cost(TurretType t, int current_level, float cost_mult = 1.0f) {
    const auto& d = turret_def(t);
    float raw = current_level <= 0
                    ? static_cast<float>(d.base_cost)
                    : static_cast<float>(d.base_cost) * 0.9f * std::pow(1.7f, static_cast<float>(current_level));
    return std::max(10, static_cast<int>(std::round(raw * cost_mult)));
}

inline int turret_refund(TurretType t, int level, float cost_mult = 1.0f) {
    int spent = 0;
    for (int l = 0; l < level; ++l) spent += turret_upgrade_cost(t, l, cost_mult);
    return spent / 2;
}

inline float turret_damage(TurretType t, int level, int stage) {
    // Turrets keep their own stage curve so tuning enemy HP does not buff them in step.
    return turret_def(t).damage * std::pow(1.6f, static_cast<float>(level - 1)) *
           std::pow(1.3f, static_cast<float>(std::max(stage, 1) - 1));
}

inline float turret_interval(TurretType t, int level) {
    return turret_def(t).interval * (1.0f - 0.06f * static_cast<float>(level - 1));
}

inline int tesla_chains(int level) {
    return 4 + level;
}

} // namespace ls
