#pragma once
#include "core/types.hpp"
#include <raylib.h>
#include <string_view>

namespace ls {

struct Game;

enum class Align : uint8_t { Left, Center, Right };

struct Gfx {
    Texture2D disc{};
    Texture2D glow{};
    Texture2D ring{};
    Font font{};
    bool font_loaded{false};

    void init();
    void unload();
};

Color with_alpha(Color c, float a);
Color mix(Color a, Color b, float t);
Color brighten(Color c, float amount);
Color darken(Color c, float amount);

void disc(const Gfx& g, Vec2 p, float r, Color c);
void ellipse(const Gfx& g, Vec2 p, float rx, float ry, Color c);
void glow(const Gfx& g, Vec2 p, float r, Color c);
void ring(const Gfx& g, Vec2 p, float r, Color c);
void line(Vec2 a, Vec2 b, float w, Color c);

// The canvas is backed at a multiple of the virtual resolution so dense screens stay sharp.
// Drawing stays in virtual units; cameras and scissor rectangles, which raylib applies in
// framebuffer pixels, go through these so they pick up the canvas scale.
void begin_canvas(const RenderTexture2D& target, float scale);
void end_canvas();
void begin_camera(Camera2D cam);
void end_camera();
void begin_scissor(float x, float y, float w, float h);

void text(const Gfx& g, std::string_view s, Vec2 p, float size, Color c, Align align = Align::Center,
          bool outline = true);
float text_width(const Gfx& g, std::string_view s, float size);
void text_fit(const Gfx& g, std::string_view s, Vec2 p, float size, float max_width, Color c, bool outline = true);

void panel(Rectangle r, Color fill, Color border, float roundness = 0.25f, float border_w = 4.0f);
void bar(Rectangle r, float ratio, Color fill, Color back);

// Immediate-mode button: draws itself and reports a tap released inside its bounds.
// A keyboard shortcut (raylib KeyboardKey) also triggers it and shows as a small badge.
// `focused` draws the selection outline used for arrow-key navigation.
bool button(Game& game, Rectangle r, std::string_view label, Color color, bool enabled = true, float font_size = 34.0f,
            int key = 0, bool focused = false);
const char* key_name(int key);
void key_badge(const Gfx& g, Vec2 at, const char* label);
bool hit(Game& game, Rectangle r);
bool pointer_in(const Game& game, Rectangle r);

} // namespace ls
