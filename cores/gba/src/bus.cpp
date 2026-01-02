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
    // This is the standard GBA BIOS IRQ handler that:
    // 1. Saves registers R0-R3, R12, LR to stack
    // 2. Reads user IRQ handler from 0x03FFFFFC (mirrors 0x03007FFC)
    // 3. Calls user handler
    // 4. Restores registers and returns from IRQ
    //
    // Assembly:
    //   0x18: stmfd  sp!, {r0-r3, r12, lr}    ; E92D500F
    //   0x1C: mov    r0, #0x04000000          ; E3A00301
    //   0x20: add    lr, pc, #0               ; E28FE000
    //   0x24: ldr    pc, [r0, #-4]            ; E510F004
    //   0x28: ldmfd  sp!, {r0-r3, r12, lr}    ; E8BD500F
    //   0x2C: subs   pc, lr, #4               ; E25EF004
    //
    // Also need exception vectors at 0x00-0x1C (branches to handlers):
    //   0x00: b 0x00000020 (reset - loop forever for HLE)  ; EA000006
    //   0x04: b 0x00000004 (undefined - loop)              ; EAFFFFFE
    //   0x08: b swi_handler (SWI - handled by HLE)         ; EA000006 (to 0x28 but we use HLE)
    //   0x0C: b 0x0000000C (prefetch abort - loop)         ; EAFFFFFE
    //   0x10: b 0x00000010 (data abort - loop)             ; EAFFFFFE
    //   0x14: b 0x00000014 (reserved - loop)               ; EAFFFFFE
    //   0x18: IRQ handler starts here

    auto write_word = [this](uint32_t addr, uint32_t value) {
        m_bios[addr + 0] = value & 0xFF;
        m_bios[addr + 1] = (value >> 8) & 0xFF;
        m_bios[addr + 2] = (value >> 16) & 0xFF;
        m_bios[addr + 3] = (value >> 24) & 0xFF;
    };

    // IRQ handler at 0x18 (this is where CPU jumps on IRQ)
    write_word(0x18, 0xE92D500F);  // stmfd sp!, {r0-r3, r12, lr}
    write_word(0x1C, 0xE3A00301);  // mov r0, #0x04000000
    write_word(0x20, 0xE28FE000);  // add lr, pc, #0  (LR = PC+8 = addr of ldmfd)
    write_word(0x24, 0xE510F004);  // ldr pc, [r0, #-4]  (jump to [0x03FFFFFC])
    write_word(0x28, 0xE8BD500F);  // ldmfd sp!, {r0-r3, r12, lr}
    write_word(0x2C, 0xE25EF004);  // subs pc, lr, #4  (return from IRQ)

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
        case MemoryRegion::ROM_WS2:
            if (m_cartridge) {
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
        case MemoryRegion::ROM_WS2:
            if (m_cartridge) {
                uint32_t offset = address & 0x1FFFFFF;
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
        if (m_cpu && m_cpu->get_pc() < 0x4000) {
            m_last_bios_read = m_bios[address] |
                               (m_bios[address + 1] << 8) |
                               (m_bios[address + 2] << 16) |
                               (m_bios[address + 3] << 24);
            return m_last_bios_read;
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

        case MemoryRegion::IWRAM:
            m_iwram[address & 0x7FFF] = value;
            break;

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
        case MemoryRegion::ROM_WS2:
            // ROM is normally read-only, but GPIO ports are at 0x080000C4-0x080000C9
            if (m_cartridge) {
                uint32_t rom_addr = address & 0x1FFFFFF;
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
        case MemoryRegion::ROM_WS2:
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
        case 0x060 ... 0x0A6:
            return m_sound_regs[(address - 0x060) >> 1];

        // DMA (return control registers only)
        case 0x0BA: return m_dma[0].control;
        case 0x0C6: return m_dma[1].control;
        case 0x0D2: return m_dma[2].control;
        case 0x0DE: return m_dma[3].control;

        // Timers
        case 0x100: return m_timers[0].counter;
        case 0x102: return m_timers[0].control;
        case 0x104: return m_timers[1].counter;
        case 0x106: return m_timers[1].control;
        case 0x108: return m_timers[2].counter;
        case 0x10A: return m_timers[2].control;
        case 0x10C: return m_timers[3].counter;
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
    address &= 0xFFF;

    switch (address) {
        // Display
        case 0x000:
            m_dispcnt = value;
            if (m_ppu) m_ppu->sync_registers(m_dispcnt, m_dispstat);
            break;
        case 0x004:
            // Only bits 3-5 (IRQ enables) and 8-15 (VCount target) are writable in DISPSTAT
            // Bits 0-2 are read-only status flags maintained by PPU
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
        case 0x028: m_bgx[0] = (m_bgx[0] & 0xFFFF0000) | value; break;
        case 0x02A: m_bgx[0] = (m_bgx[0] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16); break;
        case 0x02C: m_bgy[0] = (m_bgy[0] & 0xFFFF0000) | value; break;
        case 0x02E: m_bgy[0] = (m_bgy[0] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16); break;

        // BG3 Affine
        case 0x030: m_bgpa[1] = static_cast<int16_t>(value); break;
        case 0x032: m_bgpb[1] = static_cast<int16_t>(value); break;
        case 0x034: m_bgpc[1] = static_cast<int16_t>(value); break;
        case 0x036: m_bgpd[1] = static_cast<int16_t>(value); break;
        case 0x038: m_bgx[1] = (m_bgx[1] & 0xFFFF0000) | value; break;
        case 0x03A: m_bgx[1] = (m_bgx[1] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16); break;
        case 0x03C: m_bgy[1] = (m_bgy[1] & 0xFFFF0000) | value; break;
        case 0x03E: m_bgy[1] = (m_bgy[1] & 0x0000FFFF) | (static_cast<int32_t>(value) << 16); break;

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
        case 0x060 ... 0x0A6:
            m_sound_regs[(address - 0x060) >> 1] = value;
            break;

        // DMA 0
        case 0x0B0: m_dma[0].src = (m_dma[0].src & 0xFFFF0000) | value; break;
        case 0x0B2: m_dma[0].src = (m_dma[0].src & 0x0000FFFF) | (value << 16); break;
        case 0x0B4: m_dma[0].dst = (m_dma[0].dst & 0xFFFF0000) | value; break;
        case 0x0B6: m_dma[0].dst = (m_dma[0].dst & 0x0000FFFF) | (value << 16); break;
        case 0x0B8: m_dma[0].count = value; break;
        case 0x0BA:
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
        case 0x200: m_ie = value; break;
        case 0x202: m_if &= ~value; break;  // Write 1 to clear
        case 0x204:
            m_waitcnt = value;
            // TODO: Update wait state tables
            break;
        case 0x208: m_ime = value & 1; break;

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
    return m_ime && (m_ie & m_if);
}

void Bus::request_interrupt(GBAInterrupt irq) {
    m_if |= static_cast<uint16_t>(irq);
}

void Bus::trigger_dma(int channel) {
    DMAChannel& dma = m_dma[channel];

    int timing = (dma.control >> 12) & 3;

    // Copy addresses for transfer (only on initial trigger, not on repeat)
    if (!dma.active) {
        dma.internal_src = dma.src;
        dma.internal_dst = dma.dst;
        dma.active = true;
    }
    dma.internal_count = dma.count;

    // Mask addresses based on channel
    if (channel == 0) {
        dma.internal_src &= 0x07FFFFFF;
        dma.internal_dst &= 0x07FFFFFF;
    } else {
        dma.internal_src &= 0x0FFFFFFF;
        dma.internal_dst &= 0x0FFFFFFF;
    }

    // If count is 0, use max value
    if (dma.internal_count == 0) {
        dma.internal_count = (channel == 3) ? 0x10000 : 0x4000;
    }

    // For immediate mode (timing = 0), run the DMA now
    if (timing == 0) {
        run_dma_channel(channel);
    }
}

int Bus::run_dma() {
    // Run pending immediate DMAs in priority order
    for (int i = 0; i < 4; i++) {
        DMAChannel& dma = m_dma[i];

        // Check if DMA is enabled and has immediate timing
        if (!(dma.control & 0x8000)) continue;

        int timing = (dma.control >> 12) & 3;
        if (timing != 0) continue;  // Only handle immediate DMAs

        run_dma_channel(i);
        bool is_32bit = dma.control & 0x0400;
        return dma.internal_count * (is_32bit ? 4 : 2);
    }

    return 0;
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
            run_dma_channel(i);
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
            run_dma_channel(i);
        }
    }
}

void Bus::write_timer_control(int timer_idx, uint16_t value) {
    Timer& timer = m_timers[timer_idx];
    uint16_t old_control = timer.control;

    // When timer is enabled (bit 7 goes from 0 to 1), reload counter
    bool was_enabled = old_control & 0x80;
    bool now_enabled = value & 0x80;

    if (!was_enabled && now_enabled) {
        // Timer is being enabled - reload counter and reset prescaler
        timer.counter = timer.reload;
        timer.prescaler_counter = 0;
    }

    timer.control = value;
}

void Bus::step_timers(int cycles) {
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
                // Overflow
                timer.counter = timer.reload;

                // Request interrupt if enabled
                if (timer.control & 0x40) {
                    request_interrupt(static_cast<GBAInterrupt>(0x0008 << i));
                }

                // Handle cascade to next timer
                if (i < 3 && (m_timers[i + 1].control & 0x84) == 0x84) {
                    m_timers[i + 1].counter++;
                    if (m_timers[i + 1].counter == 0) {
                        m_timers[i + 1].counter = m_timers[i + 1].reload;
                        if (m_timers[i + 1].control & 0x40) {
                            // Timer interrupts are at bits 3-6 (0x0008, 0x0010, 0x0020, 0x0040)
                            // For timer i+1, the interrupt is 0x0008 << (i+1)
                            request_interrupt(static_cast<GBAInterrupt>(0x0008 << (i + 1)));
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
}

} // namespace gba
