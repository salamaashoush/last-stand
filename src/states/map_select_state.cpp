#include "map_select_state.hpp"
#include "core/game.hpp"
#include <format>

namespace ls {

void MapSelectState::enter(Game&) {
    selected_ = 0;
}

void MapSelectState::update(Game& game, [[maybe_unused]] float dt) {
    auto& maps = game.map_manager.available_maps();
    int count = static_cast<int>(maps.size());

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_ = (selected_ + 1) % count;
        game.sounds.play(game.sounds.ui_click);
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_ = (selected_ + count - 1) % count;
        game.sounds.play(game.sounds.ui_click);
    }
    if (IsKeyPressed(KEY_ESCAPE)) game.state_machine.change_state(GameStateId::Menu, game);

    // Difficulty toggle with D key
    if (IsKeyPressed(KEY_D)) {
        int d = (static_cast<int>(game.difficulty) + 1) % 3;
        game.difficulty = static_cast<Difficulty>(d);
        game.sounds.play(game.sounds.ui_click);
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        game.sounds.play(game.sounds.ui_click);
        std::string path = std::format("assets/maps/{}.json", maps[selected_]);
        auto result = game.map_manager.load(path);
        if (result) {
            game.current_map = std::move(*result);
            game.state_machine.change_state(GameStateId::Playing, game);
        }
    }
}

void MapSelectState::render(Game& game) {
    ClearBackground({20, 20, 30, 255});

    DrawText("SELECT MAP", SCREEN_WIDTH / 2 - 80, 80, 32, GOLD);

    auto& maps = game.map_manager.available_maps();
    const char* descriptions[] = {
        "Classic forest path - Easy",
        "Winding desert canyon - Medium",
        "Castle siege corridors - Hard"
    };
    Color map_colors[] = {
        {60, 140, 40, 255},
        {200, 170, 80, 255},
        {140, 140, 160, 255}
    };

    for (int i = 0; i < static_cast<int>(maps.size()); ++i) {
        int y = 200 + i * 120;
        bool sel = (i == selected_);

        Rectangle box = {SCREEN_WIDTH / 2.0f - 200, static_cast<float>(y), 400, 100};
        DrawRectangleRec(box, sel ? Color{50, 60, 70, 255} : Color{35, 35, 45, 255});
        DrawRectangleLinesEx(box, sel ? 2.0f : 1.0f, sel ? GOLD : GRAY);

        // Map color preview
        DrawRectangle(SCREEN_WIDTH / 2 - 185, y + 15, 70, 70, map_colors[i]);

        DrawText(maps[i].c_str(), SCREEN_WIDTH / 2 - 100, y + 15, 24, sel ? WHITE : LIGHTGRAY);
        if (i < 3) {
            DrawText(descriptions[i], SCREEN_WIDTH / 2 - 100, y + 45, 14, sel ? LIGHTGRAY : GRAY);
        }
    }

    // Difficulty selection
    const char* diff_names[] = {"Easy", "Normal", "Hard"};
    Color diff_colors[] = {GREEN, WHITE, RED};
    const char* diff_descs[] = {
        "0 Gold | 30 Lives | 0.8x enemies | 1.2x rewards",
        "0 Gold | 20 Lives | 1.0x enemies | 1.0x rewards",
        "0 Gold | 10 Lives | 1.3x enemies | 0.8x rewards"
    };
    int di = static_cast<int>(game.difficulty);

    int dy = SCREEN_HEIGHT - 130;
    DrawRectangle(SCREEN_WIDTH / 2 - 200, dy, 400, 60, {35, 35, 45, 255});
    DrawRectangleLinesEx({SCREEN_WIDTH / 2.0f - 200, static_cast<float>(dy), 400, 60}, 1, GRAY);
    DrawText(std::format("Difficulty: {}", diff_names[di]).c_str(), SCREEN_WIDTH / 2 - 80, dy + 8, 20, diff_colors[di]);
    DrawText(diff_descs[di], SCREEN_WIDTH / 2 - 180, dy + 35, 12, LIGHTGRAY);
    DrawText("[D] to change", SCREEN_WIDTH / 2 + 110, dy + 8, 12, GRAY);

    DrawText("Press ENTER to start  |  ESC to go back", SCREEN_WIDTH / 2 - 160, SCREEN_HEIGHT - 50, 16, GRAY);
}

} // namespace ls
