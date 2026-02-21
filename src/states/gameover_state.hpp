#pragma once
#include "core/state_machine.hpp"

namespace ls {

class GameOverState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    GameStateId id() const override { return GameStateId::GameOver; }
    void update(Game& game, float dt) override;
    void render(Game& game) override;

  private:
    int xp_earned_{0};
};

class VictoryState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    GameStateId id() const override { return GameStateId::Victory; }
    void update(Game& game, float dt) override;
    void render(Game& game) override;

  private:
    int xp_earned_{0};
};

} // namespace ls
