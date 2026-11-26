#include "core/gfx.hpp"
#include "core/game.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace ls {

namespace {

void paint_radial(Color* px, int atlas_w, int ox, int oy, int size, float (*profile)(float)) {
    float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = (static_cast<float>(x) - c) / c;
            float dy = (static_cast<float>(y) - c) / c;
            float d = std::sqrt(dx * dx + dy * dy);
            float a = std::clamp(profile(d), 0.0f, 1.0f);
            px[(oy + y) * atlas_w + (ox + x)] = Color{255, 255, 255, static_cast<unsigned char>(a * 255.0f)};
        }
    }
}

float profile_disc(float d) {
    return (1.0f - d) * 64.0f;
}
float profile_glow(float d) {
    float v = std::max(0.0f, 1.0f - d);
    return v * v;
}
float profile_ring(float d) {
    return (1.0f - std::abs(d - 0.92f) / 0.06f) * 1.6f;
}

void quad(const Texture2D& t, Rectangle src, Vec2 p, float rx, float ry, Color c) {
    Rectangle dst{p.x - rx, p.y - ry, rx * 2.0f, ry * 2.0f};
    DrawTexturePro(t, src, dst, {0, 0}, 0.0f, c);
}

// Stack-allocated null-terminated copy for zero-allocation text measurement & drawing.
struct CStrBuf {
    char stack[256];
    std::string heap;
    const char* ptr;

    explicit CStrBuf(std::string_view s) {
        if (s.size() < sizeof(stack)) {
            std::copy(s.begin(), s.end(), stack);
            stack[s.size()] = '\0';
            ptr = stack;
        } else {
            heap.assign(s);
            ptr = heap.c_str();
        }
    }
};

} // namespace

void Gfx::init() {
    constexpr int AW = 256;
    constexpr int CELL = 124;
    Image img = GenImageColor(AW, AW, BLANK);
    auto* px = static_cast<Color*>(img.data);
    paint_radial(px, AW, 2, 2, CELL, profile_disc);
    paint_radial(px, AW, 130, 2, CELL, profile_glow);
    paint_radial(px, AW, 2, 130, CELL, profile_ring);
    // Solid white quadrant (with wide margin) for raylib shape batching.
    for (int y = 132; y < 252; ++y) {
        for (int x = 132; x < 252; ++x) px[y * AW + x] = WHITE;
    }
    atlas = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(atlas, TEXTURE_FILTER_BILINEAR);
    disc_rect = {2, 2, CELL, CELL};
    glow_rect = {130, 2, CELL, CELL};
    ring_rect = {2, 130, CELL, CELL};
    white_rect = {148, 148, 88, 88};
    SetShapesTexture(atlas, white_rect);

    const char* font_path = "assets/fonts/LilitaOne-Regular.ttf";
    if (FileExists(font_path)) {
        font = LoadFontEx(font_path, 72, nullptr, 0);
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        font_loaded = true;
    } else {
        font = GetFontDefault();
    }
}

void Gfx::unload() {
    SetShapesTexture(Texture2D{}, Rectangle{});
    UnloadTexture(atlas);
    if (font_loaded) UnloadFont(font);
}

Color with_alpha(Color c, float a) {
    c.a = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(c.a));
    return c;
}

Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

Color brighten(Color c, float amount) {
    return mix(c, Color{255, 255, 255, c.a}, amount);
}
Color darken(Color c, float amount) {
    return mix(c, Color{0, 0, 0, c.a}, amount);
}

void disc(const Gfx& g, Vec2 p, float r, Color c) {
    quad(g.atlas, g.disc_rect, p, r, r, c);
}
void ellipse(const Gfx& g, Vec2 p, float rx, float ry, Color c) {
    quad(g.atlas, g.disc_rect, p, rx, ry, c);
}
void glow(const Gfx& g, Vec2 p, float r, Color c) {
    quad(g.atlas, g.glow_rect, p, r, r, c);
}
void ring(const Gfx& g, Vec2 p, float r, Color c) {
    quad(g.atlas, g.ring_rect, p, r, r, c);
}

void line(Vec2 a, Vec2 b, float w, Color c) {
    DrawLineEx(a.to_raylib(), b.to_raylib(), w, c);
}

namespace {
float g_canvas_scale = 1.0f;

void begin_base_camera() {
    Camera2D base{};
    base.zoom = g_canvas_scale;
    BeginMode2D(base);
}
} // namespace

void begin_canvas(const RenderTexture2D& target, float scale) {
    g_canvas_scale = scale;
    BeginTextureMode(target);
    begin_base_camera();
}

void end_canvas() {
    EndMode2D();
    EndTextureMode();
    g_canvas_scale = 1.0f;
}

void begin_camera(Camera2D cam) {
    cam.offset.x *= g_canvas_scale;
    cam.offset.y *= g_canvas_scale;
    cam.zoom *= g_canvas_scale;
    BeginMode2D(cam);
}

void end_camera() {
    EndMode2D();
    begin_base_camera();
}

void begin_scissor(float x, float y, float w, float h) {
    float s = g_canvas_scale;
    int x0 = static_cast<int>(std::floor(x * s));
    int y0 = static_cast<int>(std::floor(y * s));
    int x1 = static_cast<int>(std::ceil((x + w) * s));
    int y1 = static_cast<int>(std::ceil((y + h) * s));
    BeginScissorMode(x0, y0, x1 - x0, y1 - y0);
}

float text_width(const Gfx& g, std::string_view s, float size) {
    CStrBuf tmp{s};
    return MeasureTextEx(g.font, tmp.ptr, size, size * 0.02f).x;
}

