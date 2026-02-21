#include "factory/enemy_factory.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ls;
using Catch::Matchers::WithinAbs;

TEST_CASE("Enemy base stats at scaling=1.0", "[enemy]") {
    auto grunt = get_enemy_stats(EnemyType::Grunt, 1.0f);
    CHECK(grunt.hp == 80);
    CHECK_THAT(grunt.speed, WithinAbs(60.0, 0.01));
    CHECK(grunt.armor == 0);
    CHECK(grunt.reward == 10);

    auto runner = get_enemy_stats(EnemyType::Runner, 1.0f);
    CHECK(runner.hp == 45);
    CHECK_THAT(runner.speed, WithinAbs(120.0, 0.01));

    auto tank = get_enemy_stats(EnemyType::Tank, 1.0f);
    CHECK(tank.hp == 300);
    CHECK(tank.armor == 6);

    auto boss = get_enemy_stats(EnemyType::Boss, 1.0f);
    CHECK(boss.hp == 1500);
    CHECK(boss.armor == 10);
}

TEST_CASE("Enemy HP scales with scaling factor", "[enemy]") {
    auto base = get_enemy_stats(EnemyType::Grunt, 1.0f);
    auto scaled = get_enemy_stats(EnemyType::Grunt, 2.0f);
    CHECK(scaled.hp == base.hp * 2);
}

TEST_CASE("Enemy speed does not scale", "[enemy]") {
    auto base = get_enemy_stats(EnemyType::Grunt, 1.0f);
    auto scaled = get_enemy_stats(EnemyType::Grunt, 3.0f);
    CHECK_THAT(scaled.speed, WithinAbs(base.speed, 0.01));
}

TEST_CASE("get_boss_ability per wave range", "[enemy]") {
    CHECK(get_boss_ability(1) == AbilityType::SpeedBurst);
    CHECK(get_boss_ability(5) == AbilityType::SpeedBurst);
    CHECK(get_boss_ability(6) == AbilityType::SpawnMinions);
    CHECK(get_boss_ability(10) == AbilityType::SpawnMinions);
    CHECK(get_boss_ability(11) == AbilityType::DamageAura);
    CHECK(get_boss_ability(30) == AbilityType::DamageAura);
}
