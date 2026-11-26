#include "game/autopilot.hpp"
#include "core/game.hpp"
#include "game/sim.hpp"
#include "game/turrets.hpp"
#include <algorithm>
#include <cstdlib>

namespace ls {

bool autopilot_enabled() {
    static const bool on = std::getenv("LS_AUTOPILOT") != nullptr || std::getenv("LS_HEADLESS") != nullptr;
    return on;
}

int autopilot_speed() {
    static const int speed = std::getenv("LS_SPEED") ? std::max(1, std::atoi(std::getenv("LS_SPEED"))) : 1;
    return speed;
}

int autopilot_start_stage() {
    static const int stage = std::getenv("LS_STAGE") ? std::max(1, std::atoi(std::getenv("LS_STAGE"))) : 1;
    return stage;
}

void autopilot_update(Game& game, float dt) {
    static float pick_timer = 0.0f;
    auto& run = *game.run;
    if (run.picking) {
        pick_timer += dt;
        if (pick_timer > 0.6f) {
            pick_timer = 0.0f;
            int best = 0;
            for (int i = 0; i < 3; ++i) {
                if (card_def(run.offer[static_cast<size_t>(i)]).rarity >
                    card_def(run.offer[static_cast<size_t>(best)]).rarity)
                    best = i;
            }
            pick_card(game, best);
        }
        return;
    }

    // Pick the lowest gate row and stand under the gate that will be best when it lands,
    // counting the growth we can still shoot into it on the way down.
    int row = -1;
    float row_y = -1e9f;
    for (const auto& g : run.gates) {
        if (g.y > row_y) {
            row_y = g.y;
            row = g.row;
        }
    }
    const Gate* target = nullptr;
    if (row != -1) {
        float best = -1e9f;
        float sps = static_cast<float>(std::min(run.soldiers, 40)) * run.stats.fire_rate *
                    static_cast<float>(run.stats.multishot);
        int hpp = hits_per_point(sps);
        float time_left = std::max(0.0f, (WALL_TOP - row_y) / GATE_SPEED);
        for (const auto& g : run.gates) {
            if (g.row != row) continue;
            int shots = static_cast<int>(sps * time_left * 0.5f);
            GateState future = gate_after_hits(g.state, shots, hpp, run.stats.gate_growth, run.soldiers);
            float score = static_cast<float>(apply_gate(run.soldiers, future));
            if (gate_is_stat(future.kind)) {
                score = static_cast<float>(run.soldiers) * (1.0f + static_cast<float>(future.value) / 100.0f);
            }
            if (score > best) {
                best = score;
                target = &g;
            }
        }
    }
    // Urgency: anything close to the wall outranks gate farming, like a real player would.
    float threat_x = 0.0f;
    float threat_w = 0.0f;
    float urgent_x = 0.0f;
    float urgent_w = 0.0f;
    for (const auto& e : run.enemies) {
        if (e.dead || e.pos.y < 400.0f) continue;
        float w =
            (e.pos.y - 400.0f) * (e.type == EnemyType::Boss ? 5.0f : 1.0f) * (e.hp / std::max(1.0f, run.stats.damage));
        threat_x += e.pos.x * w;
        threat_w += w;
        if (e.pos.y > WALL_TOP - 220.0f) {
            urgent_x += e.pos.x * w;
            urgent_w += w;
        }
    }
    float urgency = urgent_w / std::max(1.0f, run.stats.damage * 60.0f);
    const Loot* ice = nullptr;
    for (const auto& f : run.loot) {
        if (f.pos.y > 200.0f && (!ice || f.pos.y > ice->pos.y)) ice = &f;
    }
    float gate_eta = target ? (WALL_TOP - target->y) / GATE_SPEED : 1e9f;
    if (urgent_w > 0.0f && (urgency > 1.0f || (!target && !ice))) {
        run.squad_target_x = urgent_x / urgent_w;
    } else if (ice && gate_eta > 3.0f) {
        run.squad_target_x = ice->pos.x;
    } else if (target) {
        run.squad_target_x = target->x;
    } else if (threat_w > 0.0f) {
        run.squad_target_x = threat_x / threat_w;
    }

    // Smart tactical formation switching: Wedge against bosses, Guard when wall is low, Line otherwise.
    Formation want_form = run.wall_hp < run.stats.wall_max * 0.45f ? Formation::Guard
                          : run.boss_alive()                       ? Formation::Wedge
                                                                   : Formation::Line;
    if (run.formation != want_form) run.formation = want_form;

    trigger_airstrike(game);
    trigger_frenzy(game);
    if (!run.enemies.empty()) trigger_salama(game);
    if (run.wall_hp < run.stats.wall_max * 0.9f) trigger_ali(game);
    if (run.wall_hp < run.stats.wall_max * 0.75f) repair_wall(game);

    static const TurretType order[TURRET_SLOTS] = {TurretType::Cannon, TurretType::Frost, TurretType::Tesla,
                                                   TurretType::Missile};
    static const TurretType fallback[TURRET_SLOTS] = {TurretType::Cannon, TurretType::Flamer, TurretType::Cannon,
                                                      TurretType::Flamer};
    for (int s = 0; s < TURRET_SLOTS; ++s) {
        auto& t = run.turrets[static_cast<size_t>(s)];
        TurretType pref = turret_unlocked(order[s], run.unlocked_stage) ? order[s]
                          : turret_unlocked(TurretType::Laser, run.unlocked_stage) && s == 3 ? TurretType::Laser
                                                                                             : fallback[s];
        TurretType type = t.type == TurretType::None ? pref : t.type;
        if (t.level < TURRET_MAX_LEVEL &&
            run.coins >= turret_upgrade_cost(type, t.level, run.turret_cost_mult) + 20) {
            build_or_upgrade(game, s, type);
            break;
        }
    }
}

} // namespace ls

