#include "states/states.hpp"
#include "core/game.hpp"
#include "game/autopilot.hpp"
#include "game/render.hpp"
#include "game/sim.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <unordered_map>

namespace ls {

namespace {

constexpr Color GOLD_TINT{255, 205, 60, 255};
constexpr Color PANEL_FILL{28, 30, 44, 235};
constexpr Color PANEL_EDGE{90, 96, 130, 255};
constexpr Color GEM{120, 225, 255, 255};

const Theme& make_theme_cached(int stage) {
    static std::unordered_map<int, Theme> cache;
    auto it = cache.find(stage);
    if (it == cache.end()) it = cache.emplace(stage, make_theme(stage)).first;
    return it->second;
}

// Menus show a stage's battleground behind them. Grounds are render textures, so they are
// built from enter/update, never from inside a draw pass.
const Theme& backdrop_theme(Game& game, int stage) {
    const Theme& t = make_theme_cached(stage);
    if (!ground_ready(stage)) build_ground(game, t);
    return t;
}

void star(Vec2 c, float r, Color col) {
    Vector2 pts[12];
    pts[0] = c.to_raylib();
    for (int i = 0; i <= 10; ++i) {
        float a = -PI / 2 - static_cast<float>(i) * PI / 5.0f;
        float rr = (i % 2 == 0) ? r : r * 0.45f;
        pts[i + 1] = {c.x + std::cos(a) * rr, c.y + std::sin(a) * rr};
    }
    DrawTriangleFan(pts, 12, col);
}

void stars_row(Vec2 c, int filled, float r, float gap) {
    for (int i = 0; i < 3; ++i) {
        Vec2 p{c.x + static_cast<float>(i - 1) * gap, c.y + (i == 1 ? -r * 0.3f : 0.0f)};
        star(p, r + 3, Color{40, 30, 10, 255});
        star(p, r, i < filled ? GOLD_TINT : Color{70, 70, 80, 255});
    }
}

void gem_icon(Vec2 p, float r) {
    DrawPoly(p.to_raylib(), 4, r + 2, 0, Color{20, 60, 90, 255});
    DrawPoly(p.to_raylib(), 4, r, 0, GEM);
    DrawPoly((p + Vec2{-r * 0.2f, -r * 0.2f}).to_raylib(), 4, r * 0.35f, 0, Color{230, 250, 255, 255});
}

void gems_badge(Game& game) {
    std::string s = std::format("{}", game.meta.gems);
    float w = text_width(game.gfx, s, 32) + 70;
    Rectangle r{VIRTUAL_W - w - 20, 20, w, 56};
    DrawRectangleRounded(r, 0.5f, 8, Color{10, 14, 30, 210});
    gem_icon({r.x + 30, r.y + 28}, 16);
    text(game.gfx, s, {r.x + 54, r.y + 29}, 32, GEM, Align::Left);
}

void badge(Game& game, Vec2 p, int n) {
    if (n <= 0) return;
    float k = 1.0f + 0.1f * std::sin(game.time * 8.0f);
    disc(game.gfx, p, 20 * k, Color{120, 20, 20, 255});
    disc(game.gfx, p, 17 * k, Color{240, 60, 50, 255});
    text(game.gfx, std::format("{}", n), p + Vec2{0, 1}, 20, WHITE, Align::Center, false);
}

bool back_button(Game& game) {
    return button(game, {20, 20, 150, 60}, "BACK", Color{80, 90, 130, 255}, true, 26, KEY_ESCAPE);
}

int affordable_upgrades(const MetaProgress& m) {
    int n = 0;
    for (const auto& d : META_DEFS) n += m.can_buy(d.id) ? 1 : 0;
    for (const auto& u : UNIT_DEFS) n += (!m.owns(u.id) && m.gems >= u.unlock_cost) || m.can_upgrade_unit(u.id) ? 1 : 0;
    return n;
}

void screen_backdrop(Game& game, const Theme& theme) {
    draw_backdrop(game, theme, game.time);
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 8, 20, 170});
}

} // namespace

// ---------------------------------------------------------------- Menu

