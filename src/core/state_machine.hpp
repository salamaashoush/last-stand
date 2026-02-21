#pragma once
#include <memory>
#include <unordered_map>
#include <concepts>
#include "types.hpp"

namespace ls {

struct Game;

class IGameState {
public:
    virtual ~IGameState() = default;
    virtual void enter(Game& game) = 0;
    virtual void exit(Game& game) = 0;
    virtual void update(Game& game, float dt) = 0;
    virtual void render(Game& game) = 0;
    virtual GameStateId id() const = 0;
};

template<typename T>
concept GameStateLike = std::derived_from<T, IGameState> && requires(T t) {
    { t.id() } -> std::same_as<GameStateId>;
};

class StateMachine {
public:
    template<GameStateLike T, typename... Args>
    void register_state(Args&&... args) {
        auto state = std::make_unique<T>(std::forward<Args>(args)...);
        auto sid = state->id();
        states_[sid] = std::move(state);
    }

    void change_state(GameStateId new_state, Game& game) {
        if (current_) current_->exit(game);
        current_ = states_.at(new_state).get();
        current_->enter(game);
    }

    void update(Game& game, float dt) {
        if (current_) current_->update(game, dt);
    }

    void render(Game& game) {
        if (current_) current_->render(game);
    }

    GameStateId current_id() const {
        return current_ ? current_->id() : GameStateId::Menu;
    }

private:
    std::unordered_map<GameStateId, std::unique_ptr<IGameState>> states_;
    IGameState* current_{nullptr};
};

} // namespace ls
