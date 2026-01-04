#include "emu/emulator_plugin.hpp"
#include "types.hpp"
#include "lr35902.hpp"
#include "bus.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include "debug.hpp"

#include <cstring>
#include <cstdio>
#include <iostream>
#include <memory>
#include <fstream>
#include <filesystem>

// ImGui for configuration GUI
#include <imgui.h>

namespace gb {

// Pre-defined DMG color palettes (format: 0xAABBGGRR for ABGR)
struct PalettePreset {
    const char* name;
    uint32_t colors[4];  // Lightest to darkest
};

// Color conversion helpers
static uint32_t rgb_to_abgr(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

// Classic and popular DMG palette presets
static const PalettePreset s_palette_presets[] = {
    // Original DMG green (default)
    {"DMG Green (Classic)", {
        rgb_to_abgr(155, 188, 15),   // Lightest
        rgb_to_abgr(139, 172, 15),   // Light
        rgb_to_abgr(48, 98, 48),     // Dark
        rgb_to_abgr(15, 56, 15)      // Darkest
    }},
    // Game Boy Pocket / Light
    {"GB Pocket (Gray)", {
        rgb_to_abgr(255, 255, 255),
        rgb_to_abgr(170, 170, 170),
        rgb_to_abgr(85, 85, 85),
        rgb_to_abgr(0, 0, 0)
    }},
    // Black and White
    {"Pure Grayscale", {
        rgb_to_abgr(224, 224, 224),
        rgb_to_abgr(160, 160, 160),
        rgb_to_abgr(96, 96, 96),
        rgb_to_abgr(32, 32, 32)
    }},
    // Virtual Boy inspired red
    {"Virtual Boy Red", {
        rgb_to_abgr(255, 0, 0),
        rgb_to_abgr(192, 0, 0),
        rgb_to_abgr(96, 0, 0),
        rgb_to_abgr(32, 0, 0)
    }},
    // Super Game Boy inspired brown
    {"SGB Brown", {
        rgb_to_abgr(248, 224, 136),
        rgb_to_abgr(200, 168, 80),
        rgb_to_abgr(112, 88, 40),
        rgb_to_abgr(40, 32, 16)
    }},
    // BGB emulator default
    {"BGB Style", {
        rgb_to_abgr(224, 248, 208),
        rgb_to_abgr(136, 192, 112),
        rgb_to_abgr(52, 104, 86),
        rgb_to_abgr(8, 24, 32)
    }},
    // Inverted (negative)
    {"Inverted", {
        rgb_to_abgr(15, 56, 15),
        rgb_to_abgr(48, 98, 48),
        rgb_to_abgr(139, 172, 15),
        rgb_to_abgr(155, 188, 15)
    }},
    // Blue theme
    {"Ice Blue", {
        rgb_to_abgr(200, 220, 255),
        rgb_to_abgr(130, 160, 220),
        rgb_to_abgr(60, 90, 150),
        rgb_to_abgr(20, 40, 80)
    }},
    // Sepia/warm
    {"Sepia", {
        rgb_to_abgr(255, 245, 220),
        rgb_to_abgr(200, 170, 120),
        rgb_to_abgr(130, 90, 50),
        rgb_to_abgr(50, 30, 10)
    }}
};

static constexpr int NUM_PALETTE_PRESETS = sizeof(s_palette_presets) / sizeof(s_palette_presets[0]);

// Game Boy Controller button layout
static const emu::ButtonLayout GB_BUTTONS[] = {
    // D-pad (left side)
    {emu::VirtualButton::Up,     "Up",     0.15f, 0.35f, 0.10f, 0.14f, true},
    {emu::VirtualButton::Down,   "Down",   0.15f, 0.60f, 0.10f, 0.14f, true},
    {emu::VirtualButton::Left,   "Left",   0.06f, 0.47f, 0.10f, 0.14f, true},
    {emu::VirtualButton::Right,  "Right",  0.24f, 0.47f, 0.10f, 0.14f, true},
    // Select/Start (center)
    {emu::VirtualButton::Select, "SELECT", 0.35f, 0.80f, 0.12f, 0.06f, false},
    {emu::VirtualButton::Start,  "START",  0.53f, 0.80f, 0.12f, 0.06f, false},
    // B/A buttons (right side)
    {emu::VirtualButton::B,      "B",      0.70f, 0.52f, 0.12f, 0.16f, false},
    {emu::VirtualButton::A,      "A",      0.85f, 0.40f, 0.12f, 0.16f, false}
};

static const emu::ControllerLayoutInfo GB_CONTROLLER_LAYOUT = {
    "GB",
    "Game Boy",
    emu::ControllerShape::Handheld,
    0.65f,  // Portrait handheld form factor
    GB_BUTTONS,
    8,      // 8 buttons (D-pad, A, B, Start, Select)
    1       // Single player
};


class GBPlugin : public emu::IEmulatorPlugin {
public:
    GBPlugin();
    ~GBPlugin() override;

    // Plugin info
    emu::EmulatorInfo get_info() override;
    const emu::ControllerLayoutInfo* get_controller_layout() override;

    // ROM management
    bool load_rom(const uint8_t* data, size_t size) override;
    void unload_rom() override;
    bool is_rom_loaded() const override;
    uint32_t get_rom_crc32() const override;

    // Emulation
    void reset() override;
    void run_frame(const emu::InputState& input) override;
    uint64_t get_cycle_count() const override;
    uint64_t get_frame_count() const override;

    // Video
    emu::FrameBuffer get_framebuffer() override;

    // Audio
    emu::AudioBuffer get_audio() override;
    void clear_audio_buffer() override;

    // Streaming audio (low-latency)
    void set_audio_callback(AudioStreamCallback callback) override;

    // Memory access
    uint8_t read_memory(uint16_t address) override;
    void write_memory(uint16_t address, uint8_t value) override;

    // Save states
    bool save_state(std::vector<uint8_t>& data) override;
    bool load_state(const std::vector<uint8_t>& data) override;

    // Battery-backed save support
    bool has_battery_save() const override;
    std::vector<uint8_t> get_battery_save_data() const override;
    bool set_battery_save_data(const std::vector<uint8_t>& data) override;

    // Configuration GUI
    bool has_config_gui() const override { return true; }
    void set_imgui_context(void* context) override { ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context)); }
    void render_config_gui(bool& visible) override;
    void render_config_gui_content() override;
    const char* get_config_window_name() const override { return "Game Boy Settings"; }

