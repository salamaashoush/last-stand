#pragma once
#include <string>
#include <vector>
#include <expected>
#include <fstream>
#include <nlohmann/json.hpp>
#include "core/types.hpp"
#include "core/constants.hpp"

namespace ls {

struct MapData {
    std::string name;
    int cols{GRID_COLS};
    int rows{GRID_ROWS};
    std::vector<std::vector<TileType>> tiles;
    std::vector<GridPos> path_waypoints;
    GridPos spawn;
    GridPos exit_pos;

    Vec2 grid_to_world(GridPos p) const {
        return {
            static_cast<float>(GRID_OFFSET_X + p.x * TILE_SIZE + TILE_SIZE / 2),
            static_cast<float>(GRID_OFFSET_Y + p.y * TILE_SIZE + TILE_SIZE / 2)
        };
    }

    GridPos world_to_grid(Vec2 p) const {
        return {
            static_cast<int>((p.x - GRID_OFFSET_X) / TILE_SIZE),
            static_cast<int>((p.y - GRID_OFFSET_Y) / TILE_SIZE)
        };
    }

    bool in_bounds(GridPos p) const {
        return p.x >= 0 && p.x < cols && p.y >= 0 && p.y < rows;
    }

    TileType tile_at(GridPos p) const {
        if (!in_bounds(p)) return TileType::Blocked;
        return tiles[p.y][p.x];
    }

    bool is_buildable(GridPos p) const {
        auto t = tile_at(p);
        return t == TileType::Grass || t == TileType::Buildable;
    }
};

class MapManager {
public:
    std::expected<MapData, std::string> load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open())
            return std::unexpected("Cannot open map: " + path);

        try {
            nlohmann::json j;
            file >> j;
            return parse(j);
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Map parse error: ") + e.what());
        }
    }

    const std::vector<std::string>& available_maps() const { return map_names_; }

    void set_available_maps(std::vector<std::string> names) {
        map_names_ = std::move(names);
    }

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
