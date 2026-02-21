#include "managers/map_manager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ls;
using Catch::Matchers::WithinAbs;

static MapData make_test_map(int cols = 5, int rows = 5) {
    MapData m;
    m.name = "test";
    m.cols = cols;
    m.rows = rows;
    m.tiles.resize(rows, std::vector<TileType>(cols, TileType::Grass));
    m.spawn = {0, 0};
    m.exit_pos = {cols - 1, rows - 1};
    // Mark spawn and exit
    m.tiles[0][0] = TileType::Spawn;
    m.tiles[rows - 1][cols - 1] = TileType::Exit;
    // One buildable tile
    m.tiles[2][2] = TileType::Buildable;
    return m;
}

TEST_CASE("grid_to_world places center of tile", "[mapdata]") {
    auto m = make_test_map();
    auto w = m.grid_to_world({0, 0});
    CHECK_THAT(w.x, WithinAbs(TILE_SIZE / 2.0, 0.01));
    CHECK_THAT(w.y, WithinAbs(TILE_SIZE / 2.0, 0.01));
}

TEST_CASE("world_to_grid inverse of grid_to_world", "[mapdata]") {
    auto m = make_test_map();
    GridPos orig{3, 2};
    auto world = m.grid_to_world(orig);
    auto back = m.world_to_grid(world);
    CHECK(back.x == orig.x);
    CHECK(back.y == orig.y);
}

TEST_CASE("in_bounds checks correctly", "[mapdata]") {
    auto m = make_test_map();
    CHECK(m.in_bounds({0, 0}));
    CHECK(m.in_bounds({4, 4}));
    CHECK_FALSE(m.in_bounds({-1, 0}));
    CHECK_FALSE(m.in_bounds({0, -1}));
    CHECK_FALSE(m.in_bounds({5, 0}));
    CHECK_FALSE(m.in_bounds({0, 5}));
}

TEST_CASE("tile_at returns Blocked for out-of-bounds", "[mapdata]") {
    auto m = make_test_map();
    CHECK(m.tile_at({-1, 0}) == TileType::Blocked);
    CHECK(m.tile_at({99, 99}) == TileType::Blocked);
}

TEST_CASE("tile_at returns correct tile", "[mapdata]") {
    auto m = make_test_map();
    CHECK(m.tile_at({0, 0}) == TileType::Spawn);
    CHECK(m.tile_at({2, 2}) == TileType::Buildable);
    CHECK(m.tile_at({1, 1}) == TileType::Grass);
}

TEST_CASE("is_buildable only for Buildable tiles", "[mapdata]") {
    auto m = make_test_map();
    CHECK(m.is_buildable({2, 2}));
    CHECK_FALSE(m.is_buildable({0, 0}));
    CHECK_FALSE(m.is_buildable({1, 1}));
}
