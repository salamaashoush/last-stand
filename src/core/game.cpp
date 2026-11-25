#include "core/game.hpp"
#include "game/save.hpp"
#include "managers/storage.hpp"
#include <algorithm>
#include <ctime>

namespace ls {

namespace {
constexpr const char* SAVE_KEY = "laststand_save";
constexpr const char* NINJA = "assets/packs/ninja-adventure/audio/music/";
} // namespace

void Game::save() {
    if (persist) storage_write(SAVE_KEY, serialize_meta(meta));
}

void Game::load() {
    if (auto text = storage_read(SAVE_KEY)) meta = deserialize_meta(*text);
    apply_volumes();
}

void Game::apply_volumes() {
    sound.volume = meta.sfx_volume;
    if (music) SetMusicVolume(*music, meta.music_volume * 0.6f);
}

void Game::play_music(const char* name) {
    if (music_name == name) return;
    if (music) StopMusicStream(*music);
    music = assets.get_music(name);
    music_name = name;
    if (music) {
        music->looping = true;
        PlayMusicStream(*music);
        apply_volumes();
    }
}

void Game::update_music() {
    if (music) UpdateMusicStream(*music);
}

void Game::toast(std::string text, Color color) {
    toasts.push_back({std::move(text), color, 2.6f});
    if (toasts.size() > 4) toasts.erase(toasts.begin());
}

void Game::update_input() {
    float sw = static_cast<float>(GetScreenWidth());
    float sh = static_cast<float>(GetScreenHeight());
    float scale = std::min(sw / VIRTUAL_W, sh / VIRTUAL_H);
    viewport = {(sw - VIRTUAL_W * scale) * 0.5f, (sh - VIRTUAL_H * scale) * 0.5f, VIRTUAL_W * scale, VIRTUAL_H * scale};

    Vector2 raw = GetMousePosition();
    bool down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (GetTouchPointCount() > 0) {
        raw = GetTouchPosition(0);
        down = true;
        touch_ui = true;
    }
    Vec2 prev = input.pos;
    input.pos = {(raw.x - viewport.x) / scale, (raw.y - viewport.y) / scale};
    input.pressed = down && !input.down;
    input.released = !down && input.down;
    input.drag_dx = (down && input.down) ? input.pos.x - prev.x : 0.0f;
    input.down = down;
    input.consumed = false;
    if (input.pressed) input.press_pos = input.pos;
}

long long today_index() {
    return static_cast<long long>(std::time(nullptr)) / 86400LL;
}

void load_assets(Game& game) {
    auto& a = game.assets;
    auto mus = [&](const char* name, const char* file) {
        if (!a.load_music(name, std::string(NINJA) + file)) TraceLog(LOG_WARNING, "ASSET: missing music %s", file);
    };
    mus("menu", "theme_dream.ogg");
    mus("battle", "theme_plain.ogg");
    mus("battle2", "theme_swamp.ogg");
    mus("boss", "theme_lost_village.ogg");

    const std::string td = "assets/packs/kenney-tower-defense-top-down/PNG/Default size/towerDefense_tile";
    auto tex = [&](const char* name, const std::string& path) {
        if (auto t = a.load_texture(name, path)) {
            SetTextureFilter(*a.get_texture(name), TEXTURE_FILTER_BILINEAR);
        } else {
            TraceLog(LOG_WARNING, "ASSET: missing texture %s", path.c_str());
        }
    };
    tex("tree", td + "130.png");
    tex("bush", td + "131.png");
    tex("plane", td + "271.png");
    tex("plane_shadow", td + "294.png");

    const std::string pt = "assets/packs/kenney-particle-pack/PNG (Transparent)/";
    auto ptex = [&](const char* name, const char* file) {
        if (auto t = a.load_texture(name, pt + file)) {
            GenTextureMipmaps(a.get_texture(name));
            SetTextureFilter(*a.get_texture(name), TEXTURE_FILTER_TRILINEAR);
        }
    };
    ptex("p_smoke", "smoke_04.png");
    ptex("p_star", "star_06.png");
    ptex("p_scorch", "scorch_02.png");
}

} // namespace ls