namespace ls {

void autopilot_spend(Game& game) {
    auto& m = game.meta;
    static const MetaId priority[] = {MetaId::Firepower, MetaId::RateOfFire, MetaId::Recruits,  MetaId::TurretTech,
                                      MetaId::Commander, MetaId::Fortress,   MetaId::WarChest, MetaId::Lucky};
    bool bought = true;
    for (int u = 1; u < UNIT_COUNT; ++u) {
        if (m.buy_unit(static_cast<UnitType>(u))) break;
    }
    for (auto u : m.squad()) m.upgrade_unit(u);
    while (bought) {
        bought = false;
        for (auto id : priority) {
            if (m.buy(id)) {
                bought = true;
                break;
            }
        }
    }
    game.save();
}

void autopilot_log_wave(const Game& game) {
    if (!autopilot_enabled()) return;
    const auto& r = *game.run;
    int turrets = 0;
    for (const auto& t : r.turrets) turrets += t.level;
    TraceLog(LOG_INFO,
             "AP wave s=%d w=%d t=%.0f sold=%d lvl=%d wall=%.0f%% coins=%d tur=%d threat=%.2f mod=%d dmg=%.1f "
             "rate=%.2f multi=%d",
             r.stage, r.wave, r.tally.time, r.soldiers, r.level, 100.0f * r.wall_hp / r.stats.wall_max, r.coins,
             turrets, r.threat, static_cast<int>(r.mod), r.stats.damage, r.stats.fire_rate, r.stats.multishot);
}

void autopilot_log_result(const Game& game) {
    if (!autopilot_enabled()) return;
    const auto& r = game.result;
    TraceLog(LOG_INFO,
             "AP result stage=%d won=%d waves=%d stars=%d gems=%d kills=%d max_soldiers=%d bosses=%d total_gems=%d",
             r.stage, r.won ? 1 : 0, r.waves_cleared, r.stars, r.gems, r.tally.kills, r.tally.max_soldiers,
             r.tally.bosses, game.meta.gems);
}

} // namespace ls

namespace ls {

int run_headless(int runs) {
    Game game;
    game.persist = false;
    int stage = autopilot_start_stage();
    MetaProgress start_meta = game.meta;
    for (int i = 0; i < runs; ++i) {
        if (std::getenv("LS_REPEAT")) game.meta = start_meta;
        game.request = {stage, false};
        start_run(game);
        constexpr float dt = 1.0f / 60.0f;
        int steps = 0;
        while (!game.run->finished && steps < 60 * 60 * 30) {
            autopilot_update(game, dt);
            update_run(game, dt);
            game.run->pending_decals.clear();
            steps++;
        }
        autopilot_log_result(game);
        if (!std::getenv("LS_NOSPEND")) autopilot_spend(game);
        if (game.result.won && !std::getenv("LS_REPEAT")) stage = std::min(stage + 1, MAX_STAGE);
    }
    return 0;
}

} // namespace ls
