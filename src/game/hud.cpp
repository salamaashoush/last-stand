#include "core/game.hpp"
#include "game/formation.hpp"
#include "game/render.hpp"
#include "game/sim.hpp"
#include "game/turrets.hpp"
#include <algorithm>
#include <cmath>
#include <format>

namespace ls {

namespace {

constexpr Color GOLD_TINT{255, 205, 60, 255};
constexpr Color PANEL_FILL{28, 30, 44, 235};
constexpr Color PANEL_EDGE{90, 96, 130, 255};

float ease_out_back(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

void coin_icon(const Gfx& g, Vec2 p, float r) {
    disc(g, p, r, Color{150, 100, 10, 255});
    disc(g, p, r * 0.82f, GOLD_TINT);
    disc(g, p + Vec2{-r * 0.25f, -r * 0.25f}, r * 0.3f, Color{255, 255, 220, 200});
}

void gem_icon(Vec2 p, float r) {
    DrawPoly(p.to_raylib(), 4, r + 2, 0, Color{20, 60, 90, 255});
    DrawPoly(p.to_raylib(), 4, r, 0, Color{110, 220, 255, 255});
    DrawPoly((p + Vec2{-r * 0.2f, -r * 0.2f}).to_raylib(), 4, r * 0.35f, 0, Color{230, 250, 255, 255});
}

Color rarity_color(Rarity r, float time) {
    switch (r) {
    case Rarity::Common:
        return {74, 98, 132, 255};
    case Rarity::Rare:
        return {56, 96, 190, 255};
    case Rarity::Epic:
        return {136, 62, 184, 255};
    case Rarity::Evolution: {
        float k = 0.5f + 0.5f * std::sin(time * 4.0f);
        return mix(Color{220, 140, 20, 255}, Color{255, 90, 60, 255}, k);
    }
    }
    return WHITE;
}

void draw_card_icon(Game& game, CardId id, Vec2 c, float s) {
    const auto& g = game.gfx;
    Color w{255, 245, 220, 255};
    auto bullet = [&](Vec2 p, float ang, float len) {
        Vec2 d{std::cos(ang), std::sin(ang)};
        line(p - d * len * 0.5f, p + d * len * 0.5f, s * 0.12f, Color{255, 210, 90, 255});
        disc(g, p + d * len * 0.5f, s * 0.08f, w);
    };
    switch (id) {
    case CardId::Multishot:
        for (int k = -1; k <= 1; ++k)
            bullet(c + Vec2{static_cast<float>(k) * s * 0.3f, 0}, -PI / 2 + static_cast<float>(k) * 0.35f, s * 0.8f);
        break;
    case CardId::RapidFire:
        for (int k = 0; k < 3; ++k) {
            float y = c.y + s * 0.3f - static_cast<float>(k) * s * 0.3f;
            line({c.x - s * 0.35f, y + s * 0.15f}, {c.x, y - s * 0.12f}, s * 0.12f, w);
            line({c.x + s * 0.35f, y + s * 0.15f}, {c.x, y - s * 0.12f}, s * 0.12f, w);
        }
        break;
    case CardId::HeavyRounds:
        DrawRectangleRounded({c.x - s * 0.32f, c.y - s * 0.25f, s * 0.64f, s * 0.85f}, 0.15f, 4,
                             Color{190, 140, 50, 255});
        DrawRectangleRounded({c.x - s * 0.32f, c.y + s * 0.42f, s * 0.64f, s * 0.18f}, 0.3f, 4,
                             Color{150, 100, 30, 255});
        disc(g, c + Vec2{0, -s * 0.25f}, s * 0.32f, Color{230, 190, 90, 255});
        DrawRectangleRounded({c.x - s * 0.2f, c.y - s * 0.4f, s * 0.12f, s * 0.7f}, 0.5f, 4, Color{255, 240, 200, 120});
        break;
    case CardId::Piercing:
        disc(g, c, s * 0.3f, Color{200, 80, 80, 255});
        line(c + Vec2{0, s * 0.6f}, c + Vec2{0, -s * 0.6f}, s * 0.1f, w);
        DrawTriangle({c.x, c.y - s * 0.75f}, {c.x - s * 0.15f, c.y - s * 0.5f}, {c.x + s * 0.15f, c.y - s * 0.5f}, w);
        break;
    case CardId::Ricochet:
        line(c + Vec2{-s * 0.5f, s * 0.5f}, c + Vec2{-s * 0.1f, -s * 0.4f}, s * 0.1f, w);
        line(c + Vec2{-s * 0.1f, -s * 0.4f}, c + Vec2{s * 0.2f, s * 0.2f}, s * 0.1f, w);
        line(c + Vec2{s * 0.2f, s * 0.2f}, c + Vec2{s * 0.5f, -s * 0.5f}, s * 0.1f, w);
        break;
    case CardId::Incendiary:
    case CardId::Inferno:
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, c + Vec2{0, s * 0.1f}, s * 0.9f, Color{255, 90, 20, 200});
        glow(g, c + Vec2{0, -s * 0.05f}, s * 0.6f, Color{255, 170, 50, 230});
        glow(g, c + Vec2{0, s * 0.1f}, s * 0.28f, Color{255, 250, 210, 255});
        EndBlendMode();
        if (id == CardId::Inferno) ring(g, c, s * 0.8f, Color{255, 120, 40, 255});
        break;
    case CardId::Cryo:
    case CardId::Shatter:
        for (int k = 0; k < 3; ++k) {
            float a = static_cast<float>(k) * PI / 3.0f;
            Vec2 d{std::cos(a) * s * 0.6f, std::sin(a) * s * 0.6f};
            line(c - d, c + d, s * 0.1f, Color{190, 235, 255, 255});
        }
        if (id == CardId::Shatter) {
            for (int k = 0; k < 6; ++k) {
                float a = static_cast<float>(k) * PI / 3.0f + 0.5f;
                disc(g, c + Vec2{std::cos(a), std::sin(a)} * s * 0.8f, s * 0.08f, w);
            }
        }
        break;
    case CardId::Explosive:
        DrawPoly(c.to_raylib(), 8, s * 0.6f, 22.5f, Color{255, 140, 40, 255});
        DrawPoly(c.to_raylib(), 8, s * 0.35f, 0, Color{255, 230, 120, 255});
        break;
    case CardId::Critical:
        ring(g, c, s * 0.55f, Color{255, 90, 70, 255});
        line(c + Vec2{-s * 0.7f, 0}, c + Vec2{s * 0.7f, 0}, s * 0.07f, Color{255, 90, 70, 255});
        line(c + Vec2{0, -s * 0.7f}, c + Vec2{0, s * 0.7f}, s * 0.07f, Color{255, 90, 70, 255});
        disc(g, c, s * 0.1f, w);
        break;
    case CardId::ChainLightning:
    case CardId::ThunderGod: {
        Color b{200, 190, 255, 255};
        Vec2 pts[] = {{c.x + s * 0.2f, c.y - s * 0.7f},
                      {c.x - s * 0.2f, c.y},
                      {c.x + s * 0.15f, c.y},
                      {c.x - s * 0.2f, c.y + s * 0.7f}};
        for (int k = 0; k < 3; ++k) line(pts[k], pts[k + 1], s * 0.14f, b);
        if (id == CardId::ThunderGod) glow(g, c, s * 1.2f, Color{200, 190, 255, 120});
        break;
    }
    case CardId::Airstrike:
        if (auto* t = game.assets.get_texture("plane")) {
            DrawTexturePro(*t, {0, 0, static_cast<float>(t->width), static_cast<float>(t->height)},
                           {c.x, c.y, s * 1.6f, s * 1.6f}, {s * 0.8f, s * 0.8f}, 0, WHITE);
        }
        break;
    case CardId::Reinforcements:
        for (int k = 0; k < 3; ++k)
            draw_soldier_preview(game, c.x + static_cast<float>(k - 1) * s * 0.55f,
                                 c.y + (k == 1 ? -s * 0.1f : s * 0.15f), s / 34.0f, 0);
        break;
    case CardId::Fortify:
        DrawRectangleRec({c.x - s * 0.6f, c.y - s * 0.2f, s * 1.2f, s * 0.6f}, Color{170, 160, 150, 255});
        for (int k = 0; k < 3; ++k)
            DrawRectangleRec({c.x - s * 0.6f + static_cast<float>(k) * s * 0.45f, c.y - s * 0.45f, s * 0.3f, s * 0.3f},
                             Color{190, 180, 170, 255});
        line(c + Vec2{-s * 0.6f, s * 0.1f}, c + Vec2{s * 0.6f, s * 0.1f}, 3, Color{90, 80, 70, 255});
        break;
    case CardId::Plunder:
        for (int k = 0; k < 3; ++k)
            coin_icon(g, c + Vec2{static_cast<float>(k - 1) * s * 0.35f, static_cast<float>(k % 2) * s * 0.2f},
                      s * 0.3f);
        break;
    case CardId::CallIn:
        for (int k = 0; k < 3; ++k) {
            draw_soldier_preview(game, c.x + static_cast<float>(k - 1) * s * 0.55f,
                                 c.y + (k == 1 ? -s * 0.1f : s * 0.15f), s / 34.0f, 1 + k * 2);
        }
        break;
    case CardId::GateMaster:
        DrawRectangleRounded({c.x - s * 0.6f, c.y - s * 0.3f, s * 1.2f, s * 0.6f}, 0.3f, 4, Color{60, 170, 255, 200});
        text(g, "+", c, s * 0.8f, w);
        break;
    case CardId::BulletStorm:
        for (int k = -2; k <= 2; ++k)
            bullet(c + Vec2{static_cast<float>(k) * s * 0.22f, 0}, -PI / 2 + static_cast<float>(k) * 0.25f, s * 0.9f);
        break;
    case CardId::Count:
        break;
    }
}

} // namespace

void draw_card_face(Game& game, CardId id, Rectangle rr, int lvl) {
    const auto& g = game.gfx;
    const auto& def = card_def(id);
    constexpr float cw = 212.0f;
    float s = rr.width / cw;
    Vec2 center{rr.x + rr.width * 0.5f, rr.y + rr.height * 0.5f};
    Color rc = rarity_color(def.rarity, game.time);
    if (def.rarity == Rarity::Evolution || def.rarity == Rarity::Epic) {
        BeginBlendMode(BLEND_ADDITIVE);
        glow(game.gfx, center, cw * 1.1f * s, with_alpha(rc, 0.5f));
        EndBlendMode();
    }
    panel(rr, darken(rc, 0.45f), brighten(rc, 0.3f), 0.12f, 5.0f);
    DrawRectangleRounded({rr.x + 10 * s, rr.y + 10 * s, rr.width - 20 * s, 150 * s}, 0.15f, 6, darken(rc, 0.2f));
    draw_card_icon(game, id, {center.x, rr.y + 85 * s}, 58 * s);
    text_fit(g, def.name, {center.x, rr.y + 196 * s}, 30.0f * s, rr.width - 20.0f * s, WHITE);
    std::string desc = def.desc;
    auto nl = desc.find('\n');
    std::string l1 = desc.substr(0, nl);
    std::string l2 = nl == std::string::npos ? "" : desc.substr(nl + 1);
    text_fit(g, l1, {center.x, rr.y + 246 * s}, 22 * s, rr.width - 18.0f * s, Color{230, 230, 240, 255}, false);
    text_fit(g, l2, {center.x, rr.y + 274 * s}, 22 * s, rr.width - 18.0f * s, Color{230, 230, 240, 255}, false);
    if (def.rarity == Rarity::Evolution) {
        text(g, "EVOLUTION", {center.x, rr.y + 330 * s}, 24 * s, GOLD_TINT);
    } else if (def.max_level > 0) {
        float pw = 22.0f * s;
        float px = center.x - static_cast<float>(def.max_level) * pw * 0.5f;
        for (int p = 0; p < def.max_level; ++p) {
            Rectangle pip{px + static_cast<float>(p) * pw + 3, rr.y + 322 * s, pw - 6, 14 * s};
            Color pc = p < lvl ? GOLD_TINT : p == lvl ? Color{120, 255, 150, 255} : Color{0, 0, 0, 120};
            if (p == lvl) pc = with_alpha(pc, 0.6f + 0.4f * std::sin(game.time * 8.0f));
            DrawRectangleRounded(pip, 0.4f, 4, pc);
        }
    } else {
        text(g, std::format("TAKEN x{}", lvl), {center.x, rr.y + 330 * s}, 18 * s, Color{200, 200, 220, 255});
    }
    if (lvl == 0 && def.rarity != Rarity::Evolution) {
        DrawRectangleRounded({rr.x + rr.width - 70 * s, rr.y - 12 * s, 76 * s, 30 * s}, 0.5f, 6,
                             Color{255, 80, 70, 255});
        text(g, "NEW", {rr.x + rr.width - 32 * s, rr.y + 3 * s}, 20 * s, WHITE);
    }
}

namespace {

void draw_cards(Game& game) {
    auto& run = *game.run;
    const auto& g = game.gfx;
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 6, 20, 200});
    float pulse = 1.0f + 0.05f * std::sin(game.time * 5.0f);
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, {VIRTUAL_W * 0.5f, 300}, 260, Color{255, 200, 60, 70});
    EndBlendMode();
    text(g, "LEVEL UP!", {VIRTUAL_W * 0.5f, 300}, 76 * pulse, GOLD_TINT);
    text(g, "CHOOSE AN UPGRADE", {VIRTUAL_W * 0.5f, 370}, 26, Color{220, 220, 240, 255});

    constexpr float cw = 212.0f;
    constexpr float ch = 380.0f;
    constexpr float gap = 16.0f;
    float x0 = (VIRTUAL_W - (cw * 3 + gap * 2)) * 0.5f;
    for (int i = 0; i < 3; ++i) {
        CardId id = run.offer[static_cast<size_t>(i)];
        float k = ease_out_back((run.pick_anim - static_cast<float>(i) * 0.08f) / 0.3f);
        if (k <= 0.01f) continue;
        Rectangle r{x0 + static_cast<float>(i) * (cw + gap), 430, cw, ch};
        Vec2 center{r.x + cw * 0.5f, r.y + ch * 0.5f};
        bool hover = pointer_in(game, r);
        float s = k * (hover ? 1.04f : 1.0f);
        bool focused = run.pick_focus == i;
        if (focused) s *= 1.05f;
        Rectangle rr{center.x - cw * 0.5f * s, center.y - ch * 0.5f * s, cw * s, ch * s};
        draw_card_face(game, id, rr, run.stats.level_of(id));
        if (focused && k > 0.9f) {
            float pulse = 0.5f + 0.5f * std::sin(game.time * 8.0f);
            DrawRectangleRoundedLinesEx({rr.x - 6, rr.y - 6, rr.width + 12, rr.height + 12}, 0.12f, 8, 5.0f,
                                        Color{255, 230, 120, static_cast<unsigned char>(160 + 95 * pulse)});
        }
        key_badge(g, {center.x, rr.y + rr.height + 22}, i == 0 ? "1" : i == 1 ? "2" : "3");
        if (k > 0.9f && (hit(game, r) || (focused && IsKeyPressed(KEY_ENTER)))) {
            pick_card(game, i);
            return;
        }
    }
    if (run.rerolls > 0) {
        if (button(game, {VIRTUAL_W * 0.5f - 130, 850, 260, 70}, std::format("REROLL ({})", run.rerolls),
                   Color{90, 110, 200, 255}, true, 28, KEY_R)) {
            reroll_cards(game);
        }
    }
    text(g, "ARROWS + ENTER, OR 1 / 2 / 3", {VIRTUAL_W * 0.5f, 960}, 18, Color{160, 160, 190, 255}, Align::Center,
         false);
}

