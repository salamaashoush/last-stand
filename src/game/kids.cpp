#include "game/kids.hpp"
#include <algorithm>
#include <cmath>
#include <rlgl.h>
#include <utility>

namespace ls {

namespace {

struct KidLook {
    Color skin;
    Color skin_shade;
    Color hair;
    Color top;
    Color top_shade;
    Color bottom;
    Color shoes;
    float curls;
    float head;
    bool beard;
};

KidLook look(Kid k) {
    if (k == Kid::Omar) {
        return {{224, 172, 132, 255},
                {196, 140, 104, 255},
                {42, 28, 24, 255},
                {62, 122, 96, 255},
                {44, 92, 72, 255},
                {30, 30, 36, 255},
                {214, 52, 64, 255},
                1.0f,
                1.0f,
                false};
    }
    if (k == Kid::Salama) {
        return {{218, 164, 124, 255},
                {186, 132, 96, 255},
                {36, 26, 24, 255},
                {48, 76, 138, 255},
                {32, 52, 102, 255},
                {44, 48, 62, 255},
                {226, 164, 46, 255},
                0.35f,
                1.14f,
                true};
    }
    return {{230, 180, 142, 255},
            {204, 150, 112, 255},
            {48, 32, 26, 255},
            {148, 206, 222, 255},
            {104, 166, 190, 255},
            {38, 48, 92, 255},
            {50, 54, 70, 255},
            0.5f,
            1.2f,
            false};
}

void tri(Vec2 a, Vec2 b, Vec2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) std::swap(b, c);
    DrawTriangle(a.to_raylib(), b.to_raylib(), c.to_raylib(), col);
}

void heart(Vec2 c, float s, Color col) {
    DrawCircleV({c.x - s * 0.5f, c.y - s * 0.2f}, s * 0.55f, col);
    DrawCircleV({c.x + s * 0.5f, c.y - s * 0.2f}, s * 0.55f, col);
    tri({c.x - s * 1.02f, c.y}, {c.x + s * 1.02f, c.y}, {c.x, c.y + s * 1.1f}, col);
}

void draw_hair(const Gfx& g, const KidLook& L, Vec2 h, float r, bool back) {
    Color dark = L.hair;
    Color light = brighten(L.hair, 0.22f);
    float vol = L.curls;
    if (L.beard) {
        // Neat styled Dad hair with a fun front swoop!
        if (back) {
            ellipse(g, h + Vec2{0, -r * 0.22f}, r * 1.04f, r * 0.82f, darken(dark, 0.15f));
            return;
        }
        ellipse(g, h + Vec2{0, -r * 0.72f}, r * 0.94f, r * 0.34f, dark);
        ellipse(g, h + Vec2{-r * 0.22f, -r * 0.86f}, r * 0.56f, r * 0.24f, dark);
        ellipse(g, h + Vec2{r * 0.26f, -r * 0.8f}, r * 0.44f, r * 0.2f, light);
        // Sideburns connecting cleanly toward the beard.
        DrawRectangleRounded({h.x - r * 0.96f, h.y - r * 0.38f, r * 0.18f, r * 0.45f}, 0.5f, 4, dark);
        DrawRectangleRounded({h.x + r * 0.78f, h.y - r * 0.38f, r * 0.18f, r * 0.45f}, 0.5f, 4, dark);
        return;
    }
    if (back) {
        // A solid cap the curls sit on, so the hair reads as a mass rather than beads.
        ellipse(g, h + Vec2{0, -r * 0.28f}, r * (1.02f + 0.1f * vol), r * (0.78f + 0.12f * vol), darken(dark, 0.15f));
        return;
    }
    ellipse(g, h + Vec2{0, -r * 0.66f}, r * (0.95f + 0.06f * vol), r * (0.38f + 0.06f * vol), dark);
    int curls = 6 + static_cast<int>(5.0f * vol);
    for (int i = 0; i < curls; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(curls - 1);
        float a = PI * (1.08f + 0.84f * t);
        float wobble = std::sin(static_cast<float>(i) * 2.7f) * 0.08f;
        float rr = r * (0.84f + 0.16f * vol + wobble);
        Vec2 p = h + Vec2{std::cos(a) * rr, std::sin(a) * rr * (0.85f + 0.1f * vol) - r * 0.3f};
        float cr = r * (0.2f + 0.1f * vol) * (0.8f + 0.3f * std::abs(std::sin(static_cast<float>(i) * 1.9f)));
        disc(g, p, cr, dark);
        ring(g, p, cr * 0.72f, Color{light.r, light.g, light.b, 170});
    }
    // The curl that falls on the forehead, like both boys wear it.
    for (int i = 0; i < 2 + static_cast<int>(vol * 2.0f); ++i) {
        Vec2 p =
            h + Vec2{-r * 0.22f + static_cast<float>(i) * r * 0.2f, -r * 0.7f + static_cast<float>(i % 2) * r * 0.07f};
        disc(g, p, r * 0.15f, dark);
        ring(g, p, r * 0.1f, Color{light.r, light.g, light.b, 170});
    }
}

void draw_face(const Gfx& g, Kid k, const KidLook& L, Vec2 h, float r, KidPose pose, float time) {
    float blink = std::fmod(time + (k == Kid::Ali ? 1.3f : 0.0f), 3.6f) < 0.12f ? 0.15f : 1.0f;
    bool shades = k == Kid::Ali && pose == KidPose::Peace;
    Vec2 el = h + Vec2{-r * 0.36f, r * 0.08f};
    Vec2 er = h + Vec2{r * 0.36f, r * 0.08f};
    if (shades) {
        Color frame{246, 214, 90, 255};
        for (Vec2 e : {el, er}) {
            DrawRectangleRounded({e.x - r * 0.3f, e.y - r * 0.2f, r * 0.6f, r * 0.4f}, 0.5f, 6, frame);
            DrawRectangleRounded({e.x - r * 0.24f, e.y - r * 0.15f, r * 0.48f, r * 0.3f}, 0.5f, 6,
                                 Color{30, 30, 40, 255});
            line(e + Vec2{-r * 0.15f, -r * 0.08f}, e + Vec2{-r * 0.02f, -r * 0.12f}, std::max(1.0f, r * 0.05f),
                 Color{255, 255, 255, 160});
        }
        line(el + Vec2{r * 0.3f, -r * 0.05f}, er + Vec2{-r * 0.3f, -r * 0.05f}, std::max(1.5f, r * 0.07f), frame);
    } else {
        for (Vec2 e : {el, er}) {
            ellipse(g, e, r * 0.2f, r * 0.24f * blink, Color{255, 255, 255, 255});
            ellipse(g, e + Vec2{0, r * 0.02f}, r * 0.15f, r * 0.19f * blink, Color{58, 34, 24, 255});
            if (blink > 0.5f) {
                disc(g, e + Vec2{-r * 0.05f, -r * 0.06f}, r * 0.06f, WHITE);
                disc(g, e + Vec2{r * 0.05f, r * 0.06f}, r * 0.03f, WHITE);
            }
        }
        // Eyebrows: Omar's lift with his grin, Ali's tilt with his cheek.
        float lift = k == Kid::Omar ? -0.04f : 0.02f;
        line(el + Vec2{-r * 0.2f, -r * (0.33f - lift)}, el + Vec2{r * 0.14f, -r * 0.38f}, std::max(1.5f, r * 0.07f),
             L.hair);
        line(er + Vec2{-r * 0.14f, -r * 0.38f}, er + Vec2{r * 0.2f, -r * (0.33f + lift)}, std::max(1.5f, r * 0.07f),
             L.hair);
    }
    disc(g, h + Vec2{-r * 0.55f, r * 0.38f}, r * 0.14f, Color{240, 130, 130, 90});
    disc(g, h + Vec2{r * 0.55f, r * 0.38f}, r * 0.14f, Color{240, 130, 130, 90});
    ellipse(g, h + Vec2{0, r * 0.3f}, r * 0.08f, r * 0.06f, L.skin_shade);

    Vec2 m = h + Vec2{0, r * 0.55f};
    if (k == Kid::Salama) {
        // Warm trimmed Dad beard & cheeky mustache!
        Color bd = Color{42, 30, 26, 255};
        DrawCircleSector((h + Vec2{0, r * 0.12f}).to_raylib(), r * 0.96f, 18, 162, 20, with_alpha(bd, 0.92f));
        ellipse(g, m + Vec2{0, r * 0.04f}, r * 0.46f, r * 0.32f, L.skin);
        // Mustache arcs
        ellipse(g, m + Vec2{-r * 0.19f, -r * 0.12f}, r * 0.22f, r * 0.09f, bd);
        ellipse(g, m + Vec2{r * 0.19f, -r * 0.12f}, r * 0.22f, r * 0.09f, bd);
        if (pose == KidPose::BabaThrow || pose == KidPose::Cheer) {
            // Big hilarious Dad belly-laugh mouth!
            ellipse(g, m + Vec2{0, r * 0.06f}, r * 0.32f, r * 0.24f, Color{120, 28, 38, 255});
            DrawRectangleRounded({m.x - r * 0.26f, m.y - r * 0.08f, r * 0.52f, r * 0.1f}, 0.4f, 4, WHITE);
            ellipse(g, m + Vec2{0, r * 0.18f}, r * 0.15f, r * 0.08f, Color{236, 115, 130, 255});
        } else {
            DrawCircleSector((m + Vec2{0, r * 0.01f}).to_raylib(), r * 0.28f, 0, 180, 16, Color{120, 30, 40, 255});
            DrawRectangleRec({m.x - r * 0.24f, m.y + r * 0.01f, r * 0.48f, r * 0.08f}, WHITE);
        }
    } else if (k == Kid::Omar) {
        bool shout = pose == KidPose::Rally;
        if (shout) {
            ellipse(g, m, r * 0.26f, r * 0.2f, Color{120, 30, 40, 255});
            ellipse(g, m + Vec2{0, -r * 0.1f}, r * 0.22f, r * 0.06f, WHITE);
            ellipse(g, m + Vec2{0, r * 0.1f}, r * 0.12f, r * 0.06f, Color{230, 110, 120, 255});
        } else {
            // His wide, slightly mischievous grin.
            DrawCircleSector(m.to_raylib(), r * 0.3f, 0, 180, 16, Color{120, 30, 40, 255});
            DrawRectangleRec({m.x - r * 0.26f, m.y, r * 0.52f, r * 0.08f}, WHITE);
            line(m + Vec2{-r * 0.34f, -r * 0.02f}, m + Vec2{-r * 0.28f, r * 0.03f}, std::max(1.0f, r * 0.05f),
                 L.skin_shade);
        }
    } else {
        DrawCircleSector((m + Vec2{r * 0.04f, 0}).to_raylib(), r * 0.22f, 0, 180, 16, Color{120, 30, 40, 255});
        if (pose == KidPose::Peace || pose == KidPose::Wave) {
            // Tongue out, his trademark.
            ellipse(g, m + Vec2{r * 0.04f, r * 0.2f}, r * 0.12f, r * 0.16f, Color{236, 120, 140, 255});
            line(m + Vec2{r * 0.04f, r * 0.12f}, m + Vec2{r * 0.04f, r * 0.28f}, std::max(1.0f, r * 0.03f),
                 Color{200, 80, 100, 255});
        }
    }
}

// Two fingers up: the peace sign Ali flashes in nearly every photo.
void draw_peace_hand(const Gfx& g, const KidLook& L, Vec2 p, float s) {
    disc(g, p, s * 0.32f, L.skin);
    for (int f = -1; f <= 1; f += 2) {
        Vec2 tip = p + Vec2{static_cast<float>(f) * s * 0.22f, -s * 0.85f};
        line(p + Vec2{static_cast<float>(f) * s * 0.1f, -s * 0.1f}, tip, s * 0.2f, L.skin_shade);
        line(p + Vec2{static_cast<float>(f) * s * 0.1f, -s * 0.1f}, tip, s * 0.15f, L.skin);
        disc(g, tip, s * 0.08f, L.skin);
    }
}

} // namespace

