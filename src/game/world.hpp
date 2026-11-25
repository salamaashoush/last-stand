#pragma once
#include "core/constants.hpp"
#include "core/rng.hpp"
#include "core/types.hpp"
#include "game/balance.hpp"
#include "game/cards.hpp"
#include "game/gates.hpp"
#include "game/theme.hpp"
#include "game/units.hpp"
#include <array>
#include <string>
#include <vector>

namespace ls {

enum PTex : int { PDisc, PSmoke, PFire, PGlow, PStar, PRing, PTexCount };

inline Color enemy_body_color(EnemyType t) {
    switch (t) {
    case EnemyType::Walker:
        return {122, 178, 92, 255};
    case EnemyType::Runner:
        return {214, 96, 84, 255};
    case EnemyType::Brute:
        return {120, 104, 150, 255};
    case EnemyType::Exploder:
        return {198, 210, 70, 255};
    case EnemyType::Spitter:
        return {170, 92, 196, 255};
    case EnemyType::Shielder:
        return {96, 140, 150, 255};
    case EnemyType::Splitter:
        return {230, 140, 60, 255};
    case EnemyType::Flyer:
        return {96, 70, 120, 255};
    case EnemyType::Barrel:
        return {200, 60, 40, 255};
    case EnemyType::Boss:
        return {150, 60, 60, 255};
    case EnemyType::Count:
        break;
    }
    return {255, 255, 255, 255};
}

inline Color enemy_blood_color(EnemyType t) {
    switch (t) {
    case EnemyType::Exploder:
        return {170, 200, 40, 255};
    case EnemyType::Spitter:
        return {140, 60, 170, 255};
    default:
        return {120, 20, 24, 255};
    }
}

struct Enemy {
    EnemyType type{};
    Vec2 pos{};
    float hp{};
    float max_hp{};
    float speed{};
    float radius{};
    float wall_dps{};
    float knock_resist{};
    int xp{};
    int coins{};
    bool mini_boss{false};
    bool elite{false};
    bool at_wall{false};
    bool dead{false};
    float anim{};
    float flash{};
    float slow_timer{};
    float slow_factor{1.0f};
    float burn_timer{};
    float burn_dps{};
    float knock{};
    float action_timer{};
    float stop_y{};
    float wobble_seed{};
    float shield_hp{};
    float shield_max{};
    float armor{};
    float shield_flash{};
    float power_timer{};
    float hurt{};
    float charm{};
    float dash{};
    bool enraged{false};
};

struct Bullet {
    Vec2 pos{};
    Vec2 prev{};
    Vec2 vel{};
    float damage{};
    int pierce{};
    int ricochet{};
    int last_hit{-1};
    bool alive{true};
    bool crit{false};
    float radius{BULLET_RADIUS};
    float life{1.4f};
    float splash{};
    float knock{14.0f};
    bool shard{false};
    bool flame{false};
    UnitType unit{UnitType::Rifleman};
};

struct Gate {
    GateState state{};
    float x{};
    float y{};
    float w{GATE_W};
    int row{};
    float pulse{};
    float hit_flash{};
    bool alive{true};
};

enum class LootKind : uint8_t { Soldiers, Turret, Card, Coins, Gems };

// A reward sealed in the stage's lock material (ice, crate, bricks, cage, crystal) drifting
// through the horde; every bullet spent breaking it open is a bullet not spent on enemies.
struct Loot {
    Vec2 pos{};
    float hp{};
    float max_hp{};
    LootKind kind{LootKind::Coins};
    LockKind lock{LockKind::Crate};
    UnitType unit{UnitType::Rifleman};
    TurretType turret{TurretType::Cannon};
    int amount{};
    float hit_flash{};
    float seed{};
    bool alive{true};
};

inline constexpr float LOOT_SIZE = 70.0f;
inline constexpr float LOOT_SPEED = 42.0f;

struct Shell {
    Vec2 from{};
    Vec2 to{};
    float t{};
    float duration{0.6f};
    float damage{};
    float radius{};
    bool bomb{false};
    bool grenade{false};
};

struct Acid {
    Vec2 pos{};
    Vec2 vel{};
    float damage{};
    bool alive{true};
};

struct Bolt {
    std::vector<Vec2> points;
    float life{0.18f};
    float max_life{0.18f};
    Color color{WHITE};
    float width{3.0f};
};

struct Particle {
    Vec2 pos{};
    Vec2 vel{};
    float life{};
    float max_life{};
    float size{};
    float size_end{};
    Color color{WHITE};
    float drag{2.0f};
    float gravity{0.0f};
    int tex{0};
    float rot{};
    float spin{};
    float z{};
    float vz{};
    float zgrav{};
    bool additive{false};
};

struct FloatText {
    std::string text;
    Vec2 pos{};
    Color color{WHITE};
    float life{};
    float max_life{};
    float size{};
    float vy{-60.0f};
};

struct CoinFly {
    Vec2 pos{};
    Vec2 vel{};
    float t{};
    int value{};
};

// A boss does not vanish on its last hit: it shudders, cracks open with explosions and then
// bursts, while the run carries on around it.
struct BossDeath {
    Vec2 pos{};
    float radius{};
    float t{};
    bool mini{false};
    int gems{};
    float next_blast{};
};

// A fallen soldier tumbling away from the squad.
struct Corpse {
    Vec2 pos{};
    Vec2 vel{};
    float rot{};
    float spin{};
    float z{};
    float vz{};
    float life{};
    UnitType unit{UnitType::Rifleman};
};

struct Decal {
    Vec2 pos{};
    float radius{};
    Color color{};
    float rot{};
    int kind{};
};

struct Turret {
    TurretType type{TurretType::None};
    int level{0};
    float cooldown{};
    float aim{-PI / 2};
    float recoil{};
    float flame_timer{};
    int target{-1};
};

enum class WavePhase : uint8_t { Intro, Fighting, Break, Won, Lost };

struct Banner {
    std::string title;
    std::string subtitle;
    Color color{WHITE};
    float timer{};
    float duration{};
};

struct RunStatsTally {
    int kills{};
    int bosses{};
    int elites{};
    int coins_earned{};
    int gates_taken{};
    int max_soldiers{};
    int cards_taken{};
    int max_combo{};
    float time{};
    double damage_dealt{};
};

struct Run {
    int stage{1};
    int wave{0};
    WavePhase phase{WavePhase::Intro};
    float phase_timer{2.2f};
    float wave_time{};
    std::vector<SpawnEvent> spawns;
    size_t spawn_cursor{};
    bool endless{false};
    bool finished{false};
    bool wave_theme_pending{false};
    bool ground_reset{false};
    WaveMod mod{WaveMod::None};
    Theme theme;
    BossGenome boss;
    BossGenome mini_boss;
    float threat{1.0f};
    float start_power{100.0f};
    int threat_tier{0};
    float wall_damage_rate{};
    float wave_wall_start{};

