#include "game/render.hpp"
#include "core/game.hpp"
#include "game/formation.hpp"
#include "game/render_parts.hpp"
#include "game/sim.hpp"
#include "game/turrets.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <rlgl.h>
#include <vector>

namespace ls {

// Grounds are cached per theme; blood and scorch decals are painted into the active one.
struct GroundSlot {
    int key{-1};
    RenderTexture2D tex{};
    unsigned use{};
};
std::array<GroundSlot, 6> g_grounds{};
unsigned g_ground_clock = 0;

GroundSlot* find_ground(int key) {
    for (auto& s : g_grounds) {
        if (s.key == key) {
            s.use = ++g_ground_clock;
            return &s;
        }
    }
    return nullptr;
}

const Texture2D* ptex(Game& g, int id) {
    switch (id) {
    case PSmoke:
        if (auto* t = g.assets.get_texture("p_smoke")) return t;
        break;
    case PStar:
        if (auto* t = g.assets.get_texture("p_star")) return t;
        break;
    case PFire:
    case PGlow:
        return &g.gfx.glow;
    case PRing:
        return &g.gfx.ring;
    default:
        break;
    }
    return &g.gfx.disc;
}

void sprite(const Texture2D& t, Vec2 p, float w, float h, float rot_deg, Color c) {
    Rectangle src{0, 0, static_cast<float>(t.width), static_cast<float>(t.height)};
    DrawTexturePro(t, src, {p.x, p.y, w, h}, {w * 0.5f, h * 0.5f}, rot_deg, c);
}

void star_shape(const Gfx&, Vec2 c, float r, Color col) {
    Vector2 pts[12];
    pts[0] = c.to_raylib();
    for (int i = 0; i <= 10; ++i) {
        float a = -PI / 2 - static_cast<float>(i) * PI / 5.0f;
        float rr = (i % 2 == 0) ? r : r * 0.45f;
        pts[i + 1] = {c.x + std::cos(a) * rr, c.y + std::sin(a) * rr};
    }
    DrawTriangleFan(pts, 12, col);
}

void tri(Vec2 a, Vec2 b, Vec2 c, Color col) {
    // raylib wants counter-clockwise winding; accept either by checking the sign.
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) std::swap(b, c);
    DrawTriangle(a.to_raylib(), b.to_raylib(), c.to_raylib(), col);
}

// Enemies and bosses dress for the battleground they march across.
void draw_dress(const Gfx& g, int biome, Vec2 c, float r, float ry, float time, bool big) {
    switch (biome) {
    case 0: {
        Color leaf{86, 150, 60, 255};
        Vec2 top = c + Vec2{r * 0.1f, -ry * 0.88f};
        for (int k = -1; k <= 1; ++k) {
            Vec2 q = top + Vec2{static_cast<float>(k) * r * 0.22f, std::abs(static_cast<float>(k)) * r * 0.08f};
            ellipse(g, q, r * 0.2f + 1.5f, r * 0.11f + 1.5f, darken(leaf, 0.5f));
            ellipse(g, q, r * 0.2f, r * 0.11f, k == 0 ? brighten(leaf, 0.15f) : leaf);
        }
        if (big) disc(g, top + Vec2{r * 0.3f, -r * 0.05f}, r * 0.09f, Color{240, 120, 160, 255});
        break;
    }
    case 1: {
        Color cloth{214, 190, 140, 255};
        Rectangle band{c.x - r * 0.98f, c.y - ry * 0.62f, r * 1.96f, ry * 0.26f};
        DrawRectangleRounded({band.x - 1.5f, band.y - 1.5f, band.width + 3, band.height + 3}, 0.6f, 6,
                             darken(cloth, 0.55f));
        DrawRectangleRounded(band, 0.6f, 6, cloth);
        line({band.x + 4, band.y + band.height * 0.5f}, {band.x + band.width - 4, band.y + band.height * 0.5f}, 1.5f,
             darken(cloth, 0.25f));
        Vec2 knot{c.x + r * 0.95f, c.y - ry * 0.5f};
        float sway = std::sin(time * 4.0f + c.x) * r * 0.06f;
        tri(knot, knot + Vec2{r * 0.35f, r * 0.15f + sway}, knot + Vec2{r * 0.25f, r * 0.42f + sway},
            darken(cloth, 0.1f));
        break;
    }
    case 2: {
        Color snow{246, 250, 255, 255};
        float sw = big ? 0.45f : 0.66f;
        float sy = big ? 0.93f : 0.8f;
        ellipse(g, c + Vec2{r * 0.1f, -ry * sy}, r * sw + 1.5f, r * sw * 0.36f + 1.5f, Color{150, 170, 200, 255});
        ellipse(g, c + Vec2{r * 0.1f, -ry * sy}, r * sw, r * sw * 0.36f, snow);
        ellipse(g, c + Vec2{-r * 0.05f, -ry * (sy + 0.1f)}, r * sw * 0.5f, r * sw * 0.22f, snow);
        for (int k = -1; k <= 1; k += 2) {
            Vec2 base = c + Vec2{static_cast<float>(k) * r * 0.45f, -ry * 0.66f};
            tri(base + Vec2{-r * 0.07f, 0}, base + Vec2{r * 0.07f, 0}, base + Vec2{0, r * 0.22f},
                Color{200, 230, 255, 230});
        }
        break;
    }
    case 3: {
        ellipse(g, c + Vec2{0, -ry * 0.62f}, r * 0.7f, r * 0.28f, Color{40, 30, 30, 160});
        BeginBlendMode(BLEND_ADDITIVE);
        float k = 0.6f + 0.4f * std::sin(time * 5.0f + c.x * 0.1f);
        Color lava{255, 120, 40, static_cast<unsigned char>(200 * k)};
        line(c + Vec2{-r * 0.55f, -r * 0.1f}, c + Vec2{-r * 0.2f, r * 0.15f}, std::max(2.0f, r * 0.07f), lava);
        line(c + Vec2{-r * 0.2f, r * 0.15f}, c + Vec2{-r * 0.3f, r * 0.45f}, std::max(2.0f, r * 0.07f), lava);
        line(c + Vec2{r * 0.5f, -r * 0.3f}, c + Vec2{r * 0.25f, r * 0.05f}, std::max(2.0f, r * 0.07f), lava);
        EndBlendMode();
        break;
    }
    default:
        break;
    }
}

void jagged_mouth(const Gfx& g, Vec2 m, float w, float h, Color outline, Color inside, int teeth, bool fangs) {
    Color bone{238, 230, 206, 255};
    ellipse(g, m, w + 2.0f, h + 2.0f, outline);
    ellipse(g, m, w, h, inside);
    for (int k = 0; k < teeth; ++k) {
        float t = (static_cast<float>(k) + 0.5f) / static_cast<float>(teeth) - 0.5f;
        float x = m.x + t * w * 1.7f;
        float edge = std::sqrt(std::max(0.0f, 1.0f - 4.0f * t * t));
        float tw = w / static_cast<float>(teeth) * 0.8f;
        float top = m.y - h * edge;
        bool fang = fangs && (k == 0 || k == teeth - 1);
        tri({x - tw, top}, {x + tw, top}, {x, top + h * (fang ? 1.4f : 0.8f)}, bone);
        if (!fangs || k % 2 == 1)
            tri({x - tw * 0.7f, m.y + h * edge}, {x + tw * 0.7f, m.y + h * edge}, {x, m.y + h * edge - h * 0.6f}, bone);
    }
}

void draw_barrel(const Gfx& g, Vec2 p, float r, float flash, float hp_ratio, float time) {
    Color red{196, 52, 36, 255};
    Color dark{90, 20, 14, 255};
    Color hoop{70, 66, 70, 255};
    ellipse(g, p + Vec2{0, r * 0.95f}, r * 0.95f, r * 0.32f, Color{0, 0, 0, 90});
    if (hp_ratio < 0.99f)
        glow(g, p, r * 2.4f, Color{255, 140, 40, static_cast<unsigned char>(60 + 60 * std::sin(time * 14.0f))});
    Rectangle body{p.x - r * 0.8f, p.y - r * 0.75f, r * 1.6f, r * 1.6f};
    DrawRectangleRounded({body.x - 2.5f, body.y - 2.5f, body.width + 5, body.height + 5}, 0.35f, 6,
                         Color{30, 10, 8, 255});
    DrawRectangleRounded(body, 0.35f, 6, red);
    DrawRectangleRounded({body.x, body.y + body.height * 0.55f, body.width, body.height * 0.45f}, 0.35f, 6,
                         darken(red, 0.25f));
    DrawRectangleRec({body.x, body.y + body.height * 0.22f, body.width, r * 0.14f}, hoop);
    DrawRectangleRec({body.x, body.y + body.height * 0.7f, body.width, r * 0.14f}, hoop);
    ellipse(g, {p.x, body.y + 1}, r * 0.8f, r * 0.3f, Color{30, 10, 8, 255});
    ellipse(g, {p.x, body.y + 1}, r * 0.74f, r * 0.25f, brighten(red, 0.2f));
    ellipse(g, {p.x + r * 0.3f, body.y + 1}, r * 0.14f, r * 0.08f, dark);
    Vec2 sym{p.x, p.y + r * 0.1f};
    tri(sym + Vec2{0, -r * 0.38f}, sym + Vec2{-r * 0.36f, r * 0.26f}, sym + Vec2{r * 0.36f, r * 0.26f},
        Color{20, 16, 10, 255});
    tri(sym + Vec2{0, -r * 0.3f}, sym + Vec2{-r * 0.28f, r * 0.21f}, sym + Vec2{r * 0.28f, r * 0.21f},
        Color{255, 210, 50, 255});
    DrawRectangleV({sym.x - r * 0.04f, sym.y - r * 0.15f}, {r * 0.08f, r * 0.2f}, Color{20, 16, 10, 255});
    disc(g, sym + Vec2{0, r * 0.12f}, r * 0.045f, Color{20, 16, 10, 255});
    DrawRectangleRounded({body.x + r * 0.15f, body.y + r * 0.2f, r * 0.18f, body.height * 0.6f}, 0.6f, 4,
                         Color{255, 255, 255, 50});
    if (flash > 0.0f)
        DrawRectangleRounded(body, 0.35f, 6,
                             Color{255, 255, 255, static_cast<unsigned char>(std::min(1.0f, flash / 0.09f) * 150)});
}

void draw_bat(const Gfx& g, Vec2 p, float r, float anim, float flash, Color body, bool elite, float time) {
    Color outline = darken(body, 0.75f);
    Color membrane = darken(body, 0.3f);
    ellipse(g, p + Vec2{0, r * 2.2f}, r * 1.4f, r * 0.35f, Color{0, 0, 0, 55});
    if (elite) glow(g, p, r * 2.6f, Color{255, 210, 60, 120});
    float flap = std::sin(anim * 22.0f);
    float span = r * (1.8f + 0.5f * flap);
    float lift = r * 0.6f * flap;
    for (int side = -1; side <= 1; side += 2) {
        float sx = static_cast<float>(side);
        Vec2 root = p + Vec2{sx * r * 0.4f, -r * 0.1f};
        Vec2 tip = p + Vec2{sx * span, -r * 0.5f - lift};
        Vec2 mid = p + Vec2{sx * span * 0.6f, r * 0.55f - lift * 0.4f};
        Vec2 low = p + Vec2{sx * span * 0.9f, r * 0.35f - lift * 0.7f};
        tri(root, tip, mid, outline);
        tri(mid, tip, low, outline);
        Vec2 inset{-sx * 1.5f, 1.5f};
        tri(root + inset, tip + inset * 1.5f, mid + Vec2{0, -1.5f}, membrane);
        tri(mid + Vec2{0, -1.5f}, tip + inset * 1.5f, low + Vec2{-sx * 1.5f, -1.5f}, membrane);
        line(root, tip, std::max(1.5f, r * 0.12f), outline);
    }
    disc(g, p, r * 0.62f + 2.0f, outline);
    disc(g, p, r * 0.62f, body);
    for (int side = -1; side <= 1; side += 2) {
        Vec2 ear = p + Vec2{static_cast<float>(side) * r * 0.32f, -r * 0.5f};
        tri(ear + Vec2{-r * 0.14f, 0}, ear + Vec2{r * 0.14f, 0},
            ear + Vec2{static_cast<float>(side) * r * 0.08f, -r * 0.4f}, outline);
    }
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, p + Vec2{-r * 0.22f, -r * 0.05f}, r * 0.45f, Color{255, 50, 30, 200});
    glow(g, p + Vec2{r * 0.22f, -r * 0.05f}, r * 0.45f, Color{255, 50, 30, 200});
    EndBlendMode();
    disc(g, p + Vec2{-r * 0.22f, -r * 0.05f}, r * 0.12f, Color{255, 220, 120, 255});
    disc(g, p + Vec2{r * 0.22f, -r * 0.05f}, r * 0.12f, Color{255, 220, 120, 255});
    tri(p + Vec2{-r * 0.16f, r * 0.25f}, p + Vec2{-r * 0.06f, r * 0.25f}, p + Vec2{-r * 0.11f, r * 0.5f}, WHITE);
    tri(p + Vec2{r * 0.06f, r * 0.25f}, p + Vec2{r * 0.16f, r * 0.25f}, p + Vec2{r * 0.11f, r * 0.5f}, WHITE);
    if (flash > 0.0f)
        disc(g, p, r * 0.7f, Color{255, 255, 255, static_cast<unsigned char>(std::min(1.0f, flash / 0.09f) * 160)});
    (void)time;
}