void draw_kid(const Gfx& g, Kid k, Vec2 feet, float scale, KidPose pose, float time) {
    KidLook L = look(k);
    auto S = [scale](float v) { return v * scale; };
    float bob = pose == KidPose::Cheer || pose == KidPose::Rally ? std::abs(std::sin(time * 9.0f)) * S(5)
                                                                 : std::sin(time * 3.0f) * S(1.2f);
    Vec2 base = feet - Vec2{0, bob};
    float head_r = S(19.0f) * L.head;
    Vec2 h = base + Vec2{0, -S(52)};
    Color outline{30, 20, 20, 255};

    ellipse(g, feet + Vec2{0, S(2)}, S(18), S(5), Color{0, 0, 0, 70});
    for (int side = -1; side <= 1; side += 2) {
        Vec2 leg = base + Vec2{static_cast<float>(side) * S(6), -S(10)};
        DrawRectangleRounded({leg.x - S(4.5f), leg.y - S(2), S(9), S(11)}, 0.4f, 4, L.bottom);
        ellipse(g, base + Vec2{static_cast<float>(side) * S(7), -S(1)}, S(7.5f), S(4.5f), outline);
        ellipse(g, base + Vec2{static_cast<float>(side) * S(7), -S(1.5f)}, S(6.5f), S(3.8f), L.shoes);
    }
    Rectangle body{base.x - S(14), base.y - S(36), S(28), S(26)};
    DrawRectangleRounded({body.x - S(1.5f), body.y - S(1.5f), body.width + S(3), body.height + S(3)}, 0.45f, 8,
                         outline);
    DrawRectangleRounded(body, 0.45f, 8, L.top);
    DrawRectangleRounded({body.x, body.y + body.height * 0.6f, body.width, body.height * 0.4f}, 0.45f, 8, L.top_shade);
    if (k == Kid::Omar) {
        // Hoodie: drawstrings and a front pocket.
        line({base.x - S(4), body.y + S(2)}, {base.x - S(5), body.y + S(11)}, S(1.5f), WHITE);
        line({base.x + S(4), body.y + S(2)}, {base.x + S(5), body.y + S(11)}, S(1.5f), WHITE);
        DrawRectangleRounded({base.x - S(9), body.y + S(15), S(18), S(7)}, 0.5f, 4, L.top_shade);
    } else if (k == Kid::Salama) {
        // Dad's royal blue jersey with gold collar & #1 Dad star crest!
        DrawRectangleRounded({base.x - S(8), body.y, S(16), S(5)}, 0.4f, 4, Color{255, 205, 60, 255});
        DrawPoly({base.x, body.y + S(13)}, 5, S(5.5f), -18.0f, Color{255, 205, 60, 255});
    } else {
        // Little sharks on his tee.
        for (int i = 0; i < 3; ++i) {
            Vec2 f = {body.x + S(7) + static_cast<float>(i) * S(8), body.y + S(8) + static_cast<float>(i % 2) * S(7)};
            ellipse(g, f, S(3.2f), S(1.6f), Color{60, 110, 180, 255});
            tri(f + Vec2{-S(1), -S(1)}, f + Vec2{S(1), -S(1)}, f + Vec2{0, -S(3.5f)}, Color{60, 110, 180, 255});
        }
    }

    // Arms, posed.
    Vec2 sl = base + Vec2{-S(13), -S(32)};
    Vec2 sr = base + Vec2{S(13), -S(32)};
    Vec2 hl = base + Vec2{-S(17), -S(14)};
    Vec2 hr = base + Vec2{S(17), -S(14)};
    float wave = std::sin(time * 10.0f) * S(4);
    switch (pose) {
    case KidPose::Wave:
        hr = base + Vec2{S(24) + wave, -S(52)};
        break;
    case KidPose::Cheer:
        hl = base + Vec2{-S(22), -S(56)};
        hr = base + Vec2{S(22), -S(56)};
        break;
    case KidPose::Rally:
        hr = base + Vec2{S(16), -S(62)};
        break;
    case KidPose::Peace:
        hr = base + Vec2{S(26), -S(46)};
        break;
    case KidPose::BabaThrow:
        hl = base + Vec2{-S(24), -S(48) + wave * 0.5f};
        hr = base + Vec2{S(26), -S(60) - wave * 0.5f};
        break;
    case KidPose::Idle:
        break;
    }
    for (auto [s, hand] : {std::pair{sl, hl}, std::pair{sr, hr}}) {
        line(s, hand, S(8.5f), outline);
        line(s, hand, S(6.5f), L.top);
    }
    disc(g, hl, S(4.5f), L.skin);
    if (pose == KidPose::Peace) {
        draw_peace_hand(g, L, hr, S(14));
    } else {
        disc(g, hr, S(4.5f), L.skin);
    }

    draw_hair(g, L, h, head_r, true);
    disc(g, h + Vec2{-head_r * 0.95f, head_r * 0.12f}, head_r * 0.2f, L.skin_shade);
    disc(g, h + Vec2{head_r * 0.95f, head_r * 0.12f}, head_r * 0.2f, L.skin_shade);
    disc(g, h, head_r + S(1.5f), outline);
    disc(g, h, head_r, L.skin);
    ellipse(g, h + Vec2{0, head_r * 0.45f}, head_r * 0.8f, head_r * 0.42f,
            Color{L.skin_shade.r, L.skin_shade.g, L.skin_shade.b, 60});
    draw_face(g, k, L, h, head_r, pose, time);
    draw_hair(g, L, h, head_r, false);

    if (pose == KidPose::Rally) {
        float side = feet.x > 600.0f ? -1.0f : 1.0f;
        Vec2 b = h + Vec2{side * S(36), -S(26)};
        DrawRectangleRounded({b.x - S(22), b.y - S(11), S(44), S(22)}, 0.6f, 8, WHITE);
        tri(b + Vec2{-side * S(14), S(9)}, b + Vec2{-side * S(4), S(9)}, b + Vec2{-side * S(18), S(18)}, WHITE);
        text(g, "YALLA!", b, S(13), Color{220, 60, 40, 255}, Align::Center, false);
    }
    if (pose == KidPose::BabaThrow) {
        float side = feet.x > 520.0f ? -1.0f : 1.0f;
        Vec2 b = h + Vec2{side * S(46), -S(28)};
        DrawRectangleRounded({b.x - S(34), b.y - S(12), S(68), S(24)}, 0.6f, 8, Color{255, 238, 120, 255});
        DrawRectangleRoundedLinesEx({b.x - S(34), b.y - S(12), S(68), S(24)}, 0.6f, 8, S(1.8f), Color{180, 70, 20, 255});
        tri(b + Vec2{-side * S(18), S(10)}, b + Vec2{-side * S(6), S(10)}, b + Vec2{-side * S(24), S(19)},
            Color{255, 238, 120, 255});
        text(g, "BABA BOOM!", b, S(11.5f), Color{200, 45, 25, 255}, Align::Center, false);
    }
    if (pose == KidPose::Peace) {
        for (int i = 0; i < 3; ++i) {
            float a = time * 2.0f + static_cast<float>(i) * 2.1f;
            heart(h + Vec2{std::cos(a) * head_r * 1.8f, -head_r * 1.2f + std::sin(a * 1.3f) * S(6)}, S(4),
                  Color{255, 110, 150, 230});
        }
    }
}

