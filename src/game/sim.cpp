#include "game/sim.hpp"
#include "core/game.hpp"
#include "game/autopilot.hpp"
#include "game/formation.hpp"
#include "game/turrets.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <format>

namespace ls {

namespace {

constexpr float DEG = PI / 180.0f;
constexpr Vec2 COIN_HUD{64.0f, 52.0f};

// Uniform grid rebuilt every frame; enemies are the only thing queried spatially.
struct Grid {
    static constexpr int CELL = 64;
    static constexpr int GW = VIRTUAL_W / CELL + 1;
    static constexpr int GH = VIRTUAL_H / CELL + 2;
    std::vector<int> head = std::vector<int>(GW * GH, -1);
    std::vector<int> next;

    static int cell_of(float v, int max) { return std::clamp(static_cast<int>(v) / CELL, 0, max - 1); }

    void build(const std::vector<Enemy>& es) {
        std::ranges::fill(head, -1);
        next.assign(es.size(), -1);
        for (int i = 0; i < static_cast<int>(es.size()); ++i) {
            if (es[static_cast<size_t>(i)].dead) continue;
            int cx = cell_of(es[static_cast<size_t>(i)].pos.x, GW);
            int cy = cell_of(es[static_cast<size_t>(i)].pos.y + CELL, GH);
            int c = cy * GW + cx;
            next[static_cast<size_t>(i)] = head[static_cast<size_t>(c)];
            head[static_cast<size_t>(c)] = i;
        }
    }

    template <typename F>
    void query(Vec2 p, float r, F&& fn) const {
        int x0 = cell_of(p.x - r, GW), x1 = cell_of(p.x + r, GW);
        int y0 = cell_of(p.y - r + CELL, GH), y1 = cell_of(p.y + r + CELL, GH);
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                for (int i = head[static_cast<size_t>(cy * GW + cx)]; i != -1; i = next[static_cast<size_t>(i)]) {
                    fn(i);
                }
            }
        }
    }
};

Grid g_grid;
constexpr float MAX_ENEMY_R = 90.0f;

struct Ctx {
    Game& game;
    Run& run;
};

float wall_damage_mult(const Run& run) {
    auto d = difficulty_for(run);
    return std::pow(1.15f, static_cast<float>(d.stage - 1)) * (1.0f + 0.06f * static_cast<float>(d.wave - 1));
}

void add_shake(Run& run, float amount) {
    run.shake = std::min(28.0f, std::max(run.shake, amount));
}

void particle(Run& run, Vec2 pos, Vec2 vel, float life, float size, float size_end, Color c, int tex, float drag = 2.0f,
              float gravity = 0.0f) {
    if (run.particles.size() > 2200) return;
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.max_life = life;
    p.size = size;
    p.size_end = size_end;
    p.color = c;
    p.tex = tex;
    p.drag = drag;
    p.gravity = gravity;
    p.rot = run.rng.uniform(0, 360);
    p.spin = run.rng.uniform(-200, 200);
    run.particles.push_back(p);
}

void burst(Run& run, Vec2 pos, int count, Color c, float speed, float size, int tex = PDisc, float life = 0.5f) {
    for (int i = 0; i < count; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(0.3f, 1.0f) * speed;
        particle(run, pos, {std::cos(a) * s, std::sin(a) * s}, life * run.rng.uniform(0.6f, 1.2f), size, 0.0f, c, tex,
                 4.0f);
    }
}

void float_text(Run& run, std::string t, Vec2 pos, Color c, float size, float life = 0.8f) {
    if (run.texts.size() > 70) return;
    run.texts.push_back({std::move(t), pos, c, life, life, size, -70.0f});
}

std::string short_num(double v) {
    if (v >= 1e9) return std::format("{:.1f}B", v / 1e9);
    if (v >= 1e6) return std::format("{:.1f}M", v / 1e6);
    if (v >= 1e4) return std::format("{:.1f}K", v / 1e3);
    return std::format("{}", static_cast<long long>(std::round(v)));
}

void additive_particle(Run& run, Vec2 pos, Vec2 vel, float life, float size, float size_end, Color c, int tex,
                       float drag = 2.0f) {
    particle(run, pos, vel, life, size, size_end, c, tex, drag);
    if (!run.particles.empty()) run.particles.back().additive = true;
}

// Chunks fly up in a fake height axis, land, bounce once and settle, which sells weight.
void gib(Run& run, Vec2 pos, Color c, float size, float speed, float up, float life = 1.1f) {
    if (run.particles.size() > 2200) return;
    float a = run.rng.uniform(0, 2 * PI);
    float s = run.rng.uniform(0.35f, 1.0f) * speed;
    Particle p;
    p.pos = pos;
    p.vel = {std::cos(a) * s, std::sin(a) * s * 0.6f};
    p.life = p.max_life = life * run.rng.uniform(0.8f, 1.2f);
    p.size = p.size_end = size * run.rng.uniform(0.7f, 1.3f);
    p.color = c;
    p.tex = PDisc;
    p.drag = 1.0f;
    p.z = 2.0f;
    p.vz = up * run.rng.uniform(0.6f, 1.2f);
    p.zgrav = 1100.0f;
    p.rot = run.rng.uniform(0, 360);
    p.spin = run.rng.uniform(-400, 400);
    run.particles.push_back(p);
}

void shockwave(Run& run, Vec2 pos, float from, float to, float life, Color c) {
    additive_particle(run, pos, {}, life, from, to, c, PRing, 0.0f);
}

void explosion_fx(Run& run, Vec2 pos, float radius, Color tint) {
    float k = radius / 60.0f;
    additive_particle(run, pos, {}, 0.16f, radius * 0.6f, radius * 1.9f, Color{255, 250, 225, 255}, PGlow, 0.0f);
    shockwave(run, pos, radius * 0.3f, radius * 1.4f, 0.26f, Color{255, 230, 180, 150});
    for (int i = 0; i < 9; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(30, 120) * k;
        Color fire = mix(tint, Color{255, 230, 120, 255}, run.rng.unit());
        additive_particle(run, pos + Vec2{std::cos(a), std::sin(a)} * radius * 0.15f,
                          {std::cos(a) * s, std::sin(a) * s}, run.rng.uniform(0.22f, 0.42f), radius * 0.32f,
                          radius * 0.62f, fire, PFire, 3.0f);
    }
    for (int i = 0; i < 12; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(160, 420) * k;
        additive_particle(run, pos, {std::cos(a) * s, std::sin(a) * s}, run.rng.uniform(0.3f, 0.6f), 3.5f, 1.0f,
                          Color{255, 210, 120, 255}, PGlow, 2.5f);
    }
    for (int i = 0; i < 5; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(15, 50);
        particle(run, pos + Vec2{std::cos(a), std::sin(a)} * radius * 0.3f, {std::cos(a) * s, std::sin(a) * s - 40},
                 run.rng.uniform(0.6f, 0.95f), radius * 0.28f, radius * 0.75f, Color{52, 46, 44, 150}, PSmoke, 1.5f);
    }
    for (int i = 0; i < 5; ++i) gib(run, pos, Color{70, 60, 52, 255}, 3.0f, 260.0f * k, 260.0f, 0.8f);
    run.pending_decals.push_back({pos, radius * 0.8f, Color{30, 24, 20, 90}, run.rng.uniform(0, 360), 1});
}

void death_fx(Run& run, Vec2 pos, float radius, Color body, Color blood, bool big) {
    additive_particle(run, pos, {}, 0.12f, radius * 0.8f, radius * 1.8f, brighten(body, 0.6f), PGlow, 0.0f);
    int chunks = big ? 16 : 6;
    for (int i = 0; i < chunks; ++i) {
        Color c = run.rng.chance(0.5f) ? body : darken(body, 0.3f);
        gib(run, pos + Vec2{run.rng.uniform(-radius, radius) * 0.4f, run.rng.uniform(-radius, radius) * 0.4f}, c,
            radius * 0.2f, 110.0f + radius * 2.5f, 200.0f + radius * 4.0f);
    }
    for (int i = 0; i < (big ? 18 : 8); ++i) gib(run, pos, blood, radius * 0.09f + 1.5f, 200.0f, 150.0f, 0.6f);
}

void kill_enemy(Ctx& c, int idx);
Enemy& spawn_enemy(Run& run, const SpawnEvent& ev);
void damage_wall(Ctx& c, float amount, Vec2 at);

// Every damage source funnels through here so status effects, numbers and kills stay consistent.
void damage_enemy(Ctx& c, int idx, float dmg, bool crit, bool status, float knock = 18.0f, bool show = true) {
    auto& e = c.run.enemies[static_cast<size_t>(idx)];
    if (e.dead) return;
    if (status && e.armor > 0.0f) dmg *= 1.0f - e.armor;
    e.hp -= dmg;
    e.flash = 0.09f;
    e.hurt = std::min(1.0f, e.hurt + 0.35f);
    e.knock += knock * (1.0f - e.knock_resist);
    c.run.tally.damage_dealt += dmg;
    if (status) {
        const auto& s = c.run.stats;
        if (s.burn_ratio > 0.0f) {
            e.burn_timer = 2.5f;
            e.burn_dps = std::max(e.burn_dps, s.burn_dps());
        }
        if (s.slow > 0.0f) {
            e.slow_timer = 3.0f;
            e.slow_factor = 1.0f - s.slow;
        }
    }
    if (show && (crit || c.run.rng.chance(0.3f))) {
        Vec2 p = e.pos + Vec2{c.run.rng.uniform(-10, 10), -e.radius};
        if (crit) {
            float_text(c.run, short_num(dmg) + "!", p, Color{255, 220, 60, 255}, 30.0f);
        } else {
            float_text(c.run, short_num(dmg), p, WHITE, 20.0f, 0.55f);
        }
    }
    if (e.hp <= 0.0f) kill_enemy(c, idx);
}

void explode(Ctx& c, Vec2 pos, float radius, float dmg, bool status, Color tint, bool big_sound = false) {
    explosion_fx(c.run, pos, radius, tint);
    add_shake(c.run, std::min(14.0f, radius * 0.12f));
    c.game.sound.play(big_sound ? Sfx::BigExplosion : Sfx::Explosion, big_sound ? 1.0f : 0.55f,
                      c.run.rng.uniform(0.9f, 1.1f));
    std::vector<int> hits;
    g_grid.query(pos, radius + MAX_ENEMY_R, [&](int i) {
        const auto& e = c.run.enemies[static_cast<size_t>(i)];
        if (!e.dead && e.pos.distance_to(pos) < radius + e.radius) hits.push_back(i);
    });
    for (int i : hits) damage_enemy(c, i, dmg, false, status, 40.0f, false);
}

void spawn_coins(Run& run, Vec2 pos, int value) {
    if (value <= 0) return;
    if (run.coin_flies.size() > 80) {
        run.coins += value;
        run.tally.coins_earned += value;
        return;
    }
    int pieces = std::min(value, 5);
    for (int i = 0; i < pieces; ++i) {
        int v = value / pieces + (i < value % pieces ? 1 : 0);
        float a = run.rng.uniform(-PI, 0);
        float s = run.rng.uniform(80, 220);
        run.coin_flies.push_back({pos, {std::cos(a) * s, std::sin(a) * s}, 0.0f, v});
    }
}

