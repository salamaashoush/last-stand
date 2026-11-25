#pragma once
#include "core/constants.hpp"
#include "core/gfx.hpp"
#include "core/state_machine.hpp"
#include "game/meta.hpp"
#include "game/world.hpp"
#include "managers/asset_manager.hpp"
#include "managers/sound_manager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ls {

struct Input {
    Vec2 pos{};
    Vec2 press_pos{};
    bool down{false};
    bool pressed{false};
    bool released{false};
    bool consumed{false};
    float drag_dx{};
};

struct RunRequest {
    int stage{1};
    bool endless{false};
};

struct RunResult {
    bool won{false};
    bool endless{false};
    int stage{1};
    int waves_cleared{0};
    int stars{0};
    int gems{0};
    int new_stars{0};
    bool new_unlock{false};
    RunStatsTally tally;
};

struct Toast {
    std::string text;
    Color color{WHITE};
    float timer{};
};

struct Game {
    StateMachine states;
    AssetManager assets;
    SoundManager sound;
    Gfx gfx;
    MetaProgress meta;
    std::unique_ptr<Run> run;
    RunRequest request;
    RunResult result;
    Input input;
    RenderTexture2D canvas{};
    Rectangle viewport{};
    std::vector<Toast> toasts;
    Music* music{nullptr};
    std::string music_name;
    float time{};
    bool running{true};
    bool paused{false};
    bool persist{true};
    bool touch_ui{false};

    void save();
    void load();
    void play_music(const char* name);
    void update_music();
    void apply_volumes();
    void toast(std::string text, Color color);
    void update_input();
};

void load_assets(Game& game);
long long today_index();

} // namespace ls
