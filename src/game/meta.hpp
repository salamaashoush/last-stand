#pragma once
#include "game/cards.hpp"
#include "game/units.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ls {

enum class MetaId : uint8_t { Recruits, Firepower, RateOfFire, Fortress, WarChest, Lucky, Count };

inline constexpr int META_COUNT = static_cast<int>(MetaId::Count);

struct MetaDef {
    MetaId id;
    const char* name;
    const char* desc;
    int max_level;
    int base_cost;
};

inline constexpr std::array<MetaDef, META_COUNT> META_DEFS{{
    {MetaId::Recruits, "RECRUITS", "+2 starting soldiers", 10, 20},
    {MetaId::Firepower, "FIREPOWER", "+12% bullet damage", 15, 25},
    {MetaId::RateOfFire, "TRIGGER DRILL", "+8% fire rate", 15, 25},
    {MetaId::Fortress, "FORTRESS", "+20% wall HP", 10, 20},
    {MetaId::WarChest, "WAR CHEST", "+40 starting coins", 8, 15},
    {MetaId::Lucky, "LUCKY CHARM", "+1 card reroll per run", 3, 60},
}};

inline const MetaDef& meta_def(MetaId id) {
    return META_DEFS[static_cast<size_t>(id)];
}

enum class Lifetime : uint8_t {
    Kills,
    BossKills,
    EliteKills,
    GatesTaken,
    MostSoldiers,
    CardsTaken,
    TurretsBuilt,
    StagesCleared,
    BestEndlessWave,
    CoinsEarned,
    Count
};

inline constexpr int LIFETIME_COUNT = static_cast<int>(Lifetime::Count);

// Running totals add; records keep the best value seen.
inline constexpr bool lifetime_is_record(Lifetime l) {
    return l == Lifetime::MostSoldiers || l == Lifetime::StagesCleared || l == Lifetime::BestEndlessWave;
}

struct MissionDef {
    Lifetime stat;
    const char* name;
    std::array<long long, 5> tiers;
    int base_reward;
};

inline constexpr std::array<MissionDef, 10> MISSION_DEFS{{
    {Lifetime::Kills, "ZOMBIE SLAYER", {500, 2500, 10000, 40000, 150000}, 25},
    {Lifetime::BossKills, "GIANT KILLER", {1, 5, 15, 40, 100}, 40},
    {Lifetime::EliteKills, "ELITE HUNTER", {5, 25, 100, 300, 1000}, 30},
    {Lifetime::GatesTaken, "GATE CRASHER", {10, 50, 200, 600, 2000}, 25},
    {Lifetime::MostSoldiers, "ARMY OF MANY", {30, 80, 200, 500, 999}, 35},
    {Lifetime::CardsTaken, "COLLECTOR", {20, 100, 300, 800, 2000}, 25},
    {Lifetime::TurretsBuilt, "ENGINEER", {5, 25, 80, 200, 500}, 25},
    {Lifetime::StagesCleared, "CONQUEROR", {1, 3, 6, 10, 15}, 60},
    {Lifetime::BestEndlessWave, "ENDLESS", {10, 20, 30, 45, 60}, 60},
    {Lifetime::CoinsEarned, "TREASURER", {1000, 5000, 20000, 80000, 250000}, 25},
}};

inline constexpr int MISSION_COUNT = static_cast<int>(MISSION_DEFS.size());
inline constexpr int MAX_STAGE = 15;

struct MetaProgress {
    int gems{0};
    int unlocked_stage{1};
    std::array<int, META_COUNT> levels{};
    std::array<int, MAX_STAGE + 1> stars{};
    std::array<int, UNIT_COUNT> unit_levels{1, 0, 0, 0, 0, 0};
    std::array<int, LOADOUT_SLOTS> loadout{0, -1, -1};
    std::array<long long, LIFETIME_COUNT> lifetime{};
    std::array<int, MISSION_COUNT> missions_claimed{};
    long long last_daily_day{-1};
    int daily_streak{0};
    float music_volume{0.5f};
    float sfx_volume{0.8f};
    bool tutorial_done{false};

    int level(MetaId id) const { return levels[static_cast<size_t>(id)]; }

    int cost(MetaId id) const {
        const auto& d = meta_def(id);
        return static_cast<int>(std::round(d.base_cost * std::pow(1.55, level(id))));
    }

    bool maxed(MetaId id) const { return level(id) >= meta_def(id).max_level; }
    bool can_buy(MetaId id) const { return !maxed(id) && gems >= cost(id); }

    bool buy(MetaId id) {
        if (!can_buy(id)) return false;
        gems -= cost(id);
        levels[static_cast<size_t>(id)]++;
        return true;
    }

    int unit_level(UnitType u) const { return unit_levels[static_cast<size_t>(u)]; }
    bool owns(UnitType u) const { return unit_level(u) > 0; }

    int loadout_slots() const { return unlocked_stage >= 5 ? 3 : unlocked_stage >= 3 ? 2 : 1; }

    bool in_loadout(UnitType u) const {
        for (int i = 0; i < loadout_slots(); ++i) {
            if (loadout[static_cast<size_t>(i)] == static_cast<int>(u)) return true;
        }
        return false;
    }