void draw_kid_portrait(const Gfx& g, Kid k, Vec2 c, float r, float time, bool happy) {
    KidLook L = look(k);
    float hr = r * 0.62f;
    Vec2 h = c + Vec2{0, r * 0.1f};
    DrawRectangleRounded({c.x - r * 0.75f, c.y + r * 0.55f, r * 1.5f, r * 0.6f}, 0.6f, 8, L.top);
    draw_hair(g, L, h, hr, true);
    disc(g, h + Vec2{-hr * 0.95f, hr * 0.12f}, hr * 0.2f, L.skin_shade);
    disc(g, h + Vec2{hr * 0.95f, hr * 0.12f}, hr * 0.2f, L.skin_shade);
    disc(g, h, hr + 1.5f, Color{30, 20, 20, 255});
    disc(g, h, hr, L.skin);
    KidPose p = happy ? (k == Kid::Omar ? KidPose::Rally : k == Kid::Salama ? KidPose::BabaThrow : KidPose::Peace)
                      : KidPose::Idle;
    draw_face(g, k, L, h, hr, p, time);
    draw_hair(g, L, h, hr, false);
}

void draw_salama_head(const Gfx& g, Vec2 c, float r, float rot_deg, float time, float puff) {
    KidLook L = look(Kid::Salama);
    float rr = r * (1.0f + puff * 0.22f);
    // Comedic golden glow & comic impact ring around the flying Dad head!
    glow(g, c, rr * 2.1f, Color{255, 195, 55, 130});
    ring(g, c, rr * (1.28f + 0.08f * std::sin(time * 18.0f)), Color{255, 225, 95, 195});
    rlPushMatrix();
    rlTranslatef(c.x, c.y, 0.0f);
    rlRotatef(rot_deg, 0.0f, 0.0f, 1.0f);
    Vec2 z{0.0f, 0.0f};
    draw_hair(g, L, z, rr, true);
    disc(g, {-rr * 0.96f, rr * 0.12f}, rr * 0.22f, L.skin_shade);
    disc(g, {rr * 0.96f, rr * 0.12f}, rr * 0.22f, L.skin_shade);
    disc(g, z, rr + std::max(2.5f, rr * 0.09f), Color{34, 22, 18, 255});
    disc(g, z, rr, mix(L.skin, Color{255, 190, 140, 255}, puff * 0.35f));
    draw_face(g, Kid::Salama, L, z, rr, KidPose::BabaThrow, time);
    draw_hair(g, L, z, rr, false);
    // Comedic cheeky 4-point cartoon gleam star near temple!
    Vec2 sp{rr * 0.72f, -rr * 0.62f};
    float sl = rr * (0.28f + 0.05f * std::sin(time * 16.0f));
    float sw = sl * 0.30f;
    tri(sp + Vec2{0, -sl}, sp + Vec2{-sw, 0}, sp + Vec2{sw, 0}, Color{255, 245, 130, 250});
    tri(sp + Vec2{0, sl}, sp + Vec2{sw, 0}, sp + Vec2{-sw, 0}, Color{255, 245, 130, 250});
    tri(sp + Vec2{-sl, 0}, sp + Vec2{0, sw}, sp + Vec2{0, -sw}, Color{255, 245, 130, 250});
    tri(sp + Vec2{sl, 0}, sp + Vec2{0, -sw}, sp + Vec2{0, sw}, Color{255, 245, 130, 250});
    disc(g, sp, sw * 0.9f, WHITE);
    rlPopMatrix();
}

void draw_heart(Vec2 c, float s, Color col) {
    heart(c, s, col);
}

} // namespace ls
