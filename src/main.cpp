#include "core/game.hpp"
#include "game/autopilot.hpp"
#include "game/render.hpp"
#include "states/states.hpp"
#include <algorithm>
#include <cstdlib>
#include <raylib.h>
#include <rlgl.h>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

ls::Game* g_game = nullptr;
bool g_game_touch = false;

void draw_toasts(ls::Game& game, float dt) {
    float y = 120.0f;
    for (auto& t : game.toasts) {
        t.timer -= dt;
        float a = std::min(1.0f, t.timer * 2.0f);
        float w = ls::text_width(game.gfx, t.text, 28) + 60;
        Rectangle r{ls::VIRTUAL_W * 0.5f - w * 0.5f, y, w, 56};
        DrawRectangleRounded(r, 0.5f, 8, Color{10, 10, 30, static_cast<unsigned char>(220 * a)});
        DrawRectangleRoundedLinesEx(r, 0.5f, 8, 3, ls::with_alpha(t.color, a));
        ls::text(game.gfx, t.text, {r.x + w * 0.5f, y + 29}, 28, ls::with_alpha(t.color, a));
        y += 66.0f;
    }
    std::erase_if(game.toasts, [](const ls::Toast& t) { return t.timer <= 0.0f; });
}

// LS_SHOT_DIR + LS_SHOT_EVERY dump the canvas periodically, for checking visuals without a human.
void capture_if_requested(ls::Game& game) {
    static const char* dir = std::getenv("LS_SHOT_DIR");
    static const float every = std::getenv("LS_SHOT_EVERY") ? std::strtof(std::getenv("LS_SHOT_EVERY"), nullptr) : 5.0f;
    static float next = 1.0f;
    static int index = 0;
    if (!dir || game.time < next) return;
    next = game.time + every;
    Image img = LoadImageFromTexture(game.canvas.texture);
    ImageFlipVertical(&img);
    std::string path = std::string(dir) + "/shot_" + std::to_string(index++) + ".png";
    ExportImage(img, path.c_str());
    UnloadImage(img);
}

#ifdef __EMSCRIPTEN__
// Render at the screen's real pixel density (capped) so phones get a sharp image; touch and
// mouse input are already scaled by raylib from CSS pixels to canvas pixels.
void fit_canvas() {
    double dpr = std::min(EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; }), 2.5);
    int w = static_cast<int>(EM_ASM_INT({ return window.innerWidth; }) * dpr);
    int h = static_cast<int>(EM_ASM_INT({ return window.innerHeight; }) * dpr);
    if (w > 0 && h > 0 && (w != GetScreenWidth() || h != GetScreenHeight())) SetWindowSize(w, h);
}
#endif

void frame() {
    auto& game = *g_game;
#ifdef __EMSCRIPTEN__
    fit_canvas();
#endif
    float dt = std::min(GetFrameTime(), 1.0f / 20.0f);
    game.time += dt;
    game.update_input();
    game.update_music();
    game.states.update(game, dt);

    BeginTextureMode(game.canvas);
    ClearBackground(BLACK);
    game.states.render(game);
    draw_toasts(game, dt);
    EndTextureMode();

    Rectangle src{0, 0, static_cast<float>(game.canvas.texture.width), -static_cast<float>(game.canvas.texture.height)};
    static RenderTexture2D blur = [] {
        RenderTexture2D t = LoadRenderTexture(18, 32);
        SetTextureFilter(t.texture, TEXTURE_FILTER_BILINEAR);
        return t;
    }();
    BeginTextureMode(blur);
    DrawTexturePro(game.canvas.texture, src, {0, 0, 18, 32}, {0, 0}, 0.0f, WHITE);
    EndTextureMode();

    BeginDrawing();
    ClearBackground(Color{6, 6, 12, 255});
    // Letterbox bars show a dimmed, heavily blurred copy of the scene instead of flat black.
    float sw = static_cast<float>(GetScreenWidth());
    float sh = static_cast<float>(GetScreenHeight());
    float cover = std::max(sw / ls::VIRTUAL_W, sh / ls::VIRTUAL_H) * 1.15f;
    Rectangle bg{(sw - ls::VIRTUAL_W * cover) * 0.5f, (sh - ls::VIRTUAL_H * cover) * 0.5f, ls::VIRTUAL_W * cover,
                 ls::VIRTUAL_H * cover};
    Rectangle blur_src{0, 0, 18, -32};
    DrawTexturePro(blur.texture, blur_src, bg, {0, 0}, 0.0f, Color{80, 80, 92, 255});
    DrawTexturePro(game.canvas.texture, src, game.viewport, {0, 0}, 0.0f, WHITE);
    if (std::getenv("LS_DEBUG_SIZE")) {
        static int n = 0;
        if (n < 4 || n == 60) {
            TraceLog(LOG_INFO, "SIZE frame=%d screen=%dx%d render=%dx%d dpi=%.2f", n, GetScreenWidth(),
                     GetScreenHeight(), GetRenderWidth(), GetRenderHeight(), GetWindowScaleDPI().x);
            rlDrawRenderBatchActive();
            Image img = LoadImageFromScreen();
            ExportImage(img, TextFormat("%s/screen_%d.png", std::getenv("LS_DEBUG_SIZE"), n));
            UnloadImage(img);
        }
        n++;
    }
    EndDrawing();

    game.sound.end_frame();
    game.states.apply_pending(game);
    capture_if_requested(game);
}

} // namespace

