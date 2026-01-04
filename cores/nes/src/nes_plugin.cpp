#include "emu/emulator_plugin.hpp"
#include "emu/netplay_plugin.hpp"
#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"

#include <imgui.h>
#include <cstring>
#include <iostream>
#include <fstream>

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

    // =========================================================================
    // Configuration GUI
    // =========================================================================

    // Check if core requests fast/uncapped mode
    bool is_fast_mode_enabled() const override { return m_fast_mode; }

    // Configuration GUI support
    bool has_config_gui() const override { return true; }
    void set_imgui_context(void* context) override;
    void render_config_gui(bool& visible) override;
    void render_config_gui_content() override;
    const char* get_config_window_name() const override { return "NES Settings"; }

    // Configuration persistence
    bool save_config(const char* path) override;
    bool load_config(const char* path) override;

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

    // Configuration options
    bool m_fast_mode = false;           // Run at uncapped speed when true
    bool m_disable_sprite_limit = false; // Allow >8 sprites per scanline when true
    bool m_crop_overscan = false;        // Hide top/bottom 8 rows (typically hidden on CRT TVs)

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

    // Set controller state BEFORE running the frame
    // This ensures NMI handlers can read the current input
    m_bus->set_controller_state(0, player1_buttons);
    m_bus->set_controller_state(1, player2_buttons);

    // Run until PPU signals frame completion (at VBlank start)
    // With cycle-accurate mode, PPU and APU are ticked during each CPU memory access
    // via the Bus, but NMI detection happens at instruction boundaries here.
    bool frame_complete = false;

    while (!frame_complete) {
        // Handle OAM DMA inline if active
        // DMA ticks PPU/APU for each cycle via the bus
        // During DMA, interrupts are detected but not serviced until DMA completes
        while (m_bus->is_dma_active()) {
            m_bus->run_dma_cycle();
            m_total_cycles++;

            // Check for NMI during DMA - it will be pending when DMA completes
            // The bus tick() already triggers NMI detection via check_nmi()
            // but we also need to poll for IRQ changes
            bool mapper_irq = m_bus->mapper_irq_pending(m_ppu->get_frame_cycle());
            bool apu_irq = m_apu->irq_pending();
            m_cpu->set_irq_line(mapper_irq || apu_irq);

            // Check for frame completion during DMA
            if (m_ppu->check_frame_complete()) {
                frame_complete = true;
            }
        }

        if (frame_complete) break;

        // Step CPU - memory accesses tick PPU/APU via the bus
        int cpu_cycles = m_cpu->step();
        m_total_cycles += cpu_cycles;

        // Check for NMI at instruction boundary (proper NMI timing)
        int nmi_type = m_ppu->check_nmi();
        if (nmi_type == 1) {
            m_cpu->trigger_nmi();
        } else if (nmi_type == 2) {
            m_cpu->trigger_nmi_delayed();
        }

        // Check for mapper IRQ and APU IRQ at instruction boundary
        bool mapper_irq = m_bus->mapper_irq_pending(m_ppu->get_frame_cycle());
        bool apu_irq = m_apu->irq_pending();
        m_cpu->set_irq_line(mapper_irq || apu_irq);

        // Check for frame completion
        if (m_ppu->check_frame_complete()) {
            frame_complete = true;
            // Check for test ROM output once per frame
            static int check_interval = 0;
            if (++check_interval >= 30) {
                check_interval = 0;
                m_bus->check_test_output();
            }
        }

        // Get expansion audio from mapper and pass to APU for mixing
        float expansion_audio = m_bus->get_mapper_audio();
        m_apu->set_expansion_audio(expansion_audio);
    }

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

