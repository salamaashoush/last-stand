#include "factory/tower_factory.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

TEST_CASE("Tower HP base values per type", "[tower_hp]") {
    CHECK(tower_max_hp(TowerType::Arrow, 1) == 80);
    CHECK(tower_max_hp(TowerType::Cannon, 1) == 150);
    CHECK(tower_max_hp(TowerType::Ice, 1) == 90);
    CHECK(tower_max_hp(TowerType::Lightning, 1) == 100);
    CHECK(tower_max_hp(TowerType::Poison, 1) == 90);
    CHECK(tower_max_hp(TowerType::Laser, 1) == 120);
}

TEST_CASE("Tower HP increases +30 per level", "[tower_hp]") {
    TowerType types[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                         TowerType::Lightning, TowerType::Poison, TowerType::Laser};
    for (auto t : types) {
        int hp1 = tower_max_hp(t, 1);
        int hp2 = tower_max_hp(t, 2);
        int hp3 = tower_max_hp(t, 3);
        CHECK(hp2 - hp1 == 30);
        CHECK(hp3 - hp2 == 30);
    }
}