// Enemies are one silhouette (feet, arms and body share a single outline) with an angry face
// that tells each type apart at a glance.
void draw_monster(const Gfx& g, EnemyType type, Vec2 p, float r, float anim, float flash, bool slowed, bool elite,
                  float time, float shield_ratio, float shield_flash, bool armored, Color tint, float tint_amount,
                  int biome, float hurt) {
    if (type == EnemyType::Barrel) {
        draw_barrel(g, p, r, flash, 1.0f, time);
        return;
    }
    if (type == EnemyType::Flyer) {
        Color bat = blend(enemy_body_color(type), tint, tint_amount);
        if (slowed) bat = mix(bat, Color{170, 220, 255, 255}, 0.45f);
        draw_bat(g, p, r, anim, flash, bat, elite, time);
        return;
    }
    Color base = blend(enemy_body_color(type), tint, tint_amount);
    if (slowed) base = mix(base, Color{170, 220, 255, 255}, 0.45f);
    Color body = base;
    Color shade = darken(base, 0.38f);
    Color rim = brighten(base, 0.3f);
    Color outline = darken(base, 0.78f);
    Color inside{46, 8, 14, 255};
    float stride = type == EnemyType::Runner ? 16.0f : 9.0f;
    float step = std::sin(anim * stride);
    float squash = type == EnemyType::Exploder ? 1.0f + std::sin(anim * 8.0f) * 0.07f : 1.0f - std::abs(step) * 0.035f;
    Vec2 c = p - Vec2{0, std::abs(step) * r * 0.06f};
    float ry = r * squash * (1.0f - 0.16f * hurt);
    float rw = r * (1.0f + 0.14f * hurt);
    float ow = std::max(2.0f, r * 0.1f);
    bool big = type == EnemyType::Brute;

    ellipse(g, p + Vec2{0, r * 0.92f}, r * 1.0f, r * 0.34f, Color{0, 0, 0, 85});
    if (elite) glow(g, c, r * 2.3f, Color{255, 210, 60, static_cast<unsigned char>(100 + 50 * std::sin(time * 6.0f))});
    if (type == EnemyType::Exploder) glow(g, c, r * 2.1f, Color{220, 255, 80, 90});

    Vec2 feet[2] = {p + Vec2{-r * 0.38f, r * 0.9f + step * r * 0.08f},
                    p + Vec2{r * 0.38f, r * 0.9f - step * r * 0.08f}};
    float arm_w = r * (big ? 0.5f : 0.34f);
    float hand_r = r * (big ? 0.3f : 0.2f);
    Vec2 shoulders[2], hands[2];
    for (int side = 0; side < 2; ++side) {
        float sx = side == 0 ? -1.0f : 1.0f;
        float reach = (side == 0 ? -step : step) * r * 0.14f;
        shoulders[side] = c + Vec2{sx * r * 0.7f, r * 0.05f};
        hands[side] = c + Vec2{sx * r * (big ? 0.95f : 0.82f), r * 0.66f + reach};
    }

    for (const auto& f : feet) ellipse(g, f, r * 0.25f + ow, r * 0.16f + ow, outline);
    for (int k = 0; k < 2; ++k) {
        line(shoulders[k], hands[k], arm_w + ow * 2.0f, outline);
        disc(g, hands[k], hand_r + ow, outline);
    }
    ellipse(g, c, rw + ow, ry + ow, outline);

    for (const auto& f : feet) ellipse(g, f, r * 0.25f, r * 0.16f, shade);
    for (int k = 0; k < 2; ++k) line(shoulders[k], hands[k], arm_w, body);
    ellipse(g, c, rw, ry, body);
    for (int k = 0; k < 3; ++k) {
        float t = static_cast<float>(k) / 2.0f;
        ellipse(g, c + Vec2{0, ry * (0.46f - 0.1f * t)}, r * (0.9f - 0.08f * t), ry * (0.5f - 0.08f * t),
                with_alpha(shade, 0.3f));
    }
    glow(g, c + Vec2{-r * 0.25f, -ry * 0.5f}, r * 0.8f, with_alpha(rim, 0.45f));
    glow(g, c + Vec2{-r * 0.35f, -ry * 0.58f}, r * 0.22f, Color{255, 255, 255, 60});
    for (int k = 0; k < 2; ++k) {
        disc(g, hands[k], hand_r, body);
        if (big || type == EnemyType::Runner) {
            for (int cl = -1; cl <= 1; ++cl) {
                Vec2 at = hands[k] + Vec2{static_cast<float>(cl) * hand_r * 0.55f, hand_r * 0.7f};
                tri(at + Vec2{-hand_r * 0.2f, 0}, at + Vec2{hand_r * 0.2f, 0}, at + Vec2{0, hand_r * 0.6f},
                    Color{236, 228, 204, 255});
            }
        }
    }

    if (big) {
        float hr = r * 0.36f;
        for (int side = -1; side <= 1; side += 2) {
            float sx = static_cast<float>(side);
            Vec2 root = c + Vec2{sx * r * 0.5f, -ry * 0.68f};
            for (int pass = 0; pass < 2; ++pass) {
                for (int i = 0; i <= 6; ++i) {
                    float t = static_cast<float>(i) / 6.0f;
                    Vec2 q = root + Vec2{sx * hr * (0.6f * t + 0.3f * t * t), -hr * (1.1f * t - 0.3f * t * t)};
                    float w = hr * 0.35f * (1.0f - 0.8f * t);
                    disc(g, q, pass == 0 ? w + ow * 0.7f : w, pass == 0 ? outline : Color{236, 226, 200, 255});
                }
            }
        }
    }

    auto brow = [&](Vec2 e, float er, float sx, float droop) {
        line(e + Vec2{-sx * er * 1.4f, -er * (1.5f + droop)}, e + Vec2{sx * er * 1.0f, -er * 0.7f},
             std::max(2.5f, r * 0.13f), outline);
    };
    auto eye = [&](Vec2 e, float er, Color iris, bool slit) {
        disc(g, e, er + std::max(1.5f, r * 0.05f), outline);
        disc(g, e, er, iris);
        if (slit) {
            DrawRectangleRounded({e.x - er * 0.18f, e.y - er * 0.7f, er * 0.36f, er * 1.4f}, 0.8f, 4,
                                 Color{20, 8, 8, 255});
        } else {
            disc(g, e + Vec2{0, er * 0.3f}, er * 0.5f, Color{20, 8, 8, 255});
        }
        disc(g, e + Vec2{-er * 0.3f, -er * 0.25f}, er * 0.22f, Color{255, 255, 255, 200});
    };

    switch (type) {
    case EnemyType::Walker: {
        float er = r * 0.22f;
        Vec2 el = c + Vec2{-r * 0.33f, -r * 0.08f};
        Vec2 erp = c + Vec2{r * 0.36f, -r * 0.02f};
        eye(el, er * 1.1f, Color{240, 240, 200, 255}, false);
        eye(erp, er * 0.85f, Color{240, 240, 200, 255}, false);
        brow(el, er, -1.0f, 0.2f);
        brow(erp, er, 1.0f, -0.3f);
        jagged_mouth(g, c + Vec2{r * 0.05f, r * 0.45f}, r * 0.38f, r * 0.14f, outline, inside, 5, false);
        ellipse(g, c + Vec2{r * 0.22f, r * 0.62f}, r * 0.05f, r * 0.1f, Color{170, 230, 120, 200});
        break;
    }
    case EnemyType::Runner: {
        float er = r * 0.22f;
        for (int side = -1; side <= 1; side += 2) {
            Vec2 e = c + Vec2{static_cast<float>(side) * r * 0.33f, -r * 0.05f};
            glow(g, e, er * 2.5f, Color{255, 220, 60, 120});
            eye(e, er, Color{255, 220, 70, 255}, true);
            brow(e, er, static_cast<float>(side), 0.0f);
        }
        jagged_mouth(g, c + Vec2{0, r * 0.45f}, r * 0.42f, r * 0.16f, outline, inside, 6, true);
        break;
    }
    case EnemyType::Brute: {
        float er = r * 0.17f;
        for (int side = -1; side <= 1; side += 2) {
            Vec2 e = c + Vec2{static_cast<float>(side) * r * 0.3f, -r * 0.1f};
            eye(e, er, Color{255, 200, 80, 255}, false);
            brow(e, er * 1.2f, static_cast<float>(side), 0.1f);
        }
        jagged_mouth(g, c + Vec2{0, r * 0.42f}, r * 0.5f, r * 0.18f, outline, inside, 6, true);
        for (int side = -1; side <= 1; side += 2) {
            Vec2 t0 = c + Vec2{static_cast<float>(side) * r * 0.35f, r * 0.5f};
            tri(t0 + Vec2{-r * 0.07f, 0}, t0 + Vec2{r * 0.07f, 0},
                t0 + Vec2{static_cast<float>(side) * r * 0.05f, -r * 0.35f}, Color{238, 230, 206, 255});
        }
        break;
    }
    case EnemyType::Spitter: {
        Vec2 e = c + Vec2{0, -r * 0.14f};
        float er = r * 0.34f;
        eye(e, er, Color{230, 255, 140, 255}, true);
        line(e + Vec2{-er * 1.2f, -er * 1.0f}, e + Vec2{er * 1.2f, -er * 0.55f}, std::max(3.0f, r * 0.14f), outline);
        float open = 0.16f + 0.1f * std::sin(anim * 4.0f);
        jagged_mouth(g, c + Vec2{0, r * 0.5f}, r * 0.34f, r * open, outline, Color{50, 8, 50, 255}, 4, false);
        ellipse(g, c + Vec2{0, r * 0.55f}, r * 0.15f, r * open * 0.4f, Color{170, 255, 120, 255});
        break;
    }
    case EnemyType::Exploder: {
        for (int k = 0; k < 5; ++k) {
            float a = static_cast<float>(k) * 1.26f + 0.4f;
            Vec2 q = c + Vec2{std::cos(a) * r * 0.62f, std::sin(a) * r * 0.55f};
            float pr = r * (0.12f + 0.04f * std::sin(anim * 6.0f + static_cast<float>(k)));
            disc(g, q, pr + 1.5f, darken(body, 0.45f));
            disc(g, q, pr, Color{250, 250, 130, 255});
        }
        for (int side = -1; side <= 1; side += 2) {
            Vec2 e = c + Vec2{static_cast<float>(side) * r * 0.27f, -r * 0.1f};
            eye(e, r * 0.15f, Color{255, 255, 220, 255}, false);
        }
        jagged_mouth(g, c + Vec2{0, r * 0.33f}, r * 0.46f, r * 0.12f, outline, inside, 7, false);
        break;
    }
    case EnemyType::Shielder: {
        Rectangle visor{c.x - r * 0.62f, c.y - r * 0.3f, r * 1.24f, r * 0.34f};
        DrawRectangleRounded({visor.x - 2, visor.y - 2, visor.width + 4, visor.height + 4}, 0.6f, 6, outline);
        DrawRectangleRounded(visor, 0.6f, 6, Color{70, 76, 88, 255});
        glow(g, {c.x - r * 0.25f, visor.y + visor.height * 0.5f}, r * 0.25f, Color{255, 80, 60, 200});
        glow(g, {c.x + r * 0.25f, visor.y + visor.height * 0.5f}, r * 0.25f, Color{255, 80, 60, 200});
        DrawRectangleRounded({c.x - r * 0.45f, visor.y + visor.height * 0.4f, r * 0.9f, visor.height * 0.2f}, 0.8f, 4,
                             Color{255, 120, 80, 255});
        jagged_mouth(g, c + Vec2{0, r * 0.32f}, r * 0.3f, r * 0.1f, outline, inside, 4, false);
        break;
    }
    case EnemyType::Splitter: {
        line(c + Vec2{-r * 0.85f, -r * 0.05f}, c + Vec2{r * 0.85f, r * 0.08f}, std::max(2.0f, r * 0.08f), outline);
        line(c + Vec2{0, -r * 0.9f}, c + Vec2{-r * 0.1f, r * 0.9f}, std::max(2.0f, r * 0.08f), outline);
        for (int k = 0; k < 3; ++k) {
            Vec2 e = c + Vec2{static_cast<float>(k - 1) * r * 0.42f, -r * 0.3f + (k == 1 ? -r * 0.1f : 0.0f)};
            eye(e, r * 0.13f, Color{255, 236, 180, 255}, true);
            brow(e, r * 0.13f, k == 0 ? -1.0f : 1.0f, 0.0f);
        }
        jagged_mouth(g, c + Vec2{0, r * 0.45f}, r * 0.34f, r * 0.12f, outline, inside, 5, false);
        break;
    }
    case EnemyType::Flyer:
    case EnemyType::Barrel:
    case EnemyType::Boss:
    case EnemyType::Count:
        break;
    }

    if (!armored) draw_dress(g, biome, c, r, ry, time, big);
    if (armored) {
        ellipse(g, c + Vec2{0, -r * 0.55f}, r * 0.82f, r * 0.44f, Color{50, 54, 62, 255});
        ellipse(g, c + Vec2{0, -r * 0.6f}, r * 0.74f, r * 0.36f, Color{150, 156, 170, 255});
        ellipse(g, c + Vec2{-r * 0.22f, -r * 0.72f}, r * 0.26f, r * 0.1f, Color{235, 240, 250, 190});
    }
    if (elite) {
        float cw = r * 0.5f;
        float ch = cw * 0.55f;
        Vec2 cb{c.x, c.y - ry - ch * 0.15f};
        Color gold{255, 205, 60, 255};
        Color gold_dark{170, 110, 20, 255};
        DrawRectangleRounded({cb.x - cw - 1.5f, cb.y - ch * 0.45f - 1.5f, cw * 2 + 3, ch * 0.5f + 3}, 0.3f, 4,
                             gold_dark);
        for (int k = 0; k < 3; ++k) {
            float x0 = cb.x - cw + static_cast<float>(k) * (2.0f * cw / 3.0f);
            float x1 = x0 + 2.0f * cw / 3.0f;
            tri({x0 - 1.5f, cb.y - ch * 0.3f}, {x1 + 1.5f, cb.y - ch * 0.3f}, {(x0 + x1) * 0.5f, cb.y - ch * 1.25f},
                gold_dark);
            tri({x0, cb.y - ch * 0.35f}, {x1, cb.y - ch * 0.35f}, {(x0 + x1) * 0.5f, cb.y - ch * 1.1f}, gold);
        }
        DrawRectangleRounded({cb.x - cw, cb.y - ch * 0.45f, cw * 2, ch * 0.5f}, 0.3f, 4, gold);
    }
    if (flash > 0.0f)
        ellipse(g, c, r + ow, ry + ow,
                Color{255, 255, 255, static_cast<unsigned char>(std::min(1.0f, flash / 0.09f) * 150)});

    if (shield_ratio > 0.0f) {
        Rectangle sh{c.x - r * 1.15f, c.y + r * 0.45f, r * 2.3f, r * 0.8f};
        Color steel = mix(Color{120, 132, 148, 255}, Color{190, 90, 76, 255}, 1.0f - shield_ratio);
        if (shield_flash > 0.0f) steel = brighten(steel, 0.6f);
        DrawRectangleRounded({sh.x, sh.y + 4, sh.width, sh.height}, 0.45f, 6, Color{0, 0, 0, 90});
        DrawRectangleRounded({sh.x - 2, sh.y - 2, sh.width + 4, sh.height + 4}, 0.45f, 6, Color{36, 40, 48, 255});
        DrawRectangleRounded(sh, 0.45f, 6, steel);
        DrawRectangleRounded({sh.x + 5, sh.y + 3, sh.width - 10, sh.height * 0.28f}, 0.5f, 6, Color{255, 255, 255, 80});
        line({sh.x + sh.width * 0.5f, sh.y + 3}, {sh.x + sh.width * 0.5f, sh.y + sh.height - 3}, 2.0f,
             darken(steel, 0.3f));
        disc(g, {c.x, sh.y + sh.height * 0.5f}, r * 0.13f, Color{230, 196, 80, 255});
        if (shield_ratio < 0.6f) {
            line({sh.x + sh.width * 0.25f, sh.y + 2}, {sh.x + sh.width * 0.36f, sh.y + sh.height * 0.6f}, 2.0f,
                 Color{30, 30, 30, 220});
        }
        if (shield_ratio < 0.3f) {
            line({sh.x + sh.width * 0.7f, sh.y + sh.height - 2}, {sh.x + sh.width * 0.62f, sh.y + sh.height * 0.3f},
                 2.0f, Color{30, 30, 30, 220});
        }
    }
}

