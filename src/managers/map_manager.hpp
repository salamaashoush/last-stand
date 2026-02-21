#pragma once
#include "core/biome_theme.hpp"
#include "core/constants.hpp"
#include "core/types.hpp"
#include <expected>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ls {

struct Decoration {
    GridPos pos;
    int texture_index; // 0-6 maps to decoration texture array
};

struct MapData {
    std::string name;
    int cols{GRID_COLS};
    int rows{GRID_ROWS};
    std::vector<std::vector<TileType>> tiles;
    std::vector<GridPos> path_waypoints;
    std::vector<Decoration> decorations;
    GridPos spawn;
    GridPos exit_pos;

    Vec2 grid_to_world(GridPos p) const {
        return {static_cast<float>(GRID_OFFSET_X + p.x * TILE_SIZE + TILE_SIZE / 2),
                static_cast<float>(GRID_OFFSET_Y + p.y * TILE_SIZE + TILE_SIZE / 2)};
    }

    GridPos world_to_grid(Vec2 p) const {
        return {static_cast<int>((p.x - GRID_OFFSET_X) / TILE_SIZE),
                static_cast<int>((p.y - GRID_OFFSET_Y) / TILE_SIZE)};
    }

    bool in_bounds(GridPos p) const { return p.x >= 0 && p.x < cols && p.y >= 0 && p.y < rows; }

    TileType tile_at(GridPos p) const {
        if (!in_bounds(p)) return TileType::Blocked;
        return tiles[p.y][p.x];
    }

    bool is_buildable(GridPos p) const {
        auto t = tile_at(p);
        return t == TileType::Buildable;
    }

    void generate_decorations() {
        decorations.clear();
        // Seed based on map name for consistent results
        unsigned seed = 0;
        for (auto c : name) seed = seed * 31 + static_cast<unsigned>(c);
        std::srand(seed);

        auto is_path_adjacent = [&](int x, int y) -> bool {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    GridPos np{x + dx, y + dy};
                    if (in_bounds(np)) {
                        auto t = tile_at(np);
                        if (t == TileType::Path || t == TileType::Spawn || t == TileType::Exit) return true;
                    }
                }
            }
            return false;
        };

        auto& theme = get_biome_theme(name);

        // Compute cumulative weights for weighted random selection
        int total_weight = 0;
        int cumulative[8];
        for (int i = 0; i < 8; ++i) {
            total_weight += theme.deco_weights[i];
            cumulative[i] = total_weight;
        }

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (tile_at({x, y}) != TileType::Grass) continue;
                if (is_path_adjacent(x, y)) continue;
                if ((std::rand() % 100) >= theme.deco_density) continue;
                if (total_weight <= 0) continue;
                // Weighted random decoration selection
                int r = std::rand() % total_weight;
                int tex_idx = 0;
                for (int i = 0; i < 8; ++i) {
                    if (r < cumulative[i]) {
                        tex_idx = i;
                        break;
                    }
                }
                decorations.push_back({{x, y}, tex_idx});
            }
        }
    }
};

class MapManager {
  public:
    std::expected<MapData, std::string> load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return std::unexpected("Cannot open map: " + path);

        try {
            nlohmann::json j;
            file >> j;
            return parse(j);
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Map parse error: ") + e.what());
        }
    }

    const std::vector<std::string>& available_maps() const { return map_names_; }

    void set_available_maps(std::vector<std::string> names) { map_names_ = std::move(names); }

  private:
    std::vector<std::string> map_names_{"forest", "desert", "castle"};

    MapData parse(const nlohmann::json& j) {
        MapData map;
        map.name = j.value("name", "Unknown");
        map.cols = j.value("cols", GRID_COLS);
        map.rows = j.value("rows", GRID_ROWS);

        auto& jtiles = j.at("tiles");
        map.tiles.resize(map.rows);
        for (int y = 0; y < map.rows; ++y) {
            map.tiles[y].resize(map.cols);
            for (int x = 0; x < map.cols; ++x) {
                map.tiles[y][x] = static_cast<TileType>(jtiles[y][x].get<int>());
            }
        }

        for (auto& wp : j.at("waypoints")) {
            map.path_waypoints.push_back({wp[0].get<int>(), wp[1].get<int>()});
        }

        map.spawn = {j.at("spawn")[0].get<int>(), j.at("spawn")[1].get<int>()};
        map.exit_pos = {j.at("exit")[0].get<int>(), j.at("exit")[1].get<int>()};

        return map;
    }
};

} // namespace ls
