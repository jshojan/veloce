#include "emu/emulator_plugin.hpp"
#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"

#include <cstring>
#include <iostream>

namespace nes {

// NES Controller button layout - defined by this plugin
static const emu::ButtonLayout NES_BUTTONS[] = {
    // D-pad (left side)
    {emu::VirtualButton::Up,     "Up",     0.15f, 0.35f, 0.08f, 0.12f, true},
    {emu::VirtualButton::Down,   "Down",   0.15f, 0.60f, 0.08f, 0.12f, true},
    {emu::VirtualButton::Left,   "Left",   0.08f, 0.47f, 0.08f, 0.12f, true},
    {emu::VirtualButton::Right,  "Right",  0.22f, 0.47f, 0.08f, 0.12f, true},
    // Select/Start (center)
    {emu::VirtualButton::Select, "SELECT", 0.38f, 0.55f, 0.10f, 0.06f, false},
    {emu::VirtualButton::Start,  "START",  0.52f, 0.55f, 0.10f, 0.06f, false},
    // B/A buttons (right side)
    {emu::VirtualButton::B,      "B",      0.72f, 0.47f, 0.10f, 0.14f, false},
    {emu::VirtualButton::A,      "A",      0.85f, 0.47f, 0.10f, 0.14f, false}
};

static const emu::ControllerLayoutInfo NES_CONTROLLER_LAYOUT = {
    "NES",
    "NES Controller",
    emu::ControllerShape::Rectangle,
    2.5f,  // Width is 2.5x height
    NES_BUTTONS,
    8,     // 8 buttons
    2      // 2 controllers supported
};

class NESPlugin : public emu::IEmulatorPlugin {
public:
    NESPlugin();
    ~NESPlugin() override;

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

private:
    std::unique_ptr<Bus> m_bus;
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Cartridge> m_cartridge;

    bool m_rom_loaded = false;
    uint32_t m_rom_crc32 = 0;
    uint64_t m_total_cycles = 0;
    uint64_t m_frame_count = 0;

    // Framebuffer
    uint32_t m_framebuffer[256 * 240];

    // Audio buffer
    static constexpr size_t AUDIO_BUFFER_SIZE = 2048;
    float m_audio_buffer[AUDIO_BUFFER_SIZE * 2];  // Stereo
    size_t m_audio_samples = 0;