void NESPlugin::set_audio_callback(AudioStreamCallback callback) {
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

uint8_t NESPlugin::read_memory(uint16_t address) {
    // Use peek to avoid side effects (ticking PPU/APU) for debugging
    return m_bus->cpu_peek(address);
}

void NESPlugin::write_memory(uint16_t address, uint8_t value) {
    // Note: This will tick PPU/APU, which may have side effects during debugging
    // Consider adding a cpu_poke function if this causes issues
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

    // Hash RAM through bus peeks (first 256 bytes as quick check)
    // For full sync verification, consider hashing all 2KB
    // Use cpu_peek to avoid side effects (ticking PPU/APU)
    uint8_t ram_sample[256];
    for (int i = 0; i < 256; i++) {
        ram_sample[i] = m_bus->cpu_peek(static_cast<uint16_t>(i));
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

// =============================================================================
// Configuration GUI Implementation
// =============================================================================

void NESPlugin::set_imgui_context(void* context) {
    // Set the ImGui context for this plugin
    // This is required because the plugin may be a separate shared library
    // with its own statically-linked ImGui, which would have a different context
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
}

void NESPlugin::render_config_gui(bool& visible) {
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("NES Settings", &visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    render_config_gui_content();

    ImGui::End();
}

void NESPlugin::render_config_gui_content() {
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
                "(60.0988 FPS for NTSC) to match real NES hardware timing."
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

    // Video / Graphics Section
    if (ImGui::CollapsingHeader("Video / Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Sprite limit disable option
        if (ImGui::Checkbox("Disable Sprite Limit", &m_disable_sprite_limit)) {
            // Update PPU when option changes
            if (m_ppu) {
                m_ppu->set_sprite_limit_enabled(!m_disable_sprite_limit);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(
                "The NES hardware can only display 8 sprites per scanline. "
                "When this limit is reached, sprites flicker.\n\n"
                "Disabling this limit shows all sprites but is NOT accurate "
                "to real hardware. Some games use sprite priority for effects "
                "that may look wrong with this enabled."
            );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // Overscan crop option
        if (ImGui::Checkbox("Crop Overscan", &m_crop_overscan)) {
            // Update PPU when option changes
            if (m_ppu) {
                m_ppu->set_crop_overscan(m_crop_overscan);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(
                "CRT TVs typically hide the top and bottom 8 rows of the NES "
                "display (scanlines 0-7 and 232-239).\n\n"
                "Games often have garbage or debug info in these areas. "
                "Enable this to hide overscan like a real TV would."
            );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    // System Information Section
    if (ImGui::CollapsingHeader("System Information")) {
        if (m_rom_loaded && m_cartridge) {
            ImGui::Text("ROM CRC32: %08X", m_rom_crc32);
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No ROM loaded");
        }
        ImGui::Text("Frame: %llu", static_cast<unsigned long long>(m_frame_count));
        ImGui::Text("Cycles: %llu", static_cast<unsigned long long>(m_total_cycles));
    }
}

bool NESPlugin::save_config(const char* path) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    // Simple JSON format
    file << "{\n";
    file << "  \"fast_mode\": " << (m_fast_mode ? "true" : "false") << ",\n";
    file << "  \"disable_sprite_limit\": " << (m_disable_sprite_limit ? "true" : "false") << ",\n";
    file << "  \"crop_overscan\": " << (m_crop_overscan ? "true" : "false") << "\n";
    file << "}\n";

    return true;
}

bool NESPlugin::load_config(const char* path) {
    std::ifstream file(path);
    if (!file) {
        // File doesn't exist - use defaults (not an error)
        return true;
    }

    // Simple JSON parsing
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Parse fast_mode
    size_t pos = content.find("\"fast_mode\":");
    if (pos != std::string::npos) {
        size_t true_pos = content.find("true", pos);
        size_t false_pos = content.find("false", pos);
        m_fast_mode = (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos));
    }

    // Parse disable_sprite_limit
    pos = content.find("\"disable_sprite_limit\":");
    if (pos != std::string::npos) {
        size_t true_pos = content.find("true", pos);
        size_t false_pos = content.find("false", pos);
        m_disable_sprite_limit = (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos));
    }

    // Parse crop_overscan
    pos = content.find("\"crop_overscan\":");
    if (pos != std::string::npos) {
        size_t true_pos = content.find("true", pos);
        size_t false_pos = content.find("false", pos);
        m_crop_overscan = (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos));
    }

    // Apply loaded settings to PPU if it exists
    if (m_ppu) {
        m_ppu->set_sprite_limit_enabled(!m_disable_sprite_limit);
        m_ppu->set_crop_overscan(m_crop_overscan);
    }

    return true;
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
