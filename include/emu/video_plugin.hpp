#pragma once

#include "plugin_types.hpp"
#include <cstdint>
#include <cstddef>

#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_VIDEO_PLUGIN_API_VERSION 1

namespace emu {

// Video plugin information
struct VideoPluginInfo {
    const char* name;           // "OpenGL Software", "Vulkan HLE", etc.
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description
    uint32_t capabilities;      // VideoCapabilities flags
    int max_internal_resolution; // Maximum upscale factor (1 = native)
};

// Shader/filter information
struct ShaderInfo {
    const char* name;           // "CRT Royale", "xBRZ 4x", etc.
    const char* description;    // Brief description
    int parameter_count;        // Number of adjustable parameters
};

// Shader parameter information
struct ShaderParameter {
    const char* name;           // "Scanline Intensity"
    const char* description;    // What the parameter does
    float min_value;
    float max_value;
    float default_value;
    float current_value;
};

// Host interface provided to video plugins
class IVideoHost {
public:
    virtual ~IVideoHost() = default;

    // Window management
    virtual void* get_native_window_handle() = 0;
    virtual void get_window_size(int& width, int& height) = 0;

    // OpenGL context (for plugins that use OpenGL)
    virtual void* get_gl_context() = 0;
    virtual void make_gl_context_current() = 0;
    virtual void swap_buffers() = 0;

    // ImGui integration (optional - may return nullptr)
    virtual void* get_imgui_context() = 0;

    // VSync control
    virtual void set_vsync(bool enabled) = 0;
    virtual bool get_vsync() const = 0;
};

// Video plugin interface
class IVideoPlugin {
public:
    virtual ~IVideoPlugin() = default;

    // Get plugin info
    virtual VideoPluginInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(IVideoHost* host) = 0;
    virtual void shutdown() = 0;

    // Framebuffer upload from emulator
    // pixels: RGBA8888 format, row-major order
    virtual void upload_framebuffer(const uint32_t* pixels, int width, int height) = 0;

    // Get the current texture dimensions (may differ from source if upscaled)
    virtual void get_output_size(int& width, int& height) const = 0;

    // Frame rendering
    virtual void begin_frame() = 0;
    virtual void render_game(int x, int y, int width, int height) = 0;
    virtual void end_frame() = 0;

    // Clear screen
    virtual void clear(float r, float g, float b, float a = 1.0f) = 0;

    // Get texture ID for ImGui rendering (OpenGL texture ID)
    virtual uint32_t get_texture_id() const = 0;

    // Shader/filter support
    virtual int get_shader_count() const { return 0; }
    virtual ShaderInfo get_shader_info(int index) const { return {}; }
    virtual int get_active_shader() const { return -1; }
    virtual void set_active_shader(int index) {}

    // Shader parameters
    virtual int get_shader_parameter_count(int shader_index) const { return 0; }
    virtual ShaderParameter get_shader_parameter(int shader_index, int param_index) const { return {}; }
    virtual void set_shader_parameter(int shader_index, int param_index, float value) {}

    // Internal resolution scaling
    virtual int get_internal_resolution() const { return 1; }
    virtual void set_internal_resolution(int scale) {}

    // Screenshot capture
    virtual bool save_screenshot(const char* path) { return false; }

    // Video recording (optional)
    virtual bool start_recording(const char* path, int fps = 60) { return false; }
    virtual void stop_recording() {}
    virtual bool is_recording() const { return false; }

    // Fullscreen support
    virtual bool is_fullscreen() const { return false; }
    virtual void set_fullscreen(bool enabled) {}
    virtual void toggle_fullscreen() { set_fullscreen(!is_fullscreen()); }

    // Aspect ratio
    virtual float get_aspect_ratio() const { return 4.0f / 3.0f; }
    virtual void set_aspect_ratio(float ratio) {}
    virtual bool get_maintain_aspect_ratio() const { return true; }
    virtual void set_maintain_aspect_ratio(bool maintain) {}

    // Integer scaling (for pixel-perfect rendering)
    virtual bool get_integer_scaling() const { return false; }
    virtual void set_integer_scaling(bool enabled) {}
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::IVideoPlugin* create_video_plugin();
    EMU_PLUGIN_EXPORT void destroy_video_plugin(emu::IVideoPlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_video_plugin_api_version();
}