// Bosses are drawn as one silhouette: every limb's outline goes down first, then every fill,
// so arms and shoulders grow out of the body instead of sitting on top of it.
void draw_boss(const Gfx& g, const BossGenome& gen, int biome, Vec2 p, float r, float anim, float flash, bool enraged,
               float time, float shield_ratio) {
    Color base = enraged ? mix(gen.body, Color{200, 30, 20, 255}, 0.4f + 0.15f * std::sin(time * 10.0f)) : gen.body;
    Color body = darken(base, 0.08f);
    Color shade = darken(base, 0.42f);
    Color rim = brighten(base, 0.28f);
    Color outline = darken(base, 0.82f);
    Color bone{232, 220, 192, 255};
    Color eye = enraged ? Color{255, 60, 30, 255} : gen.accent;
    float breathe = std::sin(time * 2.4f) * 0.03f;
    float step = std::sin(anim * 5.0f);
    Vec2 c = p - Vec2{0, std::abs(step) * r * 0.05f};
    float rx = r * gen.girth * (1.0f + breathe);
    float ry = r * (1.0f - breathe * 0.5f);
    float ow = std::max(3.0f, r * 0.075f);

    BeginBlendMode(BLEND_ALPHA);
    glow(g, p + Vec2{0, r * 0.4f}, r * 2.2f, Color{0, 0, 0, 110});
    EndBlendMode();
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, c, r * 2.1f, with_alpha(eye, enraged ? 0.45f : 0.25f));
    EndBlendMode();
    ellipse(g, p + Vec2{0, r * 0.98f}, rx * 1.1f, r * 0.32f, Color{0, 0, 0, 110});

    struct Limb {
        Vec2 shoulder, elbow, hand;
    };
    Limb limbs[2];
    for (int side = 0; side < 2; ++side) {
        float sx = side == 0 ? -1.0f : 1.0f;
        float reach = (side == 0 ? -step : step) * r * 0.12f;
        limbs[side].shoulder = c + Vec2{sx * rx * 0.72f, -ry * 0.1f};
        limbs[side].elbow = c + Vec2{sx * rx * 1.02f, ry * 0.35f + reach * 0.5f};
        limbs[side].hand = c + Vec2{sx * rx * 0.8f, ry * 0.82f + reach};
    }
    float arm_w = r * 0.46f;
    float fist = r * 0.32f;
    Vec2 feet[2] = {p + Vec2{-rx * 0.42f, r * 0.95f + step * r * 0.06f},
                    p + Vec2{rx * 0.42f, r * 0.95f - step * r * 0.06f}};

    // Silhouette outline pass.
    for (const auto& f : feet) ellipse(g, f, r * 0.34f + ow, r * 0.2f + ow, outline);
    for (const auto& l : limbs) {
        line(l.shoulder, l.elbow, arm_w + ow * 2, outline);
        line(l.elbow, l.hand, arm_w * 0.9f + ow * 2, outline);
        disc(g, l.elbow, arm_w * 0.5f + ow, outline);
        disc(g, l.hand, fist + ow, outline);
    }
    ellipse(g, c + Vec2{0, -ry * 0.05f}, rx * 1.08f + ow, ry * 0.78f + ow, outline);
    ellipse(g, c, rx + ow, ry + ow, outline);

    // Back spikes sit behind the fill so they read as part of the hide.
    for (int k = 0; k < gen.spikes; ++k) {
        float a = -PI * 0.88f + PI * 0.76f * static_cast<float>(k) / static_cast<float>(std::max(1, gen.spikes - 1));
        Vec2 dir{std::cos(a), std::sin(a)};
        Vec2 n{-dir.y, dir.x};
        Vec2 base_pt = c + Vec2{dir.x * rx * 0.85f, dir.y * ry * 0.85f};
        Vec2 tip = c + Vec2{dir.x * rx * 1.35f, dir.y * ry * 1.35f};
        tri(base_pt + n * (r * 0.16f + ow), base_pt - n * (r * 0.16f + ow), tip + dir * ow, outline);
        tri(base_pt + n * r * 0.14f, base_pt - n * r * 0.14f, tip, shade);
        tri(base_pt + n * r * 0.05f, base_pt - n * r * 0.02f, tip - dir * r * 0.05f, darken(gen.accent, 0.25f));
    }

    // Fill pass: one continuous body.
    for (const auto& f : feet) ellipse(g, f, r * 0.34f, r * 0.2f, shade);
    for (const auto& l : limbs) {
        line(l.shoulder, l.elbow, arm_w, body);
        line(l.elbow, l.hand, arm_w * 0.9f, body);
        disc(g, l.elbow, arm_w * 0.5f, body);
    }
    ellipse(g, c + Vec2{0, -ry * 0.05f}, rx * 1.08f, ry * 0.78f, body);
    ellipse(g, c, rx, ry, body);

    // Shading: a dark belly that fades up into a lit back.
    for (int k = 0; k < 4; ++k) {
        float t = static_cast<float>(k) / 3.0f;
        ellipse(g, c + Vec2{0, ry * (0.5f - 0.12f * t)}, rx * (0.92f - 0.1f * t), ry * (0.5f - 0.08f * t),
                with_alpha(shade, 0.35f));
    }
    glow(g, c + Vec2{-rx * 0.2f, -ry * 0.55f}, rx * 0.8f, with_alpha(rim, 0.5f));
    glow(g, c + Vec2{-rx * 0.35f, -ry * 0.62f}, rx * 0.25f, Color{255, 255, 255, 60});
    for (const auto& l : limbs) {
        line(l.shoulder + Vec2{0, -arm_w * 0.2f}, l.elbow + Vec2{0, -arm_w * 0.2f}, arm_w * 0.35f,
             with_alpha(rim, 0.45f));
    }

    if (gen.plates) {
        for (int k = -1; k <= 1; ++k) {
            Vec2 q =
                c + Vec2{static_cast<float>(k) * rx * 0.5f, -ry * 0.5f + std::abs(static_cast<float>(k)) * ry * 0.18f};
            DrawPoly(q.to_raylib(), 6, r * 0.22f + ow * 0.6f, 30, outline);
            DrawPoly(q.to_raylib(), 6, r * 0.22f, 30, Color{110, 112, 124, 255});
            DrawPoly((q - Vec2{r * 0.04f, r * 0.05f}).to_raylib(), 6, r * 0.12f, 30, Color{170, 174, 188, 255});
        }
    }

    // Scars.
    line(c + Vec2{rx * 0.15f, -ry * 0.25f}, c + Vec2{rx * 0.5f, ry * 0.1f}, std::max(2.0f, r * 0.05f),
         with_alpha(outline, 0.6f));
    line(c + Vec2{rx * 0.22f, -ry * 0.12f}, c + Vec2{rx * 0.38f, -ry * 0.18f}, std::max(2.0f, r * 0.04f),
         with_alpha(outline, 0.6f));

    // Fists with claws.
    for (const auto& l : limbs) {
        disc(g, l.hand, fist, body);
        disc(g, l.hand + Vec2{0, fist * 0.3f}, fist * 0.8f, with_alpha(shade, 0.5f));
        for (int k = -1; k <= 1; ++k) {
            Vec2 at = l.hand + Vec2{static_cast<float>(k) * fist * 0.55f, fist * 0.75f};
            tri(at + Vec2{-fist * 0.18f, 0}, at + Vec2{fist * 0.18f, 0}, at + Vec2{0, fist * 0.55f}, bone);
        }
    }

    // Curved, tapering horns built from overlapping discs.
    for (int k = 0; k < gen.horns; ++k) {
        float side = (k % 2 == 0) ? -1.0f : 1.0f;
        float tier = static_cast<float>(k / 2);
        Vec2 root = c + Vec2{side * rx * (0.42f + 0.22f * tier), -ry * (0.72f - 0.18f * tier)};
        float len = r * (0.85f - 0.2f * tier);
        float thick = r * (0.17f - 0.03f * tier);
        constexpr int SEG = 10;
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i <= SEG; ++i) {
                float t = static_cast<float>(i) / SEG;
                Vec2 q = root + Vec2{side * len * (0.55f * t + 0.25f * t * t), -len * (0.9f * t - 0.35f * t * t)};
                float w = thick * (1.0f - 0.85f * t);
                if (pass == 0) {
                    disc(g, q, w + ow * 0.8f, outline);
                } else {
                    disc(g, q, w, mix(bone, darken(bone, 0.35f), (i % 3 == 0) ? 0.5f : 0.0f));
                }
            }
        }
    }

    // Brow, eyes and maw.
    float er = r * (gen.eyes == 1 ? 0.26f : gen.eyes == 2 ? 0.19f : 0.15f);
    float span = gen.eyes == 1 ? 0.0f : rx * 0.42f;
    Vec2 brow_l = c + Vec2{-rx * 0.55f, -ry * 0.32f};
    Vec2 brow_r = c + Vec2{rx * 0.55f, -ry * 0.32f};
    Vec2 brow_mid = c + Vec2{0, -ry * 0.08f};
    float bw = r * 0.16f;
    line(brow_l, brow_mid, bw + ow, outline);
    line(brow_mid, brow_r, bw + ow, outline);
    line(brow_l, brow_mid, bw, shade);
    line(brow_mid, brow_r, bw, shade);
    for (int k = 0; k < gen.eyes; ++k) {
        float x = gen.eyes == 1 ? 0.0f : (static_cast<float>(k) - static_cast<float>(gen.eyes - 1) * 0.5f) * span;
        Vec2 e = c + Vec2{x, -ry * 0.02f};
        float slant = (x < 0 ? 1.0f : x > 0 ? -1.0f : 0.0f) * er * 0.6f;
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, e, er * 3.4f, with_alpha(eye, 0.75f));
        EndBlendMode();
        tri(e + Vec2{-er * 1.25f, -er * 0.45f - slant}, e + Vec2{er * 1.25f, -er * 0.45f + slant},
            e + Vec2{0, er * 0.8f}, Color{20, 6, 6, 255});
        tri(e + Vec2{-er * 1.0f, -er * 0.3f - slant * 0.8f}, e + Vec2{er * 1.0f, -er * 0.3f + slant * 0.8f},
            e + Vec2{0, er * 0.6f}, eye);
        tri(e + Vec2{-er * 0.4f, -er * 0.15f}, e + Vec2{er * 0.4f, -er * 0.15f}, e + Vec2{0, er * 0.3f},
            Color{255, 250, 220, 255});
    }
    Vec2 m = c + Vec2{0, ry * 0.42f};
    float mw = rx * 0.5f;
    float mh = ry * (0.2f + 0.05f * std::sin(time * 6.0f));
    ellipse(g, m, mw + ow * 0.6f, mh + ow * 0.6f, outline);
    ellipse(g, m, mw, mh, Color{50, 6, 10, 255});
    BeginBlendMode(BLEND_ADDITIVE);
    ellipse(g, m + Vec2{0, mh * 0.2f}, mw * 0.6f, mh * 0.5f, with_alpha(eye, 0.5f));
    EndBlendMode();
    constexpr int TEETH = 6;
    for (int k = 0; k < TEETH; ++k) {
        float t = (static_cast<float>(k) + 0.5f) / TEETH - 0.5f;
        float x = m.x + t * mw * 1.8f;
        float edge = std::sqrt(std::max(0.0f, 1.0f - 4.0f * t * t));
        float tw = mw * 0.15f;
        float top = m.y - mh * edge;
        float bottom = m.y + mh * edge;
        tri({x - tw, top}, {x + tw, top}, {x, top + mh * (0.9f + 0.3f * (k % 2))}, bone);
        tri({x - tw * 0.8f, bottom}, {x + tw * 0.8f, bottom}, {x, bottom - mh * 0.7f}, bone);
    }

    draw_dress(g, biome, c, r, ry, time, true);
    if (gen.crown) {
        float cw = r * 0.55f;
        float ch = cw * 0.55f;
        Vec2 cb{c.x, c.y - ry - ch * 0.05f};
        Color gold = gen.accent;
        Color gold_dark = darken(gold, 0.5f);
        DrawRectangleRounded({cb.x - cw - 1.5f, cb.y - ch * 0.45f - 1.5f, cw * 2 + 3, ch * 0.5f + 3}, 0.3f, 4,
                             gold_dark);
        for (int k = 0; k < 3; ++k) {
            float x0 = cb.x - cw + static_cast<float>(k) * (2.0f * cw / 3.0f);
            float x1 = x0 + 2.0f * cw / 3.0f;
            tri({x0 - 1.5f, cb.y - ch * 0.3f}, {x1 + 1.5f, cb.y - ch * 0.3f}, {(x0 + x1) * 0.5f, cb.y - ch * 1.25f},
                gold_dark);
            tri({x0, cb.y - ch * 0.35f}, {x1, cb.y - ch * 0.35f}, {(x0 + x1) * 0.5f, cb.y - ch * 1.1f}, gold);
        }
        DrawRectangleRounded({cb.x - cw, cb.y - ch * 0.45f, cw * 2, ch * 0.5f}, 0.3f, 4, gold);
    }
    if (enraged) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int k = 0; k < 3; ++k) {
            float a = time * 3.0f + static_cast<float>(k) * 2.1f;
            glow(g, c + Vec2{std::cos(a) * rx * 0.9f, -ry * 0.9f + std::sin(a * 2.0f) * r * 0.1f}, r * 0.25f,
                 Color{255, 80, 30, 160});
        }
        EndBlendMode();
    }
    if (flash > 0.0f)
        ellipse(g, c, rx + ow, ry + ow,
                Color{255, 255, 255, static_cast<unsigned char>(std::min(1.0f, flash / 0.09f) * 130)});
    if (shield_ratio > 0.0f) {
        BeginBlendMode(BLEND_ADDITIVE);
        ring(g, c, r * 1.4f, Color{140, 190, 255, static_cast<unsigned char>(120 + 100 * shield_ratio)});
        glow(g, c, r * 1.6f, Color{120, 170, 255, static_cast<unsigned char>(60 * shield_ratio)});
        EndBlendMode();
    }
}

void draw_soldier(const Gfx& g, Vec2 p, float recoil, UnitType u, float scale, float time, int biome) {
    const auto& def = unit_def(u);
    auto S = [scale](float v) { return v * scale; };
    Color dark{34, 34, 40, 255};
    Color metal{86, 88, 98, 255};
    Color skin{232, 184, 140, 255};
    static const Color outfit[4] = {{96, 116, 64, 255}, {206, 178, 128, 255}, {236, 240, 248, 255}, {64, 62, 68, 255}};
    Color uni = biome >= 0 && biome < 4 ? mix(def.uniform, outfit[biome], 0.35f) : def.uniform;
    Color uni_dark = darken(uni, 0.5f);
    float kick = recoil * S(3.0f);
    Vec2 hand = p + Vec2{S(6.5f), S(-2) + kick};
    float len = gun_length(static_cast<int>(u));

    ellipse(g, p + Vec2{0, S(8)}, S(13), S(5), Color{0, 0, 0, 80});
    if (u == UnitType::Flamer) {
        for (int k = -1; k <= 1; k += 2) {
            Rectangle tank{p.x + S(static_cast<float>(k) * 4.0f) - S(3.5f), p.y + S(1), S(7), S(11)};
            DrawRectangleRounded({tank.x - S(1), tank.y - S(1), tank.width + S(2), tank.height + S(2)}, 0.6f, 6, dark);
            DrawRectangleRounded(tank, 0.6f, 6, Color{226, 116, 48, 255});
        }
    } else {
        Rectangle pack{p.x - S(6), p.y + S(1), S(12), S(9)};
        DrawRectangleRounded({pack.x - S(1), pack.y - S(1), pack.width + S(2), pack.height + S(2)}, 0.4f, 4, dark);
        DrawRectangleRounded(pack, 0.4f, 4, darken(uni, 0.3f));
    }
    ellipse(g, p, S(12), S(8.5f), uni_dark);
    ellipse(g, p, S(10.5f), S(7.2f), uni);
    ellipse(g, p + Vec2{S(-3), S(-2.5f)}, S(4), S(2.2f), Color{255, 255, 255, 45});

    Vec2 support = hand + Vec2{0, S(-len * 0.4f)};
    line(p + Vec2{S(8), S(-1)}, hand, S(4.2f), uni_dark);
    line(p + Vec2{S(8), S(-1)}, hand, S(2.8f), uni);
    line(p + Vec2{S(-8), S(-1)}, support, S(4.2f), uni_dark);
    line(p + Vec2{S(-8), S(-1)}, support, S(2.8f), uni);

    Vec2 head = p + Vec2{S(-1.5f), S(-1.5f)};
    disc(g, head, S(7.0f), Color{20, 22, 26, 255});
    disc(g, head, S(6.0f), def.helmet);
    ring(g, head, S(4.6f), with_alpha(darken(def.helmet, 0.4f), 0.9f));
    disc(g, head + Vec2{S(-1.8f), S(-1.8f)}, S(2.2f), Color{255, 255, 255, 110});
    switch (biome) {
    case 0:
        for (int k = 0; k < 5; ++k) {
            float a = static_cast<float>(k) * 1.26f;
            disc(g, head + Vec2{std::cos(a), std::sin(a)} * S(3.6f), S(1.3f), Color{70, 120, 50, 255});
        }
        break;
    case 1:
        line(head + Vec2{S(-6), S(1)}, head + Vec2{S(6), S(1)}, S(2.2f), Color{60, 40, 30, 255});
        disc(g, head + Vec2{S(-3), S(1)}, S(1.8f), Color{120, 200, 230, 255});
        disc(g, head + Vec2{S(3), S(1)}, S(1.8f), Color{120, 200, 230, 255});
        ellipse(g, p + Vec2{0, S(-1)}, S(7), S(2.5f), Color{214, 190, 140, 255});
        break;
    case 2:
        ring(g, head, S(6.6f), Color{250, 250, 255, 255});
        ring(g, head, S(5.9f), Color{230, 234, 240, 255});
        ellipse(g, p + Vec2{0, S(-1)}, S(7.5f), S(2.6f), Color{200, 50, 50, 255});
        break;
    case 3:
        line(head + Vec2{S(-6), S(0.5f)}, head + Vec2{S(6), S(0.5f)}, S(1.6f), Color{20, 20, 24, 255});
        disc(g, head + Vec2{0, S(5)}, S(2.6f), Color{40, 40, 44, 255});
        break;
    default:
        break;
    }

    auto gun = [&](float w, Color col) {
        Rectangle r{hand.x - S(w * 0.5f), hand.y - S(len), S(w), S(len + 3)};
        DrawRectangleRounded({r.x - S(0.8f), r.y - S(0.8f), r.width + S(1.6f), r.height + S(1.6f)}, 0.3f, 4,
                             Color{15, 15, 20, 255});
        DrawRectangleRounded(r, 0.3f, 4, col);
    };
    switch (u) {
    case UnitType::Rifleman:
        gun(3.6f, dark);
        DrawRectangleV({hand.x - S(2.4f), hand.y - S(7)}, {S(4.8f), S(5)}, metal);
        break;
    case UnitType::Gunner:
        DrawRectangleRounded({hand.x - S(4.8f), hand.y - S(9), S(9.6f), S(11)}, 0.3f, 4, metal);
        for (int k = -1; k <= 1; ++k) {
            DrawRectangleV({hand.x + S(static_cast<float>(k) * 2.2f) - S(0.9f), hand.y - S(len)}, {S(1.8f), S(len - 7)},
                           dark);
        }
        DrawRectangleRounded({hand.x - S(10), hand.y - S(3), S(6), S(6)}, 0.3f, 4, Color{120, 104, 52, 255});
        break;
    case UnitType::Shotgunner:
        gun(4.8f, dark);
        DrawRectangleRounded({hand.x - S(3.4f), hand.y - S(len * 0.6f), S(6.8f), S(4)}, 0.3f, 4,
                             Color{140, 92, 52, 255});
        break;
    case UnitType::Sniper:
        gun(2.6f, dark);
        DrawRectangleRounded({hand.x - S(2.4f), hand.y - S(13), S(4.8f), S(7)}, 0.4f, 4, metal);
        disc(g, {hand.x, hand.y - S(10)}, S(1.4f), Color{120, 200, 255, 255});
        break;
    case UnitType::Grenadier:
        gun(7.0f, Color{70, 74, 56, 255});
        disc(g, {hand.x, hand.y - S(len)}, S(3.4f), dark);
        for (int k = 0; k < 3; ++k)
            disc(g, p + Vec2{S(-6 + static_cast<float>(k) * 3.5f), S(4)}, S(1.8f), Color{90, 124, 60, 255});
        break;
    case UnitType::Flamer: {
        gun(4.6f, metal);
        float k = 0.7f + 0.3f * std::sin(time * 25.0f);
        glow(g, {hand.x, hand.y - S(len + 1)}, S(6) * k, Color{255, 160, 60, 220});
        break;
    }
    case UnitType::Count:
        break;
    }
    disc(g, hand, S(2.2f), skin);
    disc(g, support, S(2.2f), skin);

    if (recoil > 0.6f && def.shot == Shot::Bullet) {
        Vec2 tip = p + muzzle_offset(static_cast<int>(u), scale) + Vec2{0, kick};
        glow(g, tip, S(9), Color{255, 220, 120, static_cast<unsigned char>(recoil * 220)});
    }
}