void MenuState::enter(Game& game) {
    game.play_music("menu");
    selected_stage_ = std::clamp(game.meta.unlocked_stage, 1, MAX_STAGE);
    backdrop_theme(game, selected_stage_);
    walkers_.clear();
    tracers_.clear();
    long long today = today_index();
    if (game.meta.daily_available(today)) {
        daily_open_ = true;
        int streak = game.meta.last_daily_day == today - 1 ? game.meta.daily_streak + 1 : 0;
        daily_amount_ = MetaProgress::daily_reward_for(streak);
    }
}

void MenuState::update(Game& game, float dt) {
    backdrop_theme(game, selected_stage_);
    if (autopilot_enabled()) {
        game.request = {autopilot_start_stage(), false};
        game.states.request(GameStateId::Playing);
    }
    spawn_timer_ -= dt;
    if (spawn_timer_ <= 0.0f) {
        spawn_timer_ = 0.18f;
        int type = GetRandomValue(0, 10);
        int t = type < 6 ? 0 : type < 8 ? 1 : type < 9 ? 2 : type < 10 ? 3 : 4;
        walkers_.push_back({{static_cast<float>(GetRandomValue(30, VIRTUAL_W - 30)), -30}, t, 0.0f, 3.0f});
    }
    for (auto& w : walkers_) {
        w.anim += dt;
        w.pos.y += (w.type == 1 ? 90.0f : 40.0f) * dt;
    }
    fire_timer_ -= dt;
    if (fire_timer_ <= 0.0f && !walkers_.empty()) {
        fire_timer_ = 0.05f;
        tracers_.push_back({{static_cast<float>(GetRandomValue(40, VIRTUAL_W - 40)), 1180}, 1.0f});
    }
    for (auto& t : tracers_) {
        t.pos.y -= 1400.0f * dt;
        t.life -= dt;
        for (auto& w : walkers_) {
            if (w.hp > 0 && std::abs(w.pos.x - t.pos.x) < 18 && std::abs(w.pos.y - t.pos.y) < 18) {
                w.hp -= 1.0f;
                t.life = 0;
                break;
            }
        }
    }
    std::erase_if(tracers_, [](const Tracer& t) { return t.life <= 0 || t.pos.y < -20; });
    std::erase_if(walkers_, [](const Walker& w) { return w.hp <= 0 || w.pos.y > 1080; });
}

