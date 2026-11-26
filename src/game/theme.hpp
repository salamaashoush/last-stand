#pragma once
#include "core/rng.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace ls {

inline constexpr int BIOME_COUNT = 6;

enum class Weather : uint8_t { Clear, Snow, Rain, Leaves, Embers, Dust, Fireflies, Spores, Starlight };
enum class LockKind : uint8_t { Ice, Crate, Stone, Cage, Crystal };
enum class Decor : uint8_t { Trees, Rocks, Bones, Crystals, Ruins, Obelisks };
enum class StageHazard : uint8_t { Bountiful, Sandstorm, Permafrost, Magma, ToxicMire, Leylines };

struct StageHazardDef {
    const char* name;
    const char* short_desc;
    Color accent;
};

inline constexpr std::array<StageHazardDef, BIOME_COUNT> STAGE_HAZARD_DEFS{{
    {"BOUNTIFUL LAND", "+25% coins & bonus supply crates", {120, 220, 100, 255}},
    {"SANDSTORM", "Gusts sweep foes & boost shot range", {238, 194, 110, 255}},
    {"PERMAFROST", "Ice slicks chill & slow advancing hordes", {130, 215, 255, 255}},
    {"MAGMA FISSURES", "Lava geysers erupt & scorch the horde", {255, 115, 50, 255}},
    {"TOXIC MIRE", "Spore pods burst into armor-melting fog", {160, 235, 90, 255}},
    {"ARCANE LEYLINES", "Rune circles supercharge bullets +35%", {175, 125, 255, 255}},
}};

inline const StageHazardDef& stage_hazard_def(StageHazard h) {
    return STAGE_HAZARD_DEFS[static_cast<size_t>(h)];
}

inline const char* biome_name(int biome) {
    static const char* names[BIOME_COUNT] = {"VERDANT FRONTIER", "SCORCHED BADLANDS", "GLACIAL RIFT",
                                             "ASHEN CALDERA",    "BLIGHTED MIRE",     "ASTRAL CITADEL"};
    return names[std::clamp(biome, 0, BIOME_COUNT - 1)];
}