// Small world details living on the battlements: snow piling up over the run, rain
// splashes, sand drifting against the wall, grass along its foot.
void draw_wall_weather(const Run& run) {
    const Theme& th = run.theme;
    float t = run.time;
    if (th.weather == Weather::Snow) {
        float acc = std::min(1.0f, 0.25f + t / 120.0f);
        for (float x = 4; x < VIRTUAL_W; x += 52.0f) {
            float h = 3.0f + 7.0f * acc;
            DrawRectangleRounded({x - 2, WALL_TOP - 16.0f - h * 0.6f, 38.0f, h}, 0.8f, 6, Color{248, 251, 255, 255});
        }
        DrawRectangleRounded({0, WALL_TOP + 4, static_cast<float>(VIRTUAL_W), 3.0f + 5.0f * acc}, 0.8f, 6,
                             Color{240, 246, 255, 235});
        for (float x = 10; x < VIRTUAL_W; x += 46.0f) {
            float w = 30.0f + std::fmod(x * 7.0f, 20.0f);
            DrawEllipse(static_cast<int>(x), static_cast<int>(WALL_TOP - 14), w * (0.6f + 0.4f * acc),
                        7.0f * acc + 2.0f, Color{244, 248, 255, 230});
        }
    } else if (th.weather == Weather::Rain) {
        for (int i = 0; i < 14; ++i) {
            float fi = static_cast<float>(i);
            float phase = std::fmod(t * 1.7f + fi * 0.37f, 1.0f);
            float x = std::fmod(fi * 173.0f + std::floor(t * 1.7f + fi * 0.37f) * 97.0f, VIRTUAL_W);
            float y = i % 2 ? WALL_TOP - 2.0f : WALL_TOP - 40.0f - std::fmod(fi * 53.0f, 300.0f);
            DrawEllipseLines(static_cast<int>(x), static_cast<int>(y), 3.0f + phase * 12.0f, 1.5f + phase * 4.0f,
                             Color{210, 225, 255, static_cast<unsigned char>(180 * (1.0f - phase))});
        }
        DrawRectangle(0, static_cast<int>(WALL_TOP), VIRTUAL_W, static_cast<int>(WALL_BOTTOM - WALL_TOP),
                      Color{20, 30, 60, 40});
    } else if (th.weather == Weather::Dust || th.biome == 1) {
        Color sand = th.light;
        for (float x = -20; x < VIRTUAL_W + 20; x += 64.0f) {
            float w = 46.0f + std::fmod(x * 13.0f, 30.0f);
            DrawEllipse(static_cast<int>(x), static_cast<int>(WALL_TOP - 8), w, 14.0f, darken(sand, 0.12f));
            DrawEllipse(static_cast<int>(x), static_cast<int>(WALL_TOP - 10), w * 0.85f, 10.0f, sand);
        }
    } else if (th.biome == 0) {
        for (float x = 6; x < VIRTUAL_W; x += 11.0f) {
            float h = 8.0f + std::fmod(x * 17.0f, 9.0f);
            float sway = std::sin(t * 2.2f + x * 0.08f) * 3.0f;
            Color blade = std::fmod(x, 3.0f) < 1.0f ? th.dark : darken(th.dark, 0.2f);
            DrawLineEx({x, WALL_TOP - 14}, {x + sway, WALL_TOP - 14 - h}, 2.5f, blade);
            DrawLineEx({x + 4, WALL_TOP - 14}, {x + 6 + sway, WALL_TOP - 12 - h * 0.7f}, 2.0f, th.light);
        }
    } else if (th.biome == 3) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (float x = 30; x < VIRTUAL_W; x += 140.0f) {
            float k = 0.6f + 0.4f * std::sin(t * 2.0f + x);
            DrawEllipse(static_cast<int>(x), static_cast<int>(WALL_TOP - 12), 36, 8,
                        Color{255, 90, 30, static_cast<unsigned char>(70 * k)});
        }
        EndBlendMode();
    }
}

void draw_courtyard(const Theme& b) {
    DrawRectangle(0, static_cast<int>(WALL_BOTTOM), VIRTUAL_W, static_cast<int>(VIRTUAL_H - WALL_BOTTOM), b.court);
    for (int y = static_cast<int>(WALL_BOTTOM) + 18; y < VIRTUAL_H; y += 36) {
        DrawLine(0, y, VIRTUAL_W, y, darken(b.court, 0.15f));
        for (int x = ((y / 36) % 2) * 36; x < VIRTUAL_W; x += 72) DrawLine(x, y, x, y + 36, darken(b.court, 0.15f));
    }
}

// The wall breaks up brick by brick: each block falls after a delay that ripples out from
// the centre, tumbles, and leaves a growing heap of rubble.
void draw_wall_collapse(const Run& run, const Gfx& g) {
    const Theme& th = run.theme;
    float t = run.collapse_t;
    Color stone = mix(th.stone, Color{110, 70, 60, 255}, 0.35f);
    Rng rubble{777};
    float pile = std::min(1.0f, t / 1.2f);
    for (int i = 0; i < 40; ++i) {
        float x = rubble.uniform(-20, VIRTUAL_W + 20);
        float r = rubble.uniform(14, 30) * pile;
        if (r < 1.0f) continue;
        float y = WALL_BOTTOM - 8.0f + rubble.uniform(-10, 18);
        DrawPoly({x + 2, y + 4}, 6, r, rubble.uniform(0, 60), Color{0, 0, 0, 70});
        DrawPoly({x, y}, 6, r, rubble.uniform(0, 60), darken(stone, rubble.uniform(0.1f, 0.45f)));
    }
    constexpr float BW = 60.0f;
    constexpr float BH = 17.0f;
    for (int row = -1; row < 4; ++row) {
        float y0 = row < 0 ? WALL_TOP - 16.0f : WALL_TOP + 8.0f + static_cast<float>(row) * BH;
        float w = row < 0 ? 34.0f : BW;
        float h = row < 0 ? 22.0f : BH;
        float step = row < 0 ? 52.0f : BW;
        float start = row < 0 ? 4.0f : ((row % 2) ? -10.0f : 20.0f);
        for (float x = start; x < VIRTUAL_W; x += step) {
            Rng r{static_cast<uint64_t>((row + 2) * 1000 + static_cast<int>(x))};
            float centre = std::abs(x + w * 0.5f - VIRTUAL_W * 0.5f) / (VIRTUAL_W * 0.5f);
            float delay = centre * 0.5f + r.uniform(0.0f, 0.35f) + static_cast<float>(3 - row) * 0.05f;
            float ft = std::max(0.0f, t - delay);
            float dy = 0.5f * 1800.0f * ft * ft;
            float dx = r.uniform(-60, 60) * ft;
            float rot = r.uniform(-260, 260) * ft;
            float landed = WALL_BOTTOM + r.uniform(0, 70);
            Vec2 c{x + w * 0.5f + dx, y0 + h * 0.5f + dy};
            if (c.y > landed) c.y = landed;
            float fade = std::clamp(1.0f - (t - delay - 0.8f), 0.0f, 1.0f);
            if (fade <= 0.0f) continue;
            Color fill = with_alpha(row < 0 ? brighten(stone, 0.15f) : stone, fade);
            DrawRectanglePro({c.x, c.y, w, h}, {w * 0.5f, h * 0.5f}, rot, with_alpha(darken(stone, 0.45f), fade));
            DrawRectanglePro({c.x, c.y, w - 4, h - 4}, {(w - 4) * 0.5f, (h - 4) * 0.5f}, rot, fill);
        }
    }
    (void)g;
}

void draw_wall(const Run& run) {
    const Theme& b = run.theme;
    draw_courtyard(b);
    if (run.collapse_t >= 0.0f) return;

    float jolt = run.wall_jolt;
    Vec2 shake{std::sin(run.time * 70.0f) * jolt * 3.0f, std::cos(run.time * 53.0f) * jolt * 2.0f};
    rlPushMatrix();
    rlTranslatef(shake.x, shake.y, 0);

    Color stone = b.stone;
    float ratio = std::clamp(run.wall_hp / run.stats.wall_max, 0.0f, 1.0f);
    stone = mix(stone, Color{110, 70, 60, 255}, (1.0f - ratio) * 0.35f);
    DrawRectangleGradientV(0, static_cast<int>(WALL_TOP), VIRTUAL_W, static_cast<int>(WALL_BOTTOM - WALL_TOP),
                           brighten(stone, 0.1f), darken(stone, 0.25f));
    for (int row = 0; row < 4; ++row) {
        float y = WALL_TOP + 8.0f + static_cast<float>(row) * 17.0f;
        DrawLineEx({0, y}, {VIRTUAL_W, y}, 2.0f, darken(stone, 0.3f));
        for (float x = (row % 2) ? 20.0f : 50.0f; x < VIRTUAL_W; x += 60.0f) {
            DrawLineEx({x, y}, {x, y + 17}, 2.0f, darken(stone, 0.3f));
        }
    }
    // Battlements break away one by one as the wall takes damage.
    int index = 0;
    for (float x = 4; x < VIRTUAL_W; x += 52.0f, ++index) {
        float threshold = std::fmod(static_cast<float>(index) * 0.618f, 1.0f) * 0.85f;
        if (1.0f - ratio > threshold + 0.1f) {
            DrawRectangleRec({x + 4, WALL_TOP - 6.0f, 26.0f, 12.0f}, darken(stone, 0.3f));
            DrawPoly({x + 10, WALL_TOP - 4}, 5, 5, 20, darken(stone, 0.15f));
            continue;
        }
        Rectangle m{x, WALL_TOP - 16.0f, 34.0f, 22.0f};
        DrawRectangleRec(m, brighten(stone, 0.15f));
        DrawRectangleLinesEx(m, 2.0f, darken(stone, 0.35f));
    }
    DrawRectangle(0, static_cast<int>(WALL_BOTTOM) - 6, VIRTUAL_W, 6, darken(stone, 0.45f));

    Rng crack{4242};
    int cracks = static_cast<int>((1.0f - ratio) * 18.0f);
    for (int k = 0; k < cracks; ++k) {
        Vec2 p{crack.uniform(20, VIRTUAL_W - 20), crack.uniform(WALL_TOP + 6, WALL_BOTTOM - 10)};
        for (int s = 0; s < 4; ++s) {
            Vec2 q = p + Vec2{crack.uniform(-18, 18), crack.uniform(4, 14)};
            line(p, q, 2.5f, Color{40, 30, 30, 200});
            p = q;
        }
    }
    // Below half health the wall burns; more fires the closer it is to falling.
    int fires = ratio < 0.5f ? 1 + static_cast<int>((0.5f - ratio) * 12.0f) : 0;
    Rng fire_rng{99};
    BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < fires; ++k) {
        Vec2 p{fire_rng.uniform(30, VIRTUAL_W - 30), WALL_TOP + fire_rng.uniform(2, 30)};
        float flick = 0.7f + 0.3f * std::sin(run.time * (11.0f + static_cast<float>(k)) + static_cast<float>(k));
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - 10 * flick), 14 * flick, 22 * flick,
                    Color{255, 90, 20, 150});
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - 6 * flick), 8 * flick, 14 * flick,
                    Color{255, 200, 80, 200});
    }
    EndBlendMode();
    draw_wall_weather(run);
    if (run.wall_flash > 0.0f) {
        DrawRectangle(0, static_cast<int>(WALL_TOP) - 16, VIRTUAL_W, static_cast<int>(WALL_BOTTOM - WALL_TOP) + 16,
                      Color{255, 80, 60, static_cast<unsigned char>(run.wall_flash / 0.12f * 70)});
    }
    rlPopMatrix();
}

void draw_turret(const Gfx& g, TurretType type, int level, Vec2 p, float aim, float recoil, float time, float charge) {
    Color stone{128, 124, 118, 255};
    Color steel{92, 96, 108, 255};
    Color black{22, 22, 28, 255};
    DrawPoly((p + Vec2{0, 5}).to_raylib(), 8, 33, 22.5f, Color{0, 0, 0, 90});
    DrawPoly(p.to_raylib(), 8, 33, 22.5f, darken(stone, 0.5f));
    DrawPoly(p.to_raylib(), 8, 30, 22.5f, stone);
    DrawPoly(p.to_raylib(), 8, 23, 22.5f, darken(stone, 0.2f));
    for (int k = 0; k < 8; ++k) {
        float a = static_cast<float>(k) * PI / 4.0f + PI / 8.0f;
        disc(g, p + Vec2{std::cos(a), std::sin(a)} * 26.5f, 2.2f, darken(stone, 0.55f));
    }
    Vec2 d{std::cos(aim), std::sin(aim)};
    Vec2 n{-d.y, d.x};
    Vec2 c = p - d * (recoil * 6.0f);
    auto barrel = [&](Vec2 from, float len, float w, Color col) {
        Vec2 to = from + d * len;
        line(from, to, w + 3.0f, black);
        line(from, to, w, col);
        line(from + n * (w * 0.22f), to + n * (w * 0.22f), std::max(1.0f, w * 0.22f), Color{255, 255, 255, 70});
    };
    const auto& def = turret_def(type);
    switch (type) {
    case TurretType::Cannon: {
        for (int k = -1; k <= 1; k += 2) {
            Vec2 from = c + n * (static_cast<float>(k) * 6.0f);
            barrel(from, TURRET_BARREL, 7.0f, steel);
            Vec2 tip = from + d * (TURRET_BARREL - 3.0f);
            line(tip, tip + d * 5.0f, 12.0f, black);
            line(tip, tip + d * 5.0f, 9.0f, darken(steel, 0.2f));
        }
        disc(g, c, 20, black);
        disc(g, c, 18, darken(def.color, 0.15f));
        disc(g, c + d * 3.0f, 12, def.color);
        disc(g, c - d * 8.0f, 6, darken(def.color, 0.45f));
        disc(g, c + Vec2{-5, -5}, 4.5f, Color{255, 255, 255, 70});
        break;
    }
    case TurretType::Tesla: {
        Color copper{208, 128, 64, 255};
        disc(g, p, 18, black);
        disc(g, p, 16, steel);
        for (int r = 0; r < 4; ++r) {
            Vec2 q = p + Vec2{0, -static_cast<float>(r) * 4.5f};
            float rr = 13.0f - static_cast<float>(r) * 2.2f;
            disc(g, q, rr + 1.5f, darken(copper, 0.5f));
            disc(g, q, rr, r % 2 ? copper : brighten(copper, 0.15f));
        }
        Vec2 orb = p + TESLA_ORB;
        float k = 0.6f + 0.4f * std::sin(time * 14.0f);
        glow(g, orb, 26.0f * k + 10.0f, Color{140, 210, 255, 210});
        disc(g, orb, 7.5f, Color{170, 225, 255, 255});
        disc(g, orb, 4.5f, WHITE);
        Rng arc{static_cast<uint64_t>(time * 20.0f) + 7};
        for (int a = 0; a < 3; ++a) {
            float ang = arc.uniform(0, 2 * PI);
            Vec2 mid = orb + Vec2{std::cos(ang), std::sin(ang)} * 11.0f + Vec2{arc.uniform(-3, 3), arc.uniform(-3, 3)};
            Vec2 end = orb + Vec2{std::cos(ang), std::sin(ang)} * 21.0f;
            line(orb, mid, 2.0f, Color{210, 240, 255, 230});
            line(mid, end, 1.5f, Color{210, 240, 255, 180});
        }
        break;
    }
    case TurretType::Flamer: {
        Vec2 tank = c - d * 14.0f;
        line(tank - n * 9.0f, tank + n * 9.0f, 14.0f, black);
        line(tank - n * 8.0f, tank + n * 8.0f, 11.0f, def.color);
        line(tank - n * 8.0f + d * 2.0f, tank + n * 8.0f + d * 2.0f, 3.0f, Color{255, 220, 170, 120});
        barrel(c, TURRET_BARREL - 6.0f, 10.0f, steel);
        Vec2 tip = c + d * (TURRET_BARREL - 6.0f);
        line(tip, tip + d * 6.0f, 15.0f, black);
        line(tip, tip + d * 6.0f, 12.0f, darken(steel, 0.25f));
        float k = 0.7f + 0.3f * std::sin(time * 30.0f);
        glow(g, c + d * TURRET_BARREL, 10.0f * k, Color{255, 150, 50, 230});
        disc(g, c, 15, black);
        disc(g, c, 13, Color{100, 96, 90, 255});
        disc(g, c + Vec2{-4, -4}, 3.5f, Color{255, 255, 255, 60});
        break;
    }
    case TurretType::Laser: {
        for (int k = -1; k <= 1; k += 2)
            barrel(c + n * (static_cast<float>(k) * 7.0f), TURRET_BARREL + 2.0f, 4.5f, steel);
        float glow_k = std::clamp(charge, 0.0f, 1.0f);
        line(c, c + d * TURRET_BARREL, 3.5f, with_alpha(def.color, 0.35f + 0.65f * glow_k));
        glow(g, c + d * TURRET_BARREL, 6.0f + 12.0f * glow_k, with_alpha(def.color, glow_k));
        DrawPoly(c.to_raylib(), 6, 17, aim * RAD2DEG, black);
        DrawPoly(c.to_raylib(), 6, 15, aim * RAD2DEG, darken(def.color, 0.3f));
        disc(g, c, 9, def.color);
        disc(g, c, 4.5f, Color{255, 235, 245, 255});
        break;
    }
    case TurretType::None:
    case TurretType::Count:
        break;
    }
    for (int l = 0; l < level; ++l) {
        float x = p.x - static_cast<float>(level - 1) * 7.5f + static_cast<float>(l) * 15.0f;
        star_shape(g, {x, p.y + 40}, 7.5f, Color{30, 22, 8, 255});
        star_shape(g, {x, p.y + 40}, 5.5f, Color{255, 205, 60, 255});
    }
}

void draw_turrets(Game& game, const Run& run) {
    const auto& g = game.gfx;
    for (int s = 0; s < TURRET_SLOTS; ++s) {
        const auto& t = run.turrets[static_cast<size_t>(s)];
        Vec2 p{TURRET_SLOT_X[s], TURRET_Y};
        bool selected = run.build_slot == s;
        if (t.type == TurretType::None) {
            int cheapest = TURRET_DEFS[0].base_cost;
            bool afford = run.coins >= cheapest;
            float pulse = afford ? 0.5f + 0.5f * std::sin(game.time * 5.0f) : 0.0f;
            Color c = afford ? Color{255, 220, 90, 255} : Color{210, 210, 210, 150};
            if (afford) glow(g, p, 60, Color{255, 220, 90, static_cast<unsigned char>(40 + pulse * 50)});
            DrawPoly(p.to_raylib(), 8, 29, 22.5f, Color{20, 20, 30, 150});
            ring(g, p, 31 + pulse * 3, c);
            line(p - Vec2{11, 0}, p + Vec2{11, 0}, 5, c);
            line(p - Vec2{0, 11}, p + Vec2{0, 11}, 5, c);
        } else {
            float charge = 1.0f - std::clamp(t.cooldown / turret_interval(t.type, t.level), 0.0f, 1.0f);
            draw_turret(g, t.type, t.level, p, t.aim, t.recoil, game.time + static_cast<float>(s), charge);
        }
        if (selected) ring(g, p, 44 + std::sin(game.time * 8) * 2, Color{255, 255, 255, 230});
    }
}

