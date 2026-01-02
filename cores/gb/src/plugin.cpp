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

namespace gb {

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

    // File extensions
    static const char* s_extensions[];
};

const char* GBPlugin::s_extensions[] = { ".gb", ".GB", ".gbc", ".GBC", nullptr };

GBPlugin::GBPlugin() {
    std::memset(m_framebuffer, 0, sizeof(m_framebuffer));
    std::memset(m_audio_buffer, 0, sizeof(m_audio_buffer));

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
        info.description = "Game Boy Color emulator with Sharp LR35902 CPU, color palette "
                           "support, double-speed mode, and HDMA transfers.";
        info.file_extensions = s_extensions;
        info.native_fps = 59.7275;
        info.cycles_per_second = 8388608;  // 8.39 MHz (double speed capable)
        info.screen_width = 160;
        info.screen_height = 144;
    } else {
        info.name = "GB";
        info.version = "0.1.0";
        info.author = "Veloce Team";
        info.description = "Game Boy emulator with Sharp LR35902 CPU, accurate PPU timing, "
                           "and four-channel audio processing.";
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
        int m_cycles = m_cpu->step();
        int t_cycles = m_cycles * 4;  // Convert to T-cycles

        m_total_cycles += m_cycles;
        t_cycles_run += t_cycles;

        // Step timer (operates on M-cycles for proper timing)
        m_bus->step_timer(m_cycles);

        // Step OAM DMA (one byte per M-cycle when active)
        for (int i = 0; i < m_cycles; i++) {
            m_bus->step_oam_dma();
        }

        // Step PPU (operates on T-cycles, 1 T-cycle per step)
        for (int i = 0; i < t_cycles; i++) {
            m_ppu->step();
        }

        // Step APU (operates on T-cycles for proper timing)
        m_apu->step(t_cycles);

        // Step serial (for link cable emulation)
        m_bus->step_serial(m_cycles);

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