void kill_enemy(Ctx& c, int idx) {
    auto& run = c.run;
    auto& e = run.enemies[static_cast<size_t>(idx)];
    if (e.dead) return;
    e.dead = true;
    Vec2 pos = e.pos;
    EnemyType type = e.type;
    float radius = e.radius;
    bool was_burning = e.burn_timer > 0.0f;
    bool was_slowed = e.slow_timer > 0.0f;
    bool elite = e.elite;
    auto d = difficulty_for(run);

    run.tally.kills++;
    if (run.frenzy_t <= 0.0f) run.rage = std::min(1.0f, run.rage + (type == EnemyType::Boss ? 0.25f : 0.004f));
    run.combo++;
    run.combo_timer = 2.2f;
    run.tally.max_combo = std::max(run.tally.max_combo, run.combo);
    if (run.combo == 50 || run.combo == 100 || run.combo == 250 || run.combo == 500 || run.combo == 1000) {
        const char* words = run.combo >= 500   ? "UNSTOPPABLE!"
                            : run.combo >= 250 ? "MASSACRE!"
                            : run.combo >= 100 ? "RAMPAGE!"
                                               : "KILLING SPREE!";
        int bonus = run.combo / 2;
        float_text(run, std::format("{} +{}", words, bonus), {VIRTUAL_W * 0.5f, 560}, Color{255, 200, 60, 255}, 44.0f,
                   1.4f);
        run.coins += bonus;
        run.tally.coins_earned += bonus;
        c.game.sound.play(Sfx::LevelUp, 0.6f, 1.3f);
    }

    run.xp += static_cast<int>(std::ceil(static_cast<float>(e.xp) * xp_mult(d.wave)));
    int coins = static_cast<int>(std::round(static_cast<float>(e.coins) * run.stats.coin_mult));
    spawn_coins(run, pos, coins);

    Color blood = enemy_blood_color(type);
    Color body = enemy_body_color(type);
    if (run.rng.chance(type == EnemyType::Walker || type == EnemyType::Runner ? 0.5f : 1.0f)) {
        run.pending_decals.push_back(
            {pos, radius * run.rng.uniform(0.9f, 1.3f), with_alpha(blood, 0.4f), run.rng.uniform(0, 360), 0});
    }
    death_fx(run, pos, radius, body, blood, type == EnemyType::Boss || type == EnemyType::Brute || elite);
    c.game.sound.play(Sfx::Splat, 0.35f, run.rng.uniform(0.8f, 1.3f));

    if (elite) {
        run.tally.elites++;
        int gems = 1 + d.stage / 3;
        run.gems_found += gems;
        float_text(run, std::format("+{} GEM", gems), pos, Color{120, 230, 255, 255}, 30.0f, 1.2f);
        c.game.sound.play(Sfx::Gem, 0.8f);
        shockwave(run, pos, radius * 0.5f, radius * 3.0f, 0.4f, Color{255, 215, 90, 170});
        for (int i = 0; i < 14; ++i) {
            float a = run.rng.uniform(0, 2 * PI);
            float s = run.rng.uniform(120, 320);
            additive_particle(run, pos, {std::cos(a) * s, std::sin(a) * s}, run.rng.uniform(0.5f, 0.9f), 9.0f, 2.0f,
                              i % 2 ? Color{120, 230, 255, 255} : Color{255, 220, 90, 255}, PStar, 3.0f);
        }
    }

    if (type == EnemyType::Splitter) {
        float child_hp = e.max_hp * 0.22f;
        for (int k = 0; k < 3; ++k) {
            Enemy child;
            child.type = EnemyType::Runner;
            child.pos = pos + Vec2{static_cast<float>(k - 1) * radius * 0.8f, run.rng.uniform(-6, 6)};
            child.hp = child.max_hp = child_hp;
            child.speed = base_stats(EnemyType::Runner).speed * 1.1f;
            child.radius = 13.0f;
            child.wall_dps = 2.0f * wall_damage_mult(run);
            child.xp = 0;
            child.coins = 0;
            child.knock = 80.0f;
            child.wobble_seed = run.rng.uniform(0, 100);
            run.new_enemies.push_back(child);
        }
        burst(run, pos, 10, Color{255, 170, 80, 255}, 220.0f, 5.0f);
    }
    if (type == EnemyType::Barrel) {
        explode(c, pos, 135.0f, std::max(run.stats.damage * 18.0f, e.max_hp * 25.0f), true, Color{255, 140, 40, 255},
                true);
        float_text(run, "BOOM!", pos + Vec2{0, -30}, Color{255, 200, 80, 255}, 40.0f, 1.0f);
    }
    if (type == EnemyType::Exploder) {
        explode(c, pos, 95.0f, run.stats.damage * 6.0f, false, Color{200, 230, 80, 255});
    }
    if (run.stats.inferno && was_burning) {
        explode(c, pos, 62.0f, run.stats.burn_dps() * 1.5f, true, Color{255, 130, 40, 255});
    }
    if (run.stats.shatter && was_slowed) {
        for (int i = 0; i < 6; ++i) {
            float a = static_cast<float>(i) / 6.0f * 2 * PI + run.rng.uniform(0, 0.5f);
            Bullet b;
            b.pos = b.prev = pos;
            b.vel = {std::cos(a) * 700.0f, std::sin(a) * 700.0f};
            b.damage = run.stats.damage * 0.8f;
            b.life = 0.35f;
            b.last_hit = idx;
            b.shard = true;
            b.radius = 6.0f;
            run.new_bullets.push_back(b);
        }
        burst(run, pos, 8, Color{180, 230, 255, 255}, 240.0f, 4.0f, PStar, 0.4f);
    }
    if (type == EnemyType::Boss) {
        run.tally.bosses++;
        int gems = (e.mini_boss ? 3 : 8) * d.stage;
        run.gems_found += gems;
        run.slowmo = 1.8f;
        run.hitstop = 0.12f;
        add_shake(run, 14.0f);
        c.game.sound.play(Sfx::BossRoar, 1.0f, 0.7f);
        run.boss_deaths.push_back({pos, radius, 0.0f, e.mini_boss, gems, 0.0f});
        const BossGenome& genome = e.mini_boss ? run.mini_boss : run.boss;
        if (genome.has(BossPower::Splitter)) {
            for (int k = 0; k < 2; ++k) {
                SpawnEvent ev{0, EnemyType::Brute, std::clamp(pos.x + (k ? 60.0f : -60.0f), 40.0f, VIRTUAL_W - 40.0f)};
                Enemy& child = spawn_enemy(run, ev);
                child.pos.y = pos.y;
                child.elite = true;
                child.hp = child.max_hp = e.max_hp * 0.15f;
                child.radius = radius * 0.6f;
            }
        }
        run.pending_boss_banner = {genome.name, std::format("DEFEATED!  +{} GEMS", gems), Color{255, 210, 70, 255},
                                   2.6f, 2.6f};
        if (run.wave != WAVES_PER_STAGE || run.endless) c.game.play_music(run.stage % 2 ? "battle" : "battle2");
    }
}

float squad_power(const Run& run);

Enemy& spawn_enemy(Run& run, const SpawnEvent& ev) {
    auto d = difficulty_for(run);
    auto st = base_stats(ev.type);
    Enemy e;
    e.type = ev.type;
    e.pos = {ev.x, SPAWN_Y + run.rng.uniform(-30, 0)};
    // Bosses take only the square root of the director's threat so dominating a stage does
    // not turn its finale into a wall of HP.
    float threat = ev.type == EnemyType::Boss ? std::sqrt(run.threat) : run.threat;
    e.hp = e.max_hp = enemy_hp(ev.type, d.stage, d.wave, ev.mini_boss) * threat_hp_mult(threat) *
                      power_hp_mult(squad_power(run), run.start_power, d.wave);
    e.speed = st.speed * run.rng.uniform(0.88f, 1.12f);
    e.radius = st.radius;
    e.wall_dps = st.wall_dps * wall_damage_mult(run);
    e.knock_resist = st.knock_resist;
    e.xp = st.xp;
    e.coins = st.coins;
    e.mini_boss = ev.mini_boss;
    e.wobble_seed = run.rng.uniform(0, 100);
    e.anim = run.rng.uniform(0, 10);
    e.stop_y = run.rng.uniform(420, 620);
    e.action_timer = run.rng.uniform(1.0f, 2.5f);
    if (ev.type == EnemyType::Boss) {
        e.pos.y = SPAWN_Y - 40;
        if (ev.mini_boss) {
            e.radius = 64;
            e.wall_dps *= 0.5f;
            e.xp = 25;
            e.coins = 30;
        }
        e.action_timer = 5.0f;
    }
    switch (run.mod) {
    case WaveMod::Fast:
        e.speed *= 1.35f;
        break;
    case WaveMod::Armored:
        e.armor = 0.35f;
        break;
    case WaveMod::Swarm:
        e.hp = e.max_hp = e.hp * 0.5f;
        break;
    case WaveMod::Giants:
        if (ev.type != EnemyType::Boss) {
            e.radius *= 1.3f;
            e.hp = e.max_hp = e.hp * 2.0f;
            e.wall_dps *= 1.5f;
            e.xp *= 2;
            e.coins *= 2;
        }
        break;
    default:
        break;
    }
    if (ev.type == EnemyType::Shielder) e.shield_hp = e.shield_max = e.max_hp * 3.0f;
    if (ev.type == EnemyType::Barrel) {
        // About half a second of the squad's fire, whatever the stage: a target, not a tank.
        e.hp = e.max_hp = std::max(10.0f, squad_power(run) * 0.5f);
        e.radius = base_stats(EnemyType::Barrel).radius;
        e.speed = base_stats(EnemyType::Barrel).speed;
        run.new_enemies.push_back(e);
        return run.new_enemies.back();
    }
    float elite_chance =
        ev.type == EnemyType::Boss ? 0.0f : std::min(0.06f, 0.004f * static_cast<float>(d.wave + 3 * (d.stage - 1)));
    if (run.mod == WaveMod::Elite) elite_chance = std::max(0.12f, elite_chance * 5.0f);
    if (ev.type != EnemyType::Walker && ev.type != EnemyType::Runner) elite_chance *= 3.0f;
    if (run.rng.chance(elite_chance)) {
        e.elite = true;
        e.hp = e.max_hp = e.hp * 4.0f;
        e.radius *= 1.3f;
        e.coins *= 5;
        e.xp *= 4;
        e.knock_resist = std::min(1.0f, e.knock_resist + 0.4f);
        e.shield_hp *= 2.0f;
        e.shield_max = e.shield_hp;
    }
    run.new_enemies.push_back(e);
    return run.new_enemies.back();
}

int unit_soldiers(const Run& run, size_t slot) {
    int visible = visible_soldiers(run.soldiers);
    int n = static_cast<int>(run.squad.size());
    int s = static_cast<int>(slot);
    return visible / n + (s < visible % n ? 1 : 0);
}

float unit_shot_rate(const Run& run, UnitType u) {
    return run.stats.fire_rate * unit_def(u).rate_mult * unit_rate_mult(run.unit_levels[static_cast<size_t>(u)]);
}

float squad_power(const Run& run) {
    float total = 0.0f;
    float over = overflow_mult(run.soldiers);
    for (size_t i = 0; i < run.squad.size(); ++i) {
        UnitType u = run.squad[i];
        const auto& def = unit_def(u);
        float per_shot = run.stats.damage * def.damage_mult *
                         unit_damage_mult(run.unit_levels[static_cast<size_t>(u)]) *
                         static_cast<float>(def.pellets + run.stats.multishot - 1);
        total += static_cast<float>(unit_soldiers(run, i)) * unit_shot_rate(run, u) * per_shot;
    }
    return total * over * (1.0f + run.stats.crit_chance * (run.stats.crit_mult - 1.0f));
}

float shots_per_second(const Run& run) {
    float total = 0.0f;
    for (size_t i = 0; i < run.squad.size(); ++i) {
        total += static_cast<float>(unit_soldiers(run, i)) * unit_shot_rate(run, run.squad[i]);
    }
    return total;
}