void draw_build_menu(Game& game) {
    auto& run = *game.run;
    const auto& g = game.gfx;
    int slot = run.build_slot;
    auto& t = run.turrets[static_cast<size_t>(slot)];
    float cx = std::clamp(TURRET_SLOT_X[slot], 200.0f, VIRTUAL_W - 200.0f);
    bool empty = t.type == TurretType::None;
    float h = empty ? 4 * 88.0f + 70.0f : 290.0f;
    Rectangle r{cx - 190, TURRET_Y - 60 - h, 380, h};
    panel(r, PANEL_FILL, PANEL_EDGE, 0.08f, 4.0f);
    DrawTriangle({TURRET_SLOT_X[slot] - 16, r.y + r.height}, {TURRET_SLOT_X[slot], r.y + r.height + 18},
                 {TURRET_SLOT_X[slot] + 16, r.y + r.height}, PANEL_FILL);
    auto draw_icon = [&](TurretType tt, Vec2 p) {
        draw_turret_preview(game, static_cast<int>(tt), 0, p.x, p.y, -PI / 2);
    };
    int options = empty ? TURRET_KINDS : 1;
    if (IsKeyPressed(KEY_DOWN)) run.build_focus = (run.build_focus + 1) % options;
    if (IsKeyPressed(KEY_UP)) run.build_focus = (run.build_focus + options - 1) % options;
    run.build_focus = std::clamp(run.build_focus, 0, options - 1);
    if (empty) {
        text(g, "BUILD TURRET", {r.x + r.width / 2, r.y + 34}, 28, GOLD_TINT);
        for (int k = 0; k < TURRET_KINDS; ++k) {
            const auto& def = TURRET_DEFS[static_cast<size_t>(k)];
            float y = r.y + 70 + static_cast<float>(k) * 88.0f;
            draw_icon(def.type, {r.x + 48, y + 40});
            text(g, def.name, {r.x + 92, y + 24}, 24, WHITE, Align::Left);
            text(g, def.blurb, {r.x + 92, y + 54}, 17, Color{190, 190, 210, 255}, Align::Left, false);
            bool afford = run.coins >= def.base_cost;
            Rectangle br{r.x + r.width - 126, y + 12, 112, 56};
            bool focused = run.build_focus == k;
            if (button(game, br, std::format("   {}", def.base_cost),
                       afford ? Color{230, 160, 40, 255} : Color{90, 90, 100, 255}, afford, 26, focused ? KEY_ENTER : 0,
                       focused)) {
                build_or_upgrade(game, slot, def.type);
                run.build_slot = -1;
                return;
            }
            coin_icon(g, {br.x + 26, br.y + 28}, 11);
        }
    } else {
        const auto& def = turret_def(t.type);
        draw_icon(t.type, {r.x + 60, r.y + 70});
        text(g, std::format("{} LV {}", def.name, t.level), {r.x + 112, r.y + 50}, 28, WHITE, Align::Left);
        auto d = difficulty_for(run);
        float dmg = turret_damage(t.type, t.level, d.stage);
        std::string stat = t.type == TurretType::Flamer
                               ? std::format("{:.0f} DPS + BURN", dmg / turret_interval(t.type, t.level))
                               : std::format("{:.0f} DMG / {:.1f}S", dmg, turret_interval(t.type, t.level));
        text(g, stat, {r.x + 112, r.y + 88}, 20, Color{200, 220, 255, 255}, Align::Left, false);
        if (t.level < TURRET_MAX_LEVEL) {
            int cost = turret_upgrade_cost(t.type, t.level);
            bool afford = run.coins >= cost;
            text(g, std::format("NEXT: {:.0f} DMG", turret_damage(t.type, t.level + 1, d.stage)),
                 {r.x + r.width / 2, r.y + 140}, 20, Color{120, 255, 150, 255}, Align::Center, false);
            Rectangle br{r.x + 20, r.y + 170, r.width - 40, 64};
            if (button(game, br, std::format("UPGRADE  {}", cost),
                       afford ? Color{60, 170, 80, 255} : Color{90, 90, 100, 255}, afford, 28, KEY_ENTER, true)) {
                build_or_upgrade(game, slot, t.type);
                return;
            }
        } else {
            text(g, "MAX LEVEL", {r.x + r.width / 2, r.y + 200}, 30, GOLD_TINT);
        }
        if (button(game, {r.x + r.width / 2 - 80, r.y + 244, 160, 38},
                   std::format("SELL +{}", turret_refund(t.type, t.level)), Color{170, 60, 60, 255}, true, 18, KEY_S)) {
            sell_turret(game, slot);
            run.build_slot = -1;
            return;
        }
    }
    if (game.input.pressed && !game.input.consumed && !pointer_in(game, r)) {
        game.input.consumed = true;
        run.build_slot = -1;
    }
}

