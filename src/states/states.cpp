#include "states/states.hpp"
#include "core/game.hpp"
#include "game/autopilot.hpp"
#include "game/kids.hpp"
#include "game/render.hpp"
#include "game/render_parts.hpp"
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

void draw_gem_button_label(const Gfx& g, Rectangle r, bool pressed, std::string_view label, float font_size,
                           float gem_r, bool enabled) {
    float tw = text_width(g, label, font_size);
    float gap = 7.0f;
    float gem_d = gem_r * 2.0f;
    float total = gem_d + gap + tw;
    float left = r.x + (r.width - total) * 0.5f;
    float cy = r.y + r.height * 0.5f + (pressed ? 6.0f : 0.0f);
    gem_icon({left + gem_r, cy}, gem_r);
    text(g, label, {left + gem_d + gap, cy}, font_size, enabled ? WHITE : Color{200, 200, 200, 255}, Align::Left);
}

void draw_info_dot(const Gfx& g, Vec2 p, bool active) {
    disc(g, p, 10.5f, active ? Color{70, 120, 195, 240} : Color{46, 54, 80, 215});
    text(g, "i", p + Vec2{0, 0.5f}, 13, active ? WHITE : Color{190, 210, 240, 255}, Align::Center, false);
}

void draw_turret_mini(const Gfx& g, TurretType tt, Vec2 p, float scale, float time) {
    Camera2D cam{};
    cam.offset = p.to_raylib();
    cam.target = {0.0f, 0.0f};
    cam.zoom = scale;
    begin_camera(cam);
    draw_turret(g, tt, 1, {0.0f, 0.0f}, -PI * 0.5f, 0.0f, time, 0.0f);
    end_camera();
}

const char* hazard_short_label(StageHazard h, int twist) {
    switch (twist) {
    case 1:
        return "BLOOD MOON";
    case 2:
        return "PHARAOH";
    case 3:
        return "HELLFIRE";
    case 4:
        return "ECLIPSE";
    default:
        break;
    }
    switch (h) {
    case StageHazard::Bountiful:
        return "BOUNTIFUL";
    case StageHazard::Sandstorm:
        return "SANDSTORM";
    case StageHazard::Permafrost:
        return "PERMAFROST";
    case StageHazard::Magma:
        return "MAGMA";
    case StageHazard::ToxicMire:
        return "TOXIC MIRE";
    case StageHazard::Leylines:
        return "LEYLINES";
    }
    return "HAZARD";
}

