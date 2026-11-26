#include "states/states.hpp"
#include "core/game.hpp"
#include "game/autopilot.hpp"
#include "game/kids.hpp"
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

const BossGenome& make_boss_cached(int stage) {
    static std::unordered_map<int, BossGenome> cache;
    auto it = cache.find(stage);
    if (it == cache.end()) it = cache.emplace(stage, make_boss(stage, false)).first;
    return it->second;
}

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
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 8, 20, 175});
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
        int type = GetRandomValue(0, 12);
        int t = type < 5 ? 0 : type < 7 ? 1 : type < 8 ? 2 : type < 9 ? 3 : type < 10 ? 4 : type < 11 ? 10 : 12;
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
    const BossGenome& boss = make_boss_cached(selected_stage_);
    const auto& hz = stage_hazard_def(theme.hazard);
    draw_backdrop(game, theme, game.time);
    std::ranges::sort(walkers_, {}, [](const Walker& w) { return w.pos.y; });
    for (const auto& w : walkers_) draw_enemy_preview(game, w.type, w.pos.x, w.pos.y, 1.0f, w.anim);
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& t : tracers_) {
        line(t.pos, t.pos + Vec2{0, 18}, 5, Color{255, 230, 140, 160});
        glow(g, t.pos, 10, Color{255, 230, 140, 200});
    }
    EndBlendMode();
    DrawRectangleGradientV(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 8, 20, 115}, Color{8, 8, 20, 225});

    float bob = std::sin(game.time * 2.0f) * 6.0f;
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, {VIRTUAL_W * 0.5f, 220}, 340, Color{255, 120, 40, 65});
    EndBlendMode();
    text(g, "LAST", {VIRTUAL_W * 0.5f, 158 + bob}, 126, GOLD_TINT);
    text(g, "STAND", {VIRTUAL_W * 0.5f, 272 + bob}, 126, Color{255, 120, 60, 255});
    text(g, "HORDE DEFENSE", {VIRTUAL_W * 0.5f, 356}, 28, Color{230, 230, 240, 255});
    text(g, "STARRING OMAR, ALI & SALAMA", {VIRTUAL_W * 0.5f, 392}, 20, GOLD_TINT);
    draw_kid(g, Kid::Omar, {82, 338}, 1.45f, KidPose::Wave, game.time);
    draw_kid(g, Kid::Ali, {640, 338}, 1.45f, std::fmod(game.time, 6.0f) < 3.0f ? KidPose::Peace : KidPose::Wave,
             game.time);

    gems_badge(game);
    star({44, 48}, 20, GOLD_TINT);
    text(g, std::format("{}", meta.total_stars()), {72, 50}, 30, GOLD_TINT, Align::Left);

    // Rich Stage Selection Panel showing Biome, Stage Hazard, Boss Preview, Clear Unlocks & Stars!
    Rectangle sp{58, 418, 604, 256};
    panel(sp, PANEL_FILL, theme.accent, 0.14f, 4.0f);
    bool locked = selected_stage_ > meta.unlocked_stage;
    text(g, std::format("STAGE {}  -  {}", selected_stage_, theme.name), {VIRTUAL_W * 0.5f, 456}, 33,
         locked ? Color{150, 150, 160, 255} : WHITE);
    // Hazard / Tactical Identity pill
    Rectangle hp{VIRTUAL_W * 0.5f - 196, 480, 392, 32};
    DrawRectangleRounded(hp, 0.5f, 6, Color{18, 20, 34, 220});
    DrawRectangleRoundedLinesEx(hp, 0.5f, 6, 2.0f, hz.accent);
    text_fit(g, theme.tagline.empty() ? std::format("{}  :  {}", hz.name, hz.short_desc) : theme.tagline,
             {VIRTUAL_W * 0.5f, hp.y + 16}, 14, hp.width - 18, hz.accent, false);
    // Stage boss preview line
    text_fit(g, std::format("BOSS: {}", boss.name), {VIRTUAL_W * 0.5f, 534}, 19, 380, Color{255, 145, 120, 255}, false);
    // Upcoming first-clear weapon/turret unlock teaser!
    std::string rw_preview = stage_reward_preview(selected_stage_);
    bool first_clear_pending = meta.stars[static_cast<size_t>(selected_stage_)] == 0;
    if (!rw_preview.empty() && first_clear_pending) {
        Rectangle rwp{VIRTUAL_W * 0.5f - 196, 554, 392, 30};
        DrawRectangleRounded(rwp, 0.5f, 6, Color{34, 48, 28, 225});
        DrawRectangleRoundedLinesEx(rwp, 0.5f, 6, 1.8f, GOLD_TINT);
        text_fit(g, std::format("UNLOCKS: {}", rw_preview), {VIRTUAL_W * 0.5f, rwp.y + 15}, 14, rwp.width - 18,
                 GOLD_TINT, false);
    }
    if (locked) {
        text(g, "LOCKED", {VIRTUAL_W * 0.5f, 624}, 28, Color{240, 90, 80, 255});
    } else {
        stars_row({VIRTUAL_W * 0.5f, 624}, meta.stars[static_cast<size_t>(selected_stage_)], 19, 52);
    }
    int k = daily_open_ ? 0 : 1;
    if (selected_stage_ > 1 && button(game, {74, 496, 76, 108}, "<", Color{70, 80, 120, 255}, true, 46, k * KEY_LEFT)) {
        selected_stage_--;
    }
    if (selected_stage_ < std::min(MAX_STAGE, meta.unlocked_stage + 1) &&
        button(game, {570, 496, 76, 108}, ">", Color{70, 80, 120, 255}, true, 46, k * KEY_RIGHT)) {
        selected_stage_++;
    }

    float pulse = 1.0f + 0.03f * std::sin(game.time * 5.0f);
    Rectangle play{VIRTUAL_W * 0.5f - 200 * pulse, 700, 400 * pulse, 114 * pulse};
    if (button(game, play, "PLAY", Color{60, 180, 80, 255}, !locked, 58, k * KEY_ENTER)) {
        game.request = {selected_stage_, false};
        game.states.request(GameStateId::Playing);
    }

    if (meta.endless_unlocked()) {
        if (button(game, {160, 842, 400, 76}, "ENDLESS", Color{140, 70, 200, 255}, true, 34, k * KEY_E)) {
            game.request = {1, true};
            game.states.request(GameStateId::Playing);
        }
        if (meta.stat(Lifetime::BestEndlessWave) > 0) {
            text(g, std::format("BEST: WAVE {}", meta.stat(Lifetime::BestEndlessWave)), {VIRTUAL_W * 0.5f, 940}, 20,
                 Color{200, 180, 240, 255});
        }
    } else {
        Rectangle r{160, 842, 400, 76};
        DrawRectangleRounded(r, 0.35f, 8, Color{40, 40, 52, 220});
        text(g, "ENDLESS", {VIRTUAL_W * 0.5f, 868}, 30, Color{140, 140, 150, 255});
        text(g, "CLEAR STAGE 3 TO UNLOCK", {VIRTUAL_W * 0.5f, 900}, 16, Color{140, 140, 150, 255}, Align::Center,
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
        float kf = 1.0f + 0.08f * std::sin(game.time * 6.0f);
        gem_icon({VIRTUAL_W * 0.5f - 60, 680}, 30 * kf);
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
        if (IsKeyPressed(KEY_W)) trigger_salama(game);
        if (IsKeyPressed(KEY_Q)) trigger_ali(game);
        for (int s = 0; s < TURRET_SLOTS; ++s) {
            if (IsKeyPressed(KEY_ONE + s)) {
                run.build_slot = run.build_slot == s ? -1 : s;
                run.build_focus = 0;
            }
        }
    }
    int steps = autopilot_speed();
    float step_dt = dt;
    if (!autopilot_enabled()) {
        if (game.sim_speed >= 1.9f) {
            steps = 2;
            step_dt = dt;
        } else if (game.sim_speed >= 1.4f) {
            steps = 2;
            step_dt = dt * 0.75f;
        }
    }
    for (int i = 0; i < steps && game.run; ++i) {
        if (autopilot_enabled()) autopilot_update(game, dt);
        update_run(game, step_dt);
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
    const char* title = (r.won && !r.endless && r.stage == MAX_STAGE) ? "CHAMPION!"
                        : r.won                                       ? "VICTORY!"
                                                                      : (r.endless ? "OVERRUN" : "DEFEAT");
    Color tc = r.won ? GOLD_TINT : Color{255, 90, 80, 255};
    text(g, title, {VIRTUAL_W * 0.5f, 114}, 88 * (0.6f + 0.4f * in), tc);
    std::string sub = r.endless                         ? std::format("SURVIVED {} WAVES", r.waves_cleared)
                      : (r.won && r.stage == MAX_STAGE) ? "ALL 10 STAGES CONQUERED!"
                      : r.won ? std::format("STAGE {} CLEARED", r.stage)
                              : std::format("STAGE {}  -  REACHED WAVE {}", r.stage, r.waves_cleared + 1);
    text(g, sub, {VIRTUAL_W * 0.5f, 180}, 26, WHITE);

    if (r.won && !r.endless) {
        for (int i = 0; i < 3; ++i) {
            float appear = std::clamp((t_ - 0.4f - static_cast<float>(i) * 0.3f) * 4.0f, 0.0f, 1.0f);
            Vec2 p{VIRTUAL_W * 0.5f + static_cast<float>(i - 1) * 106.0f, i == 1 ? 242.0f : 258.0f};
            star(p, 42, Color{40, 30, 10, 255});
            if (i < r.stars && appear > 0)
                star(p, 38 * (1.0f + (1.0f - appear) * 0.8f), GOLD_TINT);
            else
                star(p, 36, Color{70, 70, 80, 255});
        }
    }
    if (r.won) {
        // Family celebration row: Omar, Dad Salama, and Ali cheering together!
        draw_kid(g, Kid::Omar, {254, 392}, 1.12f, KidPose::Cheer, game.time);
        draw_kid(g, Kid::Salama, {360, 392}, 1.15f, KidPose::Cheer, game.time + 0.2f);
        draw_kid(g, Kid::Ali, {466, 392}, 1.12f, KidPose::Peace, game.time + 0.4f);
    }

    Rectangle pr{70, 398, 580, 336};
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
        float ry = 434 + static_cast<float>(i) * 40.0f;
        text(g, rows[i].first, {110, ry}, 22, with_alpha(Color{190, 195, 220, 255}, appear), Align::Left, false);
        text(g, rows[i].second, {610, ry}, 26, with_alpha(WHITE, appear), Align::Right);
    }
    DrawLineEx({100, 672}, {620, 672}, 2, PANEL_EDGE);
    gem_icon({140, 704}, 17);
    text(g, "GEMS EARNED", {170, 704}, 25, GEM, Align::Left);
    text(g, std::format("+{}", static_cast<int>(gems_shown_)), {610, 704}, 34, GEM, Align::Right);

    float y = 766;
    if (!r.unlocks.empty()) {
        // Big celebratory New Weapons & Unlocks reward card!
        float uh = 46.0f + static_cast<float>(r.unlocks.size()) * 30.0f;
        Rectangle ur{70, 748, 580, uh};
        panel(ur, Color{34, 48, 28, 244}, GOLD_TINT, 0.14f, 3.5f);
        text(g, "NEW WEAPONS & UNLOCKS!", {VIRTUAL_W * 0.5f, ur.y + 23}, 23, GOLD_TINT);
        for (size_t i = 0; i < r.unlocks.size(); ++i) {
            float uy = ur.y + 52.0f + static_cast<float>(i) * 30.0f;
            star({112, uy}, 8, GOLD_TINT);
            text_fit(g, r.unlocks[i], {VIRTUAL_W * 0.5f, uy}, 20, ur.width - 76, Color{165, 255, 175, 255});
            star({608, uy}, 8, GOLD_TINT);
        }
        y = ur.y + uh + 28.0f;
    }
    if (r.endless && r.waves_cleared > 0 && r.waves_cleared >= game.meta.stat(Lifetime::BestEndlessWave)) {
        float k = 1.0f + 0.05f * std::sin(game.time * 6.0f);
        text(g, std::format("NEW ENDLESS RECORD: WAVE {}!", r.waves_cleared), {VIRTUAL_W * 0.5f, y}, 28 * k, GOLD_TINT);
        y += 42;
    } else if (r.new_unlock) {
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
    text(g, "ARMORY", {VIRTUAL_W * 0.5f, 108}, 54, GOLD_TINT);

    if (IsKeyPressed(KEY_TAB)) {
        tab_ ^= 1;
        focus_ = 0;
    }
    int count = tab_ == 0 ? META_COUNT : UNIT_COUNT;
    if (IsKeyPressed(KEY_DOWN)) focus_ = (focus_ + 2) % count;
    if (IsKeyPressed(KEY_UP)) focus_ = (focus_ + count - 2) % count;
    if (IsKeyPressed(KEY_RIGHT)) focus_ = (focus_ + 1) % count;
    if (IsKeyPressed(KEY_LEFT)) focus_ = (focus_ + count - 1) % count;
    focus_ = std::clamp(focus_, 0, count - 1);

    if (!game.touch_ui) key_badge(g, {360, 146}, "TAB");
    const char* tabs[2] = {"UPGRADES", "SQUAD & UNITS"};
    for (int i = 0; i < 2; ++i) {
        Rectangle r{60.0f + static_cast<float>(i) * 310.0f, 156, 290, 56};
        if (button(game, r, tabs[i], tab_ == i ? Color{200, 120, 40, 255} : Color{60, 64, 90, 255}, true, 24)) {
            tab_ = i;
            focus_ = 0;
        }
    }

    if (tab_ == 0) {
        // 2-column grid for all 8 permanent Armory upgrades!
        for (int i = 0; i < META_COUNT; ++i) {
            const auto& d = META_DEFS[static_cast<size_t>(i)];
            int col = i % 2;
            int row = i / 2;
            Rectangle r{30.0f + static_cast<float>(col) * 336.0f, 240.0f + static_cast<float>(row) * 246.0f, 324.0f,
                        228.0f};
            if (pointer_in(game, r)) focus_ = i;
            panel(r, PANEL_FILL, focus_ == i ? GOLD_TINT : PANEL_EDGE, 0.14f, focus_ == i ? 4.0f : 3.0f);
            text_fit(g, d.name, {r.x + r.width * 0.5f, r.y + 34}, 26, r.width - 24, WHITE);
            text_fit(g, d.desc, {r.x + r.width * 0.5f, r.y + 68}, 18, r.width - 24, Color{190, 205, 228, 255}, false);
            int lvl = meta.level(d.id);
            float pw = (r.width - 36.0f) / static_cast<float>(d.max_level);
            for (int p = 0; p < d.max_level; ++p) {
                DrawRectangleRounded({r.x + 18 + static_cast<float>(p) * pw, r.y + 100, std::max(4.0f, pw - 3), 13},
                                     0.4f, 4, p < lvl ? GOLD_TINT : Color{0, 0, 0, 125});
            }
            text(g, std::format("LV {} / {}", lvl, d.max_level), {r.x + r.width * 0.5f, r.y + 130}, 16,
                 Color{180, 190, 210, 255}, Align::Center, false);
            Rectangle br{r.x + 28, r.y + 154, r.width - 56, 56};
            if (meta.maxed(d.id)) {
                text(g, "MAXED", {br.x + br.width / 2, br.y + 28}, 26, GOLD_TINT);
            } else {
                bool afford = meta.can_buy(d.id);
                if (button(game, br, std::format("   {}", meta.cost(d.id)),
                           afford ? Color{60, 170, 80, 255} : Color{80, 80, 90, 255}, afford, 26,
                           focus_ == i ? KEY_ENTER : 0, focus_ == i)) {
                    meta.buy(d.id);
                    game.sound.play(Sfx::Build, 0.9f);
                    game.save();
                }
                gem_icon({br.x + 28, br.y + 28}, 11);
            }
        }
    } else {
        // Clear Active Squad header explaining how fielding works + clickable slots to bench!
        int slots = meta.loadout_slots();
        int fielded_count = static_cast<int>(meta.squad().size());
        text_fit(g,
                 std::format("ACTIVE SQUAD ({}/{})  -  SOLDIERS SPLIT ACROSS FIELDED UNITS  (TAP SLOT TO REMOVE)",
                             fielded_count, slots),
                 {VIRTUAL_W * 0.5f, 228}, 14, VIRTUAL_W - 36, Color{205, 220, 245, 255}, false);
        for (int sl = 0; sl < LOADOUT_SLOTS; ++sl) {
            Rectangle r{30.0f + static_cast<float>(sl) * 168.0f, 242, 156, 56};
            bool open = sl < slots;
            int id = meta.loadout[static_cast<size_t>(sl)];
            bool filled = open && id >= 0 && id < UNIT_COUNT;
            Color fill = !open ? Color{26, 26, 34, 200} : filled ? Color{30, 54, 44, 235} : Color{32, 36, 52, 220};
            Color edge = !open    ? Color{70, 70, 82, 255}
                         : filled ? Color{110, 230, 130, 255}
                                  : Color{110, 145, 210, 220};
            DrawRectangleRounded(r, 0.3f, 6, fill);
            DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2.0f, edge);
            if (!open) {
                const char* req = sl == 1 ? "UNLOCK STG 2" : sl == 2 ? "UNLOCK STG 4" : "UNLOCK STG 6";
                text(g, req, {r.x + 78, r.y + 28}, 14, Color{135, 135, 150, 255}, Align::Center, false);
            } else if (filled) {
                auto u = static_cast<UnitType>(id);
                begin_scissor(r.x + 2, r.y + 2, r.width - 4, r.height - 4);
                draw_soldier_preview(game, r.x + 25, r.y + 39, 0.90f, id);
                EndScissorMode();
                text_fit(g, unit_def(u).name, {r.x + 92, r.y + 20}, 13, 88, WHITE, false);
                text(g, std::format("LV {}", meta.unit_level(u)), {r.x + 92, r.y + 39}, 13, GOLD_TINT, Align::Center,
                     false);
                // Small 'X' chip indicating clicking removes from squad
                disc(g, {r.x + r.width - 12, r.y + 12}, 9, Color{165, 45, 45, 220});
                text(g, "x", {r.x + r.width - 12, r.y + 11}, 12, WHITE, Align::Center, false);
                if (hit(game, r)) {
                    if (fielded_count > 1) {
                        meta.toggle_loadout(u);
                        game.sound.play(Sfx::Click, 0.8f);
                        game.toast(std::format("BENCHED {}", unit_def(u).name), Color{190, 205, 230, 255});
                        game.save();
                    } else {
                        game.sound.play(Sfx::GateBad, 0.6f);
                        game.toast("KEEP AT LEAST 1 UNIT IN SQUAD", Color{255, 160, 80, 255});
                    }
                }
            } else {
                text(g, "+ OPEN SLOT", {r.x + 78, r.y + 28}, 14, Color{150, 185, 235, 255}, Align::Center, false);
            }
        }
        // Active squad synergy bar
        auto syn = compute_synergies(meta.squad());
        Rectangle syn_bar{30, 304, VIRTUAL_W - 60, 28};
        DrawRectangleRounded(syn_bar, 0.45f, 6, syn.count() > 0 ? Color{24, 52, 38, 220} : Color{24, 28, 44, 210});
        DrawRectangleRoundedLinesEx(syn_bar, 0.45f, 6, 1.5f, syn.count() > 0 ? Color{110, 235, 145, 220} : PANEL_EDGE);
        std::string syn_str;
        auto add_syn = [&](const char* s) {
            if (!syn_str.empty()) syn_str += "  |  ";
            syn_str += s;
        };
        if (syn.combined_arms) add_syn("COMBINED ARMS +15% DMG");
        if (syn.bullet_storm) add_syn("BULLET HOSE +15% ROF");
        if (syn.heavy_ordnance) add_syn("ORDNANCE +25% KNOCK");
        if (syn.elemental_fury) add_syn("ELEMENTAL +22% ELEM");
        if (syn_str.empty()) syn_str = "SYNERGIES: DEPLOY 3+ CLASSES (+15% DMG) OR 2 ELEMENTALS (+22%)";
        text_fit(g, syn_str, {VIRTUAL_W * 0.5f, syn_bar.y + 14}, 14, syn_bar.width - 18,
                 syn.count() > 0 ? Color{140, 250, 170, 255} : Color{175, 190, 220, 220}, false);

        static const char* ROLE_TAG[UNIT_COUNT] = {"ALL-ROUND", "RAPID", "BREAKER", "ELITE",
                                                   "SPLASH",    "BURN",  "FREEZE",  "CHAIN"};
        static const Color ROLE_COL[UNIT_COUNT] = {{110, 200, 110, 255}, {110, 175, 245, 255}, {235, 155, 85, 255},
                                                   {245, 225, 110, 255}, {195, 210, 95, 255},  {255, 120, 65, 255},
                                                   {120, 230, 255, 255}, {195, 140, 255, 255}};

        // 2-column grid for all 8 Soldier Classes!
        for (int i = 0; i < UNIT_COUNT; ++i) {
            auto u = static_cast<UnitType>(i);
            const auto& def = unit_def(u);
            int col = i % 2;
            int row = i / 2;
            Rectangle r{30.0f + static_cast<float>(col) * 336.0f, 342.0f + static_cast<float>(row) * 232.0f, 324.0f,
                        220.0f};
            if (pointer_in(game, r)) focus_ = i;
            bool owned = meta.owns(u);
            bool fielded = meta.in_loadout(u);
            Color edge = focus_ == i ? GOLD_TINT : fielded ? Color{110, 230, 130, 255} : PANEL_EDGE;
            panel(r, fielded ? Color{32, 54, 42, 242} : PANEL_FILL, edge, 0.14f, focus_ == i ? 4.0f : 3.0f);
            begin_scissor(r.x + 4, r.y + 4, 86, 102);
            draw_soldier_preview(game, r.x + 44, r.y + 72, 1.52f, i);
            EndScissorMode();

            // Unit Name + Role badge + Level pill
            text_fit(g, def.name, {r.x + 174, r.y + 24}, 21, 162, owned ? WHITE : Color{175, 175, 190, 255});
            Rectangle rpill{r.x + 252, r.y + 13, 62, 22};
            DrawRectangleRounded(rpill, 0.5f, 4, Color{18, 20, 32, 210});
            DrawRectangleRoundedLinesEx(rpill, 0.5f, 4, 1.5f, ROLE_COL[i]);
            text_fit(g, ROLE_TAG[i], {rpill.x + rpill.width * 0.5f, rpill.y + 11}, 11, 56, ROLE_COL[i], false);

            text_fit(g, def.blurb, {r.x + 200, r.y + 52}, 14, 215, Color{200, 212, 232, 255}, false);
            text_fit(g, def.passive, {r.x + 200, r.y + 74}, 13, 218, Color{145, 225, 255, 255}, false);
            int lvl = meta.unit_level(u);
            float spec = 1.0f + 0.35f * static_cast<float>(std::min(def.bonus_pierce, 4));
            if (def.splash > 0.0f) spec *= 2.8f;
            if (def.shot == Shot::Flame) spec = 2.4f;
            if (def.shot == Shot::Cryo) spec *= 1.55f;
            if (def.shot == Shot::Volt) spec *= 1.85f;
            if (u == UnitType::Sniper) spec *= 1.45f;
            float dmg = def.damage_mult * def.rate_mult * static_cast<float>(def.pellets) * spec *
                        unit_damage_mult(std::max(1, lvl)) * unit_rate_mult(std::max(1, lvl));
            text(g,
                 owned ? std::format("LV {}  PWR {:.0f}", lvl, dmg * 100.0f) : std::format("PWR {:.0f}", dmg * 100.0f),
                 {r.x + 16, r.y + 108}, 14, Color{255, 210, 110, 255}, Align::Left, false);
            if (owned) {
                for (int p = 0; p < UNIT_MAX_LEVEL; ++p) {
                    DrawRectangleRounded({r.x + 132 + static_cast<float>(p) * 17.8f, r.y + 104, 14, 9}, 0.4f, 4,
                                         p < lvl ? GOLD_TINT : Color{0, 0, 0, 125});
                }
            } else {
                int free_stg = unit_free_clear_stage(u);
                text(g, free_stg > 0 ? std::format("FREE AFTER STG {}", free_stg) : "LOCKED", {r.x + 306, r.y + 108},
                     13, Color{140, 235, 165, 235}, Align::Right, false);
            }
            if (!owned) {
                Rectangle br{r.x + 24, r.y + 142, r.width - 48, 56};
                bool afford = meta.gems >= def.unlock_cost;
                if (button(game, br, std::format("UNLOCK    {}", def.unlock_cost),
                           afford ? Color{210, 125, 40, 255} : Color{80, 80, 90, 255}, afford, 22,
                           focus_ == i ? KEY_ENTER : 0, focus_ == i)) {
                    meta.buy_unit(u);
                    game.sound.play(Sfx::LevelUp, 0.9f);
                    game.toast(std::format("UNLOCKED {}!", def.name), GOLD_TINT);
                    game.save();
                }
                gem_icon({br.x + 26, br.y + 28}, 10);
            } else {
                Rectangle br{r.x + 14, r.y + 142, 158, 56};
                if (lvl < UNIT_MAX_LEVEL) {
                    bool afford = meta.can_upgrade_unit(u);
                    if (button(game, br, std::format("LV UP  {}", unit_upgrade_cost(lvl)),
                               afford ? Color{60, 175, 85, 255} : Color{80, 80, 90, 255}, afford, 19,
                               focus_ == i ? KEY_ENTER : 0, focus_ == i)) {
                        meta.upgrade_unit(u);
                        game.sound.play(Sfx::Build, 0.9f);
                        game.toast(std::format("{} LV {}!", def.name, meta.unit_level(u)), Color{130, 250, 160, 255});
                        game.save();
                    }
                    gem_icon({br.x + 16, br.y + 28}, 8);
                } else {
                    text(g, "MAX LV 10", {br.x + br.width * 0.5f, br.y + 28}, 20, GOLD_TINT);
                }
                Rectangle fb{r.x + 180, r.y + 142, 130, 56};
                if (button(game, fb, fielded ? "IN SQUAD" : "DEPLOY",
                           fielded ? Color{70, 120, 88, 255} : Color{56, 122, 206, 255}, true, 18,
                           focus_ == i ? KEY_F : 0)) {
                    if (fielded && fielded_count <= 1) {
                        game.sound.play(Sfx::GateBad, 0.6f);
                        game.toast("KEEP AT LEAST 1 UNIT IN SQUAD", Color{255, 160, 80, 255});
                    } else {
                        meta.toggle_loadout(u);
                        game.toast(meta.in_loadout(u) ? std::format("DEPLOYED {}!", def.name)
                                                      : std::format("BENCHED {}", def.name),
                                   Color{150, 225, 255, 255});
                        game.save();
                    }
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
