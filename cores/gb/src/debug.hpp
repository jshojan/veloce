#pragma once

#include <cstdlib>

namespace gb {

// Check if debug mode is enabled via environment variable
inline bool is_debug_mode() {
    static bool checked = false;
    static bool enabled = false;

    if (!checked) {
        const char* env = std::getenv("DEBUG");
        enabled = env && (env[0] == '1' || env[0] == 'y' || env[0] == 'Y');
        checked = true;
    }

    return enabled;
}

} // namespace gb
