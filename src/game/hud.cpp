#include "core/game.hpp"
#include "game/formation.hpp"
#include "game/kids.hpp"
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
    case CardId::OrbitalStrike:
        if (id == CardId::OrbitalStrike) {
            BeginBlendMode(BLEND_ADDITIVE);
            glow(g, c, s * 1.0f, Color{255, 90, 180, 170});
            EndBlendMode();
        }
        if (auto* t = game.assets.get_texture("plane")) {
            DrawTexturePro(*t, {0, 0, static_cast<float>(t->width), static_cast<float>(t->height)},
                           {c.x, c.y, s * 1.6f, s * 1.6f}, {s * 0.8f, s * 0.8f}, 0,
                           id == CardId::OrbitalStrike ? Color{255, 190, 230, 255} : WHITE);
        }
        break;
    case CardId::Reinforcements:
        for (int k = 0; k < 3; ++k)
            draw_soldier_preview(game, c.x + static_cast<float>(k - 1) * s * 0.55f,
                                 c.y + (k == 1 ? -s * 0.1f : s * 0.15f), s / 34.0f, 0);
        break;
    case CardId::Fortify:
    case CardId::FortressOverdrive:
        DrawRectangleRec({c.x - s * 0.6f, c.y - s * 0.2f, s * 1.2f, s * 0.6f}, Color{170, 160, 150, 255});
        for (int k = 0; k < 3; ++k)
            DrawRectangleRec({c.x - s * 0.6f + static_cast<float>(k) * s * 0.45f, c.y - s * 0.45f, s * 0.3f, s * 0.3f},
                             Color{190, 180, 170, 255});
        line(c + Vec2{-s * 0.6f, s * 0.1f}, c + Vec2{s * 0.6f, s * 0.1f}, 3, Color{90, 80, 70, 255});
        if (id == CardId::FortressOverdrive) ring(g, c, s * 0.82f, Color{120, 230, 255, 255});
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
    case CardId::Overclock:
        DrawPoly(c.to_raylib(), 8, s * 0.52f, 22.5f, Color{255, 195, 70, 255});
        disc(g, c, s * 0.26f, Color{40, 44, 64, 255});
        ring(g, c, s * 0.72f, Color{120, 220, 255, 220});
        break;
    case CardId::FieldRepair:
        DrawRectangleRounded({c.x - s * 0.22f, c.y - s * 0.58f, s * 0.44f, s * 1.16f}, 0.3f, 4,
                             Color{110, 245, 145, 255});
        DrawRectangleRounded({c.x - s * 0.58f, c.y - s * 0.22f, s * 1.16f, s * 0.44f}, 0.3f, 4,
                             Color{110, 245, 145, 255});
        break;
    case CardId::Executioner:
        line(c + Vec2{-s * 0.5f, -s * 0.5f}, c + Vec2{s * 0.5f, s * 0.5f}, s * 0.16f, Color{255, 85, 75, 255});
        line(c + Vec2{s * 0.5f, -s * 0.5f}, c + Vec2{-s * 0.5f, s * 0.5f}, s * 0.16f, Color{255, 85, 75, 255});
        ring(g, c, s * 0.62f, GOLD_TINT);
        break;
    case CardId::ShockwaveRounds:
        ring(g, c, s * 0.75f, Color{130, 225, 255, 240});
        ring(g, c, s * 0.48f, Color{190, 240, 255, 220});
        disc(g, c, s * 0.2f, w);
        break;
    case CardId::Adrenaline:
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, c, s * 0.85f, Color{255, 165, 50, 200});
        EndBlendMode();
        DrawTriangle({c.x, c.y - s * 0.65f}, {c.x - s * 0.45f, c.y + s * 0.1f}, {c.x + s * 0.1f, c.y + s * 0.1f},
                     GOLD_TINT);
        DrawTriangle({c.x, c.y + s * 0.65f}, {c.x + s * 0.45f, c.y - s * 0.1f}, {c.x - s * 0.1f, c.y - s * 0.1f}, w);
        break;
    case CardId::PhantomBarrage:
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, c, s * 0.9f, Color{185, 120, 255, 180});
        EndBlendMode();
        for (int k = -1; k <= 1; k += 2) {
            line(c + Vec2{0, s * 0.5f}, c + Vec2{static_cast<float>(k) * s * 0.5f, -s * 0.5f}, s * 0.13f,
                 Color{210, 165, 255, 255});
        }
        disc(g, c + Vec2{0, s * 0.5f}, s * 0.16f, w);
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
    DrawRectangleRounded({rr.x + 10 * s, rr.y + 10 * s, rr.width - 20 * s, 146 * s}, 0.15f, 6, darken(rc, 0.2f));
    draw_card_icon(game, id, {center.x, rr.y + 82 * s}, 56 * s);
    text_fit(g, def.name, {center.x, rr.y + 188 * s}, 29.0f * s, rr.width - 18.0f * s, WHITE);
    std::string desc = def.desc;
    auto nl = desc.find('\n');
    std::string l1 = desc.substr(0, nl);
    std::string l2 = nl == std::string::npos ? "" : desc.substr(nl + 1);
    text_fit(g, l1, {center.x, rr.y + 236 * s}, 21 * s, rr.width - 18.0f * s, Color{230, 230, 240, 255}, false);
    text_fit(g, l2, {center.x, rr.y + 262 * s}, 21 * s, rr.width - 18.0f * s, Color{230, 230, 240, 255}, false);

    // Evolution recipe hint badge if this card pairs into an Evolution!
    CardId partner = evolution_partner(id);
    if (partner != CardId::Count && def.rarity != Rarity::Evolution) {
        bool has_partner = game.run && game.run->stats.level_of(partner) > 0;
        Color tag_col = has_partner ? Color{255, 215, 90, 255} : Color{170, 185, 220, 210};
        text_fit(g, std::format("EVO + {}", card_def(partner).name), {center.x, rr.y + 296 * s}, 13 * s,
                 rr.width - 16.0f * s, tag_col, false);
    }

    if (def.rarity == Rarity::Evolution) {
        text(g, "EVOLUTION", {center.x, rr.y + 334 * s}, 24 * s, GOLD_TINT);
    } else if (def.max_level > 0) {
        float pw = 22.0f * s;
        float px = center.x - static_cast<float>(def.max_level) * pw * 0.5f;
        for (int p = 0; p < def.max_level; ++p) {
            Rectangle pip{px + static_cast<float>(p) * pw + 3, rr.y + 328 * s, pw - 6, 14 * s};
            Color pc = p < lvl ? GOLD_TINT : p == lvl ? Color{120, 255, 150, 255} : Color{0, 0, 0, 120};
            if (p == lvl) pc = with_alpha(pc, 0.6f + 0.4f * std::sin(game.time * 8.0f));
            DrawRectangleRounded(pip, 0.4f, 4, pc);
        }
    } else {
        text(g, std::format("TAKEN x{}", lvl), {center.x, rr.y + 334 * s}, 18 * s, Color{200, 200, 220, 255});
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
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 6, 20, 205});
    float pulse = 1.0f + 0.05f * std::sin(game.time * 5.0f);
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, {VIRTUAL_W * 0.5f, 270}, 260, Color{255, 200, 60, 70});
    EndBlendMode();
    text(g, "LEVEL UP!", {VIRTUAL_W * 0.5f, 268}, 74 * pulse, GOLD_TINT);
    text(g, std::format("LEVEL {}  -  CHOOSE AN UPGRADE", run.level), {VIRTUAL_W * 0.5f, 336}, 24,
         Color{220, 220, 240, 255});

    constexpr float cw = 212.0f;
    constexpr float ch = 380.0f;
    constexpr float gap = 16.0f;
    float x0 = (VIRTUAL_W - (cw * 3 + gap * 2)) * 0.5f;
    for (int i = 0; i < 3; ++i) {
        CardId id = run.offer[static_cast<size_t>(i)];
        float k = ease_out_back((run.pick_anim - static_cast<float>(i) * 0.08f) / 0.3f);
        if (k <= 0.01f) continue;
        Rectangle r{x0 + static_cast<float>(i) * (cw + gap), 390, cw, ch};
        Vec2 center{r.x + cw * 0.5f, r.y + ch * 0.5f};
        bool hover = pointer_in(game, r);
        float s = k * (hover ? 1.04f : 1.0f);
        bool focused = run.pick_focus == i;
        if (focused) s *= 1.05f;
        Rectangle rr{center.x - cw * 0.5f * s, center.y - ch * 0.5f * s, cw * s, ch * s};
        draw_card_face(game, id, rr, run.stats.level_of(id));
        if (focused && k > 0.9f) {
            float fp = 0.5f + 0.5f * std::sin(game.time * 8.0f);
            DrawRectangleRoundedLinesEx({rr.x - 6, rr.y - 6, rr.width + 12, rr.height + 12}, 0.12f, 8, 5.0f,
                                        Color{255, 230, 120, static_cast<unsigned char>(160 + 95 * fp)});
        }
        if (!game.touch_ui) key_badge(g, {center.x, rr.y + rr.height + 22}, i == 0 ? "1" : i == 1 ? "2" : "3");
        if (k > 0.9f && (hit(game, r) || (focused && IsKeyPressed(KEY_ENTER)))) {
            pick_card(game, i);
            return;
        }
    }

    // Live build summary strip so players can see their build trajectory while choosing.
    Rectangle strip{42, 822, VIRTUAL_W - 84, 54};
    DrawRectangleRounded(strip, 0.3f, 6, Color{20, 22, 38, 220});
    DrawRectangleRoundedLinesEx(strip, 0.3f, 6, 2.0f, PANEL_EDGE);
    std::string summary =
        std::format("DPS {:.0f}   |   CRIT {:.0f}%   |   SQUAD {}   |   FORMATION {}", squad_power_estimate(run),
                    std::min(100.0f, (run.stats.crit_chance + formation_crit_bonus(run.formation)) * 100.0f),
                    run.soldiers, formation_name(run.formation));
    text_fit(g, summary, {VIRTUAL_W * 0.5f, strip.y + 27}, 19, strip.width - 24, Color{200, 220, 255, 255}, false);

    if (run.rerolls > 0) {
        if (button(game, {VIRTUAL_W * 0.5f - 130, 898, 260, 68}, std::format("REROLL ({})", run.rerolls),
                   Color{90, 110, 200, 255}, true, 28, KEY_R)) {
            reroll_cards(game);
        }
    }
    text(g, "ARROWS + ENTER, OR 1 / 2 / 3", {VIRTUAL_W * 0.5f, 996}, 18, Color{160, 160, 190, 255}, Align::Center,
         false);
}