void fire_squad(Ctx& c, float dt) {
    auto& run = c.run;
    int visible = visible_soldiers(run.soldiers);
    if (visible <= 0 || run.squad.empty()) return;
    float over = overflow_mult(run.soldiers);

    // Soldiers nudge their aim toward the closest threat inside a narrow cone, so position
    // still matters but a small squad is not helpless against a horde off to one side.
    constexpr float AIM_CONE = 15.0f * DEG;
    static std::vector<int> front;
    front.clear();
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        const auto& e = run.enemies[static_cast<size_t>(i)];
        if (!e.dead && e.pos.y > HUD_TOP && e.pos.y < WALL_TOP + 10.0f) front.push_back(i);
    }
    size_t keep = std::min<size_t>(front.size(), 80);
    std::partial_sort(front.begin(), front.begin() + static_cast<long>(keep), front.end(), [&](int a, int b) {
        return run.enemies[static_cast<size_t>(a)].pos.y > run.enemies[static_cast<size_t>(b)].pos.y;
    });
    front.resize(keep);
    auto aim_for = [&](Vec2 origin, float max_range) -> std::pair<float, int> {
        for (int idx : front) {
            const auto& e = run.enemies[static_cast<size_t>(idx)];
            Vec2 d = e.pos - origin;
            if (d.y >= -1.0f || d.length() > max_range) continue;
            float off = std::atan2(d.x, -d.y);
            if (std::abs(off) <= AIM_CONE) return {-PI / 2 + off, idx};
        }
        return {-PI / 2, -1};
    };

    int n = static_cast<int>(run.squad.size());
    bool played = false;
    for (size_t slot = 0; slot < run.squad.size(); ++slot) {
        UnitType u = run.squad[slot];
        const auto& def = unit_def(u);
        int count = unit_soldiers(run, slot);
        if (count <= 0) continue;
        auto& accum = run.fire_accum_by_unit[static_cast<size_t>(u)];
        accum += static_cast<float>(count) * unit_shot_rate(run, u) * dt * (run.frenzy_t > 0.0f ? 2.0f : 1.0f);
        float level_dmg = unit_damage_mult(run.unit_levels[static_cast<size_t>(u)]);
        while (accum >= 1.0f) {
            accum -= 1.0f;
            int k = run.fire_cursor_by_unit[static_cast<size_t>(u)]++ % count;
            int si = static_cast<int>(slot) + k * n;
            if (si >= visible) si = static_cast<int>(slot);
            Vec2 origin = Vec2{run.squad_x, SQUAD_Y} + soldier_offset(si, visible) + muzzle_offset(static_cast<int>(u));
            run.soldier_recoil[static_cast<size_t>(si)] = 1.0f;
            float base_dmg =
                run.stats.damage * def.damage_mult * level_dmg * over * (run.frenzy_t > 0.0f ? 1.25f : 1.0f);

            if (def.shot == Shot::Grenade) {
                auto [ang, target] = aim_for(origin, 900.0f);
                Vec2 to = target >= 0 ? run.enemies[static_cast<size_t>(target)].pos
                                      : origin + Vec2{std::cos(ang), std::sin(ang)} * 520.0f;
                to += Vec2{run.rng.uniform(-def.spread_deg, def.spread_deg) * 3.0f, run.rng.uniform(-20, 20)};
                bool crit = run.rng.chance(run.stats.crit_chance);
                float dmg = base_dmg * (crit ? run.stats.crit_mult : 1.0f);
                float flight = std::clamp(origin.distance_to(to) / 900.0f, 0.35f, 0.9f);
                Shell sh{origin,
                         to,
                         0.0f,
                         flight,
                         dmg * static_cast<float>(run.stats.multishot),
                         def.splash + run.stats.explosive_radius,
                         false};
                sh.grenade = true;
                run.shells.push_back(sh);
                continue;
            }

            int pellets = def.pellets + run.stats.multishot - 1;
            float fan = (def.pellets > 1 ? def.spread_deg : 6.0f) * DEG;
            float budget = run.bullets.size() > 1600 ? 0.25f : 1.0f;
            int spawned = std::max(1, static_cast<int>(std::round(static_cast<float>(pellets) * budget)));
            float dmg_scale = static_cast<float>(pellets) / static_cast<float>(spawned);
            float range = def.speed * def.life;
            float base_ang = aim_for(origin, range).first;
            for (int p = 0; p < spawned; ++p) {
                float t = spawned > 1 ? static_cast<float>(p) / static_cast<float>(spawned - 1) - 0.5f : 0.0f;
                float ang = base_ang + t * fan * static_cast<float>(std::min(spawned, 5)) / 2.0f +
                            run.rng.uniform(-def.spread_deg, def.spread_deg) * DEG * (def.pellets > 1 ? 0.5f : 1.0f);
                Bullet b;
                b.pos = b.prev = origin;
                b.vel = {std::cos(ang) * def.speed, std::sin(ang) * def.speed};
                b.damage = base_dmg * dmg_scale;
                b.crit = run.rng.chance(run.stats.crit_chance);
                if (b.crit) b.damage *= run.stats.crit_mult;
                b.pierce = run.stats.pierce + def.bonus_pierce;
                b.ricochet = def.shot == Shot::Flame ? 0 : run.stats.ricochet;
                b.life = def.life;
                b.splash = def.shot == Shot::Flame ? 0.0f : def.splash + run.stats.explosive_radius;
                b.knock = def.knock;
                b.unit = u;
                b.flame = def.shot == Shot::Flame;
                b.radius = b.flame ? 16.0f : u == UnitType::Sniper ? 6.0f : BULLET_RADIUS;
                run.bullets.push_back(b);
            }
            if (def.shot == Shot::Bullet && k % 2 == 0) {
                particle(run, origin + Vec2{0, -4}, {0, -30}, 0.06f, 12.0f, 5.0f, Color{255, 230, 150, 255}, PGlow,
                         0.0f);
            }
            if (!played && run.shot_sound_cd <= 0.0f) {
                played = true;
                bool heavy = u == UnitType::Sniper || u == UnitType::Shotgunner;
                Sfx sfx = def.shot == Shot::Flame ? Sfx::Flame : heavy ? Sfx::ShotHeavy : Sfx::Shot;
                c.game.sound.play(sfx, heavy ? 0.35f : 0.18f, run.rng.uniform(0.9f, 1.15f));
                run.shot_sound_cd = heavy ? 0.09f : 0.055f;
            }
        }
    }
}

int nearest_enemy(const Run& run, Vec2 p, float max_dist, int exclude) {
    int best = -1;
    float bd = max_dist * max_dist;
    g_grid.query(p, max_dist, [&](int i) {
        if (i == exclude) return;
        const auto& e = run.enemies[static_cast<size_t>(i)];
        if (e.dead) return;
        float d = (e.pos - p).length_sq();
        if (d < bd) {
            bd = d;
            best = i;
        }
    });
    return best;
}

void open_cards(Ctx& c);

void break_loot(Ctx& c, Loot& f) {
    auto& run = c.run;
    if (!f.alive) return;
    f.alive = false;
    Color debris = lock_color(f.lock);
    bool glassy = f.lock == LockKind::Ice || f.lock == LockKind::Crystal;
    if (glassy) {
        c.game.sound.play(Sfx::IceBreak, 0.9f, f.lock == LockKind::Crystal ? 0.8f : 1.0f);
    } else {
        c.game.sound.play(Sfx::Explosion, 0.6f, f.lock == LockKind::Cage ? 1.5f : 1.2f);
        c.game.sound.play(Sfx::GateGood, 0.6f, 1.2f);
    }
    additive_particle(run, f.pos, {}, 0.18f, LOOT_SIZE * 0.6f, LOOT_SIZE * 1.8f, brighten(debris, 0.6f), PGlow, 0.0f);
    shockwave(run, f.pos, LOOT_SIZE * 0.4f, LOOT_SIZE * 2.2f, 0.35f, with_alpha(brighten(debris, 0.4f), 0.8f));
    for (int i = 0; i < 18; ++i) {
        gib(run, f.pos + Vec2{run.rng.uniform(-30, 30), run.rng.uniform(-30, 30)},
            i % 3 ? debris : brighten(debris, 0.35f), run.rng.uniform(3.0f, 7.0f), 260.0f, 260.0f, 0.9f);
    }
    add_shake(run, 6.0f);
    float_text(run, loot_label(f), f.pos + Vec2{0, -20}, Color{170, 235, 255, 255}, 38.0f, 1.5f);
    switch (f.kind) {
    case LootKind::Soldiers: {
        if (std::ranges::find(run.squad, f.unit) == run.squad.end()) run.squad.push_back(f.unit);
        run.soldiers = std::min(MAX_SOLDIERS, run.soldiers + f.amount);
        run.soldier_pop = 1.0f;
        run.tally.max_soldiers = std::max(run.tally.max_soldiers, run.soldiers);
        break;
    }
    case LootKind::Turret: {
        int slot = -1;
        for (int s = 0; s < TURRET_SLOTS; ++s) {
            if (run.turrets[static_cast<size_t>(s)].type == TurretType::None) {
                slot = s;
                break;
            }
        }
        if (slot >= 0) {
            run.turrets[static_cast<size_t>(slot)] = Turret{f.turret, 1};
        } else {
            for (auto& t : run.turrets) {
                if (t.level < TURRET_MAX_LEVEL) {
                    t.level++;
                    break;
                }
            }
        }
        c.game.sound.play(Sfx::Build, 0.8f);
        break;
    }
    case LootKind::Card:
        run.pending_levels++;
        run.card_delay = 0.4f;
        break;
    case LootKind::Coins:
        spawn_coins(run, f.pos, f.amount);
        break;
    case LootKind::Gems:
        run.gems_found += f.amount;
        c.game.sound.play(Sfx::Gem, 0.9f);
        break;
    }
}

void spawn_loot(Ctx& c) {
    auto& run = c.run;
    auto d = difficulty_for(run);
    Loot f;
    f.pos = {run.rng.uniform(70.0f, VIRTUAL_W - 70.0f), HUD_TOP - LOOT_SIZE};
    f.seed = run.rng.uniform(0, 100);
    f.lock = run.theme.lock;
    int roll = run.rng.range(0, 99);
    if (roll < 35) {
        f.kind = LootKind::Soldiers;
        f.unit = static_cast<UnitType>(run.rng.range(0, UNIT_COUNT - 1));
        f.amount = std::max(4, run.soldiers / 4);
    } else if (roll < 55) {
        f.kind = LootKind::Turret;
        f.turret = static_cast<TurretType>(run.rng.range(1, TURRET_KINDS));
    } else if (roll < 80) {
        f.kind = LootKind::Card;
    } else if (roll < 95) {
        f.kind = LootKind::Coins;
        f.amount = 40 + 20 * d.wave + 30 * (d.stage - 1);
    } else {
        f.kind = LootKind::Gems;
        f.amount = 2 + d.stage;
    }
    // Roughly four seconds of the whole squad's fire, so thawing it is a real decision.
    f.hp = f.max_hp = std::max(60.0f, squad_power(run) * 4.0f);
    run.loot.push_back(f);
}

void update_loot(Ctx& c, float dt) {
    auto& run = c.run;
    auto d = difficulty_for(run);
    if (run.phase == WavePhase::Fighting && (d.wave >= 2 || run.endless)) {
        run.loot_timer -= dt;
        if (run.loot_timer <= 0.0f) {
            run.loot_timer = run.rng.uniform(15.0f, 22.0f);
            spawn_loot(c);
        }
    }
    for (auto& f : run.loot) {
        if (!f.alive) continue;
        f.pos.y += LOOT_SPEED * dt;
        f.hit_flash = std::max(0.0f, f.hit_flash - dt);
        if (f.pos.y + LOOT_SIZE * 0.5f >= WALL_TOP - 4.0f) {
            f.alive = false;
            // Like the genre's upgrade wheels, loot left unbroken smashes into the defences.
            float crash = run.stats.wall_max * 0.06f;
            damage_wall(c, crash, {f.pos.x, WALL_TOP});
            add_shake(run, 9.0f);
            float_text(run, std::format("CRASH -{:.0f}", crash), f.pos, Color{255, 120, 100, 255}, 30.0f);
            for (int i = 0; i < 10; ++i) gib(run, f.pos, lock_color(f.lock), 4.0f, 180.0f, 160.0f, 0.6f);
            c.game.sound.play(Sfx::IceHit, 0.5f, 0.6f);
        }
    }
    std::erase_if(run.loot, [](const Loot& f) { return !f.alive; });
}

void update_bullets(Ctx& c, float dt) {
    auto& run = c.run;
    int hpp = hits_per_point(shots_per_second(run) * static_cast<float>(run.stats.multishot));
    for (auto& b : run.bullets) {
        if (!b.alive) continue;
        b.prev = b.pos;
        b.pos += b.vel * dt;
        b.life -= dt;
        // Squad bullets start behind the wall, so only cull ones heading down past it (shards).
        if (b.life <= 0.0f || b.pos.y < HUD_TOP - 40 || b.pos.x < -20 || b.pos.x > VIRTUAL_W + 20 ||
            (b.vel.y > 0.0f && b.pos.y > WALL_BOTTOM)) {
            b.alive = false;
            continue;
        }
        if (!b.shard) {
            for (auto& f : run.loot) {
                if (!f.alive) continue;
                if (std::abs(b.pos.x - f.pos.x) < LOOT_SIZE * 0.5f && std::abs(b.pos.y - f.pos.y) < LOOT_SIZE * 0.5f) {
                    b.alive = false;
                    f.hp -= b.damage;
                    f.hit_flash = 0.06f;
                    if (run.rng.chance(0.25f)) {
                        particle(run, b.pos, {run.rng.uniform(-90, 90), run.rng.uniform(40, 120)}, 0.3f, 3.0f, 1.0f,
                                 lock_color(f.lock), PDisc, 3.0f, 300.0f);
                    }
                    if (run.shot_sound_cd <= 0.02f) {
                        bool glassy = f.lock == LockKind::Ice || f.lock == LockKind::Crystal;
                        c.game.sound.play(glassy ? Sfx::IceHit : Sfx::Hit, 0.25f,
                                          run.rng.uniform(0.9f, 1.2f) * (f.lock == LockKind::Cage ? 1.6f : 1.0f));
                    }
                    if (f.hp <= 0.0f) break_loot(c, f);
                    break;
                }
            }
            if (!b.alive) continue;
            for (auto& g : run.gates) {
                if (!g.alive) continue;
                if (std::abs(b.pos.x - g.x) < g.w * 0.5f && std::abs(b.pos.y - g.y) < GATE_H * 0.5f) {
                    b.alive = false;
                    g.hit_flash = 0.06f;
                    if (gate_hit(g.state, hpp, run.stats.gate_growth, run.soldiers)) {
                        g.pulse = 1.0f;
                        c.game.sound.play(Sfx::GateTick, 0.35f, g.state.value > 0 ? 1.2f : 0.9f);
                    }
                    break;
                }
            }
            if (!b.alive) continue;
        }
        int target = -1;
        g_grid.query(b.pos, b.radius + MAX_ENEMY_R, [&](int i) {
            if (target != -1 || i == b.last_hit) return;
            const auto& e = run.enemies[static_cast<size_t>(i)];
            if (e.dead) return;
            float rr = e.radius + b.radius;
            if ((e.pos - b.pos).length_sq() < rr * rr) target = i;
        });
        if (target == -1) continue;
        Vec2 hit_pos = b.pos;
        auto& te = run.enemies[static_cast<size_t>(target)];
        if (te.shield_hp > 0.0f && b.vel.y < 0.0f && !b.shard && !b.flame) {
            te.shield_hp -= b.damage;
            te.shield_flash = 0.08f;
            b.alive = false;
            particle(run, hit_pos, {run.rng.uniform(-80, 80), 60}, 0.15f, 6.0f, 2.0f, Color{220, 230, 240, 255}, PGlow,
                     3.0f);
            if (te.shield_hp <= 0.0f) {
                burst(run, te.pos + Vec2{0, te.radius}, 14, Color{170, 180, 190, 255}, 260.0f, 6.0f);
                c.game.sound.play(Sfx::WallHit, 0.5f, 1.6f);
                float_text(run, "SHIELD BROKEN", te.pos + Vec2{0, -te.radius - 10}, Color{200, 220, 255, 255}, 20.0f);
            } else {
                c.game.sound.play(Sfx::Hit, 0.15f, 1.8f);
            }
            continue;
        }
        damage_enemy(c, target, b.damage, b.crit, true, b.knock, !b.flame);
        if (b.flame) {
            auto& fe = run.enemies[static_cast<size_t>(target)];
            fe.burn_timer = 2.2f;
            fe.burn_dps = std::max(fe.burn_dps, b.damage * 4.0f);
        } else {
            particle(run, hit_pos, {0, 0}, 0.08f, 10.0f, 18.0f, Color{255, 240, 200, 220}, PGlow, 0.0f);
        }
        if (b.splash > 0.0f) {
            explode(c, hit_pos, b.splash, b.damage * 0.6f, true, Color{255, 160, 60, 255});
        }
        b.last_hit = target;
        if (b.pierce > 0) {
            b.pierce--;
            b.damage *= 0.85f;
        } else if (b.ricochet > 0) {
            b.ricochet--;
            int next = nearest_enemy(run, hit_pos, 240.0f, target);
            if (next == -1) {
                b.alive = false;
            } else {
                float spd = b.vel.length();
                b.vel = (run.enemies[static_cast<size_t>(next)].pos - hit_pos).normalized() * spd;
                b.life = std::max(b.life, 0.4f);
            }
        } else {
            b.alive = false;
        }
    }
    std::erase_if(run.bullets, [](const Bullet& b) { return !b.alive; });
    run.bullets.insert(run.bullets.end(), run.new_bullets.begin(), run.new_bullets.end());
    run.new_bullets.clear();
}

