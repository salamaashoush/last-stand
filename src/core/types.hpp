#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <raylib.h>

namespace ls {

using EntityId = uint32_t;
using Gold = int32_t;
using WaveNum = uint32_t;

struct Vec2 {
    float x{}, y{};

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    auto operator<=>(const Vec2&) const = default;

    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const {
        float l = length();
        return l > 0.0001f ? Vec2{x / l, y / l} : Vec2{};
    }
    float distance_to(Vec2 o) const { return (*this - o).length(); }

    Vector2 to_raylib() const { return {x, y}; }
    static Vec2 from_raylib(Vector2 v) { return {v.x, v.y}; }
};

struct GridPos {
    int x{}, y{};
    auto operator<=>(const GridPos&) const = default;
    bool operator==(const GridPos&) const = default;
};

enum class TileType : uint8_t {
    Grass,
    Path,
    Blocked,
    Spawn,
    Exit,
    Buildable
};

enum class TowerType : uint8_t {
    Arrow,
    Cannon,
    Ice,
    Lightning,
    Poison,
    Laser
};

enum class EnemyType : uint8_t {
    Grunt,
    Runner,
    Tank,
    Healer,
    Flying,
    Boss
};

enum class EffectType : uint8_t {
    None,
    Slow,
    Poison,
    Burn,
    Stun
};

enum class DamageType : uint8_t {
    Physical,
    Magic,
    True
};

enum class AbilityId : uint8_t {
    Fireball,
    HealAura,
    LightningStrike
};

enum class GameStateId : uint8_t {
    Menu,
    MapSelect,
    Playing,
    Paused,
    GameOver,
    Victory
};

enum class Difficulty : uint8_t {
    Easy,
    Normal,
    Hard
};

enum class AbilityType : uint8_t {
    SpeedBurst,
    SpawnMinions,
    DamageAura
};

} // namespace ls