int main() {
    if (const char* runs = std::getenv("LS_HEADLESS")) {
        SetTraceLogLevel(LOG_INFO);
        return ls::run_headless(std::max(1, std::atoi(runs)));
    }
#ifdef __EMSCRIPTEN__
    // The browser build renders at CSS-pixel size: raylib's web backend sizes the canvas
    // backing store by devicePixelRatio under HIGHDPI without scaling its own viewport,
    // which shrank the game into a corner on Retina screens.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
#endif
    InitWindow(540, 960, "Last Stand - Horde Defense");
#ifdef __EMSCRIPTEN__
    fit_canvas();
    g_game_touch = EM_ASM_INT({ return window.matchMedia('(pointer: coarse)').matches ? 1 : 0; }) != 0;
#else
    int monitor = GetCurrentMonitor();
    int h = std::min(1100, static_cast<int>(static_cast<float>(GetMonitorHeight(monitor)) * 0.85f));
    if (h > 400) {
        SetWindowSize(h * 9 / 16, h);
        SetWindowPosition((GetMonitorWidth(monitor) - h * 9 / 16) / 2, (GetMonitorHeight(monitor) - h) / 2);
    }
#endif
    SetExitKey(KEY_NULL);
    SetTargetFPS(ls::TARGET_FPS);
    InitAudioDevice();

    auto game = std::make_unique<ls::Game>();
    g_game = game.get();
    game->canvas = LoadRenderTexture(ls::VIRTUAL_W, ls::VIRTUAL_H);
    SetTextureFilter(game->canvas.texture, TEXTURE_FILTER_BILINEAR);
    game->gfx.init();
    game->sound.init();
    ls::load_assets(*game);
    game->load();
    game->touch_ui = g_game_touch;

    game->states.register_state<ls::MenuState>();
    game->states.register_state<ls::PlayingState>();
    game->states.register_state<ls::ResultsState>();
    game->states.register_state<ls::ArmoryState>();
    game->states.register_state<ls::MissionsState>();
    game->states.request(ls::GameStateId::Menu);
    game->states.apply_pending(*game);

#ifndef __EMSCRIPTEN__
    if (const char* dir = std::getenv("LS_GALLERY")) {
        game->persist = false;
        game->meta = ls::MetaProgress{};
        game->meta.gems = 1234;
        game->meta.unlocked_stage = 5;
        game->meta.unit_levels = {4, 2, 0, 1, 0, 0};
        game->meta.loadout = {0, 1, 3};
        for (int page = 0; page < ls::gallery_page_count(); ++page) {
            game->input = {};
            ls::gallery_prepare(*game, page);
            BeginTextureMode(game->canvas);
            ClearBackground(BLACK);
            ls::render_gallery(*game, page);
            EndTextureMode();
            Image img = LoadImageFromTexture(game->canvas.texture);
            ImageFlipVertical(&img);
            ExportImage(img, TextFormat("%s/gallery_%02d.png", dir, page));
            UnloadImage(img);
        }
        ls::unload_ground();
        game->gfx.unload();
        UnloadRenderTexture(game->canvas);
        game.reset();
        CloseAudioDevice();
        CloseWindow();
        return 0;
    }
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose() && game->running) frame();
    game->save();
    ls::unload_ground();
    game->sound.cleanup();
    game->gfx.unload();
    UnloadRenderTexture(game->canvas);
    game.reset();
    CloseAudioDevice();
    CloseWindow();
#endif
    return 0;
}