void damage_wall(Ctx& c, float amount, Vec2 at) {
    auto& run = c.run;
    run.wall_hp -= amount;
    run.wall_damage_rate += amount;
    if (amount > 8.0f) {
        run.wall_flash = 0.12f;
        run.wall_jolt = std::min(1.0f, run.wall_jolt + amount / 60.0f);
        int chunks = std::min(10, static_cast<int>(amount / 6.0f));
        for (int i = 0; i < chunks; ++i) {
            gib(run, {at.x + run.rng.uniform(-24, 24), WALL_TOP + run.rng.uniform(-4, 20)},
                mix(run.theme.stone, Color{90, 84, 78, 255}, run.rng.unit()), run.rng.uniform(2.5f, 5.5f), 160.0f,
                300.0f, 0.9f);
        }
    }
    if (run.rng.chance(std::min(1.0f, amount * 0.05f))) {
        particle(run, at, {run.rng.uniform(-40, 40), run.rng.uniform(-90, -30)}, 0.6f, 6.0f, 16.0f,
                 Color{150, 130, 110, 160}, PSmoke, 2.0f);
    }
}

void update_boss(Ctx& c, int idx, float dt) {
    auto& run = c.run;
    auto& e = run.enemies[static_cast<size_t>(idx)];
    const BossGenome& g = e.mini_boss ? run.mini_boss : run.boss;
    if (e.pos.y < HUD_TOP) return;
    e.dash = std::max(0.0f, e.dash - dt);
    Vec2 boss_pos = e.pos;
    float boss_r = e.radius;

    e.action_timer -= dt;
    if (e.action_timer <= 0.0f) {
        e.action_timer = e.mini_boss ? 7.0f : 5.5f;
        if (g.has(BossPower::Summoner) || !e.mini_boss) {
            c.game.sound.play(Sfx::BossRoar, 0.7f, e.mini_boss ? 1.2f : 1.0f);
            add_shake(run, 10.0f);
            shockwave(run, boss_pos, boss_r, boss_r * 4.0f, 0.5f, Color{255, 80, 60, 200});
            int n = (e.mini_boss ? 3 : 5) + (g.has(BossPower::Summoner) ? 4 : 0);
            for (int k = 0; k < n; ++k) {
                SpawnEvent ev{0, k % 3 == 0 ? EnemyType::Runner : EnemyType::Walker,
                              std::clamp(boss_pos.x + run.rng.uniform(-140, 140), 30.0f, VIRTUAL_W - 30.0f)};
                spawn_enemy(run, ev).pos.y = boss_pos.y + run.rng.uniform(-30, 30);
            }
        }
    }

    e.power_timer -= dt;
    if (e.power_timer <= 0.0f) {
        e.power_timer = 4.5f;
        if (g.has(BossPower::Charger) && !e.at_wall) {
            e.dash = 0.7f;
            c.game.sound.play(Sfx::BossRoar, 0.6f, 1.5f);
            float_text(run, "CHARGE!", boss_pos + Vec2{0, -boss_r - 20}, Color{255, 120, 80, 255}, 30.0f);
        }
        if (g.has(BossPower::Barrage)) {
            for (int k = 0; k < 6; ++k) {
                Vec2 target{60.0f + static_cast<float>(k) * 120.0f + run.rng.uniform(-30, 30), WALL_TOP + 20};
                Vec2 v = (target - boss_pos).normalized() * 420.0f;
                run.acids.push_back({boss_pos, v, 16.0f * wall_damage_mult(run)});
            }
            c.game.sound.play(Sfx::Cannon, 0.5f, 0.7f);
        }
        if (g.has(BossPower::Shielded) && e.shield_hp <= 0.0f) {
            e.shield_hp = e.shield_max = e.max_hp * 0.2f;
            float_text(run, "SHIELD UP", boss_pos + Vec2{0, -boss_r - 20}, Color{170, 200, 255, 255}, 28.0f);
        }
    }
    if (e.dash > 0.0f && run.rng.chance(dt * 30.0f)) {
        particle(run, boss_pos + Vec2{run.rng.uniform(-boss_r, boss_r), boss_r * 0.8f}, {0, -40}, 0.6f, 14.0f, 34.0f,
                 Color{150, 130, 110, 150}, PSmoke, 1.5f);
        add_shake(run, 3.0f);
    }
    if (g.has(BossPower::Healer)) {
        g_grid.query(boss_pos, 220.0f, [&](int j) {
            auto& o = run.enemies[static_cast<size_t>(j)];
            if (o.dead || j == idx) return;
            if (o.pos.distance_to(boss_pos) < 220.0f) o.hp = std::min(o.max_hp, o.hp + o.max_hp * 0.04f * dt);
        });
        if (run.rng.chance(dt * 8.0f)) {
            float a = run.rng.uniform(0, 2 * PI);
            additive_particle(run, boss_pos + Vec2{std::cos(a), std::sin(a)} * 200.0f, {0, -30}, 0.6f, 6.0f, 2.0f,
                              Color{120, 255, 140, 255}, PGlow, 1.0f);
        }
    }
    if (g.has(BossPower::Enrage) && !e.enraged && e.hp < e.max_hp * 0.4f) {
        e.enraged = true;
        c.game.sound.play(Sfx::BossRoar, 1.0f, 0.8f);
        add_shake(run, 18.0f);
        run.banner = {"ENRAGED!", g.name, Color{255, 60, 40, 255}, 1.6f, 1.6f};
    }
}

void update_enemies(Ctx& c, float dt) {
    auto& run = c.run;
    float wall_sound = 0.0f;
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        auto& e = run.enemies[static_cast<size_t>(i)];
        if (e.dead) continue;
        e.anim += dt;
        e.flash = std::max(0.0f, e.flash - dt);
        e.shield_flash = std::max(0.0f, e.shield_flash - dt);
        if (e.slow_timer > 0.0f) e.slow_timer -= dt;
        if (e.burn_timer > 0.0f) {
            e.burn_timer -= dt;
            e.hp -= e.burn_dps * dt;
            run.tally.damage_dealt += e.burn_dps * dt;
            if (run.rng.chance(dt * 10.0f)) {
                particle(run, e.pos + Vec2{run.rng.uniform(-e.radius, e.radius) * 0.6f, -e.radius * 0.3f}, {0, -60},
                         0.45f, e.radius * 0.5f, 2.0f, Color{255, 150, 50, 230}, PFire, 1.0f);
            }
            if (e.hp <= 0.0f) {
                kill_enemy(c, i);
                continue;
            }
        }
        if (e.charm > 0.0f) {
            // Charmed by Ali's peace sign: the monster stops to wave back and leaves the wall alone.
            e.charm -= dt;
            e.knock *= std::exp(-10.0f * dt);
            continue;
        }
        float speed = e.speed * (e.slow_timer > 0.0f ? e.slow_factor : 1.0f) * (e.dash > 0.0f ? 6.0f : 1.0f) *
                      (e.enraged ? 1.5f : 1.0f);
        e.pos.y -= e.knock * dt * 6.0f;
        e.knock *= std::exp(-10.0f * dt);
        float stop_line = run.collapse_t > 0.5f ? VIRTUAL_H + 200.0f : WALL_TOP - e.radius * 0.55f;
        e.hurt = std::max(0.0f, e.hurt - dt * 5.0f);

        if (e.type == EnemyType::Spitter && e.pos.y >= e.stop_y) {
            e.action_timer -= dt;
            if (e.action_timer <= 0.0f) {
                e.action_timer = 2.6f;
                Vec2 target{std::clamp(e.pos.x + run.rng.uniform(-60, 60), 20.0f, VIRTUAL_W - 20.0f), WALL_TOP + 20};
                Vec2 v = (target - e.pos).normalized() * 360.0f;
                run.acids.push_back({e.pos + Vec2{0, e.radius * 0.5f}, v, 22.0f * wall_damage_mult(run)});
            }
        } else if (!e.at_wall) {
            e.pos.y += speed * dt;
            float sway = e.type == EnemyType::Flyer ? 90.0f : 14.0f;
            float freq = e.type == EnemyType::Flyer ? 3.1f : 1.7f;
            e.pos.x += std::sin(run.time * freq + e.wobble_seed) * sway * dt;
            if (e.type == EnemyType::Boss && e.pos.y > HUD_TOP + 20) {
                float step = std::sin(e.anim * 3.0f);
                if (step > 0.97f) add_shake(run, e.mini_boss ? 2.0f : 4.0f);
            }
            if (e.pos.y >= stop_line) {
                e.pos.y = stop_line;
                e.at_wall = true;
                if (e.type == EnemyType::Barrel) {
                    kill_enemy(c, i);
                    continue;
                }
                if (e.type == EnemyType::Exploder) {
                    damage_wall(c, std::min(120.0f * wall_damage_mult(run), run.stats.wall_max * 0.12f), e.pos);
                    add_shake(run, 10.0f);
                    e.hp = 0;
                    kill_enemy(c, i);
                    continue;
                }
            }
        } else {
            if (e.pos.y < stop_line - 4.0f || run.collapse_t > 0.5f) e.at_wall = false;
            float dps = e.wall_dps * (e.enraged ? 1.5f : 1.0f) * (e.slow_timer > 0.0f ? e.slow_factor : 1.0f);
            damage_wall(c, dps * dt, {e.pos.x, WALL_TOP + 4});
            wall_sound += dps * dt;
        }

        if (e.type == EnemyType::Boss) update_boss(c, i, dt);
    }
    if (wall_sound > 0.0f && run.rng.chance(std::min(0.25f, wall_sound * 0.04f))) {
        c.game.sound.play(Sfx::WallHit, 0.35f, run.rng.uniform(0.8f, 1.2f));
        add_shake(run, 2.5f);
    }

    // Cheap separation so a horde spreads along the wall instead of stacking into one blob.
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        auto& a = run.enemies[static_cast<size_t>(i)];
        if (a.dead) continue;
        g_grid.query(a.pos, a.radius + MAX_ENEMY_R, [&](int j) {
            if (j <= i) return;
            auto& b = run.enemies[static_cast<size_t>(j)];
            if (b.dead) return;
            Vec2 d = b.pos - a.pos;
            float rr = (a.radius + b.radius) * 0.8f;
            float dsq = d.length_sq();
            if (dsq >= rr * rr || dsq < 0.0001f) return;
            float dist = std::sqrt(dsq);
            float push = (rr - dist) * 0.5f;
            Vec2 n = d * (1.0f / dist);
            float wa = b.radius / (a.radius + b.radius);
            float wb = 1.0f - wa;
            if (a.type == EnemyType::Boss) wa = 0.0f, wb = 1.0f;
            if (b.type == EnemyType::Boss) wa = 1.0f, wb = 0.0f;
            a.pos.x -= n.x * push * wa;
            b.pos.x += n.x * push * wb;
            if (!a.at_wall) a.pos.y -= n.y * push * wa * 0.6f;
            if (!b.at_wall) b.pos.y += n.y * push * wb * 0.6f;
        });
        a.pos.x = std::clamp(a.pos.x, a.radius * 0.6f, VIRTUAL_W - a.radius * 0.6f);
    }
}

