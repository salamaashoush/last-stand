#pragma once
#include <entt/entt.hpp>
#include "types.hpp"

namespace ls {

struct EnemyDeathEvent {
    entt::entity entity;
    EnemyType type;
    Gold reward;
    Vec2 position;
};

struct EnemyReachedExitEvent {
    entt::entity entity;
    int damage{1};
};

struct TowerPlacedEvent {
    entt::entity entity;
    TowerType type;
    GridPos pos;
};

struct TowerSoldEvent {
    entt::entity entity;
    TowerType type;
    Gold refund;
};

struct WaveStartEvent {
    WaveNum wave;
};

struct WaveCompleteEvent {
    WaveNum wave;
};

struct DamageDealtEvent {
    entt::entity target;
    int amount;
    Vec2 position;
};

struct HeroLevelUpEvent {
    int new_level;
};

struct GameOverEvent {};
struct VictoryEvent {};

using EventDispatcher = entt::dispatcher;

} // namespace ls
