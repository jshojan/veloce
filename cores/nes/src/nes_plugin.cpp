#include "emu/emulator_plugin.hpp"
#include "emu/netplay_plugin.hpp"
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

class NESPlugin : public emu::IEmulatorPlugin, public emu::INetplayCapable {
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

    // Battery-backed save support
    bool has_battery_save() const override;
    std::vector<uint8_t> get_battery_save_data() const override;
    bool set_battery_save_data(const std::vector<uint8_t>& data) override;

    // =========================================================================
    // INetplayCapable implementation - Netplay/Rollback support
    // =========================================================================

    // The NES emulator core is fully deterministic:
    // - All integer arithmetic in CPU/PPU/APU
    // - No random number generation
    // - Audio uses floats only for output mixing, not core emulation state
    bool is_deterministic() const override { return true; }

    // Run frame with explicit input for both players (for netplay)
    void run_frame_netplay(uint32_t player1_buttons, uint32_t player2_buttons) override;

    // N-player netplay variant - delegates to 2-player version for NES
    void run_frame_netplay_n(const std::vector<uint32_t>& player_inputs) override {
        uint32_t p1 = player_inputs.size() > 0 ? player_inputs[0] : 0;
        uint32_t p2 = player_inputs.size() > 1 ? player_inputs[1] : 0;
        run_frame_netplay(p1, p2);
    }

    // Maximum players supported (NES supports 2 standard controllers)
    int get_max_players() const override { return 2; }

    // Fast save state for rollback - writes directly to buffer, no allocations
    size_t get_max_state_size() const override;
    size_t save_state_fast(uint8_t* buffer, size_t buffer_size) override;
    bool load_state_fast(const uint8_t* buffer, size_t size) override;

    // State hash for desync detection
    uint64_t get_state_hash() const override;

    // Discard audio during rollback re-simulation
    void discard_audio() override { m_audio_samples = 0; }

private:
    // Internal run_frame that takes both player inputs
    void run_frame_internal(uint32_t player1_buttons, uint32_t player2_buttons);

    // Fast serialization helpers (write directly to buffer)
    size_t serialize_state(uint8_t* buffer) const;
    bool deserialize_state(const uint8_t* buffer, size_t size);

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
    info.author = "Veloce Team";
    info.description = "Cycle-accurate NES/Famicom emulator with dot-by-dot PPU rendering. "
                       "Supports 20+ mappers covering ~90% of the NES library including "
                       "MMC1, MMC3, VRC, and Sunsoft FME-7.";
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
    // Standard single-player run_frame - routes player 1 input only
    // For multiplayer, use run_frame_netplay() or run_frame_internal()
    run_frame_internal(input.buttons, 0);
}

void NESPlugin::run_frame_netplay(uint32_t player1_buttons, uint32_t player2_buttons) {
    // Netplay version - accepts input for both players
    // Input is already in NES format (A, B, Select, Start, Up, Down, Left, Right)
    run_frame_internal(player1_buttons, player2_buttons);
}

