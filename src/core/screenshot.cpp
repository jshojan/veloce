#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../external/stb_image_write.h"

#include "screenshot.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <vector>
#include <iostream>

namespace emu {

bool Screenshot::save_png(const std::filesystem::path& path,
                          const uint32_t* pixels,
                          int width, int height) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    // Ensure parent directory exists
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    // stb_image_write expects RGBA in top-to-bottom order
    // Our framebuffer may be in BGRA format, so convert
    std::vector<uint8_t> rgba_data(width * height * 4);

    for (int i = 0; i < width * height; i++) {
        uint32_t pixel = pixels[i];
        // Convert from ARGB/BGRA to RGBA
        rgba_data[i * 4 + 0] = (pixel >> 16) & 0xFF; // R
        rgba_data[i * 4 + 1] = (pixel >> 8) & 0xFF;  // G
        rgba_data[i * 4 + 2] = pixel & 0xFF;         // B
        rgba_data[i * 4 + 3] = (pixel >> 24) & 0xFF; // A
    }

    int result = stbi_write_png(path.string().c_str(),
                                width, height,
                                4, // RGBA
                                rgba_data.data(),
                                width * 4); // stride

    if (result) {
        std::cout << "[Screenshot] Saved: " << path << std::endl;
    } else {
        std::cerr << "[Screenshot] Failed to save: " << path << std::endl;
    }

    return result != 0;
}

bool Screenshot::save_bmp(const std::filesystem::path& path,
                          const uint32_t* pixels,
                          int width, int height) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    // Ensure parent directory exists
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    // Convert to RGBA for stb
    std::vector<uint8_t> rgba_data(width * height * 4);

    for (int i = 0; i < width * height; i++) {
        uint32_t pixel = pixels[i];
        rgba_data[i * 4 + 0] = (pixel >> 16) & 0xFF; // R
        rgba_data[i * 4 + 1] = (pixel >> 8) & 0xFF;  // G
        rgba_data[i * 4 + 2] = pixel & 0xFF;         // B
        rgba_data[i * 4 + 3] = (pixel >> 24) & 0xFF; // A
    }

    int result = stbi_write_bmp(path.string().c_str(),
                                width, height,
                                4, // RGBA
                                rgba_data.data());

    return result != 0;
}

std::string Screenshot::generate_filename(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << prefix << "_"
        << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
        << "_" << std::setfill('0') << std::setw(3) << ms.count()
        << ".png";

    return oss.str();
}

} // namespace emu
