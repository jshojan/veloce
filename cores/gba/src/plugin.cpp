#include "emu/emulator_plugin.hpp"
#include "types.hpp"
#include "arm7tdmi.hpp"
#include "bus.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include "debug.hpp"

#include <cstring>
#include <cstdio>
#include <iostream>
#include <memory>

namespace gba {

// GBA Controller button layout
static const emu::ButtonLayout GBA_BUTTONS[] = {
    // D-pad (left side)
    {emu::VirtualButton::Up,     "Up",     0.12f, 0.30f, 0.08f, 0.12f, true},
    {emu::VirtualButton::Down,   "Down",   0.12f, 0.55f, 0.08f, 0.12f, true},
    {emu::VirtualButton::Left,   "Left",   0.05f, 0.42f, 0.08f, 0.12f, true},
    {emu::VirtualButton::Right,  "Right",  0.19f, 0.42f, 0.08f, 0.12f, true},
    // Select/Start (center bottom)
    {emu::VirtualButton::Select, "SELECT", 0.38f, 0.75f, 0.10f, 0.06f, false},
    {emu::VirtualButton::Start,  "START",  0.52f, 0.75f, 0.10f, 0.06f, false},
    // B/A buttons (right side)
    {emu::VirtualButton::B,      "B",      0.75f, 0.50f, 0.10f, 0.14f, false},
    {emu::VirtualButton::A,      "A",      0.88f, 0.38f, 0.10f, 0.14f, false},
    // L/R shoulder buttons (top)
    {emu::VirtualButton::L,      "L",      0.08f, 0.05f, 0.15f, 0.08f, false},
    {emu::VirtualButton::R,      "R",      0.77f, 0.05f, 0.15f, 0.08f, false}
};

static const emu::ControllerLayoutInfo GBA_CONTROLLER_LAYOUT = {
    "GBA",
    "Game Boy Advance",
    emu::ControllerShape::Handheld,
    1.6f,  // Width is 1.6x height (handheld form factor)
    GBA_BUTTONS,
    10,    // 10 buttons (D-pad, A, B, L, R, Start, Select)
    1      // Single player
};


class GBAPlugin : public emu::IEmulatorPlugin {
public:
    GBAPlugin();
    ~GBAPlugin() override;

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

private:
    void run_gba_frame(const emu::InputState& input);

    // GBA components
    std::unique_ptr<ARM7TDMI> m_cpu;
    std::unique_ptr<Bus> m_bus;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Cartridge> m_cartridge;

    bool m_rom_loaded = false;
    uint32_t m_rom_crc32 = 0;
    uint64_t m_total_cycles = 0;
    uint64_t m_frame_count = 0;

