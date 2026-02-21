#pragma once
#include "core/types.hpp"
#include "managers/map_manager.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ls {

struct GridPosHash {
    size_t operator()(GridPos p) const { return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 16); }
};

class Pathfinder {
  public:
    // blocked_tiles are positions occupied by towers
    static std::vector<Vec2> find_path(const MapData& map, GridPos start, GridPos goal,
                                       const std::unordered_set<GridPos, GridPosHash>& blocked_tiles = {}) {
        using Node = std::pair<float, GridPos>; // cost, pos
        std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
        std::unordered_map<GridPos, GridPos, GridPosHash> came_from;
        std::unordered_map<GridPos, float, GridPosHash> cost_so_far;

        open.push({0, start});
        cost_so_far[start] = 0;

        constexpr GridPos dirs[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

        while (!open.empty()) {
            auto [_, current] = open.top();
            open.pop();

            if (current == goal) break;

            for (auto d : dirs) {
                GridPos next{current.x + d.x, current.y + d.y};

                if (!map.in_bounds(next)) continue;
                auto tile = map.tile_at(next);
                if (tile == TileType::Blocked) continue;
                if (blocked_tiles.contains(next)) continue;

                float new_cost = cost_so_far[current] + 1.0f;
                if (!cost_so_far.contains(next) || new_cost < cost_so_far[next]) {
                    cost_so_far[next] = new_cost;
                    float h = static_cast<float>(std::abs(next.x - goal.x) + std::abs(next.y - goal.y));
                    open.push({new_cost + h, next});
                    came_from[next] = current;
                }
            }
        }

        // Reconstruct
        std::vector<Vec2> path;
        if (!came_from.contains(goal) && !(start == goal)) return path;

        GridPos current = goal;
        while (!(current == start)) {
            path.push_back(map.grid_to_world(current));
            current = came_from[current];
        }
        path.push_back(map.grid_to_world(start));
        std::ranges::reverse(path);
        return path;
    }

    // Check if placing a tower would block the path
    static bool would_block_path(const MapData& map, GridPos tower_pos,
                                 const std::unordered_set<GridPos, GridPosHash>& existing_towers) {
        auto blocked = existing_towers;
        blocked.insert(tower_pos);
        auto path = find_path(map, map.spawn, map.exit_pos, blocked);
        return path.empty();
    }
};

} // namespace ls
