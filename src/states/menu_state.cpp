#include "menu_state.hpp"
#include "core/asset_paths.hpp"
#include "core/game.hpp"
#include <cmath>
#include <format>

namespace ls {

static void menu_text(AssetManager& a, const char* text, float x, float y, float size, Color color) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        DrawTextEx(*font, text, {x, y}, size, 1.0f, color);
    } else {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
    }
}

static float menu_measure(AssetManager& a, const char* text, float size) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        return MeasureTextEx(*font, text, size, 1.0f).x;
    }
    return static_cast<float>(MeasureText(text, static_cast<int>(size)));
}

void MenuState::enter(Game& game) {
    // Build menu items based on whether there's an active game
    items_.clear();
    if (game.state_machine.has_active_game()) {
        items_.push_back({"Resume Game", MenuItem::ResumeGame});
    }
    items_.push_back({"New Game", MenuItem::NewGame});
    items_.push_back({"Load Game", MenuItem::LoadGame});
    items_.push_back({"Upgrades", MenuItem::Upgrades});
    items_.push_back({"Quit", MenuItem::Quit});
    selected_ = 0;
}

void MenuState::update(Game& game, float dt) {
    title_pulse_ += dt;

    // Initialize sounds if not already done
    if (!game.sounds.initialized_) {
        game.sounds.init();
    }

    // Start menu music
    Music* menu_music = game.assets.get_music(assets::MUSIC_MENU);
    if (menu_music && !IsMusicStreamPlaying(*menu_music)) {
        PlayMusicStream(*menu_music);
        SetMusicVolume(*menu_music, game.music_muted ? 0.0f : game.music_volume);
        game.current_music = menu_music;
    }
    if (game.current_music) UpdateMusicStream(*game.current_music);

    auto play_click = [&]() {
        Sound* click = game.assets.get_sound(assets::SND_CLICK);
        if (click)
            PlaySound(*click);
        else
            game.sounds.play(game.sounds.ui_click);
    };

    int count = static_cast<int>(items_.size());

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_ = (selected_ + 1) % count;
        play_click();
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_ = (selected_ + count - 1) % count;
        play_click();
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        play_click();
        auto action = items_[selected_].action;
        switch (action) {
        case MenuItem::ResumeGame: {
            // Resume the in-progress game
            Music* gameplay_music = game.assets.get_music(assets::MUSIC_PLAIN);
            if (gameplay_music) {
                if (game.current_music) StopMusicStream(*game.current_music);
                PlayMusicStream(*gameplay_music);
                SetMusicVolume(*gameplay_music, game.music_muted ? 0.0f : game.music_volume);
                game.current_music = gameplay_music;
            }
            game.state_machine.resume_state(GameStateId::Playing, game);
            break;
        }
        case MenuItem::NewGame:
            game.state_machine.set_active_game(false);
            if (game.current_music) StopMusicStream(*game.current_music);
            game.current_music = nullptr;
            game.state_machine.change_state(GameStateId::MapSelect, game);
            break;
        case MenuItem::LoadGame: {
            auto result = game.save_manager.load(game.save_path);
            if (result) {
                game.pending_load = std::move(*result);
                std::string name = game.pending_load->map_name;
                std::string lower_name = name;
                for (auto& ch : lower_name) ch = static_cast<char>(std::tolower(ch));
                auto map_result = game.map_manager.load("assets/maps/" + lower_name + ".json");
                if (!map_result) {
                    map_result = game.map_manager.load("assets/maps/" + name + ".json");
                }
                if (map_result) {
                    game.current_map = std::move(*map_result);
                    if (game.current_music) StopMusicStream(*game.current_music);
                    game.current_music = nullptr;
                    game.state_machine.set_active_game(true);
                    game.state_machine.change_state(GameStateId::Playing, game);
                } else {
                    load_error_flash_ = 2.0f;
                }
            } else {
                load_error_flash_ = 2.0f;
            }
            break;
        }
        case MenuItem::Upgrades:
            game.state_machine.change_state(GameStateId::Upgrades, game);
            break;
        case MenuItem::Quit:
            game.running = false;
            break;
        }
    }

    if (load_error_flash_ > 0) load_error_flash_ -= dt;
}

void MenuState::render(Game& game) {
    ClearBackground({20, 20, 30, 255});
    auto& a = game.assets;

    // Title
    float pulse = 1.0f + 0.05f * std::sin(title_pulse_ * 2.0f);
    float title_size = 48 * pulse;
    const char* title = "LAST STAND";
    float tw = menu_measure(a, title, title_size);
    menu_text(a, title, SCREEN_WIDTH / 2.0f - tw / 2, 120, title_size, GOLD);

    const char* subtitle = "Tower Defense";
    float sw = menu_measure(a, subtitle, 24);
    menu_text(a, subtitle, SCREEN_WIDTH / 2.0f - sw / 2, 180, 24, LIGHTGRAY);

    // Menu items
    int count = static_cast<int>(items_.size());
    float start_y = 280.0f;
    float spacing = 50.0f;
    // Center the menu vertically
    if (count <= 4) start_y = 300.0f;

    for (int i = 0; i < count; ++i) {
        float y = start_y + i * spacing;
        bool is_sel = (i == selected_);
        Color c = is_sel ? GOLD : LIGHTGRAY;
        float size = is_sel ? 28.0f : 24.0f;
        float iw = menu_measure(a, items_[i].label, size);
        menu_text(a, items_[i].label, SCREEN_WIDTH / 2.0f - iw / 2, y, size, c);
        if (is_sel) {
            menu_text(a, ">", SCREEN_WIDTH / 2.0f - iw / 2 - 30, y, size, GOLD);
        }
    }

    // Load error flash
    if (load_error_flash_ > 0) {
        float alpha = std::min(1.0f, load_error_flash_);
        Color err_col = {255, 100, 100, static_cast<unsigned char>(255 * alpha)};
        auto err_text = "No save file found!";
        float ew = menu_measure(a, err_text, 18);
        menu_text(a, err_text, SCREEN_WIDTH / 2.0f - ew / 2, start_y + count * spacing + 20, 18, err_col);
    }

    // Upgrade XP display
    if (game.upgrades.upgrade_xp > 0) {
        auto xp_text = std::format("Upgrade XP: {}", game.upgrades.upgrade_xp);
        float xw = menu_measure(a, xp_text.c_str(), 16);
        menu_text(a, xp_text.c_str(), SCREEN_WIDTH / 2.0f - xw / 2, 230, 16, {100, 200, 255, 200});
    }

    menu_text(a, "Press ENTER to select", SCREEN_WIDTH / 2.0f - 100, 580, 16, GRAY);
    menu_text(a, "v0.4.0 - Built with Raylib + EnTT", SCREEN_WIDTH / 2.0f - 130, static_cast<float>(SCREEN_HEIGHT - 30),
              14, DARKGRAY);
}

} // namespace ls
