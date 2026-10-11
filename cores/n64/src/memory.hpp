#pragma once
#include <cstdint>
namespace n64 {
struct Memory {
    virtual ~Memory() = default;
    virtual void clock() {}
    // Live external IP2..IP6 levels, expressed in Cause IP bit positions.
    virtual uint8_t interrupt_pending() const { return 0; }
    virtual bool read(uint64_t address, unsigned bytes, uint64_t& value) = 0;
    virtual bool write(uint64_t address, unsigned bytes, uint64_t value) = 0;
    // Cached fills/writebacks are separate bus transactions from uncached CPU
    // accesses. MI EBUS mode cannot supply an ordinary cache line.
    virtual bool read_cacheline(uint64_t address,unsigned bytes,uint64_t& value) {return read(address,bytes,value);}
    virtual bool write_cacheline(uint64_t address,unsigned bytes,uint64_t value) {return write(address,bytes,value);}
    // Aligned word/doubleword transaction; bit i enables ascending memory byte i.
    // Reject before modifying storage. No read-modify-write bus read is required.
    virtual bool write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) = 0;
};
}
