#include "managers/storage.hpp"
#include <fstream>
#include <sstream>

#ifdef __EMSCRIPTEN__
#include <cstdlib>
#include <emscripten/emscripten.h>

// clang-format off
EM_JS(char*, ls_storage_get, (const char* key), {
    var v = null;
    try { v = window.localStorage.getItem(UTF8ToString(key)); } catch (e) { v = null; }
    if (v === null) return 0;
    var len = lengthBytesUTF8(v) + 1;
    var ptr = _malloc(len);
    stringToUTF8(v, ptr, len);
    return ptr;
});

EM_JS(int, ls_storage_set, (const char* key, const char* value), {
    try { window.localStorage.setItem(UTF8ToString(key), UTF8ToString(value)); return 1; } catch (e) { return 0; }
});
// clang-format on
#endif

namespace ls {

std::optional<std::string> storage_read(const std::string& key) {
#ifdef __EMSCRIPTEN__
    char* raw = ls_storage_get(key.c_str());
    if (raw == nullptr) return std::nullopt;
    std::string out{raw};
    std::free(raw);
    return out;
#else
    std::ifstream f(key + ".json");
    if (!f.is_open()) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
#endif
}

bool storage_write(const std::string& key, const std::string& data) {
#ifdef __EMSCRIPTEN__
    return ls_storage_set(key.c_str(), data.c_str()) != 0;
#else
    std::ofstream f(key + ".json");
    if (!f.is_open()) return false;
    f << data;
    return f.good();
#endif
}

} // namespace ls
