#pragma once
#include <array>
#include <cstdint>
#include <raylib.h>
#include <vector>

namespace ls {

enum class Sfx : uint8_t {
    Shot,
    ShotHeavy,
    Hit,
    Splat,
    Explosion,
    BigExplosion,
    GateGood,
    GateBad,
    GateTick,
    LevelUp,
    CardPick,
    Coin,
    WallHit,
    Cannon,
    Zap,
    Laser,
    Flame,
    BossRoar,
    Victory,
    Defeat,
    Click,
    Build,
    Gem,
    IceHit,
    IceBreak,
    Count
};

// Every effect is synthesized at startup, so the game ships no sound files of its own.
// Each effect owns a few aliases of one buffer so overlapping plays do not cut each other off.
class SoundManager {
  public:
    void init();
    void cleanup();
    void play(Sfx s, float volume = 1.0f, float pitch = 1.0f);
    void end_frame();

    float volume{0.8f};

  private:
    static constexpr int VOICES = 6;
    struct Slot {
        Sound base{};
        std::array<Sound, VOICES> voices{};
        int next{0};
        int plays_this_frame{0};
        int max_per_frame{2};
        bool loaded{false};
    };
    std::array<Slot, static_cast<size_t>(Sfx::Count)> slots_{};

    void set(Sfx s, const std::vector<float>& samples, int max_per_frame = 2);
};

} // namespace ls