void draw_hazard_icon(const Gfx& g, StageHazard h, int twist, Vec2 c, float r, float time) {
    float pulse = 0.5f + 0.5f * std::sin(time * 3.5f);
    if (twist == 1) {
        // Blood Moon crescent
        glow(g, c, r * 1.45f, Color{255, 65, 85, static_cast<unsigned char>(95 + 40 * pulse)});
        disc(g, c, r * 0.85f, Color{255, 78, 92, 255});
        disc(g, c + Vec2{r * 0.34f, -r * 0.14f}, r * 0.72f, Color{20, 23, 36, 255});
        return;
    }
    if (twist == 2) {
        // Pharaoh's golden pyramid & relic eye
        glow(g, c, r * 1.4f, Color{255, 195, 70, 95});
        DrawTriangle({c.x, c.y - r * 0.88f}, {c.x - r * 0.92f, c.y + r * 0.72f}, {c.x + r * 0.92f, c.y + r * 0.72f},
                     Color{245, 185, 65, 255});
        DrawTriangle({c.x, c.y - r * 0.88f}, {c.x, c.y + r * 0.72f}, {c.x + r * 0.92f, c.y + r * 0.72f},
                     Color{195, 138, 42, 255});
        disc(g, c + Vec2{0, r * 0.1f}, r * 0.24f, Color{90, 230, 255, 255});
        return;
    }
    if (twist == 3) {
        // Hellfire double blaze
        glow(g, c, r * 1.5f, Color{255, 80, 30, static_cast<unsigned char>(110 + 40 * pulse)});
        DrawTriangle({c.x - r * 0.32f, c.y - r * 0.86f}, {c.x - r * 0.82f, c.y + r * 0.6f},
                     {c.x + r * 0.2f, c.y + r * 0.6f}, Color{255, 72, 34, 255});
        DrawTriangle({c.x + r * 0.32f, c.y - r * 0.94f}, {c.x - r * 0.2f, c.y + r * 0.6f},
                     {c.x + r * 0.84f, c.y + r * 0.6f}, Color{255, 115, 42, 255});
        disc(g, c + Vec2{0, r * 0.28f}, r * 0.45f, Color{255, 215, 80, 255});
        return;
    }
    if (twist == 4) {
        // Golden Eclipse ring
        glow(g, c, r * 1.55f, Color{255, 200, 70, static_cast<unsigned char>(115 + 45 * pulse)});
        disc(g, c, r * 0.86f, Color{255, 205, 75, 255});
        disc(g, c, r * 0.66f, Color{24, 18, 46, 255});
        ring(g, c, r * 0.92f, Color{255, 235, 150, 220});
        return;
    }
    switch (h) {
    case StageHazard::Bountiful:
        // Golden coin + green sprout
        glow(g, c, r * 1.35f, Color{135, 230, 100, 90});
        disc(g, c + Vec2{-r * 0.12f, r * 0.14f}, r * 0.64f, Color{245, 195, 55, 255});
        disc(g, c + Vec2{-r * 0.12f, r * 0.14f}, r * 0.44f, Color{255, 228, 110, 255});
        ellipse(g, c + Vec2{r * 0.45f, -r * 0.38f}, r * 0.36f, r * 0.22f, Color{110, 225, 95, 255});
        line(c + Vec2{r * 0.12f, -r * 0.1f}, c + Vec2{r * 0.42f, -r * 0.36f}, 3.0f, Color{70, 175, 65, 255});
        break;
    case StageHazard::Sandstorm:
        // Swirling golden wind gusts
        glow(g, c, r * 1.35f, Color{245, 195, 100, 85});
        for (int k = -1; k <= 1; ++k) {
            float y = c.y + static_cast<float>(k) * r * 0.42f;
            float w = r * (k == 0 ? 0.9f : 0.68f);
            line({c.x - w, y}, {c.x + w * 0.7f, y - r * 0.12f}, 4.0f, Color{245, 200, 115, 255});
            disc(g, {c.x + w * 0.75f, y - r * 0.14f}, 3.5f, Color{255, 230, 160, 255});
        }
        break;
    case StageHazard::Permafrost:
        // Crisp 6-point snowflake
        glow(g, c, r * 1.4f, Color{120, 220, 255, 100});
        for (int i = 0; i < 3; ++i) {
            float a = static_cast<float>(i) * PI / 3.0f + time * 0.15f;
            Vec2 d{std::cos(a) * r * 0.85f, std::sin(a) * r * 0.85f};
            line(c - d, c + d, 3.2f, Color{150, 235, 255, 255});
            Vec2 n{-std::sin(a) * r * 0.22f, std::cos(a) * r * 0.22f};
            for (float s : {-0.55f, 0.55f}) {
                Vec2 b = c + d * s;
                line(b, b + d * (s > 0 ? 0.25f : -0.25f) + n, 2.2f, Color{215, 248, 255, 255});
                line(b, b + d * (s > 0 ? 0.25f : -0.25f) - n, 2.2f, Color{215, 248, 255, 255});
            }
        }
        disc(g, c, r * 0.22f, WHITE);
        break;
    case StageHazard::Magma:
        // Layered flickering flame
        glow(g, c, r * 1.45f, Color{255, 105, 42, static_cast<unsigned char>(105 + 40 * pulse)});
        DrawTriangle({c.x, c.y - r * 0.92f}, {c.x - r * 0.74f, c.y + r * 0.35f}, {c.x + r * 0.74f, c.y + r * 0.35f},
                     Color{255, 92, 36, 255});
        disc(g, c + Vec2{0, r * 0.25f}, r * 0.62f, Color{255, 105, 40, 255});
        DrawTriangle({c.x, c.y - r * 0.45f}, {c.x - r * 0.42f, c.y + r * 0.42f}, {c.x + r * 0.42f, c.y + r * 0.42f},
                     Color{255, 210, 70, 255});
        disc(g, c + Vec2{0, r * 0.36f}, r * 0.34f, Color{255, 242, 165, 255});
        break;
    case StageHazard::ToxicMire:
        // Glowing bio-droplet + bubbles
        glow(g, c, r * 1.4f, Color{155, 240, 85, 95});
        DrawTriangle({c.x, c.y - r * 0.82f}, {c.x - r * 0.58f, c.y + r * 0.22f}, {c.x + r * 0.58f, c.y + r * 0.22f},
                     Color{140, 230, 75, 255});
        disc(g, c + Vec2{0, r * 0.24f}, r * 0.56f, Color{155, 240, 85, 255});
        disc(g, c + Vec2{-r * 0.16f, r * 0.14f}, r * 0.18f, Color{225, 255, 180, 255});
        disc(g, c + Vec2{r * 0.58f, -r * 0.48f}, r * 0.16f, Color{175, 250, 110, 220});
        break;
    case StageHazard::Leylines:
        // Arcane 4-point star rune inside circle
        glow(g, c, r * 1.45f, Color{180, 120, 255, static_cast<unsigned char>(100 + 40 * pulse)});
        ring(g, c, r * 0.86f, Color{195, 140, 255, 235});
        DrawTriangle({c.x, c.y - r * 0.82f}, {c.x - r * 0.24f, c.y}, {c.x + r * 0.24f, c.y}, Color{225, 185, 255, 255});
        DrawTriangle({c.x, c.y + r * 0.82f}, {c.x + r * 0.24f, c.y}, {c.x - r * 0.24f, c.y}, Color{225, 185, 255, 255});
        DrawTriangle({c.x - r * 0.82f, c.y}, {c.x, c.y + r * 0.24f}, {c.x, c.y - r * 0.24f}, Color{190, 130, 255, 255});
        DrawTriangle({c.x + r * 0.82f, c.y}, {c.x, c.y - r * 0.24f}, {c.x, c.y + r * 0.24f}, Color{190, 130, 255, 255});
        disc(g, c, r * 0.2f, WHITE);
        break;
    }
}

