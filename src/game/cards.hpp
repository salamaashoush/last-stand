#pragma once
#include "core/rng.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace ls {

enum class CardId : uint8_t {
    Multishot,
    RapidFire,
    HeavyRounds,
    Piercing,
    Ricochet,
    Incendiary,
    Cryo,
    Explosive,
    Critical,
    ChainLightning,
    Airstrike,
    Reinforcements,
    Fortify,
    Plunder,
    GateMaster,
    CallIn,
    BulletStorm,
    Inferno,
    ThunderGod,
    Shatter,
    Count
};

inline constexpr int CARD_COUNT = static_cast<int>(CardId::Count);

enum class Rarity : uint8_t { Common, Rare, Epic, Evolution };

struct Requirement {
    CardId card;
    int level;
};

struct CardDef {
    CardId id;
    const char* name;
    const char* desc;
    int max_level; // 0 = unlimited
    int weight;
    Rarity rarity;
    std::array<Requirement, 2> needs{{{CardId::Count, 0}, {CardId::Count, 0}}};
};

inline constexpr std::array<CardDef, CARD_COUNT> CARD_DEFS{{
    {CardId::Multishot, "MULTISHOT", "+1 bullet\nper shot", 5, 7, Rarity::Rare},
    {CardId::RapidFire, "RAPID FIRE", "+25% fire\nrate", 6, 10, Rarity::Common},
    {CardId::HeavyRounds, "HEAVY ROUNDS", "+35% bullet\ndamage", 8, 10, Rarity::Common},
    {CardId::Piercing, "PIERCING", "Bullets pass\nthrough +1", 4, 7, Rarity::Rare},
    {CardId::Ricochet, "RICOCHET", "Bullets bounce\nto +1 target", 3, 5, Rarity::Rare},
    {CardId::Incendiary, "INCENDIARY", "Hits set\nenemies ablaze", 4, 6, Rarity::Rare},
    {CardId::Cryo, "CRYO ROUNDS", "Hits slow\nenemies", 3, 6, Rarity::Common},
    {CardId::Explosive, "EXPLOSIVE", "Bullets burst\non impact", 4, 4, Rarity::Epic},
    {CardId::Critical, "DEADEYE", "+12% crit,\ncrits x3", 4, 7, Rarity::Common},
    {CardId::ChainLightning, "STORMCALLER", "Lightning arcs\nthrough hordes", 5, 4, Rarity::Epic},
    {CardId::Airstrike, "BOMBER PILOT", "Stronger, faster\nairstrikes", 4, 5, Rarity::Rare},
    {CardId::Reinforcements, "REINFORCEMENTS", "+6 soldiers\nright now", 0, 8, Rarity::Common},
    {CardId::Fortify, "FORTIFY", "+30% wall HP\nand full repair", 4, 6, Rarity::Common},
    {CardId::Plunder, "PLUNDER", "+50% coins\nfrom kills", 3, 5, Rarity::Common},
    {CardId::GateMaster, "GATE MASTER", "Gates grow\ntwice as fast", 2, 4, Rarity::Rare},
    {CardId::CallIn, "CALL IN", "A new unit type\njoins the squad", 3, 6, Rarity::Epic},
    {CardId::BulletStorm,
     "BULLET STORM",
     "+2 bullets and\n+50% fire rate",
     1,
     30,
     Rarity::Evolution,
     {{{CardId::Multishot, 3}, {CardId::RapidFire, 3}}}},
    {CardId::Inferno,
     "INFERNO",
     "Burning foes\nexplode in fire",
     1,
     30,
     Rarity::Evolution,
     {{{CardId::Incendiary, 2}, {CardId::Explosive, 1}}}},
    {CardId::ThunderGod,
     "THUNDER GOD",
     "Lightning crits\nand forks twice",
     1,
     30,
     Rarity::Evolution,
     {{{CardId::ChainLightning, 2}, {CardId::Critical, 2}}}},
    {CardId::Shatter,
     "SHATTER",
     "Loot foes burst\ninto ice shards",
     1,
     30,
     Rarity::Evolution,
     {{{CardId::Cryo, 2}, {CardId::Piercing, 2}}}},
}};

inline const CardDef& card_def(CardId id) {
    return CARD_DEFS[static_cast<size_t>(id)];
}