void draw_op(Vec2 c, char op, float size, Color col) {
    float t = size * 0.2f;
    float h = size * 0.38f;
    Color edge{20, 16, 30, col.a};
    std::vector<std::pair<Vec2, Vec2>> bars;
    std::vector<Vec2> dots;
    switch (op) {
    case '+':
        bars = {{c - Vec2{h, 0}, c + Vec2{h, 0}}, {c - Vec2{0, h}, c + Vec2{0, h}}};
        break;
    case '-':
        bars = {{c - Vec2{h, 0}, c + Vec2{h, 0}}};
        break;
    case 'x':
        bars = {{c - Vec2{h * 0.8f, h * 0.8f}, c + Vec2{h * 0.8f, h * 0.8f}},
                {c - Vec2{h * 0.8f, -h * 0.8f}, c + Vec2{h * 0.8f, -h * 0.8f}}};
        break;
    case '/':
        bars = {{c - Vec2{h, 0}, c + Vec2{h, 0}}};
        dots = {c - Vec2{0, h * 0.75f}, c + Vec2{0, h * 0.75f}};
        break;
    default:
        break;
    }
    for (const auto& [a, b] : bars) line(a, b, t + 4.0f, edge);
    for (const auto& d : dots) DrawCircleV(d.to_raylib(), t * 0.75f + 2.0f, edge);
    for (const auto& [a, b] : bars) line(a, b, t, col);
    for (const auto& d : dots) DrawCircleV(d.to_raylib(), t * 0.75f, col);
}

void draw_gate(Game& game, const Gate& gate) {
    const auto& g = game.gfx;
    bool good = !gate.state.red;
    bool mul = gate.state.kind == GateKind::Mul;
    Color base = good ? (mul ? Color{110, 90, 255, 255} : Color{40, 150, 255, 255}) : Color{230, 56, 60, 255};
    bool negative = gate.state.value < 0 || (mul && gate.state.value == 1);
    float s = 1.0f + gate.pulse * 0.08f;
    Rectangle r{gate.x - gate.w * 0.5f * s, gate.y - GATE_H * 0.5f * s, gate.w * s, GATE_H * s};
    float rnd = 0.3f;

    if (good && !negative) {
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, {gate.x, gate.y}, gate.w * 0.55f, with_alpha(base, 0.35f));
        EndBlendMode();
    }
    DrawRectangleRounded({r.x, r.y + 6, r.width, r.height}, rnd, 6, Color{0, 0, 0, 70});
    DrawRectangleRounded({r.x - 4, r.y - 4, r.width + 8, r.height + 8}, rnd, 6, darken(base, 0.6f));
    DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                           static_cast<int>(r.height), with_alpha(brighten(base, 0.2f), 0.85f),
                           with_alpha(darken(base, 0.25f), 0.85f));
    DrawRectangleRoundedLinesEx(r, rnd, 6, 3.0f, brighten(base, 0.45f));
    if (!good) {
        BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                         static_cast<int>(r.height));
        for (float x = r.x - r.height; x < r.x + r.width; x += 26.0f) {
            line({x, r.y + r.height}, {x + r.height, r.y}, 9.0f, Color{0, 0, 0, 40});
        }
        EndScissorMode();
    } else {
        float sweep = std::fmod(game.time * 0.8f + gate.x * 0.003f, 1.6f) - 0.3f;
        float sx = r.x + sweep * r.width;
        BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                         static_cast<int>(r.height));
        line({sx, r.y + r.height}, {sx + r.height * 0.6f, r.y}, 14.0f, Color{255, 255, 255, 50});
        EndScissorMode();
    }
    DrawRectangleRounded({r.x + 6, r.y + 4, r.width - 12, r.height * 0.28f}, 0.5f, 6, Color{255, 255, 255, 55});
    if (gate.hit_flash > 0.0f) DrawRectangleRounded(r, rnd, 6, Color{255, 255, 255, 60});

    for (int side = -1; side <= 1; side += 2) {
        Vec2 at{side < 0 ? r.x + 22.0f : r.x + r.width - 22.0f, gate.y};
        float bounce = std::sin(game.time * 6.0f) * 3.0f * (good ? -1.0f : 1.0f);
        for (int k = 0; k < 2; ++k) {
            float y =
                at.y + (good ? 6.0f - static_cast<float>(k) * 10.0f : -6.0f + static_cast<float>(k) * 10.0f) + bounce;
            Vec2 tip{at.x, y + (good ? -6.0f : 6.0f)};
            line({at.x - 7, y}, tip, 3.5f, Color{255, 255, 255, 170});
            line({at.x + 7, y}, tip, 3.5f, Color{255, 255, 255, 170});
        }
    }
    char op = '+';
    int magnitude = std::abs(gate.state.value);
    bool stat = gate.state.kind == GateKind::Rate || gate.state.kind == GateKind::Power;
    if (mul)
        op = gate.state.value > 0 ? 'x' : '/';
    else if (gate.state.value < 0)
        op = '-';
    std::string num = stat ? std::format("{}%", magnitude) : std::format("{}", magnitude);
    if (stat) {
        text(g, gate.state.kind == GateKind::Rate ? "FIRE RATE" : "DAMAGE", {gate.x, r.y + 11}, 15,
             Color{255, 255, 255, 230}, Align::Center, false);
    }
    float size = 44.0f * s;
    float tw = text_width(g, num, size);
    float gap = size * 0.72f;
    float total = tw + gap + size * 0.38f;
    Vec2 op_at{gate.x - total * 0.5f + size * 0.38f, gate.y + 1};
    draw_op(op_at, op, size, WHITE);
    text(g, num, {op_at.x + gap + tw * 0.5f, gate.y + 2}, size, WHITE);
    if (mul) {
        float k = static_cast<float>(gate.state.points) / static_cast<float>(MUL_STEP_POINTS);
        Color fill = gate.state.red ? Color{255, 90, 80, 255} : Color{120, 255, 150, 255};
        bar({r.x + 46, r.y + r.height - 13, r.width - 92, 8}, k, fill, Color{0, 0, 0, 120});
    }
}

// Render textures accumulate alpha below 1 wherever translucent shapes were drawn into them,
// so the ground is copied with blending off; otherwise whatever is beneath shows through.
void draw_opaque(const Texture2D& t, Vec2 at) {
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    Rectangle src{0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)};
    DrawTexturePro(t, src, {at.x, at.y, static_cast<float>(t.width), static_cast<float>(t.height)}, {0, 0}, 0, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
}

bool ground_ready(int key) {
    return find_ground(key) != nullptr;
}

void draw_loot_content(Game& game, const Loot& f, Vec2 c) {
    const auto& g = game.gfx;
    switch (f.kind) {
    case LootKind::Soldiers:
        draw_soldier(g, c + Vec2{-12, 8}, 0, f.unit, 1.5f, game.time);
        draw_soldier(g, c + Vec2{12, 8}, 0, f.unit, 1.5f, game.time);
        draw_soldier(g, c + Vec2{0, -6}, 0, f.unit, 1.5f, game.time);
        break;
    case LootKind::Turret:
        draw_turret(g, f.turret, 0, c, -PI / 2, 0, game.time, 1.0f);
        break;
    case LootKind::Card:
        DrawRectangleRounded({c.x - 17, c.y - 23, 34, 46}, 0.2f, 4, Color{40, 30, 10, 255});
        DrawRectangleRounded({c.x - 15, c.y - 21, 30, 42}, 0.2f, 4, Color{230, 150, 40, 255});
        draw_op(c, '+', 26, WHITE);
        break;
    case LootKind::Coins:
        for (int k = 0; k < 5; ++k) {
            Vec2 q = c + Vec2{static_cast<float>(k % 3 - 1) * 14.0f, static_cast<float>(k / 3) * -12.0f + 6.0f};
            disc(g, q, 11, Color{150, 100, 10, 255});
            disc(g, q, 9, Color{255, 205, 50, 255});
        }
        break;
    case LootKind::Gems:
        for (int k = -1; k <= 1; ++k) {
            Vec2 q = c + Vec2{static_cast<float>(k) * 15.0f, k == 0 ? -6.0f : 6.0f};
            DrawPoly(q.to_raylib(), 4, 13, 0, Color{20, 60, 90, 255});
            DrawPoly(q.to_raylib(), 4, 11, 0, Color{110, 220, 255, 255});
        }
        break;
    }
}

void draw_loot(Game& game, const Loot& f) {
    const auto& g = game.gfx;
    float half = LOOT_SIZE * 0.5f;
    float bob = std::sin(game.time * 2.0f + f.seed) * 2.0f;
    Vec2 c = f.pos + Vec2{0, bob};
    Rectangle r{c.x - half, c.y - half, LOOT_SIZE, LOOT_SIZE};
    Color accent = lock_color(f.lock);
    float ratio = std::clamp(f.hp / f.max_hp, 0.0f, 1.0f);
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, c, LOOT_SIZE * 1.1f, with_alpha(brighten(accent, 0.3f), 0.25f + 0.12f * std::sin(game.time * 4.0f)));
    EndBlendMode();
    ellipse(g, f.pos + Vec2{0, half + 8}, half * 0.9f, 8, Color{0, 0, 0, 70});

    bool light_cracks = f.lock == LockKind::Ice || f.lock == LockKind::Crystal;
    switch (f.lock) {
    case LockKind::Ice:
        DrawRectangleRounded({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, 0.22f, 6, Color{40, 96, 130, 255});
        DrawRectangleRounded(r, 0.22f, 6, Color{150, 214, 245, 255});
        draw_loot_content(game, f, c);
        DrawRectangleRounded(r, 0.22f, 6, Color{185, 232, 255, 120});
        BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                         static_cast<int>(r.height));
        line({r.x, r.y + r.height * 0.55f}, {r.x + r.width * 0.55f, r.y}, 7.0f, Color{255, 255, 255, 70});
        line({r.x + r.width * 0.35f, r.y + r.height}, {r.x + r.width, r.y + r.height * 0.3f}, 4.0f,
             Color{255, 255, 255, 50});
        EndScissorMode();
        DrawRectangleRoundedLinesEx(r, 0.22f, 6, 2.5f, Color{230, 250, 255, 220});
        break;
    case LockKind::Crate: {
        Color wood{156, 104, 58, 255};
        DrawRectangleRec({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, Color{60, 36, 18, 255});
        DrawRectangleRec(r, wood);
        for (int k = 1; k < 4; ++k) {
            float y = r.y + r.height * static_cast<float>(k) / 4.0f;
            DrawLineEx({r.x, y}, {r.x + r.width, y}, 2.0f, darken(wood, 0.4f));
        }
        Rectangle win{c.x - 22, c.y - 22, 44, 44};
        DrawRectangleRec(win, Color{40, 26, 14, 255});
        draw_loot_content(game, f, c);
        line({r.x + 4, r.y + 4}, {win.x, win.y}, 6.0f, darken(wood, 0.2f));
        line({r.x + r.width - 4, r.y + r.height - 4}, {win.x + win.width, win.y + win.height}, 6.0f,
             darken(wood, 0.2f));
        DrawRectangleLinesEx(win, 3.0f, darken(wood, 0.5f));
        for (Vec2 k : {Vec2{r.x, r.y}, Vec2{r.x + r.width - 12, r.y}, Vec2{r.x, r.y + r.height - 12},
                       Vec2{r.x + r.width - 12, r.y + r.height - 12}}) {
            DrawRectangleRec({k.x, k.y, 12, 12}, Color{110, 112, 120, 255});
            disc(g, k + Vec2{6, 6}, 2, Color{60, 60, 66, 255});
        }
        break;
    }
    case LockKind::Stone: {
        Color st{150, 144, 136, 255};
        DrawRectangleRec({r.x - 3, r.y - 3, r.width + 6, r.height + 6}, darken(st, 0.6f));
        DrawRectangleRec(r, st);
        for (int row = 0; row < 4; ++row) {
            float y = r.y + static_cast<float>(row) * r.height / 4.0f;
            DrawLineEx({r.x, y}, {r.x + r.width, y}, 2.0f, darken(st, 0.4f));
            for (float x = r.x + ((row % 2) ? 12.0f : 24.0f); x < r.x + r.width; x += 24.0f) {
                DrawLineEx({x, y}, {x, y + r.height / 4.0f}, 2.0f, darken(st, 0.4f));
            }
        }
        DrawRectangleRounded({c.x - 23, c.y - 20, 46, 44}, 0.5f, 6, Color{36, 32, 30, 255});
        draw_loot_content(game, f, c + Vec2{0, 2});
        DrawRectangleRoundedLinesEx({c.x - 23, c.y - 20, 46, 44}, 0.5f, 6, 3.0f, darken(st, 0.5f));
        break;
    }
    case LockKind::Cage: {
        DrawRectangleRec(r, Color{30, 30, 36, 200});
        draw_loot_content(game, f, c);
        Color iron{84, 86, 96, 255};
        for (int k = 0; k <= 5; ++k) {
            float x = r.x + 4 + static_cast<float>(k) * (r.width - 8) / 5.0f;
            line({x, r.y}, {x, r.y + r.height}, 5.0f, darken(iron, 0.5f));
            line({x, r.y}, {x, r.y + r.height}, 3.0f, iron);
            line({x - 0.8f, r.y}, {x - 0.8f, r.y + r.height}, 1.0f, Color{200, 200, 210, 120});
        }
        DrawRectangleRec({r.x - 3, r.y - 3, r.width + 6, 9}, iron);
        DrawRectangleRec({r.x - 3, r.y + r.height - 6, r.width + 6, 9}, iron);
        DrawRectangleRounded({c.x - 7, c.y + 10, 14, 14}, 0.3f, 4, Color{200, 160, 50, 255});
        break;
    }
    case LockKind::Crystal: {
        Color cr = accent;
        DrawPoly(c.to_raylib(), 6, half + 6, 0, darken(cr, 0.6f));
        DrawPoly(c.to_raylib(), 6, half + 3, 0, cr);
        draw_loot_content(game, f, c);
        DrawPoly(c.to_raylib(), 6, half + 3, 0, with_alpha(brighten(cr, 0.2f), 0.45f));
        tri(c + Vec2{-half * 0.5f, -half * 0.8f}, c + Vec2{0, -half * 0.95f}, c + Vec2{-half * 0.2f, -half * 0.2f},
            Color{255, 255, 255, 90});
        break;
    }
    }

    float crack_half = f.lock == LockKind::Crystal ? half * 0.8f : half;
    Rng crack{static_cast<uint64_t>(f.seed * 1000.0f) + 3};
    int cracks = static_cast<int>((1.0f - ratio) * 9.0f);
    Color cc = light_cracks ? Color{255, 255, 255, 210} : Color{30, 22, 18, 230};
    BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                     static_cast<int>(r.height));
    for (int k = 0; k < cracks; ++k) {
        Vec2 p = c + Vec2{crack.uniform(-crack_half, crack_half) * 0.6f, crack.uniform(-crack_half, crack_half) * 0.6f};
        for (int sgm = 0; sgm < 3; ++sgm) {
            Vec2 q = p + Vec2{crack.uniform(-16, 16), crack.uniform(-16, 16)};
            line(p, q, 2.2f, cc);
            p = q;
        }
    }
    EndScissorMode();
    if (f.hit_flash > 0.0f) DrawRectangleRounded(r, 0.22f, 6, Color{255, 255, 255, 80});
    bar({r.x + 4, r.y + r.height + 6, r.width - 8, 8}, ratio, brighten(accent, 0.2f), Color{0, 0, 0, 150});
    text_fit(g, loot_label(f), {c.x, r.y - 14}, 20, 170, brighten(accent, 0.5f));
}

void draw_particles(Game& game, const Run& run, bool additive) {
    for (const auto& p : run.particles) {
        bool add = p.additive || p.tex == PFire || p.tex == PGlow || p.tex == PStar || p.tex == PRing;
        if (add != additive) continue;
        float k = 1.0f - p.life / p.max_life;
        float size = p.size + (p.size_end - p.size) * k;
        float alpha =
            p.zgrav > 0.0f ? std::min(1.0f, p.life / (p.max_life * 0.3f)) : std::min(1.0f, p.life / p.max_life * 1.6f);
        const Texture2D* t = ptex(game, p.tex);
        if (p.zgrav > 0.0f) {
            ellipse(game.gfx, p.pos + Vec2{0, size * 0.4f}, size * 0.9f, size * 0.4f,
                    Color{0, 0, 0, static_cast<unsigned char>(60 * alpha)});
            Vec2 at = p.pos - Vec2{0, p.z};
            if (size > 4.0f) {
                ellipse(game.gfx, at, size + 2.0f, size * 0.85f + 2.0f, with_alpha(darken(p.color, 0.55f), alpha));
            }
            ellipse(game.gfx, at, size, size * 0.85f, with_alpha(p.color, alpha));
            if (size > 4.0f) {
                ellipse(game.gfx, at - Vec2{size * 0.3f, size * 0.3f}, size * 0.35f, size * 0.25f,
                        Color{255, 255, 255, static_cast<unsigned char>(70 * alpha)});
            }
            continue;
        }
        sprite(*t, p.pos, size * 2.0f, size * 2.0f, p.rot, with_alpha(p.color, alpha));
    }
}