void MenuState::render(Game& game) {
    const auto& g = game.gfx;
    auto& meta = game.meta;
    const Theme& theme = make_theme_cached(selected_stage_);
    draw_backdrop(game, theme, game.time);
    std::ranges::sort(walkers_, {}, [](const Walker& w) { return w.pos.y; });
    for (const auto& w : walkers_) draw_enemy_preview(game, w.type, w.pos.x, w.pos.y, 1.0f, w.anim);
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& t : tracers_) {
        line(t.pos, t.pos + Vec2{0, 18}, 5, Color{255, 230, 140, 160});
        glow(g, t.pos, 10, Color{255, 230, 140, 200});
    }
    EndBlendMode();
    DrawRectangleGradientV(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 8, 20, 120}, Color{8, 8, 20, 220});

    float bob = std::sin(game.time * 2.0f) * 6.0f;
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, {VIRTUAL_W * 0.5f, 230}, 340, Color{255, 120, 40, 60});
    EndBlendMode();
    text(g, "LAST", {VIRTUAL_W * 0.5f, 170 + bob}, 130, GOLD_TINT);
    text(g, "STAND", {VIRTUAL_W * 0.5f, 290 + bob}, 130, Color{255, 120, 60, 255});
    text(g, "HORDE DEFENSE", {VIRTUAL_W * 0.5f, 385}, 30, Color{230, 230, 240, 255});

    gems_badge(game);
    star({44, 48}, 20, GOLD_TINT);
    text(g, std::format("{}", meta.total_stars()), {72, 50}, 30, GOLD_TINT, Align::Left);

    Rectangle sp{80, 450, 560, 200};
    panel(sp, PANEL_FILL, PANEL_EDGE, 0.15f);
    bool locked = selected_stage_ > meta.unlocked_stage;
    text(g, std::format("STAGE {}", selected_stage_), {VIRTUAL_W * 0.5f, 500}, 52,
         locked ? Color{150, 150, 160, 255} : WHITE);
    text(g, theme.name, {VIRTUAL_W * 0.5f, 552}, 24, Color{170, 200, 230, 255});
    if (locked) {
        text(g, "LOCKED", {VIRTUAL_W * 0.5f, 606}, 28, Color{240, 90, 80, 255});
    } else {
        stars_row({VIRTUAL_W * 0.5f, 606}, meta.stars[static_cast<size_t>(selected_stage_)], 20, 52);
    }
    int k = daily_open_ ? 0 : 1;
    if (selected_stage_ > 1 && button(game, {96, 500, 80, 100}, "<", Color{70, 80, 120, 255}, true, 48, k * KEY_LEFT)) {
        selected_stage_--;
    }
    if (selected_stage_ < std::min(MAX_STAGE, meta.unlocked_stage + 1) &&
        button(game, {544, 500, 80, 100}, ">", Color{70, 80, 120, 255}, true, 48, k * KEY_RIGHT)) {
        selected_stage_++;
    }

    float pulse = 1.0f + 0.03f * std::sin(game.time * 5.0f);
    Rectangle play{VIRTUAL_W * 0.5f - 200 * pulse, 690, 400 * pulse, 120 * pulse};
    if (button(game, play, "PLAY", Color{60, 180, 80, 255}, !locked, 60, k * KEY_ENTER)) {
        game.request = {selected_stage_, false};
        game.states.request(GameStateId::Playing);
    }

    if (meta.endless_unlocked()) {
        if (button(game, {160, 840, 400, 80}, "ENDLESS", Color{140, 70, 200, 255}, true, 34, k * KEY_E)) {
            game.request = {1, true};
            game.states.request(GameStateId::Playing);
        }
        if (meta.stat(Lifetime::BestEndlessWave) > 0) {
            text(g, std::format("BEST: WAVE {}", meta.stat(Lifetime::BestEndlessWave)), {VIRTUAL_W * 0.5f, 940}, 20,
                 Color{200, 180, 240, 255});
        }
    } else {
        Rectangle r{160, 840, 400, 80};
        DrawRectangleRounded(r, 0.35f, 8, Color{40, 40, 52, 220});
        text(g, "ENDLESS", {VIRTUAL_W * 0.5f, 868}, 30, Color{140, 140, 150, 255});
        text(g, "CLEAR STAGE 3 TO UNLOCK", {VIRTUAL_W * 0.5f, 902}, 16, Color{140, 140, 150, 255}, Align::Center,
             false);
    }

    if (button(game, {50, 980, 300, 96}, "ARMORY", Color{200, 120, 40, 255}, true, 36, k * KEY_A)) {
        game.states.request(GameStateId::Armory);
    }
    badge(game, {340, 986}, affordable_upgrades(meta));
    if (button(game, {370, 980, 300, 96}, "MISSIONS", Color{50, 130, 200, 255}, true, 36, k * KEY_M)) {
        game.states.request(GameStateId::Missions);
    }
    badge(game, {660, 986}, meta.claimable_missions());

    auto toggle = [&](Rectangle br, const char* label, float& v, float on) {
        bool is_on = v > 0.0f;
        if (button(game, br, std::format("{} {}", label, is_on ? "ON" : "OFF"),
                   is_on ? Color{70, 80, 110, 255} : Color{50, 50, 60, 255}, true, 20)) {
            v = is_on ? 0.0f : on;
            game.apply_volumes();
            game.save();
        }
    };
    toggle({170, 1150, 180, 56}, "MUSIC", meta.music_volume, 0.5f);
    toggle({370, 1150, 180, 56}, "SFX", meta.sfx_volume, 0.8f);

    if (daily_open_) {
        DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{0, 0, 0, 180});
        Rectangle r{90, 380, 540, 480};
        panel(r, PANEL_FILL, GOLD_TINT, 0.1f, 5);
        text(g, "SUPPLY DROP!", {VIRTUAL_W * 0.5f, 440}, 48, GOLD_TINT);
        int streak = meta.last_daily_day == today_index() - 1 ? meta.daily_streak + 1 : 0;
        text(g, std::format("DAY {} STREAK", streak + 1), {VIRTUAL_W * 0.5f, 495}, 24, WHITE);
        for (int d = 0; d < 7; ++d) {
            Vec2 p{150.0f + static_cast<float>(d) * 70.0f, 570};
            bool got = d <= std::min(streak, 6);
            DrawRectangleRounded({p.x - 28, p.y - 34, 56, 68}, 0.3f, 6,
                                 got ? Color{60, 120, 180, 255} : Color{50, 50, 64, 255});
            gem_icon(p + Vec2{0, -8}, 10);
            text(g, std::format("{}", MetaProgress::daily_reward_for(d)), p + Vec2{0, 18}, 16, WHITE, Align::Center,
                 false);
        }
        float k = 1.0f + 0.08f * std::sin(game.time * 6.0f);
        gem_icon({VIRTUAL_W * 0.5f - 60, 680}, 30 * k);
        text(g, std::format("+{}", daily_amount_), {VIRTUAL_W * 0.5f + 20, 682}, 56, GEM);
        text(g, "COME BACK TOMORROW FOR MORE", {VIRTUAL_W * 0.5f, 740}, 18, Color{180, 180, 200, 255}, Align::Center,
             false);
        if (button(game, {VIRTUAL_W * 0.5f - 150, 770, 300, 70}, "CLAIM", Color{60, 180, 80, 255}, true, 34,
                   KEY_ENTER)) {
            meta.claim_daily(today_index());
            game.sound.play(Sfx::Gem, 1.0f);
            game.save();
            daily_open_ = false;
        }
        game.input.consumed = true;
    }
}

