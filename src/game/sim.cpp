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
#ifdef __EMSCRIPTEN__
constexpr size_t MAX_PARTICLES = 900;
constexpr size_t MAX_TEXTS = 40;
constexpr size_t BULLET_SOFT_CAP = 700;
#else
constexpr size_t MAX_PARTICLES = 2400;
constexpr size_t MAX_TEXTS = 75;
constexpr size_t BULLET_SOFT_CAP = 1600;
#endif

// Uniform grid rebuilt every frame; enemies are the only thing queried spatially.
struct Grid {
    static constexpr int CELL = 72;
    static constexpr float BIG_R = 40.0f;
    static constexpr int GW = VIRTUAL_W / CELL + 1;
    static constexpr int GH = VIRTUAL_H / CELL + 2;
    std::vector<int> head = std::vector<int>(GW * GH, -1);
    std::vector<int> next;
    std::vector<int> big;
    float max_small_r{0.0f};

    static int cell_of(float v, int max) { return std::clamp(static_cast<int>(v) / CELL, 0, max - 1); }
    static int row_of(float y) { return cell_of(y + CELL, GH); }

    void build(const std::vector<Enemy>& es) {
        std::ranges::fill(head, -1);
        next.assign(es.size(), -1);
        big.clear();
        max_small_r = 0.0f;
        for (int i = 0; i < static_cast<int>(es.size()); ++i) {
            const auto& e = es[static_cast<size_t>(i)];
            if (e.dead) continue;
            if (e.radius > BIG_R) {
                big.push_back(i);
                continue;
            }
            max_small_r = std::max(max_small_r, e.radius);
            int c = row_of(e.pos.y) * GW + cell_of(e.pos.x, GW);
            next[static_cast<size_t>(i)] = head[static_cast<size_t>(c)];
            head[static_cast<size_t>(c)] = i;
        }
    }

    template <typename F>
    bool find(Vec2 p, float r, F&& fn) const {
        float reach = r + max_small_r;
        int x0 = cell_of(p.x - reach, GW), x1 = cell_of(p.x + reach, GW);
        int y0 = row_of(p.y - reach), y1 = row_of(p.y + reach);
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                for (int i = head[static_cast<size_t>(cy * GW + cx)]; i != -1; i = next[static_cast<size_t>(i)]) {
                    if (fn(i)) return true;
                }
            }
        }
        for (int i : big) {
            if (fn(i)) return true;
        }
        return false;
    }

    template <typename F>
    void query(Vec2 p, float r, F&& fn) const {
        find(p, r, [&](int i) {
            fn(i);
            return false;
        });
    }

    template <typename F>
    void pairs(const std::vector<Enemy>& es, F&& fn) const {
        static constexpr int dx[4] = {1, -1, 0, 1};
        static constexpr int dy[4] = {0, 1, 1, 1};
        for (int cy = 0; cy < GH; ++cy) {
            for (int cx = 0; cx < GW; ++cx) {
                for (int i = head[static_cast<size_t>(cy * GW + cx)]; i != -1; i = next[static_cast<size_t>(i)]) {
                    for (int j = next[static_cast<size_t>(i)]; j != -1; j = next[static_cast<size_t>(j)]) fn(i, j);
                    for (int k = 0; k < 4; ++k) {
                        int nx = cx + dx[k];
                        int ny = cy + dy[k];
                        if (nx < 0 || nx >= GW || ny >= GH) continue;
                        for (int j = head[static_cast<size_t>(ny * GW + nx)]; j != -1;
                             j = next[static_cast<size_t>(j)]) {
                            fn(i, j);
                        }
                    }
                }
            }
        }
        for (size_t bi = 0; bi < big.size(); ++bi) {
            int b = big[bi];
            const auto& e = es[static_cast<size_t>(b)];
            float reach = e.radius + max_small_r;
            int x0 = cell_of(e.pos.x - reach, GW), x1 = cell_of(e.pos.x + reach, GW);
            int y0 = row_of(e.pos.y - reach), y1 = row_of(e.pos.y + reach);
            for (int cy = y0; cy <= y1; ++cy) {
                for (int cx = x0; cx <= x1; ++cx) {
                    for (int j = head[static_cast<size_t>(cy * GW + cx)]; j != -1; j = next[static_cast<size_t>(j)]) {
                        fn(b, j);
                    }
                }
            }
            for (size_t bj = bi + 1; bj < big.size(); ++bj) fn(b, big[bj]);
        }
    }
};

Grid g_grid;

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
    if (run.particles.size() >= MAX_PARTICLES) return;
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
    if (run.texts.size() >= MAX_TEXTS) return;
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

void spark_streak(Run& run, Vec2 pos, Vec2 vel, float life, float width, Color c, float drag = 3.2f) {
    if (run.particles.size() >= MAX_PARTICLES) return;
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.max_life = life;
    p.size = width;
    p.size_end = width * 0.25f;
    p.color = c;
    p.tex = PGlow;
    p.drag = drag;
    p.additive = true;
    p.streak = true;
    run.particles.push_back(p);
}