void draw_pause(Game& game) {
    const auto& g = game.gfx;
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 6, 20, 190});
    Rectangle r{110, 360, 500, 560};
    panel(r, PANEL_FILL, PANEL_EDGE, 0.08f);
    text(g, "PAUSED", {VIRTUAL_W * 0.5f, 430}, 60, WHITE);
    float y = 500;
    if (button(game, {160, y, 400, 76}, "RESUME", Color{60, 170, 80, 255}, true, 34, KEY_ENTER)) game.paused = false;
    if (button(game, {160, y + 96, 400, 76}, "RESTART", Color{60, 120, 200, 255}, true, 34, KEY_R)) {
        game.paused = false;
        game.states.request(GameStateId::Playing);
    }
    if (button(game, {160, y + 192, 400, 76}, "RETREAT", Color{170, 60, 60, 255}, true, 34, KEY_Q)) {
        game.paused = false;
        game.states.request(GameStateId::Menu);
    }
    auto toggle = [&](Rectangle br, const char* label, float& v, float on) {
        bool is_on = v > 0.0f;
        if (button(game, br, std::format("{} {}", label, is_on ? "ON" : "OFF"),
                   is_on ? Color{80, 90, 130, 255} : Color{60, 60, 70, 255}, true, 24)) {
            v = is_on ? 0.0f : on;
            game.apply_volumes();
            game.save();
        }
    };
    toggle({160, y + 300, 190, 64}, "MUSIC", game.meta.music_volume, 0.5f);
    toggle({370, y + 300, 190, 64}, "SFX", game.meta.sfx_volume, 0.8f);
}

} // namespace