// ---------------------------------------------------------------- Playing

void PlayingState::enter(Game& game) {
    game.paused = false;
    start_run(game);
    build_ground(game, game.run->theme);
}

void PlayingState::exit(Game& game) {
    game.paused = false;
}

void PlayingState::update(Game& game, float dt) {
    auto& run = *game.run;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        if (run.build_slot >= 0) {
            run.build_slot = -1;
        } else if (!run.picking) {
            game.paused = !game.paused;
        }
    }
    if (game.paused) return;
    if (run.picking) {
        if (IsKeyPressed(KEY_ONE)) pick_card(game, 0);
        if (IsKeyPressed(KEY_TWO)) pick_card(game, 1);
        if (IsKeyPressed(KEY_THREE)) pick_card(game, 2);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) run.pick_focus = (run.pick_focus + 2) % 3;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) run.pick_focus = (run.pick_focus + 1) % 3;
    } else {
        float dir = 0.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) dir -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir += 1.0f;
        run.squad_target_x += dir * 720.0f * dt;
        if (IsKeyPressed(KEY_SPACE)) trigger_airstrike(game);
        if (IsKeyPressed(KEY_E)) trigger_frenzy(game);
        for (int s = 0; s < TURRET_SLOTS; ++s) {
            if (IsKeyPressed(KEY_ONE + s)) {
                run.build_slot = run.build_slot == s ? -1 : s;
                run.build_focus = 0;
            }
        }
    }
    int steps = autopilot_speed();
    for (int i = 0; i < steps && game.run; ++i) {
        if (autopilot_enabled()) autopilot_update(game, dt);
        update_run(game, dt);
    }
    if (game.run && game.run->ground_reset) {
        game.run->ground_reset = false;
        game.run->pending_decals.clear();
        build_ground(game, game.run->theme);
    }
    flush_decals(game);
}

void PlayingState::render(Game& game) {
    if (!game.run) return;
    render_world(game);
    render_hud(game);
}

// ---------------------------------------------------------------- Results

void ResultsState::enter(Game& game) {
    backdrop_theme(game, game.result.stage);
    autopilot_log_result(game);
    t_ = 0.0f;
    gems_shown_ = 0.0f;
    game.play_music("menu");
}

void ResultsState::update(Game& game, float dt) {
    t_ += dt;
    if (autopilot_enabled() && t_ > 2.5f) {
        autopilot_spend(game);
        const auto& r = game.result;
        game.request = {r.won && r.stage < MAX_STAGE ? r.stage + 1 : r.stage, r.endless};
        game.states.request(GameStateId::Playing);
    }
    float target = static_cast<float>(game.result.gems);
    if (t_ > 0.8f && gems_shown_ < target) {
        float before = gems_shown_;
        gems_shown_ = std::min(target, gems_shown_ + std::max(1.0f, target * 0.9f) * dt);
        if (static_cast<int>(before / 5) != static_cast<int>(gems_shown_ / 5)) game.sound.play(Sfx::Coin, 0.3f, 1.4f);
    }
}