void draw_ambient(Game& game, const Theme& th, float time) {
    const auto& g = game.gfx;
    switch (th.weather) {
    case Weather::Snow:
        for (int i = 0; i < 80; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 97.0f + std::sin(time * 0.7f + fi) * 30.0f + time * 15.0f, VIRTUAL_W);
            float y = std::fmod(fi * 173.0f + time * (40.0f + std::fmod(fi * 7.0f, 30.0f)), WALL_TOP);
            disc(g, {x, y}, 2.0f + std::fmod(fi, 3.0f), Color{255, 255, 255, 190});
        }
        break;
    case Weather::Embers:
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 45; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 131.0f + std::sin(time + fi) * 20.0f, VIRTUAL_W);
            float y = WALL_TOP - std::fmod(fi * 211.0f + time * (30.0f + std::fmod(fi * 5.0f, 25.0f)), WALL_TOP);
            glow(g, {x, y}, 6.0f, Color{255, 120, 40, 170});
        }
        EndBlendMode();
        break;
    case Weather::Leaves:
        for (int i = 0; i < 16; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 211.0f + time * 25.0f, VIRTUAL_W + 40) - 20;
            float y = std::fmod(fi * 337.0f + time * 35.0f, WALL_TOP);
            ellipse(g, {x, y}, 5, 3, Color{170, 210, 90, 170});
        }
        break;
    case Weather::Rain:
        for (int i = 0; i < 90; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 89.0f + time * 120.0f, VIRTUAL_W + 60) - 30;
            float y = std::fmod(fi * 151.0f + time * 900.0f, WALL_TOP);
            line({x, y}, {x - 6, y + 22}, 1.6f, Color{200, 220, 255, 110});
        }
        break;
    case Weather::Dust:
        DrawRectangle(0, 0, VIRTUAL_W, static_cast<int>(WALL_TOP), Color{220, 180, 120, 26});
        for (int i = 0; i < 50; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 67.0f + time * (90.0f + std::fmod(fi * 13.0f, 60.0f)), VIRTUAL_W + 40) - 20;
            float y = std::fmod(fi * 193.0f + std::sin(time + fi) * 12.0f, WALL_TOP);
            ellipse(g, {x, y}, 3.5f, 1.6f, Color{240, 210, 160, 150});
        }
        break;
    case Weather::Fireflies:
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 26; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 131.0f + std::sin(time * 0.6f + fi) * 40.0f, VIRTUAL_W);
            float y = std::fmod(fi * 233.0f + std::cos(time * 0.5f + fi * 2.0f) * 30.0f, WALL_TOP);
            float blink = 0.5f + 0.5f * std::sin(time * 3.0f + fi * 1.7f);
            glow(g, {x, y}, 8.0f, Color{200, 255, 120, static_cast<unsigned char>(200 * blink)});
        }
        EndBlendMode();
        break;
    case Weather::Clear:
        break;
    }
}

// Props scattered over the battleground, by biome. Each is drawn top-down-ish with a soft
// shadow so it sits on the ground the horde walks over.
void prop_shadow(const Gfx& g, Vec2 at, float rx) {
    ellipse(g, at + Vec2{rx * 0.15f, rx * 0.35f}, rx, rx * 0.4f, Color{0, 0, 0, 60});
}

void prop_flowers(const Gfx& g, Vec2 at, Rng& rng) {
    static const Color petals[] = {
        {240, 120, 160, 255}, {255, 220, 90, 255}, {200, 140, 255, 255}, {255, 255, 255, 255}};
    for (int i = 0; i < 7; ++i) {
        Vec2 q = at + Vec2{rng.uniform(-22, 22), rng.uniform(-12, 12)};
        Color c = petals[rng.range(0, 3)];
        for (int k = 0; k < 4; ++k) disc(g, q + Vec2{std::cos(k * 1.57f), std::sin(k * 1.57f)} * 2.6f, 2.4f, c);
        disc(g, q, 1.8f, Color{255, 200, 60, 255});
    }
}

void prop_mushrooms(const Gfx& g, Vec2 at, Rng& rng) {
    int n = rng.range(2, 4);
    for (int i = 0; i < n; ++i) {
        Vec2 q = at + Vec2{rng.uniform(-14, 14), rng.uniform(-6, 6)};
        float r = rng.uniform(5, 9);
        prop_shadow(g, q, r);
        DrawRectangleRounded({q.x - r * 0.25f, q.y - r * 0.2f, r * 0.5f, r * 0.9f}, 0.5f, 4, Color{240, 230, 210, 255});
        ellipse(g, q - Vec2{0, r * 0.3f}, r + 1.5f, r * 0.62f + 1.5f, Color{90, 20, 20, 255});
        ellipse(g, q - Vec2{0, r * 0.3f}, r, r * 0.62f, Color{210, 50, 50, 255});
        disc(g, q + Vec2{-r * 0.35f, -r * 0.45f}, r * 0.16f, WHITE);
        disc(g, q + Vec2{r * 0.3f, -r * 0.3f}, r * 0.12f, WHITE);
    }
}

void prop_log(const Gfx& g, Vec2 at, Rng& rng) {
    float len = rng.uniform(46, 70);
    float ang = rng.uniform(-0.5f, 0.5f);
    Vec2 d{std::cos(ang), std::sin(ang)};
    Vec2 a = at - d * len * 0.5f;
    Vec2 b = at + d * len * 0.5f;
    prop_shadow(g, at, len * 0.5f);
    line(a, b, 18, Color{60, 38, 22, 255});
    line(a, b, 14, Color{128, 84, 48, 255});
    line(a + Vec2{0, -3}, b + Vec2{0, -3}, 3, Color{160, 112, 70, 255});
    disc(g, b, 8.5f, Color{60, 38, 22, 255});
    disc(g, b, 7, Color{200, 160, 110, 255});
    ring(g, b, 4.5f, Color{150, 110, 70, 255});
    ellipse(g, at + d * 6.0f + Vec2{0, -4}, 7, 3, Color{80, 150, 60, 255});
}

void prop_gravestone(const Gfx& g, Vec2 at, Rng& rng) {
    float w = rng.uniform(16, 22);
    float h = w * 1.3f;
    float tilt = rng.uniform(-12, 12);
    prop_shadow(g, at, w * 0.8f);
    DrawRectanglePro({at.x, at.y, w + 4, h + 4}, {(w + 4) / 2, h + 2}, tilt, Color{50, 52, 58, 255});
    DrawRectanglePro({at.x, at.y, w, h}, {w / 2, h}, tilt, Color{146, 150, 160, 255});
    DrawCircleV({at.x + std::sin(tilt * DEG2RAD) * h, at.y - std::cos(tilt * DEG2RAD) * h}, w / 2,
                Color{146, 150, 160, 255});
    DrawRectanglePro({at.x, at.y, w * 0.12f, h * 0.55f}, {w * 0.06f, h * 0.8f}, tilt, Color{90, 94, 104, 255});
    DrawRectanglePro({at.x, at.y, w * 0.5f, w * 0.12f}, {w * 0.25f, h * 0.65f}, tilt, Color{90, 94, 104, 255});
    ellipse(g, at + Vec2{0, 3}, w * 0.7f, 4, Color{90, 70, 50, 200});
}

void prop_cactus(const Gfx& g, Vec2 at, Rng& rng) {
    float h = rng.uniform(34, 56);
    Color green{74, 140, 76, 255};
    Color dark{36, 80, 42, 255};
    prop_shadow(g, at, 14);
    auto stalk = [&](Vec2 base, Vec2 top, float w) {
        line(base, top, w + 4, dark);
        line(base, top, w, green);
        disc(g, top, w * 0.5f + 2, dark);
        disc(g, top, w * 0.5f, green);
        line(base + Vec2{-w * 0.2f, 0}, top + Vec2{-w * 0.2f, 0}, 2, brighten(green, 0.25f));
    };
    stalk(at, at - Vec2{0, h}, 13);
    float arm = rng.uniform(0.35f, 0.6f);
    stalk(at - Vec2{0, h * arm}, at - Vec2{16, h * arm}, 8);
    stalk(at - Vec2{16, h * arm}, at - Vec2{16, h * (arm + 0.3f)}, 8);
    if (rng.chance(0.6f)) {
        stalk(at - Vec2{0, h * (arm + 0.15f)}, at - Vec2{-14, h * (arm + 0.15f)}, 7);
        stalk(at - Vec2{-14, h * (arm + 0.15f)}, at - Vec2{-14, h * (arm + 0.38f)}, 7);
    }
    if (rng.chance(0.5f)) disc(g, at - Vec2{0, h + 4}, 4, Color{255, 120, 160, 255});
}

void prop_beast_skull(const Gfx& g, Vec2 at, Rng&) {
    Color bone{236, 226, 200, 255};
    Color dark{90, 80, 66, 255};
    prop_shadow(g, at, 22);
    for (int side = -1; side <= 1; side += 2) {
        float sx = static_cast<float>(side);
        for (int i = 0; i <= 8; ++i) {
            float t = static_cast<float>(i) / 8.0f;
            Vec2 q = at + Vec2{sx * (10 + 26 * t), -6 - 14 * t * t};
            disc(g, q, 5.0f * (1.0f - 0.7f * t) + 1.5f, dark);
        }
        for (int i = 0; i <= 8; ++i) {
            float t = static_cast<float>(i) / 8.0f;
            disc(g, at + Vec2{sx * (10 + 26 * t), -6 - 14 * t * t}, 5.0f * (1.0f - 0.7f * t), bone);
        }
    }
    ellipse(g, at, 15, 12, dark);
    ellipse(g, at, 13.5f, 10.5f, bone);
    DrawRectangleRounded({at.x - 7, at.y + 2, 14, 16}, 0.6f, 4, bone);
    disc(g, at + Vec2{-5, -1}, 3.5f, Color{40, 30, 26, 255});
    disc(g, at + Vec2{5, -1}, 3.5f, Color{40, 30, 26, 255});
    disc(g, at + Vec2{-2, 13}, 1.5f, Color{40, 30, 26, 255});
    disc(g, at + Vec2{2, 13}, 1.5f, Color{40, 30, 26, 255});
}

void prop_pine(const Gfx& g, Vec2 at, Rng& rng, bool snowy) {
    float s = rng.uniform(0.8f, 1.2f);
    Color dark{30, 70, 52, 255};
    Color mid{46, 104, 72, 255};
    ellipse(g, at + Vec2{12 * s, 10 * s}, 30 * s, 12 * s, Color{0, 0, 0, 60});
    for (int tier = 0; tier < 3; ++tier) {
        float r = (30.0f - static_cast<float>(tier) * 8.0f) * s;
        Vec2 c = at - Vec2{0, static_cast<float>(tier) * 6.0f * s};
        DrawPoly(c.to_raylib(), 7, r + 2, rng.uniform(0, 50), dark);
        DrawPoly(c.to_raylib(), 7, r, rng.uniform(0, 50), mid);
        if (snowy) {
            DrawPoly((c - Vec2{r * 0.18f, r * 0.2f}).to_raylib(), 7, r * 0.62f, rng.uniform(0, 50),
                     Color{240, 246, 255, 255});
        } else {
            DrawPoly((c - Vec2{r * 0.2f, r * 0.2f}).to_raylib(), 7, r * 0.45f, rng.uniform(0, 50),
                     brighten(mid, 0.15f));
        }
    }
}

void prop_frozen_pond(const Gfx& g, Vec2 at, Rng& rng) {
    float rx = rng.uniform(40, 64);
    ellipse(g, at, rx + 5, rx * 0.5f + 4, Color{200, 214, 230, 255});
    ellipse(g, at, rx, rx * 0.5f, Color{150, 200, 236, 255});
    ellipse(g, at - Vec2{rx * 0.2f, rx * 0.1f}, rx * 0.6f, rx * 0.22f, Color{196, 228, 250, 255});
    for (int k = 0; k < 4; ++k) {
        Vec2 p = at + Vec2{rng.uniform(-rx, rx) * 0.6f, rng.uniform(-rx, rx) * 0.2f};
        for (int sgm = 0; sgm < 3; ++sgm) {
            Vec2 q = p + Vec2{rng.uniform(-14, 14), rng.uniform(-6, 6)};
            line(p, q, 1.5f, Color{255, 255, 255, 200});
            p = q;
        }
    }
}

void prop_snowman(const Gfx& g, Vec2 at, Rng&) {
    prop_shadow(g, at, 16);
    disc(g, at, 15, Color{180, 196, 216, 255});
    disc(g, at, 13.5f, Color{248, 250, 255, 255});
    disc(g, at - Vec2{0, 18}, 10.5f, Color{180, 196, 216, 255});
    disc(g, at - Vec2{0, 18}, 9.5f, Color{248, 250, 255, 255});
    disc(g, at - Vec2{3, 20}, 1.6f, Color{30, 30, 30, 255});
    disc(g, at - Vec2{-3, 20}, 1.6f, Color{30, 30, 30, 255});
    tri(at - Vec2{0, 17}, at - Vec2{-1, 15.5f}, at - Vec2{-9, 16}, Color{255, 140, 40, 255});
    line(at - Vec2{10, 10}, at - Vec2{18, 4}, 2, Color{90, 60, 30, 255});
    line(at - Vec2{-10, 10}, at - Vec2{-18, 4}, 2, Color{90, 60, 30, 255});
    DrawRectangleRec({at.x - 8, at.y - 13, 16, 4}, Color{200, 40, 40, 255});
}

void prop_dead_tree(const Gfx& g, Vec2 at, Rng& rng) {
    Color bark{40, 32, 30, 255};
    prop_shadow(g, at, 24);
    std::function<void(Vec2, float, float, int)> branch = [&](Vec2 from, float ang, float len, int depth) {
        Vec2 to = from + Vec2{std::cos(ang), std::sin(ang)} * len;
        line(from, to, static_cast<float>(depth) * 2.0f + 1.0f, bark);
        if (depth <= 0) return;
        branch(to, ang - rng.uniform(0.3f, 0.7f), len * 0.68f, depth - 1);
        branch(to, ang + rng.uniform(0.3f, 0.7f), len * 0.68f, depth - 1);
    };
    disc(g, at, 7, bark);
    branch(at, -PI / 2 + rng.uniform(-0.3f, 0.3f), rng.uniform(22, 30), 3);
}

void prop_vent(const Gfx& g, Vec2 at, Rng&) {
    DrawPoly((at + Vec2{2, 4}).to_raylib(), 7, 18, 10, Color{0, 0, 0, 70});
    DrawPoly(at.to_raylib(), 7, 18, 10, Color{46, 36, 34, 255});
    DrawPoly(at.to_raylib(), 7, 12, 30, Color{70, 56, 50, 255});
    disc(g, at, 7, Color{30, 10, 6, 255});
}

void prop_ruin_pillar(const Gfx& g, Vec2 at, Rng& rng, Color stone) {
    float h = rng.uniform(26, 48);
    prop_shadow(g, at, 16);
    DrawRectangleRec({at.x - 11, at.y - h, 22, h}, darken(stone, 0.45f));
    DrawRectangleRec({at.x - 9, at.y - h, 18, h}, stone);
    DrawRectangleRec({at.x - 9, at.y - h, 6, h}, brighten(stone, 0.12f));
    for (float y = at.y - h + 8; y < at.y; y += 10) DrawLineEx({at.x - 9, y}, {at.x + 9, y}, 1.5f, darken(stone, 0.3f));
    ellipse(g, {at.x, at.y - h}, 13, 5, brighten(stone, 0.2f));
    for (int k = 0; k < 3; ++k) {
        Vec2 q = at + Vec2{rng.uniform(-24, 24), rng.uniform(0, 8)};
        DrawPoly(q.to_raylib(), 5, rng.uniform(3, 6), rng.uniform(0, 60), darken(stone, 0.2f));
    }
}

enum class Prop : uint8_t {
    Flowers,
    Mushrooms,
    Log,
    Grave,
    Pillar,
    Cactus,
    Skull,
    Pine,
    Pond,
    Snowman,
    DeadTree,
    Vent
};

struct FieldProp {
    Vec2 at;
    Prop kind;
};

std::vector<FieldProp> field_props(const Theme& th) {
    Rng rng{static_cast<uint64_t>(th.key * 7919 + 5)};
    std::vector<FieldProp> out;
    for (int i = 0; i < 10; ++i) {
        float x = (i % 2 == 0) ? rng.uniform(50, 230) : rng.uniform(490, 670);
        Vec2 at{x, rng.uniform(HUD_TOP + 60, WALL_TOP - 90)};
        int roll = rng.range(0, 99);
        Prop kind;
        switch (th.biome) {
        case 0:
            kind = roll < 30   ? Prop::Flowers
                   : roll < 50 ? Prop::Mushrooms
                   : roll < 70 ? Prop::Log
                   : roll < 85 ? Prop::Grave
                               : Prop::Pillar;
            break;
        case 1:
            kind = roll < 40 ? Prop::Cactus : roll < 65 ? Prop::Skull : Prop::Pillar;
            break;
        case 2:
            kind = roll < 45 ? Prop::Pine : roll < 70 ? Prop::Pond : roll < 82 ? Prop::Snowman : Prop::Pillar;
            break;
        default:
            kind = roll < 40 ? Prop::DeadTree : roll < 70 ? Prop::Vent : Prop::Skull;
            break;
        }
        out.push_back({at, kind});
    }
    return out;
}