    // Fast mode support
    bool is_fast_mode_enabled() const override { return m_fast_mode; }

    // Configuration persistence
    bool save_config(const char* path) override;
    bool load_config(const char* path) override;

private:
    void run_gb_frame(const emu::InputState& input);

    // Components
    std::unique_ptr<LR35902> m_cpu;
    std::unique_ptr<Bus> m_bus;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Cartridge> m_cartridge;

    SystemType m_system_type = SystemType::GameBoy;
    bool m_rom_loaded = false;
    uint32_t m_rom_crc32 = 0;
    uint64_t m_total_cycles = 0;
    uint64_t m_frame_count = 0;

    // Framebuffer - GB is 160x144
    static constexpr int SCREEN_WIDTH = 160;
    static constexpr int SCREEN_HEIGHT = 144;
    uint32_t m_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

    // Audio buffer
    static constexpr size_t AUDIO_BUFFER_SIZE = 2048;
    float m_audio_buffer[AUDIO_BUFFER_SIZE * 2];  // Stereo
    size_t m_audio_samples = 0;

    // Test ROM result tracking (for DEBUG mode)
    bool m_test_result_reported = false;

    // Configuration state
    int m_selected_palette = 0;      // Currently selected palette preset
    bool m_use_custom_palette = false;  // True if using custom colors
    uint32_t m_custom_palette[4];    // Custom palette colors (ABGR format)
    bool m_fast_mode = false;        // Run at uncapped speed when true

