#pragma once
#include "core/gfx.hpp"
#include <cstdint>

// Omar and Ali, the two heroes of the wall, drawn as cartoon kids. Omar captains the squad
// and rallies it; Ali charms the horde with his peace sign.
namespace ls {

enum class Kid : uint8_t { Omar, Ali };
enum class KidPose : uint8_t { Idle, Wave, Cheer, Rally, Peace };

void draw_kid(const Gfx& g, Kid k, Vec2 feet, float scale, KidPose pose, float time);
void draw_kid_portrait(const Gfx& g, Kid k, Vec2 centre, float radius, float time, bool happy);
void draw_heart(Vec2 centre, float size, Color col);

} // namespace ls
