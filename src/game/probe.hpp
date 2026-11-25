#pragma once

namespace ls {

// LS_PROBE=1: run every card, armory upgrade, unit and turret through the same seeded fight
// and report whether each one changes the outcome it is meant to. Exits non-zero on failure.
int run_probe();

} // namespace ls