void render_hud(Game& game) {
    auto& run = *game.run;
    const auto& g = game.gfx;

    if (run.soldiers > 0) {
        float pop = 1.0f + run.soldier_pop * 0.5f;
        int visible = visible_soldiers(run.soldiers);
        float half = formation_half_width(visible);
        std::string label = std::format("{}", run.soldiers);
        float w = text_width(g, label, 30 * pop) + 44;
        bool right = run.squad_x + half + w + 12 < VIRTUAL_W;
        float x = right ? run.squad_x + half + 8 : run.squad_x - half - 8 - w;
        float y = SQUAD_Y + 16;
        DrawRectangleRounded({x, y - 19 * pop, w, 38 * pop}, 0.5f, 6, Color{14, 22, 48, 230});
        DrawRectangleRoundedLinesEx({x, y - 19 * pop, w, 38 * pop}, 0.5f, 6, 2.0f, Color{100, 170, 255, 255});
        draw_soldier_preview(game, x + 18, y + 3, 0.75f, static_cast<int>(run.squad.front()));
        text(g, label, {x + 30 + (w - 30) * 0.5f, y}, 30 * pop, Color{150, 225, 255, 255});
    }

    DrawRectangleGradientV(0, 0, VIRTUAL_W, static_cast<int>(HUD_TOP), Color{10, 10, 20, 210}, Color{10, 10, 20, 120});
    coin_icon(g, {52, 40}, 20);
    text(g, std::format("{}", run.coins), {82, 42}, 36, GOLD_TINT, Align::Left);
    if (run.gems_found > 0) {
        gem_icon({52, 78}, 11);
        text(g, std::format("{}", run.gems_found), {72, 79}, 22, Color{140, 230, 255, 255}, Align::Left);
    }
    auto d = difficulty_for(run);
    std::string wave_label =
        run.endless ? std::format("WAVE {}", std::max(1, run.wave))
                    : std::format("STAGE {}  -  WAVE {}/{}", run.stage, std::max(1, run.wave), WAVES_PER_STAGE);
    text(g, wave_label, {VIRTUAL_W * 0.5f, 30}, 24, WHITE);
    float progress = 0.0f;
    if (run.phase == WavePhase::Fighting && !run.spawns.empty()) {
        int alive = 0;
        for (const auto& e : run.enemies) alive += e.dead ? 0 : 1;
        progress = std::clamp((static_cast<float>(run.spawn_cursor) - static_cast<float>(alive)) /
                                  static_cast<float>(run.spawns.size()),
                              0.0f, 1.0f);
    } else if (run.phase != WavePhase::Intro) {
        progress = 1.0f;
    }
    bool boss_wave = d.wave == WAVES_PER_STAGE || d.wave == MINI_BOSS_WAVE;
    if (run.mod != WaveMod::None) {
        text(g, wave_mod_def(run.mod).name, {VIRTUAL_W * 0.5f, 84}, 16, Color{255, 150, 60, 255}, Align::Center, false);
    }
    {
        int tier = run.threat_tier + 1;
        Color tc = tier >= 5   ? Color{255, 60, 60, 255}
                   : tier >= 3 ? Color{255, 150, 60, 255}
                               : Color{200, 200, 210, 255};
        text(g, "THREAT", {600, 86}, 13, tc, Align::Right, false);
        for (int k = 0; k < std::min(tier, 8); ++k) {
            disc(g, {610.0f + static_cast<float>(k % 4) * 13.0f, 82.0f + static_cast<float>(k / 4) * 0.0f}, 5, tc);
        }
    }
    bar({220, 54, 280, 16}, progress, boss_wave ? Color{240, 70, 60, 255} : Color{120, 220, 120, 255},
        Color{0, 0, 0, 150});

    if (!run.picking && button(game, {640, 14, 64, 64}, "II", Color{70, 80, 120, 255}, true, 30)) game.paused = true;

    if (run.combo >= 10) {
        float k = std::clamp(run.combo_timer / 2.2f, 0.0f, 1.0f);
        Color c = mix(Color{255, 160, 40, 255}, Color{255, 60, 60, 255},
                      std::min(1.0f, static_cast<float>(run.combo) / 300.0f));
        text(g, std::format("x{} COMBO", run.combo), {VIRTUAL_W * 0.5f, HUD_TOP + 34},
             30 + std::min(20.0f, static_cast<float>(run.combo) * 0.05f), with_alpha(c, 0.4f + 0.6f * k));
    }

    for (const auto& e : run.enemies) {
        if (e.dead || e.type != EnemyType::Boss) continue;
        float y = HUD_TOP + (run.combo >= 10 ? 74.0f : 20.0f);
        const BossGenome& genome = e.mini_boss ? run.mini_boss : run.boss;
        text_fit(g, genome.name, {VIRTUAL_W * 0.5f, y}, 26, 520,
                 e.enraged ? Color{255, 60, 40, 255} : Color{255, 120, 100, 255});
        bar({90, y + 20, 540, 22}, e.hp / e.max_hp, Color{220, 50, 50, 255}, Color{0, 0, 0, 170});
        break;
    }

    DrawRectangleGradientV(0, static_cast<int>(PANEL_TOP), VIRTUAL_W, static_cast<int>(VIRTUAL_H - PANEL_TOP),
                           Color{10, 10, 20, 150}, Color{10, 10, 20, 230});
    float xp_ratio = static_cast<float>(run.xp) / static_cast<float>(xp_for_level(run.level));
    bar({76, PANEL_TOP + 14, 504, 20}, xp_ratio, Color{90, 200, 255, 255}, Color{0, 0, 0, 160});
    disc(g, {46, PANEL_TOP + 24}, 26, Color{20, 60, 110, 255});
    disc(g, {46, PANEL_TOP + 24}, 22, Color{60, 150, 230, 255});
    text(g, std::format("{}", run.level), {46, PANEL_TOP + 25}, 24, WHITE);
    float wall_ratio = run.wall_hp / run.stats.wall_max;
    Color wc = wall_ratio > 0.5f    ? Color{120, 220, 120, 255}
               : wall_ratio > 0.25f ? Color{240, 200, 60, 255}
                                    : Color{240, 70, 60, 255};
    bar({76, PANEL_TOP + 50, 330, 32}, wall_ratio, wc, Color{0, 0, 0, 160});
    text(g, std::format("WALL  {:.0f} / {:.0f}", std::max(0.0f, run.wall_hp), run.stats.wall_max),
         {241, PANEL_TOP + 67}, 19, WHITE);
    {
        int cost = repair_cost(run);
        bool can = run.coins >= cost && wall_ratio < 1.0f && !run.picking && !game.paused;
        if (button(game, {418, PANEL_TOP + 44, 162, 46}, std::format("REPAIR {}", cost),
                   can ? Color{60, 160, 90, 255} : Color{80, 80, 90, 255}, can, 20, KEY_R)) {
            repair_wall(game);
        }
    }
    DrawRectangleRec({30, PANEL_TOP + 52, 32, 28}, Color{170, 160, 150, 255});
    for (int k = 0; k < 3; ++k)
        DrawRectangleRec({30 + static_cast<float>(k) * 12, PANEL_TOP + 44, 8, 10}, Color{170, 160, 150, 255});

    Vec2 ab{646, PANEL_TOP + 50};
    float cd = run.airstrike_cd / run.stats.airstrike_cooldown();
    bool ready = run.airstrike_cd <= 0.0f && run.plane_t < 0.0f;
    if (ready) {
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, ab, 70 + 6 * std::sin(game.time * 6.0f), Color{255, 120, 60, 150});
        EndBlendMode();
    }
    disc(g, ab, 46, Color{20, 20, 30, 255});
    disc(g, ab, 42, ready ? Color{220, 80, 50, 255} : Color{70, 70, 80, 255});
    if (!ready && run.plane_t < 0.0f) {
        DrawCircleSector(ab.to_raylib(), 42, -90, -90 + 360.0f * cd, 32, Color{0, 0, 0, 120});
    }
    if (auto* pl = game.assets.get_texture("plane")) {
        DrawTexturePro(*pl, {0, 0, static_cast<float>(pl->width), static_cast<float>(pl->height)}, {ab.x, ab.y, 60, 60},
                       {30, 30}, 0, WHITE);
    }
    key_badge(g, {ab.x, ab.y + 46}, "SPACE");
    {
        Vec2 fb{58, 1128};
        bool full = run.rage >= 1.0f && run.frenzy_t <= 0.0f;
        bool active = run.frenzy_t > 0.0f;
        if (full || active) {
            BeginBlendMode(BLEND_ADDITIVE);
            glow(g, fb, 64 + 6 * std::sin(game.time * 8.0f), Color{255, 180, 40, 160});
            EndBlendMode();
        }
        disc(g, fb, 34, Color{20, 20, 30, 230});
        float k = active ? run.frenzy_t / 6.0f : run.rage;
        DrawCircleSector(fb.to_raylib(), 32, -90, -90 + 360.0f * k, 40,
                         active ? Color{255, 120, 40, 255} : Color{255, 190, 50, 255});
        disc(g, fb, 25, full ? Color{230, 120, 30, 255} : Color{60, 50, 40, 255});
        Color hot = full || active ? Color{255, 220, 120, 255} : Color{140, 130, 120, 255};
        Color core = full || active ? Color{255, 250, 220, 255} : Color{170, 160, 150, 255};
        float flick = full || active ? 1.0f + 0.08f * std::sin(game.time * 20.0f) : 1.0f;
        DrawTriangle({fb.x, fb.y - 19 * flick}, {fb.x - 11, fb.y + 2}, {fb.x + 11, fb.y + 2}, hot);
        disc(g, fb + Vec2{0, 5}, 11, hot);
        DrawTriangle({fb.x, fb.y - 7 * flick}, {fb.x - 6, fb.y + 6}, {fb.x + 6, fb.y + 6}, core);
        disc(g, fb + Vec2{0, 8}, 6, core);
        key_badge(g, {fb.x + 26, fb.y - 26}, "E");
        if (full && !run.picking && !game.paused && hit(game, {fb.x - 34, fb.y - 34, 68, 68})) trigger_frenzy(game);
    }
    if (run.frenzy_t > 0.0f) {
        float a = 0.5f + 0.5f * std::sin(game.time * 10.0f);
        DrawRectangleLinesEx({0, 0, static_cast<float>(VIRTUAL_W), static_cast<float>(VIRTUAL_H)}, 10,
                             Color{255, 170, 40, static_cast<unsigned char>(90 + 80 * a)});
    }
    if (!run.picking && !game.paused && ready && hit(game, {ab.x - 46, ab.y - 46, 92, 92})) trigger_airstrike(game);

    if (!run.picking && !game.paused && run.collapse_t < 0.0f) {
        for (int s = 0; s < TURRET_SLOTS; ++s) {
            key_badge(g, {TURRET_SLOT_X[s] + 30, TURRET_Y - 30}, s == 0 ? "1" : s == 1 ? "2" : s == 2 ? "3" : "4");
            if (s == run.build_slot) continue;
            if (hit(game, {TURRET_SLOT_X[s] - 40, TURRET_Y - 40, 80, 80})) {
                run.build_slot = s;
                game.sound.play(Sfx::Click, 0.7f);
            }
        }
    }

    if (run.banner.timer > 0.0f) {
        float t = 1.0f - run.banner.timer / run.banner.duration;
        float in = ease_out_back(t / 0.18f);
        float out = std::clamp(run.banner.timer / 0.35f, 0.0f, 1.0f);
        float y = 520;
        DrawRectangle(0, static_cast<int>(y - 70 * in), VIRTUAL_W, static_cast<int>(140 * in),
                      Color{0, 0, 0, static_cast<unsigned char>(140 * out)});
        text(g, run.banner.title, {VIRTUAL_W * 0.5f, y - 18}, 58 * in, with_alpha(run.banner.color, out));
        text(g, run.banner.subtitle, {VIRTUAL_W * 0.5f, y + 38}, 26 * in, with_alpha(WHITE, out));
    }

    if (run.hint_timer > 0.0f && !run.picking) {
        float a = std::min(1.0f, run.hint_timer);
        Rectangle r{60, 700, 600, 110};
        DrawRectangleRounded(r, 0.25f, 8, Color{10, 10, 30, static_cast<unsigned char>(200 * a)});
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, 3, with_alpha(GOLD_TINT, a));
        auto nl = run.hint.find('\n');
        if (nl == std::string::npos) {
            text(g, run.hint, {360, 755}, 24, with_alpha(WHITE, a));
        } else {
            text(g, run.hint.substr(0, nl), {360, 738}, 24, with_alpha(WHITE, a));
            text(g, run.hint.substr(nl + 1), {360, 772}, 24, with_alpha(WHITE, a));
        }
        if (run.hint.starts_with("DRAG,")) {
            float hx = run.squad_x + std::sin(game.time * 3.0f) * 120.0f;
            disc(g, {hx, SQUAD_Y + 10}, 26, with_alpha(WHITE, a * 0.7f));
            ring(g, {hx, SQUAD_Y + 10}, 34, with_alpha(GOLD_TINT, a));
        }
    }

    if (run.build_slot >= 0 && !run.picking && !game.paused) draw_build_menu(game);

    if (run.picking) draw_cards(game);
    if (game.paused) draw_pause(game);

    if (!run.picking && !game.paused && run.build_slot < 0 && game.input.down && !game.input.consumed &&
        game.input.press_pos.y < PANEL_TOP) {
        run.squad_target_x += game.input.drag_dx * 1.25f;
    }
}

} // namespace ls