void ResultsState::render(Game& game) {
    const auto& g = game.gfx;
    const auto& r = game.result;
    screen_backdrop(game, make_theme_cached(r.stage));
    float in = std::min(1.0f, t_ * 3.0f);
    const char* title = r.won ? "VICTORY!" : (r.endless ? "OVERRUN" : "DEFEAT");
    Color tc = r.won ? GOLD_TINT : Color{255, 90, 80, 255};
    text(g, title, {VIRTUAL_W * 0.5f, 170}, 96 * (0.6f + 0.4f * in), tc);
    std::string sub = r.endless ? std::format("SURVIVED {} WAVES", r.waves_cleared)
                      : r.won   ? std::format("STAGE {} CLEARED", r.stage)
                                : std::format("STAGE {}  -  REACHED WAVE {}", r.stage, r.waves_cleared + 1);
    text(g, sub, {VIRTUAL_W * 0.5f, 250}, 28, WHITE);

    if (r.won && !r.endless) {
        for (int i = 0; i < 3; ++i) {
            float appear = std::clamp((t_ - 0.4f - static_cast<float>(i) * 0.3f) * 4.0f, 0.0f, 1.0f);
            Vec2 p{VIRTUAL_W * 0.5f + static_cast<float>(i - 1) * 110.0f, i == 1 ? 330.0f : 350.0f};
            star(p, 46, Color{40, 30, 10, 255});
            if (i < r.stars && appear > 0)
                star(p, 42 * (1.0f + (1.0f - appear) * 0.8f), GOLD_TINT);
            else
                star(p, 40, Color{70, 70, 80, 255});
        }
    }

    Rectangle pr{70, 410, 580, 420};
    panel(pr, PANEL_FILL, PANEL_EDGE, 0.08f);
    const auto& t = r.tally;
    std::pair<const char*, std::string> rows[] = {
        {"ZOMBIES KILLED", std::format("{}", t.kills)},
        {"BIGGEST ARMY", std::format("{}", t.max_soldiers)},
        {"BEST COMBO", std::format("x{}", t.max_combo)},
        {"GATES TAKEN", std::format("{}", t.gates_taken)},
        {"BOSSES SLAIN", std::format("{}", t.bosses)},
        {"DAMAGE DEALT",
         t.damage_dealt >= 1e6 ? std::format("{:.1f}M", t.damage_dealt / 1e6) : std::format("{:.0f}", t.damage_dealt)},
    };
    for (size_t i = 0; i < std::size(rows); ++i) {
        float appear = std::clamp((t_ - 0.2f - static_cast<float>(i) * 0.08f) * 5.0f, 0.0f, 1.0f);
        float y = 460 + static_cast<float>(i) * 52.0f;
        text(g, rows[i].first, {110, y}, 24, with_alpha(Color{190, 195, 220, 255}, appear), Align::Left, false);
        text(g, rows[i].second, {610, y}, 28, with_alpha(WHITE, appear), Align::Right);
    }
    DrawLineEx({100, 772}, {620, 772}, 2, PANEL_EDGE);
    gem_icon({140, 800}, 18);
    text(g, "GEMS EARNED", {170, 800}, 26, GEM, Align::Left);
    text(g, std::format("+{}", static_cast<int>(gems_shown_)), {610, 800}, 36, GEM, Align::Right);

    float y = 860;
    if (r.new_unlock) {
        float k = 1.0f + 0.06f * std::sin(game.time * 6.0f);
        text(g, std::format("STAGE {} UNLOCKED!", r.stage + 1), {VIRTUAL_W * 0.5f, y}, 32 * k,
             Color{120, 255, 150, 255});
        y += 46;
    } else if (!r.won) {
        int n = affordable_upgrades(game.meta);
        text(g, n > 0 ? std::format("{} UPGRADES READY IN THE ARMORY", n) : "UPGRADE YOUR ARMY AND TRY AGAIN",
             {VIRTUAL_W * 0.5f, y}, 22, Color{255, 200, 120, 255});
        y += 40;
    }

    if (t_ < 0.6f) return;
    bool can_next = r.won && !r.endless && r.stage < MAX_STAGE;
    const char* main_label = can_next ? "NEXT STAGE" : "TRY AGAIN";
    if (button(game, {110, y + 10, 500, 100}, main_label, Color{60, 180, 80, 255}, true, 44, KEY_ENTER)) {
        game.request = {can_next ? r.stage + 1 : r.stage, r.endless};
        game.states.request(GameStateId::Playing);
    }
    if (button(game, {110, y + 130, 240, 80}, "ARMORY", Color{200, 120, 40, 255}, true, 30, KEY_A)) {
        game.states.request(GameStateId::Armory);
    }
    badge(game, {340, y + 136}, affordable_upgrades(game.meta));
    if (button(game, {370, y + 130, 240, 80}, "MENU", Color{80, 90, 130, 255}, true, 30, KEY_M)) {
        game.states.request(GameStateId::Menu);
    }
}

