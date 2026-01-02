#include "mapper_020.hpp"
#include <cstring>
#include <cmath>

namespace nes {

Mapper020::Mapper020(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode initial_mirror,
                     bool has_chr_ram)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = initial_mirror;
    m_has_chr_ram = true;  // FDS always uses CHR RAM

    m_prg_ram_main.fill(0);
    m_prg_ram_bios.fill(0);
    m_chr_ram.fill(0);
    m_wave_table.fill(0);
    m_mod_table.fill(0);

    // If PRG ROM data is provided, copy it to the BIOS area
    // (FDS ROMs typically have the BIOS prepended)
    if (!prg_rom.empty()) {
        size_t copy_size = std::min(prg_rom.size(), m_prg_ram_bios.size());
        std::memcpy(m_prg_ram_bios.data(), prg_rom.data(), copy_size);
    }

    reset();
}

void Mapper020::reset() {
    m_irq_reload = 0;
    m_irq_counter = 0;
    m_irq_enabled = false;
    m_irq_repeat = false;
    m_irq_pending = false;

    m_disk_ready = m_disk_inserted;
    m_motor_on = false;
    m_transfer_reset = false;
    m_read_mode = true;
    m_crc_control = false;
    m_disk_position = 0;
    m_data_read = 0;
    m_data_write = 0;
    m_byte_transfer = false;
    m_ext_connector = 0;

    m_wave_freq = 0;
    m_wave_volume = 0;
    m_wave_pos = 0;
    m_wave_accum = 0;
    m_wave_enabled = false;
    m_wave_write_enabled = false;

    m_mod_freq = 0;
    m_mod_pos = 0;
    m_mod_accum = 0;
    m_mod_counter = 0;
    m_mod_gain = 0;
    m_mod_enabled = false;

    m_master_volume = 0;
    m_env_speed = 0xE8;
    m_env_enabled = false;
}

uint8_t Mapper020::cpu_read(uint16_t address) {
    if (address >= 0x4030 && address <= 0x4092) {
        // Disk I/O and audio registers
        switch (address) {
            case 0x4030: {
                // Disk Status Register
                uint8_t status = 0;
                if (m_irq_pending) status |= 0x01;
                if (m_byte_transfer) status |= 0x02;
                // Bits 4-6: battery status, etc.
                status |= 0x80;  // Disk is spinning (if motor on)
                m_irq_pending = false;
                m_byte_transfer = false;
                return status;
            }
            case 0x4031:
                // Read data from disk
                return m_data_read;
            case 0x4032: {
                // Drive Status
                uint8_t status = 0;
                if (!m_disk_inserted) status |= 0x01;  // No disk
                if (!m_disk_ready) status |= 0x02;     // Not ready
                if (!m_disk_ready) status |= 0x04;     // Not writable (when not ready)
                return status;
            }
            case 0x4033:
                // External connector read
                return m_ext_connector | 0x80;  // Battery good
            case 0x4090:
                // Volume envelope output
                return m_wave_volume;
            case 0x4092:
                // Mod envelope output
                return m_mod_gain;
        }
    }

    if (address >= 0x6000 && address <= 0xDFFF) {
        // Main PRG RAM (32KB)
        return m_prg_ram_main[address - 0x6000];
    }

    if (address >= 0xE000) {
        // BIOS area or mirrored PRG RAM
        return m_prg_ram_bios[address - 0xE000];
    }

    return 0;
}