struct Theme {
    int key{0};
    int biome{0};
    std::string name;
    std::string tagline;
    Color ground{};
    Color dark{};
    Color light{};
    Color stone{};
    Color court{};
    Color fog{};
    Color accent{};
    Color enemy_tint{};
    float enemy_tint_amount{0.0f};
    Weather weather{Weather::Clear};
    LockKind lock{LockKind::Crate};
    Decor decor{Decor::Rocks};
    StageHazard hazard{StageHazard::Bountiful};
    float night{0.0f};
    int stage_twist{0}; // 0=normal, 1=Blood Moon Blitz, 2=Cursed Phalanx, 3=Hellfire Cataclysm, 4=Eclipse Reckoning
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

// Handcrafted 10-stage campaign (each with bespoke mood, palette, twist & landmark feel),
// plus deterministic procedural themes for Endless Mode (key > 10).
inline Theme make_theme(int key) {
    Theme t;
    t.key = key;
    if (key >= 1 && key <= 10) {
        switch (key) {
        case 1: // Stage 1: Sunlit Outpost — bright welcoming frontier
            t.biome = 0;
            t.name = "SUNLIT OUTPOST";
            t.tagline = "FRONTIER DEFENSE - LEARN THE WALL";
            t.ground = {96, 156, 74, 255};
            t.dark = {72, 128, 54, 255};
            t.light = {132, 184, 92, 255};
            t.stone = {170, 162, 150, 255};
            t.court = {88, 86, 80, 255};
            t.fog = {20, 38, 22, 255};
            t.accent = {140, 225, 96, 255};
            t.weather = Weather::Leaves;
            t.lock = LockKind::Crate;
            t.decor = Decor::Trees;
            t.hazard = StageHazard::Bountiful;
            t.night = 0.0f;
            return t;
        case 2: // Stage 2: Golden Dunes — aerial & tunneling desert ambush
            t.biome = 1;
            t.name = "GOLDEN DUNES";
            t.tagline = "SKY & SAND AMBUSH - FLYERS & BURROWERS";
            t.ground = {218, 186, 122, 255};
            t.dark = {190, 154, 94, 255};
            t.light = {240, 212, 154, 255};
            t.stone = {200, 170, 124, 255};
            t.court = {120, 98, 72, 255};
            t.fog = {68, 44, 18, 255};
            t.accent = {248, 198, 96, 255};
            t.enemy_tint = {235, 205, 110, 255};
            t.enemy_tint_amount = 0.14f;
            t.weather = Weather::Dust;
            t.lock = LockKind::Crate;
            t.decor = Decor::Bones;
            t.hazard = StageHazard::Sandstorm;
            t.night = 0.0f;
            return t;
        case 3: // Stage 3: Crystal Glacier — shielded frost phalanx & splitters
            t.biome = 2;
            t.name = "CRYSTAL GLACIER";
            t.tagline = "FROST PHALANX - SHIELDERS & SPLITTERS";
            t.ground = {214, 228, 244, 255};
            t.dark = {180, 200, 224, 255};
            t.light = {242, 248, 255, 255};
            t.stone = {146, 166, 190, 255};
            t.court = {76, 92, 116, 255};
            t.fog = {22, 38, 66, 255};
            t.accent = {115, 220, 255, 255};
            t.enemy_tint = {135, 215, 245, 255};
            t.enemy_tint_amount = 0.20f;
            t.weather = Weather::Snow;
            t.lock = LockKind::Ice;
            t.decor = Decor::Crystals;
            t.hazard = StageHazard::Permafrost;
            t.night = 0.0f;
            return t;
        case 4: // Stage 4: Obsidian Caldera — volatile volcanic stampede
            t.biome = 3;
            t.name = "OBSIDIAN CALDERA";
            t.tagline = "VOLCANIC STAMPEDE - EXPLODERS & BERSERKERS";
            t.ground = {84, 72, 72, 255};
            t.dark = {58, 48, 50, 255};
            t.light = {118, 94, 86, 255};
            t.stone = {104, 92, 90, 255};
            t.court = {50, 44, 46, 255};
            t.fog = {26, 8, 8, 255};
            t.accent = {255, 112, 42, 255};
            t.enemy_tint = {245, 115, 75, 255};
            t.enemy_tint_amount = 0.22f;
            t.weather = Weather::Embers;
            t.lock = LockKind::Crystal;
            t.decor = Decor::Bones;
            t.hazard = StageHazard::Magma;
            t.night = 0.14f;
            return t;
        case 5: // Stage 5: Venom Hollow — backline spitters & shaman healers
            t.biome = 4;
            t.name = "VENOM HOLLOW";
            t.tagline = "PLAGUE COVEN - SPITTERS & HEALERS";
            t.ground = {68, 96, 72, 255};
            t.dark = {46, 70, 54, 255};
            t.light = {98, 130, 86, 255};
            t.stone = {114, 126, 110, 255};
            t.court = {52, 64, 56, 255};
            t.fog = {14, 28, 18, 255};
            t.accent = {158, 238, 86, 255};
            t.enemy_tint = {150, 235, 105, 255};
            t.enemy_tint_amount = 0.20f;
            t.weather = Weather::Spores;
            t.lock = LockKind::Cage;
            t.decor = Decor::Trees;
            t.hazard = StageHazard::ToxicMire;
            t.night = 0.30f;
            return t;
        case 6: // Stage 6: Twilight Citadel — arcane warlord legion
            t.biome = 5;
            t.name = "TWILIGHT CITADEL";
            t.tagline = "ARCANE LEGION - WARLORD COMMANDERS";
            t.ground = {68, 62, 98, 255};
            t.dark = {46, 42, 74, 255};
            t.light = {98, 90, 138, 255};
            t.stone = {108, 100, 134, 255};
            t.court = {46, 42, 70, 255};
            t.fog = {14, 10, 34, 255};
            t.accent = {180, 122, 255, 255};
            t.enemy_tint = {190, 120, 240, 255};
            t.enemy_tint_amount = 0.22f;
            t.weather = Weather::Starlight;
            t.lock = LockKind::Crystal;
            t.decor = Decor::Obelisks;
            t.hazard = StageHazard::Leylines;
            t.night = 0.36f;
            return t;
        case 7: // Stage 7: Blood-Moon Forest — frantic high-speed night blitz
            t.biome = 0;
            t.name = "BLOOD-MOON WOODS";
            t.tagline = "NIGHT BLITZ - FAST HORDE & +35% HERO CHARGE";
            t.ground = {76, 98, 66, 255};
            t.dark = {52, 70, 48, 255};
            t.light = {108, 126, 82, 255};
            t.stone = {126, 110, 114, 255};
            t.court = {64, 54, 58, 255};
            t.fog = {48, 10, 18, 255};
            t.accent = {255, 78, 92, 255};
            t.enemy_tint = {255, 95, 110, 255};
            t.enemy_tint_amount = 0.28f;
            t.weather = Weather::Fireflies;
            t.lock = LockKind::Stone;
            t.decor = Decor::Ruins;
            t.hazard = StageHazard::Bountiful;
            t.night = 0.44f;
            t.stage_twist = 1;
            return t;
        case 8: // Stage 8: Pharaoh's Crypt — heavily armored desert tomb siege
            t.biome = 1;
            t.name = "PHARAOH'S CRYPT";
            t.tagline = "CURSED PHALANX - ARMORED GIANTS & RELIC GOLD";
            t.ground = {178, 134, 82, 255};
            t.dark = {144, 102, 58, 255};
            t.light = {212, 168, 108, 255};
            t.stone = {166, 134, 88, 255};
            t.court = {86, 66, 46, 255};
            t.fog = {46, 24, 10, 255};
            t.accent = {255, 185, 65, 255};
            t.enemy_tint = {245, 185, 70, 255};
            t.enemy_tint_amount = 0.26f;
            t.weather = Weather::Dust;
            t.lock = LockKind::Cage;
            t.decor = Decor::Rocks;
            t.hazard = StageHazard::Sandstorm;
            t.night = 0.30f;
            t.stage_twist = 2;
            return t;
        case 9: // Stage 9: Inferno Abyss — relentless molten cataclysm
            t.biome = 3;
            t.name = "INFERNO ABYSS";
            t.tagline = "HELLFIRE CATACLYSM - RAPID ERUPTIONS & DEMONS";
            t.ground = {56, 42, 46, 255};
            t.dark = {38, 28, 32, 255};
            t.light = {92, 62, 58, 255};
            t.stone = {78, 64, 66, 255};
            t.court = {38, 30, 34, 255};
            t.fog = {36, 6, 4, 255};
            t.accent = {255, 78, 36, 255};
            t.enemy_tint = {255, 85, 45, 255};
            t.enemy_tint_amount = 0.30f;
            t.weather = Weather::Embers;
            t.lock = LockKind::Crystal;
            t.decor = Decor::Ruins;
            t.hazard = StageHazard::Magma;
            t.night = 0.42f;
            t.stage_twist = 3;
            return t;
        case 10: // Stage 10: Eclipse Throne — epic campaign finale!
            t.biome = 5;
            t.name = "ECLIPSE THRONE";
            t.tagline = "FINAL RECKONING - THE SOVEREIGN'S LEGION";
            t.ground = {52, 42, 84, 255};
            t.dark = {34, 26, 60, 255};
            t.light = {86, 70, 126, 255};
            t.stone = {94, 82, 124, 255};
            t.court = {36, 30, 58, 255};
            t.fog = {18, 6, 36, 255};
            t.accent = {255, 205, 75, 255};
            t.enemy_tint = {225, 130, 255, 255};
            t.enemy_tint_amount = 0.25f;
            t.weather = Weather::Starlight;
            t.lock = LockKind::Crystal;
            t.decor = Decor::Obelisks;
            t.hazard = StageHazard::Leylines;
            t.night = 0.46f;
            t.stage_twist = 4;
            return t;
        }
    }

    Rng rng{static_cast<uint64_t>(key) * 2654435761ull + 0x51ED27ull};
    t.biome = (std::max(key, 1) - 1) % BIOME_COUNT;
    struct Base {
        Color ground, dark, light, stone, court, fog, accent;
    };
    static const Base bases[BIOME_COUNT] = {
        {{94, 152, 72, 255}, {72, 126, 54, 255}, {128, 178, 88, 255}, {168, 160, 150, 255}, {86, 84, 80, 255},
         {20, 36, 20, 255}, {140, 220, 96, 255}},
        {{214, 182, 120, 255}, {188, 152, 94, 255}, {236, 208, 150, 255}, {196, 166, 122, 255}, {118, 96, 72, 255},
         {64, 42, 18, 255}, {245, 195, 95, 255}},
        {{214, 226, 240, 255}, {182, 200, 222, 255}, {240, 246, 254, 255}, {146, 164, 186, 255}, {78, 92, 114, 255},
         {24, 38, 62, 255}, {115, 215, 255, 255}},
        {{82, 74, 74, 255}, {58, 52, 54, 255}, {112, 94, 88, 255}, {106, 96, 94, 255}, {52, 46, 48, 255},
         {22, 8, 8, 255}, {255, 110, 42, 255}},
        {{72, 96, 74, 255}, {50, 72, 56, 255}, {100, 128, 88, 255}, {116, 126, 112, 255}, {56, 66, 58, 255},
         {16, 28, 20, 255}, {156, 232, 86, 255}},
        {{68, 64, 96, 255}, {48, 44, 74, 255}, {98, 92, 134, 255}, {108, 102, 132, 255}, {48, 44, 68, 255},
         {14, 10, 32, 255}, {176, 120, 255, 255}},
    };
    const auto& b = bases[t.biome];
    t.ground = jitter(b.ground, rng, 12);
    t.dark = jitter(b.dark, rng, 12);
    t.light = jitter(b.light, rng, 12);
    auto neutral = [&](Color c, int amount) {
        int d = rng.range(-amount, amount);
        auto j = [d](unsigned char v) { return static_cast<unsigned char>(std::clamp(v + d, 0, 255)); };
        return Color{j(c.r), j(c.g), j(c.b), c.a};
    };
    t.stone = neutral(b.stone, 14);
    t.court = neutral(b.court, 10);
    t.fog = b.fog;
    t.accent = b.accent;
    t.hazard = static_cast<StageHazard>(t.biome);
    t.stage_twist = key > 100 ? ((key - 101) % 5) : 0;
    static const char* twist_tags[5] = {"ENDLESS REALM - ESCALATING HORDE", "ENDLESS BLITZ - +18% SPEED & +35% CHARGE",
                                        "ENDLESS PHALANX - ARMORED FOES & GOLD",
                                        "ENDLESS CATACLYSM - RAPID ERUPTIONS", "ENDLESS ECLIPSE - OVERLORD LEGION"};
    t.tagline = twist_tags[t.stage_twist];

    static const char* adjectives[BIOME_COUNT][6] = {
        {"VERDANT", "WHISPERING", "SUNLIT", "MISTY", "EMERALD", "WILD"},
        {"SCORCHED", "BLEACHED", "SHIFTING", "GOLDEN", "SUNKEN", "BLISTERING"},
        {"FROZEN", "SILENT", "BITTER", "CRYSTAL", "HOWLING", "GLACIAL"},
        {"BURNING", "ASHEN", "CINDER", "OBSIDIAN", "MOLTEN", "SCARRED"},
        {"BLIGHTED", "VENOM", "SHROUDED", "ROTTING", "SPORE", "MURKY"},
        {"ASTRAL", "TWILIGHT", "RUNIC", "ECLIPSE", "STARLIT", "VOID"},
    };
    static const char* places[BIOME_COUNT][6] = {
        {"MEADOW", "HOLLOW", "VALE", "FIELDS", "GROVE", "CROSSING"},
        {"DUNES", "MESA", "WASTES", "CANYON", "OASIS", "SANDS"},
        {"PASS", "TUNDRA", "PEAKS", "FJORD", "DRIFTS", "GLACIER"},
        {"PITS", "CALDERA", "RIDGE", "FORGE", "CRATER", "BADLANDS"},
        {"MIRE", "BOG", "FEN", "MARSH", "SWAMP", "BASIN"},
        {"CITADEL", "SANCTUM", "SPIRE", "RIFT", "BASTION", "KEEP"},
    };
    t.name = std::string(adjectives[t.biome][rng.range(0, 5)]) + " " + places[t.biome][rng.range(0, 5)];
    t.night = rng.chance(t.biome == 5 ? 0.65f : 0.30f) ? rng.uniform(0.28f, 0.48f) : 0.0f;
    switch (t.biome) {
    case 0:
        t.weather = t.night > 0.0f ? Weather::Fireflies : (rng.chance(0.55f) ? Weather::Leaves : Weather::Rain);
        t.lock = rng.chance(0.5f) ? LockKind::Crate : LockKind::Stone;
        t.decor = rng.chance(0.6f) ? Decor::Trees : Decor::Ruins;
        break;
    case 1:
        t.weather = rng.chance(0.75f) ? Weather::Dust : Weather::Clear;
        t.lock = rng.chance(0.5f) ? LockKind::Crate : LockKind::Cage;
        t.decor = rng.chance(0.5f) ? Decor::Bones : Decor::Rocks;
        break;
    case 2:
        t.weather = Weather::Snow;
        t.lock = rng.chance(0.7f) ? LockKind::Ice : LockKind::Crystal;
        t.decor = rng.chance(0.55f) ? Decor::Crystals : Decor::Rocks;
        break;
    case 3:
        t.weather = Weather::Embers;
        t.lock = rng.chance(0.5f) ? LockKind::Crystal : LockKind::Cage;
        t.decor = rng.chance(0.5f) ? Decor::Bones : Decor::Ruins;
        break;
    case 4:
        t.weather = Weather::Spores;
        t.lock = rng.chance(0.5f) ? LockKind::Cage : LockKind::Crate;
        t.decor = rng.chance(0.5f) ? Decor::Trees : Decor::Ruins;
        break;
    default:
        t.weather = Weather::Starlight;
        t.lock = LockKind::Crystal;
        t.decor = rng.chance(0.6f) ? Decor::Obelisks : Decor::Crystals;
        break;
    }
    static const Color tints[] = {{120, 220, 200, 255}, {230, 210, 90, 255},  {200, 120, 220, 255},
                                  {240, 120, 90, 255},  {120, 150, 240, 255}, {170, 230, 110, 255}};
    t.enemy_tint = tints[rng.range(0, 5)];
    t.enemy_tint_amount = rng.uniform(0.14f, 0.30f);
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
        return {230, 70, 150, 255};
    }
    return {255, 255, 255, 255};
}