// ---------------------------------------------------------------- Armory

void ArmoryState::enter(Game& game) {
    tab_ = 0;
    backdrop_theme(game, game.meta.unlocked_stage);
}

void ArmoryState::render(Game& game) {
    const auto& g = game.gfx;
    auto& meta = game.meta;
    screen_backdrop(game, make_theme_cached(game.meta.unlocked_stage));
    if (back_button(game)) game.states.request(GameStateId::Menu);
    gems_badge(game);
    text(g, "ARMORY", {VIRTUAL_W * 0.5f, 140}, 64, GOLD_TINT);

    if (IsKeyPressed(KEY_TAB)) {
        tab_ ^= 1;
        focus_ = 0;
    }
    int rows = tab_ == 0 ? META_COUNT : UNIT_COUNT;
    if (IsKeyPressed(KEY_DOWN)) focus_ = (focus_ + 1) % rows;
    if (IsKeyPressed(KEY_UP)) focus_ = (focus_ + rows - 1) % rows;
    if (!game.touch_ui) key_badge(g, {380, 186}, "TAB");
    const char* tabs[2] = {"UPGRADES", "UNITS"};
    for (int i = 0; i < 2; ++i) {
        Rectangle r{60.0f + static_cast<float>(i) * 310.0f, 200, 290, 70};
        if (button(game, r, tabs[i], tab_ == i ? Color{200, 120, 40, 255} : Color{60, 64, 90, 255}, true, 28)) {
            tab_ = i;
            focus_ = 0;
        }
    }

    if (tab_ == 0) {
        for (int i = 0; i < META_COUNT; ++i) {
            const auto& d = META_DEFS[static_cast<size_t>(i)];
            Rectangle r{40, 300 + static_cast<float>(i) * 146.0f, 640, 132};
            panel(r, PANEL_FILL, focus_ == i ? GOLD_TINT : PANEL_EDGE, 0.15f, focus_ == i ? 4.0f : 3.0f);
            text(g, d.name, {r.x + 24, r.y + 34}, 30, WHITE, Align::Left);
            text(g, d.desc, {r.x + 24, r.y + 72}, 20, Color{190, 200, 220, 255}, Align::Left, false);
            int lvl = meta.level(d.id);
            float pw = 300.0f / static_cast<float>(d.max_level);
            for (int p = 0; p < d.max_level; ++p) {
                DrawRectangleRounded({r.x + 24 + static_cast<float>(p) * pw, r.y + 98, pw - 4, 14}, 0.4f, 4,
                                     p < lvl ? GOLD_TINT : Color{0, 0, 0, 120});
            }
            Rectangle br{r.x + r.width - 210, r.y + 26, 190, 80};
            if (meta.maxed(d.id)) {
                text(g, "MAXED", {br.x + br.width / 2, br.y + 40}, 30, GOLD_TINT);
            } else {
                bool afford = meta.can_buy(d.id);
                if (button(game, br, std::format("{}", meta.cost(d.id)),
                           afford ? Color{60, 170, 80, 255} : Color{80, 80, 90, 255}, afford, 32,
                           focus_ == i ? KEY_ENTER : 0, focus_ == i)) {
                    meta.buy(d.id);
                    game.sound.play(Sfx::Build, 0.9f);
                    game.save();
                }
                gem_icon({br.x + 22, br.y + 40}, 12);
            }
        }
    } else {
        int slots = meta.loadout_slots();
        text(g, "SQUAD", {60, 300}, 24, Color{200, 210, 230, 255}, Align::Left);
        for (int sl = 0; sl < LOADOUT_SLOTS; ++sl) {
            Rectangle r{170.0f + static_cast<float>(sl) * 160.0f, 276, 140, 64};
            bool open = sl < slots;
            DrawRectangleRounded(r, 0.3f, 6, open ? Color{30, 44, 40, 230} : Color{30, 30, 36, 200});
            DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2.0f, open ? Color{120, 220, 120, 255} : Color{70, 70, 80, 255});
            int id = meta.loadout[static_cast<size_t>(sl)];
            if (!open) {
                text(g, sl == 1 ? "STAGE 3" : "STAGE 5", {r.x + 70, r.y + 32}, 18, Color{130, 130, 140, 255},
                     Align::Center, false);
            } else if (id >= 0) {
                draw_soldier_preview(game, r.x + 30, r.y + 36, 1.3f, id);
                text(g, unit_def(static_cast<UnitType>(id)).name, {r.x + 54, r.y + 32}, 14, WHITE, Align::Left, false);
            } else {
                text(g, "EMPTY", {r.x + 70, r.y + 32}, 18, Color{150, 150, 160, 255}, Align::Center, false);
            }
        }
        for (int i = 0; i < UNIT_COUNT; ++i) {
            auto u = static_cast<UnitType>(i);
            const auto& def = unit_def(u);
            Rectangle r{40, 356 + static_cast<float>(i) * 146.0f, 640, 134};
            bool owned = meta.owns(u);
            bool fielded = meta.in_loadout(u);
            Color edge = focus_ == i ? GOLD_TINT : fielded ? Color{120, 220, 120, 255} : PANEL_EDGE;
            panel(r, fielded ? Color{36, 54, 44, 240} : PANEL_FILL, edge, 0.15f, focus_ == i ? 4.0f : 3.0f);
            for (int k = 0; k < 3; ++k) {
                draw_soldier_preview(game, r.x + 40 + static_cast<float>(k) * 34, r.y + 80 + (k == 1 ? -18.0f : 0.0f),
                                     2.0f, i);
            }
            text(g, def.name, {r.x + 150, r.y + 30}, 28, owned ? WHITE : Color{170, 170, 180, 255}, Align::Left);
            text(g, def.blurb, {r.x + 150, r.y + 62}, 18, Color{190, 200, 220, 255}, Align::Left, false);
            int lvl = meta.unit_level(u);
            float dmg = def.damage_mult * def.rate_mult * static_cast<float>(def.pellets) *
                        unit_damage_mult(std::max(1, lvl)) * unit_rate_mult(std::max(1, lvl));
            text(g,
                 std::format("POWER {:.0f}{}{}", dmg * 100.0f, def.bonus_pierce ? "  PIERCE" : "",
                             def.splash > 0            ? "  SPLASH"
                             : def.shot == Shot::Flame ? "  BURN"
                                                       : ""),
                 {r.x + 150, r.y + 92}, 16, Color{255, 200, 120, 255}, Align::Left, false);
            if (owned) {
                for (int p = 0; p < UNIT_MAX_LEVEL; ++p) {
                    DrawRectangleRounded({r.x + 150 + static_cast<float>(p) * 24.0f, r.y + 110, 20, 10}, 0.4f, 4,
                                         p < lvl ? GOLD_TINT : Color{0, 0, 0, 120});
                }
            }
            Rectangle br{r.x + r.width - 176, r.y + 14, 160, 60};
            if (!owned) {
                bool afford = meta.gems >= def.unlock_cost;
                if (button(game, br, std::format("{}", def.unlock_cost),
                           afford ? Color{200, 120, 40, 255} : Color{80, 80, 90, 255}, afford, 28,
                           focus_ == i ? KEY_ENTER : 0, focus_ == i)) {
                    meta.buy_unit(u);
                    game.sound.play(Sfx::LevelUp, 0.9f);
                    game.save();
                }
                gem_icon({br.x + 22, br.y + 30}, 11);
                text(g, "UNLOCK", {br.x + br.width / 2, br.y + 84}, 16, Color{200, 200, 210, 255}, Align::Center,
                     false);
            } else {
                if (lvl < UNIT_MAX_LEVEL) {
                    bool afford = meta.can_upgrade_unit(u);
                    if (button(game, br, std::format("{}", unit_upgrade_cost(lvl)),
                               afford ? Color{60, 170, 80, 255} : Color{80, 80, 90, 255}, afford, 28,
                               focus_ == i ? KEY_ENTER : 0, focus_ == i)) {
                        meta.upgrade_unit(u);
                        game.sound.play(Sfx::Build, 0.9f);
                        game.save();
                    }
                    gem_icon({br.x + 22, br.y + 30}, 11);
                } else {
                    text(g, "MAX", {br.x + br.width / 2, br.y + 30}, 28, GOLD_TINT);
                }
                Rectangle fb{r.x + r.width - 176, r.y + 82, 160, 40};
                if (button(game, fb, fielded ? "BENCH" : "FIELD",
                           fielded ? Color{90, 90, 110, 255} : Color{60, 120, 200, 255}, true, 20,
                           focus_ == i ? KEY_F : 0)) {
                    meta.toggle_loadout(u);
                    game.save();
                }
            }
        }
    }
}