void gib(Run& run, Vec2 pos, Color c, float size, float speed, float up, float life = 1.1f) {
    if (run.particles.size() >= MAX_PARTICLES) return;
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
    float k = std::clamp(radius / 60.0f, 0.55f, 2.8f);
    // 1. Instant white-hot core flash + double shockwave rings (fast white pressure ring + colored outer blast wave).
    additive_particle(run, pos, {}, 0.14f, radius * 0.55f, radius * 2.15f, Color{255, 252, 232, 255}, PGlow, 0.0f);
    shockwave(run, pos, radius * 0.18f, radius * 1.15f, 0.19f, Color{255, 250, 225, 225});
    shockwave(run, pos, radius * 0.30f, radius * 1.65f, 0.32f, with_alpha(brighten(tint, 0.35f), 0.80f));
    // 2. Contrast dark billowing smoke plume behind the fire so explosions pop on bright biomes too.
    int smoke_n = radius >= 95.0f ? 8 : 5;
    for (int i = 0; i < smoke_n; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(18, 68) * std::min(1.6f, k);
        particle(run, pos + Vec2{std::cos(a), std::sin(a)} * radius * 0.25f, {std::cos(a) * s, std::sin(a) * s - 46.0f},
                 run.rng.uniform(0.55f, 1.05f), radius * 0.30f, radius * 0.85f, Color{42, 36, 36, 170}, PSmoke, 1.4f);
    }
    // 3. Layered fireball clouds: bright core yellow -> vivid tint -> deep fiery crimson outer tongues.
    int fire_n = radius >= 95.0f ? 12 : 8;
    for (int i = 0; i < fire_n; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(35, 135) * k;
        Color fire = i % 3 == 0   ? Color{255, 242, 165, 255}
                     : i % 3 == 1 ? mix(tint, Color{255, 215, 100, 255}, 0.35f)
                                  : darken(tint, 0.18f);
        additive_particle(run, pos + Vec2{std::cos(a), std::sin(a)} * radius * 0.12f,
                          {std::cos(a) * s, std::sin(a) * s}, run.rng.uniform(0.20f, 0.44f), radius * 0.30f,
                          radius * 0.68f, fire, PFire, 2.8f);
    }
    // 4. High-velocity incandescent directional spark streaks shooting radially outward!
    int streak_n = radius >= 95.0f ? 18 : 12;
    for (int i = 0; i < streak_n; ++i) {
        float a = run.rng.uniform(0, 2 * PI);
        float s = run.rng.uniform(220, 560) * std::min(1.6f, k);
        Color sc = i % 2 == 0 ? brighten(tint, 0.5f) : Color{255, 245, 195, 255};
        spark_streak(run, pos + Vec2{std::cos(a), std::sin(a)} * radius * 0.1f, {std::cos(a) * s, std::sin(a) * s},
                     run.rng.uniform(0.22f, 0.48f), radius >= 95.0f ? 4.2f : 3.2f, sc, 3.0f);
    }
    // 5. Bouncing charred shrapnel debris + blast crater decal.
    for (int i = 0; i < (radius >= 95.0f ? 8 : 5); ++i) {
        gib(run, pos, i % 2 ? Color{72, 60, 52, 255} : Color{42, 34, 30, 255}, 3.2f, 280.0f * k, 280.0f, 0.85f);
    }
    run.pending_decals.push_back({pos, radius * 0.85f, Color{26, 20, 18, 105}, run.rng.uniform(0, 360), 1});
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
void chain_lightning(Ctx& c, int first, int count, float dmg, Vec2 origin, Color color, bool can_crit);
void spawn_coins(Run& run, Vec2 pos, int value);
Color b_muzzle_color(UnitType u);

void apply_frost(Run& run, Enemy& e, float slow_val, float stacks) {
    e.slow_timer = std::max(e.slow_timer, 3.0f);
    e.slow_factor = std::min(e.slow_factor, 1.0f - std::clamp(slow_val, 0.1f, 0.7f));
    e.frost_stacks += stacks;
    float threshold = e.type == EnemyType::Boss ? 8.0f : 4.0f;
    if (e.frost_stacks >= threshold && e.freeze_timer <= 0.0f) {
        e.frost_stacks = 0.0f;
        e.freeze_timer = e.type == EnemyType::Boss ? 0.65f : 1.7f;
        e.burrowed = false;
        shockwave(run, e.pos, e.radius * 0.4f, e.radius * 1.9f, 0.28f, Color{150, 235, 255, 200});
        burst(run, e.pos, 8, Color{180, 240, 255, 255}, 180.0f, 4.5f, PStar, 0.45f);
    }
}

// Every damage source funnels through here so status effects, numbers and kills stay consistent.
void damage_enemy(Ctx& c, int idx, float dmg, bool crit, bool status, float knock = 18.0f, bool show = true,
                  bool armor_pierce = false, Color num_color = WHITE) {
    auto& e = c.run.enemies[static_cast<size_t>(idx)];
    if (e.dead) return;
    float eff_armor = armor_pierce ? 0.0f : std::clamp(e.armor + (e.warlord_buff > 0.0f ? 0.18f : 0.0f), 0.0f, 0.65f);
    if (e.corrode_timer > 0.0f) eff_armor *= 0.25f;
    if (status && eff_armor > 0.0f) dmg *= 1.0f - eff_armor;
    // Napalm coat & corrosion amplify incoming damage.
    if (e.burn_timer > 0.0f) dmg *= 1.2f;
    if (e.corrode_timer > 0.0f) dmg *= 1.25f;
    if (e.freeze_timer > 0.0f) dmg *= 1.2f;
    // Executioner card bonuses.
    const auto& s = c.run.stats;
    if (s.execute_bonus > 0.0f && e.hp < e.max_hp * 0.35f) dmg *= 1.0f + s.execute_bonus;
    if (s.boss_bonus > 0.0f && (e.type == EnemyType::Boss || e.elite)) dmg *= 1.0f + s.boss_bonus;

    e.hp -= dmg;
    e.flash = 0.09f;
    e.hurt = std::min(1.0f, e.hurt + 0.35f);
    float eff_knock = knock * formation_knock_mult(c.run.formation) * c.run.synergies.knock_mult();
    e.knock += eff_knock * (1.0f - e.knock_resist);
    c.run.tally.damage_dealt += dmg;
    if (status) {
        if (s.burn_ratio > 0.0f) {
            e.burn_timer = 2.5f;
            e.burn_dps = std::max(e.burn_dps, s.burn_dps() * c.run.synergies.elemental_mult());
        }
        if (s.slow > 0.0f) {
            e.slow_timer = std::max(e.slow_timer, 3.0f);
            e.slow_factor = std::min(e.slow_factor, 1.0f - s.slow);
        }
    }
    if (show && (crit || c.run.rng.chance(0.28f))) {
        Vec2 p = e.pos + Vec2{c.run.rng.uniform(-10, 10), -e.radius};
        if (crit) {
            float_text(c.run, short_num(dmg) + "!", p, Color{255, 220, 60, 255}, 30.0f);
        } else {
            float_text(c.run, short_num(dmg), p, num_color, 20.0f, 0.55f);
        }
    }
    if (e.hp <= 0.0f) kill_enemy(c, idx);
}

void explode(Ctx& c, Vec2 pos, float radius, float dmg, bool status, Color tint, bool big_sound = false,
             bool strip_armor = false, bool frost = false) {
    explosion_fx(c.run, pos, radius, tint);
    add_shake(c.run, std::min(18.0f, radius * (big_sound ? 0.16f : 0.12f)));
    if (big_sound && radius >= 80.0f) {
        c.run.zoom_kick = std::max(c.run.zoom_kick, 0.28f);
        c.run.flash = std::max(c.run.flash, 0.14f);
    }
    c.game.sound.play(big_sound ? Sfx::BigExplosion : Sfx::Explosion, big_sound ? 1.0f : 0.55f,
                      c.run.rng.uniform(0.9f, 1.1f));
    int hits[256];
    int hit_n = 0;
    g_grid.query(pos, radius, [&](int i) {
        if (hit_n >= 256) return;
        const auto& e = c.run.enemies[static_cast<size_t>(i)];
        if (e.dead || e.burrowed) return;
        float rr = radius + e.radius;
        if ((e.pos - pos).length_sq() < rr * rr) hits[hit_n++] = i;
    });
    for (int k = 0; k < hit_n; ++k) {
        int i = hits[k];
        auto& e = c.run.enemies[static_cast<size_t>(i)];
        if (strip_armor) e.armor = std::max(0.0f, e.armor - 0.15f);
        if (frost) apply_frost(c.run, e, 0.45f, 2.0f);
        damage_enemy(c, i, dmg, false, status, 40.0f, false);
    }
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
    bool was_slowed = e.slow_timer > 0.0f || e.freeze_timer > 0.0f;
    bool elite = e.elite;
    auto d = difficulty_for(run);

    run.tally.kills++;
    float twist_charge = run.theme.stage_twist == 1 ? 1.35f : 1.0f;
    if (run.frenzy_t <= 0.0f) {
        float gain = (type == EnemyType::Boss ? 0.25f : 0.0042f) * run.stats.ability_charge_mult * twist_charge;
        run.rage = std::min(1.0f, run.rage + gain);
    }
    if (run.salama_t < 0.0f) {
        float sgain = (type == EnemyType::Boss ? 0.30f : 0.0052f) * run.stats.ability_charge_mult * twist_charge;
        run.salama_charge = std::min(1.0f, run.salama_charge + sgain);
    }
    // Field Engineers card repairs the wall periodically on kills.
    if (run.stats.field_repair_level > 0 && run.wall_hp < run.stats.wall_max) {
        run.kills_since_repair++;
        if (run.kills_since_repair >= 35) {
            run.kills_since_repair = 0;
            float heal = run.stats.wall_max * (0.025f * static_cast<float>(run.stats.field_repair_level));
            run.wall_hp = std::min(run.stats.wall_max, run.wall_hp + heal);
            float_text(run, std::format("+{:.0f} WALL", heal), {VIRTUAL_W * 0.5f, WALL_TOP - 22},
                       Color{120, 255, 160, 255}, 24.0f, 1.0f);
        }
    }

    run.combo++;
    run.combo_timer = 2.4f;
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

    float combo_xp = 1.0f + std::min(0.4f, static_cast<float>(run.combo) * 0.0015f);
    run.xp_accum += static_cast<float>(e.xp) * xp_mult(d.wave) * run.xp_gain_mult * combo_xp;
    int gained_xp = static_cast<int>(run.xp_accum);
    if (gained_xp > 0) {
        run.xp += gained_xp;
        run.xp_accum -= static_cast<float>(gained_xp);
    }
    float biome_coin = (run.theme.hazard == StageHazard::Bountiful || run.theme.stage_twist == 2) ? 1.25f : 1.0f;
    int coins = static_cast<int>(std::round(static_cast<float>(e.coins) * run.stats.coin_mult * biome_coin));
    spawn_coins(run, pos, coins);

    Color blood = enemy_blood_color(type);
    Color body = enemy_body_color(type);
    if (run.rng.chance(type == EnemyType::Walker || type == EnemyType::Runner ? 0.5f : 1.0f)) {
        run.pending_decals.push_back(
            {pos, radius * run.rng.uniform(0.9f, 1.3f), with_alpha(blood, 0.4f), run.rng.uniform(0, 360), 0});
    }
    death_fx(run, pos, radius, body, blood,
             type == EnemyType::Boss || type == EnemyType::Brute || type == EnemyType::Warlord || elite);
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
        if (run.theme.hazard == StageHazard::ToxicMire) {
            // Spore barrels burst into lingering armor-melting toxic clouds!
            run.zones.push_back({ZoneKind::SporeCloud, pos, 115.0f, 7.0f, 7.0f, 0.0f, run.rng.uniform(0, 100), true});
            explode(c, pos, 130.0f, std::max(run.stats.damage * 16.0f, e.max_hp * 22.0f), true,
                    Color{150, 235, 80, 255}, true, true, false);
            float_text(run, "SPORE BURST!", pos + Vec2{0, -30}, Color{170, 255, 95, 255}, 38.0f, 1.0f);
        } else {
            explode(c, pos, 135.0f, std::max(run.stats.damage * 18.0f, e.max_hp * 25.0f), true,
                    Color{255, 140, 40, 255}, true);
            float_text(run, "BOOM!", pos + Vec2{0, -30}, Color{255, 200, 80, 255}, 40.0f, 1.0f);
        }
    }
    if (type == EnemyType::Exploder) {
        explode(c, pos, 95.0f, run.stats.damage * 6.0f, false, Color{200, 230, 80, 255});
    }
    if (run.stats.inferno && was_burning) {
        explode(c, pos, 62.0f, run.stats.burn_dps() * 1.5f * run.synergies.elemental_mult(), true,
                Color{255, 130, 40, 255});
    }
    if (run.stats.shatter && was_slowed) {
        for (int i = 0; i < 6; ++i) {
            float a = static_cast<float>(i) / 6.0f * 2 * PI + run.rng.uniform(0, 0.5f);
            Bullet b;
            b.pos = b.prev = pos;
            b.vel = {std::cos(a) * 700.0f, std::sin(a) * 700.0f};
            b.damage = run.stats.damage * 0.85f * run.synergies.elemental_mult();
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
    float threat = ev.type == EnemyType::Boss ? std::sqrt(run.threat) : run.threat;
    e.hp = e.max_hp = enemy_hp(ev.type, d.stage, d.wave, ev.mini_boss) * threat_hp_mult(threat) *
                      power_hp_mult(squad_power(run), run.start_power, d.wave);
    e.speed = st.speed * run.rng.uniform(0.88f, 1.12f) * (run.theme.stage_twist == 1 ? 1.18f : 1.0f);
    e.radius = st.radius;
    e.wall_dps = st.wall_dps * wall_damage_mult(run);
    e.knock_resist = st.knock_resist;
    e.xp = st.xp;
    e.coins = st.coins;
    e.mini_boss = ev.mini_boss;
    e.wobble_seed = run.rng.uniform(0, 100);
    e.anim = run.rng.uniform(0, 10);
    e.stop_y = ev.type == EnemyType::Healer ? run.rng.uniform(360, 560) : run.rng.uniform(420, 620);
    e.action_timer = run.rng.uniform(1.2f, 2.6f);
    e.burrow_timer = run.rng.uniform(2.0f, 3.8f);
    if (ev.type == EnemyType::Warlord) e.armor = 0.2f;
    if (run.theme.stage_twist == 2 && ev.type != EnemyType::Barrel && ev.type != EnemyType::Boss) {
        e.armor = std::max(e.armor, 0.16f);
    }
    if (ev.type == EnemyType::Boss) {
        e.pos.y = SPAWN_Y - 40;
        if (ev.mini_boss) {
            e.radius = 64;
            e.wall_dps *= 0.5f;
            e.xp = 25;
            e.coins = 30;
        } else if (run.theme.stage_twist == 4) {
            // Stage 10 Finale Boss: MALAKOR, ECLIPSE SOVEREIGN!
            e.radius = 94.0f;
            e.hp = e.max_hp = e.max_hp * 1.20f;
        }
        e.action_timer = 5.0f;
        e.power_timer = 3.2f;
    }
    switch (run.mod) {
    case WaveMod::Fast:
        e.speed *= 1.35f;
        break;
    case WaveMod::Armored:
        e.armor = std::max(e.armor, 0.35f);
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
    return run.stats.fire_rate * unit_def(u).rate_mult * unit_rate_mult(run.unit_levels[static_cast<size_t>(u)]) *
           formation_rate_mult(run.formation) * run.synergies.rate_mult();
}

float squad_power(const Run& run) {
    float total = 0.0f;
    float over = overflow_mult(run.soldiers);
    float dmg_bonus = formation_damage_mult(run.formation) * run.synergies.damage_mult();
    float crit_c = std::min(0.95f, run.stats.crit_chance + formation_crit_bonus(run.formation));
    for (size_t i = 0; i < run.squad.size(); ++i) {
        UnitType u = run.squad[i];
        const auto& def = unit_def(u);
        float per_shot = run.stats.damage * def.damage_mult * dmg_bonus *
                         unit_damage_mult(run.unit_levels[static_cast<size_t>(u)]) *
                         static_cast<float>(def.pellets + run.stats.multishot - 1);
        total += static_cast<float>(unit_soldiers(run, i)) * unit_shot_rate(run, u) * per_shot;
    }
    return total * over * (1.0f + crit_c * (run.stats.crit_mult - 1.0f));
}

float shots_per_second(const Run& run) {
    float total = 0.0f;
    for (size_t i = 0; i < run.squad.size(); ++i) {
        total += static_cast<float>(unit_soldiers(run, i)) * unit_shot_rate(run, run.squad[i]);
    }
    return total;
}

void init_stage_zones(Run& run) {
    run.zones.clear();
    run.boss_waves.clear();
    run.boss_beams.clear();
    run.wind_force = 0.0f;
    run.wind_timer = 0.0f;
    run.hazard_timer = 7.0f;
    Rng zrng{static_cast<uint64_t>(run.theme.key) * 6151ull + 31337ull};
    switch (run.theme.hazard) {
    case StageHazard::Permafrost:
        for (int i = 0; i < 3; ++i) {
            float x = 140.0f + static_cast<float>(i) * 220.0f + zrng.uniform(-35, 35);
            float y = 340.0f + static_cast<float>(i % 2) * 240.0f + zrng.uniform(-50, 50);
            run.zones.push_back(
                {ZoneKind::IceSlick, {x, y}, zrng.uniform(76, 98), 0.0f, -1.0f, 0.0f, zrng.uniform(0, 100), true});
        }
        break;
    case StageHazard::Magma: {
        int cnt = run.theme.stage_twist == 3 ? 4 : 3;
        for (int i = 0; i < cnt; ++i) {
            float x = 110.0f + static_cast<float>(i) * (500.0f / static_cast<float>(std::max(1, cnt - 1))) +
                      zrng.uniform(-35, 35);
            float y = 330.0f + static_cast<float>((i + 1) % 2) * 240.0f + zrng.uniform(-40, 40);
            run.zones.push_back({ZoneKind::LavaFissure,
                                 {x, y},
                                 zrng.uniform(62, 78),
                                 zrng.uniform(1.8f, 5.0f),
                                 -1.0f,
                                 0.0f,
                                 zrng.uniform(0, 100),
                                 true});
        }
        break;
    }
    case StageHazard::ToxicMire:
        for (int i = 0; i < 2; ++i) {
            float x = 180.0f + static_cast<float>(i) * 360.0f + zrng.uniform(-40, 40);
            float y = 420.0f + static_cast<float>(i) * 190.0f + zrng.uniform(-40, 40);
            run.zones.push_back({ZoneKind::SporeCloud, {x, y}, 84.0f, 0.0f, -1.0f, 0.0f, zrng.uniform(0, 100), true});
        }
        break;
    case StageHazard::Leylines: {
        int cnt = run.theme.stage_twist == 4 ? 4 : 3;
        for (int i = 0; i < cnt; ++i) {
            float x = 115.0f + static_cast<float>(i) * (490.0f / static_cast<float>(std::max(1, cnt - 1))) +
                      zrng.uniform(-25, 25);
            float y = 605.0f + static_cast<float>(i % 2) * 145.0f + zrng.uniform(-30, 30);
            run.zones.push_back({ZoneKind::Leyline, {x, y}, 74.0f, 0.0f, -1.0f, 0.0f, zrng.uniform(0, 100), true});
        }
        break;
    }
    case StageHazard::Bountiful:
    case StageHazard::Sandstorm:
        break;
    }
}

void update_stage_hazards(Ctx& c, float dt) {
    auto& run = c.run;
    // Sandstorm periodic gusts in Desert biome
    if (run.theme.hazard == StageHazard::Sandstorm && run.phase == WavePhase::Fighting) {
        if (run.wind_timer > 0.0f) {
            run.wind_timer -= dt;
            if (run.wind_timer <= 0.0f) run.wind_force = 0.0f;
        } else {
            run.hazard_timer -= dt;
            if (run.hazard_timer <= 0.0f) {
                run.hazard_timer = run.rng.uniform(11.0f, 16.0f);
                run.wind_timer = 4.5f;
                run.wind_force = run.rng.chance(0.5f) ? 85.0f : -85.0f;
                float_text(run, "SANDSTORM GUST!", {VIRTUAL_W * 0.5f, 240}, Color{245, 200, 115, 255}, 30.0f, 1.4f);
            }
        }
    }

    for (auto& z : run.zones) {
        if (!z.active) continue;
        if (z.max_life > 0.0f) {
            z.timer -= dt;
            if (z.timer <= 0.0f) {
                z.active = false;
                continue;
            }
        }
        switch (z.kind) {
        case ZoneKind::IceSlick:
            g_grid.query(z.pos, z.radius, [&](int i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead || e.type == EnemyType::Flyer || e.type == EnemyType::Barrel) return;
                if (e.pos.distance_to(z.pos) < z.radius + e.radius * 0.5f) {
                    e.slow_timer = std::max(e.slow_timer, 0.45f);
                    e.slow_factor = std::min(e.slow_factor, 0.65f);
                }
            });
            break;
        case ZoneKind::LavaFissure:
            z.timer -= dt * (run.theme.stage_twist == 3 ? 1.35f : 1.0f);
            if (z.timer <= 1.8f && z.timer > 0.0f) {
                z.charge = 1.0f - z.timer / 1.8f;
            } else if (z.timer <= 0.0f) {
                z.timer = run.rng.uniform(7.5f, 11.0f);
                z.charge = 0.0f;
                float dmg = std::max(30.0f, run.stats.damage * 5.0f);
                explode(c, z.pos, z.radius * 1.45f, dmg, false, Color{255, 110, 35, 255}, false, false, false);
                g_grid.query(z.pos, z.radius * 1.45f, [&](int i) {
                    auto& e = run.enemies[static_cast<size_t>(i)];
                    if (!e.dead && e.pos.distance_to(z.pos) < z.radius * 1.45f + e.radius) {
                        e.burn_timer = std::max(e.burn_timer, 3.0f);
                        e.burn_dps = std::max(e.burn_dps, dmg * 0.35f);
                    }
                });
            }
            break;
        case ZoneKind::SporeCloud:
            g_grid.query(z.pos, z.radius, [&](int i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead || e.type == EnemyType::Barrel) return;
                if (e.pos.distance_to(z.pos) < z.radius + e.radius * 0.5f) {
                    e.corrode_timer = std::max(e.corrode_timer, 1.5f);
                    float dot = std::max(4.0f, run.stats.damage * 0.35f) * dt;
                    e.hp -= dot;
                    run.tally.damage_dealt += dot;
                    if (e.hp <= 0.0f) kill_enemy(c, i);
                }
            });
            break;
        case ZoneKind::NapalmPool:
            g_grid.query(z.pos, z.radius, [&](int i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead || e.type == EnemyType::Barrel) return;
                if (e.pos.distance_to(z.pos) < z.radius + e.radius * 0.6f) {
                    e.burn_timer = std::max(e.burn_timer, 2.0f);
                    e.burn_dps = std::max(e.burn_dps, run.stats.damage * 2.2f);
                }
            });
            if (run.rng.chance(dt * 12.0f)) {
                float a = run.rng.uniform(0, 2 * PI);
                float r = run.rng.uniform(0, z.radius * 0.7f);
                additive_particle(run, z.pos + Vec2{std::cos(a) * r, std::sin(a) * r}, {0, -50}, 0.45f, 12.0f, 3.0f,
                                  Color{255, 130, 40, 220}, PFire, 1.5f);
            }
            break;
        case ZoneKind::Leyline:
        case ZoneKind::Vortex:
            break;
        }
    }
    std::erase_if(run.zones, [](const StageZone& z) { return !z.active; });
}

