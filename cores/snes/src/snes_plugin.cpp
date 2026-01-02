#include "emu/emulator_plugin.hpp"
#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "dma.hpp"
#include "cartridge.hpp"
#include "debug.hpp"

#include <cstring>
#include <iostream>

namespace snes {

// SNES Controller button layout
// The SNES controller has: D-pad, Select, Start, Y, B, X, A, L, R
static const emu::ButtonLayout SNES_BUTTONS[] = {
    // D-pad (left side)
    {emu::VirtualButton::Up,     "Up",     0.12f, 0.30f, 0.06f, 0.10f, true},
    {emu::VirtualButton::Down,   "Down",   0.12f, 0.52f, 0.06f, 0.10f, true},
    {emu::VirtualButton::Left,   "Left",   0.06f, 0.41f, 0.06f, 0.10f, true},
    {emu::VirtualButton::Right,  "Right",  0.18f, 0.41f, 0.06f, 0.10f, true},
    // Select/Start (center)
    {emu::VirtualButton::Select, "SELECT", 0.38f, 0.50f, 0.08f, 0.05f, false},
    {emu::VirtualButton::Start,  "START",  0.52f, 0.50f, 0.08f, 0.05f, false},
    // Face buttons (right side) - arranged in diamond
    {emu::VirtualButton::X,      "X",      0.82f, 0.25f, 0.08f, 0.10f, false},
    {emu::VirtualButton::A,      "A",      0.90f, 0.41f, 0.08f, 0.10f, false},
    {emu::VirtualButton::B,      "B",      0.82f, 0.57f, 0.08f, 0.10f, false},
    {emu::VirtualButton::Y,      "Y",      0.74f, 0.41f, 0.08f, 0.10f, false},
    // Shoulder buttons
    {emu::VirtualButton::L,      "L",      0.08f, 0.08f, 0.12f, 0.06f, false},
    {emu::VirtualButton::R,      "R",      0.80f, 0.08f, 0.12f, 0.06f, false}
};

static const emu::ControllerLayoutInfo SNES_CONTROLLER_LAYOUT = {
    "SNES",
    "SNES Controller",
    emu::ControllerShape::Rectangle,
    2.8f,  // Width is 2.8x height (SNES controller is wider than NES)
    SNES_BUTTONS,
    12,    // 12 buttons (D-pad counts as 4)
    2      // 2 controllers supported
};

class SNESPlugin : public emu::IEmulatorPlugin {
public:
    SNESPlugin();
    ~SNESPlugin() override;

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
    // Convert input state to SNES controller format
    uint32_t convert_input(uint32_t buttons);

    std::unique_ptr<Bus> m_bus;
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<DMA> m_dma;
    std::unique_ptr<Cartridge> m_cartridge;

    bool m_rom_loaded = false;
    uint32_t m_rom_crc32 = 0;
    uint64_t m_total_cycles = 0;
    uint64_t m_frame_count = 0;

    // Framebuffer (256x224 native, stored as 256x240 for overscan handling)
    uint32_t m_framebuffer[256 * 240];

    // Audio buffer
    static constexpr size_t AUDIO_BUFFER_SIZE = 2048;
    float m_audio_buffer[AUDIO_BUFFER_SIZE * 2];  // Stereo
    size_t m_audio_samples = 0;

