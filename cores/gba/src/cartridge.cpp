#include "cartridge.hpp"
#include "debug.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace gba {

Cartridge::Cartridge() = default;
Cartridge::~Cartridge() = default;

bool Cartridge::detect_rtc(const uint8_t* data, size_t size) {
    // Check for RTC identifier string in ROM
    std::string rom_str(reinterpret_cast<const char*>(data), size);
    if (rom_str.find("RTC_V") != std::string::npos) {
        return true;
    }

    // Also check for known Pokemon games that use RTC
    // Game codes are at offset 0xAC-0xAF
    if (size >= 0xB0) {
        char game_code[5] = {0};
        std::memcpy(game_code, data + 0xAC, 4);

        // Pokemon Ruby/Sapphire/Emerald/FireRed/LeafGreen use RTC
        if (strncmp(game_code, "AXVE", 4) == 0 ||  // Ruby (US)
            strncmp(game_code, "AXPE", 4) == 0 ||  // Sapphire (US)
            strncmp(game_code, "BPEE", 4) == 0 ||  // Emerald (US)
            strncmp(game_code, "BPRE", 4) == 0 ||  // Fire Red (US)
            strncmp(game_code, "BPGE", 4) == 0 ||  // Leaf Green (US)
            strncmp(game_code, "AXVJ", 4) == 0 ||  // Ruby (JP)
            strncmp(game_code, "AXPJ", 4) == 0 ||  // Sapphire (JP)
            strncmp(game_code, "BPEJ", 4) == 0 ||  // Emerald (JP)
            strncmp(game_code, "BPRJ", 4) == 0 ||  // Fire Red (JP)
            strncmp(game_code, "BPGJ", 4) == 0) {  // Leaf Green (JP)
            return true;
        }
    }

    return false;
}

SaveType Cartridge::detect_save_type(const uint8_t* data, size_t size) {
    // Search for save type strings in ROM
    // These strings are placed by the SDK to indicate save type
    std::string rom_str(reinterpret_cast<const char*>(data), size);

    // Check for Flash 1M (128KB) first - most specific
    if (rom_str.find("FLASH1M_V") != std::string::npos ||
        rom_str.find("FLASH1M_") != std::string::npos) {
        if (is_debug_mode()) {
            std::cout << "[GBA] Detected save type: Flash 128KB" << std::endl;
        }
        return SaveType::Flash_128K;
    }

    // Check for Flash 512K (64KB)
    if (rom_str.find("FLASH_V") != std::string::npos ||
        rom_str.find("FLASH512_V") != std::string::npos) {
        if (is_debug_mode()) {
            std::cout << "[GBA] Detected save type: Flash 64KB" << std::endl;
        }
        return SaveType::Flash_64K;
    }

    // Check for EEPROM
    if (rom_str.find("EEPROM_V") != std::string::npos) {
        // Determine EEPROM size based on ROM size
        // Large ROMs (>16MB) typically use 8KB EEPROM
        if (is_debug_mode()) {
            std::cout << "[GBA] Detected save type: EEPROM" << std::endl;
        }
        return (size > 16 * 1024 * 1024) ? SaveType::EEPROM_8K : SaveType::EEPROM_512;
    }

    // Check for SRAM
    if (rom_str.find("SRAM_V") != std::string::npos ||
        rom_str.find("SRAM_F_V") != std::string::npos) {
        if (is_debug_mode()) {
            std::cout << "[GBA] Detected save type: SRAM 32KB" << std::endl;
        }
        return SaveType::SRAM_32K;
    }

    // No save type detected - default to SRAM for compatibility
    if (is_debug_mode()) {
        std::cout << "[GBA] No save type detected, defaulting to SRAM 32KB" << std::endl;
    }
    return SaveType::SRAM_32K;
}