void Mapper020::cpu_write(uint16_t address, uint8_t value) {
    if (address >= 0x4020 && address <= 0x4092) {
        switch (address) {
            case 0x4020:
                // IRQ reload value low
                m_irq_reload = (m_irq_reload & 0xFF00) | value;
                break;
            case 0x4021:
                // IRQ reload value high
                m_irq_reload = (m_irq_reload & 0x00FF) | (static_cast<uint16_t>(value) << 8);
                break;
            case 0x4022:
                // IRQ control
                m_irq_enabled = (value & 0x02) != 0;
                m_irq_repeat = (value & 0x01) != 0;
                if (m_irq_enabled) {
                    m_irq_counter = m_irq_reload;
                }
                m_irq_pending = false;
                break;
            case 0x4023:
                // I/O enable
                // Bit 0: enable disk I/O
                // Bit 1: enable sound I/O
                break;
            case 0x4024:
                // Write data to disk
                m_data_write = value;
                m_byte_transfer = false;
                break;
            case 0x4025:
                // Control register
                m_motor_on = (value & 0x01) != 0;
                m_transfer_reset = (value & 0x02) != 0;
                m_read_mode = (value & 0x04) != 0;
                m_mirror_mode = (value & 0x08) ? MirrorMode::Horizontal : MirrorMode::Vertical;
                m_crc_control = (value & 0x10) != 0;
                // Bit 6: disk ready
                m_disk_ready = m_disk_inserted && ((value & 0x40) == 0);
                // Bit 7: IRQ on disk transfer
                if (m_transfer_reset) {
                    m_disk_position = 0;
                    m_byte_transfer = false;
                }
                break;
            case 0x4026:
                // External connector write
                m_ext_connector = value;
                break;

            // Sound registers
            case 0x4040: case 0x4041: case 0x4042: case 0x4043:
            case 0x4044: case 0x4045: case 0x4046: case 0x4047:
            case 0x4048: case 0x4049: case 0x404A: case 0x404B:
            case 0x404C: case 0x404D: case 0x404E: case 0x404F:
            case 0x4050: case 0x4051: case 0x4052: case 0x4053:
            case 0x4054: case 0x4055: case 0x4056: case 0x4057:
            case 0x4058: case 0x4059: case 0x405A: case 0x405B:
            case 0x405C: case 0x405D: case 0x405E: case 0x405F:
            case 0x4060: case 0x4061: case 0x4062: case 0x4063:
            case 0x4064: case 0x4065: case 0x4066: case 0x4067:
            case 0x4068: case 0x4069: case 0x406A: case 0x406B:
            case 0x406C: case 0x406D: case 0x406E: case 0x406F:
            case 0x4070: case 0x4071: case 0x4072: case 0x4073:
            case 0x4074: case 0x4075: case 0x4076: case 0x4077:
            case 0x4078: case 0x4079: case 0x407A: case 0x407B:
            case 0x407C: case 0x407D: case 0x407E: case 0x407F:
                // Wave table (64 entries)
                if (m_wave_write_enabled) {
                    m_wave_table[address - 0x4040] = value & 0x3F;
                }
                break;
            case 0x4080:
                // Volume envelope
                m_wave_volume = value & 0x3F;
                m_env_enabled = (value & 0x80) == 0;
                break;
            case 0x4082:
                // Wave frequency low
                m_wave_freq = (m_wave_freq & 0x0F00) | value;
                break;
            case 0x4083:
                // Wave frequency high + control
                m_wave_freq = (m_wave_freq & 0x00FF) | ((value & 0x0F) << 8);
                m_wave_enabled = (value & 0x80) == 0;
                m_env_enabled = (value & 0x40) == 0;
                break;
            case 0x4084:
                // Modulation envelope
                m_mod_gain = value & 0x3F;
                break;
            case 0x4085:
                // Modulation counter
                m_mod_counter = static_cast<int8_t>(value << 1) >> 1;  // Sign extend 7 bits
                break;
            case 0x4086:
                // Modulation frequency low
                m_mod_freq = (m_mod_freq & 0x0F00) | value;
                break;
            case 0x4087:
                // Modulation frequency high + control
                m_mod_freq = (m_mod_freq & 0x00FF) | ((value & 0x0F) << 8);
                m_mod_enabled = (value & 0x80) == 0;
                break;
            case 0x4088:
                // Modulation table write
                if (!m_mod_enabled) {
                    // Write to modulation table (shift in from high end)
                    for (int i = 0; i < 30; i++) {
                        m_mod_table[i] = m_mod_table[i + 2];
                    }
                    m_mod_table[30] = value & 0x07;
                    m_mod_table[31] = value & 0x07;
                }
                break;
            case 0x4089:
                // Master volume + wave write enable
                m_master_volume = value & 0x03;
                m_wave_write_enabled = (value & 0x80) != 0;
                break;
            case 0x408A:
                // Envelope speed
                m_env_speed = value;
                break;
        }
    }

    if (address >= 0x6000 && address <= 0xDFFF) {
        // Main PRG RAM (32KB, writable)
        m_prg_ram_main[address - 0x6000] = value;
    }
}