void update_acids(Ctx& c, float dt) {
    auto& run = c.run;
    for (auto& a : run.acids) {
        a.pos += a.vel * dt;
        if (run.rng.chance(dt * 20.0f)) {
            particle(run, a.pos, {0, 0}, 0.3f, 6.0f, 1.0f, Color{180, 90, 220, 200}, PDisc, 0.0f);
        }
        if (a.pos.y >= WALL_TOP + 10) {
            a.alive = false;
            damage_wall(c, a.damage, a.pos);
            burst(run, a.pos, 8, Color{180, 90, 220, 255}, 160.0f, 5.0f);
            c.game.sound.play(Sfx::Hit, 0.4f, 0.6f);
        }
    }
    std::erase_if(run.acids, [](const Acid& a) { return !a.alive; });
}

void spawn_corpses(Run& run, int count) {
    int visible = visible_soldiers(run.soldiers);
    for (int i = 0; i < count && visible > 0; ++i) {
        int si = run.rng.range(0, visible - 1);
        Corpse k;
        k.pos = Vec2{run.squad_x, SQUAD_Y} + soldier_offset(si, visible);
        k.vel = {run.rng.uniform(-160, 160), run.rng.uniform(40, 160)};
        k.spin = run.rng.uniform(-720, 720);
        k.vz = run.rng.uniform(220, 380);
        k.life = 1.6f;
        k.unit = run.squad[static_cast<size_t>(si) % run.squad.size()];
        run.corpses.push_back(k);
        burst(run, k.pos, 3, Color{150, 20, 24, 255}, 120.0f, 3.0f, PDisc, 0.4f);
    }
}

void update_corpses(Run& run, float dt) {
    for (auto& k : run.corpses) {
        k.life -= dt;
        k.pos += k.vel * dt;
        k.vel = k.vel * std::exp(-1.5f * dt);
        k.vz -= 1100.0f * dt;
        k.z += k.vz * dt;
        k.rot += k.spin * dt;
        if (k.z <= 0.0f) {
            k.z = 0.0f;
            k.vz = std::abs(k.vz) > 120.0f ? -k.vz * 0.3f : 0.0f;
            k.spin *= 0.3f;
            k.vel = k.vel * 0.4f;
        }
    }
    std::erase_if(run.corpses, [](const Corpse& k) { return k.life <= 0.0f; });
}

float boss_death_duration(const BossDeath& b) {
    return b.mini ? 1.3f : 2.1f;
}

void update_boss_deaths(Ctx& c, float dt) {
    auto& run = c.run;
    for (auto& b : run.boss_deaths) {
        b.t += dt;
        b.next_blast -= dt;
        float dur = boss_death_duration(b);
        if (b.t < dur && b.next_blast <= 0.0f) {
            float k = b.t / dur;
            b.next_blast = 0.16f - 0.1f * k;
            Vec2 p =
                b.pos + Vec2{run.rng.uniform(-b.radius, b.radius) * 0.8f, run.rng.uniform(-b.radius, b.radius) * 0.8f};
            explosion_fx(run, p, b.radius * (0.5f + 0.4f * k), Color{255, 140, 60, 255});
            add_shake(run, 6.0f + 10.0f * k);
            c.game.sound.play(Sfx::Explosion, 0.6f, run.rng.uniform(0.8f, 1.2f));
        }
        if (b.t >= dur) {
            const BossGenome& gen = b.mini ? run.mini_boss : run.boss;
            run.flash = 0.7f;
            run.zoom_kick = 1.0f;
            run.hitstop = 0.08f;
            add_shake(run, 30.0f);
            c.game.sound.play(Sfx::BigExplosion, 1.0f, 0.75f);
            explosion_fx(run, b.pos, b.radius * 2.2f, Color{255, 160, 70, 255});
            for (int r = 0; r < 3; ++r) {
                shockwave(run, b.pos, b.radius * 0.5f, b.radius * (3.0f + 1.5f * static_cast<float>(r)),
                          0.4f + 0.15f * static_cast<float>(r),
                          r == 1 ? with_alpha(gen.accent, 0.9f) : Color{255, 230, 180, 200});
            }
            for (int i = 0; i < 34; ++i) {
                Color col = i % 5 == 0 ? Color{232, 220, 192, 255} : (i % 2 ? gen.body : darken(gen.body, 0.35f));
                gib(run,
                    b.pos +
                        Vec2{run.rng.uniform(-b.radius, b.radius) * 0.5f, run.rng.uniform(-b.radius, b.radius) * 0.5f},
                    col, run.rng.uniform(5.0f, b.radius * 0.18f), 520.0f, 520.0f, 1.6f);
            }
            for (int i = 0; i < 26; ++i) {
                float a = run.rng.uniform(0, 2 * PI);
                float sp = run.rng.uniform(200, 520);
                additive_particle(run, b.pos, {std::cos(a) * sp, std::sin(a) * sp - 120.0f},
                                  run.rng.uniform(0.7f, 1.2f), 10.0f, 3.0f,
                                  i % 2 ? Color{120, 230, 255, 255} : Color{255, 220, 90, 255}, PStar, 2.0f);
            }
            run.pending_decals.push_back({b.pos, b.radius * 1.6f, Color{30, 24, 20, 120}, run.rng.uniform(0, 360), 1});
            run.pending_decals.push_back(
                {b.pos, b.radius * 1.2f, with_alpha(enemy_blood_color(EnemyType::Boss), 0.6f), 0, 0});
            run.banner = run.pending_boss_banner;
        }
    }
    std::erase_if(run.boss_deaths, [](const BossDeath& b) { return b.t >= boss_death_duration(b); });
}

void update_collapse(Ctx& c, float dt) {
    auto& run = c.run;
    if (run.collapse_t < 0.0f) return;
    float before = run.collapse_t;
    run.collapse_t += dt;
    if (run.collapse_t < 1.4f && run.rng.chance(dt * 40.0f)) {
        Vec2 p{run.rng.uniform(0, VIRTUAL_W), WALL_TOP + run.rng.uniform(10, 70)};
        particle(run, p, {run.rng.uniform(-60, 60), run.rng.uniform(-80, -20)}, run.rng.uniform(1.0f, 1.8f), 18.0f,
                 60.0f, Color{150, 136, 120, 170}, PSmoke, 1.2f);
    }
    for (float mark : {0.15f, 0.5f, 0.9f}) {
        if (before < mark && run.collapse_t >= mark) {
            c.game.sound.play(Sfx::WallHit, 1.0f, 0.6f);
            c.game.sound.play(Sfx::Explosion, 0.8f, 0.6f);
            add_shake(run, 18.0f);
        }
    }
}

void apply_gate_to_squad(Ctx& c, Gate& g) {
    auto& run = c.run;
    if (g.state.kind == GateKind::Rate || g.state.kind == GateKind::Power) {
        float k = 1.0f + static_cast<float>(g.state.value) / 100.0f;
        bool rate = g.state.kind == GateKind::Rate;
        if (rate) {
            run.stats.fire_rate *= k;
        } else {
            run.stats.damage *= k;
        }
        run.tally.gates_taken++;
        Vec2 at{run.squad_x, SQUAD_Y - 40};
        bool up = g.state.value >= 0;
        float_text(run, std::format("{}{}% {}", up ? "+" : "", g.state.value, rate ? "FIRE RATE" : "DAMAGE"), at,
                   up ? Color{120, 255, 140, 255} : Color{255, 90, 90, 255}, 40.0f, 1.3f);
        c.game.sound.play(up ? Sfx::GateGood : Sfx::GateBad, 0.9f);
        if (!up) add_shake(run, 8.0f);
        burst(run, at, 20, up ? Color{120, 255, 160, 255} : Color{255, 80, 80, 255}, 280.0f, 6.0f, PStar, 0.6f);
        return;
    }
    int before = run.soldiers;
    run.soldiers = apply_gate(run.soldiers, g.state);
    int delta = run.soldiers - before;
    if (delta < 0) {
        int lost_visible = visible_soldiers(before) - visible_soldiers(run.soldiers);
        int was = run.soldiers;
        run.soldiers = before;
        spawn_corpses(run, std::max(std::min(-delta, 6), lost_visible));
        run.soldiers = was;
    }
    run.tally.gates_taken++;
    run.ali_charge = std::min(1.0f, run.ali_charge + 0.12f);
    run.tally.max_soldiers = std::max(run.tally.max_soldiers, run.soldiers);
    run.soldier_pop = 1.0f;
    run.squad_x = clamp_squad_x(run.squad_x, visible_soldiers(run.soldiers));
    Vec2 at{run.squad_x, SQUAD_Y - 40};
    if (delta >= 0) {
        float_text(run, std::format("+{}", delta), at, Color{120, 255, 140, 255}, 60.0f, 1.2f);
        c.game.sound.play(Sfx::GateGood, 0.9f);
        burst(run, at, 24, Color{120, 255, 160, 255}, 320.0f, 6.0f, PStar, 0.7f);
    } else {
        float_text(run, std::format("{}", delta), at, Color{255, 90, 90, 255}, 60.0f, 1.2f);
        c.game.sound.play(Sfx::GateBad, 0.9f);
        burst(run, at, 18, Color{255, 80, 80, 255}, 260.0f, 6.0f, PDisc, 0.6f);
        add_shake(run, 8.0f);
    }
}

void update_gates(Ctx& c, float dt) {
    auto& run = c.run;
    if (run.phase == WavePhase::Fighting) {
        run.gate_timer -= dt;
        if (run.gate_timer <= 0.0f) {
            auto d = difficulty_for(run);
            run.gate_timer = run.rng.uniform(10.0f, 13.0f);
            int lanes = (d.wave >= 6 && run.rng.chance(0.4f)) ? 3 : 2;
            auto row = roll_gate_row(run.rng, d.wave, run.soldiers, lanes);
            float lane_w = static_cast<float>(VIRTUAL_W) / static_cast<float>(lanes);
            run.gate_rows++;
            for (int i = 0; i < lanes; ++i) {
                Gate g;
                g.state = row[static_cast<size_t>(i)];
                g.x = lane_w * (static_cast<float>(i) + 0.5f);
                g.y = HUD_TOP - GATE_H;
                g.w = lane_w - 22.0f;
                g.row = run.gate_rows;
                run.gates.push_back(g);
            }
            if (!run.gate_hint_shown && run.stage == 1 && !c.game.meta.tutorial_done) {
                run.gate_hint_shown = true;
                run.hint = "YOU MUST PASS ONE GATE!\nSHOOT IT TO RAISE ITS NUMBER";
                run.hint_timer = 5.0f;
            }
        }
    }
    for (auto& g : run.gates) {
        if (!g.alive) continue;
        g.y += GATE_SPEED * dt;
        g.pulse = std::max(0.0f, g.pulse - dt * 3.0f);
        g.hit_flash = std::max(0.0f, g.hit_flash - dt);
    }
    for (auto& g : run.gates) {
        if (!g.alive || g.y + GATE_H * 0.5f < WALL_TOP - 6.0f) continue;
        Gate* chosen = nullptr;
        for (auto& o : run.gates) {
            if (!o.alive || o.row != g.row) continue;
            if (!chosen || std::abs(o.x - run.squad_x) < std::abs(chosen->x - run.squad_x)) chosen = &o;
        }
        if (chosen) apply_gate_to_squad(c, *chosen);
        for (auto& o : run.gates) {
            if (!o.alive || o.row != g.row) continue;
            o.alive = false;
            bool picked = &o == chosen;
            Color col = o.state.red ? Color{255, 90, 90, 255} : Color{90, 200, 255, 255};
            for (int k = 0; k < (picked ? 24 : 8); ++k) {
                particle(run, {o.x + run.rng.uniform(-o.w / 2, o.w / 2), o.y},
                         {run.rng.uniform(-80, 80), run.rng.uniform(-160, -40)}, 0.6f, 7.0f, 0.0f, col, PDisc, 2.0f,
                         300.0f);
            }
        }
    }
    std::erase_if(run.gates, [](const Gate& g) { return !g.alive; });
}

Vec2 turret_pos(int slot) {
    return {TURRET_SLOT_X[slot], TURRET_Y};
}

int pick_front_enemy(const Run& run, Vec2 from, float range) {
    int best = -1;
    float best_y = -1e9f;
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        const auto& e = run.enemies[static_cast<size_t>(i)];
        if (e.dead || e.pos.y < HUD_TOP) continue;
        if (e.pos.distance_to(from) > range) continue;
        float score = e.pos.y + (e.type == EnemyType::Boss ? 200.0f : 0.0f) + (e.elite ? 80.0f : 0.0f);
        if (score > best_y) {
            best_y = score;
            best = i;
        }
    }
    return best;
}