    // File extensions
    static const char* s_extensions[];
};

const char* SNESPlugin::s_extensions[] = { ".sfc", ".smc", ".SFC", ".SMC", nullptr };

SNESPlugin::SNESPlugin() {
    std::memset(m_framebuffer, 0, sizeof(m_framebuffer));
    std::memset(m_audio_buffer, 0, sizeof(m_audio_buffer));

    m_bus = std::make_unique<Bus>();
    m_cpu = std::make_unique<CPU>(*m_bus);
    m_ppu = std::make_unique<PPU>(*m_bus);
    m_apu = std::make_unique<APU>();
    m_dma = std::make_unique<DMA>(*m_bus);
    m_cartridge = std::make_unique<Cartridge>();

    // Connect components through the bus
    m_bus->connect_cpu(m_cpu.get());
    m_bus->connect_ppu(m_ppu.get());
    m_bus->connect_apu(m_apu.get());
    m_bus->connect_dma(m_dma.get());
    m_bus->connect_cartridge(m_cartridge.get());
}

SNESPlugin::~SNESPlugin() = default;

emu::EmulatorInfo SNESPlugin::get_info() {
    emu::EmulatorInfo info;
    info.name = "SNES";
    info.version = "0.1.0";
    info.author = "Veloce Team";
    info.description = "Super Nintendo Entertainment System emulator with support for "
                       "LoROM and HiROM cartridges. Features accurate 65816 CPU emulation, "
                       "all PPU background modes including Mode 7, SPC700 audio processor, "
                       "and DMA/HDMA support.";
    info.file_extensions = s_extensions;
    info.native_fps = 60.0988;  // NTSC: 21477272.0 / 357366.0
    info.cycles_per_second = 21477272;  // Master clock
    info.screen_width = 256;
    info.screen_height = 224;  // Standard NTSC visible height
    return info;
}

const emu::ControllerLayoutInfo* SNESPlugin::get_controller_layout() {
    return &SNES_CONTROLLER_LAYOUT;
}

bool SNESPlugin::load_rom(const uint8_t* data, size_t size) {
    if (!m_cartridge->load(data, size)) {
        std::cerr << "Failed to load SNES ROM" << std::endl;
        return false;
    }

    m_rom_loaded = true;
    m_rom_crc32 = m_cartridge->get_crc32();
    reset();

    std::cout << "SNES ROM loaded, CRC32: " << std::hex << m_rom_crc32 << std::dec << std::endl;
    return true;
}

void SNESPlugin::unload_rom() {
    m_cartridge->unload();
    m_rom_loaded = false;
    m_rom_crc32 = 0;
    m_total_cycles = 0;
    m_frame_count = 0;
}

bool SNESPlugin::is_rom_loaded() const {
    return m_rom_loaded;
}

uint32_t SNESPlugin::get_rom_crc32() const {
    return m_rom_crc32;
}

void SNESPlugin::reset() {
    m_cpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_dma->reset();
    m_cartridge->reset();
    m_total_cycles = 0;
    m_frame_count = 0;
    m_audio_samples = 0;

    // Pre-run the APU to give it a head start
    // The SPC700 IPL ROM needs ~2000 cycles to initialize and write $BBAA to ports
    // This is about 42000 master cycles (2000 * 21)
    // We run it for a bit more to ensure it's ready before the main CPU starts
    // accessing the APU ports.
    //
    // Reference: The IPL ROM clears ~240 bytes of memory (240 iterations * ~8 cycles)
    // plus some initialization, totaling roughly 2000-2500 SPC cycles.
    m_apu->step(50000);  // Run APU for ~50000 master cycles (~2380 SPC cycles)
}

uint32_t SNESPlugin::convert_input(uint32_t buttons) {
    // Convert from VirtualButton bitmask to SNES controller format
    // SNES controller bit layout (active low on hardware, but we use active high):
    // Bit 0:  B
    // Bit 1:  Y
    // Bit 2:  Select
    // Bit 3:  Start
    // Bit 4:  Up
    // Bit 5:  Down
    // Bit 6:  Left
    // Bit 7:  Right
    // Bit 8:  A
    // Bit 9:  X
    // Bit 10: L
    // Bit 11: R
    // Bits 12-15: ID bits (always 0)

    uint32_t snes_buttons = 0;

    if (buttons & (1 << static_cast<int>(emu::VirtualButton::B)))      snes_buttons |= 0x0001;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Y)))      snes_buttons |= 0x0002;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Select))) snes_buttons |= 0x0004;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Start)))  snes_buttons |= 0x0008;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Up)))     snes_buttons |= 0x0010;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Down)))   snes_buttons |= 0x0020;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Left)))   snes_buttons |= 0x0040;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::Right)))  snes_buttons |= 0x0080;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::A)))      snes_buttons |= 0x0100;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::X)))      snes_buttons |= 0x0200;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::L)))      snes_buttons |= 0x0400;
    if (buttons & (1 << static_cast<int>(emu::VirtualButton::R)))      snes_buttons |= 0x0800;

    return snes_buttons;
}

