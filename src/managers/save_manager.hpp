#pragma once
#include <string>
#include <fstream>
#include <expected>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include "components/components.hpp"
#include "core/types.hpp"

namespace ls {

struct SaveData {
    std::string map_name;
    Gold gold;
    int lives;
    WaveNum current_wave;
    int hero_level;
    int hero_xp;
    struct TowerSave {
        TowerType type;
        int level;
        GridPos pos;
    };
    std::vector<TowerSave> towers;
};

class SaveManager {
public:
    std::expected<void, std::string> save(const SaveData& data, const std::string& path) {
        nlohmann::json j;
        j["map"] = data.map_name;
        j["gold"] = data.gold;
        j["lives"] = data.lives;
        j["wave"] = data.current_wave;
        j["hero_level"] = data.hero_level;
        j["hero_xp"] = data.hero_xp;

        auto& towers = j["towers"];
        for (auto& t : data.towers) {
            towers.push_back({
                {"type", static_cast<int>(t.type)},
                {"level", t.level},
                {"x", t.pos.x},
                {"y", t.pos.y}
            });
        }

        std::ofstream file(path);
        if (!file.is_open())
            return std::unexpected("Cannot write save file: " + path);
        file << j.dump(2);
        return {};
    }

    std::expected<SaveData, std::string> load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open())
            return std::unexpected("Cannot open save file: " + path);

        try {
            nlohmann::json j;
            file >> j;

            SaveData data;
            data.map_name = j.at("map").get<std::string>();
            data.gold = j.at("gold").get<Gold>();
            data.lives = j.at("lives").get<int>();
            data.current_wave = j.at("wave").get<WaveNum>();
            data.hero_level = j.at("hero_level").get<int>();
            data.hero_xp = j.at("hero_xp").get<int>();

            for (auto& t : j.at("towers")) {
                data.towers.push_back({
                    static_cast<TowerType>(t.at("type").get<int>()),
                    t.at("level").get<int>(),
                    {t.at("x").get<int>(), t.at("y").get<int>()}
                });
            }

            return data;
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Save parse error: ") + e.what());
        }
    }
};

} // namespace ls