    // File extensions
    static const char* s_extensions[];
};

const char* GBPlugin::s_extensions[] = { ".gb", ".GB", ".gbc", ".GBC", nullptr };

GBPlugin::GBPlugin() {
    std::memset(m_framebuffer, 0, sizeof(m_framebuffer));
    std::memset(m_audio_buffer, 0, sizeof(m_audio_buffer));

    // Initialize custom palette with default values
    std::memcpy(m_custom_palette, s_palette_presets[0].colors, sizeof(m_custom_palette));

    m_cartridge = std::make_unique<Cartridge>();
    m_apu = std::make_unique<APU>();
}

GBPlugin::~GBPlugin() = default;

emu::EmulatorInfo GBPlugin::get_info() {
    emu::EmulatorInfo info;

    if (m_system_type == SystemType::GameBoyColor) {
        info.name = "GBC";
        info.version = "0.1.0";
        info.author = "Veloce Team";
        info.description = "M-cycle accurate Game Boy Color emulator with Sharp LR35902 CPU. "
                           "Features color palettes, double-speed mode, HDMA transfers, "
                           "and passes 100% of Mooneye timing tests.";
        info.file_extensions = s_extensions;
        info.native_fps = 59.7275;
        info.cycles_per_second = 8388608;  // 8.39 MHz (double speed capable)
        info.screen_width = 160;
        info.screen_height = 144;
    } else {
        info.name = "GB";
        info.version = "0.1.0";
        info.author = "Veloce Team";
        info.description = "M-cycle accurate Game Boy emulator with Sharp LR35902 CPU. "
                           "Features accurate PPU/APU emulation and passes 100% of "
                           "Blargg and Mooneye timing tests.";
        info.file_extensions = s_extensions;
        info.native_fps = 59.7275;  // 70224 cycles per frame at 4.19 MHz
        info.cycles_per_second = 4194304;  // 4.19 MHz
        info.screen_width = 160;
        info.screen_height = 144;
    }

    return info;
}

const emu::ControllerLayoutInfo* GBPlugin::get_controller_layout() {
    return &GB_CONTROLLER_LAYOUT;
}

bool GBPlugin::load_rom(const uint8_t* data, size_t size) {
    if (m_rom_loaded) {
        unload_rom();
    }

    // Load cartridge
    if (!m_cartridge->load(data, size)) {
        return false;
    }

    // Get system type from cartridge
    m_system_type = m_cartridge->get_system_type();
    m_rom_crc32 = m_cartridge->get_crc32();

    // Setup GB/GBC system
    m_bus = std::make_unique<Bus>();
    m_cpu = std::make_unique<LR35902>(*m_bus);
    m_ppu = std::make_unique<PPU>(*m_bus);

    // Connect components
    m_bus->connect_cpu(m_cpu.get());
    m_bus->connect_ppu(m_ppu.get());
    m_bus->connect_apu(m_apu.get());
    m_bus->connect_cartridge(m_cartridge.get());

    // Set CGB mode
    bool is_cgb = (m_system_type == SystemType::GameBoyColor);
    m_bus->set_cgb_mode(is_cgb);
    m_ppu->set_cgb_mode(is_cgb);
    m_apu->set_cgb_mode(is_cgb);

    // Reset everything
    m_cpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_cartridge->reset();

    // Apply the current palette setting to the new PPU
    if (!is_cgb) {
        if (m_use_custom_palette) {
            m_ppu->set_dmg_palette(m_custom_palette);
        } else {
            m_ppu->set_dmg_palette(s_palette_presets[m_selected_palette].colors);
        }
    }

    m_rom_loaded = true;
    m_total_cycles = 0;
    m_frame_count = 0;
    m_test_result_reported = false;

    if (is_debug_mode()) {
        printf("[GB] ROM loaded: %s (%s)\n",
               m_cartridge->get_title().c_str(),
               is_cgb ? "GBC" : "DMG");
    }

    return true;
}