inline Color tint_enemy(const Theme& t, Color c) {
    return blend(c, t.enemy_tint, t.enemy_tint_amount);
}

enum class BossPower : uint8_t {
    Summoner,
    Charger,
    Shielded,
    Splitter,
    Barrage,
    Healer,
    Enrage,
    Shockwave,
    Vortex,
    Necromancer,
    LaserSweep,
    Count
};

inline const char* boss_power_name(BossPower p) {
    static const char* names[] = {"SUMMONER", "CHARGER",   "SHIELDED", "SPLITTER",    "BARRAGE",   "HEALER",
                                  "ENRAGE",   "SHOCKWAVE", "VORTEX",   "NECROMANCER", "EYE BEAM"};
    return names[static_cast<size_t>(p)];
}

inline Color boss_power_color(BossPower p) {
    switch (p) {
    case BossPower::Summoner:
        return {190, 120, 255, 255};
    case BossPower::Charger:
        return {255, 120, 70, 255};
    case BossPower::Shielded:
        return {110, 200, 255, 255};
    case BossPower::Splitter:
        return {245, 165, 70, 255};
    case BossPower::Barrage:
        return {185, 245, 90, 255};
    case BossPower::Healer:
        return {100, 240, 140, 255};
    case BossPower::Enrage:
        return {255, 70, 60, 255};
    case BossPower::Shockwave:
        return {255, 195, 75, 255};
    case BossPower::Vortex:
        return {165, 95, 255, 255};
    case BossPower::Necromancer:
        return {110, 235, 195, 255};
    case BossPower::LaserSweep:
        return {255, 80, 160, 255};
    case BossPower::Count:
        break;
    }
    return WHITE;
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
    // 10 handcrafted signature campaign bosses (+ tailored mini-bosses), with procedural bosses for Endless (>10).
    if (key >= 1 && key <= 10) {
        auto mk = [](const char* name, Color body, Color acc, int horns, int eyes, int spikes, bool plates, bool crown,
                     float girth, std::initializer_list<BossPower> ps) {
            BossGenome g;
            g.name = name;
            g.body = body;
            g.accent = acc;
            g.horns = horns;
            g.eyes = eyes;
            g.spikes = spikes;
            g.plates = plates;
            g.crown = crown;
            g.girth = girth;
            g.power_count = 0;
            for (BossPower p : ps) {
                if (g.power_count < 3) g.powers[static_cast<size_t>(g.power_count++)] = p;
            }
            return g;
        };
        if (mini) {
            switch (key) {
            case 1:
                return mk("KRUNG THE RAIDER", {104, 134, 78, 255}, {255, 205, 65, 255}, 2, 2, 2, false, false, 0.96f,
                          {BossPower::Charger});
            case 2:
                return mk("DUSTCLAW STALKER", {176, 126, 68, 255}, {255, 185, 75, 255}, 2, 2, 3, false, false, 0.94f,
                          {BossPower::Barrage});
            case 3:
                return mk("RIMEGUARD CRUSHER", {82, 132, 168, 255}, {140, 230, 255, 255}, 2, 2, 4, true, false, 1.06f,
                          {BossPower::Shielded});
            case 4:
                return mk("MAGMA HOUND", {168, 66, 48, 255}, {255, 140, 50, 255}, 3, 2, 4, false, false, 1.00f,
                          {BossPower::Enrage});
            case 5:
                return mk("SPORE SHAMAN", {76, 122, 82, 255}, {170, 255, 95, 255}, 1, 3, 2, false, false, 0.95f,
                          {BossPower::Healer});
            case 6:
                return mk("ASTRAL WARDEN", {112, 82, 164, 255}, {210, 140, 255, 255}, 2, 1, 3, true, false, 1.02f,
                          {BossPower::Vortex});
            case 7:
                return mk("CRIMSON HOWLER", {148, 48, 64, 255}, {255, 95, 115, 255}, 3, 2, 4, false, false, 0.96f,
                          {BossPower::Charger});
            case 8:
                return mk("TOMB COLOSSUS", {156, 118, 66, 255}, {255, 215, 85, 255}, 2, 2, 4, true, true, 1.12f,
                          {BossPower::Shockwave});
            case 9:
                return mk("PYRO EXECUTIONER", {134, 46, 44, 255}, {255, 110, 45, 255}, 4, 2, 5, true, false, 1.05f,
                          {BossPower::LaserSweep});
            case 10:
                return mk("ECLIPSE HARBINGER", {92, 62, 148, 255}, {255, 210, 80, 255}, 3, 3, 5, true, true, 1.08f,
                          {BossPower::Necromancer});
            }
        }
        switch (key) {
        case 1:
            return mk("GRUK WALLBREAKER", {94, 134, 72, 255}, {255, 205, 60, 255}, 2, 2, 3, true, true, 1.08f,
                      {BossPower::Charger, BossPower::Shockwave});
        case 2:
            return mk("SKARN DUNE-RIPPER", {176, 114, 58, 255}, {255, 190, 70, 255}, 3, 2, 4, false, true, 1.02f,
                      {BossPower::Charger, BossPower::Barrage});
        case 3:
            return mk("FROSTMAW IRONHIDE", {68, 122, 162, 255}, {135, 232, 255, 255}, 2, 2, 5, true, true, 1.14f,
                      {BossPower::Shielded, BossPower::Splitter});
        case 4:
            return mk("IGNIS CINDERLORD", {166, 52, 46, 255}, {255, 130, 45, 255}, 4, 2, 5, true, true, 1.06f,
                      {BossPower::Enrage, BossPower::Shockwave, BossPower::Barrage});
        case 5:
            return mk("MORVEX PLAGUEBRINGER", {72, 118, 76, 255}, {175, 255, 95, 255}, 2, 3, 3, false, true, 1.04f,
                      {BossPower::Healer, BossPower::Summoner, BossPower::Barrage});
        case 6:
            return mk("ZORBO THE VOID-EYE", {112, 74, 166, 255}, {225, 130, 255, 255}, 2, 1, 4, true, true, 1.05f,
                      {BossPower::Vortex, BossPower::LaserSweep, BossPower::Shielded});
        case 7:
            return mk("VEXX NIGHTSTALKER", {148, 38, 56, 255}, {255, 85, 110, 255}, 3, 3, 5, false, true, 0.98f,
                      {BossPower::Charger, BossPower::Enrage, BossPower::Vortex});
        case 8:
            return mk("AMON-RAKH THE UNDYING", {166, 122, 58, 255}, {255, 218, 75, 255}, 2, 2, 4, true, true, 1.15f,
                      {BossPower::Necromancer, BossPower::Shielded, BossPower::Shockwave});
        case 9:
            return mk("GOROTH HELLFORGED", {126, 40, 40, 255}, {255, 95, 40, 255}, 4, 2, 6, true, true, 1.14f,
                      {BossPower::Enrage, BossPower::LaserSweep, BossPower::Necromancer});
        case 10:
            return mk("MALAKOR, ECLIPSE SOVEREIGN", {86, 52, 142, 255}, {255, 215, 70, 255}, 4, 3, 6, true, true, 1.18f,
                      {BossPower::Vortex, BossPower::LaserSweep, BossPower::Necromancer});
        }
    }

    Rng rng{static_cast<uint64_t>(key) * 0x9E3779B1ull + (mini ? 77ull : 991ull)};
    BossGenome g;
    static const char* first[] = {"GRUK",  "MORGASH", "VEXX",  "THRALL", "ZORBO",  "KRAGG",
                                  "ULVAR", "BLORT",   "SKARN", "NUXX",   "GOROTH", "MAWL"};
    static const char* titles[] = {"THE ROTTEN", "THE UNDYING",         "BONECRUSHER", "THE VAST", "WALLBREAKER",
                                   "THE HUNGRY", "OF A THOUSAND TEETH", "THE BLIGHT",  "IRONHIDE", "THE ENDLESS"};
    g.name = std::string(first[rng.range(0, 11)]) + " " + titles[rng.range(0, 9)];
    static const Color bodies[] = {{156, 56, 62, 255},  {84, 126, 72, 255},  {116, 78, 162, 255},
                                   {66, 112, 148, 255}, {166, 108, 56, 255}, {84, 84, 102, 255}};
    g.body = bodies[rng.range(0, 5)];
    static const Color accents[] = {
        {255, 205, 60, 255}, {120, 230, 255, 255}, {255, 90, 60, 255}, {180, 255, 120, 255}, {230, 110, 255, 255}};
    g.accent = accents[rng.range(0, 4)];
    g.horns = rng.range(1, 4);
    g.eyes = rng.range(1, 3);
    g.spikes = rng.range(2, 6);
    g.plates = rng.chance(0.55f);
    g.crown = rng.chance(0.65f);
    g.girth = rng.uniform(0.92f, 1.15f);
    g.power_count = mini ? 1 : 3;
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