    bool buy_unit(UnitType u) {
        int c = unit_def(u).unlock_cost;
        if (owns(u) || gems < c) return false;
        gems -= c;
        unit_levels[static_cast<size_t>(u)] = 1;
        toggle_loadout(u);
        return true;
    }

    bool can_upgrade_unit(UnitType u) const {
        return owns(u) && unit_level(u) < UNIT_MAX_LEVEL && gems >= unit_upgrade_cost(unit_level(u));
    }

    bool upgrade_unit(UnitType u) {
        if (!can_upgrade_unit(u)) return false;
        gems -= unit_upgrade_cost(unit_level(u));
        unit_levels[static_cast<size_t>(u)]++;
        return true;
    }

    // Tapping a fielded unit benches it (never the last one); tapping a benched unit fields it,
    // taking the first free slot or replacing the last slot when the squad is full.
    void toggle_loadout(UnitType u) {
        if (!owns(u)) return;
        int n = loadout_slots();
        int id = static_cast<int>(u);
        int fielded = 0;
        for (int i = 0; i < n; ++i) fielded += loadout[static_cast<size_t>(i)] >= 0 ? 1 : 0;
        for (int i = 0; i < n; ++i) {
            if (loadout[static_cast<size_t>(i)] == id) {
                if (fielded > 1) loadout[static_cast<size_t>(i)] = -1;
                return;
            }
        }
        for (int i = 0; i < n; ++i) {
            if (loadout[static_cast<size_t>(i)] < 0) {
                loadout[static_cast<size_t>(i)] = id;
                return;
            }
        }
        loadout[static_cast<size_t>(n - 1)] = id;
    }

    std::vector<UnitType> squad() const {
        std::vector<UnitType> out;
        for (int i = 0; i < loadout_slots(); ++i) {
            int id = loadout[static_cast<size_t>(i)];
            if (id >= 0 && id < UNIT_COUNT && owns(static_cast<UnitType>(id))) out.push_back(static_cast<UnitType>(id));
        }
        if (out.empty()) out.push_back(UnitType::Rifleman);
        return out;
    }

    long long stat(Lifetime l) const { return lifetime[static_cast<size_t>(l)]; }

    void record(Lifetime l, long long v) {
        auto& slot = lifetime[static_cast<size_t>(l)];
        slot = lifetime_is_record(l) ? std::max(slot, v) : slot + v;
    }

    int mission_tier(int i) const { return missions_claimed[static_cast<size_t>(i)]; }

    bool mission_claimable(int i) const {
        int t = mission_tier(i);
        if (t >= 5) return false;
        const auto& m = MISSION_DEFS[static_cast<size_t>(i)];
        return stat(m.stat) >= m.tiers[static_cast<size_t>(t)];
    }

    int mission_reward(int i) const { return MISSION_DEFS[static_cast<size_t>(i)].base_reward * (mission_tier(i) + 1); }

    int claim_mission(int i) {
        if (!mission_claimable(i)) return 0;
        int r = mission_reward(i);
        gems += r;
        missions_claimed[static_cast<size_t>(i)]++;
        return r;
    }

    int claimable_missions() const {
        int n = 0;
        for (int i = 0; i < MISSION_COUNT; ++i) n += mission_claimable(i) ? 1 : 0;
        return n;
    }

    bool daily_available(long long today) const { return today != last_daily_day; }

    static int daily_reward_for(int streak) { return 20 + 10 * std::min(streak, 6); }

    // Days are whole days since the epoch; missing one resets the streak.
    int claim_daily(long long today) {
        if (!daily_available(today)) return 0;
        daily_streak = (last_daily_day == today - 1) ? daily_streak + 1 : 0;
        last_daily_day = today;
        int r = daily_reward_for(daily_streak);
        gems += r;
        return r;
    }

    int total_stars() const {
        int s = 0;
        for (int v : stars) s += v;
        return s;
    }

    void finish_stage(int stage, int earned_stars) {
        if (stage < 1 || stage > MAX_STAGE) return;
        auto& s = stars[static_cast<size_t>(stage)];
        s = std::max(s, earned_stars);
        unlocked_stage = std::max(unlocked_stage, std::min(stage + 1, MAX_STAGE));
        record(Lifetime::StagesCleared, stage);
    }

    bool endless_unlocked() const { return stat(Lifetime::StagesCleared) >= 3; }

    int starting_soldiers() const { return 5 + 2 * level(MetaId::Recruits); }
    int starting_coins() const { return 40 * level(MetaId::WarChest); }
    int rerolls() const { return level(MetaId::Lucky); }

    RunStats initial_stats() const {
        RunStats s;
        s.damage *= 1.0f + 0.12f * static_cast<float>(level(MetaId::Firepower));
        s.fire_rate *= 1.0f + 0.08f * static_cast<float>(level(MetaId::RateOfFire));
        s.wall_max *= 1.0f + 0.2f * static_cast<float>(level(MetaId::Fortress));
        return s;
    }
};

inline int stars_for_wall(float wall_ratio) {
    if (wall_ratio >= 0.75f) return 3;
    if (wall_ratio >= 0.4f) return 2;
    return 1;
}

} // namespace ls
