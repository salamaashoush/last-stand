#include "game/probe.hpp"
#include "core/game.hpp"
#include "game/sim.hpp"
#include "game/turrets.hpp"
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// LS_PROBE runs the same seeded fight with and without each upgrade and checks that the
// upgrade moves the number it is supposed to move. It proves upgrades are wired into the
// simulation, which the unit tests on stat arithmetic alone cannot.

namespace ls {

namespace {

struct Metrics {
    double damage{};
    int kills{};
    int coins{};
    float wall_lost{};
    float wall_max{};
    int soldiers{};
    int squad_types{};
    int gate_value{};
    int rerolls{};
    int start_coins{};
    float hero_charge{};
    int xp_earned{};
};

using Setup = std::function<void(Game&)>;

struct Scenario {
    bool gates{false};
    bool airstrikes{false};
    int hero_trigger{0}; // 1=Salama, 2=Omar, 3=Ali fired mid-fight when enemies fill the field
    float seconds{30.0f};
};

Metrics fight(Game& game, const MetaProgress& meta, const Setup& before_start, const Setup& after_start, Scenario sc) {
    game.meta = meta;
    if (before_start) before_start(game);
    game.request = {2, false};
    start_run(game);
    auto& run = *game.run;
    run.rng = Rng{424242};
    run.phase = WavePhase::Fighting;
    run.wave = 3;
    run.squad_x = run.squad_target_x = VIRTUAL_W * 0.5f;
    if (after_start) after_start(game);
    // The wall must survive the whole fight or soldier and wall readings collapse to zero.
    float wall_max = run.stats.wall_max;
    run.wall_hp = 1e6f;
    Metrics m;
    m.start_coins = run.coins;
    m.rerolls = run.rerolls;
    // A steady, dense stream across the field so damage is never capped by running out of
    // targets and area effects always have neighbours to hit.
    run.spawns.clear();
    Rng spawn_rng{99};
    for (int i = 0; i < 900; ++i) {
        EnemyType type = i % 9 == 0 ? EnemyType::Brute : i % 4 == 0 ? EnemyType::Runner : EnemyType::Walker;
        run.spawns.push_back(
            {sc.seconds * static_cast<float>(i) / 900.0f, type, spawn_rng.uniform(60, VIRTUAL_W - 60)});
    }
    run.spawn_cursor = 0;
    run.wave_time = 0.0f;
    if (sc.gates) {
        run.spawns.clear();
        Gate g;
        g.state = {GateKind::Add, -20, false, 0, 0};
        g.x = run.squad_x;
        g.y = 300.0f;
        g.w = 300.0f;
        g.row = 99;
        run.gates.push_back(g);
    }
    float start_wall = run.wall_hp;
    m.wall_max = wall_max;
    int gate_start = sc.gates ? run.gates.front().state.value : 0;
    float coins_before = static_cast<float>(run.tally.coins_earned);
    int total_xp = 0;
    bool hero_fired = false;
    constexpr float dt = 1.0f / 60.0f;
    for (float t = 0.0f; t < sc.seconds; t += dt) {
        run.gate_timer = 999.0f;
        run.loot_timer = 999.0f;
        total_xp += run.xp;
        run.xp = 0;
        run.pending_levels = 0;
        run.picking = false;
        if (sc.gates && !run.gates.empty()) {
            run.gates.front().y = 300.0f;
            m.gate_value = run.gates.front().state.value - gate_start;
        }
        if (sc.airstrikes) trigger_airstrike(game);
        if (sc.hero_trigger > 0 && !hero_fired && t >= 12.0f) {
            hero_fired = true;
            if (sc.hero_trigger == 1) {
                run.salama_charge = 1.0f;
                trigger_salama(game);
            } else if (sc.hero_trigger == 2) {
                run.rage = 1.0f;
                trigger_frenzy(game);
            } else if (sc.hero_trigger == 3) {
                run.ali_charge = 1.0f;
                trigger_ali(game);
            }
        }
        update_run(game, dt);
        run.pending_decals.clear();
        if (run.phase != WavePhase::Fighting) run.phase = WavePhase::Fighting;
    }
    m.damage = run.tally.damage_dealt;
    m.kills = run.tally.kills;
    m.coins = run.tally.coins_earned - static_cast<int>(coins_before);
    m.wall_lost = start_wall - run.wall_hp;
    m.soldiers = run.soldiers;
    m.squad_types = static_cast<int>(run.squad.size());
    m.hero_charge = run.rage + run.salama_charge + run.ali_charge;
    m.xp_earned = total_xp + run.xp;
    return m;
}

Setup give_card(CardId id) {
    return [id](Game& game) {
        auto& run = *game.run;
        run.offer = {id, id, id};
        run.picking = true;
        run.pending_levels = 1;
        pick_card(game, 0);
    };
}

Setup give_cards(std::vector<CardId> ids) {
    return [ids](Game& game) {
        for (auto id : ids) give_card(id)(game);
    };
}

int failures = 0;

void report(const char* name, const char* metric, double base, double with, bool higher_is_better = true) {
    bool ok = higher_is_better ? with > base * 1.02 : with < base * 0.98;
    if (!ok) failures++;
    double ratio = base != 0.0 ? with / base : 0.0;
    std::printf("%-5s %-22s %-14s base %12.1f  with %12.1f  x%.2f\n", ok ? "PASS" : "FAIL", name, metric, base, with,
                ratio);
}

} // namespace

int run_probe() {
    Game game;
    game.persist = false;
    MetaProgress meta;
    meta.unlocked_stage = 5;
    Scenario normal;

    Metrics base = fight(game, meta, nullptr, nullptr, normal);
    std::printf("baseline: damage %.0f kills %d coins %d wall lost %.0f\n\n", base.damage, base.kills, base.coins,
                base.wall_lost);

    std::printf("== cards\n");
    struct DamageCard {
        CardId id;
        std::vector<CardId> prereq;
    };
    std::vector<DamageCard> damage_cards = {
        {CardId::Multishot, {}},
        {CardId::RapidFire, {}},
        {CardId::HeavyRounds, {}},
        {CardId::Piercing, {}},
        {CardId::Ricochet, {}},
        {CardId::Incendiary, {}},
        {CardId::Explosive, {}},
        {CardId::Critical, {}},
        {CardId::ChainLightning, {}},
        {CardId::BulletStorm,
         {CardId::Multishot, CardId::Multishot, CardId::Multishot, CardId::RapidFire, CardId::RapidFire,
          CardId::RapidFire}},
        {CardId::Inferno, {CardId::Incendiary, CardId::Incendiary, CardId::Explosive}},
        {CardId::ThunderGod, {CardId::ChainLightning, CardId::ChainLightning, CardId::Critical, CardId::Critical}},
        {CardId::Shatter, {CardId::Cryo, CardId::Cryo, CardId::Piercing, CardId::Piercing}},
        {CardId::Executioner, {}},
        {CardId::ShockwaveRounds, {}},
        {CardId::PhantomBarrage, {CardId::Ricochet, CardId::Ricochet, CardId::Piercing, CardId::Piercing}},
    };
    for (const auto& dc : damage_cards) {
        Metrics b = dc.prereq.empty() ? base : fight(game, meta, nullptr, give_cards(dc.prereq), normal);
        auto with_ids = dc.prereq;
        with_ids.push_back(dc.id);
        Metrics w = fight(game, meta, nullptr, give_cards(with_ids), normal);
        report(card_def(dc.id).name, "damage", b.damage, w.damage);
    }
    {
        Metrics w = fight(game, meta, nullptr, give_card(CardId::Cryo), normal);
        report("CRYO ROUNDS", "wall lost", base.wall_lost, w.wall_lost, false);
    }
    {
        Scenario air = normal;
        air.airstrikes = true;
        Metrics b = fight(game, meta, nullptr, nullptr, air);
        Metrics w = fight(game, meta, nullptr, give_card(CardId::Airstrike), air);
        report("BOMBER PILOT", "damage", b.damage, w.damage);
        Metrics ob =
            fight(game, meta, nullptr,
                  give_cards({CardId::Airstrike, CardId::Airstrike, CardId::Explosive, CardId::Explosive}), air);
        Metrics ow = fight(game, meta, nullptr,
                           give_cards({CardId::Airstrike, CardId::Airstrike, CardId::Explosive, CardId::Explosive,
                                       CardId::OrbitalStrike}),
                           air);
        report("ORBITAL FURY", "damage", ob.damage, ow.damage);
    }
    {
        Metrics w = fight(game, meta, nullptr, give_card(CardId::Reinforcements), normal);
        report("REINFORCEMENTS", "soldiers", base.soldiers, w.soldiers);
        report("REINFORCEMENTS", "damage", base.damage, w.damage);
    }
    {
        Metrics w = fight(game, meta, nullptr, give_card(CardId::Fortify), normal);
        report("FORTIFY", "wall max", base.wall_max, w.wall_max);
    }
    {
        Metrics w = fight(game, meta, nullptr, give_card(CardId::Plunder), normal);
        report("PLUNDER", "coins", base.coins, w.coins);
    }
    {
        Metrics w = fight(game, meta, nullptr, give_card(CardId::CallIn), normal);
        report("CALL IN", "unit types", base.squad_types, w.squad_types);
    }
    {
        Scenario gate = normal;
        gate.gates = true;
        gate.seconds = 4.0f;
        Metrics b = fight(game, meta, nullptr, nullptr, gate);
        Metrics w = fight(game, meta, nullptr, give_card(CardId::GateMaster), gate);
        report("GATE MASTER", "gate gain", b.gate_value, w.gate_value);
    }
    {
        auto with_cannon = [](Game& g) { g.run->turrets[1] = Turret{TurretType::Cannon, 2}; };
        Metrics b = fight(game, meta, nullptr, with_cannon, normal);
        Metrics w1 = fight(
            game, meta, nullptr,
            [&](Game& g) {
                with_cannon(g);
                give_card(CardId::Overclock)(g);
            },
            normal);
        Metrics w3 = fight(
            game, meta, nullptr,
            [&](Game& g) {
                with_cannon(g);
                give_cards({CardId::Overclock, CardId::Overclock, CardId::Overclock})(g);
            },
            normal);
        report("OVERCLOCK LV1", "damage", b.damage, w1.damage);
        report("OVERCLOCK LV3 vs LV1", "damage", w1.damage, w3.damage);

        auto aegis_pre = [&](Game& g) {
            with_cannon(g);
            give_cards({CardId::Fortify, CardId::Fortify, CardId::Overclock, CardId::Overclock})(g);
        };
        Metrics ab = fight(game, meta, nullptr, aegis_pre, normal);
        Metrics aw = fight(
            game, meta, nullptr,
            [&](Game& g) {
                aegis_pre(g);
                give_card(CardId::FortressOverdrive)(g);
            },
            normal);
        report("AEGIS WALL EVO", "damage", ab.damage, aw.damage);
        report("AEGIS WALL SHIELD", "wall lost", ab.wall_lost, aw.wall_lost, false);
    }
    {
        // FieldRepair heals the wall after kills (simulate starting wall damaged below max).
        auto dmg_wall_setup = [](Game& g) { g.run->wall_hp = g.run->stats.wall_max * 0.5f; };
        auto field_rep_fight = [&](int lvls) {
            game.meta = meta;
            game.request = {2, false};
            start_run(game);
            auto& run = *game.run;
            run.rng = Rng{424242};
            run.phase = WavePhase::Fighting;
            run.wave = 3;
            run.soldiers = 20;
            for (int i = 0; i < lvls; ++i) give_card(CardId::FieldRepair)(game);
            dmg_wall_setup(game);
            float start_hp = run.wall_hp;
            run.spawns.clear();
            for (int i = 0; i < 160; ++i)
                run.spawns.push_back({10.0f * static_cast<float>(i) / 160.0f, EnemyType::Walker, VIRTUAL_W * 0.5f});
            for (int step = 0; step < 60 * 14; ++step) {
                run.gate_timer = 999.0f;
                run.loot_timer = 999.0f;
                run.xp = 0;
                run.picking = false;
                update_run(game, 1.0f / 60.0f);
                run.pending_decals.clear();
            }
            return run.wall_hp - start_hp;
        };
        double b_heal = field_rep_fight(0);
        double w_heal = field_rep_fight(2);
        report("ENGINEERS (FIELD REP)", "wall healed", b_heal + 100.0, w_heal + 100.0);
    }
    {
        Scenario short_sc = normal;
        short_sc.seconds = 8.0f;
        Metrics b = fight(game, meta, nullptr, nullptr, short_sc);
        Metrics w = fight(game, meta, nullptr, give_cards({CardId::Adrenaline, CardId::Adrenaline}), short_sc);
        report("ADRENALINE", "hero charge", b.hero_charge, w.hero_charge);
    }
    {
        // Verify multi-level scaling on core cards (Lv3 > Lv1)
        Metrics h1 = fight(game, meta, nullptr, give_card(CardId::HeavyRounds), normal);
        Metrics h3 = fight(game, meta, nullptr,
                           give_cards({CardId::HeavyRounds, CardId::HeavyRounds, CardId::HeavyRounds}), normal);
        report("HEAVY ROUNDS LV3>LV1", "damage", h1.damage, h3.damage);
        Metrics s1 = fight(game, meta, nullptr, give_card(CardId::ShockwaveRounds), normal);
        Metrics s3 =
            fight(game, meta, nullptr,
                  give_cards({CardId::ShockwaveRounds, CardId::ShockwaveRounds, CardId::ShockwaveRounds}), normal);
        report("PULSE ROUNDS LV3>LV1", "damage", s1.damage, s3.damage);
    }

    std::printf("\n== armory upgrades\n");
    auto with_meta = [&](MetaId id, int level) {
        MetaProgress m = meta;
        m.levels[static_cast<size_t>(id)] = level;
        return m;
    };
    {
        Metrics w = fight(game, with_meta(MetaId::Recruits, 5), nullptr, nullptr, normal);
        report("RECRUITS", "soldiers", base.soldiers, w.soldiers);
    }
    {
        Metrics w = fight(game, with_meta(MetaId::Firepower, 5), nullptr, nullptr, normal);
        report("FIREPOWER", "damage", base.damage, w.damage);
    }
    {
        Metrics w = fight(game, with_meta(MetaId::RateOfFire, 5), nullptr, nullptr, normal);
        report("TRIGGER DRILL", "damage", base.damage, w.damage);
    }
    {
        Metrics w = fight(game, with_meta(MetaId::Fortress, 5), nullptr, nullptr, normal);
        report("FORTRESS", "wall max", base.wall_max, w.wall_max);
        report("FORTRESS THORNS", "damage", base.damage, w.damage);
    }
    {
        Metrics w = fight(game, with_meta(MetaId::WarChest, 3), nullptr, nullptr, normal);
        report("WAR CHEST", "start coins", base.start_coins, w.start_coins);
    }
    {
        Metrics w = fight(game, with_meta(MetaId::Lucky, 2), nullptr, nullptr, normal);
        report("LUCKY CHARM", "rerolls", base.rerolls + 1.0, w.rerolls + 1.0);
    }
    {
        auto with_cannon = [](Game& g) { g.run->turrets[1] = Turret{TurretType::Cannon, 2}; };
        Metrics b = fight(game, meta, nullptr, with_cannon, normal);
        Metrics w = fight(game, with_meta(MetaId::TurretTech, 4), nullptr, with_cannon, normal);
        report("TURRET TECH", "damage", b.damage, w.damage);
    }
    {
        Scenario short_sc = normal;
        short_sc.seconds = 8.0f;
        Metrics b = fight(game, meta, nullptr, nullptr, short_sc);
        Metrics w = fight(game, with_meta(MetaId::Commander, 4), nullptr, nullptr, short_sc);
        report("COMMANDER XP", "xp", b.xp_earned, w.xp_earned);
        report("COMMANDER CHARGE", "hero charge", b.hero_charge, w.hero_charge);
    }

    std::printf("\n== family hero powers & formations\n");
    {
        Scenario s_sc = normal;
        s_sc.hero_trigger = 1;
        Metrics s_on = fight(game, meta, nullptr, nullptr, s_sc);
        report("SALAMA BABA BOOM", "damage", base.damage, s_on.damage);

        Scenario o_sc = normal;
        o_sc.hero_trigger = 2;
        Metrics o_on = fight(game, meta, nullptr, nullptr, o_sc);
        report("OMAR YALLA RALLY", "damage", base.damage, o_on.damage);

        Scenario a_sc = normal;
        a_sc.hero_trigger = 3;
        Metrics a_on = fight(game, meta, nullptr, nullptr, a_sc);
        report("ALI PEACE CHARM", "wall lost", base.wall_lost, a_on.wall_lost, false);

        Metrics wedge = fight(game, meta, nullptr, [](Game& g) { g.run->formation = Formation::Wedge; }, normal);
        Metrics guard = fight(game, meta, nullptr, [](Game& g) { g.run->formation = Formation::Guard; }, normal);
        report("FORMATION WEDGE", "damage", base.damage, wedge.damage);
        report("FORMATION GUARD", "wall lost", base.wall_lost, guard.wall_lost, false);
    }

    std::printf("\n== units (level 1 vs level 6, alone in the squad)\n");
    for (int u = 0; u < UNIT_COUNT; ++u) {
        auto solo = [u](int level) {
            MetaProgress m;
            m.unlocked_stage = 6;
            m.unit_levels[static_cast<size_t>(u)] = level;
            m.loadout = {u, -1, -1, -1};
            return m;
        };
        Metrics l1 = fight(game, solo(1), nullptr, nullptr, normal);
        Metrics l6 = fight(game, solo(6), nullptr, nullptr, normal);
        report(unit_def(static_cast<UnitType>(u)).name, "damage", l1.damage, l6.damage);
    }

    std::printf("\n== turrets (none vs lv1 vs lv3 vs lv5 in slot 2)\n");
    for (int t = 1; t <= TURRET_KINDS; ++t) {
        auto turret = [t](int level) {
            return [t, level](Game& game) { game.run->turrets[1] = Turret{static_cast<TurretType>(t), level}; };
        };
        Metrics l1 = fight(game, meta, nullptr, turret(1), normal);
        Metrics l3 = fight(game, meta, nullptr, turret(3), normal);
        Metrics l5 = fight(game, meta, nullptr, turret(5), normal);
        report(turret_def(static_cast<TurretType>(t)).name, "lv1 vs none", base.damage, l1.damage);
        report(turret_def(static_cast<TurretType>(t)).name, "lv3 vs lv1", l1.damage, l3.damage);
        report(turret_def(static_cast<TurretType>(t)).name, "lv5 vs lv3", l3.damage, l5.damage);
    }

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

} // namespace ls