void draw_field_prop(Game& game, const Theme& th, const FieldProp& fp, Rng& rng) {
    const auto& g = game.gfx;
    Color pillar_stone = th.biome == 1   ? Color{204, 176, 128, 255}
                         : th.biome == 2 ? Color{176, 190, 210, 255}
                                         : th.stone;
    switch (fp.kind) {
    case Prop::Flowers:
        prop_flowers(g, fp.at, rng);
        break;
    case Prop::Mushrooms:
        prop_mushrooms(g, fp.at, rng);
        break;
    case Prop::Log:
        prop_log(g, fp.at, rng);
        break;
    case Prop::Grave:
        prop_gravestone(g, fp.at, rng);
        break;
    case Prop::Pillar:
        prop_ruin_pillar(g, fp.at, rng, pillar_stone);
        break;
    case Prop::Cactus:
        prop_cactus(g, fp.at, rng);
        break;
    case Prop::Skull:
        prop_beast_skull(g, fp.at, rng);
        break;
    case Prop::Pine:
        prop_pine(g, fp.at, rng, th.biome == 2);
        break;
    case Prop::Pond:
        prop_frozen_pond(g, fp.at, rng);
        break;
    case Prop::Snowman:
        prop_snowman(g, fp.at, rng);
        break;
    case Prop::DeadTree:
        prop_dead_tree(g, fp.at, rng);
        break;
    case Prop::Vent:
        prop_vent(g, fp.at, rng);
        break;
    }
}

void draw_decor(Game& game, const Theme& th, Vec2 at, Rng& rng) {
    const auto& g = game.gfx;
    switch (th.decor) {
    case Decor::Trees:
        if (rng.chance(0.65f)) {
            if (auto* t = game.assets.get_texture(rng.chance(0.5f) ? "tree" : "bush")) {
                float sz = rng.uniform(80, 110);
                sprite(*t, at + Vec2{4, 6}, sz, sz, 0, Color{0, 0, 0, 50});
                sprite(*t, at, sz, sz, rng.uniform(0, 360), WHITE);
            }
            return;
        }
        break;
    case Decor::Bones: {
        Color bone{232, 222, 196, 255};
        if (rng.chance(0.4f)) {
            float r = rng.uniform(12, 18);
            ellipse(g, at + Vec2{2, 4}, r, r * 0.9f, Color{0, 0, 0, 60});
            disc(g, at, r + 2, darken(bone, 0.5f));
            disc(g, at, r, bone);
            DrawRectangleRounded({at.x - r * 0.6f, at.y + r * 0.4f, r * 1.2f, r * 0.7f}, 0.4f, 4, bone);
            disc(g, at + Vec2{-r * 0.38f, -r * 0.05f}, r * 0.28f, Color{40, 30, 30, 255});
            disc(g, at + Vec2{r * 0.38f, -r * 0.05f}, r * 0.28f, Color{40, 30, 30, 255});
        } else {
            float len = rng.uniform(22, 40);
            float ang = rng.uniform(0, PI);
            Vec2 d{std::cos(ang), std::sin(ang)};
            Vec2 a2 = at - d * len * 0.5f;
            Vec2 b2 = at + d * len * 0.5f;
            line(a2, b2, 8, darken(bone, 0.5f));
            line(a2, b2, 6, bone);
            for (Vec2 e : {a2, b2}) {
                Vec2 n{-d.y, d.x};
                disc(g, e + n * 3.0f, 4.5f, bone);
                disc(g, e - n * 3.0f, 4.5f, bone);
            }
        }
        return;
    }
    case Decor::Crystals: {
        Color cc = th.biome == 3 ? Color{255, 90, 60, 255} : Color{140, 200, 255, 255};
        BeginBlendMode(BLEND_ADDITIVE);
        glow(g, at, 34, with_alpha(cc, 0.3f));
        EndBlendMode();
        int n = rng.range(2, 4);
        for (int k = 0; k < n; ++k) {
            float h = rng.uniform(22, 42);
            float w = rng.uniform(8, 13);
            float lean = rng.uniform(-0.5f, 0.5f);
            Vec2 base = at + Vec2{rng.uniform(-12, 12), rng.uniform(0, 8)};
            Vec2 tip = base + Vec2{lean * h, -h};
            tri(base + Vec2{-w, 0}, base + Vec2{w, 0}, tip, darken(cc, 0.45f));
            tri(base + Vec2{-w * 0.6f, -1}, base + Vec2{w * 0.2f, -1}, tip, cc);
            tri(base + Vec2{w * 0.2f, -1}, base + Vec2{w * 0.7f, -1}, tip, brighten(cc, 0.3f));
        }
        return;
    }
    case Decor::Ruins: {
        Color st = th.stone;
        float w = rng.uniform(22, 34);
        float h = rng.uniform(26, 50);
        Rectangle r{at.x - w / 2, at.y - h, w, h};
        DrawRectangleRec({r.x + 4, r.y + 6, r.width, r.height}, Color{0, 0, 0, 60});
        DrawRectangleRec(r, darken(st, 0.15f));
        DrawRectangleRec({r.x, r.y, r.width * 0.35f, r.height}, brighten(st, 0.1f));
        ellipse(g, {at.x, r.y}, w / 2, w / 4, brighten(st, 0.2f));
        DrawRectangleLinesEx(r, 2, darken(st, 0.5f));
        return;
    }
    case Decor::Rocks:
        break;
    }
    float r = rng.uniform(14, 30);
    float rot = rng.uniform(0, 60);
    int sides = rng.range(5, 7);
    Color base = th.biome == 1   ? Color{196, 160, 112, 255}
                 : th.biome == 2 ? Color{168, 182, 200, 255}
                 : th.biome == 3 ? Color{82, 72, 70, 255}
                                 : Color{132, 134, 128, 255};
    DrawPoly({at.x + 3, at.y + 5}, sides, r, rot, Color{0, 0, 0, 60});
    DrawPoly(at.to_raylib(), sides, r + 2, rot, darken(base, 0.45f));
    DrawPoly(at.to_raylib(), sides, r, rot, base);
    DrawPoly({at.x - r * 0.18f, at.y - r * 0.2f}, sides, r * 0.6f, rot, brighten(base, 0.18f));
}

void build_ground(Game& game, const Theme& th) {
    GroundSlot* slot = find_ground(th.key);
    if (!slot) {
        slot = &g_grounds[0];
        for (auto& s : g_grounds) {
            if (s.key == -1) {
                slot = &s;
                break;
            }
            if (s.use < slot->use) slot = &s;
        }
        if (slot->key == -1) {
            slot->tex = LoadRenderTexture(VIRTUAL_W, static_cast<int>(WALL_TOP) + 20);
            SetTextureFilter(slot->tex.texture, TEXTURE_FILTER_BILINEAR);
        }
        slot->key = th.key;
        slot->use = ++g_ground_clock;
    }
    const auto& g = game.gfx;
    Rng rng{static_cast<uint64_t>(th.key * 977 + 13)};
    BeginTextureMode(slot->tex);
    ClearBackground(th.ground);
    for (int i = 0; i < 120; ++i) {
        Vec2 p{rng.uniform(-60, VIRTUAL_W + 60), rng.uniform(-60, WALL_TOP + 60)};
        float r = rng.uniform(60, 170);
        Color c = rng.chance(0.5f) ? th.dark : th.light;
        glow(g, p, r, with_alpha(c, rng.uniform(0.35f, 0.7f)));
    }
    for (int i = 0; i < 26; ++i) {
        Vec2 p{rng.uniform(0, VIRTUAL_W), rng.uniform(HUD_TOP, WALL_TOP - 30)};
        float r = rng.uniform(16, 40);
        ellipse(g, p, r, r * 0.55f, with_alpha(darken(th.dark, 0.15f), 0.35f));
        ellipse(g, p - Vec2{0, r * 0.08f}, r * 0.8f, r * 0.42f, with_alpha(th.dark, 0.5f));
    }
    for (int i = 0; i < 500; ++i) {
        Vec2 p{rng.uniform(0, VIRTUAL_W), rng.uniform(0, WALL_TOP)};
        Color c = darken(th.dark, rng.uniform(0.0f, 0.25f));
        if (th.biome == 0) {
            line(p, p + Vec2{rng.uniform(-3, 3), -rng.uniform(5, 10)}, 2.0f, c);
            line(p, p + Vec2{rng.uniform(2, 6), -rng.uniform(4, 8)}, 2.0f, c);
        } else {
            disc(g, p, rng.uniform(1.5f, 3.5f), with_alpha(c, 0.7f));
        }
    }
    if (th.weather == Weather::Rain) {
        for (int i = 0; i < 10; ++i) {
            Vec2 p{rng.uniform(40, VIRTUAL_W - 40), rng.uniform(HUD_TOP + 60, WALL_TOP - 60)};
            float rx = rng.uniform(30, 70);
            ellipse(g, p, rx + 4, rx * 0.4f + 3, with_alpha(darken(th.dark, 0.3f), 0.6f));
            ellipse(g, p, rx, rx * 0.4f, Color{120, 140, 170, 190});
            ellipse(g, p - Vec2{rx * 0.3f, rx * 0.1f}, rx * 0.4f, rx * 0.1f, Color{220, 230, 255, 120});
        }
    }
    if (th.biome == 3) {
        for (int i = 0; i < 4; ++i) {
            Vec2 p{rng.uniform(60, VIRTUAL_W - 60), rng.uniform(HUD_TOP + 80, WALL_TOP - 120)};
            float rx = rng.uniform(34, 64);
            ellipse(g, p, rx + 8, rx * 0.5f + 6, Color{40, 20, 16, 255});
            ellipse(g, p, rx, rx * 0.5f, Color{230, 80, 20, 255});
            ellipse(g, p, rx * 0.6f, rx * 0.28f, Color{255, 170, 60, 255});
            glow(g, p, rx * 1.4f, Color{255, 120, 40, 90});
        }
        for (int i = 0; i < 7; ++i) {
            Vec2 p{rng.uniform(40, VIRTUAL_W - 40), rng.uniform(HUD_TOP + 40, WALL_TOP - 60)};
            for (int sgm = 0; sgm < 5; ++sgm) {
                Vec2 q = p + Vec2{rng.uniform(-40, 40), rng.uniform(-30, 30)};
                line(p, q, 6.0f, Color{60, 20, 10, 200});
                line(p, q, 2.5f, Color{255, 110, 40, 220});
                p = q;
            }
        }
    }
    for (int i = 0; i < 20; ++i) {
        bool left = i % 2 == 0;
        float x = left ? rng.uniform(-10, 34) : rng.uniform(VIRTUAL_W - 34, VIRTUAL_W + 10);
        float y = rng.uniform(HUD_TOP + 20, WALL_TOP - 70);
        draw_decor(game, th, {x, y}, rng);
    }
    Rng prop_rng{static_cast<uint64_t>(th.key * 31 + 9)};
    for (const auto& fp : field_props(th)) draw_field_prop(game, th, fp, prop_rng);
    EndTextureMode();
}

void flush_decals(Game& game) {
    if (!game.run || game.run->pending_decals.empty()) return;
    auto& run = *game.run;
    GroundSlot* slot = find_ground(run.theme.key);
    if (!slot) {
        build_ground(game, run.theme);
        slot = find_ground(run.theme.key);
    }
    BeginTextureMode(slot->tex);
    for (const auto& d : run.pending_decals) {
        if (d.kind == 1) {
            if (auto* t = game.assets.get_texture("p_scorch")) {
                sprite(*t, d.pos, d.radius * 2.6f, d.radius * 2.6f, d.rot, Color{20, 14, 10, 120});
            }
        } else {
            Rng r{static_cast<uint64_t>(d.pos.x * 31 + d.pos.y * 17)};
            Color edge = with_alpha(d.color, 0.55f);
            for (int k = 0; k < 7; ++k) {
                float a = r.uniform(0, 2 * PI);
                float dist = d.radius * r.uniform(0.35f, 0.9f);
                Vec2 q = d.pos + Vec2{std::cos(a) * dist, std::sin(a) * dist * 0.65f};
                ellipse(game.gfx, q, d.radius * r.uniform(0.25f, 0.45f), d.radius * r.uniform(0.18f, 0.32f), edge);
            }
            ellipse(game.gfx, d.pos, d.radius * 0.75f, d.radius * 0.5f, d.color);
            ellipse(game.gfx, d.pos, d.radius * 0.4f, d.radius * 0.26f, darken(d.color, 0.3f));
            for (int k = 0; k < 6; ++k) {
                float a = r.uniform(0, 2 * PI);
                float dist = d.radius * r.uniform(1.1f, 1.8f);
                disc(game.gfx, d.pos + Vec2{std::cos(a) * dist, std::sin(a) * dist * 0.65f}, r.uniform(1.5f, 3.5f),
                     d.color);
            }
        }
    }
    EndTextureMode();
    run.pending_decals.clear();
}

void unload_ground() {
    for (auto& s : g_grounds) {
        if (s.key != -1) UnloadRenderTexture(s.tex);
        s.key = -1;
    }
}

// Effects that move every frame on top of the baked ground.
void draw_live_env(Game& game, const Theme& th, float time) {
    const auto& g = game.gfx;
    if (th.night <= 0.0f && th.biome != 3) {
        for (int i = 0; i < 3; ++i) {
            float fi = static_cast<float>(i);
            float x = std::fmod(fi * 310.0f + time * (14.0f + fi * 4.0f), VIRTUAL_W + 600.0f) - 300.0f;
            float y = 220.0f + fi * 260.0f + std::sin(time * 0.1f + fi) * 40.0f;
            glow(g, {x, y}, 240.0f, Color{0, 0, 0, 34});
            glow(g, {x + 120.0f, y + 30.0f}, 180.0f, Color{0, 0, 0, 28});
        }
    }
    for (const auto& fp : field_props(th)) {
        if (fp.kind == Prop::Vent) {
            for (int k = 0; k < 5; ++k) {
                float ph = std::fmod(time * 0.45f + static_cast<float>(k) * 0.2f + fp.at.x * 0.001f, 1.0f);
                Vec2 q = fp.at + Vec2{std::sin(ph * 6.0f + static_cast<float>(k)) * 8.0f, -ph * 120.0f};
                disc(g, q, 8.0f + ph * 26.0f, Color{70, 64, 62, static_cast<unsigned char>(130 * (1.0f - ph))});
            }
            BeginBlendMode(BLEND_ADDITIVE);
            glow(g, fp.at, 22.0f + 4.0f * std::sin(time * 5.0f + fp.at.x), Color{255, 110, 40, 150});
            EndBlendMode();
        } else if (fp.kind == Prop::Pond) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int k = 0; k < 3; ++k) {
                float tw = std::max(0.0f, std::sin(time * 2.3f + static_cast<float>(k) * 2.1f + fp.at.y));
                Vec2 q = fp.at + Vec2{static_cast<float>(k - 1) * 22.0f, static_cast<float>(k % 2) * 6.0f - 4.0f};
                line(q - Vec2{6 * tw, 0}, q + Vec2{6 * tw, 0}, 1.5f,
                     Color{255, 255, 255, static_cast<unsigned char>(220 * tw)});
                line(q - Vec2{0, 6 * tw}, q + Vec2{0, 6 * tw}, 1.5f,
                     Color{255, 255, 255, static_cast<unsigned char>(220 * tw)});
            }
            EndBlendMode();
        } else if (fp.kind == Prop::Flowers && th.night <= 0.0f) {
            float a = time * 1.6f + fp.at.x;
            Vec2 q = fp.at + Vec2{std::cos(a) * 26.0f, std::sin(a * 1.3f) * 14.0f - 18.0f};
            float flap = std::abs(std::sin(time * 18.0f + fp.at.y));
            ellipse(g, q - Vec2{3 * flap, 0}, 3.0f * flap + 0.5f, 3.0f, Color{255, 170, 60, 230});
            ellipse(g, q + Vec2{3 * flap, 0}, 3.0f * flap + 0.5f, 3.0f, Color{255, 170, 60, 230});
        }
    }
    if (th.biome == 3) {
        Rng lava{static_cast<uint64_t>(th.key * 13 + 1)};
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 6; ++i) {
            Vec2 p{lava.uniform(40, VIRTUAL_W - 40), lava.uniform(HUD_TOP + 40, WALL_TOP - 60)};
            float k = 0.5f + 0.5f * std::sin(time * 1.8f + static_cast<float>(i) * 1.3f);
            glow(g, p, 50.0f + 20.0f * k, Color{255, 90, 30, static_cast<unsigned char>(30 + 50 * k)});
        }
        EndBlendMode();
    }
    if (th.biome == 1) {
        for (int i = 0; i < 2; ++i) {
            float fi = static_cast<float>(i);
            float speed = 70.0f + fi * 30.0f;
            float x = std::fmod(time * speed + fi * 400.0f, VIRTUAL_W + 200.0f) - 100.0f;
            float y = 300.0f + fi * 330.0f + std::abs(std::sin(time * 3.0f + fi)) * -18.0f;
            float rot = time * speed * 2.0f;
            ellipse(g, {x, 300.0f + fi * 330.0f + 16.0f}, 16, 5, Color{0, 0, 0, 50});
            disc(g, {x, y}, 15, Color{120, 90, 50, 200});
            for (int k = 0; k < 6; ++k) {
                float a = rot * DEG2RAD + static_cast<float>(k) * 1.05f;
                line({x, y}, Vec2{x, y} + Vec2{std::cos(a), std::sin(a)} * 14.0f, 2.0f, Color{80, 56, 30, 220});
            }
            ring(g, {x, y}, 13, Color{90, 64, 34, 220});
        }
        float dx = 360.0f + std::sin(time * 0.2f) * 240.0f;
        float dy = 520.0f + std::cos(time * 0.13f) * 160.0f;
        for (int k = 0; k < 26; ++k) {
            float fk = static_cast<float>(k);
            float a = time * 6.0f + fk * 0.9f;
            float rise = std::fmod(fk * 9.0f + time * 40.0f, 110.0f);
            float rad = 6.0f + rise * 0.35f;
            disc(g, {dx + std::cos(a) * rad, dy - rise + std::sin(a) * rad * 0.3f}, 2.5f,
                 Color{220, 190, 140, static_cast<unsigned char>(170 * (1.0f - rise / 110.0f))});
        }
    }
}

