#include "core/constants.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

TEST_CASE("TILE_SIZE is positive", "[constants]") {
    CHECK(TILE_SIZE > 0);
}

TEST_CASE("MAX_WAVES is positive", "[constants]") {
    CHECK(MAX_WAVES > 0);
}

TEST_CASE("STARTING_LIVES is positive", "[constants]") {
    CHECK(STARTING_LIVES > 0);
}

TEST_CASE("Grid dimensions are positive", "[constants]") {
    CHECK(GRID_COLS > 0);
    CHECK(GRID_ROWS > 0);
}

TEST_CASE("Hero constants are sane", "[constants]") {
    CHECK(HERO_SPEED > 0.0f);
    CHECK(HERO_BASE_HP > 0);
    CHECK(HERO_BASE_DAMAGE > 0);
    CHECK(HERO_ATTACK_RANGE > 0.0f);
}