void draw_build_menu(Game& game) {
    auto& run = *game.run;
    const auto& g = game.gfx;
    int slot = run.build_slot;
    auto& t = run.turrets[static_cast<size_t>(slot)];
    float cx = std::clamp(TURRET_SLOT_X[slot], 200.0f, VIRTUAL_W - 200.0f);
    bool empty = t.type == TurretType::None;
    float row_h = 66.0f;
    float h = empty ? static_cast<float>(TURRET_KINDS) * row_h + 62.0f : 318.0f;
    Rectangle r{cx - 194, TURRET_Y - 56 - h, 388, h};
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
        text(g, "BUILD TURRET", {r.x + r.width / 2, r.y + 28}, 25, GOLD_TINT);
        for (int k = 0; k < TURRET_KINDS; ++k) {
            const auto& def = turret_def(TURRET_DISPLAY_ORDER[static_cast<size_t>(k)]);
            bool unlocked = turret_unlocked(def.type, run.unlocked_stage);
            bool newly_unlocked = unlocked && def.unlock_stage > 1 && def.unlock_stage == run.unlocked_stage;
            float y = r.y + 56 + static_cast<float>(k) * row_h;
            int cost = turret_upgrade_cost(def.type, 0, run.turret_cost_mult);
            draw_icon(def.type, {r.x + 44, y + 32});
            if (!unlocked) disc(g, {r.x + 44, y + 32}, 28, Color{14, 16, 26, 150});
            text(g, def.name, {r.x + 86, y + 19}, 20, unlocked ? WHITE : Color{145, 148, 165, 255}, Align::Left);
            if (newly_unlocked) {
                float nw = text_width(g, def.name, 20);
                Rectangle np{r.x + 92 + nw, y + 9, 44, 19};
                DrawRectangleRounded(np, 0.5f, 4, Color{255, 90, 60, 255});
                text(g, "NEW!", {np.x + 22, np.y + 10}, 12, WHITE);
            }
            text(g, def.blurb, {r.x + 86, y + 44}, 14, unlocked ? Color{190, 196, 218, 255} : Color{125, 128, 145, 255},
                 Align::Left, false);
            Rectangle br{r.x + r.width - 122, y + 7, 108, 48};
            bool focused = run.build_focus == k;
            if (pointer_in(game, {r.x, y, r.width, row_h - 2})) run.build_focus = k;
            if (!unlocked) {
                DrawRectangleRounded(br, 0.28f, 6, Color{34, 36, 48, 235});
                DrawRectangleRoundedLinesEx(br, 0.28f, 6, 1.5f, Color{90, 95, 118, 255});
                text(g, std::format("STAGE {}", def.unlock_stage), {br.x + br.width * 0.5f, br.y + 17}, 14,
                     Color{255, 195, 95, 230}, Align::Center, false);
                text(g, "LOCKED", {br.x + br.width * 0.5f, br.y + 34}, 12, Color{155, 160, 180, 220}, Align::Center,
                     false);
            } else {
                bool afford = run.coins >= cost;
                if (button(game, br, std::format("   {}", cost),
                           afford ? Color{230, 160, 40, 255} : Color{90, 90, 100, 255}, afford, 22,
                           focused ? KEY_ENTER : 0, focused)) {
                    build_or_upgrade(game, slot, def.type);
                    run.build_slot = -1;
                    return;
                }
                coin_icon(g, {br.x + 24, br.y + 24}, 9);
            }
        }
    } else {
        const auto& def = turret_def(t.type);
        draw_icon(t.type, {r.x + 60, r.y + 70});
        text(g, std::format("{} LV {}", def.name, t.level), {r.x + 112, r.y + 50}, 28, WHITE, Align::Left);
        auto d = difficulty_for(run);
        float dmg = turret_damage(t.type, t.level, d.stage) * run.stats.turret_damage_mult;
        float iv = turret_interval(t.type, t.level) / run.stats.turret_rate_mult;
        std::string stat = t.type == TurretType::Flamer ? std::format("{:.0f} DPS + BURN", dmg / iv)
                                                        : std::format("{:.0f} DMG / {:.1f}S", dmg, iv);
        text(g, stat, {r.x + 112, r.y + 88}, 20, Color{200, 220, 255, 255}, Align::Left, false);
        const char* perk = t.level < 3 ? def.perk_lv3 : def.perk_lv5;
        Color perk_col = t.level >= 5   ? GOLD_TINT
                         : t.level >= 3 ? Color{140, 235, 255, 255}
                                        : Color{255, 210, 120, 230};
        text_fit(g, perk, {r.x + r.width * 0.5f, r.y + 124}, 16, r.width - 28, perk_col, false);
        if (t.level < TURRET_MAX_LEVEL) {
            int cost = turret_upgrade_cost(t.type, t.level, run.turret_cost_mult);
            bool afford = run.coins >= cost;
            text(g,
                 std::format("NEXT LV {}: {:.0f} DMG", t.level + 1,
                             turret_damage(t.type, t.level + 1, d.stage) * run.stats.turret_damage_mult),
                 {r.x + r.width / 2, r.y + 156}, 19, Color{120, 255, 150, 255}, Align::Center, false);
            Rectangle br{r.x + 20, r.y + 186, r.width - 40, 62};
            if (button(game, br, std::format("UPGRADE  {}", cost),
                       afford ? Color{60, 170, 80, 255} : Color{90, 90, 100, 255}, afford, 26, KEY_ENTER, true)) {
                build_or_upgrade(game, slot, t.type);
                return;
            }
        } else {
            text(g, "APEX LEVEL 5 MAXED", {r.x + r.width / 2, r.y + 214}, 26, GOLD_TINT);
        }
        if (button(game, {r.x + r.width / 2 - 80, r.y + 266, 160, 38},
                   std::format("SELL +{}", turret_refund(t.type, t.level, run.turret_cost_mult)),
                   Color{170, 60, 60, 255}, true, 18, KEY_S)) {
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

void toggle_mute_all(Game& game) {
    bool muted = game.meta.music_volume <= 0.0f && game.meta.sfx_volume <= 0.0f;
    if (muted) {
        game.meta.music_volume = game.saved_music_vol > 0.0f ? game.saved_music_vol : 0.5f;
        game.meta.sfx_volume = game.saved_sfx_vol > 0.0f ? game.saved_sfx_vol : 0.8f;
        game.toast("AUDIO UNMUTED", Color{140, 240, 175, 255});
    } else {
        if (game.meta.music_volume > 0.0f) game.saved_music_vol = game.meta.music_volume;
        if (game.meta.sfx_volume > 0.0f) game.saved_sfx_vol = game.meta.sfx_volume;
        game.meta.music_volume = 0.0f;
        game.meta.sfx_volume = 0.0f;
        game.toast("AUDIO MUTED", Color{255, 165, 110, 255});
    }
    game.apply_volumes();
    game.save();
}

void cycle_sim_speed(Game& game) {
    if (game.sim_speed < 1.25f)
        game.sim_speed = 1.5f;
    else if (game.sim_speed < 1.75f)
        game.sim_speed = 2.0f;
    else
        game.sim_speed = 1.0f;
    game.sound.play(Sfx::Click, 0.7f, 0.9f + game.sim_speed * 0.2f);
}

std::string speed_label(float s) {
    if (s >= 1.9f) return "2x";
    if (s >= 1.4f) return "1.5x";
    return "1x";
}

void draw_pause(Game& game) {
    const auto& g = game.gfx;
    const auto& run = *game.run;
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{8, 6, 20, 205});
    Rectangle r{56, 170, 608, 886};
    panel(r, PANEL_FILL, PANEL_EDGE, 0.06f, 4.0f);
    text(g, "PAUSED", {VIRTUAL_W * 0.5f, 218}, 54, GOLD_TINT);
    std::string sub = run.endless
                          ? std::format("ENDLESS  -  WAVE {}  (BEST {})", std::max(1, run.wave),
                                        std::max<long long>(run.wave, game.meta.stat(Lifetime::BestEndlessWave)))
                          : std::format("STAGE {}  -  {}  (WAVE {}/{})", run.stage, run.theme.name,
                                        std::max(1, run.wave), WAVES_PER_STAGE);
    text_fit(g, sub, {VIRTUAL_W * 0.5f, 258}, 18, 540, Color{200, 215, 240, 255}, false);

    // Live Battle Stats & Active Build Snapshot Card
    Rectangle sr{84, 282, 552, 168};
    DrawRectangleRounded(sr, 0.14f, 6, Color{18, 20, 34, 230});
    DrawRectangleRoundedLinesEx(sr, 0.14f, 6, 2.0f, PANEL_EDGE);
    float wall_pct = 100.0f * std::clamp(run.wall_hp / std::max(1.0f, run.stats.wall_max), 0.0f, 1.0f);
    float crit_pct = std::min(100.0f, (run.stats.crit_chance + formation_crit_bonus(run.formation)) * 100.0f);
    struct StatCell {
        const char* k;
        std::string v;
        Color col;
    };
    StatCell cells[6] = {
        {"SQUAD", std::format("{}", run.soldiers), Color{140, 225, 255, 255}},
        {"EST. DPS", std::format("{:.0f}", squad_power_estimate(run)), Color{255, 215, 95, 255}},
        {"WALL HP", std::format("{:.0f}%", wall_pct),
         wall_pct < 35.0f ? Color{255, 100, 90, 255} : Color{120, 245, 150, 255}},
        {"KILLS", std::format("{}", run.tally.kills), WHITE},
        {"CRIT", std::format("{:.0f}%", crit_pct), Color{255, 175, 85, 255}},
        {"BEST COMBO", std::format("x{}", run.tally.max_combo), Color{255, 145, 200, 255}},
    };
    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        float cx = sr.x + 92.0f + static_cast<float>(col) * 184.0f;
        float cy = sr.y + 30.0f + static_cast<float>(row) * 48.0f;
        text(g, cells[i].k, {cx, cy - 11.0f}, 12, Color{155, 168, 196, 230}, Align::Center, false);
        text(g, cells[i].v, {cx, cy + 10.0f}, 21, cells[i].col);
    }
    DrawLineEx({sr.x + 18, sr.y + 110}, {sr.x + sr.width - 18, sr.y + 110}, 1.5f, Color{70, 78, 106, 200});
    std::string evos;
    for (int id = 0; id < CARD_COUNT; ++id) {
        const auto& cd = CARD_DEFS[static_cast<size_t>(id)];
        if (cd.rarity == Rarity::Evolution && run.stats.level_of(cd.id) > 0) {
            if (!evos.empty()) evos += ", ";
            evos += cd.name;
        }
    }
    std::string build_line = !evos.empty()
                                 ? std::format("EVOLUTIONS: {}  |  CARDS: {}", evos, run.tally.cards_taken)
                                 : std::format("FORMATION: {}  |  LEVEL: {}  |  CARDS: {}",
                                               formation_name(run.formation), run.level, run.tally.cards_taken);
    text_fit(g, build_line, {VIRTUAL_W * 0.5f, sr.y + 139}, 15, sr.width - 28,
             !evos.empty() ? GOLD_TINT : Color{185, 210, 245, 255}, false);

    // Primary Actions
    if (button(game, {104, 470, 512, 74}, "RESUME", Color{60, 175, 85, 255}, true, 34, KEY_ENTER)) game.paused = false;
    if (button(game, {104, 560, 246, 66}, "RESTART", Color{58, 122, 204, 255}, true, 28, KEY_R)) {
        game.paused = false;
        game.states.request(GameStateId::Playing);
    }
    if (button(game, {370, 560, 246, 66}, "RETREAT", Color{178, 58, 58, 255}, true, 28, KEY_Q)) {
        game.paused = false;
        game.states.request(GameStateId::Menu);
    }

    // Utility Row: Speed Control + Master Mute!
    bool fast = game.sim_speed > 1.25f;
    if (button(game, {104, 646, 246, 64}, std::format("SPEED  {}", speed_label(game.sim_speed)),
               fast ? Color{215, 130, 42, 255} : Color{72, 84, 122, 255}, true, 24, KEY_S)) {
        cycle_sim_speed(game);
    }
    bool all_muted = game.meta.music_volume <= 0.0f && game.meta.sfx_volume <= 0.0f;
    if (button(game, {370, 646, 246, 64}, all_muted ? "UNMUTE ALL" : "MUTE ALL",
               all_muted ? Color{196, 72, 62, 255} : Color{62, 128, 138, 255}, true, 24, KEY_M)) {
        toggle_mute_all(game);
    }

    // Separate Music & SFX Toggles
    auto toggle = [&](Rectangle br, const char* label, float& v, float& saved, float fallback) {
        bool is_on = v > 0.0f;
        if (button(game, br, std::format("{}  {}", label, is_on ? "ON" : "OFF"),
                   is_on ? Color{80, 92, 132, 255} : Color{56, 56, 68, 255}, true, 22)) {
            if (is_on) {
                saved = v;
                v = 0.0f;
            } else {
                v = saved > 0.0f ? saved : fallback;
            }
            game.apply_volumes();
            game.save();
        }
    };
    toggle({104, 726, 246, 56}, "MUSIC", game.meta.music_volume, game.saved_music_vol, 0.5f);
    toggle({370, 726, 246, 56}, "SFX", game.meta.sfx_volume, game.saved_sfx_vol, 0.8f);

    // Quick Controls Reference Card
    Rectangle cr{84, 804, 552, 166};
    DrawRectangleRounded(cr, 0.14f, 6, Color{18, 20, 34, 220});
    DrawRectangleRoundedLinesEx(cr, 0.14f, 6, 1.8f, PANEL_EDGE);
    text(g, "CONTROLS & SHORTCUTS", {VIRTUAL_W * 0.5f, cr.y + 24}, 17, GOLD_TINT);
    text_fit(g, "MOVE: Drag or A / D   |   FORMATION: F   |   REPAIR WALL: R", {VIRTUAL_W * 0.5f, cr.y + 56}, 14,
             cr.width - 24, Color{205, 218, 242, 255}, false);
    text_fit(g, "HERO POWERS:  Q (Ali)   |   W (Salama)   |   E (Omar)", {VIRTUAL_W * 0.5f, cr.y + 84}, 14,
             cr.width - 24, Color{255, 220, 140, 255}, false);
    text_fit(g, "AIRSTRIKE: SPACE   |   BUILD / UPGRADE TURRETS: Tap or 1 - 4", {VIRTUAL_W * 0.5f, cr.y + 112}, 14,
             cr.width - 24, Color{205, 218, 242, 255}, false);
    text_fit(g, "PAUSE: ESC / P   |   MUTE: M   |   SPEED: S", {VIRTUAL_W * 0.5f, cr.y + 140}, 14, cr.width - 24,
             Color{155, 225, 195, 255}, false);

    text(g, "PRESS ESC OR ENTER TO RESUME", {VIRTUAL_W * 0.5f, 1018}, 16, Color{170, 178, 204, 225}, Align::Center,
         false);
}

} // namespace

