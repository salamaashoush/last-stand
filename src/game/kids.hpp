#pragma once
#include "core/gfx.hpp"
#include <cstdint>

// Omar, Ali, and Dad Salama: the family heroes defending the wall!
// Omar captains the squad with Yalla Rally; Ali charms the horde with his peace sign;
// Salama throws giant spinning, laughing Salama heads that bounce and explode!
namespace ls {

enum class Kid : uint8_t { Omar, Ali, Salama };
enum class KidPose : uint8_t { Idle, Wave, Cheer, Rally, Peace, BabaThrow };

void draw_kid(const Gfx& g, Kid k, Vec2 feet, float scale, KidPose pose, float time);
void draw_kid_portrait(const Gfx& g, Kid k, Vec2 centre, float radius, float time, bool happy);
void draw_salama_head(const Gfx& g, Vec2 centre, float radius, float rot_deg, float time, float puff = 0.0f);
void draw_heart(Vec2 centre, float size, Color col);

} // namespace ls
