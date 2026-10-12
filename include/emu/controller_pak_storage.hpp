#pragma once
#include <cstdint>
#include <vector>

namespace emu {
// Optional raw Controller Pak storage contract. The frontend owns .mpk file
// I/O; a core owns insertion, Joybus behavior and state serialization.
class IControllerPakStorage {
public:
    virtual ~IControllerPakStorage() = default;
    virtual unsigned controller_pak_ports() const = 0;
    virtual bool controller_pak_inserted(unsigned port) const = 0;
    virtual std::vector<uint8_t> get_controller_pak_data(unsigned port) const = 0;
    virtual bool set_controller_pak_data(unsigned port,const std::vector<uint8_t>& data) = 0;
};
}
