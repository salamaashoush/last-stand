#include "gameover_state.hpp"
#include "core/game.hpp"
#include <format>

namespace ls {

static void render_stats(Game& game, int base_y) {
    auto& st = game.play.stats;
    int x = SCREEN_WIDTH / 2 - 150;
    int y = base_y;
    int spacing = 24;

    DrawRectangle(x - 10, y - 10, 320, spacing * 9 + 20, {20, 20, 30, 200});
    DrawRectangleLinesEx({static_cast<float>(x - 10), static_cast<float>(y - 10), 320, static_cast<float>(spacing * 9 + 20)}, 1, GRAY);

    DrawText("--- STATS ---", x + 80, y, 18, GOLD);
    y += spacing + 4;

    int minutes = static_cast<int>(st.time_elapsed) / 60;
    int seconds = static_cast<int>(st.time_elapsed) % 60;

    DrawText(std::format("Time Played: {}:{:02d}", minutes, seconds).c_str(), x, y, 16, WHITE); y += spacing;
    DrawText(std::format("Enemies Killed: {}", st.total_kills).c_str(), x, y, 16, WHITE); y += spacing;
    DrawText(std::format("Boss Kills: {}", st.boss_kills).c_str(), x, y, 16, RED); y += spacing;
    DrawText(std::format("Gold Earned: {}", st.gold_earned).c_str(), x, y, 16, GOLD); y += spacing;
    DrawText(std::format("Gold Spent: {}", st.gold_spent).c_str(), x, y, 16, GOLD); y += spacing;
    DrawText(std::format("Towers Built: {}", st.towers_built).c_str(), x, y, 16, WHITE); y += spacing;
    DrawText(std::format("Towers Sold: {}", st.towers_sold).c_str(), x, y, 16, WHITE); y += spacing;
    DrawText(std::format("Hero Deaths: {}", st.hero_deaths).c_str(), x, y, 16, LIGHTGRAY);
}

void GameOverState::update(Game& game, [[maybe_unused]] float dt) {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        game.state_machine.change_state(GameStateId::Menu, game);
    }
}

void GameOverState::render(Game& game) {
    ClearBackground({30, 10, 10, 255});

    DrawText("GAME OVER", SCREEN_WIDTH / 2 - 100, 80, 48, RED);
    DrawText(std::format("Survived {} waves", game.play.current_wave).c_str(),
            SCREEN_WIDTH / 2 - 100, 140, 24, WHITE);
    DrawText(std::format("Gold remaining: {}", game.play.gold).c_str(),
            SCREEN_WIDTH / 2 - 90, 175, 20, GOLD);
    DrawText(std::format("Lives remaining: {}", game.play.lives).c_str(),
            SCREEN_WIDTH / 2 - 90, 200, 20, GREEN);

    render_stats(game, 240);

    DrawText("Press ENTER to return to menu", SCREEN_WIDTH / 2 - 130, SCREEN_HEIGHT - 50, 18, GRAY);
}

void VictoryState::update(Game& game, [[maybe_unused]] float dt) {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        game.state_machine.change_state(GameStateId::Menu, game);
    }
}

void VictoryState::render(Game& game) {
    ClearBackground({10, 20, 30, 255});

    DrawText("VICTORY!", SCREEN_WIDTH / 2 - 80, 80, 48, GOLD);
    DrawText("You survived all 30 waves!", SCREEN_WIDTH / 2 - 120, 140, 24, WHITE);
    DrawText(std::format("Gold remaining: {}", game.play.gold).c_str(),
            SCREEN_WIDTH / 2 - 90, 175, 20, GOLD);
    DrawText(std::format("Lives remaining: {}", game.play.lives).c_str(),
            SCREEN_WIDTH / 2 - 90, 200, 20, GREEN);

    render_stats(game, 240);

    DrawText("Press ENTER to return to menu", SCREEN_WIDTH / 2 - 130, SCREEN_HEIGHT - 50, 18, GRAY);
}

} // namespace ls