void text(const Gfx& g, std::string_view s, Vec2 p, float size, Color c, Align align, bool outline) {
    CStrBuf tmp{s};
    float spacing = size * 0.02f;
    Vector2 m = MeasureTextEx(g.font, tmp.ptr, size, spacing);
    float x = p.x;
    if (align == Align::Center) x -= m.x * 0.5f;
    if (align == Align::Right) x -= m.x;
    float y = p.y - m.y * 0.5f;
    if (outline) {
        float o = std::max(2.0f, size * 0.07f);
        Color oc = Color{20, 16, 30, c.a};
        DrawTextEx(g.font, tmp.ptr, {x, y + o * 1.3f}, size, spacing, oc);
        DrawTextEx(g.font, tmp.ptr, {x - o, y}, size, spacing, oc);
        DrawTextEx(g.font, tmp.ptr, {x + o, y}, size, spacing, oc);
        DrawTextEx(g.font, tmp.ptr, {x, y - o}, size, spacing, oc);
    }
    DrawTextEx(g.font, tmp.ptr, {x, y}, size, spacing, c);
}

void text_fit(const Gfx& g, std::string_view s, Vec2 p, float size, float max_width, Color c, bool outline) {
    float w = text_width(g, s, size);
    if (w > max_width && w > 0.0f) size *= max_width / w;
    text(g, s, p, size, c, Align::Center, outline);
}

void panel(Rectangle r, Color fill, Color border, float roundness, float border_w) {
    DrawRectangleRounded({r.x, r.y + 6.0f, r.width, r.height}, roundness, 8, Color{0, 0, 0, 70});
    DrawRectangleRounded(r, roundness, 8, fill);
    if (border_w > 0.0f) DrawRectangleRoundedLinesEx(r, roundness, 8, border_w, border);
}

void bar(Rectangle r, float ratio, Color fill, Color back) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    DrawRectangleRounded(r, 0.5f, 6, back);
    if (ratio > 0.0f) {
        Rectangle f{r.x, r.y, std::max(r.height, r.width * ratio), r.height};
        DrawRectangleRounded(f, 0.5f, 6, fill);
        DrawRectangleRounded({f.x + 3, f.y + 2, f.width - 6, f.height * 0.35f}, 0.5f, 6, Color{255, 255, 255, 60});
    }
}

bool pointer_in(const Game& game, Rectangle r) {
    return CheckCollisionPointRec(game.input.pos.to_raylib(), r);
}

bool hit(Game& game, Rectangle r) {
    if (game.input.consumed || !game.input.pressed) return false;
    if (!pointer_in(game, r)) return false;
    game.input.consumed = true;
    return true;
}

const char* key_name(int key) {
    switch (key) {
    case KEY_ENTER:
        return "ENTER";
    case KEY_SPACE:
        return "SPACE";
    case KEY_ESCAPE:
        return "ESC";
    case KEY_TAB:
        return "TAB";
    case KEY_LEFT:
        return "<";
    case KEY_RIGHT:
        return ">";
    default:
        break;
    }
    static char buf[2] = {0, 0};
    buf[0] = (key >= 32 && key < 127) ? static_cast<char>(key) : '?';
    return buf;
}

void key_badge(const Gfx& g, Vec2 at, const char* label) {
    float w = std::max(26.0f, text_width(g, label, 16) + 14.0f);
    Rectangle r{at.x - w * 0.5f, at.y - 13, w, 26};
    DrawRectangleRounded({r.x, r.y + 3, r.width, r.height}, 0.4f, 6, Color{0, 0, 0, 120});
    DrawRectangleRounded(r, 0.4f, 6, Color{236, 236, 244, 255});
    DrawRectangleRoundedLinesEx(r, 0.4f, 6, 2.0f, Color{60, 60, 80, 255});
    text(g, label, {at.x, at.y}, 16, Color{40, 40, 60, 255}, Align::Center, false);
}

bool button(Game& game, Rectangle r, std::string_view label, Color color, bool enabled, float font_size, int key,
            bool focused) {
    bool held = enabled && game.input.down && pointer_in(game, r);
    Color fill = enabled ? color : Color{90, 90, 100, 255};
    Rectangle body = r;
    float lift = held ? 2.0f : 0.0f;
    DrawRectangleRounded({r.x, r.y + 8.0f, r.width, r.height}, 0.35f, 8, darken(fill, 0.45f));
    body.y += lift + (held ? 4.0f : 0.0f);
    DrawRectangleRounded(body, 0.35f, 8, fill);
    DrawRectangleRounded({body.x + 6, body.y + 4, body.width - 12, body.height * 0.42f}, 0.35f, 8,
                         Color{255, 255, 255, 50});
    DrawRectangleRoundedLinesEx(body, 0.35f, 8, 3.0f, darken(fill, 0.35f));
    text(game.gfx, label, {body.x + body.width * 0.5f, body.y + body.height * 0.5f}, font_size,
         enabled ? WHITE : Color{200, 200, 200, 255});
    if (focused) {
        float pulse = 0.5f + 0.5f * std::sin(game.time * 8.0f);
        DrawRectangleRoundedLinesEx({body.x - 5, body.y - 5, body.width + 10, body.height + 10}, 0.35f, 8, 4.0f,
                                    Color{255, 230, 120, static_cast<unsigned char>(160 + 95 * pulse)});
    }
    if (key != 0 && !game.touch_ui) key_badge(game.gfx, {body.x + body.width - 8, body.y + 2}, key_name(key));
    if (!enabled) return false;
    if (key != 0 && IsKeyPressed(key)) {
        game.sound.play(Sfx::Click, 0.7f);
        return true;
    }
    if (hit(game, r)) {
        game.sound.play(Sfx::Click, 0.7f);
        return true;
    }
    return false;
}

} // namespace ls
