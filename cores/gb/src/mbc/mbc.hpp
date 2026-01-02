#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace gb {

// Memory Bank Controller base class
class MBC {
public:
    MBC(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram, int rom_banks, int ram_banks);
    virtual ~MBC() = default;

    // Factory method
    static std::unique_ptr<MBC> create(int type, std::vector<uint8_t>& rom, std::vector<uint8_t>& ram, int rom_banks, int ram_banks);

    // Reset MBC state
    virtual void reset();

    // ROM access
    virtual uint8_t read_rom(uint16_t address);

    // RAM access
    virtual uint8_t read_ram(uint16_t address);
    virtual void write_ram(uint16_t address, uint8_t value);

    // MBC register writes
    virtual void write(uint16_t address, uint8_t value) = 0;

    // Save state
    virtual void save_state(std::vector<uint8_t>& data);
    virtual void load_state(const uint8_t*& data, size_t& remaining);

protected:
    std::vector<uint8_t>& m_rom;
    std::vector<uint8_t>& m_ram;
    int m_rom_banks;
    int m_ram_banks;

    int m_rom_bank = 1;
    int m_ram_bank = 0;
    bool m_ram_enabled = false;
};

// No MBC (ROM only)
class MBC0 : public MBC {
public:
    using MBC::MBC;
    void write(uint16_t address, uint8_t value) override;
};

} // namespace gb
