#pragma once
#include "core/rng.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace ls {

enum class Weather : uint8_t { Clear, Snow, Rain, Leaves, Embers, Dust, Fireflies };
enum class LockKind : uint8_t { Ice, Crate, Stone, Cage, Crystal };
enum class Decor : uint8_t { Trees, Rocks, Bones, Crystals, Ruins };

struct Theme {
    int key{0};
    int biome{0};
    std::string name;
    Color ground{};
    Color dark{};
    Color light{};
    Color stone{};
    Color court{};
    Color fog{};
    Color enemy_tint{};
    float enemy_tint_amount{0.0f};
    Weather weather{Weather::Clear};
    LockKind lock{LockKind::Crate};
    Decor decor{Decor::Rocks};
    float night{0.0f};
};

inline Color jitter(Color c, Rng& rng, int amount) {
    auto j = [&](unsigned char v) {
        return static_cast<unsigned char>(std::clamp(v + rng.range(-amount, amount), 0, 255));
    };
    return {j(c.r), j(c.g), j(c.b), c.a};
}

inline Color blend(Color a, Color b, float t) {
    auto l = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

// Every stage gets its own deterministic look from its key: palette, weather, time of day,
// set dressing, the material its locked loot is sealed in, and a tint on the horde.
inline Theme make_theme(int key) {
    Rng rng{static_cast<uint64_t>(key) * 2654435761ull + 0x51ED27ull};
    Theme t;
    t.key = key;
    t.biome = (std::max(key, 1) - 1) % 4;
    struct Base {
        Color ground, dark, light, stone, court, fog;
    };
    static const Base bases[4] = {
        {{98, 158, 74, 255},
         {78, 134, 58, 255},
         {130, 182, 90, 255},
         {168, 160, 150, 255},
         {92, 88, 84, 255},
         {20, 34, 18, 255}},
        {{214, 186, 124, 255},
         {192, 160, 102, 255},
         {232, 208, 150, 255},
         {196, 170, 130, 255},
         {120, 100, 76, 255},
         {60, 40, 20, 255}},
        {{224, 232, 240, 255},
         {196, 210, 226, 255},
         {244, 248, 252, 255},
         {150, 164, 182, 255},
         {84, 96, 116, 255},
         {30, 40, 60, 255}},
        {{88, 80, 78, 255},
         {66, 60, 60, 255},
         {118, 100, 92, 255},
         {120, 110, 104, 255},
         {56, 50, 50, 255},
         {16, 8, 8, 255}},
    };
    const auto& b = bases[t.biome];
    t.ground = jitter(b.ground, rng, 14);
    t.dark = jitter(b.dark, rng, 14);
    t.light = jitter(b.light, rng, 14);
    auto neutral = [&](Color c, int amount) {
        int d = rng.range(-amount, amount);
        auto j = [d](unsigned char v) { return static_cast<unsigned char>(std::clamp(v + d, 0, 255)); };
        return Color{j(c.r), j(c.g), j(c.b), c.a};
    };
    t.stone = neutral(b.stone, 16);
    t.court = neutral(b.court, 10);
    t.fog = b.fog;

    static const char* adjectives[4][6] = {
        {"VERDANT", "WHISPERING", "ROTTING", "MISTY", "BROKEN", "GREEN"},
        {"SCORCHED", "BLEACHED", "SHIFTING", "GOLDEN", "SUNKEN", "DRY"},
        {"FROZEN", "SILENT", "BITTER", "PALE", "HOWLING", "GLACIAL"},
        {"BURNING", "ASHEN", "CINDER", "BLACK", "MOLTEN", "CURSED"},
    };
    static const char* places[4][6] = {
        {"MEADOW", "HOLLOW", "VALE", "FIELDS", "GROVE", "CROSSING"},
        {"DUNES", "MESA", "WASTES", "CANYON", "OASIS", "SANDS"},
        {"PASS", "TUNDRA", "PEAKS", "FJORD", "DRIFTS", "GLACIER"},
        {"PITS", "CALDERA", "RIDGE", "FORGE", "SCAR", "BADLANDS"},
    };
    t.name = std::string(adjectives[t.biome][rng.range(0, 5)]) + " " + places[t.biome][rng.range(0, 5)];

    t.night = rng.chance(key <= 2 ? 0.0f : 0.3f) ? rng.uniform(0.3f, 0.55f) : 0.0f;
    switch (t.biome) {
    case 0:
        t.weather = t.night > 0.0f ? Weather::Fireflies : (rng.chance(0.5f) ? Weather::Leaves : Weather::Rain);
        t.lock = rng.chance(0.5f) ? LockKind::Crate : LockKind::Stone;
        t.decor = rng.chance(0.6f) ? Decor::Trees : Decor::Ruins;
        break;
    case 1:
        t.weather = rng.chance(0.6f) ? Weather::Dust : Weather::Clear;
        t.lock = rng.chance(0.5f) ? LockKind::Crate : LockKind::Cage;
        t.decor = rng.chance(0.5f) ? Decor::Bones : Decor::Rocks;
        break;
    case 2:
        t.weather = Weather::Snow;
        t.lock = rng.chance(0.7f) ? LockKind::Ice : LockKind::Crystal;
        t.decor = rng.chance(0.5f) ? Decor::Crystals : Decor::Rocks;
        break;
    default:
        t.weather = Weather::Embers;
        t.lock = rng.chance(0.5f) ? LockKind::Crystal : (rng.chance(0.5f) ? LockKind::Cage : LockKind::Stone);
        t.decor = rng.chance(0.5f) ? Decor::Bones : Decor::Ruins;
        break;
    }

    static const Color tints[] = {{120, 220, 200, 255}, {230, 210, 90, 255},  {200, 120, 220, 255},
                                  {240, 120, 90, 255},  {120, 150, 240, 255}, {170, 230, 110, 255}};
    t.enemy_tint = tints[rng.range(0, 5)];
    t.enemy_tint_amount = key <= 1 ? 0.0f : rng.uniform(0.15f, 0.35f);
    return t;
}

inline Color lock_color(LockKind k) {
    switch (k) {
    case LockKind::Ice:
        return {150, 214, 245, 255};
    case LockKind::Crate:
        return {196, 140, 80, 255};
    case LockKind::Stone:
        return {170, 164, 156, 255};
    case LockKind::Cage:
        return {150, 156, 170, 255};
    case LockKind::Crystal:
        return {230, 70, 110, 255};
    }
    return {255, 255, 255, 255};
}

inline Color tint_enemy(const Theme& t, Color c) {
    return blend(c, t.enemy_tint, t.enemy_tint_amount);
}

enum class BossPower : uint8_t { Summoner, Charger, Shielded, Splitter, Barrage, Healer, Enrage, Count };

inline const char* boss_power_name(BossPower p) {
    static const char* names[] = {"SUMMONER", "CHARGER", "SHIELDED", "SPLITTER", "BARRAGE", "HEALER", "ENRAGE"};
    return names[static_cast<size_t>(p)];
}

// A boss is grown from its stage seed: silhouette, colours, face, armour and a set of powers.
struct BossGenome {
    std::string name;
    Color body{};
    Color accent{};
    int horns{2};
    int eyes{2};
    int spikes{0};
    bool plates{false};
    bool crown{true};
    float girth{1.0f};
    std::array<BossPower, 3> powers{};
    int power_count{1};

    bool has(BossPower p) const {
        for (int i = 0; i < power_count; ++i) {
            if (powers[static_cast<size_t>(i)] == p) return true;
        }
        return false;
    }
};

inline BossGenome make_boss(int key, bool mini) {
    Rng rng{static_cast<uint64_t>(key) * 0x9E3779B1ull + (mini ? 77ull : 991ull)};
    BossGenome g;
    static const char* first[] = {"GRUK",  "MORGASH", "VEXX",  "THRALL", "ZORBO",  "KRAGG",
                                  "ULVAR", "BLORT",   "SKARN", "NUXX",   "GOROTH", "MAWL"};
    static const char* titles[] = {"THE ROTTEN", "THE UNDYING",         "BONECRUSHER", "THE VAST", "WALLBREAKER",
                                   "THE HUNGRY", "OF A THOUSAND TEETH", "THE BLIGHT",  "IRONHIDE", "THE ENDLESS"};
    g.name = std::string(first[rng.range(0, 11)]) + " " + titles[rng.range(0, 9)];
    static const Color bodies[] = {{150, 60, 60, 255},  {90, 120, 70, 255},  {110, 80, 150, 255},
                                   {70, 110, 140, 255}, {160, 110, 60, 255}, {80, 80, 90, 255}};
    g.body = bodies[rng.range(0, 5)];
    static const Color accents[] = {
        {255, 205, 60, 255}, {120, 230, 255, 255}, {255, 90, 60, 255}, {180, 255, 120, 255}};
    g.accent = accents[rng.range(0, 3)];
    g.horns = rng.range(0, 4);
    g.eyes = rng.range(1, 3);
    g.spikes = rng.range(0, 6);
    g.plates = rng.chance(0.5f);
    g.crown = rng.chance(0.6f);
    g.girth = rng.uniform(0.9f, 1.15f);
    g.power_count = mini ? 1 : rng.range(2, 3);
    std::array<bool, static_cast<size_t>(BossPower::Count)> used{};
    for (int i = 0; i < g.power_count; ++i) {
        BossPower p;
        do {
            p = static_cast<BossPower>(rng.range(0, static_cast<int>(BossPower::Count) - 1));
        } while (used[static_cast<size_t>(p)]);
        used[static_cast<size_t>(p)] = true;
        g.powers[static_cast<size_t>(i)] = p;
    }
    return g;
}

} // namespace ls