void render_hud(Game& game) {
    auto& run = *game.run;
    const auto& g = game.gfx;

    if (run.soldiers > 0) {
        float pop = 1.0f + run.soldier_pop * 0.5f;
        int visible = visible_soldiers(run.soldiers);
        float half = formation_half_width(visible, run.formation);
        std::string label = std::format("{}", run.soldiers);
        float w = text_width(g, label, 30 * pop) + 44;
        bool right = omar_x(run.squad_x, visible, run.formation) < run.squad_x;
        float x = right ? run.squad_x + half + 8 : run.squad_x - half - 8 - w;
        float y = SQUAD_Y + 16;
        DrawRectangleRounded({x, y - 19 * pop, w, 38 * pop}, 0.5f, 6, Color{14, 22, 48, 230});
        DrawRectangleRoundedLinesEx({x, y - 19 * pop, w, 38 * pop}, 0.5f, 6, 2.0f, Color{100, 170, 255, 255});
        draw_soldier_preview(game, x + 18, y + 3, 0.75f, static_cast<int>(run.squad.front()));
        text(g, label, {x + 30 + (w - 30) * 0.5f, y}, 30 * pop, Color{150, 225, 255, 255});
    }

    DrawRectangleGradientV(0, 0, VIRTUAL_W, static_cast<int>(HUD_TOP), Color{10, 10, 22, 232}, Color{10, 10, 22, 148});
    coin_icon(g, {52, 40}, 20);
    text(g, std::format("{}", run.coins), {82, 42}, 36, GOLD_TINT, Align::Left);
    if (run.gems_found > 0) {
        gem_icon({52, 78}, 11);
        text(g, std::format("{}", run.gems_found), {72, 79}, 22, Color{140, 230, 255, 255}, Align::Left);
    }
    auto d = difficulty_for(run);
    long long best_end = game.meta.stat(Lifetime::BestEndlessWave);
    bool new_rec = run.endless && best_end > 0 && run.wave > best_end;
    std::string wave_label =
        run.endless ? (new_rec ? std::format("ENDLESS W{} - NEW BEST!", std::max(1, run.wave))
                               : (best_end > 0 ? std::format("ENDLESS W{} (BEST {})", std::max(1, run.wave), best_end)
                                               : std::format("ENDLESS  -  WAVE {}", std::max(1, run.wave))))
                    : std::format("STAGE {}  -  WAVE {}/{}", run.stage, std::max(1, run.wave), WAVES_PER_STAGE);
    text_fit(g, wave_label, {VIRTUAL_W * 0.5f, 28}, 23, 330, new_rec ? GOLD_TINT : WHITE);
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
    bar({220, 50, 280, 16}, progress, boss_wave ? Color{240, 70, 60, 255} : Color{120, 220, 120, 255},
        Color{0, 0, 0, 150});

    if (run.mod != WaveMod::None) {
        text(g, wave_mod_def(run.mod).name, {VIRTUAL_W * 0.5f, 81}, 15, Color{255, 165, 70, 255}, Align::Center, true);
    } else {
        const auto& hz = stage_hazard_def(run.theme.hazard);
        text(g, hz.name, {VIRTUAL_W * 0.5f, 81}, 15, brighten(hz.accent, 0.2f), Align::Center, true);
    }
    {
        int tier = run.threat_tier + 1;
        Color tc = tier >= 5   ? Color{255, 60, 60, 255}
                   : tier >= 3 ? Color{255, 150, 60, 255}
                               : Color{200, 200, 210, 255};
        text(g, "THREAT", {600, 84}, 13, tc, Align::Right, false);
        for (int k = 0; k < std::min(tier, 8); ++k) {
            disc(g, {610.0f + static_cast<float>(k % 4) * 13.0f, 80.0f}, 5, tc);
        }
    }

    if (!run.picking) {
        bool fast = game.sim_speed > 1.25f;
        if (button(game, {564, 14, 64, 50}, speed_label(game.sim_speed),
                   fast ? Color{215, 130, 42, 255} : Color{56, 64, 96, 255}, true, 21)) {
            cycle_sim_speed(game);
        }
        if (button(game, {640, 14, 64, 64}, "II", Color{70, 80, 120, 255}, true, 30)) game.paused = true;
    }

    if (run.combo >= 10) {
        float k = std::clamp(run.combo_timer / 2.4f, 0.0f, 1.0f);
        Color c = mix(Color{255, 160, 40, 255}, Color{255, 60, 60, 255},
                      std::min(1.0f, static_cast<float>(run.combo) / 300.0f));
        text(g, std::format("x{} COMBO", run.combo), {VIRTUAL_W * 0.5f, HUD_TOP + 30},
             30 + std::min(20.0f, static_cast<float>(run.combo) * 0.05f), with_alpha(c, 0.4f + 0.6f * k));
    }

    // Enhanced Boss HUD Bar: Name + Phase 2 badge + HP bar with 50% phase notch + Shield bar + Power pills!
    for (const auto& e : run.enemies) {
        if (e.dead || e.type != EnemyType::Boss) continue;
        float y = HUD_TOP + (run.combo >= 10 ? 68.0f : 22.0f);
        const BossGenome& genome = e.mini_boss ? run.mini_boss : run.boss;
        std::string bname = e.phase2 ? std::format("{}  [PHASE 2]", genome.name) : genome.name;
        text_fit(g, bname, {VIRTUAL_W * 0.5f, y}, 25, 560,
                 e.enraged  ? Color{255, 60, 40, 255}
                 : e.phase2 ? genome.accent
                            : Color{255, 130, 105, 255});
        Rectangle hb{86, y + 18, 548, 22};
        bar(hb, e.hp / e.max_hp, e.phase2 ? Color{235, 70, 50, 255} : Color{220, 50, 50, 255}, Color{0, 0, 0, 180});
        // 50% phase threshold notch on full bosses.
        if (!e.mini_boss) {
            line({hb.x + hb.width * 0.5f, hb.y - 2}, {hb.x + hb.width * 0.5f, hb.y + hb.height + 2}, 3.0f,
                 Color{255, 235, 150, 210});
        }
        if (e.shield_hp > 0.0f && e.shield_max > 0.0f) {
            bar({hb.x, hb.y + hb.height + 3, hb.width, 7}, e.shield_hp / e.shield_max, Color{120, 215, 255, 255},
                Color{0, 0, 0, 130});
        }
        // Boss power pills!
        float py = hb.y + hb.height + (e.shield_hp > 0.0f ? 22.0f : 16.0f);
        float pill_w = 132.0f;
        float total_w =
            static_cast<float>(genome.power_count) * pill_w + static_cast<float>(genome.power_count - 1) * 10.0f;
        float px0 = VIRTUAL_W * 0.5f - total_w * 0.5f;
        for (int p = 0; p < genome.power_count; ++p) {
            BossPower pw = genome.powers[static_cast<size_t>(p)];
            Color pc = boss_power_color(pw);
            Rectangle pr{px0 + static_cast<float>(p) * (pill_w + 10.0f), py - 10, pill_w, 20};
            DrawRectangleRounded(pr, 0.5f, 4, Color{18, 16, 28, 210});
            DrawRectangleRoundedLinesEx(pr, 0.5f, 4, 1.5f, pc);
            text_fit(g, boss_power_name(pw), {pr.x + pr.width * 0.5f, py}, 13, pill_w - 10, pc, false);
        }
        break;
    }

    // Bottom Control Panel: XP + Wall HP/Shield + Formation + Repair + Family Powers (Ali Q, Salama W, Omar E) +
    // Airstrike!
    DrawRectangleGradientV(0, static_cast<int>(PANEL_TOP), VIRTUAL_W, static_cast<int>(VIRTUAL_H - PANEL_TOP),
                           Color{10, 10, 20, 165}, Color{10, 10, 20, 238});
    float xp_ratio = static_cast<float>(run.xp) / static_cast<float>(xp_for_level(run.level));
    bar({70, PANEL_TOP + 12, 174, 22}, xp_ratio, Color{90, 200, 255, 255}, Color{0, 0, 0, 160});
    text(g, std::format("LV {}", run.level), {157, PANEL_TOP + 23}, 14, WHITE);
    disc(g, {40, PANEL_TOP + 24}, 23, Color{20, 60, 110, 255});
    disc(g, {40, PANEL_TOP + 24}, 19, Color{60, 150, 230, 255});
    text(g, std::format("{}", run.level), {40, PANEL_TOP + 25}, 21, WHITE);

    float wall_ratio = run.wall_hp / run.stats.wall_max;
    Color wc = wall_ratio > 0.5f    ? Color{120, 220, 120, 255}
               : wall_ratio > 0.25f ? Color{240, 200, 60, 255}
                                    : Color{240, 70, 60, 255};
    bar({70, PANEL_TOP + 50, 174, 34}, wall_ratio, wc, Color{0, 0, 0, 160});
    if (run.wall_shield > 0.0f && run.wall_shield_max > 0.0f) {
        bar({70, PANEL_TOP + 43, 174, 7}, run.wall_shield / run.wall_shield_max, Color{120, 230, 255, 255},
            Color{0, 0, 0, 120});
    }
    text_fit(g, std::format("{:.0f}/{:.0f}", std::max(0.0f, run.wall_hp), run.stats.wall_max), {157, PANEL_TOP + 68},
             16, 162, WHITE);

    // Tactical Formation Toggle Button (F) & Wall Repair Button (R).
    {
        Color fc = run.formation == Formation::Wedge   ? Color{195, 130, 42, 255}
                   : run.formation == Formation::Guard ? Color{56, 145, 98, 255}
                                                       : Color{52, 115, 195, 255};
        if (button(game, {252, PANEL_TOP + 6, 102, 34}, formation_name(run.formation), fc, !run.picking && !game.paused,
                   15, KEY_F)) {
            cycle_formation(game);
        }
    }
    {
        int cost = repair_cost(run);
        bool can = run.coins >= cost && wall_ratio < 1.0f && !run.picking && !game.paused;
        if (button(game, {252, PANEL_TOP + 52, 102, 40}, std::format("FIX {}", cost),
                   can ? Color{60, 160, 90, 255} : Color{80, 80, 90, 255}, can, 16, KEY_R)) {
            repair_wall(game);
        }
    }
    DrawRectangleRec({24, PANEL_TOP + 54, 32, 28}, Color{170, 160, 150, 255});
    for (int k = 0; k < 3; ++k)
        DrawRectangleRec({24 + static_cast<float>(k) * 12, PANEL_TOP + 46, 8, 10}, Color{170, 160, 150, 255});

    // Ali (Q), Dad Salama (W), Omar (E) family hero buttons!
    {
        Vec2 ab2{396, PANEL_TOP + 50};
        bool full = run.ali_charge >= 1.0f && run.ali_t < 0.0f;
        if (full) {
            glow(g, ab2, 54 + 5 * std::sin(game.time * 8.0f), Color{255, 130, 190, 150});
        }
        disc(g, ab2, 29, Color{20, 20, 30, 230});
        DrawCircleSector(ab2.to_raylib(), 28, -90, -90 + 360.0f * run.ali_charge, 36, Color{255, 140, 190, 255});
        disc(g, ab2, 23, full ? Color{230, 110, 160, 255} : Color{60, 46, 56, 255});
        begin_scissor(ab2.x - 23, ab2.y - 23, 46, 46);
        draw_kid_portrait(g, Kid::Ali, ab2 + Vec2{0, 2}, 24, game.time, full);
        EndScissorMode();
        if (!full) disc(g, ab2, 23, Color{20, 20, 30, 120});
        if (!game.touch_ui) key_badge(g, {ab2.x + 22, ab2.y - 25}, "Q");
        if (full && !run.picking && !game.paused && hit(game, {ab2.x - 30, ab2.y - 30, 60, 60})) trigger_ali(game);
    }
    {
        Vec2 sb{464, PANEL_TOP + 50};
        bool full = run.salama_charge >= 1.0f && run.salama_t < 0.0f;
        bool active = run.salama_t >= 0.0f;
        if (full || active) {
            glow(g, sb, 56 + 6 * std::sin(game.time * 9.0f), Color{255, 215, 70, 165});
        }
        disc(g, sb, 29, Color{20, 20, 30, 230});
        DrawCircleSector(sb.to_raylib(), 28, -90, -90 + 360.0f * (active ? 1.0f : run.salama_charge), 36,
                         Color{255, 205, 60, 255});
        disc(g, sb, 23, full || active ? Color{70, 120, 210, 255} : Color{42, 48, 66, 255});
        begin_scissor(sb.x - 23, sb.y - 23, 46, 46);
        draw_kid_portrait(g, Kid::Salama, sb + Vec2{0, 2}, 24, game.time, full || active);
        EndScissorMode();
        if (!full && !active) disc(g, sb, 23, Color{20, 20, 30, 120});
        if (!game.touch_ui) key_badge(g, {sb.x + 22, sb.y - 25}, "W");
        if (full && !run.picking && !game.paused && hit(game, {sb.x - 30, sb.y - 30, 60, 60})) trigger_salama(game);
    }
    {
        Vec2 fb{532, PANEL_TOP + 50};
        bool full = run.rage >= 1.0f && run.frenzy_t <= 0.0f;
        bool active = run.frenzy_t > 0.0f;
        if (full || active) {
            glow(g, fb, 56 + 5 * std::sin(game.time * 8.0f), Color{255, 185, 50, 155});
        }
        disc(g, fb, 29, Color{20, 20, 30, 230});
        float k = active ? run.frenzy_t / 6.0f : run.rage;
        DrawCircleSector(fb.to_raylib(), 28, -90, -90 + 360.0f * std::clamp(k, 0.0f, 1.0f), 36,
                         active ? Color{255, 120, 40, 255} : Color{255, 190, 50, 255});
        disc(g, fb, 23, full || active ? Color{230, 120, 30, 255} : Color{60, 50, 40, 255});
        begin_scissor(fb.x - 23, fb.y - 23, 46, 46);
        draw_kid_portrait(g, Kid::Omar, fb + Vec2{0, 2}, 24, game.time, full || active);
        EndScissorMode();
        if (!full && !active) disc(g, fb, 23, Color{20, 20, 30, 120});
        if (!game.touch_ui) key_badge(g, {fb.x + 22, fb.y - 25}, "E");
        if (full && !run.picking && !game.paused && hit(game, {fb.x - 30, fb.y - 30, 60, 60})) trigger_frenzy(game);
    }

    Vec2 ab{638, PANEL_TOP + 48};
    float cd = run.airstrike_cd / run.stats.airstrike_cooldown();
    bool ready = run.airstrike_cd <= 0.0f && run.plane_t < 0.0f;
    if (ready) {
        glow(g, ab, 66 + 6 * std::sin(game.time * 6.0f), Color{255, 130, 60, 140});
    }
    disc(g, ab, 43, Color{20, 20, 30, 255});
    disc(g, ab, 39, ready ? Color{220, 80, 50, 255} : Color{70, 70, 80, 255});
    if (!ready && run.plane_t < 0.0f) {
        DrawCircleSector(ab.to_raylib(), 39, -90, -90 + 360.0f * cd, 32, Color{0, 0, 0, 120});
    }
    if (auto* pl = game.assets.get_texture("plane")) {
        DrawTexturePro(*pl, {0, 0, static_cast<float>(pl->width), static_cast<float>(pl->height)}, {ab.x, ab.y, 56, 56},
                       {28, 28}, 0, WHITE);
    }
    if (!game.touch_ui) key_badge(g, {ab.x, ab.y + 39}, "SPACE");
    if (run.frenzy_t > 0.0f) {
        float a = 0.5f + 0.5f * std::sin(game.time * 10.0f);
        DrawRectangleLinesEx({0, 0, static_cast<float>(VIRTUAL_W), static_cast<float>(VIRTUAL_H)}, 10,
                             Color{255, 170, 40, static_cast<unsigned char>(90 + 80 * a)});
    }
    if (!run.picking && !game.paused && ready && hit(game, {ab.x - 46, ab.y - 46, 92, 92})) trigger_airstrike(game);

    if (!run.picking && !game.paused && run.collapse_t < 0.0f) {
        for (int s = 0; s < TURRET_SLOTS; ++s) {
            if (!game.touch_ui)
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
        text_fit(g, run.banner.subtitle, {VIRTUAL_W * 0.5f, y + 38}, 24 * in, VIRTUAL_W - 40, with_alpha(WHITE, out));
    }

    if (run.hint_timer > 0.0f && !run.picking) {
        float a = std::min(1.0f, run.hint_timer);
        Rectangle r{60, 700, 600, 110};
        DrawRectangleRounded(r, 0.25f, 8, Color{10, 10, 30, static_cast<unsigned char>(200 * a)});
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, 3, with_alpha(GOLD_TINT, a));
        std::string hint = run.hint;
        if (game.touch_ui && hint.starts_with("DRAG")) hint = "DRAG LEFT & RIGHT TO MOVE  |  TAP FORMATION";
        auto nl = hint.find('\n');
        if (nl == std::string::npos) {
            text_fit(g, hint, {360, 755}, 22, 560, with_alpha(WHITE, a));
        } else {
            text_fit(g, hint.substr(0, nl), {360, 738}, 22, 560, with_alpha(WHITE, a));
            text_fit(g, hint.substr(nl + 1), {360, 772}, 22, 560, with_alpha(WHITE, a));
        }
        if (run.hint.starts_with("DRAG")) {
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
