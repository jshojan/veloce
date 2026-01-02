#pragma once

#include "mapper.hpp"
#include <array>

namespace nes {

// Mapper 020: Famicom Disk System (FDS)
// The FDS is a disk drive add-on for the Famicom that uses:
// - 32KB PRG RAM (at $6000-$DFFF)
// - 8KB CHR RAM
// - RAM Adapter with custom audio (wavetable synthesis)
// - Disk drive with 65,500 bytes per side
//
// Memory map:
// $4020-$4FFF: Disk I/O registers
// $6000-$DFFF: 32KB PRG RAM (disk data loaded here)
// $E000-$FFFF: 8KB PRG RAM (mirrored or BIOS)
//
// Note: This is a simplified implementation. Full FDS emulation
// requires accurate disk timing and the FDS BIOS.
class Mapper020 : public Mapper {
public:
    Mapper020(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode initial_mirror,
              bool has_chr_ram);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;
    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;
    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    bool irq_pending(uint32_t frame_cycle = 0) override;
    void irq_clear() override;
    // PERFORMANCE: Batched version - receives cycle count for efficient processing
    void cpu_cycles(int count) override;
    void cpu_cycle() override;
    float get_audio_output() const override;

    void reset() override;
    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

    // Disk operations
    void set_disk_data(const std::vector<uint8_t>& disk_data);
    void insert_disk(int side);  // 0 = side A, 1 = side B
    void eject_disk();
    bool is_disk_inserted() const { return m_disk_inserted; }

private:
    // PRG RAM (32KB main + 8KB BIOS area)
    std::array<uint8_t, 32768> m_prg_ram_main;
    std::array<uint8_t, 8192> m_prg_ram_bios;  // $E000-$FFFF

    // CHR RAM (8KB)
    std::array<uint8_t, 8192> m_chr_ram;

    // Disk data (up to 65,500 bytes per side, 2 sides max)
    std::vector<uint8_t> m_disk_data;
    bool m_disk_inserted = false;
    int m_current_side = 0;

    // Disk registers
    uint16_t m_irq_reload = 0;
    uint16_t m_irq_counter = 0;
    bool m_irq_enabled = false;
    bool m_irq_repeat = false;
    bool m_irq_pending = false;

    // Disk drive state
    bool m_disk_ready = false;
    bool m_motor_on = false;
    bool m_transfer_reset = false;
    bool m_read_mode = true;
    bool m_crc_control = false;
    uint16_t m_disk_position = 0;
    uint8_t m_data_read = 0;
    uint8_t m_data_write = 0;
    bool m_byte_transfer = false;
    uint8_t m_ext_connector = 0;

    // Audio (simplified wavetable synthesis)
    // The FDS has a wavetable channel with 64 samples
    std::array<uint8_t, 64> m_wave_table;
    uint16_t m_wave_freq = 0;
    uint8_t m_wave_volume = 0;
    uint8_t m_wave_pos = 0;
    uint32_t m_wave_accum = 0;
    bool m_wave_enabled = false;
    bool m_wave_write_enabled = false;

    // Modulation unit
    std::array<int8_t, 32> m_mod_table;
    uint16_t m_mod_freq = 0;
    uint8_t m_mod_pos = 0;
    uint32_t m_mod_accum = 0;
    int16_t m_mod_counter = 0;
    uint8_t m_mod_gain = 0;
    bool m_mod_enabled = false;

    // Master volume and envelope
    uint8_t m_master_volume = 0;
    uint8_t m_env_speed = 0;
    bool m_env_enabled = false;
};

} // namespace nes