void fire_squad(Ctx& c, float dt) {
    auto& run = c.run;
    int visible = visible_soldiers(run.soldiers);
    if (visible <= 0 || run.squad.empty()) return;
    float over = overflow_mult(run.soldiers);
    float aim_cone = formation_aim_cone_deg(run.formation) * DEG;
    float dmg_mult = formation_damage_mult(run.formation) * run.synergies.damage_mult();
    float crit_chance = std::min(0.95f, run.stats.crit_chance + formation_crit_bonus(run.formation) +
                                            (run.frenzy_t > 0.0f ? 0.20f : 0.0f));

    static std::vector<int> front;
    front.clear();
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        const auto& e = run.enemies[static_cast<size_t>(i)];
        if (!e.dead && !e.burrowed && e.pos.y > HUD_TOP && e.pos.y < WALL_TOP + 10.0f) front.push_back(i);
    }
    size_t keep = std::min<size_t>(front.size(), 80);
    std::partial_sort(front.begin(), front.begin() + static_cast<long>(keep), front.end(), [&](int a, int b) {
        return run.enemies[static_cast<size_t>(a)].pos.y > run.enemies[static_cast<size_t>(b)].pos.y;
    });
    front.resize(keep);

    auto aim_for = [&](Vec2 origin, float max_range, bool elite_hunt) -> std::pair<float, int> {
        int best_idx = -1;
        float best_ang = -PI / 2;
        float best_score = -1e9f;
        for (int idx : front) {
            const auto& e = run.enemies[static_cast<size_t>(idx)];
            Vec2 d = e.pos - origin;
            if (d.y >= -1.0f || d.length() > max_range) continue;
            float off = std::atan2(d.x, -d.y);
            if (std::abs(off) > aim_cone) continue;
            if (!elite_hunt) return {-PI / 2 + off, idx};
            float prio = e.pos.y;
            if (e.type == EnemyType::Boss) prio += 900.0f;
            if (e.elite) prio += 600.0f;
            if (e.type == EnemyType::Healer || e.type == EnemyType::Warlord || e.type == EnemyType::Spitter)
                prio += 480.0f;
            if (prio > best_score) {
                best_score = prio;
                best_idx = idx;
                best_ang = -PI / 2 + off;
            }
        }
        return {best_ang, best_idx};
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
            int shot_num = ++run.unit_shot_counter[static_cast<size_t>(u)];
            run.volley_counter++;
            int si = static_cast<int>(slot) + k * n;
            if (si >= visible) si = static_cast<int>(slot);
            Vec2 off_cur = run.soldier_off_cur[static_cast<size_t>(si)];
            Vec2 origin = Vec2{run.squad_x, SQUAD_Y} + off_cur + muzzle_offset(static_cast<int>(u));
            run.soldier_recoil[static_cast<size_t>(si)] = 1.0f;
            float base_dmg =
                run.stats.damage * def.damage_mult * level_dmg * over * dmg_mult * (run.frenzy_t > 0.0f ? 1.30f : 1.0f);
            if (def.shot == Shot::Flame || def.shot == Shot::Cryo || def.shot == Shot::Volt) {
                base_dmg *= run.synergies.elemental_mult();
            }

            // Pulse Rounds (ShockwaveRounds card): every 20th shot blasts a kinetic shockwave!
            if (run.stats.shockwave_level > 0 && run.volley_counter % 20 == 0) {
                Vec2 pulse_at{run.squad_x, WALL_TOP - 30.0f};
                float rad = 125.0f + 25.0f * static_cast<float>(run.stats.shockwave_level);
                shockwave(run, pulse_at, 20.0f, rad, 0.35f, Color{130, 225, 255, 200});
                float pdmg = run.stats.damage * (1.2f + 0.55f * static_cast<float>(run.stats.shockwave_level)) * over;
                g_grid.query(pulse_at, rad, [&](int idx) {
                    auto& e = run.enemies[static_cast<size_t>(idx)];
                    if (!e.dead && !e.burrowed && e.pos.distance_to(pulse_at) < rad + e.radius) {
                        damage_enemy(c, idx, pdmg, false, false, 65.0f, false);
                    }
                });
            }

            if (def.shot == Shot::Grenade) {
                auto [ang, target] = aim_for(origin, 900.0f, false);
                Vec2 to = target >= 0 ? run.enemies[static_cast<size_t>(target)].pos
                                      : origin + Vec2{std::cos(ang), std::sin(ang)} * 520.0f;
                to += Vec2{run.rng.uniform(-def.spread_deg, def.spread_deg) * 3.0f, run.rng.uniform(-20, 20)};
                bool crit = run.rng.chance(crit_chance);
                float dmg = base_dmg * (crit ? run.stats.crit_mult : 1.0f);
                float flight = std::clamp(origin.distance_to(to) / 900.0f, 0.35f, 0.9f);
                Shell sh{origin,
                         to,
                         0.0f,
                         flight,
                         dmg * static_cast<float>(run.stats.multishot),
                         def.splash + run.stats.explosive_radius,
                         false,
                         true,
                         false,
                         false};
                run.shells.push_back(sh);
                continue;
            }

            int pellets = def.pellets + run.stats.multishot - 1;
            float fan = (def.pellets > 1 ? def.spread_deg : 6.0f) * DEG;
            size_t cap = c.game.touch_ui ? 700u : BULLET_SOFT_CAP;
            float budget = run.bullets.size() > cap ? 0.25f : 1.0f;
            int spawned = std::max(1, static_cast<int>(std::round(static_cast<float>(pellets) * budget)));
            float dmg_scale = static_cast<float>(pellets) / static_cast<float>(spawned);
            float speed_bonus = run.wind_timer > 0.0f ? 1.18f : 1.0f;
            float range = def.speed * speed_bonus * def.life;
            float base_ang = aim_for(origin, range, u == UnitType::Sniper).first;
            bool rifle_ap = (u == UnitType::Rifleman && shot_num % 5 == 0);
            float u_crit = crit_chance + (u == UnitType::Sniper ? 0.25f : 0.0f);
            for (int p = 0; p < spawned; ++p) {
                float t = spawned > 1 ? static_cast<float>(p) / static_cast<float>(spawned - 1) - 0.5f : 0.0f;
                float ang = base_ang + t * fan * static_cast<float>(std::min(spawned, 5)) / 2.0f +
                            run.rng.uniform(-def.spread_deg, def.spread_deg) * DEG * (def.pellets > 1 ? 0.5f : 1.0f);
                Bullet b;
                b.pos = b.prev = origin;
                b.vel = {std::cos(ang) * def.speed * speed_bonus, std::sin(ang) * def.speed * speed_bonus};
                b.damage = base_dmg * dmg_scale * (rifle_ap ? 1.25f : 1.0f);
                b.crit = run.rng.chance(std::min(0.98f, u_crit));
                if (b.crit) b.damage *= run.stats.crit_mult;
                b.pierce = run.stats.pierce + def.bonus_pierce + (rifle_ap ? 1 : 0);
                b.ricochet = def.shot == Shot::Flame ? 0 : run.stats.ricochet;
                b.life = def.life * (run.wind_timer > 0.0f ? 1.15f : 1.0f);
                b.splash = def.shot == Shot::Flame ? 0.0f : def.splash + run.stats.explosive_radius;
                b.knock = def.knock;
                b.unit = u;
                b.flame = def.shot == Shot::Flame;
                b.cryo = def.shot == Shot::Cryo;
                b.volt = def.shot == Shot::Volt;
                b.armor_pierce = rifle_ap || u == UnitType::Sniper;
                b.radius = b.flame ? 16.0f : (u == UnitType::Sniper || b.cryo || b.volt) ? 6.5f : BULLET_RADIUS;
                run.bullets.push_back(b);
            }
            if (def.shot != Shot::Flame && k % 2 == 0) {
                Color mcol = b_muzzle_color(u);
                float msize = u == UnitType::Shotgunner ? 22.0f : u == UnitType::Sniper ? 18.0f : 13.0f;
                additive_particle(run, origin + Vec2{0, -4}, {0, -35}, 0.07f, msize, 4.0f, mcol, PGlow, 0.0f);
                // Crisp directional muzzle sparks + subtle gunsmoke for punchy weapon feel!
                if (u == UnitType::Shotgunner || u == UnitType::Sniper || run.rng.chance(0.45f)) {
                    float sa = base_ang + run.rng.uniform(-0.35f, 0.35f);
                    float ss = run.rng.uniform(220.0f, 390.0f);
                    spark_streak(run, origin + Vec2{0, -4}, {std::cos(sa) * ss, std::sin(sa) * ss}, 0.11f, 2.4f, mcol,
                                 5.0f);
                }
                if ((u == UnitType::Shotgunner || u == UnitType::Sniper) && run.rng.chance(0.55f)) {
                    particle(run, origin + Vec2{0, -6}, {run.rng.uniform(-25, 25), -55.0f}, 0.28f, 5.0f, 15.0f,
                             Color{215, 215, 220, 95}, PSmoke, 2.5f);
                }
                // Brass shell casing ejection for bullet weapons.
                if (def.shot == Shot::Bullet && run.rng.chance(0.45f)) {
                    gib(run, origin + Vec2{6, 8}, Color{235, 195, 75, 255}, 2.2f, 95.0f, 150.0f, 0.55f);
                }
            }
            if (!played && run.shot_sound_cd <= 0.0f) {
                played = true;
                bool heavy = u == UnitType::Sniper || u == UnitType::Shotgunner;
                Sfx sfx = def.shot == Shot::Flame  ? Sfx::Flame
                          : def.shot == Shot::Volt ? Sfx::Zap
                          : def.shot == Shot::Cryo ? Sfx::IceHit
                          : heavy                  ? Sfx::ShotHeavy
                                                   : Sfx::Shot;
                c.game.sound.play(sfx, heavy ? 0.35f : 0.18f, run.rng.uniform(0.9f, 1.15f));
                run.shot_sound_cd = heavy ? 0.09f : 0.055f;
            }
        }
    }
}

Color b_muzzle_color(UnitType u) {
    switch (u) {
    case UnitType::Cryo:
        return {150, 235, 255, 255};
    case UnitType::Volt:
        return {195, 145, 255, 255};
    case UnitType::Sniper:
        return {255, 250, 205, 255};
    default:
        return {255, 225, 140, 255};
    }
}

