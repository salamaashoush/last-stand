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
    m.loadout = {0, 3, -1, -1};
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
        R"({"gems": -50, "unlocked_stage": 99, "loadout": [9, 4, 2], "unit_levels": [0, 0, 0, 0, 0, 0, 0, 0]})");
    CHECK(m.gems == 0);
    CHECK(m.unlocked_stage == MAX_STAGE);
    CHECK(m.unit_level(UnitType::Rifleman) == 1);
    CHECK(m.squad() == std::vector<UnitType>{UnitType::Rifleman});
}

TEST_CASE("themes and bosses are deterministic and handcrafted per stage", "[theme]") {
    std::vector<std::string> stage_names;
    std::vector<std::string> boss_names;
    for (int key = 1; key <= MAX_STAGE; ++key) {
        auto a = make_theme(key);
        auto b = make_theme(key);
        CHECK(a.name == b.name);
        CHECK(a.lock == b.lock);
        CHECK(a.biome >= 0);
        CHECK(a.biome < BIOME_COUNT);
        CHECK_FALSE(a.tagline.empty());
        CHECK(std::ranges::find(stage_names, a.name) == stage_names.end());
        stage_names.push_back(a.name);
        auto boss = make_boss(key, false);
        CHECK(boss.name == make_boss(key, false).name);
        CHECK(boss.power_count >= 2);
        CHECK(std::ranges::find(boss_names, boss.name) == boss_names.end());
        boss_names.push_back(boss.name);
        CHECK(make_boss(key, true).power_count == 1);
    }
    CHECK(make_theme(1).night == 0.0f);
    // Paced enemy introductions: Flyers debut on Stage 2, Shielders on Stage 3, Warlords on Stage 6!
    CHECK(wave_counts(1, 5).flyers == 0);
    CHECK(wave_counts(2, 5).flyers > 0);
    CHECK(wave_counts(2, 5).shielders == 0);
    CHECK(wave_counts(3, 5).shielders > 0);
    CHECK(wave_counts(5, 5).warlords == 0);
    CHECK(wave_counts(6, 5).warlords > 0);
}

TEST_CASE("squad synergies activate from unit combinations", "[meta]") {
    auto syn = compute_synergies({UnitType::Rifleman, UnitType::Gunner, UnitType::Cryo, UnitType::Volt});
    CHECK(syn.combined_arms);
    CHECK(syn.bullet_storm);
    CHECK(syn.elemental_fury);
    CHECK_FALSE(syn.heavy_ordnance);
    CHECK(syn.count() == 3);
}

TEST_CASE("clearing stages unlocks new turrets, weapons, and squad slots progressively", "[meta]") {
    MetaProgress m;
    CHECK(turret_unlocked(TurretType::Cannon, m.unlocked_stage));
    CHECK(turret_unlocked(TurretType::Flamer, m.unlocked_stage));
    CHECK_FALSE(turret_unlocked(TurretType::Tesla, m.unlocked_stage));
    CHECK_FALSE(turret_unlocked(TurretType::Missile, m.unlocked_stage));
    CHECK_FALSE(m.owns(UnitType::Gunner));

    auto r1 = m.finish_stage(1, 3);
    CHECK_FALSE(r1.empty());
    CHECK(m.unlocked_stage == 2);
    CHECK(turret_unlocked(TurretType::Tesla, m.unlocked_stage));
    CHECK(m.owns(UnitType::Gunner));
    CHECK(m.in_loadout(UnitType::Gunner));
    CHECK(m.loadout_slots() == 2);

    // Replaying Stage 1 does not duplicate first-clear unlocks
    CHECK(m.finish_stage(1, 3).empty());

    auto r2 = m.finish_stage(2, 2);
    CHECK(turret_unlocked(TurretType::Frost, m.unlocked_stage));
    CHECK(m.owns(UnitType::Shotgunner));

    auto r3 = m.finish_stage(3, 3);
    CHECK(turret_unlocked(TurretType::Laser, m.unlocked_stage));
    CHECK(m.owns(UnitType::Sniper));
    CHECK(m.in_loadout(UnitType::Sniper));
    CHECK(m.endless_unlocked());

    auto r4 = m.finish_stage(4, 3);
    CHECK(turret_unlocked(TurretType::Missile, m.unlocked_stage));
    CHECK(m.owns(UnitType::Grenadier));

    auto r5 = m.finish_stage(5, 3);
    CHECK(m.owns(UnitType::Cryo));
    CHECK(m.loadout_slots() == 4);

    auto r6 = m.finish_stage(6, 3);
    CHECK(m.owns(UnitType::Flamer));
    auto r7 = m.finish_stage(7, 3);
    CHECK(m.owns(UnitType::Volt));

    // Stage 8+ grants a free squad upgrade!
    int before_lv = m.unit_level(m.squad().front());
    auto r8 = m.finish_stage(8, 3);
    CHECK_FALSE(r8.empty());
    CHECK(m.unit_level(m.squad().front()) == before_lv + 1);
}