void GBPlugin::unload_rom() {
    m_cpu.reset();
    m_bus.reset();
    m_ppu.reset();
    m_cartridge->unload();
    m_rom_loaded = false;
    m_rom_crc32 = 0;
    m_total_cycles = 0;
    m_frame_count = 0;
}

bool GBPlugin::is_rom_loaded() const {
    return m_rom_loaded;
}

uint32_t GBPlugin::get_rom_crc32() const {
    return m_rom_crc32;
}

void GBPlugin::reset() {
    if (!m_rom_loaded) return;

    m_cpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_cartridge->reset();

    m_total_cycles = 0;
    m_frame_count = 0;
    m_test_result_reported = false;
}

void GBPlugin::run_frame(const emu::InputState& input) {
    if (!m_rom_loaded) return;
    run_gb_frame(input);
    m_frame_count++;
}

void GBPlugin::run_gb_frame(const emu::InputState& input) {
    // Set input state
    m_bus->set_input_state(input.buttons);

    // GB: 70224 T-cycles per frame (154 scanlines * 456 T-cycles)
    // CPU operates in M-cycles where 1 M-cycle = 4 T-cycles
    constexpr int T_CYCLES_PER_FRAME = 70224;
    int t_cycles_run = 0;

    while (t_cycles_run < T_CYCLES_PER_FRAME) {
        // CPU step returns M-cycles
        // Note: The CPU's read/write methods now tick the bus (timer, serial, OAM DMA)
        // during each memory access for cycle-accurate timing.
        int m_cycles = m_cpu->step();
        int t_cycles = m_cycles * 4;  // Convert to T-cycles

        m_total_cycles += m_cycles;
        t_cycles_run += t_cycles;

        // Timer, serial, and OAM DMA are now stepped during memory accesses
        // via bus.tick_m_cycle() called from CPU read/write.
        // We don't step them again here to avoid double-counting.

        // Step PPU (operates on T-cycles, 1 T-cycle per step)
        for (int i = 0; i < t_cycles; i++) {
            m_ppu->step();
        }

        // Step APU (operates on T-cycles for proper timing)
        m_apu->step(t_cycles);

        // Handle interrupts
        uint8_t interrupts = m_bus->get_pending_interrupts();
        if (interrupts) {
            m_cpu->handle_interrupts(interrupts);
        }
    }

    // Copy framebuffer
    const uint32_t* ppu_fb = m_ppu->get_framebuffer();
    std::memcpy(m_framebuffer, ppu_fb, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

    // Get audio samples
    m_audio_samples = m_apu->get_samples(m_audio_buffer, AUDIO_BUFFER_SIZE);

    // Check for test ROM results in debug mode
    if (is_debug_mode() && !m_test_result_reported) {
        const std::string& serial_output = m_bus->get_serial_output();
        if (!serial_output.empty()) {
            // Look for test result patterns
            if (serial_output.find("Passed") != std::string::npos ||
                serial_output.find("PASSED") != std::string::npos ||
                serial_output.find("passed") != std::string::npos) {
                printf("\n=== TEST PASSED ===\n");
                printf("Serial output:\n%s\n", serial_output.c_str());
                m_test_result_reported = true;
            } else if (serial_output.find("Failed") != std::string::npos ||
                       serial_output.find("FAILED") != std::string::npos ||
                       serial_output.find("failed") != std::string::npos) {
                printf("\n=== TEST FAILED ===\n");
                printf("Serial output:\n%s\n", serial_output.c_str());
                m_test_result_reported = true;
            }
        }
    }
}

uint64_t GBPlugin::get_cycle_count() const {
    return m_total_cycles;
}

uint64_t GBPlugin::get_frame_count() const {
    return m_frame_count;
}

emu::FrameBuffer GBPlugin::get_framebuffer() {
    return {
        m_framebuffer,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    };
}

emu::AudioBuffer GBPlugin::get_audio() {
    return {
        m_audio_buffer,
        static_cast<int>(m_audio_samples),
        44100  // Sample rate
    };
}

void GBPlugin::clear_audio_buffer() {
    m_audio_samples = 0;
}

void GBPlugin::set_audio_callback(AudioStreamCallback callback) {
    // Store in base class
    m_audio_callback = callback;

    // Forward to APU for direct streaming
    if (m_apu) {
        if (callback) {
            m_apu->set_audio_callback([callback](const float* samples, size_t count, int rate) {
                callback(samples, count, rate);
            });
        } else {
            m_apu->set_audio_callback(nullptr);
        }
    }
}

uint8_t GBPlugin::read_memory(uint16_t address) {
    if (m_bus) {
        return m_bus->read(address);
    }
    return 0xFF;
}

void GBPlugin::write_memory(uint16_t address, uint8_t value) {
    if (m_bus) {
        m_bus->write(address, value);
    }
}

bool GBPlugin::save_state(std::vector<uint8_t>& data) {
    if (!m_rom_loaded) return false;

    // Save total cycles and frame count
    for (int i = 0; i < 8; i++) {
        data.push_back((m_total_cycles >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; i++) {
        data.push_back((m_frame_count >> (i * 8)) & 0xFF);
    }

    // Save component states
    m_cpu->save_state(data);
    m_bus->save_state(data);
    m_ppu->save_state(data);
    m_apu->save_state(data);
    m_cartridge->save_state(data);

    return true;
}

bool GBPlugin::load_state(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || data.size() < 16) return false;

    const uint8_t* ptr = data.data();
    size_t remaining = data.size();

    // Load total cycles and frame count
    m_total_cycles = 0;
    for (int i = 0; i < 8; i++) {
        m_total_cycles |= static_cast<uint64_t>(*ptr++) << (i * 8);
        remaining--;
    }
    m_frame_count = 0;
    for (int i = 0; i < 8; i++) {
        m_frame_count |= static_cast<uint64_t>(*ptr++) << (i * 8);
        remaining--;
    }

    // Load component states
    m_cpu->load_state(ptr, remaining);
    m_bus->load_state(ptr, remaining);
    m_ppu->load_state(ptr, remaining);
    m_apu->load_state(ptr, remaining);
    m_cartridge->load_state(ptr, remaining);

    return true;
}

bool GBPlugin::has_battery_save() const {
    return m_cartridge && m_cartridge->has_battery();
}

std::vector<uint8_t> GBPlugin::get_battery_save_data() const {
    if (m_cartridge) {
        return m_cartridge->get_save_data();
    }
    return {};
}

bool GBPlugin::set_battery_save_data(const std::vector<uint8_t>& data) {
    if (m_cartridge) {
        return m_cartridge->set_save_data(data);
    }
    return false;
}

// Helper to convert ABGR (internal format) to ImVec4 (RGBA float)
static ImVec4 abgr_to_imvec4(uint32_t abgr) {
    float r = ((abgr >> 0) & 0xFF) / 255.0f;
    float g = ((abgr >> 8) & 0xFF) / 255.0f;
    float b = ((abgr >> 16) & 0xFF) / 255.0f;
    float a = ((abgr >> 24) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}

// Helper to convert ImVec4 (RGBA float) to ABGR (internal format)
static uint32_t imvec4_to_abgr(const ImVec4& col) {
    uint8_t r = static_cast<uint8_t>(col.x * 255.0f);
    uint8_t g = static_cast<uint8_t>(col.y * 255.0f);
    uint8_t b = static_cast<uint8_t>(col.z * 255.0f);
    uint8_t a = static_cast<uint8_t>(col.w * 255.0f);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

void GBPlugin::render_config_gui(bool& visible) {
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Game Boy Settings", &visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    render_config_gui_content();

    ImGui::End();
}

void GBPlugin::render_config_gui_content() {
    // Display current system info
    if (m_rom_loaded) {
        ImGui::Text("System: %s", m_system_type == SystemType::GameBoyColor ? "Game Boy Color" : "Game Boy (DMG)");
        ImGui::Text("Game: %s", m_cartridge ? m_cartridge->get_title().c_str() : "Unknown");
        ImGui::Separator();
    }

    // Only show DMG palette options for non-CGB games
    bool is_dmg = (m_system_type != SystemType::GameBoyColor) || !m_rom_loaded;

    if (ImGui::CollapsingHeader("DMG Palette", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!is_dmg && m_rom_loaded) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f),
                               "Game Boy Color games use their own color palettes.");
            ImGui::TextWrapped("These palette settings only affect original Game Boy (DMG) games.");
            ImGui::Spacing();
        }

        ImGui::BeginDisabled(!is_dmg && m_rom_loaded);

        // Preset selector
        ImGui::Text("Palette Preset:");
        if (ImGui::BeginCombo("##PalettePreset",
                              m_use_custom_palette ? "Custom" : s_palette_presets[m_selected_palette].name)) {
            for (int i = 0; i < NUM_PALETTE_PRESETS; i++) {
                bool is_selected = (!m_use_custom_palette && m_selected_palette == i);
                if (ImGui::Selectable(s_palette_presets[i].name, is_selected)) {
                    m_selected_palette = i;
                    m_use_custom_palette = false;

                    // Apply the preset palette
                    if (m_ppu) {
                        m_ppu->set_dmg_palette(s_palette_presets[i].colors);
                    }
                    std::memcpy(m_custom_palette, s_palette_presets[i].colors, sizeof(m_custom_palette));
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        // Color preview and custom color pickers
        ImGui::Text("Colors (Lightest to Darkest):");
        ImGui::Spacing();

        const char* color_labels[] = {"Lightest", "Light", "Dark", "Darkest"};
        bool palette_changed = false;

        for (int i = 0; i < 4; i++) {
            ImGui::PushID(i);

            // Get current color
            ImVec4 color = abgr_to_imvec4(m_custom_palette[i]);

            // Color button with picker
            char label[32];
            snprintf(label, sizeof(label), "##Color%d", i);

            ImGui::Text("%s:", color_labels[i]);
            ImGui::SameLine(100);

            // ColorEdit3 returns true if color was changed
            if (ImGui::ColorEdit3(label, &color.x,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                m_custom_palette[i] = imvec4_to_abgr(color);
                m_use_custom_palette = true;
                palette_changed = true;
            }

            // Show hex value
            uint32_t abgr = m_custom_palette[i];
            uint8_t r = (abgr >> 0) & 0xFF;
            uint8_t g = (abgr >> 8) & 0xFF;
            uint8_t b = (abgr >> 16) & 0xFF;
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "#%02X%02X%02X", r, g, b);

            ImGui::PopID();
        }

        // Apply custom palette changes
        if (palette_changed && m_ppu) {
            m_ppu->set_dmg_palette(m_custom_palette);
        }

        ImGui::Spacing();

        // Reset to default button
        if (ImGui::Button("Reset to Default")) {
            m_selected_palette = 0;
            m_use_custom_palette = false;
            std::memcpy(m_custom_palette, s_palette_presets[0].colors, sizeof(m_custom_palette));
            if (m_ppu) {
                m_ppu->set_dmg_palette(s_palette_presets[0].colors);
            }
        }

        ImGui::EndDisabled();
    }

    // Speed / Timing Section
    if (ImGui::CollapsingHeader("Speed / Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Fast mode checkbox
        if (ImGui::Checkbox("Fast Mode (Uncapped Speed)", &m_fast_mode)) {
            // Setting is applied immediately via is_fast_mode_enabled()
        }

        // Help text
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(
                "When enabled, the emulator runs as fast as your CPU allows "
                "with no frame rate limiting.\n\n"
                "When disabled, the emulator runs at cycle-accurate speed "
                "(59.7275 FPS) to match real Game Boy hardware timing."
            );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // Show current mode status
        ImGui::Spacing();
        if (m_fast_mode) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Running at UNCAPPED speed");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Running at CYCLE-ACCURATE speed (default)");
        }
    }

    if (ImGui::CollapsingHeader("System Information")) {
        if (m_rom_loaded && m_cartridge) {
            ImGui::Text("Title: %s", m_cartridge->get_title().c_str());
            ImGui::Text("CRC32: %08X", m_rom_crc32);
            ImGui::Text("Mapper: %s", m_cartridge->get_mapper_name());
            ImGui::Text("Has Battery: %s", m_cartridge->has_battery() ? "Yes" : "No");
            ImGui::Text("Frame Count: %llu", static_cast<unsigned long long>(m_frame_count));
            ImGui::Text("Total Cycles: %llu", static_cast<unsigned long long>(m_total_cycles));
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No ROM loaded");
        }
    }
}

bool GBPlugin::save_config(const char* path) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    // Simple JSON format
    file << "{\n";
    file << "  \"selected_palette\": " << m_selected_palette << ",\n";
    file << "  \"use_custom_palette\": " << (m_use_custom_palette ? "true" : "false") << ",\n";
    file << "  \"custom_palette\": [\n";
    for (int i = 0; i < 4; i++) {
        file << "    " << m_custom_palette[i];
        if (i < 3) file << ",";
        file << "\n";
    }
    file << "  ],\n";
    file << "  \"fast_mode\": " << (m_fast_mode ? "true" : "false") << "\n";
    file << "}\n";

    return true;
}

bool GBPlugin::load_config(const char* path) {
    std::ifstream file(path);
    if (!file) {
        // File doesn't exist - use defaults (not an error)
        return true;
    }

    // Simple JSON parsing (basic, not a full parser)
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Parse selected_palette
    size_t pos = content.find("\"selected_palette\":");
    if (pos != std::string::npos) {
        pos = content.find(':', pos);
        if (pos != std::string::npos) {
            m_selected_palette = std::stoi(content.substr(pos + 1));
            if (m_selected_palette < 0 || m_selected_palette >= NUM_PALETTE_PRESETS) {
                m_selected_palette = 0;
            }
        }
    }

    // Parse use_custom_palette
    pos = content.find("\"use_custom_palette\":");
    if (pos != std::string::npos) {
        m_use_custom_palette = (content.find("true", pos) < content.find("false", pos));
    }

    // Parse custom_palette array
    pos = content.find("\"custom_palette\":");
    if (pos != std::string::npos) {
        pos = content.find('[', pos);
        if (pos != std::string::npos) {
            for (int i = 0; i < 4; i++) {
                // Find next number
                while (pos < content.size() && !isdigit(content[pos])) pos++;
                if (pos < content.size()) {
                    m_custom_palette[i] = std::stoul(content.substr(pos));
                    while (pos < content.size() && (isdigit(content[pos]) || content[pos] == '-')) pos++;
                }
            }
        }
    }

    // Parse fast_mode
    pos = content.find("\"fast_mode\":");
    if (pos != std::string::npos) {
        m_fast_mode = (content.find("true", pos) < content.find("false", pos) + 5);
    }

    // Apply loaded palette if using custom
    if (!m_use_custom_palette) {
        std::memcpy(m_custom_palette, s_palette_presets[m_selected_palette].colors, sizeof(m_custom_palette));
    }

    // Apply palette to PPU if it exists and we're in DMG mode
    if (m_ppu && m_system_type != SystemType::GameBoyColor) {
        m_ppu->set_dmg_palette(m_custom_palette);
    }

    return true;
}

} // namespace gb

// Plugin factory function - exported from shared library
extern "C" {
    EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() {
        return new gb::GBPlugin();
    }

    EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) {
        delete plugin;
    }

    EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() {
        return EMU_PLUGIN_API_VERSION;
    }
}