void NESPlugin::run_frame_internal(uint32_t player1_buttons, uint32_t player2_buttons) {
    if (!m_rom_loaded) return;

    // Run until PPU signals frame completion (at VBlank start)
    bool frame_complete = false;

    while (!frame_complete) {
        // Step CPU
        int cpu_cycles = m_cpu->step();
        int base_cpu_cycles = cpu_cycles;  // Save before adding DMA
        m_total_cycles += cpu_cycles;

        // Check for OAM DMA (takes 513-514 CPU cycles)
        int dma_cycles = m_bus->get_pending_dma_cycles();
        if (dma_cycles > 0) {
            cpu_cycles += dma_cycles;
            m_total_cycles += dma_cycles;
        }

        // Check for DMC DMA (takes 1-4 CPU cycles per sample byte)
        int dmc_dma_cycles = m_apu->get_dmc_dma_cycles();
        if (dmc_dma_cycles > 0) {
            cpu_cycles += dmc_dma_cycles;
            m_total_cycles += dmc_dma_cycles;
        }

        // Step PPU (3 PPU cycles per CPU cycle)
        // Run all PPU cycles first, then check for NMI/frame completion ONCE
        // This is a critical performance optimization - checking inside the loop
        // was causing 89,000+ redundant checks per frame!
        int ppu_cycles = cpu_cycles * 3;
        for (int i = 0; i < ppu_cycles; i++) {
            m_ppu->step();
        }

        // Check for NMI ONCE after all PPU cycles for this CPU instruction
        // NMI timing is still accurate because it's latched inside PPU::step()
        int nmi_type = m_ppu->check_nmi();
        if (nmi_type == 1) {
            m_cpu->trigger_nmi();
        } else if (nmi_type == 2) {
            m_cpu->trigger_nmi_delayed();
        }

        // Check for frame completion ONCE after PPU batch
        if (m_ppu->check_frame_complete()) {
            frame_complete = true;
            // Check for test ROM output once per frame
            static int check_interval = 0;
            if (++check_interval >= 30) {
                check_interval = 0;
                m_bus->check_test_output();
            }
        }

        // Check for mapper IRQ and APU IRQ once per CPU instruction
        // The IRQ line is level-triggered, so we just need to check the current state
        bool mapper_irq = m_bus->mapper_irq_pending(m_ppu->get_frame_cycle());
        bool apu_irq = m_apu->irq_pending();
        m_cpu->set_irq_line(mapper_irq || apu_irq);

        // Clock mapper for IRQ counters and expansion audio
        // Only clock for the actual CPU instruction cycles, NOT DMA cycles
        // DMA happens on the bus and doesn't clock mapper internals the same way
        // PERFORMANCE: Pass cycle count to mapper for batched processing instead of
        // calling once per cycle (~90,000 calls/frame reduced to ~30,000)
        m_bus->mapper_cpu_cycles(base_cpu_cycles);

        // Get expansion audio from mapper and pass to APU for mixing
        float expansion_audio = m_bus->get_mapper_audio();
        m_apu->set_expansion_audio(expansion_audio);

        // Step APU
        m_apu->step(cpu_cycles);
    }

    // Set controller state for BOTH players for NEXT frame's NMI to read
    // This must happen after the loop so the pending NMI (triggered at VBlank)
    // will read this input when it runs at the start of the next run_frame
    m_bus->set_controller_state(0, player1_buttons);
    m_bus->set_controller_state(1, player2_buttons);

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

bool NESPlugin::has_battery_save() const {
    return m_rom_loaded && m_cartridge && m_cartridge->has_battery();
}

std::vector<uint8_t> NESPlugin::get_battery_save_data() const {
    if (!m_rom_loaded || !m_cartridge) {
        return {};
    }
    return m_cartridge->get_save_data();
}

bool NESPlugin::set_battery_save_data(const std::vector<uint8_t>& data) {
    if (!m_rom_loaded || !m_cartridge) {
        return false;
    }
    return m_cartridge->set_save_data(data);
}

// =============================================================================
// INetplayCapable Implementation - Fast Save State for Rollback
// =============================================================================

// Maximum state size estimation:
// - Frame count + cycles: 16 bytes
// - CPU state: ~20 bytes (registers, flags)
// - PPU state: ~4KB (OAM, nametables, palette, registers, shifters)
// - APU state: ~500 bytes (channels, counters)
// - Bus state: ~2KB (RAM) + controller state
// - Cartridge state: ~64KB (PRG RAM, CHR RAM, mapper state)
// Total: ~70KB conservative estimate, use 128KB to be safe
static constexpr size_t MAX_NES_STATE_SIZE = 128 * 1024;

size_t NESPlugin::get_max_state_size() const {
    return MAX_NES_STATE_SIZE;
}

size_t NESPlugin::save_state_fast(uint8_t* buffer, size_t buffer_size) {
    if (!m_rom_loaded) return 0;

    // If buffer is null, just return required size
    if (buffer == nullptr) {
        return get_max_state_size();
    }

    if (buffer_size < get_max_state_size()) {
        return 0;  // Buffer too small
    }

    // Use vector-based save_state and copy to buffer
    // This is simple and correct; optimization can come later if profiling shows need
    std::vector<uint8_t> state_data;
    if (!save_state(state_data)) {
        return 0;
    }

    if (state_data.size() > buffer_size) {
        return 0;  // Shouldn't happen with proper max size estimation
    }

    std::memcpy(buffer, state_data.data(), state_data.size());
    return state_data.size();
}

bool NESPlugin::load_state_fast(const uint8_t* buffer, size_t size) {
    if (!m_rom_loaded || buffer == nullptr || size == 0) {
        return false;
    }

    // Use vector-based load_state
    // This is simple and correct; optimization can come later if profiling shows need
    std::vector<uint8_t> state_data(buffer, buffer + size);
    return load_state(state_data);
}

// =============================================================================
// INetplayCapable Implementation - State Hash for Desync Detection
// =============================================================================

// FNV-1a hash implementation for state hashing
// This is a fast, non-cryptographic hash suitable for desync detection
static uint64_t fnv1a_hash(const uint8_t* data, size_t size) {
    const uint64_t FNV_PRIME = 0x100000001b3ULL;
    const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t NESPlugin::get_state_hash() const {
    if (!m_rom_loaded) return 0;

    // Hash the critical emulation state for desync detection
    // We want to hash:
    // - CPU registers
    // - PPU state (excluding framebuffer which is output-only)
    // - APU state (excluding audio buffer)
    // - RAM
    // - Controller state

    // For efficiency, we hash a quick snapshot of the most critical state
    // A full state hash would be more thorough but slower

    uint64_t hash = 0;

    // Hash frame count and cycle count
    hash ^= fnv1a_hash(reinterpret_cast<const uint8_t*>(&m_frame_count), sizeof(m_frame_count));
    hash ^= fnv1a_hash(reinterpret_cast<const uint8_t*>(&m_total_cycles), sizeof(m_total_cycles));

    // Hash CPU registers
    uint8_t cpu_state[8];
    cpu_state[0] = static_cast<uint8_t>(m_cpu->get_pc() & 0xFF);
    cpu_state[1] = static_cast<uint8_t>(m_cpu->get_pc() >> 8);
    cpu_state[2] = m_cpu->get_a();
    cpu_state[3] = m_cpu->get_x();
    cpu_state[4] = m_cpu->get_y();
    cpu_state[5] = m_cpu->get_sp();
    cpu_state[6] = m_cpu->get_status();
    cpu_state[7] = 0;  // Padding
    hash ^= fnv1a_hash(cpu_state, sizeof(cpu_state));

    // Hash RAM through bus reads (first 256 bytes as quick check)
    // For full sync verification, consider hashing all 2KB
    uint8_t ram_sample[256];
    for (int i = 0; i < 256; i++) {
        ram_sample[i] = m_bus->cpu_read(static_cast<uint16_t>(i));
    }
    hash ^= fnv1a_hash(ram_sample, sizeof(ram_sample));

    // Hash some critical PPU state by reading a few key memory locations
    uint8_t ppu_sample[32];
    for (int i = 0; i < 32; i++) {
        // Read from PPU palette memory (most likely to differ on desync)
        ppu_sample[i] = m_bus->ppu_read(0x3F00 + i, 0);
    }
    hash ^= fnv1a_hash(ppu_sample, sizeof(ppu_sample));

    return hash;
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