uint8_t Mapper020::ppu_read(uint16_t address, uint32_t) {
    if (address < 0x2000) {
        return m_chr_ram[address];
    }
    return 0;
}

void Mapper020::ppu_write(uint16_t address, uint8_t value) {
    if (address < 0x2000) {
        m_chr_ram[address] = value;
    }
}

bool Mapper020::irq_pending(uint32_t) {
    return m_irq_pending;
}

void Mapper020::irq_clear() {
    m_irq_pending = false;
}

void Mapper020::cpu_cycles(int count) {
    // FDS audio requires per-cycle updates for accurate emulation
    // For now, just call cpu_cycle for each cycle
    // TODO: Optimize this with batched audio processing if FDS becomes a bottleneck
    for (int i = 0; i < count; i++) {
        cpu_cycle();
    }
}

void Mapper020::cpu_cycle() {
    // IRQ counter
    if (m_irq_enabled && m_irq_counter > 0) {
        m_irq_counter--;
        if (m_irq_counter == 0) {
            m_irq_pending = true;
            if (m_irq_repeat) {
                m_irq_counter = m_irq_reload;
            } else {
                m_irq_enabled = false;
            }
        }
    }

    // Update audio (simplified - runs at CPU rate, should be more accurate)
    if (m_wave_enabled && m_wave_freq > 0) {
        // Accumulate wave frequency
        m_wave_accum += m_wave_freq;
        // When accumulator overflows 16 bits worth of fractional, advance wave position
        if (m_wave_accum >= 0x10000) {
            m_wave_accum -= 0x10000;
            m_wave_pos = (m_wave_pos + 1) & 0x3F;
        }
    }

    if (m_mod_enabled && m_mod_freq > 0) {
        m_mod_accum += m_mod_freq;
        if (m_mod_accum >= 0x10000) {
            m_mod_accum -= 0x10000;
            m_mod_pos = (m_mod_pos + 1) & 0x1F;
            // Update modulation counter based on table
            int8_t mod = m_mod_table[m_mod_pos];
            // Apply modulation table value to counter
            static const int8_t mod_adjust[8] = {0, 1, 2, 4, 0, -4, -2, -1};
            if (mod < 4) {
                m_mod_counter = (m_mod_counter + mod_adjust[mod]) & 0x7F;
            } else if (mod == 4) {
                m_mod_counter = 0;
            } else {
                m_mod_counter = (m_mod_counter + mod_adjust[mod]) & 0x7F;
            }
        }
    }

    // Disk transfer simulation
    if (m_motor_on && m_disk_inserted && !m_transfer_reset) {
        // Simplified: just signal byte ready periodically
        // Real FDS timing is much more complex
        static int disk_timer = 0;
        disk_timer++;
        if (disk_timer >= 150) {  // ~150 CPU cycles per byte
            disk_timer = 0;
            if (m_read_mode && m_disk_position < m_disk_data.size()) {
                m_data_read = m_disk_data[m_disk_position];
                m_disk_position++;
                m_byte_transfer = true;
            }
        }
    }
}

float Mapper020::get_audio_output() const {
    if (!m_wave_enabled || m_wave_freq == 0) {
        return 0.0f;
    }

    // Get current wave sample
    int sample = m_wave_table[m_wave_pos];

    // Apply volume (0-63)
    int volume = m_wave_volume;
    if (volume > 32) volume = 32;  // Volume above 32 is treated as 32

    // Apply master volume (0-3)
    static const float master_mul[4] = {1.0f, 2.0f/3.0f, 1.0f/2.0f, 1.0f/4.0f};
    float output = (sample - 32) * volume * master_mul[m_master_volume] / (32.0f * 32.0f);

    return output;
}

void Mapper020::set_disk_data(const std::vector<uint8_t>& disk_data) {
    m_disk_data = disk_data;
}

