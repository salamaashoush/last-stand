#pragma once
#include "core/state_machine.hpp"
#include "core/constants.hpp"
#include <raylib.h>

namespace ls {

class MenuState : public IGameState {
public:
    void enter(Game&) override {}
    void exit(Game&) override {}
    GameStateId id() const override { return GameStateId::Menu; }

    void update(Game& game, float dt) override;
    void render(Game& game) override;

private:
    int selected_{0};
    float title_pulse_{0.0f};
};

} // namespace ls
