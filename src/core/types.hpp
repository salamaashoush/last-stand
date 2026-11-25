#pragma once
#include <cmath>
#include <cstdint>
#include <raylib.h>

namespace ls {

struct Vec2 {
    float x{}, y{};

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    bool operator==(const Vec2&) const = default;

    float length() const { return std::sqrt(x * x + y * y); }
    float length_sq() const { return x * x + y * y; }
    Vec2 normalized() const {
        float l = length();
        return l > 0.0001f ? Vec2{x / l, y / l} : Vec2{};
    }
    float distance_to(Vec2 o) const { return (*this - o).length(); }
    float dot(Vec2 o) const { return x * o.x + y * o.y; }

    Vector2 to_raylib() const { return {x, y}; }
    static Vec2 from_raylib(Vector2 v) { return {v.x, v.y}; }
};

enum class EnemyType : uint8_t { Walker, Runner, Brute, Exploder, Spitter, Shielder, Splitter, Boss, Count };

enum class TurretType : uint8_t { None, Cannon, Tesla, Flamer, Laser, Count };

enum class GateKind : uint8_t { Add, Mul };

enum class GameStateId : uint8_t { Menu, Playing, Results, Armory, Missions };

} // namespace ls
