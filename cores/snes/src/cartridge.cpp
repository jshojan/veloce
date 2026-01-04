#include "cartridge.hpp"
#include "debug.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace snes {

// CRC32 lookup table
static const uint32_t s_crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

Cartridge::Cartridge() = default;

Cartridge::~Cartridge() = default;

bool Cartridge::load(const uint8_t* data, size_t size) {
    // Minimum size check - SNES ROMs are at least 32KB
    if (size < 0x8000) {
        std::cerr << "ROM file too small (minimum 32KB)" << std::endl;
        return false;
    }

    // Check for and skip optional 512-byte copier header
    // These headers are a legacy from SNES copiers and some ROM dumps include them
    size_t rom_offset = 0;
    bool has_copier_header = (size % 1024) == 512;

    if (has_copier_header) {
        // Check if ROM data is correctly aligned by looking for the header
        // A valid LoROM header at $7FC0+512 (with copier header skip) should have valid checksum
        // But some ROMs have misaligned data where the copier header was added incorrectly

        // Score header at $7FC0 + 512 (standard with header skip)
        int score_with_skip = 0;
        if (size >= 0x8000 + 512) {
            score_with_skip = score_header(data + 0x7FC0 + 512, size - 512);
        }

        // Score header at $7FC0 without skip (misaligned ROM)
        // This would mean the "copier header" is actually padding that was added incorrectly
        int score_without_skip = 0;
        if (size >= 0x8000) {
            score_without_skip = score_header(data + 0x7FC0, size);
        }

        SNES_DEBUG_PRINT("Header check: with_skip=%d, without_skip=%d\n",
                         score_with_skip, score_without_skip);

        // If header scores better WITHOUT the skip, the ROM data is misaligned
        // In this case, don't skip the header - the file has incorrect padding
        if (score_with_skip > score_without_skip && score_with_skip > 0) {
            rom_offset = 512;
            size -= 512;
            SNES_DEBUG_PRINT("Detected and skipping 512-byte copier header\n");
        } else if (score_without_skip > 0) {
            // ROM data is at offset 0, the "copier header" is actually junk padding
            // Don't skip it - treat the whole file as ROM
            SNES_DEBUG_PRINT("ROM has 512-byte padding but data is aligned without skip - keeping full file\n");
        } else {
            // Neither works well, try the standard approach
            rom_offset = 512;
            size -= 512;
            SNES_DEBUG_PRINT("Detected and skipping 512-byte copier header (fallback)\n");
        }
    }

    // Copy ROM data (without copier header if applicable)
    m_rom.resize(size);
    std::memcpy(m_rom.data(), data + rom_offset, size);

    // Detect and parse header
    if (!detect_header(m_rom.data(), m_rom.size())) {
        std::cerr << "Failed to detect valid SNES ROM header" << std::endl;
        m_rom.clear();
        return false;
    }

    // For LoROM with copier header, check if ROM data is at expected offsets
    // Some ROM dumps have data misaligned by 512 bytes due to incorrect header handling during dump
    if (m_mapper_type == MapperType::LoROM && has_copier_header && rom_offset == 512 && m_rom.size() >= 0x18000) {
        // Check if bank 1 start (ROM $8000) is all zeros but data exists 512 bytes earlier
        // This indicates the copier header was added without properly shifting the ROM data
        bool bank1_start_zero = true;
        for (size_t i = 0; i < 16; i++) {
            if (m_rom[0x8000 + i] != 0) {
                bank1_start_zero = false;
                break;
            }
        }

        // Also check bank 2 area that games commonly use (e.g., graphics data)
        bool bank2_dma_zero = true;
        for (size_t i = 0; i < 16; i++) {
            if (m_rom[0x11000 + i] != 0) {  // Common DMA source for graphics
                bank2_dma_zero = false;
                break;
            }
        }

        if (bank1_start_zero && bank2_dma_zero) {
            std::cerr << "WARNING: ROM data appears misaligned." << std::endl;
            std::cerr << "This ROM dump may have incorrect header handling." << std::endl;
            std::cerr << "Graphics and some game data may not load correctly." << std::endl;
            std::cerr << "Consider obtaining a verified ROM dump from a trusted source." << std::endl;
        }
    }

    // Debug: verify ROM data at key locations
    if (m_rom.size() >= 0x12000) {
        fprintf(stderr, "[SNES] ROM verification - first bytes at key offsets:\n");
        fprintf(stderr, "  ROM[0x0000]: %02X %02X %02X %02X (expected: 78 9c 00 42)\n",
                m_rom[0], m_rom[1], m_rom[2], m_rom[3]);
        fprintf(stderr, "  ROM[0x8000]: %02X %02X %02X %02X\n",
                m_rom[0x8000], m_rom[0x8001], m_rom[0x8002], m_rom[0x8003]);
        fprintf(stderr, "  ROM[0x11000]: %02X %02X %02X %02X (DMA source for VRAM $A000)\n",
                m_rom[0x11000], m_rom[0x11001], m_rom[0x11002], m_rom[0x11003]);
    }

    // Calculate CRC32 of ROM data
    m_crc32 = calculate_crc32(m_rom.data(), m_rom.size());

    m_loaded = true;

    // Print ROM info
    std::cout << "SNES ROM loaded successfully:" << std::endl;
    std::cout << "  Title: " << m_title << std::endl;
    std::cout << "  Mapper: ";
    switch (m_mapper_type) {
        case MapperType::LoROM: std::cout << "LoROM"; break;
        case MapperType::HiROM: std::cout << "HiROM"; break;
        case MapperType::ExHiROM: std::cout << "ExHiROM"; break;
        case MapperType::SA1: std::cout << "SA-1"; break;
        case MapperType::SDD1: std::cout << "S-DD1"; break;
        case MapperType::SuperFX: std::cout << "SuperFX"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << std::endl;
    std::cout << "  ROM size: " << m_rom.size() / 1024 << " KB" << std::endl;
    std::cout << "  SRAM size: " << m_sram.size() / 1024 << " KB" << std::endl;
    std::cout << "  Battery: " << (m_has_battery ? "Yes" : "No") << std::endl;
    std::cout << "  FastROM: " << (m_fast_rom ? "Yes" : "No") << std::endl;
    std::cout << "  CRC32: " << std::hex << m_crc32 << std::dec << std::endl;

    return true;
}

void Cartridge::unload() {
    m_rom.clear();
    m_sram.clear();
    m_loaded = false;
    m_crc32 = 0;
    m_title.clear();
    m_mapper_type = MapperType::Unknown;
    m_enhancement_chip = EnhancementChip::None;
    m_has_battery = false;
    m_has_ram = false;
    m_fast_rom = false;
}

void Cartridge::reset() {
    // SRAM persists across resets if battery-backed
    // Otherwise clear it
    if (!m_has_battery) {
        std::fill(m_sram.begin(), m_sram.end(), 0);
    }
}

bool Cartridge::detect_header(const uint8_t* data, size_t size) {
    // SNES headers can be at several locations:
    // LoROM:   $007FC0 (32KB into ROM, maps to $00:FFC0)
    // HiROM:   $00FFC0 (64KB into ROM, maps to $40:FFC0)
    // ExHiROM: $40FFC0 (4MB + 64KB into ROM)

    // Score each possible header location
    int lorom_score = 0;
    int hirom_score = 0;
    int exhirom_score = 0;

    // LoROM header at $7FC0
    if (size >= 0x8000) {
        lorom_score = score_header(data + 0x7FC0, size);
    }

    // HiROM header at $FFC0
    if (size >= 0x10000) {
        hirom_score = score_header(data + 0xFFC0, size);
    }

    // ExHiROM header at $40FFC0
    if (size >= 0x410000) {
        exhirom_score = score_header(data + 0x40FFC0, size);
    }

    SNES_DEBUG_PRINT("Header scores - LoROM: %d, HiROM: %d, ExHiROM: %d\n",
                     lorom_score, hirom_score, exhirom_score);

    // Choose the best match
    if (exhirom_score > lorom_score && exhirom_score > hirom_score && exhirom_score > 0) {
        m_header_offset = 0x40FFC0;
        m_mapper_type = MapperType::ExHiROM;
    } else if (hirom_score > lorom_score && hirom_score > 0) {
        m_header_offset = 0xFFC0;
        m_mapper_type = MapperType::HiROM;
    } else if (lorom_score > 0) {
        m_header_offset = 0x7FC0;
        m_mapper_type = MapperType::LoROM;
    } else {
        // Try to guess from ROM size
        if (size > 0x400000) {
            m_header_offset = 0x40FFC0;
            m_mapper_type = MapperType::ExHiROM;
        } else if (size > 0x8000) {
            // Default to LoROM for smaller ROMs
            m_header_offset = 0x7FC0;
            m_mapper_type = MapperType::LoROM;
        } else {
            std::cerr << "Unable to detect ROM type" << std::endl;
            return false;
        }
    }

    return parse_header(data + m_header_offset);
}

int Cartridge::score_header(const uint8_t* header_data, size_t rom_size) {
    int score = 0;

    // Check if header is within ROM bounds
    // Header is 32 bytes ($x0-$1F relative to header start)

    // Get map mode byte
    uint8_t map_mode = header_data[0x15];
    uint8_t rom_type = header_data[0x16];
    uint8_t rom_size_byte = header_data[0x17];
    uint8_t ram_size_byte = header_data[0x18];
    uint8_t region = header_data[0x19];
    uint16_t checksum_comp = header_data[0x1C] | (header_data[0x1D] << 8);
    uint16_t checksum = header_data[0x1E] | (header_data[0x1F] << 8);

    // Checksum complement check (checksum + complement should = $FFFF)
    if ((checksum ^ checksum_comp) == 0xFFFF) {
        score += 8;
    }

    // Map mode should have specific patterns
    // Bit 4 = FastROM (set = 3.58MHz, clear = 2.68MHz)
    // Low nibble: $0 = LoROM, $1 = HiROM, $2 = LoROM + S-DD1, $3 = LoROM + SA-1, $5 = ExHiROM
    uint8_t mode = map_mode & 0x0F;
    if (mode == 0x00 || mode == 0x01 || mode == 0x02 || mode == 0x03 || mode == 0x05) {
        score += 2;
    }

    // ROM size should be reasonable (8 = 256KB to 13 = 8MB)
    if (rom_size_byte >= 0x08 && rom_size_byte <= 0x0D) {
        score += 2;
        // Check if declared size matches actual size
        size_t declared_size = 1024ULL << rom_size_byte;
        if (declared_size <= rom_size && declared_size >= rom_size / 2) {
            score += 2;
        }
    }

    // RAM size should be reasonable (0 = none, 1 = 2KB to 7 = 128KB)
    if (ram_size_byte <= 0x07) {
        score += 1;
    }

    // Region code check
    if (region <= 0x0D) {  // Valid region codes
        score += 1;
    }

    // ROM type check - common values
    if (rom_type <= 0x03 ||  // ROM only, ROM+RAM, ROM+RAM+Battery
        rom_type == 0x13 ||  // ROM+SuperFX
        rom_type == 0x14 ||  // ROM+SuperFX+RAM
        rom_type == 0x15 ||  // ROM+SuperFX+RAM+Battery
        rom_type == 0x1A ||  // ROM+SuperFX2+RAM+Battery
        rom_type == 0x23 ||  // ROM+OBC1
        rom_type == 0x32 ||  // ROM+SA-1
        rom_type == 0x33 ||  // ROM+SA-1+RAM
        rom_type == 0x34 ||  // ROM+SA-1+RAM+Battery
        rom_type == 0x35 ||  // ROM+SA-1+RAM+Battery
        rom_type == 0x43 ||  // ROM+S-DD1
        rom_type == 0x45 ||  // ROM+S-DD1+RAM+Battery
        rom_type == 0xE3 ||  // ROM+GB (Super Game Boy)
        rom_type == 0xF5 ||  // ROM+Custom (ST010/ST011)
        rom_type == 0xF6 ||  // ROM+Custom (ST010/ST011)+Battery
        rom_type == 0xF9) {  // ROM+SPC7110+Battery
        score += 2;
    }

    // Title should be printable ASCII (or spaces)
    bool valid_title = true;
    for (int i = 0; i < 21; i++) {
        uint8_t c = header_data[i];
        if (c != 0x00 && c != 0x20 && (c < 0x20 || c > 0x7E)) {
            valid_title = false;
            break;
        }
    }
    if (valid_title) {
        score += 2;
    }

    return score;
}

bool Cartridge::parse_header(const uint8_t* header_data) {
    // Extract title (21 bytes, space-padded)
    m_title.clear();
    for (int i = 0; i < 21; i++) {
        char c = header_data[i];
        if (c >= 0x20 && c <= 0x7E) {
            m_title += c;
        } else if (c == 0) {
            break;
        }
    }
    // Trim trailing spaces
    while (!m_title.empty() && m_title.back() == ' ') {
        m_title.pop_back();
    }

    // Map mode
    uint8_t map_mode = header_data[0x15];
    uint8_t mode = map_mode & 0x0F;
    m_fast_rom = (map_mode & 0x10) != 0;

    // Determine mapper type from mode byte
    switch (mode) {
        case 0x00:
            m_mapper_type = MapperType::LoROM;
            break;
        case 0x01:
            m_mapper_type = MapperType::HiROM;
            break;
        case 0x02:
            m_mapper_type = MapperType::LoROM;  // S-DD1 uses LoROM base
            break;
        case 0x03:
            m_mapper_type = MapperType::SA1;    // SA-1 has special mapping
            break;
        case 0x05:
            m_mapper_type = MapperType::ExHiROM;
            break;
        default:
            // Keep the detected type
            break;
    }

    // ROM type - determines RAM and enhancement chip
    uint8_t rom_type = header_data[0x16];

    // Base ROM type (low nibble)
    m_has_ram = false;
    m_has_battery = false;

    switch (rom_type & 0x0F) {
        case 0x00:  // ROM only
            break;
        case 0x01:  // ROM + RAM
            m_has_ram = true;
            break;
        case 0x02:  // ROM + RAM + Battery
            m_has_ram = true;
            m_has_battery = true;
            break;
        case 0x03:  // ROM + Coprocessor
            break;
        case 0x04:  // ROM + Coprocessor + RAM
            m_has_ram = true;
            break;
        case 0x05:  // ROM + Coprocessor + RAM + Battery
            m_has_ram = true;
            m_has_battery = true;
            break;
        case 0x06:  // ROM + Coprocessor + Battery
            m_has_battery = true;
            break;
    }

    // Enhancement chip (high nibble of rom_type combined with map_mode)
    m_enhancement_chip = EnhancementChip::None;

    uint8_t chip_type = (rom_type >> 4) & 0x0F;
    switch (chip_type) {
        case 0x0:
            // No coprocessor
            break;
        case 0x1:
            // DSP
            m_enhancement_chip = EnhancementChip::DSP1;
            break;
        case 0x2:
            // SuperFX
            m_enhancement_chip = EnhancementChip::SuperFX;
            m_mapper_type = MapperType::SuperFX;
            break;
        case 0x3:
            // OBC1
            m_enhancement_chip = EnhancementChip::OBC1;
            break;
        case 0x4:
            // SA-1
            m_enhancement_chip = EnhancementChip::SA1;
            m_mapper_type = MapperType::SA1;
            break;
        case 0x5:
            // Custom (S-DD1, SPC7110, etc.)
            if ((map_mode & 0x0F) == 0x02) {
                m_enhancement_chip = EnhancementChip::SDD1;
                m_mapper_type = MapperType::SDD1;
            }
            break;
        case 0xE:
            // Super Game Boy
            break;
        case 0xF:
            // Custom chip - check specific values
            if (rom_type == 0xF5 || rom_type == 0xF6) {
                m_enhancement_chip = EnhancementChip::ST010;
            } else if (rom_type == 0xF9) {
                m_enhancement_chip = EnhancementChip::SPC7110;
            }
            break;
    }

    // ROM size
    uint8_t rom_size_byte = header_data[0x17];
    // Actual ROM size was already loaded, this is just for verification

    // RAM size
    uint8_t ram_size_byte = header_data[0x18];
    if (m_has_ram && ram_size_byte > 0) {
        // RAM size = 1KB << ram_size_byte (or 0 for no RAM)
        size_t ram_size = 1024ULL << ram_size_byte;
        // Clamp to reasonable values (max 128KB)
        if (ram_size > 128 * 1024) ram_size = 128 * 1024;
        m_sram.resize(ram_size, 0);
    } else if (m_has_ram) {
        // Default to 8KB SRAM
        m_sram.resize(8 * 1024, 0);
    }

    // Region
    m_region = header_data[0x19];

    return true;
}

uint32_t Cartridge::calculate_crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = s_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint8_t Cartridge::read(uint32_t address) {
    if (!m_loaded) return 0;

    switch (m_mapper_type) {
        case MapperType::LoROM:
        case MapperType::SuperFX:
            return read_lorom(address);
        case MapperType::HiROM:
        case MapperType::ExHiROM:
            return read_hirom(address);
        case MapperType::SA1:
        case MapperType::SDD1:
            // These would need special handling
            // For now, fall back to HiROM-style access
            return read_hirom(address);
        default:
            return read_lorom(address);
    }
}

void Cartridge::write(uint32_t address, uint8_t value) {
    if (!m_loaded) return;

    switch (m_mapper_type) {
        case MapperType::LoROM:
        case MapperType::SuperFX:
            write_lorom(address, value);
            break;
        case MapperType::HiROM:
        case MapperType::ExHiROM:
            write_hirom(address, value);
            break;
        case MapperType::SA1:
        case MapperType::SDD1:
            write_hirom(address, value);
            break;
        default:
            write_lorom(address, value);
            break;
    }
}

uint8_t Cartridge::read_lorom(uint32_t address) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    // Debug: trace reads from DMA source regions
    static int lorom_debug = 0;
    static int dma_source_debug = 0;
    // Trace DMA source $029000 (graphics for VRAM $A000)
    if (is_debug_mode() && bank == 0x02 && offset >= 0x9000 && offset < 0x9020 && dma_source_debug < 5) {
        size_t calc_addr = (bank * 0x8000) + (offset - 0x8000);
        uint8_t val = m_rom.empty() ? 0 : m_rom[calc_addr % m_rom.size()];
        fprintf(stderr, "[CART] LoROM read $%02X:%04X -> rom_addr=$%06lX val=$%02X (rom_size=$%lX)\n",
            bank, offset, (unsigned long)calc_addr, val, (unsigned long)m_rom.size());
        dma_source_debug++;
    }
    if (is_debug_mode() && bank == 0x01 && offset == 0x8000 && lorom_debug < 3) {
        size_t calc_addr = (bank * 0x8000) + (offset - 0x8000);
        uint8_t val = m_rom.empty() ? 0 : m_rom[calc_addr % m_rom.size()];
        fprintf(stderr, "[CART] LoROM read $%02X:%04X -> rom_addr=$%06lX val=$%02X (rom_size=$%lX)\n",
            bank, offset, (unsigned long)calc_addr, val, (unsigned long)m_rom.size());
        lorom_debug++;
    }

    // LoROM memory map:
    // Banks $00-$3F, $80-$BF:
    //   $0000-$7FFF: System area (not handled here)
    //   $8000-$FFFF: ROM (32KB per bank)
    // Banks $40-$6F, $C0-$EF: ROM (full 64KB banks in upper 32KB)
    // Banks $70-$7D, $F0-$FF: SRAM ($0000-$7FFF) + ROM ($8000-$FFFF)

    // Mirror upper banks to lower
    uint8_t effective_bank = bank;
    if (bank >= 0x80) effective_bank = bank - 0x80;

    // SRAM access
    if ((effective_bank >= 0x70 && effective_bank <= 0x7D) && offset < 0x8000) {
        if (!m_sram.empty()) {
            // SRAM is mirrored within its size
            size_t sram_addr = ((effective_bank - 0x70) * 0x8000 + offset) % m_sram.size();
            return m_sram[sram_addr];
        }
        return 0;
    }

    // ROM access
    size_t rom_addr;
    if (offset >= 0x8000) {
        // Standard LoROM access: each bank maps 32KB at $8000-$FFFF
        rom_addr = (effective_bank * 0x8000) + (offset - 0x8000);
    } else if (effective_bank >= 0x40) {
        // Banks $40-$7D: full 64KB access, but still LoROM style
        rom_addr = ((effective_bank - 0x40) * 0x10000) + offset + (0x40 * 0x8000);
    } else {
        // Lower offset in banks $00-$3F: not ROM
        return 0;
    }

    // Mirror ROM within its actual size
    if (!m_rom.empty()) {
        return m_rom[rom_addr % m_rom.size()];
    }

    return 0;
}

