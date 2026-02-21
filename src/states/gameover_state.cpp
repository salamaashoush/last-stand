#include "gameover_state.hpp"
#include "core/asset_paths.hpp"
#include "core/game.hpp"
#include <format>

namespace ls {

void GameOverState::enter(Game& game) {
    xp_earned_ = game.play.current_wave * 10;
    game.upgrades.upgrade_xp += xp_earned_;
    game.save_manager.save_upgrades(game.upgrades, "upgrades.json");
    game.state_machine.set_active_game(false);
}

void VictoryState::enter(Game& game) {
    xp_earned_ = 500 + game.play.current_wave * 10;
    game.upgrades.upgrade_xp += xp_earned_;
    game.save_manager.save_upgrades(game.upgrades, "upgrades.json");
    game.state_machine.set_active_game(false);
}

static void go_text(AssetManager& a, const char* text, float x, float y, float size, Color color) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        DrawTextEx(*font, text, {x, y}, size, 1.0f, color);
    } else {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
    }
}

static float go_measure(AssetManager& a, const char* text, float size) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) return MeasureTextEx(*font, text, size, 1.0f).x;
    return static_cast<float>(MeasureText(text, static_cast<int>(size)));
}

static void render_stats(Game& game, int base_y) {
    auto& st = game.play.stats;
    auto& a = game.assets;
    float x = SCREEN_WIDTH / 2.0f - 150;
    float y = static_cast<float>(base_y);
    int spacing = 24;

    DrawRectangle(static_cast<int>(x - 10), base_y - 10, 320, spacing * 9 + 20, {20, 20, 30, 200});
    DrawRectangleLinesEx({x - 10, static_cast<float>(base_y - 10), 320, static_cast<float>(spacing * 9 + 20)}, 1, GRAY);

    go_text(a, "--- STATS ---", x + 80, y, 18, GOLD);
    y += spacing + 4;

    int minutes = static_cast<int>(st.time_elapsed) / 60;
    int seconds = static_cast<int>(st.time_elapsed) % 60;

    go_text(a, std::format("Time Played: {}:{:02d}", minutes, seconds).c_str(), x, y, 16, WHITE);
    y += spacing;
    go_text(a, std::format("Enemies Killed: {}", st.total_kills).c_str(), x, y, 16, WHITE);
    y += spacing;
    go_text(a, std::format("Boss Kills: {}", st.boss_kills).c_str(), x, y, 16, RED);
    y += spacing;
    go_text(a, std::format("Gold Earned: {}", st.gold_earned).c_str(), x, y, 16, GOLD);
    y += spacing;
    go_text(a, std::format("Gold Spent: {}", st.gold_spent).c_str(), x, y, 16, GOLD);
    y += spacing;
    go_text(a, std::format("Towers Built: {}", st.towers_built).c_str(), x, y, 16, WHITE);
    y += spacing;
    go_text(a, std::format("Towers Sold: {}", st.towers_sold).c_str(), x, y, 16, WHITE);
    y += spacing;
    go_text(a, std::format("Hero Deaths: {}", st.hero_deaths).c_str(), x, y, 16, LIGHTGRAY);
}

void GameOverState::update(Game& game, [[maybe_unused]] float dt) {
    if (game.current_music) UpdateMusicStream(*game.current_music);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (game.current_music) {
            StopMusicStream(*game.current_music);
            game.current_music = nullptr;
        }
        game.state_machine.change_state(GameStateId::Menu, game);
    }
}

void GameOverState::render(Game& game) {
    ClearBackground({30, 10, 10, 255});
    auto& a = game.assets;

    auto title = "GAME OVER";
    float tw = go_measure(a, title, 48);
    go_text(a, title, SCREEN_WIDTH / 2.0f - tw / 2, 80, 48, RED);
    go_text(a, std::format("Survived {} waves", game.play.current_wave).c_str(), SCREEN_WIDTH / 2.0f - 100, 140, 24,
            WHITE);
    go_text(a, std::format("Gold remaining: {}", game.play.gold).c_str(), SCREEN_WIDTH / 2.0f - 90, 175, 20, GOLD);
    go_text(a, std::format("Lives remaining: {}", game.play.lives).c_str(), SCREEN_WIDTH / 2.0f - 90, 200, 20, GREEN);
    go_text(a, std::format("XP Earned: +{}", xp_earned_).c_str(), SCREEN_WIDTH / 2.0f - 70, 228, 20,
            {100, 200, 255, 255});

    render_stats(game, 260);

    auto hint = "Press ENTER to return to menu";
    float hw = go_measure(a, hint, 18);
    go_text(a, hint, SCREEN_WIDTH / 2.0f - hw / 2, static_cast<float>(SCREEN_HEIGHT - 50), 18, GRAY);
}

void VictoryState::update(Game& game, [[maybe_unused]] float dt) {
    if (game.current_music) UpdateMusicStream(*game.current_music);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (game.current_music) {
            StopMusicStream(*game.current_music);
            game.current_music = nullptr;
        }
        game.state_machine.change_state(GameStateId::Menu, game);
    }
}

void VictoryState::render(Game& game) {
    ClearBackground({10, 20, 30, 255});
    auto& a = game.assets;

    auto title = "VICTORY!";
    float tw = go_measure(a, title, 48);
    go_text(a, title, SCREEN_WIDTH / 2.0f - tw / 2, 80, 48, GOLD);
    go_text(a, "You survived all 30 waves!", SCREEN_WIDTH / 2.0f - 120, 140, 24, WHITE);
    go_text(a, std::format("Gold remaining: {}", game.play.gold).c_str(), SCREEN_WIDTH / 2.0f - 90, 175, 20, GOLD);
    go_text(a, std::format("Lives remaining: {}", game.play.lives).c_str(), SCREEN_WIDTH / 2.0f - 90, 200, 20, GREEN);
    go_text(a, std::format("XP Earned: +{}", xp_earned_).c_str(), SCREEN_WIDTH / 2.0f - 70, 228, 20,
            {100, 200, 255, 255});

    render_stats(game, 260);

    auto hint = "Press ENTER to return to menu";
    float hw = go_measure(a, hint, 18);
    go_text(a, hint, SCREEN_WIDTH / 2.0f - hw / 2, static_cast<float>(SCREEN_HEIGHT - 50), 18, GRAY);
}

} // namespace ls