void chain_lightning(Ctx& c, int first, int count, float dmg, Vec2 origin, Color color, bool can_crit) {
    auto& run = c.run;
    std::vector<int> visited;
    Bolt bolt;
    bolt.color = color;
    bolt.points.push_back(origin);
    int cur = first;
    while (cur != -1 && static_cast<int>(visited.size()) < count) {
        visited.push_back(cur);
        Vec2 p = run.enemies[static_cast<size_t>(cur)].pos;
        Vec2 last = bolt.points.back();
        for (int s = 1; s <= 3; ++s) {
            float t = static_cast<float>(s) / 4.0f;
            Vec2 mid = last + (p - last) * t;
            bolt.points.push_back(mid + Vec2{run.rng.uniform(-14, 14), run.rng.uniform(-14, 14)});
        }
        bolt.points.push_back(p);
        bool crit = can_crit && run.rng.chance(run.stats.crit_chance);
        damage_enemy(c, cur, crit ? dmg * run.stats.crit_mult : dmg, crit, false, 10.0f);
        particle(run, p, {}, 0.15f, 10.0f, 34.0f, color, PGlow, 0.0f);
        int next = -1;
        float bd = 170.0f * 170.0f;
        g_grid.query(p, 170.0f, [&](int i) {
            const auto& e = run.enemies[static_cast<size_t>(i)];
            if (e.dead || std::ranges::find(visited, i) != visited.end()) return;
            float d = (e.pos - p).length_sq();
            if (d < bd) {
                bd = d;
                next = i;
            }
        });
        cur = next;
    }
    run.bolts.push_back(std::move(bolt));
}

void update_turrets(Ctx& c, float dt) {
    auto& run = c.run;
    auto d = difficulty_for(run);
    for (int s = 0; s < TURRET_SLOTS; ++s) {
        auto& t = run.turrets[static_cast<size_t>(s)];
        if (t.type == TurretType::None) continue;
        Vec2 pos = turret_pos(s);
        const auto& def = turret_def(t.type);
        t.cooldown -= dt;
        t.recoil = std::max(0.0f, t.recoil - dt * 5.0f);
        if (t.target >= 0 &&
            (t.target >= static_cast<int>(run.enemies.size()) || run.enemies[static_cast<size_t>(t.target)].dead)) {
            t.target = -1;
        }
        if (t.target == -1 || t.type == TurretType::Laser || t.type == TurretType::Cannon) {
            t.target = pick_front_enemy(run, pos, def.range);
        }
        if (t.target == -1) continue;
        const auto& target = run.enemies[static_cast<size_t>(t.target)];
        Vec2 dir = target.pos - pos;
        float want = std::atan2(dir.y, dir.x);
        float diff = std::remainder(want - t.aim, 2 * PI);
        t.aim += diff * std::min(1.0f, dt * 12.0f);
        if (t.cooldown > 0.0f) continue;
        float dmg = turret_damage(t.type, t.level, d.stage);
        t.cooldown = turret_interval(t.type, t.level);
        Vec2 muzzle = pos + Vec2{std::cos(t.aim), std::sin(t.aim)} * TURRET_BARREL;
        switch (t.type) {
        case TurretType::Cannon: {
            float flight = 0.55f;
            Vec2 lead = target.at_wall ? target.pos : target.pos + Vec2{0, target.speed * flight};
            run.shells.push_back({muzzle, lead, 0.0f, flight, dmg, 62.0f + 8.0f * static_cast<float>(t.level), false});
            t.recoil = 1.0f;
            c.game.sound.play(Sfx::Cannon, 0.45f, run.rng.uniform(0.9f, 1.1f));
            particle(run, muzzle, Vec2{std::cos(t.aim), std::sin(t.aim)} * 60.0f, 0.5f, 10.0f, 30.0f,
                     Color{200, 200, 200, 160}, PSmoke, 3.0f);
            break;
        }
        case TurretType::Tesla:
            chain_lightning(c, t.target, tesla_chains(t.level), dmg, pos + TESLA_ORB, Color{140, 210, 255, 255}, false);
            c.game.sound.play(Sfx::Zap, 0.4f, run.rng.uniform(0.9f, 1.2f));
            break;
        case TurretType::Flamer: {
            Vec2 fdir = Vec2{std::cos(t.aim), std::sin(t.aim)};
            float cone = std::cos(20.0f * DEG);
            for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead) continue;
                Vec2 to = e.pos - pos;
                float dist = to.length();
                if (dist > def.range + e.radius || dist < 1.0f) continue;
                if ((to * (1.0f / dist)).dot(fdir) < cone) continue;
                e.burn_timer = 2.0f;
                e.burn_dps = std::max(e.burn_dps, dmg * 4.0f);
                damage_enemy(c, i, dmg, false, false, 2.0f, false);
            }
            for (int k = 0; k < 3; ++k) {
                float a = t.aim + run.rng.uniform(-14, 14) * DEG;
                float spd = run.rng.uniform(380, 520);
                particle(run, muzzle, {std::cos(a) * spd, std::sin(a) * spd}, def.range / spd, 8.0f, 34.0f,
                         Color{255, static_cast<unsigned char>(run.rng.range(110, 200)), 40, 220}, PFire, 0.5f);
            }
            t.flame_timer -= dt;
            if (t.flame_timer <= 0.0f) {
                t.flame_timer = 0.22f;
                c.game.sound.play(Sfx::Flame, 0.25f, run.rng.uniform(0.8f, 1.2f));
            }
            break;
        }
        case TurretType::Laser: {
            Vec2 ldir = Vec2{std::cos(t.aim), std::sin(t.aim)};
            Vec2 end = muzzle + ldir * 1400.0f;
            for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead) continue;
                Vec2 to = e.pos - muzzle;
                float along = to.dot(ldir);
                if (along < 0) continue;
                Vec2 closest = muzzle + ldir * along;
                if (closest.distance_to(e.pos) < e.radius + 14.0f) damage_enemy(c, i, dmg, false, false, 30.0f);
            }
            Bolt beam;
            beam.points = {muzzle, end};
            beam.color = def.color;
            beam.width = 14.0f;
            beam.life = beam.max_life = 0.3f;
            run.bolts.push_back(beam);
            t.recoil = 1.0f;
            add_shake(run, 4.0f);
            c.game.sound.play(Sfx::Laser, 0.5f);
            break;
        }
        case TurretType::None:
        case TurretType::Count:
            break;
        }
    }
}

void update_shells(Ctx& c, float dt) {
    auto& run = c.run;
    for (auto& s : run.shells) {
        s.t += dt;
        if (s.t >= s.duration) {
            explode(c, s.to, s.radius, s.damage, s.grenade, Color{255, 150, 60, 255}, s.bomb);
        } else if (run.rng.chance(dt * 30.0f)) {
            float k = s.t / s.duration;
            Vec2 p = s.from + (s.to - s.from) * k;
            p.y -= std::sin(k * PI) * (s.bomb ? 0.0f : 120.0f);
            particle(run, p, {0, 0}, 0.4f, 5.0f, 14.0f, Color{180, 180, 180, 120}, PSmoke, 1.0f);
        }
    }
    std::erase_if(run.shells, [](const Shell& s) { return s.t >= s.duration; });
}

void update_airstrike(Ctx& c, float dt) {
    auto& run = c.run;
    run.airstrike_cd = std::max(0.0f, run.airstrike_cd - dt);
    if (run.plane_t < 0.0f) return;
    float prev = run.plane_t;
    run.plane_t += dt * 0.9f;
    int total = run.stats.airstrike_bombs();
    for (int i = 0; i < total; ++i) {
        float drop_at = 0.12f + 0.76f * static_cast<float>(i) / static_cast<float>(std::max(1, total - 1));
        if (prev < drop_at && run.plane_t >= drop_at) {
            float x = drop_at * VIRTUAL_W;
            Vec2 target{x + run.rng.uniform(-20, 20), run.plane_y + run.rng.uniform(-60, 60)};
            run.shells.push_back(
                {Vec2{x, run.plane_y - 200}, target, 0.0f, 0.45f, run.stats.airstrike_damage(), 80.0f, true});
        }
    }
    if (run.plane_t > 1.2f) run.plane_t = -1.0f;
}

void update_chain(Ctx& c, float dt) {
    auto& run = c.run;
    if (run.stats.chain_level <= 0) return;
    run.chain_timer -= dt;
    if (run.chain_timer > 0.0f) return;
    run.chain_timer = run.stats.chain_interval();
    int first = pick_front_enemy(run, {run.squad_x, SQUAD_Y}, 2000.0f);
    if (first == -1) return;
    float dmg =
        run.stats.damage * (2.0f + 0.5f * static_cast<float>(run.stats.chain_level)) * overflow_mult(run.soldiers);
    chain_lightning(c, first, run.stats.chain_targets(), dmg, {run.squad_x, SQUAD_Y - 30}, Color{190, 170, 255, 255},
                    run.stats.thunder_god);
    c.game.sound.play(Sfx::Zap, 0.35f, 0.8f);
}

void update_fx(Run& run, float dt) {
    for (auto& p : run.particles) {
        p.life -= dt;
        p.pos += p.vel * dt;
        p.vel = p.vel * std::exp(-p.drag * dt);
        p.vel.y += p.gravity * dt;
        p.rot += p.spin * dt;
        if (p.zgrav > 0.0f) {
            p.vz -= p.zgrav * dt;
            p.z += p.vz * dt;
            if (p.z <= 0.0f) {
                p.z = 0.0f;
                p.vz = std::abs(p.vz) > 90.0f ? -p.vz * 0.35f : 0.0f;
                p.vel = p.vel * 0.45f;
                p.spin *= 0.4f;
            }
        }
    }
    std::erase_if(run.particles, [](const Particle& p) { return p.life <= 0.0f; });
    for (auto& t : run.texts) {
        t.life -= dt;
        t.pos.y += t.vy * dt;
        t.vy *= std::exp(-3.0f * dt);
    }
    std::erase_if(run.texts, [](const FloatText& t) { return t.life <= 0.0f; });
    for (auto& b : run.bolts) b.life -= dt;
    std::erase_if(run.bolts, [](const Bolt& b) { return b.life <= 0.0f; });
}

void update_coins(Ctx& c, float dt) {
    auto& run = c.run;
    for (auto& f : run.coin_flies) {
        f.t += dt;
        if (f.t < 0.3f) {
            f.pos += f.vel * dt;
            f.vel = f.vel * std::exp(-4.0f * dt);
        } else {
            Vec2 to = COIN_HUD - f.pos;
            float spd = 300.0f + 2400.0f * (f.t - 0.3f);
            float dist = to.length();
            if (dist < spd * dt + 8.0f) {
                f.t = -100.0f;
                run.coins += f.value;
                run.tally.coins_earned += f.value;
                c.game.sound.play(Sfx::Coin, 0.22f, run.rng.uniform(0.95f, 1.2f));
            } else {
                f.pos += to * (spd * dt / dist);
            }
        }
    }
    std::erase_if(run.coin_flies, [](const CoinFly& f) { return f.t < -50.0f; });
}

// The director keeps a run tense: dominating raises the threat, collapsing lowers it.
std::string boss_powers_text(const BossGenome& g) {
    std::string out;
    for (int i = 0; i < g.power_count; ++i) {
        if (i) out += " + ";
        out += boss_power_name(g.powers[static_cast<size_t>(i)]);
    }
    return out;
}

void adjust_threat_after_wave(Ctx& c) {
    auto& run = c.run;
    float lost = (run.wave_wall_start - run.wall_hp) / run.stats.wall_max;
    if (lost < 0.01f)
        run.threat *= 1.35f;
    else if (lost < 0.05f)
        run.threat *= 1.12f;
    else if (lost > 0.4f)
        run.threat *= 0.7f;
    else if (lost > 0.2f)
        run.threat *= 0.85f;
    run.threat = std::clamp(run.threat, 0.6f, 8.0f);
    int tier = std::max(0, static_cast<int>((run.threat - 1.0f) / 0.35f));
    if (tier > run.threat_tier) {
        run.hint = std::format("THE HORDE ADAPTS - THREAT LEVEL {}", tier + 1);
        run.hint_timer = 2.5f;
        c.game.sound.play(Sfx::BossRoar, 0.5f, 1.4f);
    }
    run.threat_tier = tier;
}

void update_director(Run& run, float dt) {
    run.wall_damage_rate *= std::exp(-dt / 3.0f);
}