    RunStats stats;
    std::vector<UnitType> squad{UnitType::Rifleman};
    std::array<int, UNIT_COUNT> unit_levels{};
    std::array<float, UNIT_COUNT> fire_accum_by_unit{};
    std::array<int, UNIT_COUNT> fire_cursor_by_unit{};
    int soldiers{5};
    float soldier_pop{};
    float squad_x{VIRTUAL_W * 0.5f};
    float squad_target_x{VIRTUAL_W * 0.5f};
    float fire_accum{};
    int fire_cursor{};
    std::array<float, 64> soldier_recoil{};
    std::array<float, 64> soldier_join{};
    int shown_soldiers{0};

    float wall_hp{1000};
    float wall_flash{};
    int coins{};
    int level{1};
    int xp{};
    int pending_levels{};
    bool picking{false};
    float pick_anim{};
    std::array<CardId, 3> offer{};
    int rerolls{};
    float airstrike_cd{8.0f};
    float rage{};
    float frenzy_t{};
    float ali_charge{0.5f};
    float ali_t{-1.0f};
    float chain_timer{};
    std::array<Turret, TURRET_SLOTS> turrets{};
    int build_slot{-1};
    int build_focus{0};
    int repairs{0};
    int pick_focus{1};

    float gate_timer{5.0f};
    int gate_rows{};
    int combo{};
    float combo_timer{};

    float plane_t{-1.0f};
    float plane_y{};
    float plane_bomb_timer{};
    int plane_bombs_left{};
    float shot_sound_cd{};
    int gems_found{};
    std::string hint;
    float hint_timer{};
    bool gate_hint_shown{false};
    bool build_hint_shown{false};
    float boss_banner_hp{};
    float card_delay{};

    float shake{};
    float hitstop{};
    float slowmo{};
    float flash{};
    float time{};

    Banner banner;
    Banner pending_boss_banner;
    RunStatsTally tally;

    std::vector<Enemy> enemies;
    std::vector<Enemy> new_enemies;
    std::vector<Bullet> bullets;
    std::vector<Bullet> new_bullets;
    std::vector<Gate> gates;
    std::vector<BossDeath> boss_deaths;
    std::vector<Corpse> corpses;
    float collapse_t{-1.0f};
    float wall_jolt{};
    float zoom_kick{};
    std::vector<Loot> loot;
    float loot_timer{16.0f};
    std::vector<Shell> shells;
    std::vector<Acid> acids;
    std::vector<Bolt> bolts;
    std::vector<Particle> particles;
    std::vector<FloatText> texts;
    std::vector<CoinFly> coin_flies;
    std::vector<Decal> pending_decals;

    Rng rng{0xC0FFEEull};

    bool boss_alive() const {
        for (const auto& e : enemies)
            if (!e.dead && e.type == EnemyType::Boss) return true;
        return false;
    }
};

} // namespace ls