    // File extensions
    static const char* s_extensions[];
};

const char* NESPlugin::s_extensions[] = { ".nes", ".NES", nullptr };

NESPlugin::NESPlugin() {
    std::memset(m_framebuffer, 0, sizeof(m_framebuffer));
    std::memset(m_audio_buffer, 0, sizeof(m_audio_buffer));

    m_bus = std::make_unique<Bus>();
    m_cpu = std::make_unique<CPU>(*m_bus);
    m_ppu = std::make_unique<PPU>(*m_bus);
    m_apu = std::make_unique<APU>(*m_bus);
    m_cartridge = std::make_unique<Cartridge>();

    // Connect components
    m_bus->connect_cpu(m_cpu.get());
    m_bus->connect_ppu(m_ppu.get());
    m_bus->connect_apu(m_apu.get());
    m_bus->connect_cartridge(m_cartridge.get());
}

NESPlugin::~NESPlugin() = default;

emu::EmulatorInfo NESPlugin::get_info() {
    emu::EmulatorInfo info;
    info.name = "NES";
    info.version = "0.1.0";
    info.file_extensions = s_extensions;
    info.native_fps = 60.0988;
    info.cycles_per_second = 1789773;
    info.screen_width = 256;
    info.screen_height = 240;
    return info;
}

const emu::ControllerLayoutInfo* NESPlugin::get_controller_layout() {
    return &NES_CONTROLLER_LAYOUT;
}

bool NESPlugin::load_rom(const uint8_t* data, size_t size) {
    if (!m_cartridge->load(data, size)) {
        std::cerr << "Failed to load ROM" << std::endl;
        return false;
    }

    m_rom_loaded = true;
    m_rom_crc32 = m_cartridge->get_crc32();
    reset();

    std::cout << "NES ROM loaded, CRC32: " << std::hex << m_rom_crc32 << std::dec << std::endl;
    return true;
}

void NESPlugin::unload_rom() {
    m_cartridge->unload();
    m_rom_loaded = false;
    m_rom_crc32 = 0;
    m_total_cycles = 0;
    m_frame_count = 0;
}

bool NESPlugin::is_rom_loaded() const {
    return m_rom_loaded;
}

uint32_t NESPlugin::get_rom_crc32() const {
    return m_rom_crc32;
}

void NESPlugin::reset() {
    m_cpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_total_cycles = 0;
    m_frame_count = 0;
    m_audio_samples = 0;
}

void NESPlugin::run_frame(const emu::InputState& input) {
    if (!m_rom_loaded) return;

    // Run until PPU signals frame completion (at VBlank start)
    bool frame_complete = false;

    while (!frame_complete) {
        // Step CPU
        int cpu_cycles = m_cpu->step();
        m_total_cycles += cpu_cycles;

        // Check for OAM DMA (takes 513-514 CPU cycles)
        int dma_cycles = m_bus->get_pending_dma_cycles();
        if (dma_cycles > 0) {
            cpu_cycles += dma_cycles;
            m_total_cycles += dma_cycles;
        }

        // Step PPU (3 PPU cycles per CPU cycle)
        int ppu_cycles = cpu_cycles * 3;
        for (int i = 0; i < ppu_cycles; i++) {
            m_ppu->step();

            // Check for NMI (only need to check once per batch, but NMI timing is critical)
            int nmi_type = m_ppu->check_nmi();
            if (nmi_type == 1) {
                m_cpu->trigger_nmi();
            } else if (nmi_type == 2) {
                m_cpu->trigger_nmi_delayed();
            }

            // Check for frame completion (at VBlank start)
            if (m_ppu->check_frame_complete()) {
                frame_complete = true;
                // Check for test ROM output once per frame
                static int check_interval = 0;
                if (++check_interval >= 30) {
                    check_interval = 0;
                    m_bus->check_test_output();
                }
            }
        }

        // Check for mapper IRQ once per CPU instruction (sufficient for MMC3 timing)
        // The IRQ line is level-triggered, so we just need to check the current state
        m_cpu->set_irq_line(m_bus->mapper_irq_pending(m_ppu->get_frame_cycle()));

        // Step APU
        m_apu->step(cpu_cycles);
    }

    // Set controller state for NEXT frame's NMI to read
    // This must happen after the loop so the pending NMI (triggered at VBlank)
    // will read this input when it runs at the start of the next run_frame
    m_bus->set_controller_state(0, input.buttons);

    // Copy PPU framebuffer - now guaranteed to be at the correct frame boundary
    const uint32_t* ppu_fb = m_ppu->get_framebuffer();
    std::memcpy(m_framebuffer, ppu_fb, sizeof(m_framebuffer));

    // Get audio samples
    m_audio_samples = m_apu->get_samples(m_audio_buffer, AUDIO_BUFFER_SIZE);

    m_frame_count++;
}

uint64_t NESPlugin::get_cycle_count() const {
    return m_total_cycles;
}

uint64_t NESPlugin::get_frame_count() const {
    return m_frame_count;
}

emu::FrameBuffer NESPlugin::get_framebuffer() {
    emu::FrameBuffer fb;
    fb.pixels = m_framebuffer;
    fb.width = 256;
    fb.height = 240;
    return fb;
}

emu::AudioBuffer NESPlugin::get_audio() {
    emu::AudioBuffer ab;
    ab.samples = m_audio_buffer;
    ab.sample_count = static_cast<int>(m_audio_samples);
    ab.sample_rate = 44100;
    return ab;
}

void NESPlugin::clear_audio_buffer() {
    m_audio_samples = 0;
}

uint8_t NESPlugin::read_memory(uint16_t address) {
    return m_bus->cpu_read(address);
}

void NESPlugin::write_memory(uint16_t address, uint8_t value) {
    m_bus->cpu_write(address, value);
}

bool NESPlugin::save_state(std::vector<uint8_t>& data) {
    if (!m_rom_loaded) return false;

    try {
        data.clear();

        // Reserve some space for efficiency
        data.reserve(32 * 1024);  // 32KB should be plenty

        // Save frame count
        const uint8_t* fc_ptr = reinterpret_cast<const uint8_t*>(&m_frame_count);
        data.insert(data.end(), fc_ptr, fc_ptr + sizeof(m_frame_count));

        const uint8_t* tc_ptr = reinterpret_cast<const uint8_t*>(&m_total_cycles);
        data.insert(data.end(), tc_ptr, tc_ptr + sizeof(m_total_cycles));

        // Save each component
        m_cpu->save_state(data);
        m_ppu->save_state(data);
        m_apu->save_state(data);
        m_bus->save_state(data);
        m_cartridge->save_state(data);

        return true;
    } catch (...) {
        return false;
    }
}

bool NESPlugin::load_state(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || data.empty()) return false;

    try {
        const uint8_t* ptr = data.data();
        size_t remaining = data.size();

        // Load frame count
        if (remaining < sizeof(m_frame_count) + sizeof(m_total_cycles)) {
            return false;
        }
        std::memcpy(&m_frame_count, ptr, sizeof(m_frame_count));
        ptr += sizeof(m_frame_count);
        remaining -= sizeof(m_frame_count);

        std::memcpy(&m_total_cycles, ptr, sizeof(m_total_cycles));
        ptr += sizeof(m_total_cycles);
        remaining -= sizeof(m_total_cycles);

        // Load each component
        m_cpu->load_state(ptr, remaining);
        m_ppu->load_state(ptr, remaining);
        m_apu->load_state(ptr, remaining);
        m_bus->load_state(ptr, remaining);
        m_cartridge->load_state(ptr, remaining);

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace nes

// C interface for plugin loading
extern "C" {

EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() {
    return new nes::NESPlugin();
}

EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() {
    return EMU_PLUGIN_API_VERSION;
}

}