void begin_wave(Ctx& c) {
    auto& run = c.run;
    autopilot_log_wave(c.game);
    run.wave++;
    run.ground_reset = true;
    auto d = difficulty_for(run);
    if (run.wave_theme_pending) {
        run.wave_theme_pending = false;
        apply_theme(run);
        run.banner = {run.theme.name, "NEW BATTLEGROUND", Color{255, 210, 70, 255}, 2.4f, 2.4f};
    }
    run.spawns = build_wave(d.stage, d.wave, run.rng);
    run.mod = roll_wave_mod(d.stage, d.wave, run.rng);
    run.wave_wall_start = run.wall_hp;
    float count_mult = threat_count_mult(run.threat);
    if (run.mod == WaveMod::Swarm) count_mult *= 2.0f;
    if (run.mod == WaveMod::Giants) count_mult *= 0.65f;
    if (count_mult != 1.0f) {
        std::vector<SpawnEvent> scaled;
        for (const auto& ev : run.spawns) {
            if (ev.type == EnemyType::Boss) {
                scaled.push_back(ev);
                continue;
            }
            float n = count_mult;
            while (n >= 1.0f || run.rng.chance(n)) {
                SpawnEvent copy = ev;
                if (!scaled.empty() && n < count_mult) {
                    copy.time += run.rng.uniform(0.0f, 1.5f);
                    copy.x = std::clamp(copy.x + run.rng.uniform(-60, 60), 30.0f, VIRTUAL_W - 30.0f);
                }
                scaled.push_back(copy);
                n -= 1.0f;
                if (n <= 0.0f) break;
            }
            if (scaled.size() > 1200) break;
        }
        std::ranges::sort(scaled, {}, &SpawnEvent::time);
        run.spawns = std::move(scaled);
    }
    run.spawn_cursor = 0;
    run.wave_time = 0.0f;
    run.phase = WavePhase::Fighting;
    run.gate_timer = std::min(run.gate_timer, 4.0f);
    bool boss = d.wave == WAVES_PER_STAGE || d.wave == MINI_BOSS_WAVE;
    std::string title =
        run.endless ? std::format("WAVE {}", run.wave) : std::format("WAVE {}/{}", run.wave, WAVES_PER_STAGE);
    if (boss) {
        const BossGenome& genome = d.wave == WAVES_PER_STAGE ? run.boss : run.mini_boss;
        run.banner = {genome.name, std::format("{}  -  {}", title, boss_powers_text(genome)), Color{255, 80, 70, 255},
                      3.0f, 3.0f};
        c.game.play_music("boss");
        c.game.sound.play(Sfx::BossRoar, 0.9f);
    } else {
        run.banner = {title, run.wave == 1 ? "HOLD THE WALL!" : "HERE THEY COME", WHITE, 2.0f, 2.0f};
    }
    if (run.mod != WaveMod::None) {
        run.banner = {wave_mod_def(run.mod).name, wave_mod_def(run.mod).desc, Color{255, 150, 60, 255}, 2.6f, 2.6f};
    }
}

void finish_run(Game& game, bool won) {
    auto& run = *game.run;
    if (run.finished) return;
    run.finished = true;
    auto& meta = game.meta;
    RunResult r;
    r.won = won;
    r.endless = run.endless;
    r.stage = run.stage;
    r.waves_cleared = won ? run.wave : std::max(0, run.wave - 1);
    r.tally = run.tally;
    r.gems = gems_for_run(difficulty_for(run).stage, r.waves_cleared, 0, won) + run.gems_found;
    if (run.endless) r.gems = r.waves_cleared * 4 + run.gems_found + (r.waves_cleared / 10) * 30;
    if (won && !run.endless) {
        r.stars = stars_for_wall(run.wall_hp / run.stats.wall_max);
        int before = meta.stars[static_cast<size_t>(std::clamp(run.stage, 0, MAX_STAGE))];
        r.new_stars = std::max(0, r.stars - before);
        r.new_unlock = meta.unlocked_stage == run.stage && run.stage < MAX_STAGE;
        meta.finish_stage(run.stage, r.stars);
    }
    int claimable_before = meta.claimable_missions();
    meta.gems += r.gems;
    meta.record(Lifetime::Kills, run.tally.kills);
    meta.record(Lifetime::BossKills, run.tally.bosses);
    meta.record(Lifetime::EliteKills, run.tally.elites);
    meta.record(Lifetime::GatesTaken, run.tally.gates_taken);
    meta.record(Lifetime::MostSoldiers, run.tally.max_soldiers);
    meta.record(Lifetime::CardsTaken, run.tally.cards_taken);
    meta.record(Lifetime::CoinsEarned, run.tally.coins_earned);
    if (run.endless) meta.record(Lifetime::BestEndlessWave, r.waves_cleared);
    meta.tutorial_done = true;
    if (meta.claimable_missions() > claimable_before) game.toast("NEW MISSION REWARD!", Color{255, 210, 70, 255});
    game.result = r;
    game.save();
    game.states.request(GameStateId::Results);
}

void open_cards(Ctx& c) {
    auto& run = c.run;
    run.offer = roll_cards(run.stats, run.rng);
    run.picking = true;
    run.pick_anim = 0.0f;
    c.game.sound.play(Sfx::LevelUp, 0.8f);
    Vec2 at{run.squad_x, SQUAD_Y};
    shockwave(run, at, 20.0f, 220.0f, 0.5f, Color{255, 220, 90, 220});
    for (int i = 0; i < 24; ++i) {
        float a = run.rng.uniform(-PI, 0.0f);
        float sp = run.rng.uniform(160, 380);
        additive_particle(run, at, {std::cos(a) * sp, std::sin(a) * sp}, run.rng.uniform(0.5f, 0.9f), 7.0f, 2.0f,
                          Color{255, 220, 90, 255}, PStar, 2.5f);
    }
}

} // namespace

std::string loot_label(const Loot& f) {
    switch (f.kind) {
    case LootKind::Soldiers:
        return std::format("+{} {}S", f.amount, unit_def(f.unit).name);
    case LootKind::Turret:
        return std::format("FREE {}", turret_def(f.turret).name);
    case LootKind::Card:
        return "FREE UPGRADE";
    case LootKind::Coins:
        return std::format("+{} COINS", f.amount);
    case LootKind::Gems:
        return std::format("+{} GEMS", f.amount);
    }
    return "";
}

Difficulty difficulty_for(const Run& run) {
    if (!run.endless) return {run.stage, std::max(run.wave, 1)};
    int w = std::max(run.wave, 1);
    return {1 + (w - 1) / WAVES_PER_STAGE, (w - 1) % WAVES_PER_STAGE + 1};
}

int theme_key_for(const Run& run) {
    return run.endless ? 100 + difficulty_for(run).stage : run.stage;
}

void apply_theme(Run& run) {
    int key = theme_key_for(run);
    run.theme = make_theme(key);
    run.boss = make_boss(key, false);
    run.mini_boss = make_boss(key, true);
}

void start_run(Game& game) {
    game.run = std::make_unique<Run>();
    auto& run = *game.run;
    auto& meta = game.meta;
    run.rng = Rng{static_cast<uint64_t>(std::time(nullptr)) * 2654435761ull +
                  static_cast<uint64_t>(GetRandomValue(0, 1 << 30))};
    run.stage = game.request.endless ? 1 : game.request.stage;
    run.endless = game.request.endless;
    run.stats = meta.initial_stats();
    run.squad = meta.squad();
    for (int u = 0; u < UNIT_COUNT; ++u)
        run.unit_levels[static_cast<size_t>(u)] = std::max(1, meta.unit_levels[static_cast<size_t>(u)]);
    run.soldiers = meta.starting_soldiers();
    run.tally.max_soldiers = run.soldiers;
    run.coins = meta.starting_coins() + 40;
    run.wall_hp = run.stats.wall_max;
    run.rerolls = meta.rerolls();
    run.phase = WavePhase::Intro;
    run.phase_timer = 2.0f;
    run.airstrike_cd = 10.0f;
    apply_theme(run);
    run.start_power = squad_power(run);
    run.banner = {run.endless ? "ENDLESS" : std::format("STAGE {}", run.stage),
                  run.endless ? "HOW LONG CAN YOU LAST?" : run.theme.name, Color{255, 210, 70, 255}, 2.4f, 2.4f};
    if (!meta.tutorial_done) {
        run.hint = "DRAG, OR HOLD A/D OR ARROWS, TO MOVE";
        run.hint_timer = 4.5f;
    }
    game.play_music(run.stage % 2 ? "battle" : "battle2");
}

void pick_card(Game& game, int index) {
    auto& run = *game.run;
    if (!run.picking || index < 0 || index > 2) return;
    CardId id = run.offer[static_cast<size_t>(index)];
    float old_max = run.stats.wall_max;
    auto fx = apply_card(run.stats, id);
    run.wall_hp += run.stats.wall_max - old_max;
    if (fx.repair_fraction > 0.0f) run.wall_hp = run.stats.wall_max;
    if (fx.call_in) {
        std::vector<UnitType> options;
        for (int u = 0; u < UNIT_COUNT; ++u) {
            auto ut = static_cast<UnitType>(u);
            if (std::ranges::find(run.squad, ut) == run.squad.end()) options.push_back(ut);
        }
        if (!options.empty()) {
            UnitType pick = options[static_cast<size_t>(run.rng.range(0, static_cast<int>(options.size()) - 1))];
            run.squad.push_back(pick);
            float_text(run, std::format("{} JOINED!", unit_def(pick).name), {VIRTUAL_W * 0.5f, 700},
                       Color{140, 220, 255, 255}, 34.0f, 1.6f);
        }
    }
    if (fx.soldiers > 0) {
        int add = fx.soldiers * std::max(1, run.soldiers / 25);
        run.soldiers = std::min(MAX_SOLDIERS, run.soldiers + add);
        run.soldier_pop = 1.0f;
        run.tally.max_soldiers = std::max(run.tally.max_soldiers, run.soldiers);
    }
    run.tally.cards_taken++;
    run.pending_levels--;
    run.picking = false;
    game.sound.play(Sfx::CardPick, 0.9f);
    Ctx c{game, run};
    float_text(run, card_def(id).name, {VIRTUAL_W * 0.5f, 760}, Color{255, 230, 120, 255}, 40.0f, 1.2f);
    if (card_def(id).rarity == Rarity::Evolution) {
        run.flash = 0.6f;
        run.zoom_kick = 1.0f;
        run.banner = {"EVOLUTION!", card_def(id).name, Color{255, 170, 40, 255}, 2.2f, 2.2f};
        game.sound.play(Sfx::Victory, 0.8f, 1.3f);
        Vec2 at{run.squad_x, SQUAD_Y};
        for (int r = 0; r < 3; ++r)
            shockwave(run, at, 30.0f, 300.0f + 120.0f * static_cast<float>(r), 0.5f + 0.15f * static_cast<float>(r),
                      Color{255, 170, 40, 220});
    }
    if (run.pending_levels > 0) open_cards(c);
}

void reroll_cards(Game& game) {
    auto& run = *game.run;
    if (!run.picking || run.rerolls <= 0) return;
    run.rerolls--;
    run.offer = roll_cards(run.stats, run.rng);
    run.pick_anim = 0.0f;
    game.sound.play(Sfx::CardPick, 0.6f, 1.3f);
}

void build_or_upgrade(Game& game, int slot, TurretType type) {
    auto& run = *game.run;
    auto& t = run.turrets[static_cast<size_t>(slot)];
    if (t.type != TurretType::None && t.type != type) return;
    if (t.level >= TURRET_MAX_LEVEL) return;
    int cost = turret_upgrade_cost(type, t.level);
    if (run.coins < cost) return;
    run.coins -= cost;
    t.type = type;
    t.level++;
    t.cooldown = 0.3f;
    game.meta.record(Lifetime::TurretsBuilt, t.level == 1 ? 1 : 0);
    game.sound.play(Sfx::Build, 0.9f);
    Vec2 p = turret_pos(slot);
    burst(run, p, 20, Color{255, 230, 120, 255}, 260.0f, 5.0f, PStar, 0.6f);
    particle(run, p, {}, 0.4f, 20.0f, 90.0f, Color{255, 255, 200, 200}, PRing, 0.0f);
    float_text(run, t.level == 1 ? "BUILT!" : std::format("LEVEL {}", t.level), p + Vec2{0, -50},
               Color{255, 230, 120, 255}, 30.0f);
}

void sell_turret(Game& game, int slot) {
    auto& run = *game.run;
    auto& t = run.turrets[static_cast<size_t>(slot)];
    if (t.type == TurretType::None) return;
    int refund = turret_refund(t.type, t.level);
    run.coins += refund;
    float_text(run, std::format("+{}", refund), turret_pos(slot) + Vec2{0, -40}, Color{255, 220, 80, 255}, 30.0f);
    t = Turret{};
    game.sound.play(Sfx::Coin, 0.6f);
}

int repair_cost(const Run& run) {
    return 80 + 60 * run.repairs;
}

void repair_wall(Game& game) {
    auto& run = *game.run;
    int cost = repair_cost(run);
    if (run.coins < cost || run.wall_hp >= run.stats.wall_max) return;
    run.coins -= cost;
    run.repairs++;
    run.wall_hp = std::min(run.stats.wall_max, run.wall_hp + run.stats.wall_max * 0.2f);
    game.sound.play(Sfx::Build, 0.9f, 0.8f);
    for (int i = 0; i < 24; ++i) {
        Vec2 p{run.rng.uniform(0, VIRTUAL_W), run.rng.uniform(WALL_TOP, WALL_BOTTOM)};
        additive_particle(run, p, {0, -60}, 0.7f, 6.0f, 1.0f, Color{120, 255, 150, 255}, PGlow, 1.0f);
    }
    float_text(run, "+20% WALL", {VIRTUAL_W * 0.5f, WALL_TOP - 30}, Color{120, 255, 150, 255}, 34.0f);
}

