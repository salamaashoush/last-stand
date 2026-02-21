#include "paused_state.hpp"
#include "core/game.hpp"
#include "systems/systems.hpp"

namespace ls {

void PausedState::update(Game& game, [[maybe_unused]] float dt) {
    if (IsKeyPressed(KEY_DOWN)) selected_ = (selected_ + 1) % 3;
    if (IsKeyPressed(KEY_UP)) selected_ = (selected_ + 2) % 3;

    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
        game.state_machine.change_state(GameStateId::Playing, game);
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        switch (selected_) {
            case 0: game.state_machine.change_state(GameStateId::Playing, game); break;
            case 1: {
                // Save handled in playing state
                game.state_machine.change_state(GameStateId::Playing, game);
                break;
            }
            case 2: game.state_machine.change_state(GameStateId::Menu, game); break;
        }
    }
}

void PausedState::render(Game& game) {
    // Draw game underneath
    ClearBackground({20, 25, 20, 255});
    systems::render_system(game);
    systems::ui_system(game);

    // Overlay
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, {0, 0, 0, 150});

    DrawText("PAUSED", SCREEN_WIDTH / 2 - 60, 200, 40, WHITE);

    const char* items[] = {"Resume", "Save & Resume", "Quit to Menu"};
    for (int i = 0; i < 3; ++i) {
        Color c = (i == selected_) ? GOLD : LIGHTGRAY;
        int w = MeasureText(items[i], 24);
        DrawText(items[i], SCREEN_WIDTH / 2 - w / 2, 300 + i * 50, 24, c);
    }
}

} // namespace ls
