#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <raylib.h>

namespace ls {

enum class UnitType : uint8_t { Rifleman, Gunner, Shotgunner, Sniper, Grenadier, Flamer, Count };

inline constexpr int UNIT_COUNT = static_cast<int>(UnitType::Count);
inline constexpr int UNIT_MAX_LEVEL = 10;
inline constexpr int LOADOUT_SLOTS = 3;

enum class Shot : uint8_t { Bullet, Grenade, Flame };

struct UnitDef {
    UnitType id;
    const char* name;
    const char* blurb;
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
};

inline constexpr std::array<UnitDef, UNIT_COUNT> UNIT_DEFS{{
    {UnitType::Rifleman,
     "RIFLEMAN",
     "Reliable all-rounder",
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
     {58, 78, 44, 255}},
    {UnitType::Gunner,
     "GUNNER",
     "Minigun bullet hose",
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
     150,
     {84, 122, 170, 255},
     {44, 60, 96, 255}},
    {UnitType::Shotgunner,
     "SHOTGUNNER",
     "Point-blank shredder",
     Shot::Bullet,
     0.45f,
     0.8f,
     4,
     26.0f,
     1000.0f,
     0.6f,
     0,
     0.0f,
     32.0f,
     300,
     {176, 122, 76, 255},
     {96, 60, 34, 255}},
    {UnitType::Sniper,
     "SNIPER",
     "Huge hits that pierce",
     Shot::Bullet,
     0.36f,
     1.5f,
     1,
     0.5f,
     1900.0f,
     1.0f,
     3,
     0.0f,
     40.0f,
     500,
     {112, 116, 130, 255},
     {44, 46, 56, 255}},
    {UnitType::Grenadier,
     "GRENADIER",
     "Lobs splash grenades",
     Shot::Grenade,
     0.34f,
     0.8f,
     1,
     6.0f,
     0.0f,
     0.0f,
     0,
     58.0f,
     30.0f,
     800,
     {164, 164, 84, 255},
     {84, 90, 40, 255}},
    {UnitType::Flamer,
     "FLAMER",
     "Burns the front line",
     Shot::Flame,
     3.0f,
     0.11f,
     1,
     14.0f,
     820.0f,
     0.42f,
     99,
     0.0f,
     3.0f,
     1200,
     {214, 112, 70, 255},
     {70, 70, 76, 255}},
}};

inline const UnitDef& unit_def(UnitType t) {
    return UNIT_DEFS[static_cast<size_t>(t)];
}

inline float unit_damage_mult(int level) {
    return 1.0f + 0.15f * static_cast<float>(std::max(level, 1) - 1);
}
inline float unit_rate_mult(int level) {
    return 1.0f + 0.1f * static_cast<float>((std::max(level, 1) - 1) / 3);
}
inline int unit_upgrade_cost(int level) {
    return static_cast<int>(std::round(30.0 * std::pow(1.5, static_cast<double>(std::max(level, 1) - 1))));
}

} // namespace ls
