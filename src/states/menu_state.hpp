#pragma once
#include "core/constants.hpp"
#include "core/state_machine.hpp"
#include <raylib.h>
#include <vector>

namespace ls {

class MenuState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    GameStateId id() const override { return GameStateId::Menu; }

    void update(Game& game, float dt) override;
    void render(Game& game) override;

  private:
    struct MenuItem {
        const char* label;
        enum Action { ResumeGame, NewGame, LoadGame, Upgrades, Quit } action;
    };

    int selected_{0};
    float title_pulse_{0.0f};
    float load_error_flash_{0.0f};
    std::vector<MenuItem> items_;
};

} // namespace ls
