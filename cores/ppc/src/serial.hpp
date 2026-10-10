#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace ppc {
// Settled communication-RAM transactions. Polling and wire timing are separate,
// currently unsupported hardware paths; they must not return invented data.
class Serial {
public:
    static constexpr size_t state_size=156;
    void reset();
    void input(unsigned port,uint32_t buttons,bool connected);
    bool read(uint32_t offset,uint32_t& value) const;
    bool write(uint32_t offset,uint32_t value);
    std::array<uint8_t,state_size> save() const;
    bool restore(const uint8_t* data,size_t size);
private:
    void transfer();
    std::array<uint8_t,128> buffer{};
    std::array<uint32_t,4> buttons{};
    uint32_t connected=1,control=0,status=0;
};
}