// ---------------------------------------------------------------- Missions

void MissionsState::enter(Game& game) {
    backdrop_theme(game, game.meta.unlocked_stage);
}

void MissionsState::render(Game& game) {
    const auto& g = game.gfx;
    auto& meta = game.meta;
    screen_backdrop(game, make_theme_cached(game.meta.unlocked_stage));
    if (back_button(game)) game.states.request(GameStateId::Menu);
    gems_badge(game);
    text(g, "MISSIONS", {VIRTUAL_W * 0.5f, 140}, 64, Color{120, 200, 255, 255});
    bool first_claim_used = false;
    for (int i = 0; i < MISSION_COUNT; ++i) {
        const auto& m = MISSION_DEFS[static_cast<size_t>(i)];
        bool first_claim = !first_claim_used && meta.mission_claimable(i);
        first_claim_used |= first_claim;
        Rectangle r{30, 200 + static_cast<float>(i) * 104.0f, 660, 94};
        int tier = meta.mission_tier(i);
        bool done = tier >= 5;
        bool claim = meta.mission_claimable(i);
        panel(r, claim ? Color{50, 64, 40, 240} : PANEL_FILL, claim ? GOLD_TINT : PANEL_EDGE, 0.2f, 3);
        text(g, m.name, {r.x + 20, r.y + 26}, 24, WHITE, Align::Left);
        for (int s = 0; s < 5; ++s)
            star({r.x + 290 + static_cast<float>(s) * 24, r.y + 24}, 9, s < tier ? GOLD_TINT : Color{70, 70, 80, 255});
        long long have = meta.stat(m.stat);
        if (!done) {
            long long need = m.tiers[static_cast<size_t>(tier)];
            bar({r.x + 20, r.y + 52, 380, 22}, static_cast<float>(have) / static_cast<float>(need),
                Color{90, 180, 255, 255}, Color{0, 0, 0, 150});
            text(g, std::format("{} / {}", std::min(have, need), need), {r.x + 210, r.y + 64}, 16, WHITE);
            Rectangle br{r.x + r.width - 220, r.y + 14, 200, 66};
            if (claim) {
                if (button(game, br, std::format("CLAIM {}", meta.mission_reward(i)), Color{60, 180, 80, 255}, true, 24,
                           first_claim ? KEY_ENTER : 0)) {
                    int got = meta.claim_mission(i);
                    game.sound.play(Sfx::Gem, 1.0f);
                    game.toast(std::format("+{} GEMS", got), GEM);
                    game.save();
                }
            } else {
                gem_icon({br.x + 60, br.y + 33}, 12);
                text(g, std::format("{}", meta.mission_reward(i)), {br.x + 80, br.y + 34}, 26, GEM, Align::Left);
            }
        } else {
            text(g, "COMPLETE", {r.x + r.width - 120, r.y + 47}, 26, GOLD_TINT);
        }
    }
}

} // namespace ls
