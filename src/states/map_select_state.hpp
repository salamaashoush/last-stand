#pragma once
#include "core/state_machine.hpp"

namespace ls {

class MapSelectState : public IGameState {
public:
    void enter(Game&) override;
    void exit(Game&) override {}
    GameStateId id() const override { return GameStateId::MapSelect; }
    void update(Game& game, float dt) override;
    void render(Game& game) override;

private:
    int selected_{0};
};

} // namespace ls