void Cartridge::write_lorom(uint32_t address, uint8_t value) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    // Mirror upper banks
    uint8_t effective_bank = bank;
    if (bank >= 0x80) effective_bank = bank - 0x80;

    // SRAM write
    if ((effective_bank >= 0x70 && effective_bank <= 0x7D) && offset < 0x8000) {
        if (!m_sram.empty()) {
            size_t sram_addr = ((effective_bank - 0x70) * 0x8000 + offset) % m_sram.size();
            m_sram[sram_addr] = value;
        }
    }
    // ROM writes are ignored
}

uint8_t Cartridge::read_hirom(uint32_t address) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    // HiROM memory map:
    // Banks $00-$3F, $80-$BF:
    //   $0000-$7FFF: System area (not handled here)
    //   $8000-$FFFF: ROM
    // Banks $40-$7D, $C0-$FF: ROM (full 64KB banks)
    // SRAM: $20-$3F:$6000-$7FFF and mirrors

    uint8_t effective_bank = bank;
    if (bank >= 0x80) effective_bank = bank - 0x80;

    // SRAM access at $20-$3F:$6000-$7FFF (and $A0-$BF)
    if (effective_bank >= 0x20 && effective_bank <= 0x3F &&
        offset >= 0x6000 && offset < 0x8000) {
        if (!m_sram.empty()) {
            size_t sram_addr = ((effective_bank - 0x20) * 0x2000 + (offset - 0x6000)) % m_sram.size();
            return m_sram[sram_addr];
        }
        return 0;
    }

    // ROM access
    size_t rom_addr;
    if (effective_bank >= 0x40 && effective_bank <= 0x7D) {
        // Banks $40-$7D: full 64KB
        rom_addr = ((effective_bank - 0x40) * 0x10000) + offset;
    } else if (effective_bank <= 0x3F) {
        // Banks $00-$3F: only $8000-$FFFF
        if (offset < 0x8000) return 0;
        rom_addr = (effective_bank * 0x10000) + offset;
    } else {
        // Banks $7E-$7F are WRAM (not handled here)
        return 0;
    }

    // Handle ExHiROM (banks $C0-$FF map to upper 4MB)
    if (m_mapper_type == MapperType::ExHiROM && bank >= 0xC0) {
        rom_addr = ((bank - 0xC0) * 0x10000) + offset + 0x400000;
    }

    if (!m_rom.empty()) {
        rom_addr %= m_rom.size();
        return m_rom[rom_addr];
    }

    return 0;
}

