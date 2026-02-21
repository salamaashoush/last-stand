#include "paused_state.hpp"
#include "core/asset_paths.hpp"
#include "core/game.hpp"
#include "systems/systems.hpp"
#include <format>

namespace ls {

static void pause_text(AssetManager& a, const char* text, float x, float y, float size, Color color) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        DrawTextEx(*font, text, {x, y}, size, 1.0f, color);
    } else {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
    }
}

static float pause_measure(AssetManager& a, const char* text, float size) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) return MeasureTextEx(*font, text, size, 1.0f).x;
    return static_cast<float>(MeasureText(text, static_cast<int>(size)));
}

static void do_save(Game& game) {
    SaveData data;
    data.map_name = game.current_map.name;
    data.gold = game.play.gold;
    data.lives = game.play.lives;
    data.current_wave = game.play.current_wave;

    auto heroes = game.registry.view<Hero>();
    for (auto [e, hero] : heroes.each()) {
        data.hero_level = hero.level;
        data.hero_xp = hero.xp;
    }

    auto towers = game.registry.view<Tower, GridCell>();
    for (auto [e, tower, gc] : towers.each()) {
        data.towers.push_back({tower.type, tower.level, gc.pos});
    }

    game.save_manager.save(data, game.save_path);
}

void PausedState::update(Game& game, [[maybe_unused]] float dt) {
    if (game.current_music) UpdateMusicStream(*game.current_music);

    auto play_click = [&]() {
        Sound* click = game.assets.get_sound(assets::SND_CLICK);
        if (click)
            PlaySound(*click);
        else
            game.sounds.play(game.sounds.ui_click);
    };

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_ = (selected_ + 1) % 4;
        play_click();
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_ = (selected_ + 3) % 4;
        play_click();
    }

    // Quick resume with P or Escape
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
        game.state_machine.resume_state(GameStateId::Playing, game);
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        play_click();
        switch (selected_) {
        case 0: // Resume
            game.state_machine.resume_state(GameStateId::Playing, game);
            break;
        case 1: // Save Game
            do_save(game);
            save_flash_ = 1.5f;
            break;
        case 2: // Settings
            // Volume controls shown inline
            break;
        case 3:            // Quit to Menu
            do_save(game); // Auto-save before quitting
            if (game.current_music) {
                StopMusicStream(*game.current_music);
                game.current_music = nullptr;
            }
            game.state_machine.set_active_game(true); // Mark that a game can be resumed
            game.state_machine.change_state(GameStateId::Menu, game);
            break;
        }
    }

    // Settings: inline volume controls when Settings is selected
    if (selected_ == 2) {
        if (IsKeyPressed(KEY_LEFT)) {
            game.music_volume = std::max(0.0f, game.music_volume - 0.1f);
            if (game.current_music && !game.music_muted) SetMusicVolume(*game.current_music, game.music_volume);
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            game.music_volume = std::min(1.0f, game.music_volume + 0.1f);
            if (game.current_music && !game.music_muted) SetMusicVolume(*game.current_music, game.music_volume);
        }
        if (IsKeyPressed(KEY_M)) {
            game.music_muted = !game.music_muted;
            if (game.current_music) SetMusicVolume(*game.current_music, game.music_muted ? 0.0f : game.music_volume);
        }
    }

    // Update save flash timer
    if (save_flash_ > 0) save_flash_ -= dt;
}

void PausedState::render(Game& game) {
    ClearBackground({20, 25, 20, 255});

    // Draw the game world behind the overlay
    Camera2D cam = game.camera;
    cam.target.x += game.play.shake_offset.x;
    cam.target.y += game.play.shake_offset.y;
    BeginMode2D(cam);
    systems::render_system(game);
    EndMode2D();

    // Dark overlay
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, {0, 0, 0, 160});
    auto& a = game.assets;

    auto title = "PAUSED";
    float tw = pause_measure(a, title, 40);
    pause_text(a, title, SCREEN_WIDTH / 2.0f - tw / 2, 160, 40, WHITE);

    // Menu items
    const char* items[] = {"Resume", "Save Game", "Settings", "Quit to Menu"};
    for (int i = 0; i < 4; ++i) {
        float y = 260.0f + i * 55.0f;
        Color c = (i == selected_) ? GOLD : LIGHTGRAY;
        float size = (i == selected_) ? 26.0f : 22.0f;
        float w = pause_measure(a, items[i], size);
        pause_text(a, items[i], SCREEN_WIDTH / 2.0f - w / 2, y, size, c);
        if (i == selected_) {
            pause_text(a, ">", SCREEN_WIDTH / 2.0f - w / 2 - 25, y, size, GOLD);
        }

        // Settings sub-options
        if (i == 2 && selected_ == 2) {
            float sy = y + 28;
            auto vol_text = std::format("Volume: {:.0f}%  [LEFT/RIGHT]", game.music_volume * 100);
            float vw = pause_measure(a, vol_text.c_str(), 14);
            pause_text(a, vol_text.c_str(), SCREEN_WIDTH / 2.0f - vw / 2, sy, 14, WHITE);

            auto mute_text = std::format("Music: {}  [M to toggle]", game.music_muted ? "MUTED" : "ON");
            float mw = pause_measure(a, mute_text.c_str(), 14);
            pause_text(a, mute_text.c_str(), SCREEN_WIDTH / 2.0f - mw / 2, sy + 18, 14, game.music_muted ? RED : GREEN);
        }
    }

    // Save confirmation flash
    if (save_flash_ > 0) {
        float alpha = std::min(1.0f, save_flash_);
        Color flash_col = {100, 255, 100, static_cast<unsigned char>(255 * alpha)};
        auto saved_text = "Game Saved!";
        float sw = pause_measure(a, saved_text, 20);
        pause_text(a, saved_text, SCREEN_WIDTH / 2.0f - sw / 2, 500, 20, flash_col);
    }

    pause_text(a, "P / ESC to resume", SCREEN_WIDTH / 2.0f - 80, static_cast<float>(SCREEN_HEIGHT - 40), 14, GRAY);
}

} // namespace ls
