#include "managers/tower_registry.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

TEST_CASE("All tower types exist at levels 1-3", "[tower_reg]") {
    TowerRegistry reg;
    TowerType types[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                         TowerType::Lightning, TowerType::Poison, TowerType::Laser};
    for (auto t : types) {
        for (int lvl = 1; lvl <= 3; ++lvl) {
            auto& stats = reg.get(t, lvl);
            CHECK(stats.type == t);
            CHECK(stats.level == lvl);
        }
    }
}

TEST_CASE("Cost increases with level for all towers", "[tower_reg]") {
    TowerRegistry reg;
    TowerType types[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                         TowerType::Lightning, TowerType::Poison, TowerType::Laser};
    for (auto t : types) {
        CHECK(reg.get(t, 2).cost > reg.get(t, 1).cost);
        CHECK(reg.get(t, 3).cost > reg.get(t, 2).cost);
    }
}

TEST_CASE("Arrow L1 cost=50 damage=15", "[tower_reg]") {
    TowerRegistry reg;
    auto& a1 = reg.get(TowerType::Arrow, 1);
    CHECK(a1.cost == 50);
    CHECK(a1.damage == 15);
}

TEST_CASE("upgrade_cost at max level returns 0", "[tower_reg]") {
    TowerRegistry reg;
    TowerType types[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                         TowerType::Lightning, TowerType::Poison, TowerType::Laser};
    for (auto t : types) {
        CHECK(reg.upgrade_cost(t, 3) == 0);
    }
}

TEST_CASE("upgrade_cost at L1 equals L2 cost", "[tower_reg]") {
    TowerRegistry reg;
    TowerType types[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                         TowerType::Lightning, TowerType::Poison, TowerType::Laser};
    for (auto t : types) {
        CHECK(reg.upgrade_cost(t, 1) == reg.get(t, 2).cost);
    }
}
