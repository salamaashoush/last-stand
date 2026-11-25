#pragma once

namespace ls {

struct Game;

// Drives a run the way a decent player would: chases the best gate, picks cards,
// builds and upgrades turrets, fires airstrikes. Enabled by LS_AUTOPILOT=1 and used to
// screenshot and balance-test whole stages without a human at the controls.
bool autopilot_enabled();
void autopilot_update(Game& game, float dt);
int autopilot_speed();
int autopilot_start_stage();
void autopilot_spend(Game& game);
void autopilot_log_wave(const Game& game);
void autopilot_log_result(const Game& game);

// LS_HEADLESS=<runs>: simulate runs back to back with no window, printing balance logs.
int run_headless(int runs);

} // namespace ls
