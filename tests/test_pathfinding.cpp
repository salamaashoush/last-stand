#include "ai/pathfinding.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

static MapData make_open_map(int cols = 5, int rows = 5) {
    MapData m;
    m.name = "test";
    m.cols = cols;
    m.rows = rows;
    m.tiles.resize(rows, std::vector<TileType>(cols, TileType::Grass));
    m.spawn = {0, 0};
    m.exit_pos = {cols - 1, rows - 1};
    return m;
}

TEST_CASE("Path on open 5x5 grid is non-empty", "[pathfinding]") {
    auto m = make_open_map();
    auto path = Pathfinder::find_path(m, m.spawn, m.exit_pos);
    CHECK_FALSE(path.empty());
    // Path should start at spawn and end at exit
    auto start_world = m.grid_to_world(m.spawn);
    auto end_world = m.grid_to_world(m.exit_pos);
    CHECK(path.front().x == start_world.x);
    CHECK(path.front().y == start_world.y);
    CHECK(path.back().x == end_world.x);
    CHECK(path.back().y == end_world.y);
}

TEST_CASE("Blocked path returns empty", "[pathfinding]") {
    auto m = make_open_map();
    // Block the entire second column to wall off the exit
    std::unordered_set<GridPos, GridPosHash> blocked;
    for (int y = 0; y < m.rows; ++y) {
        m.tiles[y][1] = TileType::Blocked;
    }
    auto path = Pathfinder::find_path(m, m.spawn, m.exit_pos);
    CHECK(path.empty());
}

TEST_CASE("would_block_path detects blocking placement", "[pathfinding]") {
    // Create a 3x1 corridor: spawn=(0,0), exit=(2,0)
    MapData m;
    m.name = "corridor";
    m.cols = 3;
    m.rows = 1;
    m.tiles = {{TileType::Grass, TileType::Grass, TileType::Grass}};
    m.spawn = {0, 0};
    m.exit_pos = {2, 0};

    std::unordered_set<GridPos, GridPosHash> existing;
    // Blocking the middle tile should block the path
    CHECK(Pathfinder::would_block_path(m, {1, 0}, existing));
}

TEST_CASE("would_block_path allows non-blocking placement", "[pathfinding]") {
    auto m = make_open_map();
    std::unordered_set<GridPos, GridPosHash> existing;
    // Placing a tower at (2,0) should not block since many paths exist
    CHECK_FALSE(Pathfinder::would_block_path(m, {2, 0}, existing));
}