bool Cartridge::load(const uint8_t* data, size_t size, SystemType system_type) {
    (void)system_type;  // Always GBA for this plugin

    if (size < 0xC0) {
        std::cerr << "GBA ROM too small" << std::endl;
        return false;
    }

    // Copy ROM
    m_rom.assign(data, data + size);

    // Extract title (at 0xA0, 12 bytes)
    m_title.clear();
    for (int i = 0; i < 12; i++) {
        char c = data[0xA0 + i];
        if (c == 0) break;
        m_title += c;
    }

    // Detect save type from ROM strings
    m_save_type = detect_save_type(data, size);

    // Allocate save memory based on detected type
    size_t save_size = 0;
    switch (m_save_type) {
        case SaveType::None:
            save_size = 0;
            break;
        case SaveType::SRAM_32K:
            save_size = 32 * 1024;
            break;
        case SaveType::EEPROM_512:
            save_size = 512;
            break;
        case SaveType::EEPROM_8K:
            save_size = 8 * 1024;
            break;
        case SaveType::Flash_64K:
            save_size = 64 * 1024;
            break;
        case SaveType::Flash_128K:
            save_size = 128 * 1024;
            break;
    }

    m_save_data.resize(save_size, 0xFF);

    // Detect RTC
    m_has_rtc = detect_rtc(data, size);
    if (m_has_rtc && is_debug_mode()) {
        std::cout << "[GBA] Detected RTC support" << std::endl;
    }

    // Reset Flash and RTC state
    reset_flash_state();
    m_gpio_data = 0;
    m_gpio_direction = 0;
    m_gpio_control = 0;
    m_rtc_state = RTCState::Idle;
    m_rtc_bit_count = 0;
    m_rtc_byte_count = 0;
    m_rtc_last_sck = false;

    m_crc32 = calculate_crc32(data, size);
    m_loaded = true;

    std::cout << "GBA ROM loaded: " << m_title << std::endl;
    return true;
}

void Cartridge::unload() {
    m_rom.clear();
    m_save_data.clear();
    m_loaded = false;
    m_crc32 = 0;
    m_title.clear();
    m_save_type = SaveType::None;
    reset_flash_state();
}

void Cartridge::reset() {
    reset_flash_state();
}

void Cartridge::reset_flash_state() {
    m_flash_state = FlashState::Ready;
    m_flash_bank = 0;
    m_flash_id_mode = false;
}

uint8_t Cartridge::read_rom(uint32_t address) {
    // Handle GPIO reads for RTC games
    if (m_has_rtc && address >= 0xC4 && address <= 0xC9) {
        switch (address) {
            case 0xC4:  // GPIO data low byte
                if (m_gpio_control & 1) {
                    // GPIO enabled - return data with RTC output
                    uint8_t value = m_gpio_data;
                    // If SIO is set as input (from RTC), include RTC output
                    if (!(m_gpio_direction & GPIO_SIO)) {
                        value = (value & ~GPIO_SIO) | rtc_get_output();
                    }
                    static int read_count = 0;
                    if (is_debug_mode() && ++read_count <= 20) {
                        fprintf(stderr, "[GBA] GPIO read 0xC4: dir=%02X ctrl=%02X returning %02X\n",
                               m_gpio_direction, m_gpio_control, value);
                    }
                    return value;
                }
                break;
            case 0xC5:  // GPIO data high byte (always 0)
                return 0;
            case 0xC6:  // GPIO direction low byte
                return m_gpio_direction;
            case 0xC7:  // GPIO direction high byte
                return 0;
            case 0xC8:  // GPIO control low byte
                return m_gpio_control;
            case 0xC9:  // GPIO control high byte
                return 0;
        }
    }

    if (address < m_rom.size()) {
        return m_rom[address];
    }
    // Return open bus value for reads past ROM end
    // (address / 2) & 0xFF for each byte
    return (address >> 1) & 0xFF;
}

void Cartridge::write_rom(uint32_t address, uint8_t value) {
    // Handle GPIO writes for RTC games
    if (m_has_rtc && address >= 0xC4 && address <= 0xC9) {
        static int write_count = 0;
        if (is_debug_mode() && ++write_count <= 50) {
            fprintf(stderr, "[GBA] GPIO write [%02X] = %02X\n", address & 0xFF, value);
        }

        switch (address) {
            case 0xC4: {  // GPIO data
                uint8_t old_data = m_gpio_data;
                m_gpio_data = value & 0x0F;  // Only 4 bits used

                // Clock RTC on SCK rising edge
                bool old_sck = (old_data & GPIO_SCK) != 0;
                bool new_sck = (value & GPIO_SCK) != 0;

                if (!old_sck && new_sck) {
                    rtc_clock_edge();
                }
                break;
            }
            case 0xC6:  // GPIO direction
                m_gpio_direction = value & 0x0F;
                break;
            case 0xC8:  // GPIO control
                m_gpio_control = value & 0x01;
                break;
        }
    }
}