    // Framebuffer - GBA is 240x160
    static constexpr int SCREEN_WIDTH = 240;
    static constexpr int SCREEN_HEIGHT = 160;
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

const char* GBAPlugin::s_extensions[] = { ".gba", ".GBA", nullptr };

GBAPlugin::GBAPlugin() {
    std::memset(m_framebuffer, 0, sizeof(m_framebuffer));
    std::memset(m_audio_buffer, 0, sizeof(m_audio_buffer));

    m_cartridge = std::make_unique<Cartridge>();
    m_apu = std::make_unique<APU>();
}

GBAPlugin::~GBAPlugin() = default;

emu::EmulatorInfo GBAPlugin::get_info() {
    emu::EmulatorInfo info;

    info.name = "GBA";
    info.version = "0.1.0";
    info.author = "Veloce Team";
    info.description = "Game Boy Advance emulator with ARM7TDMI CPU, full PPU rendering "
                       "supporting all video modes, and DMA-fed audio channels.";
    info.file_extensions = s_extensions;
    info.native_fps = 59.7275;  // 280896 cycles per frame at 16.78 MHz
    info.cycles_per_second = 16777216;  // 16.78 MHz
    info.screen_width = SCREEN_WIDTH;
    info.screen_height = SCREEN_HEIGHT;

    return info;
}

const emu::ControllerLayoutInfo* GBAPlugin::get_controller_layout() {
    return &GBA_CONTROLLER_LAYOUT;
}

bool GBAPlugin::load_rom(const uint8_t* data, size_t size) {
    if (is_debug_mode()) {
        printf("[GBA] Loading ROM: %zu bytes\n", size);
    }

    // Verify this is a GBA ROM
    if (size < 0xC0) {
        std::cerr << "ROM too small for GBA" << std::endl;
        return false;
    }

    // Check for GBA ROM header - entry point should be a branch instruction
    uint32_t entry = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    if ((entry & 0xFF000000) != 0xEA000000) {
        if (is_debug_mode()) {
            printf("[GBA] Warning: Entry point 0x%08X doesn't look like ARM branch\n", entry);
        }
        // Continue anyway - some homebrew ROMs may have different headers
    }

    // Load the cartridge
    if (!m_cartridge->load(data, size, SystemType::GameBoyAdvance)) {
        std::cerr << "Failed to load GBA ROM" << std::endl;
        return false;
    }

    // Set up GBA system
    m_bus = std::make_unique<Bus>();
    m_cpu = std::make_unique<ARM7TDMI>(*m_bus);
    m_ppu = std::make_unique<PPU>(*m_bus);

    m_bus->connect_cpu(m_cpu.get());
    m_bus->connect_ppu(m_ppu.get());
    m_bus->connect_apu(m_apu.get());
    m_bus->connect_cartridge(m_cartridge.get());

    m_apu->set_system_type(SystemType::GameBoyAdvance);

    // Set up Direct Sound FIFO DMA callback
    Bus* bus_ptr = m_bus.get();
    m_apu->set_fifo_dma_callback([bus_ptr](int fifo_idx) {
        bus_ptr->trigger_sound_fifo_dma(fifo_idx);
    });

    m_rom_loaded = true;
    m_rom_crc32 = m_cartridge->get_crc32();
    reset();

    if (is_debug_mode()) {
        printf("[GBA] ROM loaded successfully, CRC32: 0x%08X\n", m_rom_crc32);
    }
    std::cout << "GBA ROM loaded, CRC32: " << std::hex << m_rom_crc32 << std::dec << std::endl;
    return true;
}

void GBAPlugin::unload_rom() {
    m_cartridge->unload();
    m_rom_loaded = false;
    m_rom_crc32 = 0;
    m_total_cycles = 0;
    m_frame_count = 0;
    m_test_result_reported = false;

    m_cpu.reset();
    m_bus.reset();
    m_ppu.reset();
}

bool GBAPlugin::is_rom_loaded() const {
    return m_rom_loaded;
}

uint32_t GBAPlugin::get_rom_crc32() const {
    return m_rom_crc32;
}

void GBAPlugin::reset() {
    m_total_cycles = 0;
    m_frame_count = 0;
    m_audio_samples = 0;
    m_test_result_reported = false;

    if (m_cpu) m_cpu->reset();
    if (m_ppu) m_ppu->reset();
    if (m_apu) m_apu->reset();
}

void GBAPlugin::run_frame(const emu::InputState& input) {
    if (!m_rom_loaded) return;
    run_gba_frame(input);
    m_frame_count++;
}

void GBAPlugin::run_gba_frame(const emu::InputState& input) {
    // Set input state
    m_bus->set_input_state(input.buttons);

    // GBA: 280896 cycles per frame (228 scanlines * 1232 cycles)
    // 160 visible lines + 68 VBlank lines
    constexpr int CYCLES_PER_FRAME = 280896;
    int cycles_run = 0;

    static bool debug = is_debug_mode();
    int instr_count = 0;
    while (cycles_run < CYCLES_PER_FRAME) {
        int cpu_cycles = m_cpu->step();
        instr_count++;

        // Run DMA after CPU step - DMA halts CPU while active
        int dma_cycles = m_bus->run_dma();

        // Total cycles for this iteration
        int total_cycles = cpu_cycles + dma_cycles;
        m_total_cycles += total_cycles;
        cycles_run += total_cycles;

        // Step PPU
        m_ppu->step(total_cycles);

        // Step timers
        m_bus->step_timers(total_cycles);

        // Step APU
        m_apu->step(total_cycles);

        // Handle interrupts
        if (m_bus->check_interrupts()) {
            m_cpu->signal_irq();
        }
    }

    // Log instruction count per frame
    if (debug && (m_frame_count + 1) % 60 == 0) {
        fprintf(stderr, "[FRAME] %llu: %d instructions, %d cycles\n",
               m_frame_count + 1, instr_count, cycles_run);
    }

    // Copy framebuffer
    const uint32_t* ppu_fb = m_ppu->get_framebuffer();
    std::memcpy(m_framebuffer, ppu_fb, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

    // Get audio samples
    m_audio_samples = m_apu->get_samples(m_audio_buffer, AUDIO_BUFFER_SIZE);

    // Test ROM result detection (frame-based)
    if (is_debug_mode() && !m_test_result_reported) {
        static uint32_t last_frame_pc = 0;
        static int same_pc_frames = 0;

        uint32_t current_pc = m_cpu->get_pc();

        if (current_pc == last_frame_pc) {
            same_pc_frames++;
            // If PC has been the same for 10 frames (~170ms), consider test complete
            if (same_pc_frames >= 10) {
                uint32_t r12 = m_cpu->get_register(12);
                m_test_result_reported = true;

                // jsmolka/alyosha gba-tests' shared m_test_eval macro
                // (lib/macros.inc) is `stmfd sp!,{r0-r12} / movs r12,rX /
                // ... / ldmfd sp!,{r0-r12}` followed by `idle: b idle`.
                // The ldmfd restores r12 to whatever it held *before* the
                // eval, so r12 at the idle loop is the verdict only when
                // the ROM evaluates r12 itself (`m_test_eval r12`); ROMs
                // that evaluate another register (thumb.gba: r7, the LDM
                // suite: r0 after a mode switch, start_up_vbl_irq*: r7)
                // leave an unrelated r12 behind, which was reported as a
                // false PASS (gba-01). The same ldmfd restores rX to its
                // pre-eval value -- which *is* the evaluated result, since
                // `movs r12,rX` does not modify rX -- so rX at the idle
                // loop is exactly what the macro tested.
                //
                // Recognise the epilogue near the stable PC (get_pc() may
                // be the pipeline PC, idle..idle+8), walk back to the
                // macro's stmfd/movs pair to learn X, and read rX. If the
                // pattern is absent (nba-hw-test, FuzzARM, non-macro
                // ROMs) the old R12 convention is kept unchanged.
                //
                // Review follow-up: the first version of this fix instead
                // trusted the .failed path's decimal digits at IWRAM
                // 0x03000000/4/8 whenever they looked like digits. A
                // passing run never writes them, so they hold whatever the
                // test left there -- memory.gba's IWRAM-mirror test stores
                // 1 at 0x03000000 ("Failed at test #100" on a pass), and
                // the prefetcher_boundary ROMs put the stack at IWRAM+16 so
                // the stmfd itself lands r9-r11 there.
                int eval_reg = -1;
                if (!(m_cpu->get_cpsr() & 0x20)) {  // macro epilogue is ARM code
                    uint32_t base = current_pc & ~3u;
                    uint32_t idle = 0;
                    for (int off = -8; off <= 8 && !idle; off += 4) {
                        uint32_t a = base + static_cast<uint32_t>(off);
                        if (a >= 0x100 &&
                            m_bus->read32(a) == 0xEAFFFFFEu &&        // b .
                            m_bus->read32(a - 4) == 0xE8BD1FFFu) {    // ldmfd sp!,{r0-r12}
                            idle = a;
                        }
                    }
                    if (idle) {
                        // The macro body (two text paths) is ~100 ARM
                        // instructions; 2 KiB is ample and cannot reach
                        // another m_test_eval (each ROM has exactly one).
                        for (uint32_t p = idle - 8; p + 0x800 >= idle && p >= 0x100; p -= 4) {
                            uint32_t w0 = m_bus->read32(p);
                            uint32_t w1 = m_bus->read32(p + 4);
                            if (w0 == 0xE92D1FFFu &&                   // stmfd sp!,{r0-r12}
                                (w1 & 0xFFFFFFF0u) == 0xE1B0C000u) {   // movs r12, rX
                                eval_reg = static_cast<int>(w1 & 0xF);
                                break;
                            }
                        }
                    }
                }

                uint32_t verdict = r12;
                if (eval_reg >= 0) {
                    verdict = m_cpu->get_register(eval_reg);
                }

                fprintf(stderr, "\n=== GBA TEST ROM RESULT ===\n");
                fprintf(stderr, "Detected stable PC at 0x%08X for %d frames\n", current_pc, same_pc_frames);
                fprintf(stderr, "R12 (test result): %u\n", r12);
                if (eval_reg >= 0) {
                    fprintf(stderr, "m_test_eval register: r%d = %u (verdict source)\n", eval_reg, verdict);
                } else {
                    fprintf(stderr, "m_test_eval epilogue not found; verdict source: R12\n");
                }
                fprintf(stderr, "Cycles: %llu, Frame: %llu\n",
                       static_cast<unsigned long long>(m_total_cycles),
                       static_cast<unsigned long long>(m_frame_count + 1));

                if (verdict == 0) {
                    fprintf(stderr, "[GBA] PASSED - All tests completed successfully\n");
                } else {
                    fprintf(stderr, "[GBA] FAILED - Failed at test #%u\n", verdict);
                }
                fprintf(stderr, "===========================\n");
                fflush(stderr);  // Ensure output is flushed immediately for test detection
            }
        } else {
            same_pc_frames = 0;
            last_frame_pc = current_pc;
        }
    }

    // Debug logging: frame count
    if (is_debug_mode()) {
        uint32_t pc = m_cpu->get_pc();
        uint32_t cpsr = m_cpu->get_cpsr();
        static uint64_t last_cycles = 0;

        // Print PC every 10 frames
        if ((m_frame_count + 1) % 10 == 0 || m_frame_count < 20) {
            fprintf(stderr, "[GBA] Frame %llu, PC: 0x%08X, CPSR: 0x%08X\n",
                    static_cast<unsigned long long>(m_frame_count + 1), pc, cpsr);
        }
        if ((m_frame_count + 1) % 60 == 0) {
            uint64_t cycles_this_frame = m_total_cycles - last_cycles;
            fprintf(stderr, "[GBA] Frame %llu, cycles: %llu (delta=%llu), PC: 0x%08X\n",
                   static_cast<unsigned long long>(m_frame_count + 1),
                   static_cast<unsigned long long>(m_total_cycles),
                   static_cast<unsigned long long>(cycles_this_frame),
                   pc);
            last_cycles = m_total_cycles;
        }
    }
}

uint64_t GBAPlugin::get_cycle_count() const {
    return m_total_cycles;
}

uint64_t GBAPlugin::get_frame_count() const {
    return m_frame_count;
}

emu::FrameBuffer GBAPlugin::get_framebuffer() {
    return {
        m_framebuffer,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    };
}

emu::AudioBuffer GBAPlugin::get_audio() {
    return {
        m_audio_buffer,
        static_cast<int>(m_audio_samples),
        44100
    };
}

void GBAPlugin::clear_audio_buffer() {
    m_audio_samples = 0;
}

void GBAPlugin::set_audio_callback(AudioStreamCallback callback) {
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

uint8_t GBAPlugin::read_memory(uint16_t address) {
    // For GBA, address is only 16 bits in the interface, so we read from IWRAM/IO
    return m_bus ? m_bus->read8(0x03000000 | address) : 0;
}

void GBAPlugin::write_memory(uint16_t address, uint8_t value) {
    if (m_bus) m_bus->write8(0x03000000 | address, value);
}

bool GBAPlugin::save_state(std::vector<uint8_t>& data) {
    if (!m_rom_loaded) return false;

    try {
        data.clear();
        data.reserve(64 * 1024);  // Reserve 64KB

        // Save frame count and cycles
        const uint8_t* fc_ptr = reinterpret_cast<const uint8_t*>(&m_frame_count);
        data.insert(data.end(), fc_ptr, fc_ptr + sizeof(m_frame_count));

        const uint8_t* tc_ptr = reinterpret_cast<const uint8_t*>(&m_total_cycles);
        data.insert(data.end(), tc_ptr, tc_ptr + sizeof(m_total_cycles));

        m_cpu->save_state(data);
        m_ppu->save_state(data);
        m_bus->save_state(data);
        m_apu->save_state(data);
        m_cartridge->save_state(data);

        return true;
    } catch (...) {
        return false;
    }
}

bool GBAPlugin::load_state(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || data.empty()) return false;

    // Snapshot the current, known-good machine state first. Every
    // component's load_state() is bounds-checked (state_read/state_read_bytes)
    // so a truncated or corrupted `data` can no longer read past its own end,
    // but a failure partway through still leaves whatever fields were read
    // before the failure applied. Roll back to this snapshot in that case so
    // a rejected load never leaves the machine half-updated. save_state()
    // never fails once a ROM is loaded, so `backup` is always well-formed and
    // its own load_state() replay below cannot itself fail.
    std::vector<uint8_t> backup;
    if (!save_state(backup)) return false;

    auto do_load = [&](const std::vector<uint8_t>& src) -> bool {
        const uint8_t* ptr = src.data();
        size_t remaining = src.size();

        if (!state_read(ptr, remaining, m_frame_count)) return false;
        if (!state_read(ptr, remaining, m_total_cycles)) return false;

        return m_cpu->load_state(ptr, remaining) &&
               m_ppu->load_state(ptr, remaining) &&
               m_bus->load_state(ptr, remaining) &&
               m_apu->load_state(ptr, remaining) &&
               m_cartridge->load_state(ptr, remaining);
    };

    bool ok;
    try {
        ok = do_load(data);
    } catch (...) {
        ok = false;
    }

    if (!ok) {
        do_load(backup);
        return false;
    }

    return true;
}

bool GBAPlugin::has_battery_save() const {
    return m_rom_loaded && m_cartridge && m_cartridge->has_battery();
}

std::vector<uint8_t> GBAPlugin::get_battery_save_data() const {
    if (!m_rom_loaded || !m_cartridge) {
        return {};
    }
    return m_cartridge->get_save_data();
}

bool GBAPlugin::set_battery_save_data(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || !m_cartridge) {
        return false;
    }
    return m_cartridge->set_save_data(data);
}

} // namespace gba

// C interface for plugin loading
extern "C" {

EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() {
    return new gba::GBAPlugin();
}

EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() {
    return EMU_PLUGIN_API_VERSION;
}

}
