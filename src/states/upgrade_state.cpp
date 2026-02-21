#include "upgrade_state.hpp"
#include "core/game.hpp"
#include "core/asset_paths.hpp"
#include <format>
#include <cmath>

namespace ls {

static void up_text(AssetManager& a, const char* text, float x, float y, float size, Color color) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        DrawTextEx(*font, text, {x, y}, size, 1.0f, color);
    } else {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
    }
}

static float up_measure(AssetManager& a, const char* text, float size) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) return MeasureTextEx(*font, text, size, 1.0f).x;
    return static_cast<float>(MeasureText(text, static_cast<int>(size)));
}

void UpgradeState::enter(Game& game) {
    game.upgrades = game.save_manager.load_upgrades("upgrades.json");
    selected_ = 0;
}

void UpgradeState::update(Game& game, [[maybe_unused]] float dt) {
    if (game.current_music) UpdateMusicStream(*game.current_music);

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_ = (selected_ + 1) % 5;
        game.sounds.play(game.sounds.ui_click, 0.6f);
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_ = (selected_ + 4) % 5;
        game.sounds.play(game.sounds.ui_click, 0.6f);
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        auto& u = game.upgrades;
        int* levels[] = {&u.attack_range_level, &u.magnet_level, &u.attack_damage_level,
                         &u.attack_speed_level, &u.max_hp_level};
        int& lev = *levels[selected_];
        int c = u.cost(lev);
        if (lev < HeroUpgrades::MAX_LEVEL && c > 0 && u.upgrade_xp >= c) {
            u.upgrade_xp -= c;
            lev++;
            game.save_manager.save_upgrades(u, "upgrades.json");
            game.sounds.play(game.sounds.ui_click);
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        game.state_machine.change_state(GameStateId::Menu, game);
    }
}

void UpgradeState::render(Game& game) {
    ClearBackground({20, 20, 35, 255});
    auto& a = game.assets;
    auto& u = game.upgrades;

    // Title
    const char* title = "HERO UPGRADES";
    float tw = up_measure(a, title, 40);
    up_text(a, title, SCREEN_WIDTH / 2.0f - tw / 2, 60, 40, GOLD);

    // XP budget
    auto xp_str = std::format("XP: {}", u.upgrade_xp);
    float xw = up_measure(a, xp_str.c_str(), 24);
    up_text(a, xp_str.c_str(), SCREEN_WIDTH / 2.0f - xw / 2, 115, 24, {100, 200, 255, 255});

    // Upgrade rows
    const char* names[] = {"Attack Range", "Coin Magnet", "Attack Damage", "Attack Speed", "Max HP"};
    const char* descs[] = {"+30 range/lv", "+40 pickup/lv", "+5 dmg/lv", "-0.04s cd/lv", "+40 HP/lv"};
    int levels[] = {u.attack_range_level, u.magnet_level, u.attack_damage_level,
                    u.attack_speed_level, u.max_hp_level};

    float start_y = 170;
    float row_h = 60;
    float box_x = SCREEN_WIDTH / 2.0f - 250;
    float box_w = 500;

    for (int i = 0; i < 5; ++i) {
        float y = start_y + i * row_h;
        bool is_selected = (i == selected_);
        int lev = levels[i];
        int c = u.cost(lev);
        bool affordable = (lev < HeroUpgrades::MAX_LEVEL && c > 0 && u.upgrade_xp >= c);
        bool maxed = (lev >= HeroUpgrades::MAX_LEVEL);

        // Background
        Color bg = is_selected ? Color{40, 50, 70, 220} : Color{30, 30, 45, 200};
        DrawRectangle(static_cast<int>(box_x), static_cast<int>(y), static_cast<int>(box_w), static_cast<int>(row_h - 4), bg);

        if (is_selected) {
            Color border = affordable ? GREEN : Color{100, 100, 120, 255};
            DrawRectangleLinesEx({box_x, y, box_w, row_h - 4}, 2.0f, border);
        }

        // Name
        Color name_color = is_selected ? WHITE : LIGHTGRAY;
        up_text(a, names[i], box_x + 15, y + 8, 20, name_color);

        // Description
        up_text(a, descs[i], box_x + 15, y + 32, 14, GRAY);

        // Level pips
        float pip_x = box_x + 250;
        for (int p = 0; p < HeroUpgrades::MAX_LEVEL; ++p) {
            Color pip_color = (p < lev) ? GOLD : Color{60, 60, 80, 255};
            DrawRectangle(static_cast<int>(pip_x + p * 22), static_cast<int>(y + 12), 18, 18, pip_color);
            DrawRectangleLinesEx({pip_x + p * 22, y + 12, 18, 18}, 1.0f, {100, 100, 120, 255});
        }

        // Cost
        float cost_x = box_x + box_w - 120;
        if (maxed) {
            up_text(a, "MAXED", cost_x, y + 16, 18, GOLD);
        } else {
            auto cost_str = std::format("Cost: {}", c);
            Color cost_color = affordable ? GREEN : Color{150, 80, 80, 255};
            up_text(a, cost_str.c_str(), cost_x, y + 16, 16, cost_color);
        }
    }

    // Instructions
    up_text(a, "UP/DOWN to select, ENTER to buy, ESC to return",
            SCREEN_WIDTH / 2.0f - 220, static_cast<float>(SCREEN_HEIGHT - 50), 16, GRAY);
}

} // namespace ls
