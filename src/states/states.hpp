#pragma once
#include "core/state_machine.hpp"
#include "core/types.hpp"
#include <vector>

namespace ls {

class MenuState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    void update(Game& game, float dt) override;
    void render(Game& game) override;
    GameStateId id() const override { return GameStateId::Menu; }

  private:
    struct Walker {
        Vec2 pos;
        int type;
        float anim;
        float hp;
    };
    struct Tracer {
        Vec2 pos;
        float life;
    };
    std::vector<Walker> walkers_;
    std::vector<Tracer> tracers_;
    float spawn_timer_{};
    float fire_timer_{};
    int selected_stage_{1};
    bool daily_open_{false};
    int daily_amount_{0};
};

class PlayingState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game& game) override;
    void update(Game& game, float dt) override;
    void render(Game& game) override;
    GameStateId id() const override { return GameStateId::Playing; }
};

class ResultsState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    void update(Game& game, float dt) override;
    void render(Game& game) override;
    GameStateId id() const override { return GameStateId::Results; }

  private:
    float t_{};
    float gems_shown_{};
};

class ArmoryState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    void update(Game&, float) override {}
    void render(Game& game) override;
    GameStateId id() const override { return GameStateId::Armory; }
    void set_tab(int tab) { tab_ = tab; }

  private:
    int tab_{0};
    int focus_{0};
};

class MissionsState : public IGameState {
  public:
    void enter(Game& game) override;
    void exit(Game&) override {}
    void update(Game&, float) override {}
    void render(Game& game) override;
    GameStateId id() const override { return GameStateId::Missions; }
};

} // namespace ls