uint8_t Cartridge::read_flash(uint32_t address) {
    // Handle chip ID mode
    if (m_flash_id_mode) {
        if (address == 0x0000) {
            return (m_save_type == SaveType::Flash_128K)
                   ? FLASH_128K_MANUFACTURER
                   : FLASH_64K_MANUFACTURER;
        }
        if (address == 0x0001) {
            return (m_save_type == SaveType::Flash_128K)
                   ? FLASH_128K_DEVICE
                   : FLASH_64K_DEVICE;
        }
    }

    // Normal Flash read - handle banking for 128KB
    uint32_t actual_address = address & 0xFFFF;
    if (m_save_type == SaveType::Flash_128K) {
        actual_address += m_flash_bank * 0x10000;
    }

    if (actual_address < m_save_data.size()) {
        return m_save_data[actual_address];
    }
    return 0xFF;
}

void Cartridge::write_flash(uint32_t address, uint8_t value) {
    uint16_t addr = address & 0xFFFF;

    switch (m_flash_state) {
        case FlashState::Ready:
            // Look for first command byte: 0xAA at 0x5555
            if (addr == 0x5555 && value == 0xAA) {
                m_flash_state = FlashState::Command1;
            }
            break;

        case FlashState::Command1:
            // Look for second command byte: 0x55 at 0x2AAA
            if (addr == 0x2AAA && value == 0x55) {
                m_flash_state = FlashState::Command2;
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::Command2:
            // Process command at 0x5555
            if (addr == 0x5555) {
                switch (value) {
                    case 0x90:  // Enter chip ID mode
                        m_flash_id_mode = true;
                        m_flash_state = FlashState::Ready;
                        break;

                    case 0xF0:  // Exit chip ID mode / Reset
                        m_flash_id_mode = false;
                        m_flash_state = FlashState::Ready;
                        break;

                    case 0x80:  // Erase command prefix
                        m_flash_state = FlashState::Erase;
                        break;

                    case 0xA0:  // Prepare for byte write
                        m_flash_state = FlashState::Write;
                        break;

                    case 0xB0:  // Bank switch (128KB Flash only)
                        if (m_save_type == SaveType::Flash_128K) {
                            m_flash_state = FlashState::BankSwitch;
                        } else {
                            m_flash_state = FlashState::Ready;
                        }
                        break;

                    default:
                        m_flash_state = FlashState::Ready;
                        break;
                }
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::Erase:
            // Erase command sequence: we're waiting for AA-55-10/30 after the 80 command
            if (addr == 0x5555 && value == 0xAA) {
                m_flash_state = FlashState::EraseCommand1;
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::EraseCommand1:
            if (addr == 0x2AAA && value == 0x55) {
                m_flash_state = FlashState::EraseCommand2;
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::EraseCommand2:
            if (addr == 0x5555 && value == 0x10) {
                // Erase entire chip
                std::fill(m_save_data.begin(), m_save_data.end(), 0xFF);
                m_flash_state = FlashState::Ready;
            } else if (value == 0x30) {
                // Sector erase (4KB sectors)
                uint32_t sector_base = addr & 0xF000;
                if (m_save_type == SaveType::Flash_128K) {
                    sector_base += m_flash_bank * 0x10000;
                }
                for (uint32_t i = 0; i < 0x1000 && (sector_base + i) < m_save_data.size(); i++) {
                    m_save_data[sector_base + i] = 0xFF;
                }
                m_flash_state = FlashState::Ready;
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::Write:
            // Write single byte (can only change 1s to 0s)
            {
                uint32_t actual_address = addr;
                if (m_save_type == SaveType::Flash_128K) {
                    actual_address += m_flash_bank * 0x10000;
                }
                if (actual_address < m_save_data.size()) {
                    m_save_data[actual_address] &= value;
                }
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::BankSwitch:
            // Set bank number (only for 128KB Flash)
            if (addr == 0x0000) {
                m_flash_bank = value & 1;  // Only 2 banks
            }
            m_flash_state = FlashState::Ready;
            break;

        default:
            m_flash_state = FlashState::Ready;
            break;
    }
}

uint8_t Cartridge::read_sram(uint32_t address) {
    switch (m_save_type) {
        case SaveType::Flash_64K:
        case SaveType::Flash_128K:
            return read_flash(address);

        case SaveType::SRAM_32K:
            // SRAM is mirrored in the 64KB region
            if (!m_save_data.empty()) {
                return m_save_data[address % m_save_data.size()];
            }
            return 0xFF;

        case SaveType::EEPROM_512:
        case SaveType::EEPROM_8K:
            // EEPROM is accessed via special protocol - not simple memory mapped
            // For now, return 0xFF
            return 0xFF;

        default:
            return 0xFF;
    }
}

void Cartridge::write_sram(uint32_t address, uint8_t value) {
    switch (m_save_type) {
        case SaveType::Flash_64K:
        case SaveType::Flash_128K:
            write_flash(address, value);
            break;

        case SaveType::SRAM_32K:
            // SRAM is mirrored in the 64KB region
            if (!m_save_data.empty()) {
                m_save_data[address % m_save_data.size()] = value;
            }
            break;

        case SaveType::EEPROM_512:
        case SaveType::EEPROM_8K:
            // EEPROM uses special protocol - ignored for now
            break;

        default:
            break;
    }
}

bool Cartridge::has_battery() const {
    return m_loaded && m_save_type != SaveType::None;
}

std::vector<uint8_t> Cartridge::get_save_data() const {
    if (!m_loaded) return {};
    return m_save_data;
}

bool Cartridge::set_save_data(const std::vector<uint8_t>& data) {
    if (!m_loaded) return false;
    size_t copy_size = std::min(data.size(), m_save_data.size());
    std::memcpy(m_save_data.data(), data.data(), copy_size);
    return true;
}

uint32_t Cartridge::calculate_crc32(const uint8_t* data, size_t size) {
    static const uint32_t crc_table[256] = {
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

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

void Cartridge::save_state(std::vector<uint8_t>& data) {
    // Save save_data
    data.insert(data.end(), m_save_data.begin(), m_save_data.end());

    // Save Flash state
    data.push_back(static_cast<uint8_t>(m_flash_state));
    data.push_back(m_flash_bank);
    data.push_back(m_flash_id_mode ? 1 : 0);
}

void Cartridge::load_state(const uint8_t*& data, size_t& remaining) {
    // Load save_data
    std::memcpy(m_save_data.data(), data, m_save_data.size());
    data += m_save_data.size();
    remaining -= m_save_data.size();

    // Load Flash state
    m_flash_state = static_cast<FlashState>(data[0]);
    m_flash_bank = data[1];
    m_flash_id_mode = data[2] != 0;
    data += 3;
    remaining -= 3;
}

// RTC helper: convert BCD to binary
static uint8_t bcd(uint8_t value) {
    return ((value / 10) << 4) | (value % 10);
}

// Get current bit to output from RTC
uint8_t Cartridge::rtc_get_output() {
    if (m_rtc_state == RTCState::SendData && m_rtc_byte_count < 7) {
        // Return current bit of serial data
        return (m_rtc_serial_data & (1 << m_rtc_bit_count)) ? GPIO_SIO : 0;
    }
    return 0;
}

// Process RTC clock edge
void Cartridge::rtc_clock_edge() {
    // Check CS (chip select) - must be high to communicate
    if (!(m_gpio_data & GPIO_CS)) {
        // CS low - reset state
        m_rtc_state = RTCState::Idle;
        m_rtc_bit_count = 0;
        m_rtc_byte_count = 0;
        return;
    }

    bool sio_in = (m_gpio_data & GPIO_SIO) != 0;

    switch (m_rtc_state) {
        case RTCState::Idle:
            // Start receiving command
            m_rtc_state = RTCState::ReceiveCommand;
            m_rtc_command = 0;
            m_rtc_bit_count = 0;
            // Fall through to receive first bit
            [[fallthrough]];

        case RTCState::ReceiveCommand:
            // Receive command byte (8 bits, LSB first)
            if (sio_in) {
                m_rtc_command |= (1 << m_rtc_bit_count);
            }
            m_rtc_bit_count++;

            if (m_rtc_bit_count >= 8) {
                // Command complete, check if valid
                // Command format: 0110 CCCC for read, 0010 CCCC for write
                // CCCC is command ID reversed
                if ((m_rtc_command & 0xF0) == 0x60) {
                    // Read command
                    m_rtc_state = RTCState::SendData;
                    m_rtc_bit_count = 0;
                    m_rtc_byte_count = 0;
                    rtc_process_command();
                } else if ((m_rtc_command & 0xF0) == 0x20) {
                    // Write command
                    m_rtc_state = RTCState::ReceiveData;
                    m_rtc_bit_count = 0;
                    m_rtc_byte_count = 0;
                } else {
                    // Invalid command
                    m_rtc_state = RTCState::Idle;
                }
            }
            break;

        case RTCState::ReceiveData:
            // Receive data to write
            if (sio_in) {
                m_rtc_data[m_rtc_byte_count] |= (1 << m_rtc_bit_count);
            }
            m_rtc_bit_count++;

            if (m_rtc_bit_count >= 8) {
                m_rtc_bit_count = 0;
                m_rtc_byte_count++;
                if (m_rtc_byte_count < 8) {
                    m_rtc_data[m_rtc_byte_count] = 0;
                }
            }
            break;

        case RTCState::SendData:
            // Advance to next bit
            m_rtc_bit_count++;
            if (m_rtc_bit_count >= 8) {
                m_rtc_bit_count = 0;
                m_rtc_byte_count++;
                if (m_rtc_byte_count < 7) {
                    m_rtc_serial_data = m_rtc_data[m_rtc_byte_count];
                } else {
                    m_rtc_state = RTCState::Idle;
                }
            }
            break;
    }
}

// Prepare data for read commands
void Cartridge::rtc_process_command() {
    // Clear data buffer
    std::memset(m_rtc_data, 0, sizeof(m_rtc_data));

    // Common commands:
    // 0x65 (01100101): Read date/time (7 bytes)
    // 0x67 (01100111): Read time (3 bytes)
    // 0x63 (01100011): Read status register 1
    // 0x61 (01100001): Reset
    // 0x69 (01101001): Read status register 2

    switch (m_rtc_command) {
        case 0x65: {
            // Read date/time - return current system time in BCD format
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);

            m_rtc_data[0] = bcd(t->tm_year % 100);  // Year (00-99)
            m_rtc_data[1] = bcd(t->tm_mon + 1);      // Month (1-12)
            m_rtc_data[2] = bcd(t->tm_mday);          // Day (1-31)
            m_rtc_data[3] = bcd(t->tm_wday);          // Day of week (0-6)
            m_rtc_data[4] = bcd(t->tm_hour);          // Hour (0-23)
            m_rtc_data[5] = bcd(t->tm_min);           // Minute (0-59)
            m_rtc_data[6] = bcd(t->tm_sec);           // Second (0-59)
            break;
        }

        case 0x67: {
            // Read time only (3 bytes)
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);

            m_rtc_data[0] = bcd(t->tm_hour);
            m_rtc_data[1] = bcd(t->tm_min);
            m_rtc_data[2] = bcd(t->tm_sec);
            break;
        }

        case 0x63:
            // Status register 1 - return 0 (no errors, power OK)
            m_rtc_data[0] = 0x00;
            break;

        case 0x69:
            // Status register 2 - return 0x40 (24-hour mode)
            m_rtc_data[0] = 0x40;
            break;

        case 0x61:
            // Reset - acknowledged, no data to send
            break;

        default:
            // Unknown command - return zeros
            break;
    }

    m_rtc_serial_data = m_rtc_data[0];
}

} // namespace gba
