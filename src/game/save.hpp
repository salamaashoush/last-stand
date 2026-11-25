#pragma once
#include "game/meta.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>

namespace ls {

inline constexpr int SAVE_VERSION = 1;

template <typename T, size_t N>
void read_array(const nlohmann::json& j, const char* key, std::array<T, N>& out) {
    if (!j.contains(key) || !j[key].is_array()) return;
    const auto& arr = j[key];
    for (size_t i = 0; i < N && i < arr.size(); ++i) out[i] = arr[i].get<T>();
}

inline std::string serialize_meta(const MetaProgress& m) {
    nlohmann::json j;
    j["version"] = SAVE_VERSION;
    j["gems"] = m.gems;
    j["unlocked_stage"] = m.unlocked_stage;
    j["levels"] = m.levels;
    j["stars"] = m.stars;
    j["unit_levels"] = m.unit_levels;
    j["loadout"] = m.loadout;
    j["lifetime"] = m.lifetime;
    j["missions_claimed"] = m.missions_claimed;
    j["last_daily_day"] = m.last_daily_day;
    j["daily_streak"] = m.daily_streak;
    j["music_volume"] = m.music_volume;
    j["sfx_volume"] = m.sfx_volume;
    j["tutorial_done"] = m.tutorial_done;
    return j.dump();
}

// Unknown or missing fields fall back to defaults so older saves keep loading.
inline MetaProgress deserialize_meta(const std::string& text) {
    MetaProgress m;
    auto j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return m;
    m.gems = std::max(0, j.value("gems", 0));
    m.unlocked_stage = std::clamp(j.value("unlocked_stage", 1), 1, MAX_STAGE);
    read_array(j, "levels", m.levels);
    read_array(j, "stars", m.stars);
    read_array(j, "unit_levels", m.unit_levels);
    for (auto& l : m.unit_levels) l = std::clamp(l, 0, UNIT_MAX_LEVEL);
    m.unit_levels[0] = std::max(1, m.unit_levels[0]);
    read_array(j, "loadout", m.loadout);
    for (auto& slot : m.loadout) {
        if (slot < -1 || slot >= UNIT_COUNT || (slot >= 0 && m.unit_levels[static_cast<size_t>(slot)] == 0)) slot = -1;
    }
    if (std::ranges::all_of(m.loadout, [](int slot) { return slot < 0; })) m.loadout[0] = 0;
    read_array(j, "lifetime", m.lifetime);
    read_array(j, "missions_claimed", m.missions_claimed);
    m.last_daily_day = j.value("last_daily_day", -1LL);
    m.daily_streak = j.value("daily_streak", 0);
    m.music_volume = std::clamp(j.value("music_volume", 0.5f), 0.0f, 1.0f);
    m.sfx_volume = std::clamp(j.value("sfx_volume", 0.8f), 0.0f, 1.0f);
    m.tutorial_done = j.value("tutorial_done", false);
    for (size_t i = 0; i < m.levels.size(); ++i) {
        m.levels[i] = std::clamp(m.levels[i], 0, META_DEFS[i].max_level);
    }
    return m;
}

} // namespace ls