// Night battlegrounds are lit by torches on the wall and the muzzle glow of the squad.
void draw_night_lights(Game& game, const Run& run) {
    const Theme& th = run.theme;
    if (th.night <= 0.0f && th.biome != 3) return;
    const auto& g = game.gfx;
    float strength = th.night > 0.0f ? 1.0f : 0.5f;
    BeginBlendMode(BLEND_ADDITIVE);
    for (float x = 60.0f; x < VIRTUAL_W; x += 150.0f) {
        float flick = 0.85f + 0.15f * std::sin(run.time * 13.0f + x);
        glow(g, {x, WALL_TOP - 20}, 150.0f * flick, Color{255, 150, 60, static_cast<unsigned char>(70 * strength)});
        glow(g, {x, WALL_TOP - 26}, 16.0f * flick, Color{255, 220, 120, 230});
    }
    glow(g, {run.squad_x, SQUAD_Y}, 160.0f, Color{255, 200, 120, static_cast<unsigned char>(40 * strength)});
    EndBlendMode();
    if (run.collapse_t >= 0.0f) return;
    for (float x = 60.0f; x < VIRTUAL_W; x += 150.0f) {
        DrawRectangleRec({x - 3, WALL_TOP - 22, 6, 14}, Color{60, 40, 26, 255});
        DrawRectangleRec({x - 6, WALL_TOP - 24, 12, 4}, Color{40, 36, 34, 255});
        float flick = 0.8f + 0.2f * std::sin(run.time * 17.0f + x);
        ellipse(g, {x, WALL_TOP - 30}, 5 * flick, 9 * flick, Color{255, 140, 40, 255});
        ellipse(g, {x, WALL_TOP - 28}, 2.5f * flick, 5 * flick, Color{255, 240, 160, 255});
    }
}

void draw_vignette() {
    DrawRectangleGradientH(0, 0, 90, VIRTUAL_H, Color{0, 0, 0, 70}, Color{0, 0, 0, 0});
    DrawRectangleGradientH(VIRTUAL_W - 90, 0, 90, VIRTUAL_H, Color{0, 0, 0, 0}, Color{0, 0, 0, 70});
}

void draw_backdrop(Game& game, const Theme& th, float time) {
    ClearBackground(th.ground);
    if (GroundSlot* slot = find_ground(th.key)) draw_opaque(slot->tex.texture, {0.0f, 0.0f});
    draw_live_env(game, th, time);
    DrawRectangleGradientV(0, 0, VIRTUAL_W, 420, with_alpha(th.fog, 0.85f), with_alpha(th.fog, 0.0f));
    draw_ambient(game, th, time);
}

void draw_night(const Theme& th) {
    if (th.night <= 0.0f) return;
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{10, 14, 40, static_cast<unsigned char>(th.night * 255)});
}

void draw_enemy_preview(Game& game, int type, float x, float y, float scale, float anim) {
    EnemyType t = static_cast<EnemyType>(type);
    if (t == EnemyType::Boss) {
        static const BossGenome preview = make_boss(1, false);
        draw_boss(game.gfx, preview, -1, {x, y}, base_stats(t).radius * scale, anim, 0, false, game.time, 0);
        return;
    }
    draw_monster(game.gfx, t, {x, y}, base_stats(t).radius * scale, anim, 0.0f, false, false, game.time);
}

void draw_turret_preview(Game& game, int type, int level, float x, float y, float aim) {
    draw_turret(game.gfx, static_cast<TurretType>(type), level, {x, y}, aim, 0.0f, game.time, 1.0f);
}

void draw_soldier_preview(Game& game, float x, float y, float scale, int weapon) {
    draw_soldier(game.gfx, {x, y}, 0.0f, static_cast<UnitType>(weapon), scale, game.time);
}

void draw_corpses(Game& game, const Run& run) {
    for (const auto& k : run.corpses) {
        float shrink = std::min(1.0f, k.life / 0.35f);
        ellipse(game.gfx, k.pos + Vec2{0, 8}, 12 * shrink, 5 * shrink, Color{0, 0, 0, 70});
        rlPushMatrix();
        rlTranslatef(k.pos.x, k.pos.y - k.z, 0);
        rlRotatef(k.rot, 0, 0, 1);
        draw_soldier(game.gfx, {0, 0}, 0, k.unit, SOLDIER_SCALE * shrink, game.time, run.theme.biome);
        rlPopMatrix();
    }
}

void draw_boss_death(Game& game, const Run& run, const BossDeath& b) {
    const auto& g = game.gfx;
    const BossGenome& gen = b.mini ? run.mini_boss : run.boss;
    float dur = b.mini ? 1.3f : 2.1f;
    float k = std::clamp(b.t / dur, 0.0f, 1.0f);
    Vec2 jitter{std::sin(b.t * 70.0f) * b.radius * 0.06f * (0.4f + k),
                std::cos(b.t * 63.0f) * b.radius * 0.04f * (0.4f + k)};
    float swell = 1.0f + 0.15f * k * k;
    BeginBlendMode(BLEND_ADDITIVE);
    glow(g, b.pos, b.radius * (2.0f + 1.5f * k), Color{255, 120, 40, static_cast<unsigned char>(80 + 120 * k)});
    EndBlendMode();
    draw_boss(g, gen, run.theme.biome, b.pos + jitter, b.radius * swell, b.t * 3.0f,
              std::sin(b.t * 40.0f) > 0.0f ? 0.09f : 0.0f, true, game.time, 0.0f);
    BeginBlendMode(BLEND_ADDITIVE);
    Rng rays{static_cast<uint64_t>(b.pos.x * 13.0f + b.pos.y)};
    int n = 4 + static_cast<int>(k * 8.0f);
    for (int i = 0; i < n; ++i) {
        float a = rays.uniform(0, 2 * PI);
        float len = b.radius * (0.3f + 1.4f * k) * rays.uniform(0.6f, 1.0f);
        Vec2 d{std::cos(a), std::sin(a)};
        line(b.pos + jitter, b.pos + jitter + d * len, 10.0f * (0.5f + k), Color{255, 160, 60, 120});
        line(b.pos + jitter, b.pos + jitter + d * len * 0.9f, 3.5f, Color{255, 240, 200, 220});
    }
    EndBlendMode();
}

void draw_shells(Game& game, const Run& run) {
    const auto& g = game.gfx;
    for (const auto& s : run.shells) {
        float k = s.t / s.duration;
        Vec2 ground = s.from + (s.to - s.from) * k;
        if (s.bomb) {
            float h = (1.0f - k) * 200.0f;
            ellipse(g, s.to, 14 * k + 4, 6 * k + 2, Color{0, 0, 0, 90});
            disc(g, ground + Vec2{0, -h * 0.1f}, 9, Color{40, 40, 44, 255});
            ring(g, s.to, s.radius * (0.3f + 0.7f * k), Color{255, 60, 40, static_cast<unsigned char>(160 * k)});
        } else if (s.grenade) {
            float h = std::sin(k * PI) * 90.0f;
            ellipse(g, ground, 6, 3, Color{0, 0, 0, 70});
            disc(g, ground + Vec2{0, -h}, 6.5f, Color{40, 56, 30, 255});
            disc(g, ground + Vec2{0, -h}, 5.0f, Color{96, 128, 64, 255});
            disc(g, ground + Vec2{-1.5f, -h - 1.5f}, 1.8f, Color{255, 255, 255, 110});
        } else {
            float h = std::sin(k * PI) * 120.0f;
            ellipse(g, ground, 8, 4, Color{0, 0, 0, 70});
            disc(g, ground + Vec2{0, -h}, 8, Color{40, 36, 34, 255});
            disc(g, ground + Vec2{-2, -h - 2}, 3, Color{255, 255, 255, 90});
        }
    }
}

void draw_projectiles(Game& game, const Run& run) {
    const auto& g = game.gfx;
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& a : run.acids) glow(g, a.pos, 18, Color{200, 90, 255, 200});
    for (const auto& bl : run.bullets) {
        Vec2 dir = bl.vel.normalized();
        if (bl.flame) {
            float k = 1.0f - bl.life / unit_def(UnitType::Flamer).life;
            float sz = 12.0f + 26.0f * k;
            float a = 1.0f - k * 0.75f;
            glow(g, bl.pos, sz * 1.6f, Color{255, 90, 20, static_cast<unsigned char>(160 * a)});
            glow(g, bl.pos, sz,
                 Color{255, static_cast<unsigned char>(210 - 120 * k), 60, static_cast<unsigned char>(230 * a)});
            glow(g, bl.pos, sz * 0.4f, Color{255, 250, 210, static_cast<unsigned char>(200 * a * (1.0f - k))});
            continue;
        }
        Color core = bl.crit ? Color{255, 150, 60, 255} : Color{255, 226, 120, 255};
        if (bl.shard) core = Color{170, 230, 255, 255};
        float len = bl.unit == UnitType::Sniper ? 56.0f : bl.crit ? 34.0f : 26.0f;
        float w = bl.unit == UnitType::Sniper ? 3.2f : bl.crit ? 3.0f : 2.4f;
        Vec2 tail = bl.pos - dir * len;
        line(tail, bl.pos - dir * len * 0.45f, w * 0.7f, with_alpha(core, 0.25f));
        line(bl.pos - dir * len * 0.5f, bl.pos, w * 1.4f, with_alpha(core, 0.6f));
        line(bl.pos - dir * len * 0.3f, bl.pos, w * 0.6f, WHITE);
        glow(g, bl.pos, w * 4.0f, with_alpha(core, 0.85f));
    }
    for (const auto& bolt : run.bolts) {
        float a = bolt.life / bolt.max_life;
        for (size_t i = 1; i < bolt.points.size(); ++i) {
            line(bolt.points[i - 1], bolt.points[i], bolt.width * 2.5f, with_alpha(bolt.color, a * 0.35f));
            line(bolt.points[i - 1], bolt.points[i], bolt.width * 0.6f, with_alpha(WHITE, a));
        }
    }
    EndBlendMode();
    for (const auto& a : run.acids) disc(g, a.pos, 7, Color{170, 80, 220, 255});
}

void render_world(Game& game) {
    auto& run = *game.run;
    const auto& g = game.gfx;
    Vec2 shake{};
    if (run.shake > 0.0f) {
        shake = {std::sin(game.time * 91.0f) * run.shake * 0.6f, std::cos(game.time * 77.0f) * run.shake * 0.6f};
    }
    Camera2D cam{};
    float zoom = 1.0f + 0.05f * run.zoom_kick * run.zoom_kick;
    cam.target = {VIRTUAL_W * 0.5f, VIRTUAL_H * 0.5f};
    cam.offset = {VIRTUAL_W * 0.5f + shake.x, VIRTUAL_H * 0.5f + shake.y};
    cam.zoom = zoom;
    BeginMode2D(cam);

    draw_backdrop(game, run.theme, game.time);

    draw_shells(game, run);

    std::vector<const Enemy*> order;
    order.reserve(run.enemies.size());
    for (const auto& e : run.enemies) {
        if (!e.dead) order.push_back(&e);
    }
    std::ranges::sort(order, {}, [](const Enemy* e) { return e->pos.y; });
    for (const Enemy* e : order) {
        if (e->type == EnemyType::Boss) {
            const BossGenome& gen = e->mini_boss ? run.mini_boss : run.boss;
            draw_boss(g, gen, run.theme.biome, e->pos, e->radius, e->anim, e->flash, e->enraged, game.time,
                      e->shield_max > 0.0f ? std::max(0.0f, e->shield_hp / e->shield_max) : 0.0f);
            continue;
        }
        if (e->type == EnemyType::Flyer) continue;
        if (e->type == EnemyType::Barrel) {
            draw_barrel(g, e->pos, e->radius, e->flash, e->hp / e->max_hp, game.time);
            continue;
        }
        draw_monster(g, e->type, e->pos, e->radius, e->anim, e->flash, e->slow_timer > 0.0f, e->elite, game.time,
                     e->shield_max > 0.0f ? std::max(0.0f, e->shield_hp / e->shield_max) : 0.0f, e->shield_flash,
                     e->armor > 0.0f, run.theme.enemy_tint, run.theme.enemy_tint_amount, run.theme.biome, e->hurt);
        bool show_bar =
            (e->type == EnemyType::Brute || e->type == EnemyType::Spitter || e->type == EnemyType::Exploder ||
             e->type == EnemyType::Splitter || e->type == EnemyType::Shielder || e->elite || e->mini_boss) &&
            e->hp < e->max_hp;
        if (show_bar && !(e->type == EnemyType::Boss && !e->mini_boss)) {
            float w = e->radius * 1.8f;
            bar({e->pos.x - w * 0.5f, e->pos.y - e->radius - 16, w, 7}, e->hp / e->max_hp, Color{240, 70, 60, 255},
                Color{0, 0, 0, 150});
        }
    }

    for (const auto& b : run.boss_deaths) draw_boss_death(game, run, b);
    for (const auto& gate : run.gates) draw_gate(game, gate);
    for (const Enemy* e : order) {
        if (e->type != EnemyType::Flyer) continue;
        Color bat = blend(enemy_body_color(EnemyType::Flyer), run.theme.enemy_tint, run.theme.enemy_tint_amount);
        if (e->slow_timer > 0.0f) bat = mix(bat, Color{170, 220, 255, 255}, 0.45f);
        draw_bat(g, e->pos, e->radius, e->anim, e->flash, bat, e->elite, game.time);
    }
    for (const auto& f : run.loot) draw_loot(game, f);

    draw_night(run.theme);
    draw_night_lights(game, run);
    draw_projectiles(game, run);

    draw_particles(game, run, false);
    BeginBlendMode(BLEND_ADDITIVE);
    draw_particles(game, run, true);
    EndBlendMode();

    draw_wall(run);
    if (run.collapse_t < 0.0f) draw_turrets(game, run);

    int visible = visible_soldiers(run.soldiers);
    for (int i = visible - 1; i >= 0; --i) {
        Vec2 p = Vec2{run.squad_x, SQUAD_Y} + soldier_offset(i, visible);
        float bob = std::sin(game.time * 10.0f + static_cast<float>(i)) *
                    (std::abs(run.squad_target_x - run.squad_x) > 4 ? 2.0f : 0.0f);
        UnitType u = run.squad[static_cast<size_t>(i) % run.squad.size()];
        draw_soldier(g, p + Vec2{0, bob}, run.soldier_recoil[static_cast<size_t>(i)], u, SOLDIER_SCALE, game.time,
                     run.theme.biome);
    }
    draw_corpses(game, run);
    if (run.collapse_t >= 0.0f) draw_wall_collapse(run, g);

    if (run.plane_t >= 0.0f) {
        float x = -120.0f + run.plane_t * (VIRTUAL_W + 240.0f) / 1.2f;
        if (auto* sh = game.assets.get_texture("plane_shadow")) {
            sprite(*sh, {x - 30, run.plane_y + 60}, 150, 150, 90, Color{0, 0, 0, 90});
        }
        if (auto* pl = game.assets.get_texture("plane")) sprite(*pl, {x, run.plane_y - 40}, 150, 150, 90, WHITE);
    }

    for (const auto& f : run.coin_flies) {
        disc(g, f.pos, 10, Color{150, 100, 10, 255});
        disc(g, f.pos, 8, Color{255, 205, 50, 255});
        disc(g, f.pos + Vec2{-2, -2}, 3, Color{255, 255, 220, 200});
    }

    for (const auto& t : run.texts) {
        float age = t.max_life - t.life;
        float pop = age < 0.1f ? 0.6f + 4.0f * age : 1.0f;
        float a = std::min(1.0f, t.life / (t.max_life * 0.35f));
        text(g, t.text, t.pos, t.size * pop, with_alpha(t.color, a));
    }

    EndMode2D();
    draw_vignette();

    if (run.flash > 0.0f) {
        DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, Color{255, 255, 255, static_cast<unsigned char>(run.flash * 180)});
    }
    float ratio = run.wall_hp / run.stats.wall_max;
    if (ratio < 0.3f && run.phase != WavePhase::Won) {
        float k = (0.3f - ratio) / 0.3f * (0.6f + 0.4f * std::sin(game.time * 6.0f));
        DrawRectangleGradientV(0, VIRTUAL_H - 420, VIRTUAL_W, 420, Color{255, 0, 0, 0},
                               Color{255, 0, 0, static_cast<unsigned char>(k * 90)});
    }
}

} // namespace ls