void trigger_frenzy(Game& game) {
    auto& run = *game.run;
    if (run.rage < 1.0f || run.frenzy_t > 0.0f || run.phase == WavePhase::Lost) return;
    run.rage = 0.0f;
    run.frenzy_t = 6.0f;
    run.zoom_kick = 0.8f;
    run.flash = 0.35f;
    run.banner = {"OMAR'S RALLY!", "YALLA! DOUBLE FIRE RATE", Color{255, 190, 50, 255}, 1.6f, 1.6f};
    game.sound.play(Sfx::BossRoar, 0.7f, 1.6f);
    game.sound.play(Sfx::LevelUp, 0.8f, 0.8f);
    Vec2 at{run.squad_x, SQUAD_Y};
    for (int r = 0; r < 2; ++r)
        shockwave(run, at, 30.0f, 260.0f + 140.0f * static_cast<float>(r), 0.45f, Color{255, 190, 50, 220});
}

void trigger_ali(Game& game) {
    auto& run = *game.run;
    if (run.ali_charge < 1.0f || run.ali_t >= 0.0f || run.phase == WavePhase::Lost) return;
    run.ali_charge = 0.0f;
    run.ali_t = 0.0f;
    run.banner = {"ALI'S PEACE SIGN!", "THE HORDE IS CHARMED", Color{255, 130, 170, 255}, 1.8f, 1.8f};
    game.sound.play(Sfx::GateGood, 1.0f, 1.3f);
    game.sound.play(Sfx::Gem, 0.9f, 0.8f);
    for (auto& e : run.enemies) {
        if (e.dead) continue;
        e.charm = e.type == EnemyType::Boss ? 1.5f : 3.5f;
    }
    Vec2 at{VIRTUAL_W * 0.5f, WALL_TOP - 60};
    for (int r = 0; r < 3; ++r) {
        shockwave(run, at, 30.0f, 400.0f + 200.0f * static_cast<float>(r), 0.6f + 0.2f * static_cast<float>(r),
                  Color{255, 140, 190, 200});
    }
    for (int i = 0; i < 30; ++i) {
        float a = run.rng.uniform(-PI, 0.0f);
        float sp = run.rng.uniform(200, 520);
        additive_particle(run, at, {std::cos(a) * sp, std::sin(a) * sp}, run.rng.uniform(0.8f, 1.4f), 9.0f, 3.0f,
                          i % 2 ? Color{255, 140, 190, 255} : Color{255, 230, 120, 255}, PStar, 1.5f);
    }
}

void trigger_airstrike(Game& game) {
    auto& run = *game.run;
    if (run.airstrike_cd > 0.0f || run.plane_t >= 0.0f) return;
    std::array<int, 16> bands{};
    for (const auto& e : run.enemies) {
        if (e.dead || e.pos.y < HUD_TOP) continue;
        int b = std::clamp(static_cast<int>((e.pos.y - HUD_TOP) / 60.0f), 0, 15);
        bands[static_cast<size_t>(b)] += e.type == EnemyType::Boss ? 20 : 1;
    }
    int best = 13;
    int best_n = -1;
    for (int b = 0; b < 15; ++b) {
        int n = bands[static_cast<size_t>(b)] + bands[static_cast<size_t>(b) + 1];
        if (n > best_n) {
            best_n = n;
            best = b;
        }
    }
    run.plane_y = std::min(WALL_TOP - 50.0f, HUD_TOP + 60.0f * static_cast<float>(best) + 60.0f);
    run.plane_t = 0.0f;
    run.airstrike_cd = run.stats.airstrike_cooldown();
    game.sound.play(Sfx::Laser, 0.5f, 0.5f);
}

void gallery_explosion(Run& run, Vec2 p, float radius) {
    explosion_fx(run, p, radius, Color{255, 150, 60, 255});
}

void gallery_kill(Game& game, int idx) {
    Ctx c{game, *game.run};
    g_grid.build(game.run->enemies);
    kill_enemy(c, idx);
    game.run->coin_flies.clear();
}

void gallery_chain(Game& game, int first, int count, Vec2 origin) {
    Ctx c{game, *game.run};
    g_grid.build(game.run->enemies);
    chain_lightning(c, first, count, 1.0f, origin, Color{190, 170, 255, 255}, false);
}

void gallery_gate_pass(Game& game, Gate& gate) {
    Ctx c{game, *game.run};
    apply_gate_to_squad(c, gate);
}

void gallery_step(Run& run, float dt) {
    update_fx(run, dt);
}

void gallery_step_full(Game& game, float dt) {
    Ctx c{game, *game.run};
    update_boss_deaths(c, dt);
    update_corpses(*game.run, dt);
    update_collapse(c, dt);
    update_fx(*game.run, dt);
}

void gallery_boss_death(Game& game, Vec2 p, float r) {
    game.run->boss_deaths.push_back({p, r, 0.0f, false, 10, 0.0f});
}

void gallery_corpses(Game& game, int n) {
    spawn_corpses(*game.run, n);
}

void update_run(Game& game, float real_dt) {
    auto& run = *game.run;
    if (run.finished) return;
    Ctx c{game, run};

    run.shake = std::max(0.0f, run.shake - real_dt * 40.0f);
    run.wall_jolt = std::max(0.0f, run.wall_jolt - real_dt * 6.0f);
    run.frenzy_t = std::max(0.0f, run.frenzy_t - real_dt);
    if (run.ali_t >= 0.0f) {
        run.ali_t += real_dt;
        if (run.ali_t > 2.2f) run.ali_t = -1.0f;
    } else if (run.phase == WavePhase::Fighting) {
        run.ali_charge = std::min(1.0f, run.ali_charge + real_dt / 45.0f);
    }
    run.zoom_kick = std::max(0.0f, run.zoom_kick - real_dt * 2.5f);
    run.flash = std::max(0.0f, run.flash - real_dt * 2.0f);
    run.hint_timer = std::max(0.0f, run.hint_timer - real_dt);
    run.banner.timer = std::max(0.0f, run.banner.timer - real_dt);
    run.soldier_pop = std::max(0.0f, run.soldier_pop - real_dt * 3.0f);

    if (run.picking) {
        run.pick_anim += real_dt;
        return;
    }
    if (run.hitstop > 0.0f) {
        run.hitstop -= real_dt;
        return;
    }

    float scale = 1.0f;
    if (run.slowmo > 0.0f) {
        run.slowmo -= real_dt;
        scale = 0.3f;
    }
    if (run.build_slot >= 0) scale = std::min(scale, 0.3f);
    float dt = real_dt * scale;
    run.time += dt;
    run.tally.time += dt;
    run.shot_sound_cd -= real_dt;

    int visible = visible_soldiers(run.soldiers);
    run.squad_target_x = clamp_squad_x(run.squad_target_x, visible);
    run.squad_x += (run.squad_target_x - run.squad_x) * std::min(1.0f, real_dt * 14.0f);
    run.squad_x = clamp_squad_x(run.squad_x, visible);
    for (auto& r : run.soldier_recoil) r = std::max(0.0f, r - real_dt * 8.0f);
    for (auto& j : run.soldier_join) j = std::max(0.0f, j - real_dt * 2.5f);
    // New formation slots drop in with a bounce and a puff of dust.
    if (visible > run.shown_soldiers) {
        for (int i = run.shown_soldiers; i < visible; ++i) {
            run.soldier_join[static_cast<size_t>(i)] = 1.0f + static_cast<float>(i - run.shown_soldiers) * 0.04f;
            Vec2 p = Vec2{run.squad_x, SQUAD_Y} + soldier_offset(i, visible);
            particle(run, p + Vec2{0, 8}, {0, 0}, 0.4f, 6.0f, 18.0f, Color{200, 190, 170, 150}, PSmoke, 0.0f);
        }
    }
    run.shown_soldiers = visible;
    run.wall_flash = std::max(0.0f, run.wall_flash - real_dt);

    if (run.combo_timer > 0.0f) {
        run.combo_timer -= dt;
        if (run.combo_timer <= 0.0f) run.combo = 0;
    }

    g_grid.build(run.enemies);

    bool alive_phase = run.phase != WavePhase::Lost && run.phase != WavePhase::Won;
    switch (run.phase) {
    case WavePhase::Intro:
    case WavePhase::Break:
        run.phase_timer -= dt;
        if (run.phase_timer <= 0.0f) begin_wave(c);
        break;
    case WavePhase::Fighting: {
        run.wave_time += dt;
        while (run.spawn_cursor < run.spawns.size() && run.spawns[run.spawn_cursor].time <= run.wave_time) {
            spawn_enemy(run, run.spawns[run.spawn_cursor]);
            run.spawn_cursor++;
        }
        bool any_alive =
            !run.new_enemies.empty() || std::ranges::any_of(run.enemies, [](const Enemy& e) { return !e.dead; });
        if (run.spawn_cursor >= run.spawns.size() && !any_alive) {
            auto d = difficulty_for(run);
            if (!run.endless && run.wave >= WAVES_PER_STAGE) {
                run.phase = WavePhase::Won;
                run.phase_timer = 3.2f;
                run.banner = {"VICTORY!", std::format("STAGE {} CLEARED", run.stage), Color{255, 220, 80, 255}, 3.2f,
                              3.2f};
                game.sound.play(Sfx::Victory, 1.0f);
                for (int k = 0; k < 120; ++k) {
                    Color conf = Color{static_cast<unsigned char>(run.rng.range(120, 255)),
                                       static_cast<unsigned char>(run.rng.range(120, 255)),
                                       static_cast<unsigned char>(run.rng.range(120, 255)), 255};
                    particle(run, {run.rng.uniform(0, VIRTUAL_W), -10},
                             {run.rng.uniform(-60, 60), run.rng.uniform(100, 400)}, 3.0f, 7.0f, 7.0f, conf, PDisc, 0.3f,
                             120.0f);
                }
            } else {
                adjust_threat_after_wave(c);
                int bonus = 15 + 6 * d.wave + 10 * (d.stage - 1);
                run.coins += bonus;
                run.tally.coins_earned += bonus;
                run.phase = WavePhase::Break;
                run.phase_timer = 3.0f;
                run.banner = {"WAVE CLEARED", std::format("+{} COINS", bonus), Color{120, 255, 150, 255}, 2.2f, 2.2f};
                game.sound.play(Sfx::GateGood, 0.7f, 0.8f);
                if (!run.build_hint_shown && !game.meta.tutorial_done && run.coins >= 60) {
                    run.build_hint_shown = true;
                    run.hint = "TAP A SLOT ON THE WALL\nTO BUILD A TURRET";
                    run.hint_timer = 5.0f;
                }
                if (run.endless && d.wave == WAVES_PER_STAGE) {
                    game.play_music(d.stage % 2 ? "battle2" : "battle");
                    run.wave_theme_pending = true;
                }
            }
        }
        break;
    }
    case WavePhase::Won:
    case WavePhase::Lost:
        run.phase_timer -= real_dt;
        if (run.phase_timer <= 0.0f) {
            finish_run(game, run.phase == WavePhase::Won);
            return;
        }
        break;
    }

    if (alive_phase) fire_squad(c, dt);
    update_bullets(c, dt);
    update_enemies(c, dt);
    update_acids(c, dt);
    update_gates(c, dt);
    update_loot(c, dt);
    update_turrets(c, dt);
    update_shells(c, dt);
    update_airstrike(c, dt);
    if (alive_phase) update_chain(c, dt);
    update_coins(c, dt);
    update_boss_deaths(c, dt);
    update_corpses(run, dt);
    update_collapse(c, dt);
    update_fx(run, dt);
    update_director(run, dt);
    std::erase_if(run.enemies, [](const Enemy& e) { return e.dead; });
    run.enemies.insert(run.enemies.end(), run.new_enemies.begin(), run.new_enemies.end());
    run.new_enemies.clear();

    if (alive_phase && run.wall_hp <= 0.0f) {
        run.wall_hp = 0.0f;
        run.phase = WavePhase::Lost;
        run.phase_timer = 3.8f;
        run.collapse_t = 0.0f;
        spawn_corpses(run, visible_soldiers(run.soldiers));
        run.soldiers = 0;
        run.slowmo = 2.0f;
        run.banner = {"THE WALL HAS FALLEN", std::format("SURVIVED {} WAVES", std::max(0, run.wave - 1)),
                      Color{255, 80, 70, 255}, 3.0f, 3.0f};
        game.sound.play(Sfx::Defeat, 1.0f);
        game.sound.play(Sfx::BigExplosion, 1.0f, 0.7f);
        add_shake(run, 26.0f);
        for (int k = 0; k < 5; ++k) {
            explosion_fx(run, {run.rng.uniform(40, VIRTUAL_W - 40), WALL_TOP + run.rng.uniform(0, 60)}, 70.0f,
                         Color{255, 120, 60, 255});
        }
    }

    if (alive_phase) {
        while (run.xp >= xp_for_level(run.level)) {
            run.xp -= xp_for_level(run.level);
            run.level++;
            run.pending_levels++;
            run.card_delay = 0.35f;
        }
        if (run.pending_levels > 0) {
            run.card_delay -= real_dt;
            if (run.card_delay <= 0.0f) open_cards(c);
        }
    }
}

} // namespace ls