struct RunStats {
    int multishot{1};
    float fire_rate{2.0f};
    float damage{10.0f};
    int pierce{0};
    int ricochet{0};
    float burn_ratio{0.0f};
    float slow{0.0f};
    float explosive_radius{0.0f};
    float crit_chance{0.05f};
    float crit_mult{2.0f};
    int chain_level{0};
    int airstrike_level{1};
    float coin_mult{1.0f};
    float gate_growth{1.0f};
    float wall_max{1000.0f};
    bool inferno{false};
    bool thunder_god{false};
    bool shatter{false};
    std::array<int, CARD_COUNT> card_levels{};

    int level_of(CardId id) const { return card_levels[static_cast<size_t>(id)]; }

    float airstrike_cooldown() const { return std::max(8.0f, 26.0f - 4.0f * static_cast<float>(airstrike_level - 1)); }
    int airstrike_bombs() const { return 6 + 3 * (airstrike_level - 1); }
    float airstrike_damage() const { return damage * (8.0f + 4.0f * static_cast<float>(airstrike_level - 1)); }
    float chain_interval() const { return std::max(0.35f, 1.4f - 0.2f * static_cast<float>(chain_level)); }
    int chain_targets() const { return (3 + 2 * chain_level) * (thunder_god ? 2 : 1); }
    float burn_dps() const { return damage * burn_ratio; }
};

struct CardEffect {
    int soldiers{0};
    float repair_fraction{0.0f};
    bool call_in{false};
};

inline bool card_available(const RunStats& s, CardId id) {
    const auto& def = card_def(id);
    if (def.max_level != 0 && s.level_of(id) >= def.max_level) return false;
    for (const auto& req : def.needs) {
        if (req.card != CardId::Count && s.level_of(req.card) < req.level) return false;
    }
    return true;
}

inline CardEffect apply_card(RunStats& s, CardId id) {
    CardEffect fx;
    s.card_levels[static_cast<size_t>(id)]++;
    switch (id) {
    case CardId::Multishot:
        s.multishot += 1;
        break;
    case CardId::RapidFire:
        s.fire_rate *= 1.25f;
        break;
    case CardId::HeavyRounds:
        s.damage *= 1.35f;
        break;
    case CardId::Piercing:
        s.pierce += 1;
        break;
    case CardId::Ricochet:
        s.ricochet += 1;
        break;
    case CardId::Incendiary:
        s.burn_ratio += 0.6f;
        break;
    case CardId::Cryo:
        s.slow = std::min(0.6f, s.slow + 0.2f);
        break;
    case CardId::Explosive:
        s.explosive_radius += 26.0f;
        break;
    case CardId::Critical:
        s.crit_chance += 0.12f;
        s.crit_mult = 3.0f;
        break;
    case CardId::ChainLightning:
        s.chain_level += 1;
        break;
    case CardId::Airstrike:
        s.airstrike_level += 1;
        break;
    case CardId::Reinforcements:
        fx.soldiers = 6;
        break;
    case CardId::Fortify:
        s.wall_max *= 1.3f;
        fx.repair_fraction = 1.0f;
        break;
    case CardId::Plunder:
        s.coin_mult += 0.5f;
        break;
    case CardId::GateMaster:
        s.gate_growth *= 2.0f;
        break;
    case CardId::CallIn:
        fx.call_in = true;
        fx.soldiers = 4;
        break;
    case CardId::BulletStorm:
        s.multishot += 2;
        s.fire_rate *= 1.5f;
        break;
    case CardId::Inferno:
        s.inferno = true;
        s.burn_ratio += 0.4f;
        break;
    case CardId::ThunderGod:
        s.thunder_god = true;
        break;
    case CardId::Shatter:
        s.shatter = true;
        s.slow = std::min(0.7f, s.slow + 0.1f);
        break;
    case CardId::Count:
        break;
    }
    return fx;
}

inline std::array<CardId, 3> roll_cards(const RunStats& s, Rng& rng) {
    std::array<CardId, 3> picked{CardId::Reinforcements, CardId::Reinforcements, CardId::Reinforcements};
    std::array<bool, CARD_COUNT> taken{};
    for (int slot = 0; slot < 3; ++slot) {
        int total = 0;
        for (const auto& def : CARD_DEFS) {
            if (!taken[static_cast<size_t>(def.id)] && card_available(s, def.id)) total += def.weight;
        }
        if (total == 0) break;
        int roll = rng.range(0, total - 1);
        for (const auto& def : CARD_DEFS) {
            if (taken[static_cast<size_t>(def.id)] || !card_available(s, def.id)) continue;
            roll -= def.weight;
            if (roll < 0) {
                picked[static_cast<size_t>(slot)] = def.id;
                taken[static_cast<size_t>(def.id)] = true;
                break;
            }
        }
    }
    return picked;
}

} // namespace ls
