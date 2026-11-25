#include "game/meta.hpp"
#include "game/save.hpp"
#include "game/theme.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

TEST_CASE("upgrades need gems and stop at their cap", "[meta]") {
    MetaProgress m;
    CHECK_FALSE(m.buy(MetaId::Recruits));
    m.gems = 1000000;
    while (m.buy(MetaId::Recruits)) {
    }
    CHECK(m.level(MetaId::Recruits) == meta_def(MetaId::Recruits).max_level);
    CHECK(m.starting_soldiers() == 5 + 2 * meta_def(MetaId::Recruits).max_level);
}

TEST_CASE("unlocked units join the squad, and the last one cannot be benched", "[meta]") {
    MetaProgress m;
    m.gems = 100000;
    m.unlocked_stage = 5;
    CHECK(m.squad() == std::vector<UnitType>{UnitType::Rifleman});
    m.toggle_loadout(UnitType::Rifleman);
    CHECK(m.squad().size() == 1);
    CHECK(m.buy_unit(UnitType::Gunner));
    CHECK(m.in_loadout(UnitType::Gunner));
    CHECK(m.upgrade_unit(UnitType::Gunner));
    CHECK(m.unit_level(UnitType::Gunner) == 2);
}

TEST_CASE("missions pay out tier by tier", "[meta]") {
    MetaProgress m;
    m.record(Lifetime::Kills, 600);
    CHECK(m.mission_claimable(0));
    int first = m.claim_mission(0);
    CHECK(first == MISSION_DEFS[0].base_reward);
    CHECK_FALSE(m.mission_claimable(0));
    m.record(Lifetime::MostSoldiers, 50);
    m.record(Lifetime::MostSoldiers, 20);
    CHECK(m.stat(Lifetime::MostSoldiers) == 50);
}

TEST_CASE("the daily streak grows on consecutive days and resets after a gap", "[meta]") {
    MetaProgress m;
    CHECK(m.claim_daily(100) == MetaProgress::daily_reward_for(0));
    CHECK(m.claim_daily(100) == 0);
    CHECK(m.claim_daily(101) == MetaProgress::daily_reward_for(1));
    CHECK(m.claim_daily(105) == MetaProgress::daily_reward_for(0));
}

TEST_CASE("progress survives a save round trip", "[save]") {
    MetaProgress m;
    m.gems = 321;
    m.unlocked_stage = 4;
    m.levels[static_cast<size_t>(MetaId::Firepower)] = 3;
    m.unit_levels[static_cast<size_t>(UnitType::Sniper)] = 2;
    m.loadout = {0, 3, -1};
    m.stars[2] = 3;
    m.record(Lifetime::Kills, 999);
    auto back = deserialize_meta(serialize_meta(m));
    CHECK(back.gems == 321);
    CHECK(back.unlocked_stage == 4);
    CHECK(back.level(MetaId::Firepower) == 3);
    CHECK(back.unit_level(UnitType::Sniper) == 2);
    CHECK(back.loadout == m.loadout);
    CHECK(back.stars[2] == 3);
    CHECK(back.stat(Lifetime::Kills) == 999);
}

TEST_CASE("corrupt or hostile saves fall back to safe defaults", "[save]") {
    CHECK(deserialize_meta("not json").gems == 0);
    auto m = deserialize_meta(
        R"({"gems": -50, "unlocked_stage": 99, "loadout": [7, 4, 2], "unit_levels": [0, 0, 0, 0, 0, 0]})");
    CHECK(m.gems == 0);
    CHECK(m.unlocked_stage == MAX_STAGE);
    CHECK(m.unit_level(UnitType::Rifleman) == 1);
    CHECK(m.squad() == std::vector<UnitType>{UnitType::Rifleman});
}

TEST_CASE("themes and bosses are deterministic per stage", "[theme]") {
    for (int key = 1; key <= 15; ++key) {
        auto a = make_theme(key);
        auto b = make_theme(key);
        CHECK(a.name == b.name);
        CHECK(a.lock == b.lock);
        CHECK(a.biome == (key - 1) % 4);
        auto boss = make_boss(key, false);
        CHECK(boss.name == make_boss(key, false).name);
        CHECK(boss.power_count >= 2);
        CHECK(make_boss(key, true).power_count == 1);
    }
    CHECK(make_theme(1).night == 0.0f);
}