void Mapper020::insert_disk(int side) {
    m_current_side = side;
    m_disk_inserted = true;
    m_disk_ready = true;
    m_disk_position = 0;
}

void Mapper020::eject_disk() {
    m_disk_inserted = false;
    m_disk_ready = false;
}

// Serialization helpers
namespace {
    template<typename T>
    void write_value(std::vector<uint8_t>& data, T value) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(), ptr, ptr + sizeof(T));
    }

    template<typename T>
    bool read_value(const uint8_t*& data, size_t& remaining, T& value) {
        if (remaining < sizeof(T)) return false;
        std::memcpy(&value, data, sizeof(T));
        data += sizeof(T);
        remaining -= sizeof(T);
        return true;
    }

    void write_array(std::vector<uint8_t>& data, const uint8_t* arr, size_t size) {
        data.insert(data.end(), arr, arr + size);
    }

    bool read_array(const uint8_t*& data, size_t& remaining, uint8_t* arr, size_t size) {
        if (remaining < size) return false;
        std::memcpy(arr, data, size);
        data += size;
        remaining -= size;
        return true;
    }
}

void Mapper020::save_state(std::vector<uint8_t>& data) {
    // Save PRG RAM
    write_array(data, m_prg_ram_main.data(), m_prg_ram_main.size());
    write_array(data, m_prg_ram_bios.data(), m_prg_ram_bios.size());
    write_array(data, m_chr_ram.data(), m_chr_ram.size());

    // Save registers
    write_value(data, m_irq_reload);
    write_value(data, m_irq_counter);
    write_value(data, static_cast<uint8_t>(m_irq_enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_irq_repeat ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_irq_pending ? 1 : 0));

    // Save disk state
    write_value(data, static_cast<uint8_t>(m_disk_inserted ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_disk_ready ? 1 : 0));
    write_value(data, m_disk_position);
    write_value(data, m_data_read);
    write_value(data, m_data_write);
    write_value(data, static_cast<uint8_t>(m_byte_transfer ? 1 : 0));

    // Save audio state
    write_array(data, m_wave_table.data(), m_wave_table.size());
    write_value(data, m_wave_freq);
    write_value(data, m_wave_volume);
    write_value(data, m_wave_pos);
    write_value(data, m_wave_accum);
    write_value(data, static_cast<uint8_t>(m_wave_enabled ? 1 : 0));

    // Save mirroring
    write_value(data, static_cast<uint8_t>(m_mirror_mode));
}

void Mapper020::load_state(const uint8_t*& data, size_t& remaining) {
    // Load PRG RAM
    read_array(data, remaining, m_prg_ram_main.data(), m_prg_ram_main.size());
    read_array(data, remaining, m_prg_ram_bios.data(), m_prg_ram_bios.size());
    read_array(data, remaining, m_chr_ram.data(), m_chr_ram.size());

    // Load registers
    read_value(data, remaining, m_irq_reload);
    read_value(data, remaining, m_irq_counter);
    uint8_t flag;
    read_value(data, remaining, flag);
    m_irq_enabled = flag != 0;
    read_value(data, remaining, flag);
    m_irq_repeat = flag != 0;
    read_value(data, remaining, flag);
    m_irq_pending = flag != 0;

    // Load disk state
    read_value(data, remaining, flag);
    m_disk_inserted = flag != 0;
    read_value(data, remaining, flag);
    m_disk_ready = flag != 0;
    read_value(data, remaining, m_disk_position);
    read_value(data, remaining, m_data_read);
    read_value(data, remaining, m_data_write);
    read_value(data, remaining, flag);
    m_byte_transfer = flag != 0;

    // Load audio state
    read_array(data, remaining, m_wave_table.data(), m_wave_table.size());
    read_value(data, remaining, m_wave_freq);
    read_value(data, remaining, m_wave_volume);
    read_value(data, remaining, m_wave_pos);
    read_value(data, remaining, m_wave_accum);
    read_value(data, remaining, flag);
    m_wave_enabled = flag != 0;

    // Load mirroring
    uint8_t mirror;
    read_value(data, remaining, mirror);
    m_mirror_mode = static_cast<MirrorMode>(mirror);
}

} // namespace nes
