#pragma once
#include "core/state_machine.hpp"

namespace ls {

class PlayingState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game& game) override;
    GameStateId id() const override { return GameStateId::Playing; }
    void update(Game& game, float dt) override;
    void render(Game& game) override;

  private:
    void handle_input(Game& game);
    void setup_event_handlers(Game& game);
};

} // namespace ls
