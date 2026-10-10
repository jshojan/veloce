#pragma once
#include "../../ppc/src/cpu.hpp"
#include "emu/emulator_plugin.hpp"
#include <memory>
#include <vector>
namespace wiiu {
class Machine {
    struct Bus final:ppc::Memory {
        Machine& owner; unsigned id;
        Bus(Machine& m,unsigned n):owner(m),id(n) {}
        bool read(uint32_t a,unsigned w,uint32_t& v) const override { return owner.read(id,a,w,v); }
        bool write(uint32_t a,unsigned w,uint32_t v) override { return owner.write(id,a,w,v); }
    } bus0,bus1,bus2;
    using Page=std::array<uint8_t,4096>;
    static constexpr size_t page_count=(0x02000000ull+0x80000000ull)/4096;
    std::vector<std::unique_ptr<Page>> pages;
    static bool location(uint32_t,unsigned,size_t&);
    bool store(uint32_t,unsigned,uint32_t);
public:
    Machine():bus0(*this,0),bus1(*this,1),bus2(*this,2),pages(page_count) {}
    bool load(const uint8_t*,size_t);
    void reset();
    void step(unsigned);
    void frame();
    void set_input(uint32_t v) { buttons=v&0xfff; }
    bool read(unsigned,uint32_t,unsigned,uint32_t&) const;
    bool write(unsigned,uint32_t,unsigned,uint32_t);
    bool save(std::vector<uint8_t>&) const;
    bool restore(const std::vector<uint8_t>&);
    std::array<ppc::State,3> cpus{};
    std::vector<uint8_t> rom;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0,buttons=0;
    uint64_t identity=0,frames=0;
    uint8_t done=0;
};
}