void SNESPlugin::run_frame(const emu::InputState& input) {
    if (!m_rom_loaded) return;

    // Debug: Output diagnostic info for the first few frames
    if (m_frame_count < 5) {
        SNES_DEBUG_PRINT("Frame %llu: PC=$%02X:%04X force_blank=%d brightness=%d TM=$%02X\n",
            (unsigned long long)m_frame_count,
            m_cpu->get_pbr(), m_cpu->get_pc(),
            m_ppu->is_force_blank() ? 1 : 0,
            m_ppu->get_brightness(),
            m_ppu->get_main_screen_layers());
    }

    // Set controller state at the start of the frame
    uint32_t snes_input = convert_input(input.buttons);
    m_bus->set_controller_state(0, snes_input);

    // SNES timing:
    // Master clock: 21.477272 MHz (NTSC)
    // CPU clock: Master / 6 or Master / 8 (depending on memory access speed)
    // Scanlines: 262 (NTSC), 312 (PAL)
    // Dots per scanline: 340
    // Frame time: 262 * 340 * 4 master cycles = 356,160 master cycles per frame
    //
    // For simplicity, we run scanline by scanline

    static constexpr int SCANLINES_PER_FRAME = 262;
    static constexpr int DOTS_PER_SCANLINE = 340;
    static constexpr int MASTER_CYCLES_PER_DOT = 4;
    static constexpr int MASTER_CYCLES_PER_SCANLINE = DOTS_PER_SCANLINE * MASTER_CYCLES_PER_DOT;

    // Signal start of frame
    m_bus->start_frame();
    m_dma->hdma_init();

    for (int scanline = 0; scanline < SCANLINES_PER_FRAME; scanline++) {
        // Run CPU for approximately one scanline worth of cycles
        // CPU runs at Master/6 (fast) or Master/8 (slow)
        // Average is about Master/6, giving ~227 CPU cycles per scanline

        int cycles_this_scanline = 0;
        int target_cycles = MASTER_CYCLES_PER_SCANLINE;

        while (cycles_this_scanline < target_cycles) {
            // Check for DMA (halts CPU)
            int dma_cycles = m_dma->get_dma_cycles();
            if (dma_cycles > 0) {
                cycles_this_scanline += dma_cycles;
                m_total_cycles += dma_cycles;
                m_dma->clear_dma_cycles();
                continue;
            }

            // Step CPU
            int cpu_cycles = m_cpu->step();

            // Convert CPU cycles to master cycles
            // Assume average of 6 master cycles per CPU cycle
            int master_cycles = cpu_cycles * 6;
            cycles_this_scanline += master_cycles;
            m_total_cycles += master_cycles;

            // Step APU (runs at its own clock)
            m_apu->step(master_cycles);

            // Check for NMI
            if (m_bus->nmi_pending()) {
                m_cpu->trigger_nmi();
                m_bus->clear_nmi();
            }

            // Check for IRQ
            if (m_bus->irq_pending() && !m_cpu->get_interrupt_disable()) {
                m_cpu->trigger_irq();
            }
        }

        // Render scanline (for visible scanlines 1-224)
        if (scanline >= 1 && scanline <= 224) {
            m_ppu->render_scanline(scanline - 1);
        }

        // H-blank processing
        m_bus->start_hblank();

        // HDMA transfers occur during H-blank
        if (scanline < 225) {  // HDMA only during active display
            m_dma->hdma_transfer();
        }

        // V-blank starts at scanline 225
        if (scanline == 225) {
            m_bus->start_vblank();
        }
    }

    // Copy PPU framebuffer
    // The PPU may render at 256 or 512 width depending on pseudo-hires/Mode 5-6
    // We need to handle this properly - for now, always sample from the PPU's
    // native resolution to our 256x224 output buffer
    const uint32_t* ppu_fb = m_ppu->get_framebuffer();
    int ppu_width = m_ppu->get_screen_width();  // 256 or 512
    (void)m_ppu->get_screen_height(); // 224 or 239 (unused for now, we always output 224)

    if (ppu_width == 256) {
        // Standard mode - direct copy
        std::memcpy(m_framebuffer, ppu_fb, 256 * 224 * sizeof(uint32_t));
    } else {
        // Pseudo-hires or Mode 5/6: PPU renders at 512 width
        // For now, we downsample by taking every other pixel (or averaging)
        // This gives a usable 256-width output from the 512-width source
        for (int y = 0; y < 224; y++) {
            for (int x = 0; x < 256; x++) {
                // Simple nearest-neighbor: take the "main screen" pixel (odd index)
                // In pseudo-hires, even pixels are sub screen, odd are main screen
                m_framebuffer[y * 256 + x] = ppu_fb[y * 512 + x * 2 + 1];
            }
        }
    }

    // Notify PPU frame complete (updates internal frame counter for debug)
    m_ppu->end_frame();

    // Debug: Check for non-black pixels every 60 frames
    if (is_debug_mode() && (m_frame_count % 60 == 0)) {
        int non_black = 0;
        for (int i = 0; i < 256 * 224; i++) {
            if ((m_framebuffer[i] & 0x00FFFFFF) != 0) non_black++;
        }
        fprintf(stderr, "[SNES] Frame %llu: %d non-black pixels (%.1f%%)\n",
                (unsigned long long)m_frame_count, non_black,
                non_black * 100.0 / (256.0 * 224.0));
    }

    // Get audio samples
    m_audio_samples = m_apu->get_samples(m_audio_buffer, AUDIO_BUFFER_SIZE);

    m_frame_count++;

    // Check for Blargg test completion and report results
    if (m_bus->blargg_test_completed()) {
        m_bus->report_blargg_result(m_frame_count);
    }
}

