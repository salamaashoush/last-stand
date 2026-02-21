#pragma once
#include <string>
#include <cctype>
#include <raylib.h>
#include "asset_paths.hpp"

namespace ls {

struct BiomeTheme {
    // Texture asset names per tile role
    const char* ground_tex;
    const char* path_tex;
    const char* blocked_tex;

    // Color tints (WHITE = no change)
    Color ground_tint;
    Color path_tint;
    Color blocked_tint;
    Color marker_tint; // for buildable, spawn, exit overlays

    // Fallback colors when texture missing
    Color ground_fallback;
    Color path_fallback;
    Color blocked_fallback;

    // Decoration generation
    int deco_density; // percent chance per grass tile
    int deco_weights[8]; // 0=tree, 1=bush, 2=leaf, 3=flower, 4=rock_sm, 5=rock_md, 6=rock_lg, 7=flame

    // Music
    const char* music_track;

    // Background clear color
    Color bg_color;
};

inline const BiomeTheme& get_biome_theme(const std::string& map_name) {
    static const BiomeTheme forest_theme = {
        assets::TILE_GRASS,     // ground_tex
        assets::TILE_PATH,      // path_tex
        assets::TILE_BLOCKED,   // blocked_tex
        WHITE,                  // ground_tint
        WHITE,                  // path_tint
        WHITE,                  // blocked_tint
        WHITE,                  // marker_tint
        {60, 120, 40, 255},     // ground_fallback
        {160, 140, 100, 255},   // path_fallback
        {80, 80, 80, 255},      // blocked_fallback
        15,                     // deco_density
        {20, 15, 15, 15, 15, 10, 10, 0}, // deco_weights (tree, bush, leaf, flower, rock_sm, rock_md, rock_lg, flame)
        assets::MUSIC_PLAIN,    // music_track
        {20, 35, 20, 255},      // bg_color
    };

    static const BiomeTheme desert_theme = {
        assets::BIOME_DESERT_GROUND, // ground_tex
        assets::TILE_PATH,           // path_tex
        assets::TILE_BLOCKED,        // blocked_tex
        WHITE,                       // ground_tint
        {210, 190, 140, 255},        // path_tint (sandy)
        {200, 180, 130, 255},        // blocked_tint (sandy)
        {210, 190, 140, 255},        // marker_tint (sandy)
        {180, 160, 110, 255},        // ground_fallback
        {160, 140, 90, 255},         // path_fallback
        {130, 120, 90, 255},         // blocked_fallback
        8,                           // deco_density
        {0, 20, 10, 0, 30, 20, 20, 0}, // deco_weights (no trees/flowers, sparse scrub, mostly rocks)
        assets::MUSIC_SWAMP,         // music_track
        {35, 28, 18, 255},           // bg_color
    };

    static const BiomeTheme castle_theme = {
        assets::BIOME_CASTLE_GROUND, // ground_tex
        assets::BIOME_CASTLE_PATH,   // path_tex
        assets::TILE_BLOCKED,        // blocked_tex
        WHITE,                       // ground_tint
        WHITE,                       // path_tint
        {150, 150, 170, 255},        // blocked_tint (grey-blue)
        {170, 170, 190, 255},        // marker_tint (grey)
        {80, 80, 100, 255},          // ground_fallback
        {120, 100, 80, 255},         // path_fallback
        {60, 60, 70, 255},           // blocked_fallback
        5,                           // deco_density
        {0, 0, 0, 0, 20, 20, 30, 30}, // deco_weights (rocks + flames only)
        assets::MUSIC_BOSS,          // music_track
        {22, 22, 30, 255},           // bg_color
    };

    // Case-insensitive search
    auto contains_ci = [](const std::string& str, const char* sub) {
        std::string lower = str;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return lower.find(sub) != std::string::npos;
    };
    if (contains_ci(map_name, "desert")) return desert_theme;
    if (contains_ci(map_name, "castle")) return castle_theme;
    return forest_theme;
}

} // namespace ls
