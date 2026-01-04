#include "bus.hpp"
#include "arm7tdmi.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include "debug.hpp"
#include <cstring>

namespace gba {

Bus::Bus() {
    // Initialize memory
    m_bios.fill(0);
    m_ewram.fill(0);
    m_iwram.fill(0);

    // Initialize HLE BIOS - write IRQ handler at 0x00000018
    // This is a simplified GBA BIOS IRQ handler that:
    // 1. Saves registers R0-R3, R12, LR to stack
    // 2. Reads user IRQ handler from 0x03FFFFFC (mirrors 0x03007FFC)
    // 3. Calls user handler
    // 4. Restores registers and returns from IRQ
    // Note: We rely on edge-triggered IRQ detection instead of BIOS acknowledgment
    //
    // Assembly:
    //   0x18: stmfd  sp!, {r0-r3, r12, lr}    ; E92D500F
    //   0x1C: mov    r0, #0x04000000          ; E3A00301
    //   0x20: add    lr, pc, #0               ; E28FE000
    //   0x24: ldr    pc, [r0, #-4]            ; E510F004
    //   0x28: ldmfd  sp!, {r0-r3, r12, lr}    ; E8BD500F
    //   0x2C: subs   pc, lr, #4               ; E25EF004

    auto write_word = [this](uint32_t addr, uint32_t value) {
        m_bios[addr + 0] = value & 0xFF;
        m_bios[addr + 1] = (value >> 8) & 0xFF;
        m_bios[addr + 2] = (value >> 16) & 0xFF;
        m_bios[addr + 3] = (value >> 24) & 0xFF;
    };

    // IRQ handler at 0x18 (this is where CPU jumps on IRQ)
    // When CPU enters IRQ, it jumps to 0x18 and executes our HLE handler:
    // 0x18: Save registers
    // 0x1C-0x24: Set up and call game's IRQ handler
    // 0x28-0x2C: Restore and return
    //
    // For BIOS read protection, we update m_last_bios_read to addr+8 (ARM prefetch)
    // - Executing 0x24 (branch to game handler): prefetch at 0x2C = 0xE25EF004
    // - Executing 0x2C (return from IRQ): prefetch at 0x34 = 0xE55EC002
    write_word(0x18, 0xE92D500F);  // stmfd sp!, {r0-r3, r12, lr}
    write_word(0x1C, 0xE3A00301);  // mov r0, #0x04000000
    write_word(0x20, 0xE28FE000);  // add lr, pc, #0  (LR = PC+8 = 0x28)
    write_word(0x24, 0xE510F004);  // ldr pc, [r0, #-4]  (jump to [0x03FFFFFC])
    write_word(0x28, 0xE8BD500F);  // ldmfd sp!, {r0-r3, r12, lr}
    write_word(0x2C, 0xE25EF004);  // subs pc, lr, #4  (return from IRQ)
    write_word(0x30, 0xE1A00000);  // nop (padding)
    write_word(0x34, 0xE55EC002);  // Value expected after IRQ return (at prefetch 0x2C+8)

    // BIOS read protection test values
    // - After startup: 0xE129F000 (default m_last_bios_read)
    // - After SWI: 0xE3A02004 (set by HLE SWI handler)
    // - During IRQ: 0xE25EF004 (prefetch at 0x2C when executing 0x24)
    // - After IRQ: 0xE55EC002 (prefetch at 0x34 when executing 0x2C)
    write_word(0xDC, 0xE129F000);  // Startup code location

    // Initialize I/O registers
    m_bgcnt.fill(0);
    m_bghofs.fill(0);
    m_bgvofs.fill(0);
    m_bgpa.fill(0x100);
    m_bgpb.fill(0);
    m_bgpc.fill(0);
    m_bgpd.fill(0x100);
    m_bgx.fill(0);
    m_bgy.fill(0);
    m_sound_regs.fill(0);

    // Initialize DISPSTAT with VBlank IRQ enabled
    // Some games expect this to be set by BIOS; when skipping BIOS we set it here
    // Bit 3 = VBlank IRQ Enable
    m_dispstat = 0x0008;

    // Initialize wait state tables with default values
    // These get updated when WAITCNT is written
    m_ws_n.fill(4);
    m_ws_s.fill(2);
}

Bus::~Bus() = default;

MemoryRegion Bus::get_region(uint32_t address) {
    switch (address >> 24) {
        case 0x00: return MemoryRegion::BIOS;
        case 0x02: return MemoryRegion::EWRAM;
        case 0x03: return MemoryRegion::IWRAM;
        case 0x04: return MemoryRegion::IO;
        case 0x05: return MemoryRegion::Palette;
        case 0x06: return MemoryRegion::VRAM;
        case 0x07: return MemoryRegion::OAM;
        case 0x08:
        case 0x09: return MemoryRegion::ROM_WS0;
        case 0x0A:
        case 0x0B: return MemoryRegion::ROM_WS1;
        case 0x0C:
        case 0x0D: return MemoryRegion::ROM_WS2;
        case 0x0E:
        case 0x0F: return MemoryRegion::SRAM;
        default:   return MemoryRegion::Invalid;
    }
}

int Bus::get_wait_states(uint32_t address, bool is_sequential, int access_size) {
    // Default WAITCNT = 0x0000 gives:
    // - SRAM: 4 cycles
    // - WS0 N: 4, S: 2
    // - WS1 N: 4, S: 4
    // - WS2 N: 4, S: 8
    //
    // WAITCNT register bits:
    // 0-1: SRAM wait (0=4, 1=3, 2=2, 3=8 cycles)
    // 2-3: WS0 first access (N) (0=4, 1=3, 2=2, 3=8 cycles)
    // 4:   WS0 second access (S) (0=2, 1=1 cycles)
    // 5-6: WS1 first access (N) (0=4, 1=3, 2=2, 3=8 cycles)
    // 7:   WS1 second access (S) (0=4, 1=1 cycles)
    // 8-9: WS2 first access (N) (0=4, 1=3, 2=2, 3=8 cycles)
    // 10:  WS2 second access (S) (0=8, 1=1 cycles)
    // 14:  Prefetch buffer enable
    // 15:  Game Pak type (0=GBA, 1=CGB)

    static const int first_access_cycles[] = {4, 3, 2, 8};

    MemoryRegion region = get_region(address);

    switch (region) {
        case MemoryRegion::BIOS:
        case MemoryRegion::IWRAM:
        case MemoryRegion::IO:
        case MemoryRegion::Palette:
        case MemoryRegion::OAM:
            // No wait states for these regions
            return 0;

        case MemoryRegion::EWRAM:
            // EWRAM always has 2 wait states (3 cycles total for 16-bit, +1 for 32-bit)
            return (access_size == 32) ? 5 : 2;

        case MemoryRegion::VRAM:
            // VRAM has 0 wait states normally, but +1 for 32-bit access
            return (access_size == 32) ? 1 : 0;

        case MemoryRegion::ROM_WS0: {
            int n_bits = (m_waitcnt >> 2) & 3;
            int s_bit = (m_waitcnt >> 4) & 1;
            int n_wait = first_access_cycles[n_bits];
            int s_wait = s_bit ? 1 : 2;
            int wait = is_sequential ? s_wait : n_wait;
            // 32-bit access = two 16-bit accesses (N + S or S + S)
            if (access_size == 32) {
                wait += is_sequential ? s_wait : s_wait;
            }
            return wait;
        }

        case MemoryRegion::ROM_WS1: {
            int n_bits = (m_waitcnt >> 5) & 3;
            int s_bit = (m_waitcnt >> 7) & 1;
            int n_wait = first_access_cycles[n_bits];
            int s_wait = s_bit ? 1 : 4;
            int wait = is_sequential ? s_wait : n_wait;
            if (access_size == 32) {
                wait += is_sequential ? s_wait : s_wait;
            }
            return wait;
        }

        case MemoryRegion::ROM_WS2: {
            int n_bits = (m_waitcnt >> 8) & 3;
            int s_bit = (m_waitcnt >> 10) & 1;
            int n_wait = first_access_cycles[n_bits];
            int s_wait = s_bit ? 1 : 8;
            int wait = is_sequential ? s_wait : n_wait;
            if (access_size == 32) {
                wait += is_sequential ? s_wait : s_wait;
            }
            return wait;
        }

        case MemoryRegion::SRAM: {
            int sram_bits = m_waitcnt & 3;
            return first_access_cycles[sram_bits];
        }

        default:
            return 0;
    }
}

int Bus::get_rom_s_cycles() const {
    // Return the sequential wait cycles for ROM WS0 (most commonly used for code)
    // This is used for prefetch buffer duty cycle calculation
    // WS0 S bit: WAITCNT bit 4 (0=2 cycles, 1=1 cycle)
    int s_bit = (m_waitcnt >> 4) & 1;
    return s_bit ? 1 : 2;
}

int Bus::get_prefetch_duty(uint32_t address) const {
    // Get the S-cycle wait states for the ROM region at this address
    // This determines how long each prefetch takes
    // Different ROM waitstate regions have different S-cycle timing
    //
    // WAITCNT bits:
    // - WS0 S: bit 4 (0=2, 1=1)
    // - WS1 S: bit 7 (0=4, 1=1)
    // - WS2 S: bit 10 (0=8, 1=1)

    uint32_t region = address >> 24;

    switch (region) {
        case 0x08:
        case 0x09: {
            // ROM WS0
            int s_bit = (m_waitcnt >> 4) & 1;
            return s_bit ? 1 : 2;
        }
        case 0x0A:
        case 0x0B: {
            // ROM WS1
            int s_bit = (m_waitcnt >> 7) & 1;
            return s_bit ? 1 : 4;
        }
        case 0x0C:
        case 0x0D: {
            // ROM WS2
            int s_bit = (m_waitcnt >> 10) & 1;
            return s_bit ? 1 : 8;
        }
        default:
            // Not a ROM region, default to WS0 timing
            return 2;
    }
}

uint8_t Bus::read8(uint32_t address) {
    MemoryRegion region = get_region(address);

    switch (region) {
        case MemoryRegion::BIOS:
            // BIOS is protected - can only read from within BIOS region
            // When PC is in BIOS, allow reading; otherwise return last BIOS read value
            if (address < 0x4000) {
                // Check if CPU is executing from BIOS
                if (m_cpu && m_cpu->get_pc() < 0x4000) {
                    uint8_t val = m_bios[address];
                    // Update last BIOS read (store the full aligned word)
                    uint32_t aligned = address & ~3u;
                    m_last_bios_read = m_bios[aligned] |
                                       (m_bios[aligned + 1] << 8) |
                                       (m_bios[aligned + 2] << 16) |
                                       (m_bios[aligned + 3] << 24);
                    return val;
                }
                // Return appropriate byte from last BIOS read
                return static_cast<uint8_t>(m_last_bios_read >> ((address & 3) * 8));
            }
            break;

        case MemoryRegion::EWRAM:
            return m_ewram[address & 0x3FFFF];

        case MemoryRegion::IWRAM:
            return m_iwram[address & 0x7FFF];

        case MemoryRegion::IO: {
            uint16_t io_reg = read_io(address & ~1);
            return static_cast<uint8_t>(io_reg >> ((address & 1) * 8));
        }

        case MemoryRegion::Palette:
            if (m_ppu) {
                return m_ppu->read_palette(address & 0x3FF);
            }
            break;

        case MemoryRegion::VRAM:
            if (m_ppu) {
                // VRAM is 96KB, mirrored
                uint32_t offset = address & 0x1FFFF;
                if (offset >= 0x18000) offset -= 0x8000;
                return m_ppu->read_vram(offset);
            }
            break;

        case MemoryRegion::OAM:
            if (m_ppu) {
                return m_ppu->read_oam(address & 0x3FF);
            }
            break;

        case MemoryRegion::ROM_WS0:
        case MemoryRegion::ROM_WS1:
            if (m_cartridge) {
                return m_cartridge->read_rom(address & 0x1FFFFFF);
            }
            break;

        case MemoryRegion::ROM_WS2:
            if (m_cartridge) {
                // Check for EEPROM access in 0x0D region
                SaveType save_type = m_cartridge->get_save_type();
                if (save_type == SaveType::EEPROM_512 || save_type == SaveType::EEPROM_8K) {
                    // EEPROM access - check address range
                    // For large ROMs (>16MB), EEPROM is at 0x0DFFFF00-0x0DFFFFFF
                    // For small ROMs, it can be anywhere in 0x0D000000+
                    uint32_t rom_offset = address & 0x1FFFFFF;
                    if (rom_offset >= 0x1FFFF00 || rom_offset >= m_cartridge->get_rom_size()) {
                        return m_cartridge->read_sram(address & 0xFFFF);
                    }
                }
                return m_cartridge->read_rom(address & 0x1FFFFFF);
            }
            break;

        case MemoryRegion::SRAM:
            if (m_cartridge) {
                return m_cartridge->read_sram(address & 0xFFFF);
            }
            break;

        default:
            break;
    }

    // Open bus
    return static_cast<uint8_t>(get_open_bus_value(address));
}

uint16_t Bus::read16(uint32_t address) {
    address &= ~1u;  // Force alignment
    MemoryRegion region = get_region(address);

    switch (region) {
        case MemoryRegion::BIOS:
            if (address < 0x4000) {
                if (m_cpu && m_cpu->get_pc() < 0x4000) {
                    // Update last BIOS read
                    uint32_t aligned = address & ~3u;
                    m_last_bios_read = m_bios[aligned] |
                                       (m_bios[aligned + 1] << 8) |
                                       (m_bios[aligned + 2] << 16) |
                                       (m_bios[aligned + 3] << 24);
                    return m_bios[address] | (m_bios[address + 1] << 8);
                }
                // Return from last BIOS read
                return static_cast<uint16_t>(m_last_bios_read >> ((address & 2) * 8));
            }
            break;

        case MemoryRegion::EWRAM: {
            uint32_t offset = address & 0x3FFFF;
            return m_ewram[offset] | (m_ewram[offset + 1] << 8);
        }

        case MemoryRegion::IWRAM: {
            uint32_t offset = address & 0x7FFF;
            return m_iwram[offset] | (m_iwram[offset + 1] << 8);
        }

        case MemoryRegion::IO:
            return read_io(address);

        case MemoryRegion::Palette:
            if (m_ppu) {
                uint32_t offset = address & 0x3FF;
                return m_ppu->read_palette(offset) | (m_ppu->read_palette(offset + 1) << 8);
            }
            break;

        case MemoryRegion::VRAM:
            if (m_ppu) {
                uint32_t offset = address & 0x1FFFF;
                if (offset >= 0x18000) offset -= 0x8000;
                return m_ppu->read_vram(offset) | (m_ppu->read_vram(offset + 1) << 8);
            }
            break;

        case MemoryRegion::OAM:
            if (m_ppu) {
                uint32_t offset = address & 0x3FF;
                return m_ppu->read_oam(offset) | (m_ppu->read_oam(offset + 1) << 8);
            }
            break;

        case MemoryRegion::ROM_WS0:
        case MemoryRegion::ROM_WS1:
            if (m_cartridge) {
                uint32_t offset = address & 0x1FFFFFF;
                return m_cartridge->read_rom(offset) | (m_cartridge->read_rom(offset + 1) << 8);
            }
            break;

        case MemoryRegion::ROM_WS2:
            if (m_cartridge) {
                uint32_t offset = address & 0x1FFFFFF;
                // Check for EEPROM access
                SaveType save_type = m_cartridge->get_save_type();
                if (save_type == SaveType::EEPROM_512 || save_type == SaveType::EEPROM_8K) {
                    if (offset >= 0x1FFFF00 || offset >= m_cartridge->get_rom_size()) {
                        // EEPROM read returns bit 0 only, replicated
                        uint8_t bit = m_cartridge->read_sram(address & 0xFFFF);
                        return bit;
                    }
                }
                return m_cartridge->read_rom(offset) | (m_cartridge->read_rom(offset + 1) << 8);
            }
            break;

        case MemoryRegion::SRAM:
            // SRAM is 8-bit only
            if (m_cartridge) {
                uint8_t value = m_cartridge->read_sram(address & 0xFFFF);
                return value | (value << 8);
            }
            break;

        default:
            break;
    }

    return static_cast<uint16_t>(get_open_bus_value(address));
}

uint32_t Bus::read32(uint32_t address) {
    address &= ~3u;  // Force alignment

    // Special handling for BIOS to return correct last read value
    if (address < 0x4000) {
        uint32_t pc = m_cpu ? m_cpu->get_pc() : 0xFFFFFFFF;
        if (pc < 0x4000) {
            // When PC is in BIOS, update last_bios_read
            // Due to ARM pipeline, we should also prefetch the next instruction
            // The pipeline fetches PC+8, so update last_bios_read to reflect that
            m_last_bios_read = m_bios[address] |
                               (m_bios[address + 1] << 8) |
                               (m_bios[address + 2] << 16) |
                               (m_bios[address + 3] << 24);

            // Also update with prefetch value (what pipeline would have in fetch stage)
            // This is important for BIOS read protection when branching out of BIOS
            uint32_t prefetch_addr = address + 8;  // ARM pipeline prefetches PC+8
            if (prefetch_addr < 0x4000) {
                m_last_bios_read = m_bios[prefetch_addr] |
                                   (m_bios[prefetch_addr + 1] << 8) |
                                   (m_bios[prefetch_addr + 2] << 16) |
                                   (m_bios[prefetch_addr + 3] << 24);
            }
            return m_bios[address] |
                   (m_bios[address + 1] << 8) |
                   (m_bios[address + 2] << 16) |
                   (m_bios[address + 3] << 24);
        }
        return m_last_bios_read;
    }

    // SRAM is 8-bit only - reading 32-bit returns the byte at address replicated 4 times
    if ((address >> 24) == 0x0E || (address >> 24) == 0x0F) {
        if (m_cartridge) {
            uint8_t value = m_cartridge->read_sram(address & 0xFFFF);
            return value | (value << 8) | (value << 16) | (value << 24);
        }
        return 0xFFFFFFFF;
    }

    return read16(address) | (read16(address + 2) << 16);
}

void Bus::write8(uint32_t address, uint8_t value) {
    MemoryRegion region = get_region(address);

    switch (region) {
        case MemoryRegion::EWRAM:
            m_ewram[address & 0x3FFFF] = value;
            break;

        case MemoryRegion::IWRAM: {
            uint32_t offset = address & 0x7FFF;
            // Debug: track 8-bit writes near crash location
            if (is_debug_mode() && offset >= 0x7DC6 && offset <= 0x7DCC) {
                uint32_t pc = m_cpu ? m_cpu->get_pc() : 0;
                fprintf(stderr, "[GBA] BUS_WRITE8 to 0x%08X: value=0x%02X, PC=0x%08X\n",
                        address, value, pc);
            }
            m_iwram[offset] = value;
            break;
        }

        case MemoryRegion::IO: {
            // Byte writes to I/O need special handling
            uint32_t io_addr = address & ~1;
            uint16_t old_val = read_io(io_addr);
            if (address & 1) {
                write_io(io_addr, (old_val & 0x00FF) | (value << 8));
            } else {
                write_io(io_addr, (old_val & 0xFF00) | value);
            }
            break;
        }

        case MemoryRegion::Palette:
            // Byte writes to palette write the value to both bytes
            if (m_ppu) {
                uint32_t offset = address & 0x3FE;
                m_ppu->write_palette(offset, value);
                m_ppu->write_palette(offset + 1, value);
            }
            break;

        case MemoryRegion::VRAM:
            // Byte writes to VRAM in bitmap modes write to both bytes
            // In tile modes, byte writes to OBJ VRAM are ignored
            if (m_ppu) {
                uint32_t offset = address & 0x1FFFF;
                if (offset >= 0x18000) offset -= 0x8000;

                // Check if in OBJ tile area (depends on mode)
                // For now, allow all writes
                m_ppu->write_vram(offset & ~1u, value);
                m_ppu->write_vram((offset & ~1u) + 1, value);
            }
            break;

        case MemoryRegion::SRAM:
            if (m_cartridge) {
                m_cartridge->write_sram(address & 0xFFFF, value);
            }
            break;

        case MemoryRegion::ROM_WS0:
        case MemoryRegion::ROM_WS1:
            // ROM is normally read-only, but GPIO ports are at 0x080000C4-0x080000C9
            if (m_cartridge) {
                uint32_t rom_addr = address & 0x1FFFFFF;
                if (rom_addr >= 0xC4 && rom_addr <= 0xC9) {
                    m_cartridge->write_rom(rom_addr, value);
                }
            }
            break;

        case MemoryRegion::ROM_WS2:
            if (m_cartridge) {
                uint32_t rom_addr = address & 0x1FFFFFF;
                // Check for EEPROM write
                SaveType save_type = m_cartridge->get_save_type();
                if (save_type == SaveType::EEPROM_512 || save_type == SaveType::EEPROM_8K) {
                    if (rom_addr >= 0x1FFFF00 || rom_addr >= m_cartridge->get_rom_size()) {
                        m_cartridge->write_sram(address & 0xFFFF, value);
                        break;
                    }
                }
                // GPIO ports
                if (rom_addr >= 0xC4 && rom_addr <= 0xC9) {
                    m_cartridge->write_rom(rom_addr, value);
                }
            }
            break;

        default:
            break;
    }
}

void Bus::write16(uint32_t address, uint16_t value) {
    address &= ~1u;  // Force alignment
    MemoryRegion region = get_region(address);

    switch (region) {
        case MemoryRegion::EWRAM: {
            uint32_t offset = address & 0x3FFFF;
            m_ewram[offset] = value & 0xFF;
            m_ewram[offset + 1] = value >> 8;
            break;
        }

        case MemoryRegion::IWRAM: {
            uint32_t offset = address & 0x7FFF;
            m_iwram[offset] = value & 0xFF;
            m_iwram[offset + 1] = value >> 8;
            break;
        }

        case MemoryRegion::IO:
            write_io(address, value);
            break;

        case MemoryRegion::Palette:
            if (m_ppu) {
                uint32_t offset = address & 0x3FF;
                m_ppu->write_palette(offset, value & 0xFF);
                m_ppu->write_palette(offset + 1, value >> 8);
            }
            break;

        case MemoryRegion::VRAM:
            if (m_ppu) {
                uint32_t offset = address & 0x1FFFF;
                if (offset >= 0x18000) offset -= 0x8000;
                m_ppu->write_vram(offset, value & 0xFF);
                m_ppu->write_vram(offset + 1, value >> 8);
            }
            break;

        case MemoryRegion::OAM:
            if (m_ppu) {
                uint32_t offset = address & 0x3FF;
                m_ppu->write_oam(offset, value & 0xFF);
                m_ppu->write_oam(offset + 1, value >> 8);
            }
            break;

        case MemoryRegion::SRAM:
            // SRAM is 8-bit only - halfword writes just write the low byte
            // (alignment already done, so address is even)
            if (m_cartridge) {
                m_cartridge->write_sram(address & 0xFFFF, static_cast<uint8_t>(value));
            }
            break;

        case MemoryRegion::ROM_WS0:
        case MemoryRegion::ROM_WS1:
            // ROM is normally read-only, but GPIO ports are at 0x080000C4-0x080000C9
            if (m_cartridge) {
                uint32_t rom_addr = address & 0x1FFFFFF;
                if (rom_addr >= 0xC4 && rom_addr <= 0xC9) {
                    m_cartridge->write_rom(rom_addr, value & 0xFF);
                    if (rom_addr + 1 <= 0xC9) {
                        m_cartridge->write_rom(rom_addr + 1, value >> 8);
                    }
                }
            }
            break;

        case MemoryRegion::ROM_WS2:
            if (m_cartridge) {
                uint32_t rom_addr = address & 0x1FFFFFF;
                // Check for EEPROM write (DMA transfers use 16-bit writes)
                SaveType save_type = m_cartridge->get_save_type();
                if (save_type == SaveType::EEPROM_512 || save_type == SaveType::EEPROM_8K) {
                    if (rom_addr >= 0x1FFFF00 || rom_addr >= m_cartridge->get_rom_size()) {
                        // EEPROM writes only use bit 0
                        m_cartridge->write_sram(address & 0xFFFF, static_cast<uint8_t>(value & 1));
                        break;
                    }
                }
                // GPIO ports
                if (rom_addr >= 0xC4 && rom_addr <= 0xC9) {
                    m_cartridge->write_rom(rom_addr, value & 0xFF);
                    if (rom_addr + 1 <= 0xC9) {
                        m_cartridge->write_rom(rom_addr + 1, value >> 8);
                    }
                }
            }
            break;

        default:
            break;
    }
}

void Bus::write32(uint32_t address, uint32_t value) {
    address &= ~3u;  // Force alignment
    write16(address, value & 0xFFFF);
    write16(address + 2, value >> 16);
}

void Bus::write16_unaligned(uint32_t address, uint16_t value) {
    // For SRAM (8-bit bus), write the appropriate byte based on alignment
    if ((address >> 24) == 0x0E || (address >> 24) == 0x0F) {
        if (m_cartridge) {
            // Select byte based on address bit 0
            uint8_t byte_val = static_cast<uint8_t>((address & 1) ? (value >> 8) : value);
            m_cartridge->write_sram(address & 0xFFFF, byte_val);
        }
        return;
    }

    // For other regions, force alignment and write normally
    write16(address & ~1u, value);
}

void Bus::write32_unaligned(uint32_t address, uint32_t value) {
    // For SRAM (8-bit bus), write only the byte at the address position
    if ((address >> 24) == 0x0E || (address >> 24) == 0x0F) {
        if (m_cartridge) {
            // The byte to write is selected by address[1:0]
            int shift = (address & 3) * 8;
            uint8_t byte_val = static_cast<uint8_t>(value >> shift);
            m_cartridge->write_sram(address & 0xFFFF, byte_val);
        }
        return;
    }

    // For other regions, force alignment and write normally
    write32(address & ~3u, value);
}

uint16_t Bus::read_io(uint32_t address) {
    // mGBA debug registers (0x04FFF600-0x04FFF7FF)
    // REG_DEBUG_ENABLE = 0x4FFF780 (write 0xC0DE to enable, reads 0x1DEA if supported)
    // REG_DEBUG_FLAGS  = 0x4FFF700 (write level|0x100 to flush)
    // REG_DEBUG_STRING = 0x4FFF600 (256-byte text buffer)
    uint32_t debug_offset = address & 0xFFFF;
    if (debug_offset >= 0xF600) {
        switch (debug_offset) {
            case 0xF780:  // REG_DEBUG_ENABLE
                // Return 0x1DEA to indicate debug console is available
                return 0x1DEA;
            case 0xF700:  // REG_DEBUG_FLAGS
                return m_debug_flags;
            default:
                // Debug string buffer (0xF600-0xF6FF) - write-only
                return 0;
        }
    }

    address &= 0xFFF;

    switch (address) {
        // Display
        case 0x000: return m_dispcnt;
        case 0x002: return 0;  // Green swap (not used)
        case 0x004: return m_ppu ? m_ppu->get_dispstat() : m_dispstat;
        case 0x006:
            return m_ppu ? m_ppu->get_vcount() : 0;

        // Background control
        case 0x008: return m_bgcnt[0];
        case 0x00A: return m_bgcnt[1];
        case 0x00C: return m_bgcnt[2];
        case 0x00E: return m_bgcnt[3];

        // Background offset (write-only, return 0)
        case 0x010: case 0x012: case 0x014: case 0x016:
        case 0x018: case 0x01A: case 0x01C: case 0x01E:
            return 0;

        // Affine parameters (write-only)
        case 0x020: case 0x022: case 0x024: case 0x026:
        case 0x028: case 0x02A: case 0x02C: case 0x02E:
        case 0x030: case 0x032: case 0x034: case 0x036:
        case 0x038: case 0x03A: case 0x03C: case 0x03E:
            return 0;

        // Window (write-only)
        case 0x040: case 0x042: case 0x044: case 0x046:
            return 0;
        case 0x048: return m_winin;
        case 0x04A: return m_winout;

        // Effects
        case 0x04C: return m_mosaic;
        case 0x050: return m_bldcnt;
        case 0x052: return m_bldalpha;
        case 0x054: return 0;  // BLDY is write-only

        // Sound (simplified)
        case 0x060 ... 0x080:
            return m_sound_regs[(address - 0x060) >> 1];

        // SOUNDCNT_H - Direct Sound control
        case 0x082:
            if (m_apu) return m_apu->read_soundcnt_h();
            return 0;

        case 0x084 ... 0x09E:
            return m_sound_regs[(address - 0x060) >> 1];

        // FIFO_A and FIFO_B are write-only
        case 0x0A0: case 0x0A2: case 0x0A4: case 0x0A6:
            return 0;

        // DMA (return control registers only)
        case 0x0BA: return m_dma[0].control;
        case 0x0C6: return m_dma[1].control;
        case 0x0D2: return m_dma[2].control;
        case 0x0DE: return m_dma[3].control;

        // Timers
        case 0x100: return get_timer_counter(0);
        case 0x102: return m_timers[0].control;
        case 0x104: return get_timer_counter(1);
        case 0x106: return m_timers[1].control;
        case 0x108: return get_timer_counter(2);
        case 0x10A: return m_timers[2].control;
        case 0x10C: return get_timer_counter(3);
        case 0x10E: return m_timers[3].control;

        // Key input
        case 0x130: return m_keyinput;
        case 0x132: return m_keycnt;

        // Interrupts
        case 0x200: return m_ie;
        case 0x202: return m_if;
        case 0x204: return m_waitcnt;
        case 0x208: return m_ime;

        case 0x300: return m_postflg;

        default:
            return 0;
    }
}

void Bus::write_io(uint32_t address, uint16_t value) {
    // mGBA debug registers (0x04FFF600-0x04FFF7FF)
    uint32_t debug_offset = address & 0xFFFF;
    if (debug_offset >= 0xF600) {
        if (debug_offset == 0xF780) {
            // REG_DEBUG_ENABLE - write 0xC0DE to enable debug console
            m_debug_enabled = (value == 0xC0DE);
            return;
        }
        if (debug_offset == 0xF700) {
            // REG_DEBUG_FLAGS - write (level | 0x100) to flush debug string
            m_debug_flags = value;
            if (value & 0x100) {
                flush_debug_string();
            }
            return;
        }
        if (debug_offset >= 0xF600 && debug_offset < 0xF700) {
            // REG_DEBUG_STRING - write characters to debug string buffer
            size_t buf_offset = debug_offset - 0xF600;
            if (buf_offset < 255) {
                m_debug_string[buf_offset] = static_cast<char>(value & 0xFF);
                if (buf_offset + 1 < 255) {
                    m_debug_string[buf_offset + 1] = static_cast<char>((value >> 8) & 0xFF);
                }
                // Track highest written position for null termination
                if (buf_offset + 1 > m_debug_string_pos) {
                    m_debug_string_pos = buf_offset + 2;
                }
            }
            return;
        }
        return;  // Unknown debug register
    }

    address &= 0xFFF;

    switch (address) {
        // Display
        case 0x000:
            GBA_DEBUG_PRINT("DISPCNT write: 0x%04X (Mode=%d, ForcedBlank=%s, BG0=%s, BG1=%s, BG2=%s, BG3=%s, OBJ=%s)\n",
                           value, value & 0x7, (value & 0x80) ? "YES" : "no",
                           (value & 0x100) ? "Y" : "N", (value & 0x200) ? "Y" : "N",
                           (value & 0x400) ? "Y" : "N", (value & 0x800) ? "Y" : "N",
                           (value & 0x1000) ? "Y" : "N");
            m_dispcnt = value;
            if (m_ppu) m_ppu->sync_registers(m_dispcnt, m_dispstat);
            break;
        case 0x004:
            // Only bits 3-5 (IRQ enables) and 8-15 (VCount target) are writable in DISPSTAT
            // Bits 0-2 are read-only status flags maintained by PPU
            GBA_DEBUG_PRINT("DISPSTAT write: 0x%04X (old=0x%04X, VBlankIRQ=%s)\n",
                           value, m_dispstat, (value & 0x0008) ? "ENABLED" : "disabled");
            m_dispstat = (m_dispstat & 0x0007) | (value & 0xFFF8);
            if (m_ppu) m_ppu->sync_registers(m_dispcnt, m_dispstat);
            break;
        // VCOUNT is read-only

        // Background control
        case 0x008: m_bgcnt[0] = value; break;
        case 0x00A: m_bgcnt[1] = value; break;
        case 0x00C: m_bgcnt[2] = value; break;
        case 0x00E: m_bgcnt[3] = value; break;

        // Background offset
        case 0x010: m_bghofs[0] = value & 0x1FF; break;
        case 0x012: m_bgvofs[0] = value & 0x1FF; break;
        case 0x014: m_bghofs[1] = value & 0x1FF; break;
        case 0x016: m_bgvofs[1] = value & 0x1FF; break;
        case 0x018: m_bghofs[2] = value & 0x1FF; break;
        case 0x01A: m_bgvofs[2] = value & 0x1FF; break;
        case 0x01C: m_bghofs[3] = value & 0x1FF; break;
        case 0x01E: m_bgvofs[3] = value & 0x1FF; break;

        // BG2 Affine
        case 0x020: m_bgpa[0] = static_cast<int16_t>(value); break;
        case 0x022: m_bgpb[0] = static_cast<int16_t>(value); break;
        case 0x024: m_bgpc[0] = static_cast<int16_t>(value); break;
        case 0x026: m_bgpd[0] = static_cast<int16_t>(value); break;
        case 0x028:
            m_bgx[0] = (m_bgx[0] & 0xFFFF0000) | value;
            // Per GBATEK: Writing to reference point immediately updates internal register
            if (m_ppu) m_ppu->update_bgx_internal(0, m_bgx[0]);
            break;
        case 0x02A:
            m_bgx[0] = (m_bgx[0] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16);
            if (m_ppu) m_ppu->update_bgx_internal(0, m_bgx[0]);
            break;
        case 0x02C:
            m_bgy[0] = (m_bgy[0] & 0xFFFF0000) | value;
            if (m_ppu) m_ppu->update_bgy_internal(0, m_bgy[0]);
            break;
        case 0x02E:
            m_bgy[0] = (m_bgy[0] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16);
            if (m_ppu) m_ppu->update_bgy_internal(0, m_bgy[0]);
            break;

        // BG3 Affine
        case 0x030: m_bgpa[1] = static_cast<int16_t>(value); break;
        case 0x032: m_bgpb[1] = static_cast<int16_t>(value); break;
        case 0x034: m_bgpc[1] = static_cast<int16_t>(value); break;
        case 0x036: m_bgpd[1] = static_cast<int16_t>(value); break;
        case 0x038:
            m_bgx[1] = (m_bgx[1] & 0xFFFF0000) | value;
            if (m_ppu) m_ppu->update_bgx_internal(1, m_bgx[1]);
            break;
        case 0x03A:
            m_bgx[1] = (m_bgx[1] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16);
            if (m_ppu) m_ppu->update_bgx_internal(1, m_bgx[1]);
            break;
        case 0x03C:
            m_bgy[1] = (m_bgy[1] & 0xFFFF0000) | value;
            if (m_ppu) m_ppu->update_bgy_internal(1, m_bgy[1]);
            break;
        case 0x03E:
            m_bgy[1] = (m_bgy[1] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16);
            if (m_ppu) m_ppu->update_bgy_internal(1, m_bgy[1]);
            break;

        // Window
        case 0x040: m_win0h = value; break;
        case 0x042: m_win1h = value; break;
        case 0x044: m_win0v = value; break;
        case 0x046: m_win1v = value; break;
        case 0x048: m_winin = value; break;
        case 0x04A: m_winout = value; break;

        // Effects
        case 0x04C: m_mosaic = value; break;
        case 0x050: m_bldcnt = value; break;
        case 0x052: m_bldalpha = value; break;
        case 0x054: m_bldy = value & 0x1F; break;

        // Sound (simplified, store for now)
        case 0x060 ... 0x080:
            m_sound_regs[(address - 0x060) >> 1] = value;
            break;

        // SOUNDCNT_H - Direct Sound control
        case 0x082:
            if (m_apu) m_apu->write_soundcnt_h(value);
            m_sound_regs[(address - 0x060) >> 1] = value;
            break;

        case 0x084 ... 0x09E:
            m_sound_regs[(address - 0x060) >> 1] = value;
            break;

        // FIFO_A (0x0A0) - accumulate 16-bit halves into 32-bit
        case 0x0A0:
            m_fifo_a_latch = value;
            break;
        case 0x0A2:
            if (m_apu) {
                uint32_t word = m_fifo_a_latch | (static_cast<uint32_t>(value) << 16);
                m_apu->write_fifo_a(word);
            }
            break;

        // FIFO_B (0x0A4)
        case 0x0A4:
            m_fifo_b_latch = value;
            break;
        case 0x0A6:
            if (m_apu) {
                uint32_t word = m_fifo_b_latch | (static_cast<uint32_t>(value) << 16);
                m_apu->write_fifo_b(word);
            }
            break;

        // DMA 0
        case 0x0B0: m_dma[0].src = (m_dma[0].src & 0xFFFF0000) | value; break;
        case 0x0B2: m_dma[0].src = (m_dma[0].src & 0x0000FFFF) | (value << 16); break;
        case 0x0B4: m_dma[0].dst = (m_dma[0].dst & 0xFFFF0000) | value; break;
        case 0x0B6: m_dma[0].dst = (m_dma[0].dst & 0x0000FFFF) | (value << 16); break;
        case 0x0B8: m_dma[0].count = value; break;
        case 0x0BA:
            GBA_DEBUG_PRINT("DMA0 control: 0x%04X (src=0x%08X, dst=0x%08X, cnt=%d)\n",
                           value, m_dma[0].src, m_dma[0].dst, m_dma[0].count);
            m_dma[0].control = value;
            if (value & 0x8000) trigger_dma(0);
            break;

        // DMA 1
        case 0x0BC: m_dma[1].src = (m_dma[1].src & 0xFFFF0000) | value; break;
        case 0x0BE: m_dma[1].src = (m_dma[1].src & 0x0000FFFF) | (value << 16); break;
        case 0x0C0: m_dma[1].dst = (m_dma[1].dst & 0xFFFF0000) | value; break;
        case 0x0C2: m_dma[1].dst = (m_dma[1].dst & 0x0000FFFF) | (value << 16); break;
        case 0x0C4: m_dma[1].count = value; break;
        case 0x0C6:
            GBA_DEBUG_PRINT("DMA1 control: 0x%04X (src=0x%08X, dst=0x%08X, cnt=%d)\n",
                           value, m_dma[1].src, m_dma[1].dst, m_dma[1].count);
            m_dma[1].control = value;
            if (value & 0x8000) trigger_dma(1);
            break;

        // DMA 2
        case 0x0C8: m_dma[2].src = (m_dma[2].src & 0xFFFF0000) | value; break;
        case 0x0CA: m_dma[2].src = (m_dma[2].src & 0x0000FFFF) | (value << 16); break;
        case 0x0CC: m_dma[2].dst = (m_dma[2].dst & 0xFFFF0000) | value; break;
        case 0x0CE: m_dma[2].dst = (m_dma[2].dst & 0x0000FFFF) | (value << 16); break;
        case 0x0D0: m_dma[2].count = value; break;
        case 0x0D2:
            GBA_DEBUG_PRINT("DMA2 control: 0x%04X (src=0x%08X, dst=0x%08X, cnt=%d)\n",
                           value, m_dma[2].src, m_dma[2].dst, m_dma[2].count);
            m_dma[2].control = value;
            if (value & 0x8000) trigger_dma(2);
            break;

        // DMA 3
        case 0x0D4: m_dma[3].src = (m_dma[3].src & 0xFFFF0000) | value; break;
        case 0x0D6: m_dma[3].src = (m_dma[3].src & 0x0000FFFF) | (value << 16); break;
        case 0x0D8: m_dma[3].dst = (m_dma[3].dst & 0xFFFF0000) | value; break;
        case 0x0DA: m_dma[3].dst = (m_dma[3].dst & 0x0000FFFF) | (value << 16); break;
        case 0x0DC: m_dma[3].count = value; break;
        case 0x0DE:
            GBA_DEBUG_PRINT("DMA3 control: 0x%04X (src=0x%08X, dst=0x%08X, cnt=%d)\n",
                           value, m_dma[3].src, m_dma[3].dst, m_dma[3].count);
            m_dma[3].control = value;
            if (value & 0x8000) trigger_dma(3);
            break;

        // Timers
        case 0x100: m_timers[0].reload = value; break;
        case 0x102: write_timer_control(0, value); break;
        case 0x104: m_timers[1].reload = value; break;
        case 0x106: write_timer_control(1, value); break;
        case 0x108: m_timers[2].reload = value; break;
        case 0x10A: write_timer_control(2, value); break;
        case 0x10C: m_timers[3].reload = value; break;
        case 0x10E: write_timer_control(3, value); break;

        // Key control
        case 0x132: m_keycnt = value; break;

        // Interrupts
        case 0x200:
            GBA_DEBUG_PRINT("IE write: 0x%04X (VBlank=%s, HBlank=%s, Timer0=%s)\n",
                           value, (value & 1) ? "Y" : "N", (value & 2) ? "Y" : "N", (value & 8) ? "Y" : "N");
            m_ie = value;
            break;
        case 0x202:
            GBA_DEBUG_PRINT("IF write (ack): 0x%04X (clearing IF from 0x%04X to 0x%04X)\n",
                           value, m_if, m_if & ~value);
            m_if &= ~value;
            // Also clear serviced tracking for these interrupts so they can re-trigger
            m_if_serviced &= ~value;
            break;  // Write 1 to clear
        case 0x204:
            m_waitcnt = value;
            break;
        case 0x208:
            GBA_DEBUG_PRINT("IME write: 0x%04X (Master IRQ %s)\n", value, (value & 1) ? "ENABLED" : "disabled");
            m_ime = value & 1;
            break;

        case 0x300: m_postflg = value & 1; break;
        case 0x301:
            m_haltcnt = value;
            // TODO: Enter halt or stop mode
            break;

        default:
            break;
    }
}

void Bus::set_input_state(uint32_t buttons) {
    // Convert from Veloce button layout to GBA KEYINPUT
    // GBA: bit 0=A, 1=B, 2=Select, 3=Start, 4=Right, 5=Left, 6=Up, 7=Down, 8=R, 9=L
    // Veloce: bit 0=A, 1=B, 2=X, 3=Y, 4=L, 5=R, 6=Start, 7=Select, 8=Up, 9=Down, 10=Left, 11=Right

    uint16_t key = 0x3FF;  // All released (active low)

    if (buttons & (1 << 0)) key &= ~0x001;  // A
    if (buttons & (1 << 1)) key &= ~0x002;  // B
    if (buttons & (1 << 7)) key &= ~0x004;  // Select
    if (buttons & (1 << 6)) key &= ~0x008;  // Start
    if (buttons & (1 << 11)) key &= ~0x010; // Right
    if (buttons & (1 << 10)) key &= ~0x020; // Left
    if (buttons & (1 << 8)) key &= ~0x040;  // Up
    if (buttons & (1 << 9)) key &= ~0x080;  // Down
    if (buttons & (1 << 5)) key &= ~0x100;  // R
    if (buttons & (1 << 4)) key &= ~0x200;  // L

    m_keyinput = key;

    // Check for keypad IRQ
    if (m_keycnt & 0x4000) {
        uint16_t keys_pressed = ~m_keyinput & 0x3FF;
        uint16_t keys_watched = m_keycnt & 0x3FF;

        bool trigger = false;
        if (m_keycnt & 0x8000) {
            // AND mode - all watched keys must be pressed
            trigger = (keys_pressed & keys_watched) == keys_watched;
        } else {
            // OR mode - any watched key pressed
            trigger = (keys_pressed & keys_watched) != 0;
        }

        if (trigger) {
            request_interrupt(GBAInterrupt::Keypad);
        }
    }
}

bool Bus::check_interrupts() {
    // Edge-triggered: only signal IRQ for pending interrupts that haven't been serviced yet
    // This prevents the same interrupt from re-triggering while the handler is still running
    uint16_t pending = m_ie & m_if;
    uint16_t unserviced = pending & ~m_if_serviced;

    if (m_ime && unserviced) {
        // Mark these interrupts as serviced so they won't re-trigger
        m_if_serviced |= unserviced;
        return true;
    }
    return false;
}

void Bus::request_interrupt(GBAInterrupt irq) {
    uint16_t irq_bit = static_cast<uint16_t>(irq);
    // Only log if this is a new interrupt being set
    if (!(m_if & irq_bit)) {
        GBA_DEBUG_PRINT("IRQ: request_interrupt 0x%04X (IE=0x%04X, IF=0x%04X->0x%04X, IME=%d)\n",
                        irq_bit, m_ie, m_if, m_if | irq_bit, m_ime);
    }
    m_if |= irq_bit;
    // Note: m_if_serviced is NOT modified here - the interrupt is eligible to fire
    // until check_interrupts() marks it as serviced
}

void Bus::trigger_dma(int channel) {
    DMAChannel& dma = m_dma[channel];

    int timing = (dma.control >> 12) & 3;

    // For immediate mode (timing = 0), schedule the DMA
    if (timing == 0) {
        schedule_dma(channel);
    }
    // Other timing modes are triggered by their respective events
    // (VBlank, HBlank, Sound FIFO)
}

int Bus::run_dma() {
    // Check if any DMA is scheduled to run
    int channel = find_highest_priority_dma();
    if (channel < 0) {
        return 0;  // No DMA pending
    }

    // Run the cycle-accurate DMA state machine
    // Give it enough cycles to complete typical transfers
    // The step_dma function will only consume what it needs
    constexpr int MAX_DMA_CYCLES = 65536;  // Generous limit
    return step_dma(MAX_DMA_CYCLES);
}

void Bus::run_dma_channel(int channel) {
    DMAChannel& dma = m_dma[channel];

    if (!(dma.control & 0x8000)) return;

    bool is_32bit = dma.control & 0x0400;
    int src_adj = (dma.control >> 7) & 3;
    int dst_adj = (dma.control >> 5) & 3;

    int step = is_32bit ? 4 : 2;

    // For sound FIFO DMA (channels 1 and 2 with timing mode 3), transfer 4 words
    int timing = (dma.control >> 12) & 3;
    uint32_t count = dma.internal_count;
    if (timing == 3 && (channel == 1 || channel == 2)) {
        count = 4;
        is_32bit = true;
        step = 4;
    }

    for (uint32_t j = 0; j < count; j++) {
        if (is_32bit) {
            write32(dma.internal_dst, read32(dma.internal_src));
        } else {
            write16(dma.internal_dst, read16(dma.internal_src));
        }

        // Adjust source
        switch (src_adj) {
            case 0: dma.internal_src += step; break;
            case 1: dma.internal_src -= step; break;
            case 2: break;  // Fixed
            case 3: dma.internal_src += step; break;  // Prohibited, behaves as increment
        }

        // Adjust destination (for FIFO DMA, destination is always fixed)
        if (timing == 3 && (channel == 1 || channel == 2)) {
            // FIFO DMA - destination stays fixed at FIFO address
        } else {
            switch (dst_adj) {
                case 0: dma.internal_dst += step; break;
                case 1: dma.internal_dst -= step; break;
                case 2: break;  // Fixed
                case 3: dma.internal_dst += step; break;  // Increment/Reload
            }
        }
    }

    // DMA complete
    if (dma.control & 0x4000) {
        request_interrupt(static_cast<GBAInterrupt>(0x0100 << channel));
    }

    if (!(dma.control & 0x0200)) {
        // Not repeating, disable
        dma.control &= ~0x8000;
        dma.active = false;
    } else {
        // Repeating DMA
        if (dst_adj == 3) {
            // Reload destination on repeat
            dma.internal_dst = dma.dst;
            // Mask based on channel
            if (channel == 0) {
                dma.internal_dst &= 0x07FFFFFF;
            } else {
                dma.internal_dst &= 0x0FFFFFFF;
            }
        }
        // Reload count
        dma.internal_count = dma.count;
        if (dma.internal_count == 0) {
            dma.internal_count = (channel == 3) ? 0x10000 : 0x4000;
        }
    }
}

void Bus::trigger_vblank_dma() {
    // Trigger DMAs with VBlank timing (timing mode 1)
    for (int i = 0; i < 4; i++) {
        DMAChannel& dma = m_dma[i];

        if (!(dma.control & 0x8000)) continue;

        int timing = (dma.control >> 12) & 3;
        if (timing == 1) {
            schedule_dma(i);
        }
    }
}

void Bus::trigger_hblank_dma() {
    // Trigger DMAs with HBlank timing (timing mode 2)
    for (int i = 0; i < 4; i++) {
        DMAChannel& dma = m_dma[i];

        if (!(dma.control & 0x8000)) continue;

        int timing = (dma.control >> 12) & 3;
        if (timing == 2) {
            schedule_dma(i);
        }
    }
}

void Bus::trigger_sound_fifo_dma(int fifo_idx) {
    // Trigger Sound FIFO DMA (timing mode 3) for channels 1 and 2
    // FIFO_A uses DMA1, FIFO_B uses DMA2
    // But games can also use different channels, so check destination address
    for (int i = 1; i <= 2; i++) {
        DMAChannel& dma = m_dma[i];

        if (!(dma.control & 0x8000)) continue;

        int timing = (dma.control >> 12) & 3;
        if (timing != 3) continue;  // Not sound FIFO mode

        // Check if destination matches the requested FIFO
        // FIFO_A = 0x040000A0, FIFO_B = 0x040000A4
        uint32_t expected_dst = (fifo_idx == 0) ? 0x040000A0 : 0x040000A4;
        if ((dma.dst & 0x0FFFFFFF) == (expected_dst & 0x0FFFFFFF)) {
            GBA_DEBUG_PRINT("Sound FIFO DMA: Triggering DMA%d for FIFO_%c\n",
                           i, fifo_idx == 0 ? 'A' : 'B');
            schedule_dma(i);
        }
    }
}

// ============================================================================
// Cycle-Accurate DMA Implementation
// ============================================================================

// Schedule a DMA to start (called when DMA is triggered)
void Bus::schedule_dma(int channel) {
    DMAChannel& dma = m_dma[channel];

    if (dma.phase != DMAChannel::Phase::Idle) {
        // Already running or scheduled
        return;
    }

    // Set up internal registers if this is the first trigger
    if (!dma.active) {
        dma.internal_src = dma.src;
        dma.internal_dst = dma.dst;
        dma.active = true;

        // Mask addresses based on channel
        if (channel == 0) {
            dma.internal_src &= 0x07FFFFFF;
            dma.internal_dst &= 0x07FFFFFF;
        } else {
            dma.internal_src &= 0x0FFFFFFF;
            dma.internal_dst &= 0x0FFFFFFF;
        }
    }

    dma.internal_count = dma.count;
    if (dma.internal_count == 0) {
        dma.internal_count = (channel == 3) ? 0x10000 : 0x4000;
    }

    // Start the 2-cycle startup delay
    dma.phase = DMAChannel::Phase::Startup;
    dma.startup_countdown = 2;
    dma.current_unit = 0;
    dma.first_access = true;
    dma.scheduled = true;

    GBA_DEBUG_PRINT("DMA%d: Scheduled, src=0x%08X, dst=0x%08X, count=%d\n",
                   channel, dma.internal_src, dma.internal_dst, dma.internal_count);
}

// Find the highest priority DMA that's ready to run
int Bus::find_highest_priority_dma() {
    for (int i = 0; i < 4; i++) {
        DMAChannel& dma = m_dma[i];
        if (dma.phase != DMAChannel::Phase::Idle && dma.scheduled) {
            return i;
        }
    }
    return -1;
}

// Get access cycles for DMA transfer
int Bus::get_dma_access_cycles(uint32_t address, bool is_sequential, bool is_32bit) {
    int access_size = is_32bit ? 32 : 16;
    return get_wait_states(address, is_sequential, access_size) + 1;
}

// Complete a DMA transfer
void Bus::complete_dma(int channel) {
    DMAChannel& dma = m_dma[channel];

    GBA_DEBUG_PRINT("DMA%d: Complete, transferred %d units\n", channel, dma.current_unit);

    // Request interrupt if enabled
    if (dma.control & 0x4000) {
        request_interrupt(static_cast<GBAInterrupt>(0x0100 << channel));
    }

    // Check if repeating
    if (!(dma.control & 0x0200)) {
        // Not repeating, disable
        dma.control &= ~0x8000;
        dma.active = false;
        dma.phase = DMAChannel::Phase::Idle;
        dma.scheduled = false;
    } else {
        // Repeating DMA - reload for next trigger
        int dst_adj = (dma.control >> 5) & 3;
        if (dst_adj == 3) {
            // Reload destination on repeat
            dma.internal_dst = dma.dst;
            if (channel == 0) {
                dma.internal_dst &= 0x07FFFFFF;
            } else {
                dma.internal_dst &= 0x0FFFFFFF;
            }
        }
        // Reload count
        dma.internal_count = dma.count;
        if (dma.internal_count == 0) {
            dma.internal_count = (channel == 3) ? 0x10000 : 0x4000;
        }
        dma.current_unit = 0;
        dma.first_access = true;
        dma.phase = DMAChannel::Phase::Idle;
        dma.scheduled = false;
    }

    // Find next DMA to run
    m_active_dma = find_highest_priority_dma();
}

// Step DMA for a given number of cycles
// Returns the number of cycles consumed by DMA
int Bus::step_dma(int available_cycles) {
    int cycles_used = 0;

    while (available_cycles > 0) {
        // Find highest priority DMA to run
        int channel = find_highest_priority_dma();
        if (channel < 0) {
            // No DMA pending
            break;
        }

        // Check if a higher priority DMA should preempt current
        if (m_active_dma >= 0 && channel < m_active_dma) {
            // Higher priority DMA is preempting
            GBA_DEBUG_PRINT("DMA%d: Preempting DMA%d\n", channel, m_active_dma);
        }
        m_active_dma = channel;

        DMAChannel& dma = m_dma[channel];
        bool is_32bit = dma.control & 0x0400;

        // For sound FIFO DMA, force 32-bit and 4 transfers
        int timing = (dma.control >> 12) & 3;
        uint32_t transfer_count = dma.internal_count;
        if (timing == 3 && (channel == 1 || channel == 2)) {
            transfer_count = 4;
            is_32bit = true;
        }

        switch (dma.phase) {
            case DMAChannel::Phase::Startup: {
                // Consume startup delay cycles
                int delay = std::min(available_cycles, dma.startup_countdown);
                dma.startup_countdown -= delay;
                cycles_used += delay;
                available_cycles -= delay;

                if (dma.startup_countdown <= 0) {
                    // Startup complete, begin transfer
                    dma.phase = DMAChannel::Phase::Read;
                }
                break;
            }

            case DMAChannel::Phase::Read: {
                // Calculate read cycles
                bool src_seq = !dma.first_access;
                int read_cycles = get_dma_access_cycles(dma.internal_src, src_seq, is_32bit);

                if (available_cycles < read_cycles) {
                    // Not enough cycles to complete read
                    return cycles_used;
                }

                // Perform the read
                if (is_32bit) {
                    dma.latch = read32(dma.internal_src);
                } else {
                    dma.latch = read16(dma.internal_src);
                }

                cycles_used += read_cycles;
                available_cycles -= read_cycles;
                dma.phase = DMAChannel::Phase::Write;
                break;
            }

            case DMAChannel::Phase::Write: {
                // Calculate write cycles
                bool dst_seq = !dma.first_access;
                int write_cycles = get_dma_access_cycles(dma.internal_dst, dst_seq, is_32bit);

                if (available_cycles < write_cycles) {
                    // Not enough cycles to complete write
                    return cycles_used;
                }

                // Perform the write
                if (is_32bit) {
                    write32(dma.internal_dst, dma.latch);
                } else {
                    write16(dma.internal_dst, static_cast<uint16_t>(dma.latch));
                }

                cycles_used += write_cycles;
                available_cycles -= write_cycles;

                // Update addresses
                int step = is_32bit ? 4 : 2;
                int src_adj = (dma.control >> 7) & 3;
                int dst_adj = (dma.control >> 5) & 3;

                switch (src_adj) {
                    case 0: dma.internal_src += step; break;
                    case 1: dma.internal_src -= step; break;
                    case 2: break;  // Fixed
                    case 3: dma.internal_src += step; break;  // Prohibited
                }

                // FIFO DMA keeps destination fixed
                if (timing == 3 && (channel == 1 || channel == 2)) {
                    // Destination stays fixed
                } else {
                    switch (dst_adj) {
                        case 0: dma.internal_dst += step; break;
                        case 1: dma.internal_dst -= step; break;
                        case 2: break;  // Fixed
                        case 3: dma.internal_dst += step; break;  // Increment/Reload
                    }
                }

                dma.current_unit++;
                dma.first_access = false;

                // Check if transfer complete
                if (dma.current_unit >= transfer_count) {
                    dma.phase = DMAChannel::Phase::Complete;
                } else {
                    // Continue with next unit
                    dma.phase = DMAChannel::Phase::Read;
                }
                break;
            }

            case DMAChannel::Phase::Complete:
                complete_dma(channel);
                break;

            case DMAChannel::Phase::Idle:
                // Shouldn't happen, but handle it
                break;
        }
    }

    return cycles_used;
}

// ============================================================================
// End Cycle-Accurate DMA Implementation
// ============================================================================

// Compute timer counter value on-the-fly for accurate reads during polling
// This is critical for games that poll timers in tight loops (like Pokemon Fire Red)
//
// GBA Timer behavior:
// - Timer starts counting 2 cycles after the enable bit is written
// - Counter increments by 1 every (prescaler) cycles
// - When counter overflows (0xFFFF -> 0x0000), it reloads from the reload register
// - The reload value used is the one at the time of overflow, not when enabled
// - Writing to reload while timer is running only affects the NEXT overflow
uint16_t Bus::get_timer_counter(int idx) {
    Timer& timer = m_timers[idx];

    // If timer is disabled, return the frozen counter value
    if (!(timer.control & 0x80)) {
        return timer.counter;
    }

    // Cascade mode timers are only updated by the previous timer's overflow
    // so we return the stored counter value (already incremented by step_timers)
    if ((timer.control & 0x04) && idx > 0) {
        return timer.counter;
    }

    // Calculate current value based on elapsed cycles since timer was enabled
    static const int prescaler_shifts[] = {0, 6, 8, 10};  // 1, 64, 256, 1024 -> shift amounts
    int shift = prescaler_shifts[timer.control & 3];

    // Calculate elapsed cycles since timer was enabled
    uint64_t elapsed = m_global_cycles - timer.last_enabled_cycle;

    // Calculate total ticks elapsed
    // For prescaler 0 (F/1), timer increments every cycle
    // For prescaler 1 (F/64), timer increments every 64 cycles, etc.
    uint64_t ticks = elapsed >> shift;

    // The timer counter starts at the reload value when enabled
    // It counts up to 0xFFFF, then overflows back to reload
    // We need to track based on the initial reload value, not the current one
    // (since reload can be modified while running and only takes effect on overflow)

    // Calculate how many overflows have occurred and current position
    uint32_t initial_reload = timer.initial_reload;
    uint32_t range = 0x10000 - initial_reload;  // Ticks from initial_reload to overflow

    if (range == 0) {
        // Reload was 0x10000 (shouldn't happen, but handle edge case)
        // Timer overflows immediately every tick
        return static_cast<uint16_t>(timer.reload);
    }

    // Current counter = initial_reload + (ticks mod range)
    // When ticks reaches range, counter overflows and reloads
    uint32_t position = ticks % range;
    uint32_t current = initial_reload + position;

    return static_cast<uint16_t>(current);
}

void Bus::write_timer_control(int timer_idx, uint16_t value) {
    Timer& timer = m_timers[timer_idx];
    uint16_t old_control = timer.control;

    // Get PC for debugging timer writes
    uint32_t pc = m_cpu ? m_cpu->get_pc() : 0;
    GBA_DEBUG_PRINT("Timer%d control: 0x%04X -> 0x%04X (enabled=%s, cascade=%s, prescaler=%d) from PC=0x%08X\n",
                   timer_idx, old_control, value,
                   (value & 0x80) ? "Y" : "N",
                   (value & 0x04) ? "Y" : "N",
                   value & 3, pc);

    // When timer is enabled (bit 7 goes from 0 to 1), reload counter
    bool was_enabled = old_control & 0x80;
    bool now_enabled = value & 0x80;

    if (!was_enabled && now_enabled) {
        // Timer is being enabled - reload counter and reset prescaler
        // Timer starts counting on the cycle AFTER the enable write completes
        timer.counter = timer.reload;
        timer.initial_reload = timer.reload;  // Snapshot reload value for this counting cycle
        timer.prescaler_counter = 0;
        // Record the current cycle - the timer will start counting from the next cycle
        timer.last_enabled_cycle = m_global_cycles;
        GBA_DEBUG_PRINT("Timer%d: Enabled, reload=0x%04X, starting at cycle %llu\n",
                       timer_idx, timer.reload, (unsigned long long)timer.last_enabled_cycle);
    } else if (was_enabled && !now_enabled) {
        // Timer is being disabled - freeze the counter value by computing it now
        // This ensures we have the correct value when read while disabled
        timer.counter = get_timer_counter(timer_idx);
        GBA_DEBUG_PRINT("Timer%d: Disabled, frozen at counter=0x%04X\n", timer_idx, timer.counter);
    }

    timer.control = value;
}

void Bus::step_timers(int cycles) {
    // Update global cycle counter for accurate timer reads
    m_global_cycles += cycles;

    static const int prescaler_values[] = {1, 64, 256, 1024};

    for (int i = 0; i < 4; i++) {
        Timer& timer = m_timers[i];

        // Skip if not enabled
        if (!(timer.control & 0x80)) continue;

        // Skip if cascade mode (handled by previous timer)
        if ((timer.control & 0x04) && i > 0) continue;

        int prescaler = prescaler_values[timer.control & 3];
        timer.prescaler_counter += cycles;

        while (timer.prescaler_counter >= prescaler) {
            timer.prescaler_counter -= prescaler;
            timer.counter++;

            if (timer.counter == 0) {
                // Overflow - reload counter with the current reload value
                // (not initial_reload - the reload register can be updated mid-cycle
                // and the new value is used on the NEXT overflow)
                timer.counter = timer.reload;

                // Update initial_reload for the next counting cycle
                // This is used by get_timer_counter() to track elapsed ticks
                timer.initial_reload = timer.reload;

                // Reset the reference point for accurate reads
                // The timer is now counting from reload to 0xFFFF again
                timer.last_enabled_cycle = m_global_cycles;
                timer.prescaler_counter = 0;

                // Request interrupt if enabled
                if (timer.control & 0x40) {
                    request_interrupt(static_cast<GBAInterrupt>(0x0008 << i));
                }

                // Notify APU of timer overflow for Direct Sound
                if (m_apu && (i == 0 || i == 1)) {
                    m_apu->on_timer_overflow(i);
                }

                // Handle cascade to next timer
                if (i < 3 && (m_timers[i + 1].control & 0x84) == 0x84) {
                    m_timers[i + 1].counter++;
                    if (m_timers[i + 1].counter == 0) {
                        m_timers[i + 1].counter = m_timers[i + 1].reload;
                        m_timers[i + 1].initial_reload = m_timers[i + 1].reload;
                        if (m_timers[i + 1].control & 0x40) {
                            // Timer interrupts are at bits 3-6 (0x0008, 0x0010, 0x0020, 0x0040)
                            // For timer i+1, the interrupt is 0x0008 << (i+1)
                            request_interrupt(static_cast<GBAInterrupt>(0x0008 << (i + 1)));
                        }
                        // Also notify APU for cascaded timer 1
                        if (m_apu && (i + 1 == 0 || i + 1 == 1)) {
                            m_apu->on_timer_overflow(i + 1);
                        }
                    }
                }
            }
        }
    }
}

uint32_t Bus::get_open_bus_value(uint32_t address) {
    (void)address;
    return m_last_read_value;
}

void Bus::save_state(std::vector<uint8_t>& data) {
    // Save EWRAM
    data.insert(data.end(), m_ewram.begin(), m_ewram.end());

    // Save IWRAM
    data.insert(data.end(), m_iwram.begin(), m_iwram.end());

    // Save key I/O registers
    auto save16 = [&data](uint16_t val) {
        data.push_back(val & 0xFF);
        data.push_back(val >> 8);
    };

    save16(m_dispcnt);
    save16(m_dispstat);
    save16(m_vcount);
    save16(m_ie);
    save16(m_if);
    save16(m_ime);
    save16(m_keyinput);
    save16(m_if_serviced);

    // TODO: Save more state as needed
}

void Bus::load_state(const uint8_t*& data, size_t& remaining) {
    // Load EWRAM
    std::memcpy(m_ewram.data(), data, m_ewram.size());
    data += m_ewram.size();
    remaining -= m_ewram.size();

    // Load IWRAM
    std::memcpy(m_iwram.data(), data, m_iwram.size());
    data += m_iwram.size();
    remaining -= m_iwram.size();

    // Load key I/O registers
    auto load16 = [&data, &remaining]() {
        uint16_t val = data[0] | (data[1] << 8);
        data += 2;
        remaining -= 2;
        return val;
    };

    m_dispcnt = load16();
    m_dispstat = load16();
    m_vcount = load16();
    m_ie = load16();
    m_if = load16();
    m_ime = load16();
    m_keyinput = load16();
    m_if_serviced = load16();
}

void Bus::flush_debug_string() {
    // Output the debug string buffer with [GBA/DEBUG] prefix
    // This is used by test ROMs that support mGBA's debug console
    if (m_debug_string_pos > 0) {
        // Ensure null termination
        m_debug_string[m_debug_string_pos] = '\0';

        // Determine log level from flags
        const char* level = "DEBUG";
        int log_level = m_debug_flags & 0x7;
        switch (log_level) {
            case 0: level = "FATAL"; break;
            case 1: level = "ERROR"; break;
            case 2: level = "WARN"; break;
            case 3: level = "INFO"; break;
            case 4: level = "DEBUG"; break;
            default: level = "LOG"; break;
        }

        // Output the debug message
        fprintf(stderr, "[GBA/%s] %s\n", level, m_debug_string.data());

        // Reset buffer for next message
        m_debug_string.fill('\0');
        m_debug_string_pos = 0;
    }
}

} // namespace gba
