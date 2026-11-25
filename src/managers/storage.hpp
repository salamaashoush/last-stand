#pragma once
#include <optional>
#include <string>

namespace ls {

// Desktop builds write a file next to the working directory; the browser build uses
// localStorage, because Emscripten's in-memory filesystem is wiped on every page load.
std::optional<std::string> storage_read(const std::string& key);
bool storage_write(const std::string& key, const std::string& data);

} // namespace ls