int nearest_enemy(const Run& run, Vec2 p, float max_dist, int exclude) {
    int best = -1;
    float bd = max_dist * max_dist;
    g_grid.query(p, max_dist, [&](int i) {
        if (i == exclude) return;
        const auto& e = run.enemies[static_cast<size_t>(i)];
        if (e.dead || e.burrowed) return;
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
        if (std::ranges::find(run.squad, f.unit) == run.squad.end()) {
            run.squad.push_back(f.unit);
            run.synergies = compute_synergies(run.squad);
        }
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
        std::vector<TurretType> pool;
        for (int k = 1; k <= TURRET_KINDS; ++k) {
            auto tt = static_cast<TurretType>(k);
            if (turret_unlocked(tt, run.unlocked_stage)) pool.push_back(tt);
        }
        f.turret = pool.empty() ? TurretType::Cannon
                                : pool[static_cast<size_t>(run.rng.range(0, static_cast<int>(pool.size()) - 1))];
    } else if (roll < 80) {
        f.kind = LootKind::Card;
    } else if (roll < 95) {
        f.kind = LootKind::Coins;
        f.amount = 40 + 20 * d.wave + 30 * (d.stage - 1);
    } else {
        f.kind = LootKind::Gems;
        f.amount = 2 + d.stage;
    }
    f.hp = f.max_hp = std::max(60.0f, squad_power(run) * 4.0f);
    run.loot.push_back(f);
}

void update_loot(Ctx& c, float dt) {
    auto& run = c.run;
    auto d = difficulty_for(run);
    if (run.phase == WavePhase::Fighting && (d.wave >= 2 || run.endless)) {
        run.loot_timer -= dt * (run.theme.hazard == StageHazard::Bountiful ? 1.25f : 1.0f);
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
        if (b.life <= 0.0f || b.pos.y < HUD_TOP - 40 || b.pos.x < -20 || b.pos.x > VIRTUAL_W + 20 ||
            (b.vel.y > 0.0f && b.pos.y > WALL_BOTTOM)) {
            b.alive = false;
            continue;
        }
        // Check Arcane Leyline supercharge and Boss Vortex gravity wells.
        if (!run.zones.empty()) {
            for (const auto& z : run.zones) {
                if (!z.active) continue;
                if (z.kind == ZoneKind::Leyline && !b.supercharged) {
                    if ((b.pos - z.pos).length_sq() < z.radius * z.radius) {
                        b.supercharged = true;
                        b.damage *= 1.35f;
                        additive_particle(run, b.pos, {}, 0.14f, 6.0f, 18.0f, Color{185, 130, 255, 220}, PGlow, 0.0f);
                    }
                } else if (z.kind == ZoneKind::Vortex && !b.flame && !b.shard && !b.armor_pierce) {
                    float vr = z.radius * 0.72f;
                    if ((b.pos - z.pos).length_sq() < vr * vr) {
                        b.alive = false;
                        additive_particle(run, b.pos, {}, 0.16f, 5.0f, 14.0f, Color{170, 90, 255, 210}, PGlow, 0.0f);
                        break;
                    }
                }
            }
            if (!b.alive) continue;
        }

        if (!b.shard && !b.phantom) {
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
                    int prev_val = g.state.value;
                    if (gate_hit(g.state, hpp, run.stats.gate_growth, run.soldiers)) {
                        g.pulse = 1.0f;
                        bool flipped = !g.state.red && prev_val <= 0 && g.state.value > 0;
                        c.game.sound.play(flipped ? Sfx::GateGood : Sfx::GateTick, flipped ? 0.5f : 0.35f,
                                          g.state.value > 0 ? 1.25f : 0.9f);
                        if (flipped) {
                            burst(run, {g.x, g.y}, 10, Color{120, 255, 180, 255}, 180.0f, 4.5f, PStar, 0.45f);
                        }
                    }
                    break;
                }
            }
            if (!b.alive) continue;
        }
        int target = -1;
        g_grid.find(b.pos, b.radius, [&](int i) {
            if (i == b.last_hit) return false;
            const auto& e = run.enemies[static_cast<size_t>(i)];
            if (e.dead || e.burrowed) return false;
            float rr = e.radius + b.radius;
            if ((e.pos - b.pos).length_sq() >= rr * rr) return false;
            target = i;
            return true;
        });
        if (target == -1) continue;
        Vec2 hit_pos = b.pos;
        auto& te = run.enemies[static_cast<size_t>(target)];
        if (te.shield_hp > 0.0f && b.vel.y < 0.0f && !b.shard && !b.flame) {
            float sh_mult = b.volt ? 2.0f : (b.unit == UnitType::Shotgunner ? 1.6f : 1.0f);
            te.shield_hp -= b.damage * sh_mult;
            te.shield_flash = 0.08f;
            b.alive = false;
            additive_particle(run, hit_pos, {run.rng.uniform(-80, 80), 60}, 0.14f, 8.0f, 18.0f,
                              b.volt ? Color{195, 145, 255, 255} : Color{180, 225, 255, 255}, PGlow, 3.0f);
            for (int sp = 0; sp < 3; ++sp) {
                float sa = PI * 0.5f + run.rng.uniform(-1.05f, 1.05f);
                float ss = run.rng.uniform(180.0f, 340.0f);
                spark_streak(run, hit_pos, {std::cos(sa) * ss, std::sin(sa) * ss}, 0.16f, 2.4f,
                             b.volt ? Color{205, 155, 255, 255} : Color{165, 225, 255, 255}, 4.0f);
            }
            if (te.shield_hp <= 0.0f) {
                burst(run, te.pos + Vec2{0, te.radius}, 14, Color{170, 180, 190, 255}, 260.0f, 6.0f);
                c.game.sound.play(Sfx::WallHit, 0.5f, 1.6f);
                float_text(run, "SHIELD BROKEN", te.pos + Vec2{0, -te.radius - 10}, Color{200, 220, 255, 255}, 20.0f);
            } else {
                c.game.sound.play(Sfx::Hit, 0.15f, 1.8f);
            }
            continue;
        }

        // Unit passive damage bonuses & impact effects.
        float final_dmg = b.damage;
        if (b.unit == UnitType::Shotgunner && te.pos.y > WALL_TOP - 280.0f) final_dmg *= 1.6f;
        Color num_col = b.cryo           ? Color{150, 235, 255, 255}
                        : b.volt         ? Color{200, 155, 255, 255}
                        : b.supercharged ? Color{215, 175, 255, 255}
                                         : WHITE;
        damage_enemy(c, target, final_dmg, b.crit, true, b.knock, !b.flame, b.armor_pierce, num_col);

        if (!te.dead) {
            if (b.unit == UnitType::Gunner) {
                te.slow_timer = std::max(te.slow_timer, 0.7f);
                te.slow_factor = std::min(te.slow_factor, 0.85f);
            }
            if (b.flame) {
                te.burn_timer = 2.4f;
                te.burn_dps = std::max(te.burn_dps, final_dmg * 4.2f);
            }
            if (b.cryo) {
                apply_frost(run, te, 0.35f, 1.0f);
                burst(run, hit_pos, 3, Color{160, 235, 255, 255}, 110.0f, 3.0f, PStar, 0.25f);
            }
        }
        if (b.volt) {
            int arcs = b.crit ? 3 : 2;
            chain_lightning(c, target, arcs + 1, final_dmg * 0.45f, hit_pos, Color{180, 130, 255, 255}, false);
        } else if (!b.flame) {
            Color pop_col = b.cryo           ? Color{150, 235, 255, 240}
                            : b.supercharged ? Color{210, 155, 255, 240}
                            : b.crit         ? Color{255, 190, 70, 255}
                                             : Color{255, 235, 175, 230};
            additive_particle(run, hit_pos, {0, 0}, 0.09f, b.crit ? 14.0f : 9.0f, b.crit ? 26.0f : 16.0f, pop_col,
                              PGlow, 0.0f);
            // Directional impact spark streaks (extra dramatic on crits & Sniper rail hits)!
            int sparks = b.crit ? 4 : (b.unit == UnitType::Sniper ? 3 : 1);
            if (b.crit || b.unit == UnitType::Sniper || run.rng.chance(0.55f)) {
                float base_a = std::atan2(-b.vel.y, -b.vel.x);
                for (int sp = 0; sp < sparks; ++sp) {
                    float sa = base_a + run.rng.uniform(-1.2f, 1.2f);
                    float ss = run.rng.uniform(160.0f, b.crit ? 380.0f : 270.0f);
                    spark_streak(run, hit_pos, {std::cos(sa) * ss, std::sin(sa) * ss}, b.crit ? 0.20f : 0.13f,
                                 b.crit ? 3.0f : 2.1f, pop_col, 4.2f);
                }
            }
            if (b.crit && run.rng.chance(0.5f)) {
                shockwave(run, hit_pos, 6.0f, 34.0f, 0.16f, Color{255, 215, 95, 195});
            }
        }

        if (b.splash > 0.0f) {
            explode(c, hit_pos, b.splash, final_dmg * 0.6f, true, Color{255, 160, 60, 255});
        }
        // Phantom Split Evolution: non-phantom bullet splits on first impact into 2 seeking phantom rounds!
        if (run.stats.phantom_barrage && !b.phantom && !b.shard && !b.flame && b.last_hit == -1) {
            for (int k = 0; k < 2; ++k) {
                float a = -PI * 0.5f + (k == 0 ? -0.45f : 0.45f);
                int near = nearest_enemy(run, hit_pos, 260.0f, target);
                Vec2 dir = near >= 0 ? (run.enemies[static_cast<size_t>(near)].pos - hit_pos).normalized()
                                     : Vec2{std::cos(a), std::sin(a)};
                if (k == 1 && near >= 0) {
                    dir = Vec2{dir.x * std::cos(0.25f) - dir.y * std::sin(0.25f),
                               dir.x * std::sin(0.25f) + dir.y * std::cos(0.25f)};
                }
                Bullet ph;
                ph.pos = ph.prev = hit_pos;
                ph.vel = dir * 980.0f;
                ph.damage = final_dmg * 0.65f;
                ph.life = 0.45f;
                ph.last_hit = target;
                ph.phantom = true;
                ph.supercharged = true;
                ph.radius = 5.0f;
                run.new_bullets.push_back(ph);
            }
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
    for (size_t i = 0; i < run.bullets.size();) {
        if (!run.bullets[i].alive) {
            run.bullets[i] = run.bullets.back();
            run.bullets.pop_back();
        } else {
            ++i;
        }
    }
    run.bullets.insert(run.bullets.end(), run.new_bullets.begin(), run.new_bullets.end());
    run.new_bullets.clear();
}

void damage_wall(Ctx& c, float amount, Vec2 at) {
    auto& run = c.run;
    amount *= 1.0f - formation_wall_resist(run.formation);
    if (run.wall_shield > 0.0f) {
        float absorbed = std::min(run.wall_shield, amount);
        run.wall_shield -= absorbed;
        amount -= absorbed;
        if (absorbed > 4.0f) {
            additive_particle(run, at, {0, -20}, 0.18f, 16.0f, 34.0f, Color{100, 210, 255, 190}, PGlow, 1.0f);
        }
        if (amount <= 0.0f) return;
    }
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

    // Main stage bosses transform into Phase 2 at 50% HP with a dramatic roar and bonus coin shower!
    if (!e.mini_boss && !e.phase2 && e.hp < e.max_hp * 0.5f) {
        e.phase2 = true;
        c.game.sound.play(Sfx::BossRoar, 1.0f, 0.85f);
        add_shake(run, 18.0f);
        run.zoom_kick = 0.7f;
        shockwave(run, boss_pos, boss_r * 0.5f, boss_r * 4.5f, 0.55f, with_alpha(g.accent, 0.9f));
        run.banner = {"PHASE 2!", std::format("{} UNLEASHES FULL POWER", g.name), g.accent, 2.0f, 2.0f};
        spawn_coins(run, boss_pos, 35 + 15 * difficulty_for(run).stage);
    }

    // Resolve active windup telegraph.
    if (e.telegraph_t > 0.0f) {
        e.telegraph_t -= dt;
        if (e.telegraph_t <= 0.0f) {
            int kind = e.telegraph_kind;
            e.telegraph_kind = 0;
            if (kind == 1 && !e.at_wall) {
                e.dash = 0.75f;
                c.game.sound.play(Sfx::BossRoar, 0.65f, 1.5f);
                float_text(run, "CHARGE!", boss_pos + Vec2{0, -boss_r - 20}, Color{255, 120, 80, 255}, 30.0f);
            } else if (kind == 2) {
                // Seismic Shockwave Slam!
                c.game.sound.play(Sfx::BigExplosion, 0.75f, 0.85f);
                add_shake(run, 14.0f);
                shockwave(run, boss_pos, boss_r * 0.6f, boss_r * 3.0f, 0.45f, Color{255, 195, 70, 220});
                run.boss_waves.push_back(
                    {boss_pos + Vec2{0, boss_r * 0.6f}, 170.0f, 340.0f, 45.0f * wall_damage_mult(run), g.accent, true});
            } else if (kind == 3) {
                // Sweeping Eye Beam!
                c.game.sound.play(Sfx::Laser, 0.8f, 0.65f);
                float start_ang = PI * 0.5f + (run.rng.chance(0.5f) ? -0.45f : 0.45f);
                float sweep_dir = start_ang < PI * 0.5f ? 1.0f : -1.0f;
                run.boss_beams.push_back(
                    {boss_pos, start_ang, sweep_dir, 0.0f, 1.35f, 34.0f * wall_damage_mult(run), g.accent, true});
            }
        }
    }

    float phase_cd = e.phase2 ? 0.78f : 1.0f;
    e.action_timer -= dt;
    if (e.action_timer <= 0.0f) {
        e.action_timer = (e.mini_boss ? 7.0f : 5.5f) * phase_cd;
        if (g.has(BossPower::Summoner) || g.has(BossPower::Necromancer) || !e.mini_boss) {
            c.game.sound.play(Sfx::BossRoar, 0.7f, e.mini_boss ? 1.2f : 1.0f);
            add_shake(run, 10.0f);
            shockwave(run, boss_pos, boss_r, boss_r * 4.0f, 0.5f, Color{255, 80, 60, 200});
            int n = (e.mini_boss ? 3 : 5) + (g.has(BossPower::Summoner) ? 4 : 0);
            for (int k = 0; k < n; ++k) {
                EnemyType st = g.has(BossPower::Necromancer) && k == 0 ? EnemyType::Healer
                               : k % 3 == 0                            ? EnemyType::Runner
                                                                       : EnemyType::Walker;
                SpawnEvent ev{0, st, std::clamp(boss_pos.x + run.rng.uniform(-160, 160), 30.0f, VIRTUAL_W - 30.0f)};
                Enemy& spawned = spawn_enemy(run, ev);
                spawned.pos.y = boss_pos.y + run.rng.uniform(-30, 30);
                if (g.has(BossPower::Necromancer)) spawned.armor = std::max(spawned.armor, 0.25f);
            }
        }
    }

    e.power_timer -= dt;
    if (e.power_timer <= 0.0f && e.telegraph_t <= 0.0f) {
        e.power_timer = 4.6f * phase_cd;
        if (g.has(BossPower::Charger) && !e.at_wall && run.rng.chance(0.65f)) {
            e.telegraph_t = 0.85f;
            e.telegraph_kind = 1;
            e.telegraph_x = boss_pos.x;
            float_text(run, "INCOMING!", boss_pos + Vec2{0, -boss_r - 20}, Color{255, 110, 70, 255}, 26.0f);
        } else if (g.has(BossPower::Shockwave) && run.rng.chance(0.65f)) {
            e.telegraph_t = 0.8f;
            e.telegraph_kind = 2;
            e.telegraph_x = boss_pos.x;
            float_text(run, "SEISMIC SLAM!", boss_pos + Vec2{0, -boss_r - 20}, Color{255, 205, 80, 255}, 26.0f);
        } else if (g.has(BossPower::LaserSweep) && run.rng.chance(0.65f)) {
            e.telegraph_t = 0.85f;
            e.telegraph_kind = 3;
            e.telegraph_x = boss_pos.x;
            float_text(run, "EYE BEAM!", boss_pos + Vec2{0, -boss_r - 20}, Color{255, 90, 175, 255}, 26.0f);
        }
        if (g.has(BossPower::Barrage)) {
            int count = e.phase2 ? 7 : 5;
            for (int k = 0; k < count; ++k) {
                Vec2 target{60.0f + static_cast<float>(k) * (VIRTUAL_W - 120.0f) / static_cast<float>(count - 1) +
                                run.rng.uniform(-25, 25),
                            WALL_TOP + 20};
                Vec2 v = (target - boss_pos).normalized() * 420.0f;
                run.acids.push_back({boss_pos, v, 15.0f * wall_damage_mult(run)});
            }
            c.game.sound.play(Sfx::Cannon, 0.5f, 0.7f);
        }
        if (g.has(BossPower::Vortex)) {
            Vec2 vp{std::clamp(run.squad_x + run.rng.uniform(-80, 80), 110.0f, VIRTUAL_W - 110.0f),
                    std::min(WALL_TOP - 170.0f, boss_pos.y + 150.0f)};
            run.zones.push_back({ZoneKind::Vortex, vp, 64.0f, 3.2f, 3.2f, 0.0f, run.rng.uniform(0, 100), true});
            float_text(run, "VOID VORTEX!", vp + Vec2{0, -40}, Color{190, 110, 255, 255}, 24.0f);
        }
        if (g.has(BossPower::Shielded) && e.shield_hp <= 0.0f) {
            e.shield_hp = e.shield_max = e.max_hp * 0.18f;
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

void update_boss_hazards(Ctx& c, float dt) {
    auto& run = c.run;
    for (auto& w : run.boss_waves) {
        if (!w.alive) continue;
        w.pos.y += w.speed * dt;
        if (run.rng.chance(dt * 28.0f)) {
            particle(run, w.pos + Vec2{run.rng.uniform(-w.width * 0.5f, w.width * 0.5f), 0}, {0, -40}, 0.35f, 8.0f,
                     2.0f, w.color, PGlow, 2.0f);
        }
        if (w.pos.y >= WALL_TOP) {
            w.alive = false;
            damage_wall(c, w.damage, {w.pos.x, WALL_TOP});
            add_shake(run, 10.0f);
            shockwave(run, {w.pos.x, WALL_TOP}, 20.0f, 120.0f, 0.3f, w.color);
            c.game.sound.play(Sfx::WallHit, 0.7f, 0.7f);
        }
    }
    std::erase_if(run.boss_waves, [](const BossWave& w) { return !w.alive; });

    for (auto& b : run.boss_beams) {
        if (!b.alive) continue;
        b.t += dt;
        b.angle += b.sweep_dir * 0.65f * dt;
        float dy = WALL_TOP - b.origin.y;
        if (dy > 10.0f && std::sin(b.angle) > 0.1f) {
            float hit_x = b.origin.x + dy / std::tan(b.angle);
            if (hit_x >= 0.0f && hit_x <= VIRTUAL_W) {
                damage_wall(c, b.dps * dt, {hit_x, WALL_TOP});
                if (run.rng.chance(dt * 24.0f)) {
                    additive_particle(run, {hit_x, WALL_TOP}, {run.rng.uniform(-70, 70), run.rng.uniform(-90, -20)},
                                      0.25f, 7.0f, 1.5f, b.color, PGlow, 2.0f);
                }
            }
        }
        if (b.t >= b.duration) b.alive = false;
    }
    std::erase_if(run.boss_beams, [](const BossBeam& b) { return !b.alive; });
}

void update_enemies(Ctx& c, float dt) {
    auto& run = c.run;
    float wall_sound = 0.0f;

    // First clear per-frame warlord buff and apply active Warlord aura rings.
    for (auto& e : run.enemies) e.warlord_buff = std::max(0.0f, e.warlord_buff - dt * 2.5f);
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        const auto& w = run.enemies[static_cast<size_t>(i)];
        if (w.dead || w.type != EnemyType::Warlord || w.freeze_timer > 0.0f) continue;
        g_grid.query(w.pos, 195.0f, [&](int j) {
            auto& o = run.enemies[static_cast<size_t>(j)];
            if (!o.dead && j != i && o.type != EnemyType::Barrel && o.pos.distance_to(w.pos) < 195.0f) {
                o.warlord_buff = 0.5f;
            }
        });
    }

    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        auto& e = run.enemies[static_cast<size_t>(i)];
        if (e.dead) continue;
        e.anim += dt;
        e.flash = std::max(0.0f, e.flash - dt);
        e.shield_flash = std::max(0.0f, e.shield_flash - dt);
        if (e.slow_timer > 0.0f) {
            e.slow_timer -= dt;
            if (e.slow_timer <= 0.0f) e.slow_factor = 1.0f;
        }
        if (e.corrode_timer > 0.0f) e.corrode_timer -= dt;
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
        } else if (run.mod == WaveMod::Regenerating && e.type != EnemyType::Barrel && e.hp < e.max_hp) {
            e.hp = std::min(e.max_hp, e.hp + e.max_hp * 0.025f * dt);
        }
        if (e.freeze_timer > 0.0f) {
            e.freeze_timer -= dt;
            e.knock *= std::exp(-10.0f * dt);
            continue;
        }
        if (e.charm > 0.0f) {
            e.charm -= dt;
            e.knock *= std::exp(-10.0f * dt);
            continue;
        }

        // Burrower periodically dives underground to dodge gunfire while advancing quickly.
        if (e.type == EnemyType::Burrower && !e.at_wall && e.pos.y > HUD_TOP + 60.0f && e.pos.y < WALL_TOP - 150.0f) {
            e.burrow_timer -= dt;
            if (e.burrow_timer <= 0.0f) {
                e.burrowed = !e.burrowed;
                e.burrow_timer = e.burrowed ? 1.45f : 3.2f;
                burst(run, e.pos, 8, Color{140, 110, 74, 255}, 150.0f, 5.0f, PDisc, 0.45f);
            }
            if (e.burrowed && run.rng.chance(dt * 18.0f)) {
                particle(run, e.pos, {run.rng.uniform(-30, 30), -20}, 0.35f, 6.0f, 12.0f, Color{110, 88, 60, 160},
                         PSmoke, 2.0f);
            }
        } else if (e.burrowed) {
            e.burrowed = false;
        }

        // Berserker grows faster and deadlier as its health drops.
        float berserk_k = e.type == EnemyType::Berserker ? std::clamp(1.0f - e.hp / e.max_hp, 0.0f, 1.0f) : 0.0f;
        float speed = e.speed * (e.slow_timer > 0.0f ? e.slow_factor : 1.0f) * (e.dash > 0.0f ? 6.0f : 1.0f) *
                      (e.enraged ? 1.5f : 1.0f) * (e.warlord_buff > 0.0f ? 1.25f : 1.0f) * (e.burrowed ? 1.65f : 1.0f) *
                      (1.0f + berserk_k * 0.95f);
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
        } else if (e.type == EnemyType::Healer && e.pos.y >= e.stop_y) {
            e.action_timer -= dt;
            if (e.action_timer <= 0.0f) {
                e.action_timer = 2.8f;
                int healed = 0;
                g_grid.query(e.pos, 190.0f, [&](int j) {
                    auto& o = run.enemies[static_cast<size_t>(j)];
                    if (o.dead || j == i || o.type == EnemyType::Barrel || o.hp >= o.max_hp) return;
                    if (o.pos.distance_to(e.pos) < 190.0f && healed < 5) {
                        o.hp = std::min(o.max_hp, o.hp + o.max_hp * 0.14f);
                        healed++;
                        Bolt hb;
                        hb.points = {e.pos, o.pos};
                        hb.color = Color{90, 255, 155, 255};
                        hb.width = 4.0f;
                        hb.life = hb.max_life = 0.28f;
                        run.bolts.push_back(std::move(hb));
                    }
                });
                if (healed > 0) shockwave(run, e.pos, e.radius * 0.5f, 180.0f, 0.35f, Color{90, 255, 150, 140});
            }
        } else if (!e.at_wall) {
            e.pos.y += speed * dt;
            float sway = e.type == EnemyType::Flyer ? 90.0f : 14.0f;
            float freq = e.type == EnemyType::Flyer ? 3.1f : 1.7f;
            e.pos.x += std::sin(run.time * freq + e.wobble_seed) * sway * dt;
            // Sandstorm wind pushes lightweight enemies laterally.
            if (run.wind_force != 0.0f && e.knock_resist < 0.5f && e.type != EnemyType::Barrel) {
                e.pos.x += run.wind_force * (1.0f - e.knock_resist) * dt;
            }
            if (e.type == EnemyType::Boss && e.pos.y > HUD_TOP + 20) {
                float step = std::sin(e.anim * 3.0f);
                if (step > 0.97f) add_shake(run, e.mini_boss ? 2.0f : 4.0f);
            }
            if (e.pos.y >= stop_line) {
                e.pos.y = stop_line;
                e.at_wall = true;
                e.burrowed = false;
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
            float dps = e.wall_dps * (e.enraged ? 1.5f : 1.0f) * (1.0f + berserk_k * 0.6f) *
                        (e.slow_timer > 0.0f ? e.slow_factor : 1.0f);
            damage_wall(c, dps * dt, {e.pos.x, WALL_TOP + 4});
            wall_sound += dps * dt;
            // Fortified wall spikes reflect thorns damage back at melee attackers!
            int wt = wall_tier(run);
            if (wt > 0 && e.type != EnemyType::Barrel) {
                float thorns = std::max(6.0f, run.stats.damage * (0.95f * static_cast<float>(wt))) * dt;
                e.hp -= thorns;
                run.tally.damage_dealt += thorns;
                if (run.rng.chance(dt * 4.0f)) {
                    particle(run, {e.pos.x, WALL_TOP - 8}, {run.rng.uniform(-45, 45), -60}, 0.22f, 4.0f, 1.0f,
                             wt >= 2 ? Color{140, 230, 255, 220} : Color{255, 210, 120, 220}, PGlow, 2.0f);
                }
                if (e.hp <= 0.0f) {
                    kill_enemy(c, i);
                    continue;
                }
            }
        }

        if (e.type == EnemyType::Boss) update_boss(c, i, dt);
    }
    if (wall_sound > 0.0f && run.rng.chance(std::min(0.25f, wall_sound * 0.04f))) {
        c.game.sound.play(Sfx::WallHit, 0.35f, run.rng.uniform(0.8f, 1.2f));
        add_shake(run, 2.5f);
    }

    g_grid.pairs(run.enemies, [&](int i, int j) {
        auto& a = run.enemies[static_cast<size_t>(i)];
        auto& b = run.enemies[static_cast<size_t>(j)];
        if (a.dead || b.dead || a.burrowed || b.burrowed) return;
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
    for (auto& e : run.enemies) {
        if (!e.dead) e.pos.x = std::clamp(e.pos.x, e.radius * 0.6f, VIRTUAL_W - e.radius * 0.6f);
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
        k.pos = Vec2{run.squad_x, SQUAD_Y} + run.soldier_off_cur[static_cast<size_t>(si)];
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
    if (gate_is_stat(g.state.kind)) {
        float k = 1.0f + static_cast<float>(g.state.value) / 100.0f;
        const char* stat_name = "DAMAGE";
        if (g.state.kind == GateKind::Rate) {
            run.stats.fire_rate *= k;
            stat_name = "FIRE RATE";
        } else if (g.state.kind == GateKind::Power) {
            run.stats.damage *= k;
            stat_name = "DAMAGE";
        } else if (g.state.kind == GateKind::Crit) {
            run.stats.crit_chance =
                std::clamp(run.stats.crit_chance + static_cast<float>(g.state.value) / 100.0f, 0.02f, 0.95f);
            stat_name = "CRIT";
        } else {
            run.stats.turret_damage_mult = std::max(0.5f, run.stats.turret_damage_mult * k);
            stat_name = "TURRETS";
        }
        run.tally.gates_taken++;
        Vec2 at{run.squad_x, SQUAD_Y - 40};
        bool up = g.state.value >= 0;
        float_text(run, std::format("{}{}% {}", up ? "+" : "", g.state.value, stat_name), at,
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
    run.ali_charge = std::min(1.0f, run.ali_charge + 0.12f * run.stats.ability_charge_mult);
    run.tally.max_soldiers = std::max(run.tally.max_soldiers, run.soldiers);
    run.soldier_pop = 1.0f;
    run.squad_x = clamp_squad_x(run.squad_x, visible_soldiers(run.soldiers), run.formation);
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
        if (chosen && run.phase != WavePhase::Lost && run.collapse_t < 0.0f && run.soldiers > 0) {
            apply_gate_to_squad(c, *chosen);
        }
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
    float r2 = range * range;
    for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
        const auto& e = run.enemies[static_cast<size_t>(i)];
        if (e.dead || e.burrowed || e.pos.y < HUD_TOP) continue;
        if ((e.pos - from).length_sq() > r2) continue;
        float score = e.pos.y + (e.type == EnemyType::Boss ? 220.0f : 0.0f) + (e.elite ? 90.0f : 0.0f) +
                      (e.type == EnemyType::Healer || e.type == EnemyType::Warlord ? 70.0f : 0.0f);
        if (score > best_y) {
            best_y = score;
            best = i;
        }
    }
    return best;
}

void chain_lightning(Ctx& c, int first, int count, float dmg, Vec2 origin, Color color, bool can_crit) {
    auto& run = c.run;
    int visited[32];
    int vis_n = 0;
    int max_hits = std::min(count, 32);
    Bolt bolt;
    bolt.color = color;
    bolt.points.reserve(static_cast<size_t>(max_hits) * 4 + 1);
    bolt.points.push_back(origin);
    int cur = first;
    while (cur != -1 && vis_n < max_hits) {
        visited[vis_n++] = cur;
        Vec2 p = run.enemies[static_cast<size_t>(cur)].pos;
        Vec2 last = bolt.points.back();
        for (int s = 1; s <= 3; ++s) {
            float t = static_cast<float>(s) / 4.0f;
            Vec2 mid = last + (p - last) * t;
            bolt.points.push_back(mid + Vec2{run.rng.uniform(-14, 14), run.rng.uniform(-14, 14)});
        }
        bolt.points.push_back(p);
        bool crit = can_crit && run.rng.chance(run.stats.crit_chance);
        damage_enemy(c, cur, crit ? dmg * run.stats.crit_mult : dmg, crit, false, 10.0f, true, false,
                     Color{185, 225, 255, 255});
        particle(run, p, {}, 0.15f, 10.0f, 34.0f, color, PGlow, 0.0f);
        int next = -1;
        float bd = 175.0f * 175.0f;
        g_grid.query(p, 175.0f, [&](int i) {
            const auto& e = run.enemies[static_cast<size_t>(i)];
            if (e.dead || e.burrowed) return;
            for (int v = 0; v < vis_n; ++v) {
                if (visited[v] == i) return;
            }
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
        t.cooldown -= dt * run.stats.turret_rate_mult;
        t.recoil = std::max(0.0f, t.recoil - dt * 5.0f);
        if (t.target >= 0 &&
            (t.target >= static_cast<int>(run.enemies.size()) || run.enemies[static_cast<size_t>(t.target)].dead ||
             run.enemies[static_cast<size_t>(t.target)].burrowed)) {
            t.target = -1;
        }
        if (t.target == -1 || t.type == TurretType::Laser || t.type == TurretType::Cannon ||
            t.type == TurretType::Frost || t.type == TurretType::Missile) {
            t.target = pick_front_enemy(run, pos, def.range);
        }
        if (t.target == -1) continue;
        const auto& target = run.enemies[static_cast<size_t>(t.target)];
        Vec2 dir = target.pos - pos;
        float want = std::atan2(dir.y, dir.x);
        float diff = std::remainder(want - t.aim, 2 * PI);
        t.aim += diff * std::min(1.0f, dt * 12.0f);
        if (t.cooldown > 0.0f) continue;
        float dmg = turret_damage(t.type, t.level, d.stage) * run.stats.turret_damage_mult;
        t.cooldown = turret_interval(t.type, t.level);
        Vec2 muzzle = pos + Vec2{std::cos(t.aim), std::sin(t.aim)} * TURRET_BARREL;
        switch (t.type) {
        case TurretType::Cannon: {
            float flight = 0.55f;
            Vec2 lead = target.at_wall ? target.pos : target.pos + Vec2{0, target.speed * flight};
            float rad = 64.0f + 8.0f * static_cast<float>(t.level) + (t.level >= 3 ? 22.0f : 0.0f) +
                        (run.stats.aegis_wall ? 18.0f : 0.0f);
            bool shred = t.level >= 3;
            int shells = 1 + (t.level >= 5 ? 1 : 0) + (run.stats.aegis_wall ? 1 : 0);
            for (int k = 0; k < shells; ++k) {
                Vec2 aim_to = k == 0 ? lead : lead + Vec2{run.rng.uniform(-46, 46), run.rng.uniform(-34, 34)};
                float d_scale = k == 0 ? 1.0f : 0.72f;
                run.shells.push_back({muzzle, aim_to, 0.0f, flight + 0.07f * static_cast<float>(k), dmg * d_scale, rad,
                                      false, false, false, false, shred, false});
            }
            t.recoil = 1.0f;
            c.game.sound.play(Sfx::Cannon, 0.45f, run.rng.uniform(0.9f, 1.1f));
            particle(run, muzzle, Vec2{std::cos(t.aim), std::sin(t.aim)} * 60.0f, 0.5f, 10.0f, 30.0f,
                     Color{200, 200, 200, 160}, PSmoke, 3.0f);
            break;
        }
        case TurretType::Tesla: {
            int chains = tesla_chains(t.level) + (t.level >= 3 ? 2 : 0) + (run.stats.aegis_wall ? 2 : 0);
            Vec2 target_p = target.pos;
            chain_lightning(c, t.target, chains, dmg, pos + TESLA_ORB, Color{140, 210, 255, 255}, t.level >= 5);
            if (t.level >= 3 && !run.enemies[static_cast<size_t>(t.target)].dead) {
                auto& te = run.enemies[static_cast<size_t>(t.target)];
                te.slow_timer = std::max(te.slow_timer, 1.1f);
                te.slow_factor = std::min(te.slow_factor, 0.72f);
            }
            if (t.level >= 5) {
                // Lv5 Apex: Electric Storm Nova around the strike zone!
                shockwave(run, target_p, 18.0f, 115.0f, 0.28f, Color{150, 225, 255, 215});
                explode(c, target_p, 105.0f, dmg * 0.85f, false, Color{130, 215, 255, 255}, false, false, false);
            }
            c.game.sound.play(Sfx::Zap, 0.4f, run.rng.uniform(0.9f, 1.2f));
            break;
        }
        case TurretType::Flamer: {
            Vec2 fdir = Vec2{std::cos(t.aim), std::sin(t.aim)};
            float cone_deg = (t.level >= 3 ? 27.0f : 20.0f) + (run.stats.aegis_wall ? 6.0f : 0.0f);
            float cone = std::cos(cone_deg * DEG);
            float burn_mult = t.level >= 3 ? 5.2f : 4.0f;
            g_grid.query(pos, def.range, [&](int i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead || e.burrowed) return;
                Vec2 to = e.pos - pos;
                float rr = def.range + e.radius;
                float dsq = to.length_sq();
                if (dsq > rr * rr || dsq < 1.0f) return;
                if ((to * (1.0f / std::sqrt(dsq))).dot(fdir) < cone) return;
                e.burn_timer = t.level >= 3 ? 2.8f : 2.0f;
                e.burn_dps = std::max(e.burn_dps, dmg * burn_mult);
                damage_enemy(c, i, dmg, false, false, 2.0f, false);
            });
            for (int k = 0; k < (t.level >= 3 ? 4 : 3); ++k) {
                float a = t.aim + run.rng.uniform(-cone_deg * 0.6f, cone_deg * 0.6f) * DEG;
                float spd = run.rng.uniform(380, 520);
                particle(run, muzzle, {std::cos(a) * spd, std::sin(a) * spd}, def.range / spd, 8.0f, 34.0f,
                         Color{255, static_cast<unsigned char>(run.rng.range(110, 200)), 40, 220}, PFire, 0.5f);
            }
            t.flame_timer -= dt;
            if (t.flame_timer <= 0.0f) {
                t.flame_timer = 0.22f;
                c.game.sound.play(Sfx::Flame, 0.25f, run.rng.uniform(0.8f, 1.2f));
                if (t.level >= 5 && run.rng.chance(0.18f) && run.zones.size() < 10) {
                    Vec2 pool_at = muzzle + fdir * run.rng.uniform(110.0f, 220.0f);
                    pool_at.y = std::clamp(pool_at.y, HUD_TOP + 60.0f, WALL_TOP - 40.0f);
                    run.zones.push_back(
                        {ZoneKind::NapalmPool, pool_at, 64.0f, 3.2f, 3.2f, 0.0f, run.rng.uniform(0, 100), true});
                }
            }
            break;
        }
        case TurretType::Laser: {
            Vec2 ldir = Vec2{std::cos(t.aim), std::sin(t.aim)};
            Vec2 end = muzzle + ldir * 1400.0f;
            float beam_w = (t.level >= 3 ? 20.0f : 14.0f) + (run.stats.aegis_wall ? 8.0f : 0.0f);
            bool hyper = t.level >= 5;
            float beam_dmg = hyper ? dmg * 1.4f : dmg;
            for (int i = 0; i < static_cast<int>(run.enemies.size()); ++i) {
                auto& e = run.enemies[static_cast<size_t>(i)];
                if (e.dead || e.burrowed) continue;
                Vec2 to = e.pos - muzzle;
                float along = to.dot(ldir);
                if (along < 0) continue;
                Vec2 closest = muzzle + ldir * along;
                float rr = e.radius + beam_w;
                if ((closest - e.pos).length_sq() < rr * rr) {
                    if (t.level >= 3) e.armor = std::max(0.0f, e.armor - 0.15f);
                    damage_enemy(c, i, beam_dmg, hyper, false, 30.0f, true, true);
                }
            }
            Bolt beam;
            beam.points = {muzzle, end};
            beam.color = hyper ? Color{255, 110, 195, 255} : def.color;
            beam.width = beam_w;
            beam.life = beam.max_life = 0.32f;
            run.bolts.push_back(beam);
            if (hyper) shockwave(run, muzzle, 10.0f, 70.0f, 0.22f, Color{255, 120, 200, 210});
            t.recoil = 1.0f;
            add_shake(run, hyper ? 6.0f : 4.0f);
            c.game.sound.play(Sfx::Laser, 0.5f);
            break;
        }
        case TurretType::Frost: {
            float flight = 0.5f;
            Vec2 lead = target.at_wall ? target.pos : target.pos + Vec2{0, target.speed * flight * 0.7f};
            float rad = 74.0f + 10.0f * static_cast<float>(t.level) + (t.level >= 3 ? 16.0f : 0.0f) +
                        (run.stats.aegis_wall ? 20.0f : 0.0f);
            int shells = t.level >= 5 ? 2 : 1;
            for (int k = 0; k < shells; ++k) {
                Vec2 aim_to = k == 0 ? lead : lead + Vec2{run.rng.uniform(-48, 48), run.rng.uniform(-32, 32)};
                run.shells.push_back({muzzle, aim_to, 0.0f, flight + 0.08f * static_cast<float>(k),
                                      dmg * (k == 0 ? 1.0f : 0.75f), rad, false, false, true, false, true,
                                      t.level >= 5});
            }
            t.recoil = 1.0f;
            c.game.sound.play(Sfx::IceBreak, 0.4f, 1.3f);
            burst(run, muzzle, 6, Color{140, 235, 255, 255}, 120.0f, 4.0f, PStar, 0.3f);
            break;
        }
        case TurretType::Missile: {
            int rockets = (t.level >= 5 ? 6 : t.level >= 3 ? 4 : 2) + (run.stats.aegis_wall ? 1 : 0);
            float rad = 52.0f + 6.0f * static_cast<float>(t.level);
            float r_dmg = dmg * (t.level >= 5 ? 0.56f : t.level >= 3 ? 0.65f : 0.78f);
            for (int k = 0; k < rockets; ++k) {
                float frac = rockets == 1 ? 0.0f : (static_cast<float>(k) / static_cast<float>(rockets - 1) - 0.5f);
                float a = t.aim + frac * 0.85f;
                Vec2 launch = muzzle + Vec2{-std::sin(t.aim), std::cos(t.aim)} * (frac * 18.0f);
                Shell ms;
                ms.from = launch;
                ms.to = target.pos;
                ms.pos = launch;
                ms.vel = Vec2{std::cos(a), std::sin(a)} * 310.0f;
                ms.duration = 2.4f;
                ms.damage = r_dmg;
                ms.radius = rad;
                ms.shred = t.level >= 3;
                ms.missile = true;
                ms.target = t.target;
                ms.cluster = t.level >= 5;
                run.shells.push_back(ms);
            }
            t.recoil = 1.0f;
            c.game.sound.play(Sfx::Cannon, 0.42f, run.rng.uniform(1.25f, 1.45f));
            particle(run, muzzle, Vec2{std::cos(t.aim), std::sin(t.aim)} * 40.0f, 0.45f, 12.0f, 28.0f,
                     Color{225, 210, 195, 170}, PSmoke, 2.5f);
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
    std::vector<Shell> spawn_extra;
    for (auto& s : run.shells) {
        s.t += dt;
        if (s.missile) {
            if (s.target < 0 || s.target >= static_cast<int>(run.enemies.size()) ||
                run.enemies[static_cast<size_t>(s.target)].dead ||
                run.enemies[static_cast<size_t>(s.target)].burrowed) {
                s.target = nearest_enemy(run, s.pos, 760.0f, -1);
            }
            Vec2 aim_pt = s.to;
            float hit_r = 22.0f;
            if (s.target >= 0) {
                const auto& te = run.enemies[static_cast<size_t>(s.target)];
                aim_pt = te.pos;
                hit_r = te.radius + 18.0f;
            }
            Vec2 to_t = aim_pt - s.pos;
            float dist = to_t.length();
            float speed = std::min(620.0f, 320.0f + s.t * 480.0f);
            if (dist > 1.0f) {
                Vec2 desired = to_t * (1.0f / dist) * speed;
                Vec2 dir = s.vel + desired * std::min(1.0f, dt * 8.0f);
                float dl = dir.length();
                s.vel = dl > 0.001f ? dir * (speed / dl) : desired;
            }
            s.pos += s.vel * dt;
            s.to = s.pos;
            if (run.rng.chance(dt * 42.0f)) {
                particle(run, s.pos - s.vel.normalized() * 10.0f, {run.rng.uniform(-20, 20), run.rng.uniform(-20, 20)},
                         0.32f, 6.0f, 15.0f, Color{210, 205, 200, 145}, PSmoke, 2.0f);
                additive_particle(run, s.pos - s.vel.normalized() * 12.0f, {0, 0}, 0.14f, 6.0f, 1.5f,
                                  Color{255, 165, 65, 220}, PGlow, 1.0f);
            }
            bool hit = (s.target >= 0 && dist <= hit_r) || s.t >= s.duration || s.pos.y < HUD_TOP - 40.0f;
            if (hit) {
                s.t = s.duration;
                explode(c, s.pos, s.radius, s.damage, false, Color{255, 120, 65, 255}, false, s.shred, false);
                if (s.cluster) {
                    for (int k = 0; k < 2; ++k) {
                        Vec2 cp = s.pos + Vec2{run.rng.uniform(-42, 42), run.rng.uniform(-38, 38)};
                        Shell sub;
                        sub.from = s.pos;
                        sub.to = cp;
                        sub.duration = 0.22f + 0.06f * static_cast<float>(k);
                        sub.damage = s.damage * 0.45f;
                        sub.radius = s.radius * 0.72f;
                        sub.grenade = true;
                        sub.shred = true;
                        spawn_extra.push_back(sub);
                    }
                }
            }
            continue;
        }
        if (s.t >= s.duration) {
            if (s.frost) {
                explode(c, s.to, s.radius, s.damage, false, Color{120, 230, 255, 255}, false, true, true);
                if (s.shards) {
                    for (int k = 0; k < 6; ++k) {
                        float a = static_cast<float>(k) / 6.0f * 2.0f * PI + run.rng.uniform(0, 0.3f);
                        Bullet sh;
                        sh.pos = sh.prev = s.to;
                        sh.vel = {std::cos(a) * 760.0f, std::sin(a) * 760.0f};
                        sh.damage = s.damage * 0.55f;
                        sh.life = 0.35f;
                        sh.shard = true;
                        sh.radius = 6.0f;
                        run.new_bullets.push_back(sh);
                    }
                }
            } else {
                explode(c, s.to, s.radius, s.damage, s.grenade,
                        s.napalm ? Color{255, 95, 190, 255} : Color{255, 150, 60, 255}, s.bomb, s.grenade || s.shred,
                        false);
                if (s.napalm) {
                    run.zones.push_back({ZoneKind::NapalmPool, s.to, s.radius * 0.85f, 4.0f, 4.0f, 0.0f,
                                         run.rng.uniform(0, 100), true});
                }
            }
        } else if (run.rng.chance(dt * 30.0f)) {
            float k = s.t / s.duration;
            Vec2 p = s.from + (s.to - s.from) * k;
            p.y -= std::sin(k * PI) * (s.bomb ? 0.0f : 120.0f);
            Color tr = s.frost ? Color{150, 230, 255, 170} : Color{180, 180, 180, 120};
            particle(run, p, {0, 0}, 0.4f, 5.0f, 14.0f, tr, s.frost ? PStar : PSmoke, 1.0f);
        }
    }
    std::erase_if(run.shells, [](const Shell& s) { return s.t >= s.duration; });
    for (auto& sub : spawn_extra) run.shells.push_back(sub);
}

void update_salama_heads(Ctx& c, float dt) {
    auto& run = c.run;
    static const char* BOOMS[] = {"BABA BOOM!", "SALAMA SMASH!", "HAHAHA!", "YALLA BABA!"};
    for (auto& h : run.salama_heads) {
        h.t += dt;
        h.rot += h.spin * dt;
        float k = std::clamp(h.t / h.duration, 0.0f, 1.0f);
        Vec2 p = h.from + (h.to - h.from) * k;
        float arc = std::sin(k * PI) * (h.bounces > 0 ? 155.0f : 95.0f);
        if (run.rng.chance(dt * 34.0f)) {
            Color sp = run.rng.chance(0.5f) ? Color{255, 220, 70, 230} : Color{110, 225, 255, 230};
            additive_particle(run, p + Vec2{0, -arc}, {run.rng.uniform(-45, 45), run.rng.uniform(-35, 35)}, 0.4f, 8.0f,
                              1.5f, sp, PStar, 2.0f);
        }
        if (h.t >= h.duration) {
            if (h.bounces > 0) {
                // First landing: hilarious BOING bounce with mini-shockwave & armor shred!
                h.bounces--;
                explode(c, h.to, h.radius * 0.72f, h.damage * 0.55f, true, Color{255, 200, 60, 255}, false, true,
                        false);
                shockwave(run, h.to, 24.0f, h.radius * 0.9f, 0.3f, Color{255, 225, 90, 220});
                float_text(run, "BOING!", h.to + Vec2{0, -34}, Color{255, 235, 95, 255}, 34.0f, 0.9f);
                c.game.sound.play(Sfx::GateGood, 0.8f, 1.35f);
                h.from = h.to;
                int next = nearest_enemy(run, h.to, 260.0f, -1);
                h.to = next >= 0
                           ? run.enemies[static_cast<size_t>(next)].pos
                           : Vec2{std::clamp(h.to.x + run.rng.uniform(-110, 110), 80.0f, VIRTUAL_W - 80.0f),
                                  std::clamp(h.to.y - run.rng.uniform(70, 150), HUD_TOP + 80.0f, WALL_TOP - 80.0f)};
                h.t = 0.0f;
                h.duration = 0.52f;
                h.spin = -h.spin * 1.25f;
            } else {
                // Final comedic MEGA EXPLOSION!
                explode(c, h.to, h.radius, h.damage, true, Color{255, 195, 50, 255}, true, true, false);
                run.zoom_kick = std::max(run.zoom_kick, 0.65f);
                add_shake(run, 18.0f);
                for (int r = 0; r < 2; ++r) {
                    shockwave(run, h.to, 30.0f, h.radius * (1.35f + 0.45f * static_cast<float>(r)),
                              0.35f + 0.12f * static_cast<float>(r),
                              r == 0 ? Color{255, 220, 70, 235} : Color{110, 220, 255, 210});
                }
                for (int k2 = 0; k2 < 18; ++k2) {
                    float a = run.rng.uniform(0, 2.0f * PI);
                    float sp = run.rng.uniform(160, 440);
                    Color col = k2 % 3 == 0   ? Color{255, 220, 65, 255}
                                : k2 % 3 == 1 ? Color{110, 225, 255, 255}
                                              : Color{255, 120, 170, 255};
                    additive_particle(run, h.to, {std::cos(a) * sp, std::sin(a) * sp}, run.rng.uniform(0.45f, 0.85f),
                                      10.0f, 2.0f, col, PStar, 2.2f);
                }
                spawn_coins(run, h.to, 12 + 4 * difficulty_for(run).stage);
                float_text(run, BOOMS[run.rng.range(0, 3)], h.to + Vec2{0, -40}, Color{255, 225, 75, 255}, 42.0f, 1.3f);
            }
        }
    }
    std::erase_if(run.salama_heads, [](const SalamaHead& h) { return h.bounces <= 0 && h.t >= h.duration; });
}

void update_airstrike(Ctx& c, float dt) {
    auto& run = c.run;
    run.airstrike_cd = std::max(0.0f, run.airstrike_cd - dt);
    if (run.plane_t < 0.0f) return;
    float prev = run.plane_t;
    run.plane_t += dt * 0.88f;
    float lead_x = -120.0f + run.plane_t * (VIRTUAL_W + 280.0f) / 1.35f;
    // Live wingtip contrails + hot afterburner sparks from all 3 V-formation strike jets!
    if (run.rng.chance(dt * 52.0f)) {
        const Vec2 offsets[3] = {{0.0f, -42.0f}, {-86.0f, -112.0f}, {-86.0f, 28.0f}};
        for (int j = 0; j < 3; ++j) {
            Vec2 jp{lead_x + offsets[j].x, run.plane_y + offsets[j].y};
            if (jp.x < -40.0f || jp.x > VIRTUAL_W + 40.0f) continue;
            // Afterburner jet sparks
            spark_streak(run, jp + Vec2{-38.0f, 0.0f}, {-run.rng.uniform(220.0f, 340.0f), run.rng.uniform(-16, 16)},
                         0.16f, 3.0f, run.stats.orbital_strike ? Color{255, 115, 215, 245} : Color{255, 185, 75, 245},
                         3.0f);
            // Crisp white wingtip vapor trails
            if (run.rng.chance(0.55f)) {
                particle(run, jp + Vec2{-14.0f, -44.0f}, {-40.0f, 0.0f}, 0.45f, 4.0f, 11.0f, Color{240, 246, 255, 115},
                         PSmoke, 1.5f);
                particle(run, jp + Vec2{-14.0f, 44.0f}, {-40.0f, 0.0f}, 0.45f, 4.0f, 11.0f, Color{240, 246, 255, 115},
                         PSmoke, 1.5f);
            }
        }
    }
    // Forward strafing autocannon sweep ahead of the bombing run!
    for (int s = 0; s < 5; ++s) {
        float strafe_at = 0.08f + 0.16f * static_cast<float>(s);
        if (prev < strafe_at && run.plane_t >= strafe_at) {
            Vec2 sp{std::clamp(lead_x + 95.0f, 60.0f, VIRTUAL_W - 60.0f), run.plane_y + run.rng.uniform(-42, 42)};
            float sdmg = run.stats.airstrike_damage() * 0.25f;
            explode(c, sp, 48.0f, sdmg, false, Color{255, 210, 95, 255}, false, false, false);
            c.game.sound.play(Sfx::ShotHeavy, 0.35f, 1.35f);
        }
    }
    int total = run.stats.airstrike_bombs();
    for (int i = 0; i < total; ++i) {
        float drop_at = 0.14f + 0.78f * static_cast<float>(i) / static_cast<float>(std::max(1, total - 1));
        if (prev < drop_at && run.plane_t >= drop_at) {
            float x = 55.0f + (VIRTUAL_W - 110.0f) * static_cast<float>(i) / static_cast<float>(std::max(1, total - 1));
            float wing_y = (i % 3 == 0 ? 0.0f : i % 3 == 1 ? -64.0f : 64.0f);
            Vec2 target{std::clamp(x + run.rng.uniform(-22, 22), 50.0f, VIRTUAL_W - 50.0f),
                        std::clamp(run.plane_y + wing_y + run.rng.uniform(-26, 26), HUD_TOP + 55.0f, WALL_TOP - 45.0f)};
            run.shells.push_back({Vec2{target.x - 45.0f, target.y - 210.0f}, target, 0.0f, 0.44f,
                                  run.stats.airstrike_damage(), run.stats.orbital_strike ? 124.0f : 92.0f, true, false,
                                  false, run.stats.orbital_strike, true, false});
        }
    }
    if (run.plane_t > 1.38f) run.plane_t = -1.0f;
}

void update_chain(Ctx& c, float dt) {
    auto& run = c.run;
    if (run.stats.chain_level <= 0) return;
    run.chain_timer -= dt;
    if (run.chain_timer > 0.0f) return;
    run.chain_timer = run.stats.chain_interval();
    int first = pick_front_enemy(run, {run.squad_x, SQUAD_Y}, 2000.0f);
    if (first == -1) return;
    float dmg = run.stats.damage * (2.0f + 0.5f * static_cast<float>(run.stats.chain_level)) *
                overflow_mult(run.soldiers) * run.synergies.elemental_mult();
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
    for (size_t i = 0; i < run.particles.size();) {
        if (run.particles[i].life <= 0.0f) {
            run.particles[i] = run.particles.back();
            run.particles.pop_back();
        } else {
            ++i;
        }
    }
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
    if (run.stats.aegis_wall && run.wall_shield_max > 0.0f && run.wall_shield < run.wall_shield_max) {
        run.wall_shield = std::min(run.wall_shield_max, run.wall_shield + run.wall_shield_max * 0.03f * dt);
    }
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
        init_stage_zones(run);
        run.banner = {run.theme.name, stage_hazard_def(run.theme.hazard).name, run.theme.accent, 2.4f, 2.4f};
    }
    if (run.stats.aegis_wall) {
        run.wall_shield_max = run.stats.wall_max * 0.2f;
        run.wall_shield = run.wall_shield_max;
    }
    run.spawns = build_wave(d.stage, d.wave, run.rng);
    run.mod = roll_wave_mod(d.stage, d.wave, run.rng);
    run.wave_wall_start = run.wall_hp;
    float count_mult = threat_count_mult(run.threat);
    if (run.mod == WaveMod::Swarm) count_mult *= 2.0f;
    if (run.mod == WaveMod::Giants) count_mult *= 0.65f;
    if (run.endless) {
        // Endless Mode: shift biomes/hazards every 5 waves, escalate bosses (double boss at wave 20+!),
        // and rotate twists across realms so every 5-wave chapter feels fresh!
        if (d.wave == WAVES_PER_STAGE && run.wave >= 20) {
            run.spawns.push_back({wave_duration(d.wave) * 0.55f, EnemyType::Boss, VIRTUAL_W * 0.28f, true});
        }
    }
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
        std::string sub = run.wave == 1 ? run.theme.tagline : "HERE THEY COME";
        run.banner = {title, sub, WHITE, 2.2f, 2.2f};
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
    if (run.endless) {
        int tier = std::max(1, run.wave / 5);
        r.gems = r.waves_cleared * 5 + run.gems_found + (r.waves_cleared / 5) * 18 + (r.waves_cleared / 10) * 35;
        (void)tier;
    }
    if (won && !run.endless) {
        r.stars = stars_for_wall(run.wall_hp / run.stats.wall_max);
        int before = meta.stars[static_cast<size_t>(std::clamp(run.stage, 0, MAX_STAGE))];
        r.new_stars = std::max(0, r.stars - before);
        r.new_unlock = meta.unlocked_stage == run.stage && run.stage < MAX_STAGE;
        r.unlocks = meta.finish_stage(run.stage, r.stars);
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

float squad_power_estimate(const Run& run) {
    return squad_power(run);
}

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
    if (!run.endless) return run.stage;
    int realm = 1 + std::max(0, run.wave - 1) / 5;
    return 100 + realm;
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
    run.unlocked_stage =
        game.request.endless ? std::max(meta.unlocked_stage, 5) : std::max(meta.unlocked_stage, run.stage);
    run.endless = game.request.endless;
    run.endless_record_announced = false;
    run.stats = meta.initial_stats();
    run.turret_cost_mult = meta.turret_cost_mult();
    run.xp_gain_mult = meta.xp_gain_mult();
    run.squad = meta.squad();
    run.synergies = compute_synergies(run.squad);
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
    int vis = visible_soldiers(run.soldiers);
    for (int i = 0; i < 64; ++i)
        run.soldier_off_cur[static_cast<size_t>(i)] = soldier_offset(i, std::max(1, vis), run.formation);
    apply_theme(run);
    init_stage_zones(run);
    run.start_power = squad_power(run);
    run.banner = {run.endless ? "ENDLESS" : std::format("STAGE {}", run.stage),
                  run.endless ? "HOW LONG CAN YOU LAST?"
                              : std::format("{}  -  {}", run.theme.name, stage_hazard_def(run.theme.hazard).name),
                  run.theme.accent, 2.5f, 2.5f};
    if (!meta.tutorial_done) {
        run.hint = "DRAG OR A/D TO MOVE  |  PRESS F TO CHANGE FORMATION";
        run.hint_timer = 4.5f;
    }
    game.play_music(run.stage % 2 ? "battle" : "battle2");
}

void cycle_formation(Game& game) {
    if (!game.run) return;
    auto& run = *game.run;
    if (run.picking || run.phase == WavePhase::Lost) return;
    int next = (static_cast<int>(run.formation) + 1) % static_cast<int>(Formation::Count);
    run.formation = static_cast<Formation>(next);
    run.squad_target_x = clamp_squad_x(run.squad_target_x, visible_soldiers(run.soldiers), run.formation);
    game.sound.play(Sfx::Click, 0.8f, 1.15f);
    float_text(run, std::format("{} FORMATION", formation_name(run.formation)), {run.squad_x, SQUAD_Y - 46},
               Color{150, 230, 255, 255}, 26.0f, 1.0f);
    shockwave(run, {run.squad_x, SQUAD_Y + 12}, 25.0f, 115.0f, 0.25f, Color{130, 220, 255, 160});
}

void pick_card(Game& game, int index) {
    auto& run = *game.run;
    if (!run.picking || index < 0 || index > 2) return;
    CardId id = run.offer[static_cast<size_t>(index)];
    float old_max = run.stats.wall_max;
    auto fx = apply_card(run.stats, id);
    run.wall_hp += run.stats.wall_max - old_max;
    if (fx.repair_fraction > 0.0f) {
        run.wall_hp = std::min(run.stats.wall_max, run.wall_hp + run.stats.wall_max * fx.repair_fraction);
    }
    if (fx.grant_shield) {
        run.wall_shield_max = run.stats.wall_max * 0.2f;
        run.wall_shield = run.wall_shield_max;
    }
    if (fx.call_in) {
        std::vector<UnitType> options;
        for (int u = 0; u < UNIT_COUNT; ++u) {
            auto ut = static_cast<UnitType>(u);
            if (std::ranges::find(run.squad, ut) == run.squad.end()) options.push_back(ut);
        }
        if (!options.empty()) {
            UnitType pick = options[static_cast<size_t>(run.rng.range(0, static_cast<int>(options.size()) - 1))];
            run.squad.push_back(pick);
            run.synergies = compute_synergies(run.squad);
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
    if (!turret_unlocked(type, run.unlocked_stage)) return;
    if (t.type != TurretType::None && t.type != type) return;
    if (t.level >= TURRET_MAX_LEVEL) return;
    int cost = turret_upgrade_cost(type, t.level, run.turret_cost_mult);
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
    int refund = turret_refund(t.type, t.level, run.turret_cost_mult);
    run.coins += refund;
    float_text(run, std::format("+{}", refund), turret_pos(slot) + Vec2{0, -40}, Color{255, 220, 80, 255}, 30.0f);
    t = Turret{};
    game.sound.play(Sfx::Coin, 0.6f);
}

int repair_cost(const Run& run) {
    return 80 + 60 * run.repairs;
}

int wall_tier(const Run& run) {
    if (run.stats.aegis_wall || run.stats.wall_max >= 1950.0f || run.stats.level_of(CardId::Fortify) >= 2) return 2;
    if (run.stats.wall_max >= 1300.0f || run.stats.level_of(CardId::Fortify) >= 1) return 1;
    return 0;
}

void repair_wall(Game& game) {
    auto& run = *game.run;
    int cost = repair_cost(run);
    if (run.coins < cost || run.wall_hp >= run.stats.wall_max) return;
    run.coins -= cost;
    run.repairs++;
    float pct = 0.2f + 0.06f * static_cast<float>(run.stats.field_repair_level);
    run.wall_hp = std::min(run.stats.wall_max, run.wall_hp + run.stats.wall_max * pct);
    game.sound.play(Sfx::Build, 0.9f, 0.8f);
    for (int i = 0; i < 24; ++i) {
        Vec2 p{run.rng.uniform(0, VIRTUAL_W), run.rng.uniform(WALL_TOP, WALL_BOTTOM)};
        additive_particle(run, p, {0, -60}, 0.7f, 6.0f, 1.0f, Color{120, 255, 150, 255}, PGlow, 1.0f);
    }
    float_text(run, std::format("+{:.0f}% WALL", pct * 100.0f), {VIRTUAL_W * 0.5f, WALL_TOP - 30},
               Color{120, 255, 150, 255}, 34.0f);
}

void trigger_frenzy(Game& game) {
    auto& run = *game.run;
    if (run.rage < 1.0f || run.frenzy_t > 0.0f || run.phase == WavePhase::Lost) return;
    run.rage = 0.0f;
    run.frenzy_t = 6.0f + 1.5f * static_cast<float>(run.stats.level_of(CardId::Adrenaline));
    run.zoom_kick = 0.8f;
    run.flash = 0.35f;
    run.banner = {"OMAR'S RALLY!", "YALLA! x2 FIRE RATE + CRIT + PUSH", Color{255, 190, 50, 255}, 1.6f, 1.6f};
    game.sound.play(Sfx::BossRoar, 0.7f, 1.6f);
    game.sound.play(Sfx::LevelUp, 0.8f, 0.8f);
    // Rally wave pushes back monsters crowding the wall!
    for (auto& e : run.enemies) {
        if (!e.dead && e.pos.y > WALL_TOP - 180.0f) {
            e.knock += 110.0f * (1.0f - e.knock_resist * 0.6f);
            e.at_wall = false;
        }
    }
    Vec2 at{run.squad_x, SQUAD_Y};
    for (int r = 0; r < 2; ++r)
        shockwave(run, at, 30.0f, 260.0f + 140.0f * static_cast<float>(r), 0.45f, Color{255, 190, 50, 220});
}

void trigger_ali(Game& game) {
    auto& run = *game.run;
    if (run.ali_charge < 1.0f || run.ali_t >= 0.0f || run.phase == WavePhase::Lost) return;
    run.ali_charge = 0.0f;
    run.ali_t = 0.0f;
    int adr = run.stats.level_of(CardId::Adrenaline);
    float heal = run.stats.wall_max * (0.10f + 0.03f * static_cast<float>(adr));
    if (run.wall_hp < run.stats.wall_max) run.wall_hp = std::min(run.stats.wall_max, run.wall_hp + heal);
    run.banner = {"ALI'S PEACE SIGN!", "HORDE CHARMED + WALL HEALED!", Color{255, 130, 170, 255}, 1.8f, 1.8f};
    game.sound.play(Sfx::GateGood, 1.0f, 1.3f);
    game.sound.play(Sfx::Gem, 0.9f, 0.8f);
    float bonus = 0.7f * static_cast<float>(adr);
    int charmed = 0;
    for (auto& e : run.enemies) {
        if (e.dead) continue;
        e.charm = (e.type == EnemyType::Boss ? 1.8f : 3.8f) + bonus;
        e.burrowed = false;
        charmed++;
    }
    if (charmed > 0) spawn_coins(run, {VIRTUAL_W * 0.5f, WALL_TOP - 90}, std::min(40, 8 + charmed / 2));
    float_text(run, std::format("+{:.0f} WALL", heal), {VIRTUAL_W * 0.5f, WALL_TOP - 28}, Color{255, 150, 200, 255},
               28.0f, 1.2f);
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

void trigger_salama(Game& game) {
    auto& run = *game.run;
    if (run.salama_charge < 1.0f || run.salama_t >= 0.0f || run.phase == WavePhase::Lost) return;
    run.salama_charge = 0.0f;
    run.salama_t = 0.0f;
    run.zoom_kick = 0.75f;
    int adr = run.stats.level_of(CardId::Adrenaline);
    int heads = 3 + (adr > 0 ? 1 : 0);
    float dmg = std::max(150.0f, squad_power(run) * (1.85f + 0.45f * static_cast<float>(adr)));
    run.banner = {"SALAMA BABA BOOM!", "GIANT EXPLODING DAD HEADS!", Color{255, 210, 65, 255}, 1.8f, 1.8f};
    game.sound.play(Sfx::Cannon, 0.9f, 0.75f);
    game.sound.play(Sfx::LevelUp, 0.8f, 1.1f);
    Vec2 launch{VIRTUAL_W * 0.5f, WALL_TOP - 28.0f};
    shockwave(run, launch, 25.0f, 240.0f, 0.45f, Color{255, 215, 70, 220});
    for (int i = 0; i < heads; ++i) {
        // Aim at bosses or spread across dense enemy clusters on the field!
        Vec2 target{120.0f + static_cast<float>(i) * (VIRTUAL_W - 240.0f) / static_cast<float>(std::max(1, heads - 1)),
                    run.rng.uniform(340.0f, WALL_TOP - 190.0f)};
        float best_score = -1.0f;
        for (const auto& e : run.enemies) {
            if (e.dead || e.pos.y < HUD_TOP + 30.0f) continue;
            float lane_pen = std::abs(e.pos.x - target.x) * 0.45f;
            float score = e.pos.y + (e.type == EnemyType::Boss ? 600.0f : 0.0f) + (e.elite ? 220.0f : 0.0f) - lane_pen;
            if (score > best_score) {
                best_score = score;
                target = e.pos;
            }
        }
        SalamaHead sh;
        sh.from = launch + Vec2{(static_cast<float>(i) - static_cast<float>(heads - 1) * 0.5f) * 32.0f, 0.0f};
        sh.to = target;
        sh.t = -static_cast<float>(i) * 0.14f;
        sh.duration = 0.76f;
        sh.damage = dmg;
        sh.radius = 148.0f + 16.0f * static_cast<float>(adr);
        sh.spin = (i % 2 == 0 ? 1.0f : -1.0f) * run.rng.uniform(460.0f, 640.0f);
        sh.bounces = 1;
        run.salama_heads.push_back(sh);
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
    run.plane_y = std::clamp(HUD_TOP + 60.0f * static_cast<float>(best) + 60.0f, HUD_TOP + 120.0f, WALL_TOP - 95.0f);
    run.plane_t = 0.0f;
    run.airstrike_cd = run.stats.airstrike_cooldown();
    run.banner = {run.stats.orbital_strike ? "ORBITAL FURY!" : "AIRSTRIKE INBOUND!",
                  run.stats.orbital_strike ? "NAPALM CARPET BOMBING!" : "V-SQUADRON CARPET BOMBING!",
                  run.stats.orbital_strike ? Color{255, 110, 200, 255} : Color{255, 165, 60, 255}, 1.3f, 1.3f};
    add_shake(run, 6.0f);
    game.sound.play(Sfx::Laser, 0.55f, 0.45f);
    game.sound.play(Sfx::Explosion, 0.4f, 0.65f);
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
        run.ali_charge = std::min(1.0f, run.ali_charge + (real_dt / 45.0f) * run.stats.ability_charge_mult);
    }
    if (run.salama_t >= 0.0f) {
        run.salama_t += real_dt;
        if (run.salama_t > 2.2f) run.salama_t = -1.0f;
    } else if (run.phase == WavePhase::Fighting) {
        run.salama_charge = std::min(1.0f, run.salama_charge + (real_dt / 36.0f) * run.stats.ability_charge_mult);
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
    run.squad_target_x = clamp_squad_x(run.squad_target_x, visible, run.formation);
    run.squad_x += (run.squad_target_x - run.squad_x) * std::min(1.0f, real_dt * 14.0f);
    run.squad_x = clamp_squad_x(run.squad_x, visible, run.formation);
    for (int i = 0; i < visible; ++i) {
        Vec2 want = soldier_offset(i, visible, run.formation);
        Vec2& cur = run.soldier_off_cur[static_cast<size_t>(i)];
        if (i >= run.shown_soldiers) {
            cur = want;
        } else {
            cur = cur + (want - cur) * std::min(1.0f, real_dt * 12.0f);
        }
    }
    for (auto& r : run.soldier_recoil) r = std::max(0.0f, r - real_dt * 8.0f);
    for (auto& j : run.soldier_join) j = std::max(0.0f, j - real_dt * 2.5f);
    if (visible > run.shown_soldiers) {
        for (int i = run.shown_soldiers; i < visible; ++i) {
            run.soldier_join[static_cast<size_t>(i)] = 1.0f + static_cast<float>(i - run.shown_soldiers) * 0.04f;
            Vec2 p = Vec2{run.squad_x, SQUAD_Y} + run.soldier_off_cur[static_cast<size_t>(i)];
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
                if (run.endless) {
                    // Celebrate beating your personal best Endless wave live!
                    long long prev_best = game.meta.stat(Lifetime::BestEndlessWave);
                    if (prev_best > 0 && run.wave > prev_best && !run.endless_record_announced) {
                        run.endless_record_announced = true;
                        game.toast(std::format("NEW ENDLESS RECORD: WAVE {}!", run.wave), Color{255, 215, 75, 255});
                        game.sound.play(Sfx::LevelUp, 0.9f, 1.2f);
                    }
                    // Every 5 waves in Endless: Realm Shift + Wall Repair + Gem Cache (+ Free Card Pick every 10
                    // waves)!
                    if (run.wave % 5 == 0) {
                        int realm = run.wave / 5;
                        bool major = (run.wave % 10 == 0);
                        float heal_pct = major ? 0.45f : 0.25f;
                        float heal = run.stats.wall_max * heal_pct;
                        if (run.wall_hp < run.stats.wall_max) {
                            run.wall_hp = std::min(run.stats.wall_max, run.wall_hp + heal);
                        }
                        int gem_bonus = (major ? 12 : 6) + realm * 2;
                        int coin_bonus = (major ? 120 : 60) + realm * 25;
                        run.gems_found += gem_bonus;
                        run.coins += coin_bonus;
                        run.tally.coins_earned += coin_bonus;
                        if (major) {
                            run.pending_levels++;
                            run.card_delay = 1.1f;
                        }
                        run.banner = {major ? std::format("ENDLESS MILESTONE {}!", run.wave)
                                            : std::format("REALM {} CLEARED!", realm),
                                      major ? std::format("+{} GEMS  +45% WALL  +FREE CARD!", gem_bonus)
                                            : std::format("+{} GEMS  +25% WALL  +{} COINS", gem_bonus, coin_bonus),
                                      Color{255, 215, 75, 255}, 2.8f, 2.8f};
                        game.sound.play(Sfx::Victory, 0.7f, 1.1f);
                        game.play_music(realm % 2 ? "battle2" : "battle");
                        run.wave_theme_pending = true;
                    }
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
    update_stage_hazards(c, dt);
    update_bullets(c, dt);
    update_enemies(c, dt);
    update_boss_hazards(c, dt);
    update_acids(c, dt);
    update_gates(c, dt);
    update_loot(c, dt);
    update_turrets(c, dt);
    update_shells(c, dt);
    update_salama_heads(c, dt);
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