uint64_t SNESPlugin::get_cycle_count() const {
    return m_total_cycles;
}

uint64_t SNESPlugin::get_frame_count() const {
    return m_frame_count;
}

emu::FrameBuffer SNESPlugin::get_framebuffer() {
    emu::FrameBuffer fb;
    fb.pixels = m_framebuffer;
    fb.width = 256;
    fb.height = 224;
    return fb;
}

emu::AudioBuffer SNESPlugin::get_audio() {
    emu::AudioBuffer ab;
    ab.samples = m_audio_buffer;
    ab.sample_count = static_cast<int>(m_audio_samples);
    ab.sample_rate = 44100;
    return ab;
}

void SNESPlugin::clear_audio_buffer() {
    m_audio_samples = 0;
}

uint8_t SNESPlugin::read_memory(uint16_t address) {
    // Read from bank 0 by default (for debug purposes)
    return m_bus->read(address);
}

void SNESPlugin::write_memory(uint16_t address, uint8_t value) {
    // Write to bank 0 by default
    m_bus->write(address, value);
}

bool SNESPlugin::save_state(std::vector<uint8_t>& data) {
    if (!m_rom_loaded) return false;

    try {
        data.clear();
        data.reserve(256 * 1024);  // Reserve 256KB

        // Save frame count and cycle count
        const uint8_t* fc_ptr = reinterpret_cast<const uint8_t*>(&m_frame_count);
        data.insert(data.end(), fc_ptr, fc_ptr + sizeof(m_frame_count));

        const uint8_t* tc_ptr = reinterpret_cast<const uint8_t*>(&m_total_cycles);
        data.insert(data.end(), tc_ptr, tc_ptr + sizeof(m_total_cycles));

        // Save each component
        m_cpu->save_state(data);
        m_ppu->save_state(data);
        m_apu->save_state(data);
        m_dma->save_state(data);
        m_bus->save_state(data);
        m_cartridge->save_state(data);

        return true;
    } catch (...) {
        return false;
    }
}

bool SNESPlugin::load_state(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || data.empty()) return false;

    try {
        const uint8_t* ptr = data.data();
        size_t remaining = data.size();

        // Load frame count and cycle count
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
        m_dma->load_state(ptr, remaining);
        m_bus->load_state(ptr, remaining);
        m_cartridge->load_state(ptr, remaining);

        return true;
    } catch (...) {
        return false;
    }
}

bool SNESPlugin::has_battery_save() const {
    return m_rom_loaded && m_cartridge && m_cartridge->has_battery();
}

std::vector<uint8_t> SNESPlugin::get_battery_save_data() const {
    if (!m_rom_loaded || !m_cartridge) {
        return {};
    }
    return m_cartridge->get_save_data();
}

bool SNESPlugin::set_battery_save_data(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || !m_cartridge) {
        return false;
    }
    return m_cartridge->set_save_data(data);
}

} // namespace snes

// C interface for plugin loading
extern "C" {

EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() {
    return new snes::SNESPlugin();
}

EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() {
    return EMU_PLUGIN_API_VERSION;
}

}