std::string boss_short_name(const std::string& full) {
    size_t cut = full.find_first_of(" ,");
    return cut == std::string::npos ? full : full.substr(0, cut);
}

std::string boss_powers_summary(const BossGenome& b) {
    std::string out;
    for (int i = 0; i < b.power_count; ++i) {
        if (i > 0) out += "  +  ";
        out += boss_power_name(b.powers[static_cast<size_t>(i)]);
    }
    return out;
}

void draw_synergy_icon(const Gfx& g, int kind, Vec2 c, float r, Color col) {
    if (kind == 0) {
        // Crosshair (Combined Arms DMG)
        ring(g, c, r, col);
        line({c.x - r * 1.15f, c.y}, {c.x + r * 1.15f, c.y}, 2.0f, col);
        line({c.x, c.y - r * 1.15f}, {c.x, c.y + r * 1.15f}, 2.0f, col);
    } else if (kind == 1) {
        // Lightning bolt (Bullet Storm ROF)
        DrawTriangle({c.x + r * 0.25f, c.y - r}, {c.x - r * 0.55f, c.y + r * 0.1f}, {c.x + r * 0.15f, c.y + r * 0.1f},
                     col);
        DrawTriangle({c.x - r * 0.25f, c.y + r}, {c.x + r * 0.55f, c.y - r * 0.1f}, {c.x - r * 0.15f, c.y - r * 0.1f},
                     col);
    } else if (kind == 2) {
        // Starburst (Heavy Ordnance Knockback)
        star(c, r * 1.05f, col);
    } else {
        // Elemental diamond flame
        DrawPoly(c.to_raylib(), 4, r * 1.05f, 0, col);
        disc(g, c, r * 0.35f, WHITE);
    }
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

    // Clean Stage Selection Panel: single outer frame, 3 visual symbol medallions + hover/tap tooltips!
    Rectangle sp{58, 418, 604, 256};
    panel(sp, PANEL_FILL, theme.accent, 0.14f, 3.5f);
    bool locked = selected_stage_ > meta.unlocked_stage;
    text_fit(g, std::format("STAGE {}  -  {}", selected_stage_, theme.name), {VIRTUAL_W * 0.5f, 452}, 32, sp.width - 40,
             locked ? Color{150, 150, 160, 255} : WHITE);

    static int pinned_stage_tip = -1;
    static int pinned_stage_id = -1;
    if (pinned_stage_id != selected_stage_) {
        pinned_stage_id = selected_stage_;
        pinned_stage_tip = -1;
    }

    StageReward rw = stage_clear_reward(selected_stage_);
    std::string rw_preview = stage_reward_preview(selected_stage_);
    bool first_clear_pending = meta.stars[static_cast<size_t>(selected_stage_)] == 0;

    // 3 borderless visual medallions strictly between the '<' (x<=150) and '>' (x>=570) buttons.
    const float tile_w = 122.0f;
    const float tile_h = 110.0f;
    const float tile_y = 480.0f;
    const float tile_cx[3] = {226.0f, 360.0f, 494.0f};
    bool any_tile_hit = false;

    for (int m = 0; m < 3; ++m) {
        Rectangle tr{tile_cx[m] - tile_w * 0.5f, tile_y, tile_w, tile_h};
        bool hovered = !daily_open_ && pointer_in(game, tr);
        if (!daily_open_ && hit(game, tr)) {
            pinned_stage_tip = (pinned_stage_tip == m) ? -1 : m;
            any_tile_hit = true;
            game.sound.play(Sfx::Click, 0.5f);
        }
        bool show_tip = !daily_open_ && (hovered || pinned_stage_tip == m);
        Color tile_bg = show_tip ? Color{36, 42, 66, 230} : Color{18, 21, 34, 185};
        DrawRectangleRounded(tr, 0.24f, 8, tile_bg);

        Vec2 icon_c{tile_cx[m], tile_y + 42.0f};
        if (m == 0) {
            draw_hazard_icon(g, theme.hazard, theme.stage_twist, icon_c, 22.0f, game.time);
            text_fit(g, hazard_short_label(theme.hazard, theme.stage_twist), {tile_cx[m], tile_y + 88.0f}, 14,
                     tile_w - 12, hz.accent, false);
            if (show_tip) {
                std::string body = theme.tagline.empty() ? std::string(hz.short_desc)
                                                         : std::format("{}\n{}", theme.tagline, hz.short_desc);
                queue_tooltip({tile_cx[m], tile_y}, hz.name, body, hz.accent);
            }
        } else if (m == 1) {
            begin_scissor(tr.x + 4, tr.y + 4, tr.width - 8, 72);
            draw_boss(g, boss, theme.biome, icon_c + Vec2{0, 2}, 21.0f, game.time * 2.0f, 0.0f, false, game.time,
                      boss.has(BossPower::Shielded) ? 1.0f : 0.0f);
            EndScissorMode();
            // Subtle power indicator dots beneath the boss portrait
            float dot_span = static_cast<float>(boss.power_count - 1) * 12.0f;
            for (int p = 0; p < boss.power_count; ++p) {
                Vec2 dp{tile_cx[m] - dot_span * 0.5f + static_cast<float>(p) * 12.0f, tile_y + 72.0f};
                disc(g, dp, 3.6f, boss_power_color(boss.powers[static_cast<size_t>(p)]));
            }
            text_fit(g, boss_short_name(boss.name), {tile_cx[m], tile_y + 92.0f}, 14, tile_w - 12,
                     Color{255, 155, 130, 255}, false);
            if (show_tip) {
                queue_tooltip({tile_cx[m], tile_y}, std::format("BOSS: {}", boss.name),
                              std::format("POWERS: {}", boss_powers_summary(boss)), Color{255, 135, 110, 255});
            }
        } else {
            if (!rw_preview.empty() && first_clear_pending) {
                bool has_turret = rw.turret != TurretType::None;
                bool has_unit = rw.unit != UnitType::Count;
                begin_scissor(tr.x + 4, tr.y + 4, tr.width - 8, 74);
                if (has_turret && has_unit) {
                    draw_turret_mini(g, rw.turret, icon_c + Vec2{-23.0f, 3.0f}, 0.64f, game.time);
                    draw_soldier(g, icon_c + Vec2{23.0f, 12.0f}, 0.0f, rw.unit, 1.05f, game.time);
                } else if (has_turret) {
                    draw_turret_mini(g, rw.turret, icon_c + Vec2{0.0f, 3.0f}, 0.78f, game.time);
                } else if (has_unit) {
                    draw_soldier(g, icon_c + Vec2{0.0f, 13.0f}, 0.0f, rw.unit, 1.25f, game.time);
                } else {
                    gem_icon(icon_c + Vec2{0, 2}, 18.0f);
                    if (rw.free_upgrade) star(icon_c + Vec2{18, -14}, 9.0f, GOLD_TINT);
                }
                EndScissorMode();
                text_fit(g, "REWARDS", {tile_cx[m], tile_y + 88.0f}, 14, tile_w - 12, GOLD_TINT, false);
                if (show_tip) {
                    queue_tooltip({tile_cx[m], tile_y}, "FIRST CLEAR UNLOCKS", rw_preview, GOLD_TINT);
                }
            } else {
                star(icon_c + Vec2{0, 2}, 21.0f, Color{125, 240, 155, 255});
                text_fit(g, "CLEARED", {tile_cx[m], tile_y + 88.0f}, 14, tile_w - 12, Color{125, 240, 155, 255}, false);
                if (show_tip) {
                    queue_tooltip({tile_cx[m], tile_y}, "STAGE CLEARED",
                                  "All first-clear unlocks claimed!\nAim for 3 stars with >=75% wall HP.",
                                  Color{125, 240, 155, 255});
                }
            }
        }
    }
    if (!any_tile_hit && game.input.pressed && !game.input.consumed) pinned_stage_tip = -1;

    if (locked) {
        text(g, "LOCKED", {VIRTUAL_W * 0.5f, 628}, 28, Color{240, 90, 80, 255});
    } else {
        stars_row({VIRTUAL_W * 0.5f, 628}, meta.stars[static_cast<size_t>(selected_stage_)], 19, 52);
    }
    int k = daily_open_ ? 0 : 1;
    if (selected_stage_ > 1 && button(game, {74, 490, 72, 102}, "<", Color{70, 80, 120, 255}, true, 44, k * KEY_LEFT)) {
        selected_stage_--;
    }
    if (selected_stage_ < std::min(MAX_STAGE, meta.unlocked_stage + 1) &&
        button(game, {574, 490, 72, 102}, ">", Color{70, 80, 120, 255}, true, 44, k * KEY_RIGHT)) {
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
    badge(game, {62, 986}, affordable_upgrades(meta));
    if (button(game, {370, 980, 300, 96}, "MISSIONS", Color{50, 130, 200, 255}, true, 36, k * KEY_M)) {
        game.states.request(GameStateId::Missions);
    }
    badge(game, {382, 986}, meta.claimable_missions());

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
        flush_key_badges(g);
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
    badge(game, {122, y + 136}, affordable_upgrades(game.meta));
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

    if (!game.touch_ui) queue_key_badge({360, 146}, "TAB");
    const char* tabs[2] = {"UPGRADES", "SQUAD & UNITS"};
    for (int i = 0; i < 2; ++i) {
        Rectangle r{60.0f + static_cast<float>(i) * 310.0f, 150, 290, 56};
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
            panel(r, PANEL_FILL, focus_ == i ? GOLD_TINT : PANEL_EDGE, 0.14f, focus_ == i ? 3.5f : 2.5f);
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
                bool pressed = afford && game.input.down && pointer_in(game, br);
                if (button(game, br, "", afford ? Color{60, 170, 80, 255} : Color{80, 80, 90, 255}, afford, 26,
                           focus_ == i ? KEY_ENTER : 0, false)) {
                    meta.buy(d.id);
                    game.sound.play(Sfx::Build, 0.9f);
                    game.save();
                }
                draw_gem_button_label(g, br, pressed, std::format("{}", meta.cost(d.id)), 26.0f, 11.0f, afford);
            }
        }
    } else {
        // Clean Active Squad header with hover/tap (?) tooltip — no wall of text, no nested borders!
        int slots = meta.loadout_slots();
        int fielded_count = static_cast<int>(meta.squad().size());
        std::string squad_hdr = std::format("ACTIVE SQUAD  {}/{}", fielded_count, slots);
        text(g, squad_hdr, {VIRTUAL_W * 0.5f - 14.0f, 229}, 17, Color{215, 228, 250, 255}, Align::Center, false);
        Vec2 sq_info{VIRTUAL_W * 0.5f + text_width(g, squad_hdr, 17) * 0.5f + 6.0f, 229};
        Rectangle sq_info_r{sq_info.x - 14, sq_info.y - 14, 28, 28};
        bool sq_info_hov = pointer_in(game, sq_info_r);
        draw_info_dot(g, sq_info, sq_info_hov);
        if (sq_info_hov) {
            queue_tooltip(sq_info, "ACTIVE SQUAD",
                          "Your army splits evenly across deployed units.\nTap any active slot below to bench a unit.",
                          Color{130, 220, 255, 255});
        }

        // Borderless soft-tinted Active Squad slots
        for (int sl = 0; sl < LOADOUT_SLOTS; ++sl) {
            Rectangle r{30.0f + static_cast<float>(sl) * 168.0f, 245, 156, 54};
            bool open = sl < slots;
            int id = meta.loadout[static_cast<size_t>(sl)];
            bool filled = open && id >= 0 && id < UNIT_COUNT;
            Color fill = !open ? Color{22, 24, 34, 200} : filled ? Color{28, 54, 42, 235} : Color{30, 36, 54, 220};
            DrawRectangleRounded(r, 0.28f, 6, fill);
            if (!open) {
                const char* req = sl == 1 ? "STG 2" : sl == 2 ? "STG 4" : "STG 6";
                text(g, req, {r.x + 78, r.y + 27}, 15, Color{135, 140, 160, 220}, Align::Center, false);
                if (pointer_in(game, r)) {
                    queue_tooltip({r.x + 78, r.y}, "LOCKED SQUAD SLOT",
                                  std::format("Reach {} to deploy another unit class.", req),
                                  Color{180, 190, 220, 255});
                }
            } else if (filled) {
                auto u = static_cast<UnitType>(id);
                begin_scissor(r.x + 2, r.y + 2, r.width - 4, r.height - 4);
                draw_soldier_preview(game, r.x + 26, r.y + 39, 0.90f, id);
                EndScissorMode();
                text_fit(g, unit_def(u).name, {r.x + 87, r.y + 19}, 13.5f, 80, WHITE, false);
                text(g, std::format("LV {}", meta.unit_level(u)), {r.x + 87, r.y + 38}, 13, GOLD_TINT, Align::Center,
                     false);
                disc(g, {r.x + r.width - 12, r.y + 12}, 8.5f, Color{150, 48, 52, 220});
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
                text(g, "+ OPEN", {r.x + 78, r.y + 27}, 15, Color{145, 185, 240, 240}, Align::Center, false);
            }
        }

        // 4 compact borderless Synergy icon chips with hover tooltips!
        auto syn = compute_synergies(meta.squad());
        struct SynChip {
            bool active;
            const char* short_tag;
            const char* title;
            const char* desc;
            Color col;
        };
        const SynChip chips[4] = {
            {syn.combined_arms,
             "+15% DMG",
             "COMBINED ARMS",
             "+15% squad damage\nDeploy 3+ different unit classes.",
             {130, 245, 160, 255}},
            {syn.bullet_storm,
             "+15% ROF",
             "BULLET HOSE",
             "+15% fire rate\nDeploy Gunner + Rifleman or Shotgunner.",
             {255, 215, 95, 255}},
            {syn.heavy_ordnance,
             "+25% KNOCK",
             "HEAVY ORDNANCE",
             "+25% knockback force\nDeploy 2+ heavy classes (Shotgun, Sniper, Grenadier).",
             {255, 155, 90, 255}},
            {syn.elemental_fury,
             "+22% ELEM",
             "ELEMENTAL FURY",
             "+22% elemental damage\nDeploy 2+ elemental classes (Flamer, Cryo, Volt).",
             {135, 225, 255, 255}},
        };
        for (int c = 0; c < 4; ++c) {
            Rectangle cr{30.0f + static_cast<float>(c) * 168.0f, 305, 156, 28};
            bool hov = pointer_in(game, cr);
            Color bg = chips[c].active ? with_alpha(chips[c].col, hov ? 0.28f : 0.20f)
                                       : (hov ? Color{36, 42, 62, 225} : Color{22, 26, 40, 200});
            DrawRectangleRounded(cr, 0.45f, 6, bg);
            Color ic = chips[c].active ? chips[c].col : Color{115, 125, 150, 200};
            draw_synergy_icon(g, c, {cr.x + 22.0f, cr.y + 14.0f}, 7.0f, ic);
            text_fit(g, chips[c].short_tag, {cr.x + 92.0f, cr.y + 14.0f}, 13, cr.width - 42.0f, ic, false);
            if (hov) {
                queue_tooltip({cr.x + cr.width * 0.5f, cr.y},
                              std::format("{}  ({})", chips[c].title, chips[c].active ? "ACTIVE" : "INACTIVE"),
                              chips[c].desc, chips[c].active ? chips[c].col : Color{185, 198, 225, 255});
            }
        }

        static const char* ROLE_TAG[UNIT_COUNT] = {"ALL-ROUND", "RAPID", "BREAKER", "ELITE",
                                                   "SPLASH",    "BURN",  "FREEZE",  "CHAIN"};
        static const Color ROLE_COL[UNIT_COUNT] = {{110, 200, 110, 255}, {110, 175, 245, 255}, {235, 155, 85, 255},
                                                   {245, 225, 110, 255}, {195, 210, 95, 255},  {255, 120, 65, 255},
                                                   {120, 230, 255, 255}, {195, 140, 255, 255}};

        // 2-column grid for all 8 Soldier Classes — crisp alignment, zero nested borders, tooltips for full specs!
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
            Color edge = focus_ == i ? GOLD_TINT : fielded ? Color{105, 225, 130, 255} : PANEL_EDGE;
            panel(r, fielded ? Color{28, 48, 38, 242} : PANEL_FILL, edge, 0.14f, focus_ == i ? 3.5f : 2.5f);

            begin_scissor(r.x + 6, r.y + 6, 82, 92);
            draw_soldier_preview(game, r.x + 46, r.y + 72, 1.48f, i);
            EndScissorMode();

            // Left-aligned Unit Name + Borderless tinted Role Pill on the right
            float tag_w = std::clamp(text_width(g, ROLE_TAG[i], 12) + 16.0f, 52.0f, 82.0f);
            Rectangle rpill{r.x + r.width - 14.0f - tag_w, r.y + 14.0f, tag_w, 22.0f};
            DrawRectangleRounded(rpill, 0.5f, 6, with_alpha(ROLE_COL[i], 0.22f));
            text_fit(g, ROLE_TAG[i], {rpill.x + rpill.width * 0.5f, rpill.y + 11.0f}, 12, tag_w - 8.0f, ROLE_COL[i],
                     false);

            float name_max_w = rpill.x - (r.x + 94.0f) - 6.0f;
            float name_sz = 22.0f;
            float nw = text_width(g, def.name, name_sz);
            if (nw > name_max_w && nw > 0.0f) name_sz *= name_max_w / nw;
            text(g, def.name, {r.x + 94.0f, r.y + 25.0f}, name_sz, owned ? WHITE : Color{175, 178, 194, 255},
                 Align::Left);

            // Clean 1-line blurb + info badge (hover card header or info dot for full passive & stats tooltip!)
            float blurb_max_w = r.width - 134.0f;
            float blurb_sz = 14.5f;
            float bw = text_width(g, def.blurb, blurb_sz);
            if (bw > blurb_max_w && bw > 0.0f) blurb_sz *= blurb_max_w / bw;
            text(g, def.blurb, {r.x + 94.0f, r.y + 58.0f}, blurb_sz, Color{198, 212, 234, 255}, Align::Left, false);

            Vec2 info_p{r.x + r.width - 24.0f, r.y + 58.0f};
            Rectangle info_zone{r.x + 8, r.y + 8, r.width - 16, 86};
            bool info_hov = pointer_in(game, info_zone);
            draw_info_dot(g, info_p, info_hov);

            int lvl = meta.unit_level(u);
            float spec = 1.0f + 0.35f * static_cast<float>(std::min(def.bonus_pierce, 4));
            if (def.splash > 0.0f) spec *= 2.8f;
            if (def.shot == Shot::Flame) spec = 2.4f;
            if (def.shot == Shot::Cryo) spec *= 1.55f;
            if (def.shot == Shot::Volt) spec *= 1.85f;
            if (u == UnitType::Sniper) spec *= 1.45f;
            float dmg = def.damage_mult * def.rate_mult * static_cast<float>(def.pellets) * spec *
                        unit_damage_mult(std::max(1, lvl)) * unit_rate_mult(std::max(1, lvl));

            if (info_hov) {
                int free_stg = unit_free_clear_stage(u);
                std::string tip_body = std::format("{}\nPOWER {:.0f}", def.passive, dmg * 100.0f);
                if (!owned && free_stg > 0) tip_body += std::format("  |  FREE AFTER STAGE {}", free_stg);
                queue_tooltip({r.x + r.width * 0.5f, r.y + 8.0f}, std::format("{}  ({})", def.name, ROLE_TAG[i]),
                              tip_body, ROLE_COL[i]);
            }

            // Razor-sharp unified Level + Pips + Power baseline (y = r.y + 108)
            if (owned) {
                text(g, std::format("LV {}", lvl), {r.x + 16.0f, r.y + 108.0f}, 15, GOLD_TINT, Align::Left, false);
                for (int p = 0; p < UNIT_MAX_LEVEL; ++p) {
                    DrawRectangleRounded({r.x + 70.0f + static_cast<float>(p) * 16.4f, r.y + 103.5f, 13.0f, 9.0f}, 0.4f,
                                         4, p < lvl ? GOLD_TINT : Color{0, 0, 0, 125});
                }
                text(g, std::format("PWR {:.0f}", dmg * 100.0f), {r.x + r.width - 16.0f, r.y + 108.0f}, 14,
                     Color{185, 220, 255, 255}, Align::Right, false);
            } else {
                int free_stg = unit_free_clear_stage(u);
                text(g, std::format("PWR {:.0f}", dmg * 100.0f), {r.x + 16.0f, r.y + 108.0f}, 14,
                     Color{255, 210, 110, 255}, Align::Left, false);
                text(g, free_stg > 0 ? std::format("FREE STG {}", free_stg) : "LOCKED",
                     {r.x + r.width - 16.0f, r.y + 108.0f}, 14, Color{140, 235, 165, 235}, Align::Right, false);
            }

            // Action Buttons: NO nested focus border (card already shows focus!), centered Gem+Price, top-Z keycaps!
            if (!owned) {
                Rectangle br{r.x + 20, r.y + 142, r.width - 40, 56};
                bool afford = meta.gems >= def.unlock_cost;
                bool pressed = afford && game.input.down && pointer_in(game, br);
                if (button(game, br, "", afford ? Color{210, 125, 40, 255} : Color{80, 80, 90, 255}, afford, 22,
                           focus_ == i ? KEY_ENTER : 0, false)) {
                    meta.buy_unit(u);
                    game.sound.play(Sfx::LevelUp, 0.9f);
                    game.toast(std::format("UNLOCKED {}!", def.name), GOLD_TINT);
                    game.save();
                }
                draw_gem_button_label(g, br, pressed, std::format("UNLOCK {}", def.unlock_cost), 21.0f, 9.5f, afford);
            } else {
                Rectangle br{r.x + 14, r.y + 142, 154, 56};
                if (lvl < UNIT_MAX_LEVEL) {
                    bool afford = meta.can_upgrade_unit(u);
                    bool pressed = afford && game.input.down && pointer_in(game, br);
                    if (button(game, br, "", afford ? Color{60, 175, 85, 255} : Color{80, 80, 90, 255}, afford, 19,
                               focus_ == i ? KEY_ENTER : 0, false)) {
                        meta.upgrade_unit(u);
                        game.sound.play(Sfx::Build, 0.9f);
                        game.toast(std::format("{} LV {}!", def.name, meta.unit_level(u)), Color{130, 250, 160, 255});
                        game.save();
                    }
                    draw_gem_button_label(g, br, pressed, std::format("LV UP {}", unit_upgrade_cost(lvl)), 18.0f, 8.5f,
                                          afford);
                } else {
                    text(g, "MAX LV 10", {br.x + br.width * 0.5f, br.y + 28}, 20, GOLD_TINT);
                }
                Rectangle fb{r.x + 178, r.y + 142, 132, 56};
                if (button(game, fb, fielded ? "IN SQUAD" : "DEPLOY",
                           fielded ? Color{70, 120, 88, 255} : Color{56, 122, 206, 255}, true, 18,
                           focus_ == i ? KEY_F : 0, false)) {
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
