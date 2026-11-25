#include "core/game.hpp"
#include "game/formation.hpp"
#include "game/render.hpp"
#include "game/render_parts.hpp"
#include "game/sim.hpp"
#include "game/turrets.hpp"
#include "states/states.hpp"
#include <format>

// LS_GALLERY renders every entity, state and effect in isolation, with collision radii and
// attachment points overlaid, so the art can be checked one asset at a time.

namespace ls {

namespace {

constexpr Color LABEL{230, 230, 240, 255};
constexpr Color GUIDE{255, 60, 90, 200};
constexpr Color MARK{80, 255, 120, 255};

void backdrop(const char* title) {
    ClearBackground(Color{62, 66, 74, 255});
    for (int y = 0; y < VIRTUAL_H; y += 40) {
        for (int x = (y / 40) % 2 * 40; x < VIRTUAL_W; x += 80) DrawRectangle(x, y, 40, 40, Color{70, 74, 82, 255});
    }
    DrawRectangle(0, 0, VIRTUAL_W, 56, Color{20, 20, 30, 255});
    DrawText(title, 16, 16, 24, WHITE);
}

void label(Game& game, const std::string& s, float x, float y, float size = 16) {
    text(game.gfx, s, {x, y}, size, LABEL, Align::Center, false);
}

void cross(Vec2 p, float r, Color c) {
    DrawLineEx({p.x - r, p.y}, {p.x + r, p.y}, 1.5f, c);
    DrawLineEx({p.x, p.y - r}, {p.x, p.y + r}, 1.5f, c);
}

Run& fresh_run(Game& game) {
    game.run = std::make_unique<Run>();
    game.run->squad = {UnitType::Rifleman};
    return *game.run;
}

void page_enemies(Game& game) {
    backdrop("ENEMIES  (red ring = collision radius, cross = position)");
    const char* cols[] = {"NORMAL", "HIT FLASH", "SLOWED+ARMOR", "ELITE"};
    for (int c = 0; c < 4; ++c) label(game, cols[c], 150.0f + static_cast<float>(c) * 160.0f, 76);
    for (int t = 0; t < static_cast<int>(EnemyType::Count); ++t) {
        auto type = static_cast<EnemyType>(t);
        float y = 130.0f + static_cast<float>(t) * 116.0f;
        float r = base_stats(type).radius * (type == EnemyType::Boss ? 0.6f : 1.0f);
        const char* names[] = {"WALKER",   "RUNNER",   "BRUTE", "EXPLODER", "SPITTER",
                               "SHIELDER", "SPLITTER", "FLYER", "BARREL",   "BOSS x0.6"};
        text(game.gfx, names[t], {8, y}, 14, LABEL, Align::Left, false);
        for (int c = 0; c < 4; ++c) {
            Vec2 p{150.0f + static_cast<float>(c) * 160.0f, y};
            float shield = type == EnemyType::Shielder ? (c == 2 ? 0.3f : 1.0f) : 0.0f;
            if (type == EnemyType::Boss) {
                draw_boss(game.gfx, make_boss(3 + c, false), c, p, r, 0.3f, c == 1 ? 0.09f : 0.0f, c == 3, game.time,
                          c == 2 ? 0.7f : 0.0f);
            } else {
                draw_monster(game.gfx, type, p, r, 0.3f, c == 1 ? 0.09f : 0.0f, c == 2, c == 3, game.time, shield, 0.0f,
                             c == 2);
            }
            DrawCircleLinesV(p.to_raylib(), r, GUIDE);
            cross(p, 6, GUIDE);
        }
    }
}

void page_units(Game& game) {
    backdrop("UNITS  (green dot = muzzle_offset, bullets spawn here)");
    const char* cols[] = {"IDLE x1.3", "FIRING x1.3", "IDLE x3", "FIRING x3"};
    for (int c = 0; c < 4; ++c) label(game, cols[c], 140.0f + static_cast<float>(c) * 150.0f, 76);
    for (int u = 0; u < UNIT_COUNT; ++u) {
        float y = 170.0f + static_cast<float>(u) * 180.0f;
        text(game.gfx, unit_def(static_cast<UnitType>(u)).name, {8, y - 60}, 16, LABEL, Align::Left, false);
        for (int c = 0; c < 4; ++c) {
            float scale = c < 2 ? SOLDIER_SCALE : 3.0f;
            float recoil = (c % 2) ? 1.0f : 0.0f;
            Vec2 p{140.0f + static_cast<float>(c) * 150.0f, y};
            draw_soldier(game.gfx, p, recoil, static_cast<UnitType>(u), scale, game.time);
            cross(p, 4, GUIDE);
            disc(game.gfx, p + muzzle_offset(u, scale), 3.0f, MARK);
        }
    }
}

void page_turrets(Game& game) {
    backdrop("TURRETS  (green dot = barrel tip, shots spawn here)");
    const char* cols[] = {"LV1 UP", "LV3 UP-RIGHT", "LV5 UP-LEFT"};
    for (int c = 0; c < 3; ++c) label(game, cols[c], 140.0f + static_cast<float>(c) * 160.0f, 76);
    float aims[] = {-PI / 2, -PI / 4, -PI * 0.85f};
    int levels[] = {1, 3, 5};
    for (int t = 1; t <= TURRET_KINDS; ++t) {
        float y = 180.0f + static_cast<float>(t - 1) * 200.0f;
        text(game.gfx, turret_def(static_cast<TurretType>(t)).name, {8, y - 70}, 16, LABEL, Align::Left, false);
        for (int c = 0; c < 3; ++c) {
            Vec2 p{140.0f + static_cast<float>(c) * 160.0f, y};
            draw_turret(game.gfx, static_cast<TurretType>(t), levels[c], p, aims[c], 0.0f, game.time, 0.8f);
            Vec2 tip = t == static_cast<int>(TurretType::Tesla)
                           ? p + TESLA_ORB
                           : p + Vec2{std::cos(aims[c]), std::sin(aims[c])} * TURRET_BARREL;
            disc(game.gfx, tip, 3.0f, MARK);
            cross(p, 5, GUIDE);
        }
    }
}

void page_gates_projectiles(Game& game) {
    backdrop("GATES + PROJECTILES");
    auto& run = fresh_run(game);
    auto gate = [&](GateKind k, int v, bool red, float x, float y, float w) {
        Gate g;
        g.state = {k, v, red, 0, 0};
        g.x = x;
        g.y = y;
        g.w = w;
        draw_gate(game, g);
        DrawRectangleLinesEx({x - w / 2, y - GATE_H / 2, w, GATE_H}, 1.5f, GUIDE);
    };
    gate(GateKind::Add, -6, false, 180, 130, 338);
    gate(GateKind::Add, -8, true, 540, 130, 338);
    gate(GateKind::Mul, 2, false, 180, 240, 338);
    gate(GateKind::Mul, -2, true, 540, 240, 338);
    gate(GateKind::Add, 12, false, 120, 350, 218);
    gate(GateKind::Add, -40, true, 360, 350, 218);
    gate(GateKind::Mul, 1, false, 600, 350, 218);
    gate(GateKind::Rate, 14, false, 180, 430, 338);
    gate(GateKind::Power, -22, true, 540, 430, 338);

    label(game, "BULLETS PER UNIT (flying up)", 360, 490);
    for (int u = 0; u < UNIT_COUNT; ++u) {
        Bullet b;
        b.pos = {80.0f + static_cast<float>(u) * 110.0f, 540};
        b.vel = {0, -1000};
        b.unit = static_cast<UnitType>(u);
        b.flame = unit_def(b.unit).shot == Shot::Flame;
        b.radius = b.flame ? 16.0f : b.unit == UnitType::Sniper ? 6.0f : BULLET_RADIUS;
        b.life = b.flame ? 0.2f : 1.0f;
        run.bullets.push_back(b);
        label(game, unit_def(b.unit).name, b.pos.x, 600, 12);
    }
    Bullet crit;
    crit.pos = {130, 700};
    crit.vel = {0, -1000};
    crit.crit = true;
    run.bullets.push_back(crit);
    label(game, "CRIT", 130, 740, 12);
    Bullet shard;
    shard.pos = {250, 700};
    shard.vel = {700, -300};
    shard.shard = true;
    shard.radius = 6;
    run.bullets.push_back(shard);
    label(game, "SHARD", 250, 740, 12);
    run.acids.push_back({{370, 700}, {0, 300}, 1});
    label(game, "ACID", 370, 740, 12);
    Shell grenade{{450, 820}, {450, 620}, 0.25f, 0.5f, 1, 50, false};
    grenade.grenade = true;
    run.shells.push_back(grenade);
    label(game, "GRENADE", 450, 840, 12);
    run.shells.push_back({{560, 820}, {560, 640}, 0.3f, 0.6f, 1, 60, false});
    label(game, "CANNON SHELL", 560, 840, 12);
    run.shells.push_back({{660, 600}, {660, 760}, 0.2f, 0.45f, 1, 80, true});
    label(game, "BOMB", 660, 840, 12);
    draw_shells(game, run);
    draw_projectiles(game, run);
    run.coin_flies.push_back({{130, 950}, {}, 1.0f, 1});
    disc(game.gfx, {130, 950}, 10, Color{150, 100, 10, 255});
    disc(game.gfx, {130, 950}, 8, Color{255, 205, 50, 255});
    label(game, "COIN", 130, 985, 12);

    label(game, "LOCKED LOOT: ice / crate / stone / cage / crystal", 360, 1050);
    LootKind kinds[] = {LootKind::Soldiers, LootKind::Turret, LootKind::Card, LootKind::Coins, LootKind::Gems};
    LockKind locks[] = {LockKind::Ice, LockKind::Crate, LockKind::Stone, LockKind::Cage, LockKind::Crystal};
    for (int k = 0; k < 5; ++k) {
        Loot f;
        f.kind = kinds[k];
        f.lock = locks[k];
        f.unit = UnitType::Gunner;
        f.turret = TurretType::Tesla;
        f.amount = 12;
        f.max_hp = 100;
        f.hp = k < 2 ? 100.0f : k < 4 ? 50.0f : 10.0f;
        f.seed = static_cast<float>(k) * 7.0f;
        f.pos = {75.0f + static_cast<float>(k) * 142.0f, 1160};
        draw_loot(game, f);
    }
}

void page_effects(Game& game) {
    backdrop("EFFECTS TIMELAPSE  (seconds after trigger)");
    float times[] = {0.03f, 0.1f, 0.2f, 0.35f, 0.6f, 0.9f};
    for (int c = 0; c < 6; ++c)
        label(game, std::format("{:.2f}s", times[c]), 70.0f + static_cast<float>(c) * 116.0f, 76);
    const char* rows[] = {"EXPLOSION", "WALKER DEATH", "ELITE DEATH", "EXPLODER DEATH", "SPLITTER DEATH", "GATE PASS"};
    for (int r = 0; r < 6; ++r) {
        float y = 170.0f + static_cast<float>(r) * 190.0f;
        text(game.gfx, rows[r], {8, y - 80}, 14, LABEL, Align::Left, false);
        for (int c = 0; c < 6; ++c) {
            Vec2 p{70.0f + static_cast<float>(c) * 116.0f, y};
            auto& run = fresh_run(game);
            run.soldiers = 20;
            run.squad_x = p.x;
            if (r == 0) {
                gallery_explosion(run, p, 60.0f);
            } else if (r == 5) {
                Gate g;
                g.state = {GateKind::Add, 8, false, 0, 0};
                g.x = p.x;
                gallery_gate_pass(game, g);
                for (auto& pt : run.particles) pt.pos = pt.pos - Vec2{0, SQUAD_Y - 40 - y};
                for (auto& t : run.texts) t.pos = t.pos - Vec2{0, SQUAD_Y - 40 - y};
            } else {
                Enemy e;
                e.type = r == 3 ? EnemyType::Exploder : r == 4 ? EnemyType::Splitter : EnemyType::Walker;
                e.pos = p;
                e.radius = base_stats(e.type).radius;
                e.hp = e.max_hp = 10;
                e.elite = r == 2;
                run.enemies.push_back(e);
                gallery_kill(game, 0);
                run.enemies.clear();
                run.texts.clear();
            }
            float t = 0.0f;
            while (t < times[c]) {
                gallery_step(run, 1.0f / 120.0f);
                t += 1.0f / 120.0f;
            }
            BeginScissorMode(static_cast<int>(p.x - 58), static_cast<int>(y - 90), 116, 180);
            for (const auto& n : run.new_enemies) {
                draw_monster(game.gfx, n.type, n.pos, n.radius, 0, 0, false, false, game.time);
            }
            draw_particles(game, run, false);
            BeginBlendMode(BLEND_ADDITIVE);
            draw_particles(game, run, true);
            EndBlendMode();
            for (const auto& tx : run.texts) text(game.gfx, tx.text, tx.pos, tx.size * 0.6f, tx.color);
            EndScissorMode();
            cross(p, 4, GUIDE);
        }
    }
}

void page_cards(Game& game) {
    backdrop("CARDS");
    float cw = 212.0f * 0.62f;
    float ch = 380.0f * 0.62f;
    for (int i = 0; i < CARD_COUNT; ++i) {
        int col = i % 5;
        int row = i / 5;
        Rectangle r{10.0f + static_cast<float>(col) * (cw + 9), 80.0f + static_cast<float>(row) * (ch + 22), cw, ch};
        draw_card_face(game, static_cast<CardId>(i), r, i % 3);
    }
}

constexpr int THEME_SAMPLES[2][4] = {{1, 2, 3, 4}, {6, 7, 11, 14}};

void page_themes(Game& game, int set) {
    backdrop("STAGE THEMES  (name, weather, night, decor, lock, enemy tint)");
    for (int i = 0; i < 4; ++i) {
        Theme th = make_theme(THEME_SAMPLES[set][i]);
        auto& run = fresh_run(game);
        run.theme = th;
        run.wall_hp = run.stats.wall_max * (1.0f - 0.3f * static_cast<float>(i));
        float band_top = 60.0f + static_cast<float>(i) * 305.0f;
        Camera2D cam{};
        cam.zoom = 1.0f;
        cam.offset = {0, band_top - (WALL_TOP - 230.0f)};
        BeginScissorMode(0, static_cast<int>(band_top), VIRTUAL_W, 300);
        BeginMode2D(cam);
        draw_backdrop(game, th, game.time);
        for (int k = 0; k < 4; ++k) {
            draw_monster(game.gfx, static_cast<EnemyType>(k), {200.0f + static_cast<float>(k) * 90.0f, WALL_TOP - 120},
                         22, 0.2f, 0, false, false, game.time, 0, 0, false, th.enemy_tint, th.enemy_tint_amount,
                         th.biome);
        }
        Loot l;
        l.kind = LootKind::Soldiers;
        l.lock = th.lock;
        l.unit = UnitType::Gunner;
        l.amount = 8;
        l.hp = 60;
        l.max_hp = 100;
        l.pos = {600, WALL_TOP - 130};
        draw_night(th);
        draw_wall(run);
        EndMode2D();
        EndScissorMode();
        BeginMode2D(cam);
        draw_loot(game, l);
        EndMode2D();
        const char* weather[] = {"CLEAR", "SNOW", "RAIN", "LEAVES", "EMBERS", "DUST", "FIREFLIES"};
        const char* locks[] = {"ICE", "CRATE", "STONE", "CAGE", "CRYSTAL"};
        const char* decor[] = {"TREES", "ROCKS", "BONES", "CRYSTALS", "RUINS"};
        DrawRectangle(0, static_cast<int>(band_top), VIRTUAL_W, 30, Color{0, 0, 0, 150});
        text(game.gfx,
             std::format("STAGE {}: {}  |  {}{}  |  {}  |  LOCK {}", th.key, th.name,
                         weather[static_cast<int>(th.weather)], th.night > 0 ? " NIGHT" : "",
                         decor[static_cast<int>(th.decor)], locks[static_cast<int>(th.lock)]),
             {8, band_top + 15}, 16, WHITE, Align::Left, false);
    }
}

void page_bosses(Game& game) {
    backdrop("PROCEDURAL BOSSES  (stage seed -> genome + powers)");
    int keys[] = {1, 3, 5, 7, 9, 11, 13, 15};
    for (int i = 0; i < 8; ++i) {
        BossGenome gen = make_boss(keys[i], false);
        float x = 190.0f + static_cast<float>(i % 2) * 340.0f;
        float y = 190.0f + static_cast<float>(i / 2) * 290.0f;
        draw_boss(game.gfx, gen, (keys[i] - 1) % 4, {x, y}, 70, 0.3f, 0, i == 5, game.time, i == 2 ? 0.8f : 0.0f);
        text_fit(game.gfx, gen.name, {x, y + 118}, 20, 320, Color{255, 200, 120, 255});
        std::string powers;
        for (int k = 0; k < gen.power_count; ++k) {
            if (k) powers += " + ";
            powers += boss_power_name(gen.powers[static_cast<size_t>(k)]);
        }
        text_fit(game.gfx, powers, {x, y + 142}, 15, 320, LABEL, false);
    }
}

void page_outfits(Game& game) {
    backdrop("THEME OUTFITS  (grass / desert / snow / ash)");
    const char* names[] = {"GRASS", "DESERT", "SNOW", "ASH"};
    for (int b = 0; b < 4; ++b) {
        float y = 150.0f + static_cast<float>(b) * 290.0f;
        text(game.gfx, names[b], {8, y - 70}, 18, LABEL, Align::Left, false);
        for (int t = 0; t < 4; ++t) {
            auto type = static_cast<EnemyType>(t);
            draw_monster(game.gfx, type, {70.0f + static_cast<float>(t) * 82.0f, y}, base_stats(type).radius * 1.2f,
                         0.3f, 0, false, false, game.time, 0, 0, false, WHITE, 0, b);
        }
        for (int u = 0; u < UNIT_COUNT; ++u) {
            draw_soldier(game.gfx,
                         {400.0f + static_cast<float>(u % 3) * 100.0f, y - 40.0f + static_cast<float>(u / 3) * 90.0f},
                         0, static_cast<UnitType>(u), 2.6f, game.time, b);
        }
    }
}

void stage_battle(Game& game) {
    game.request = {3, false};
    start_run(game);
    auto& run = *game.run;
    run.phase = WavePhase::Fighting;
    run.wave = 4;
    run.soldiers = 26;
    run.squad = {UnitType::Rifleman, UnitType::Gunner, UnitType::Grenadier};
    run.coins = 140;
    run.turrets[0] = {TurretType::Cannon, 2};
    run.turrets[1] = {TurretType::Tesla, 1};
    run.turrets[3] = {TurretType::Laser, 3};
    Rng rng{99};
    for (int i = 0; i < 70; ++i) {
        SpawnEvent ev{0,
                      i % 9 == 0   ? EnemyType::Brute
                      : i % 7 == 0 ? EnemyType::Shielder
                      : i % 5 == 0 ? EnemyType::Runner
                                   : EnemyType::Walker,
                      rng.uniform(40, VIRTUAL_W - 40)};
        Enemy e;
        e.type = ev.type;
        e.pos = {ev.x, rng.uniform(250, WALL_TOP - 20)};
        e.radius = base_stats(e.type).radius;
        e.hp = e.max_hp = 100;
        e.anim = rng.uniform(0, 5);
        if (e.type == EnemyType::Shielder) e.shield_hp = e.shield_max = 100;
        run.enemies.push_back(e);
    }
    auto row = roll_gate_row(rng, 4, run.soldiers, 2);
    for (int i = 0; i < 2; ++i) {
        Gate g;
        g.state = row[static_cast<size_t>(i)];
        g.x = 180.0f + static_cast<float>(i) * 360.0f;
        g.y = 330;
        g.w = 338;
        g.row = 1;
        run.gates.push_back(g);
    }
    for (int i = 0; i < 40; ++i) update_run(game, 1.0f / 60.0f);
}

MenuState g_menu;
ArmoryState g_armory;
ResultsState g_results;
MissionsState g_missions;

} // namespace

int gallery_page_count() {
    return 19;
}

// Everything that renders into an offscreen target (ground textures, decals) happens here,
// before the caller binds the canvas, because raylib texture modes cannot nest.
void gallery_prepare(Game& game, int page) {
    game.meta.last_daily_day = today_index();
    if (page == 6 || page == 16) {
        for (int k : THEME_SAMPLES[page == 6 ? 0 : 1]) build_ground(game, make_theme(k));
    }
    switch (page) {
    case 7:
    case 8:
    case 9:
        stage_battle(game);
        build_ground(game, game.run->theme);
        for (int i = 0; i < 90; ++i) update_run(game, 1.0f / 60.0f);
        flush_decals(game);
        if (page == 8) {
            game.run->offer = {CardId::Multishot, CardId::BulletStorm, CardId::Fortify};
            game.run->picking = true;
            game.run->pick_anim = 2.0f;
        }
        if (page == 9) game.run->build_slot = 2;
        break;
    case 10:
        g_menu.enter(game);
        for (int i = 0; i < 180; ++i) g_menu.update(game, 1.0f / 60.0f);
        break;
    case 11:
    case 13:
        g_armory.enter(game);
        g_armory.set_tab(page == 13 ? 1 : 0);
        break;
    case 15:
        stage_battle(game);
        build_ground(game, game.run->theme);
        game.paused = true;
        break;
    case 14:
        g_missions.enter(game);
        break;
    case 12:
        game.result = {true, false, 2, 10, 2, 180, 2, true, {}};
        game.result.tally.kills = 1423;
        game.result.tally.max_soldiers = 211;
        g_results.enter(game);
        for (int i = 0; i < 240; ++i) g_results.update(game, 1.0f / 60.0f);
        break;
    default:
        break;
    }
}

void render_gallery(Game& game, int page) {
    switch (page) {
    case 0:
        page_enemies(game);
        break;
    case 1:
        page_units(game);
        break;
    case 2:
        page_turrets(game);
        break;
    case 3:
        page_gates_projectiles(game);
        break;
    case 4:
        page_effects(game);
        break;
    case 5:
        page_cards(game);
        break;
    case 6:
        page_themes(game, 0);
        break;
    case 16:
        page_themes(game, 1);
        break;
    case 17:
        page_bosses(game);
        break;
    case 18:
        page_outfits(game);
        break;
    case 7:
    case 8:
    case 9:
        render_world(game);
        render_hud(game);
        break;
    case 10:
        g_menu.render(game);
        break;
    case 11:
        g_armory.render(game);
        break;
    case 12:
        g_results.render(game);
        break;
    case 13:
        g_armory.render(game);
        break;
    case 14:
        game.meta.lifetime[static_cast<size_t>(Lifetime::Kills)] = 3200;
        game.meta.lifetime[static_cast<size_t>(Lifetime::GatesTaken)] = 14;
        g_missions.render(game);
        break;
    case 15:
        render_world(game);
        render_hud(game);
        game.paused = false;
        break;
    default:
        break;
    }
}

} // namespace ls
