#include "menu_state.hpp"
#include "core/game.hpp"
#include <cmath>
#include <format>

namespace ls {

void MenuState::update(Game& game, float dt) {
    title_pulse_ += dt;

    // Initialize sounds if not already done
    if (!game.sounds.initialized_) {
        game.sounds.init();
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_ = (selected_ + 1) % 4;
        game.sounds.play(game.sounds.ui_click);
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_ = (selected_ + 3) % 4;
        game.sounds.play(game.sounds.ui_click);
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        game.sounds.play(game.sounds.ui_click);
        switch (selected_) {
            case 0: // New Game
                game.state_machine.change_state(GameStateId::MapSelect, game);
                break;
            case 1: { // Load Game
                auto result = game.save_manager.load(game.save_path);
                if (result) {
                    game.pending_load = std::move(*result);
                    // Load the saved map - try both original and lowercase names
                    std::string name = game.pending_load->map_name;
                    std::string lower_name = name;
                    for (auto& ch : lower_name) ch = static_cast<char>(std::tolower(ch));
                    auto map_result = game.map_manager.load("assets/maps/" + lower_name + ".json");
                    if (!map_result) {
                        map_result = game.map_manager.load("assets/maps/" + name + ".json");
                    }
                    if (map_result) {
                        game.current_map = std::move(*map_result);
                        game.state_machine.change_state(GameStateId::Playing, game);
                    }
                }
                break;
            }
            case 2: // Options (placeholder)
                break;
            case 3: // Quit
                game.running = false;
                break;
        }
    }
}

void MenuState::render(Game&) {
    ClearBackground({20, 20, 30, 255});

    // Title
    float pulse = 1.0f + 0.05f * std::sin(title_pulse_ * 2.0f);
    int title_size = static_cast<int>(48 * pulse);
    const char* title = "LAST STAND";
    int tw = MeasureText(title, title_size);
    DrawText(title, SCREEN_WIDTH / 2 - tw / 2, 120, title_size, GOLD);

    const char* subtitle = "Tower Defense";
    int sw = MeasureText(subtitle, 24);
    DrawText(subtitle, SCREEN_WIDTH / 2 - sw / 2, 180, 24, LIGHTGRAY);

    // Menu items
    const char* items[] = {"New Game", "Load Game", "Options", "Quit"};
    for (int i = 0; i < 4; ++i) {
        int y = 300 + i * 60;
        Color c = (i == selected_) ? GOLD : LIGHTGRAY;
        int size = (i == selected_) ? 28 : 24;
        int iw = MeasureText(items[i], size);
        DrawText(items[i], SCREEN_WIDTH / 2 - iw / 2, y, size, c);
        if (i == selected_) {
            DrawText(">", SCREEN_WIDTH / 2 - iw / 2 - 30, y, size, GOLD);
        }
    }

    DrawText("Press ENTER to select", SCREEN_WIDTH / 2 - 100, 580, 16, GRAY);
    DrawText("v0.2.0 - Built with Raylib + EnTT", SCREEN_WIDTH / 2 - 130, SCREEN_HEIGHT - 30, 14, DARKGRAY);
}

} // namespace ls
