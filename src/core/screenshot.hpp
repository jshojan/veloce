#pragma once

#include <cstdint>
#include <string>
#include <filesystem>

namespace emu {

// Screenshot utility for saving framebuffer to PNG files
class Screenshot {
public:
    // Save RGBA framebuffer to PNG file
    // Returns true on success
    static bool save_png(const std::filesystem::path& path,
                         const uint32_t* pixels,
                         int width, int height);

    // Save RGBA framebuffer to BMP file (fallback, no compression)
    static bool save_bmp(const std::filesystem::path& path,
                         const uint32_t* pixels,
                         int width, int height);

    // Generate a timestamped filename for screenshots
    static std::string generate_filename(const std::string& prefix = "screenshot");
};

} // namespace emu
