#pragma once
#include <cstdlib>   // std::getenv
#include <cctype>    // std::tolower, std::isspace
#include <string>    // std::string
#include <string_view>

namespace Masstree {

// Case-insensitive equality without lambdas.
inline bool iequals_cs(std::string_view sv, std::string_view t) {
    if (sv.size() != t.size()) return false;
    for (size_t i = 0; i < t.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(sv[i]);
        unsigned char b = static_cast<unsigned char>(t[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

// Trim helpers (no lambdas)
inline void trim_left(std::string_view& sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
}
inline void trim_right(std::string_view& sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
}

// Return true iff env var `name` is "1", "true", "on", or "yes" (case-insensitive).
inline bool env_on(const char* name) {
    const char* s = std::getenv(name);
    if (!s) return false;

    std::string_view sv{s};
    trim_left(sv);
    trim_right(sv);
    if (sv.empty()) return false;

    // Fast path: single char '1'
    if (sv.size() == 1 && sv[0] == '1') return true;

    // Case-insensitive checks (no lambdas)
    return iequals_cs(sv, "true") || iequals_cs(sv, "on") || iequals_cs(sv, "yes");
}

// Cached runtime flags (evaluated once per process)
inline bool learned_env_enabled() {
    static const bool v = env_on("MAKO_LEARNED_INDEX");
    return v;
}
inline bool learned_debug_enabled() {
    static const bool v = env_on("LEARNED_DEBUG");
    return v;
}

} // namespace Masstree

