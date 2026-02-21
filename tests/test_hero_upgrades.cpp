#include "core/hero_upgrades.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ls;
using Catch::Matchers::WithinAbs;

TEST_CASE("HeroUpgrades bonus_range scales with level", "[hero]") {
    HeroUpgrades u{};
    CHECK_THAT(u.bonus_range(), WithinAbs(0.0, 0.01));
    u.attack_range_level = 3;
    CHECK_THAT(u.bonus_range(), WithinAbs(90.0, 0.01));
    u.attack_range_level = 5;
    CHECK_THAT(u.bonus_range(), WithinAbs(150.0, 0.01));
}

TEST_CASE("HeroUpgrades bonus_pickup scales with level", "[hero]") {
    HeroUpgrades u{};
    u.magnet_level = 2;
    CHECK_THAT(u.bonus_pickup(), WithinAbs(80.0, 0.01));
}

TEST_CASE("HeroUpgrades bonus_damage scales with level", "[hero]") {
    HeroUpgrades u{};
    u.attack_damage_level = 4;
    CHECK(u.bonus_damage() == 20);
}

TEST_CASE("HeroUpgrades bonus_cooldown scales with level", "[hero]") {
    HeroUpgrades u{};
    u.attack_speed_level = 5;
    CHECK_THAT(u.bonus_cooldown(), WithinAbs(0.2, 0.001));
}

TEST_CASE("HeroUpgrades bonus_hp scales with level", "[hero]") {
    HeroUpgrades u{};
    u.max_hp_level = 3;
    CHECK(u.bonus_hp() == 120);
}

TEST_CASE("HeroUpgrades cost() valid and out-of-bounds", "[hero]") {
    HeroUpgrades u{};
    CHECK(u.cost(0) == 100);
    CHECK(u.cost(1) == 200);
    CHECK(u.cost(4) == 1000);
    CHECK(u.cost(5) == 0);
    CHECK(u.cost(-1) == 0);
}
