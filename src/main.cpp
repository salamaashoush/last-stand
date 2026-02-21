#include <raylib.h>
#include "core/game.hpp"
#include "states/menu_state.hpp"
#include "states/map_select_state.hpp"
#include "states/playing_state.hpp"
#include "states/paused_state.hpp"
#include "states/gameover_state.hpp"

int main() {
    InitWindow(ls::SCREEN_WIDTH, ls::SCREEN_HEIGHT, "Last Stand - Tower Defense");
    SetTargetFPS(ls::TARGET_FPS);
    InitAudioDevice();

    ls::Game game;

    // Register all states
    game.state_machine.register_state<ls::MenuState>();
    game.state_machine.register_state<ls::MapSelectState>();
    game.state_machine.register_state<ls::PlayingState>();
    game.state_machine.register_state<ls::PausedState>();
    game.state_machine.register_state<ls::GameOverState>();
    game.state_machine.register_state<ls::VictoryState>();

    game.state_machine.change_state(ls::GameStateId::Menu, game);

    while (!WindowShouldClose() && game.running) {
        float dt = GetFrameTime();
        game.state_machine.update(game, dt);

        BeginDrawing();
        game.state_machine.render(game);
        DrawFPS(ls::SCREEN_WIDTH - 80, ls::SCREEN_HEIGHT - 20);
        EndDrawing();
    }

    game.sounds.cleanup();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
