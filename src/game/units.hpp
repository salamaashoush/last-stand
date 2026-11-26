#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <raylib.h>
#include <vector>

namespace ls {

enum class UnitType : uint8_t { Rifleman, Gunner, Shotgunner, Sniper, Grenadier, Flamer, Cryo, Volt, Count };

inline constexpr int UNIT_COUNT = static_cast<int>(UnitType::Count);
inline constexpr int UNIT_MAX_LEVEL = 10;
inline constexpr int LOADOUT_SLOTS = 4;

enum class Shot : uint8_t { Bullet, Grenade, Flame, Cryo, Volt };

struct UnitDef {
    UnitType id;
    const char* name;
    const char* blurb;
    const char* passive;
    Shot shot;
    float rate_mult;
    float damage_mult;
    int pellets;
    float spread_deg;
    float speed;
    float life;
    int bonus_pierce;
    float splash;
    float knock;
    int unlock_cost;
    Color uniform;
    Color helmet;
    Color tracer;
};

inline constexpr std::array<UnitDef, UNIT_COUNT> UNIT_DEFS{{
    {UnitType::Rifleman,
     "RIFLEMAN",
     "Reliable all-rounder",
     "Every 5th shot pierces armor",
     Shot::Bullet,
     1.0f,
     1.0f,
     1,
     3.0f,
     1150.0f,
     1.4f,
     0,
     0.0f,
     14.0f,
     0,
     {98, 140, 80, 255},
     {58, 78, 44, 255},
     {255, 226, 120, 255}},
    {UnitType::Gunner,
     "GUNNER",
     "Minigun bullet hose",
     "Suppressing fire slows targets",
     Shot::Bullet,
     2.4f,
     0.46f,
     1,
     8.0f,
     1250.0f,
     1.2f,
     0,
     0.0f,
     8.0f,
     140,
     {84, 122, 170, 255},
     {44, 60, 96, 255},
     {255, 180, 75, 255}},
    {UnitType::Shotgunner,
     "SHOTGUNNER",
     "Point-blank shredder",
     "+60% shield & close-range dmg",
     Shot::Bullet,
     0.45f,
     0.8f,
     4,
     26.0f,
     1000.0f,
     0.6f,
     0,
     0.0f,
     34.0f,
     260,
     {176, 122, 76, 255},
     {96, 60, 34, 255},
     {255, 145, 70, 255}},
    {UnitType::Sniper,
     "SNIPER",
     "Rail rifle, hunts elites",
     "+25% crit & prioritizes elites",
     Shot::Bullet,
     0.38f,
     1.55f,
     1,
     0.4f,
     1950.0f,
     1.0f,
     3,
     0.0f,
     42.0f,
     420,
     {112, 116, 130, 255},
     {44, 46, 56, 255},
     {255, 245, 180, 255}},
    {UnitType::Grenadier,
     "GRENADIER",
     "Lobs armor-shred shells",
     "Blasts strip enemy armor",
     Shot::Grenade,
     0.34f,
     0.85f,
     1,
     6.0f,
     0.0f,
     0.0f,
     0,
     60.0f,
     32.0f,
     620,
     {164, 164, 84, 255},
     {84, 90, 40, 255},
     {255, 160, 60, 255}},
    {UnitType::Flamer,
     "FLAMER",
     "Napalm line-burner",
     "Burning foes take +20% dmg",
     Shot::Flame,
     3.0f,
     0.12f,
     1,
     14.0f,
     820.0f,
     0.44f,
     99,
     0.0f,
     3.0f,
     840,
     {214, 112, 70, 255},
     {70, 70, 76, 255},
     {255, 110, 40, 255}},
    {UnitType::Cryo,
     "CRYO TROOPER",
     "Frost lance chills hordes",
     "Stacks frost to freeze foes",
     Shot::Cryo,
     0.78f,
     0.95f,
     1,
     4.0f,
     1120.0f,
     1.1f,
     1,
     0.0f,
     16.0f,
     520,
     {82, 162, 206, 255},
     {42, 92, 132, 255},
     {130, 230, 255, 255}},
    {UnitType::Volt,
     "VOLT TROOPER",
     "Arc carbine chains shock",
     "Arcs to 2 foes, x2 vs shields",
     Shot::Volt,
     0.92f,
     0.88f,
     1,
     5.0f,
     1350.0f,
     1.15f,
     0,
     0.0f,
     12.0f,
     980,
     {126, 94, 186, 255},
     {58, 42, 96, 255},
     {185, 130, 255, 255}},
}};

inline const UnitDef& unit_def(UnitType t) {
    return UNIT_DEFS[static_cast<size_t>(t)];
}

inline float unit_damage_mult(int level) {
    return 1.0f + 0.16f * static_cast<float>(std::max(level, 1) - 1);
}
inline float unit_rate_mult(int level) {
    return 1.0f + 0.1f * static_cast<float>((std::max(level, 1) - 1) / 3);
}
inline int unit_upgrade_cost(int level) {
    return static_cast<int>(std::round(30.0 * std::pow(1.48, static_cast<double>(std::max(level, 1) - 1))));
}

// Squad synergies reward building cohesive multi-class loadouts.
struct SquadSynergies {
    bool combined_arms{false};  // 3+ unique unit types: +15% damage
    bool bullet_storm{false};   // Rifleman + Gunner: +15% fire rate
    bool heavy_ordnance{false}; // Shotgunner + (Grenadier or Sniper): +25% knockback & armor shred
    bool elemental_fury{false}; // Any 2 of Flamer / Cryo / Volt: +22% status & elemental damage

    float damage_mult() const { return combined_arms ? 1.15f : 1.0f; }
    float rate_mult() const { return bullet_storm ? 1.15f : 1.0f; }
    float knock_mult() const { return heavy_ordnance ? 1.25f : 1.0f; }
    float elemental_mult() const { return elemental_fury ? 1.22f : 1.0f; }
    int count() const {
        return (combined_arms ? 1 : 0) + (bullet_storm ? 1 : 0) + (heavy_ordnance ? 1 : 0) + (elemental_fury ? 1 : 0);
    }
};

inline SquadSynergies compute_synergies(const std::vector<UnitType>& squad) {
    SquadSynergies syn;
    auto has = [&](UnitType u) { return std::ranges::find(squad, u) != squad.end(); };
    int unique = 0;
    for (int i = 0; i < UNIT_COUNT; ++i) {
        if (has(static_cast<UnitType>(i))) unique++;
    }
    syn.combined_arms = unique >= 3;
    syn.bullet_storm = has(UnitType::Rifleman) && has(UnitType::Gunner);
    syn.heavy_ordnance = has(UnitType::Shotgunner) && (has(UnitType::Grenadier) || has(UnitType::Sniper));
    int elem = (has(UnitType::Flamer) ? 1 : 0) + (has(UnitType::Cryo) ? 1 : 0) + (has(UnitType::Volt) ? 1 : 0);
    syn.elemental_fury = elem >= 2;
    return syn;
}

} // namespace ls