void Cartridge::write_hirom(uint32_t address, uint8_t value) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    uint8_t effective_bank = bank;
    if (bank >= 0x80) effective_bank = bank - 0x80;

    // SRAM write at $20-$3F:$6000-$7FFF
    if (effective_bank >= 0x20 && effective_bank <= 0x3F &&
        offset >= 0x6000 && offset < 0x8000) {
        if (!m_sram.empty()) {
            size_t sram_addr = ((effective_bank - 0x20) * 0x2000 + (offset - 0x6000)) % m_sram.size();
            m_sram[sram_addr] = value;
        }
    }
    // ROM writes are ignored
}

std::vector<uint8_t> Cartridge::get_save_data() const {
    if (!m_has_battery || !m_loaded) {
        return {};
    }
    return m_sram;
}

bool Cartridge::set_save_data(const std::vector<uint8_t>& data) {
    if (!m_has_battery || !m_loaded || data.empty()) {
        return false;
    }

    size_t copy_size = std::min(data.size(), m_sram.size());
    std::memcpy(m_sram.data(), data.data(), copy_size);

    std::cout << "Loaded save file (" << copy_size << " bytes)" << std::endl;
    return true;
}

void Cartridge::save_state(std::vector<uint8_t>& data) {
    // Save SRAM
    uint32_t sram_size = static_cast<uint32_t>(m_sram.size());
    data.push_back(sram_size & 0xFF);
    data.push_back((sram_size >> 8) & 0xFF);
    data.push_back((sram_size >> 16) & 0xFF);
    data.push_back((sram_size >> 24) & 0xFF);

    if (!m_sram.empty()) {
        data.insert(data.end(), m_sram.begin(), m_sram.end());
    }
}

void Cartridge::load_state(const uint8_t*& data, size_t& remaining) {
    // Load SRAM
    if (remaining < 4) return;

    uint32_t sram_size = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    data += 4;
    remaining -= 4;

    if (sram_size > 0 && remaining >= sram_size) {
        if (m_sram.size() == sram_size) {
            std::memcpy(m_sram.data(), data, sram_size);
        }
        data += sram_size;
        remaining -= sram_size;
    }
}

} // namespace snes
