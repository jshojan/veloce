#include "trace.hpp"

#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"

#include <cstdlib>
#include <cstring>

namespace nes {

namespace {

// Addressing modes, matching the operand-formatting rules nestest uses.
enum class Mode : uint8_t {
    IMP,    // implied (no operand)
    ACC,    // accumulator ("A")
    IMM,    // #$NN
    ZP,     // $NN = VV
    ZPX,    // $NN,X @ EA = VV
    ZPY,    // $NN,Y @ EA = VV
    ABS,    // $NNNN = VV
    ABS_J,  // $NNNN  (JMP/JSR: no "= VV")
    ABX,    // $NNNN,X @ EAEA = VV
    ABY,    // $NNNN,Y @ EAEA = VV
    IND,    // ($NNNN) = EAEA  (JMP indirect)
    IZX,    // ($NN,X) @ ZZ = EAEA = VV
    IZY,    // ($NN),Y = EAEA @ FAFA = VV
    REL,    // $NNNN (branch target)
};

struct OpInfo {
    const char* name;  // 3-letter mnemonic
    Mode mode;
    uint8_t len;       // total instruction length in bytes (1..3)
    bool illegal;      // prefixed with '*' in nestest.log
};

// Length implied by addressing mode.
constexpr uint8_t mode_len(Mode m) {
    switch (m) {
        case Mode::IMP:
        case Mode::ACC:   return 1;
        case Mode::IMM:
        case Mode::ZP:
        case Mode::ZPX:
        case Mode::ZPY:
        case Mode::IZX:
        case Mode::IZY:
        case Mode::REL:   return 2;
        case Mode::ABS:
        case Mode::ABS_J:
        case Mode::ABX:
        case Mode::ABY:
        case Mode::IND:   return 3;
    }
    return 1;
}

#define OP(n, m, ill) { n, Mode::m, mode_len(Mode::m), ill }

// Full 256-entry opcode table. Illegal/undocumented opcodes carry the names and
// the '*' marker that Nintendulator's nestest.log uses.
const OpInfo kOps[256] = {
    /* 00 */ OP("BRK", IMP, false), OP("ORA", IZX, false), OP("KIL", IMP, true),  OP("SLO", IZX, true),
    /* 04 */ OP("NOP", ZP,  true),  OP("ORA", ZP,  false), OP("ASL", ZP,  false), OP("SLO", ZP,  true),
    /* 08 */ OP("PHP", IMP, false), OP("ORA", IMM, false), OP("ASL", ACC, false), OP("ANC", IMM, true),
    /* 0C */ OP("NOP", ABS, true),  OP("ORA", ABS, false), OP("ASL", ABS, false), OP("SLO", ABS, true),
    /* 10 */ OP("BPL", REL, false), OP("ORA", IZY, false), OP("KIL", IMP, true),  OP("SLO", IZY, true),
    /* 14 */ OP("NOP", ZPX, true),  OP("ORA", ZPX, false), OP("ASL", ZPX, false), OP("SLO", ZPX, true),
    /* 18 */ OP("CLC", IMP, false), OP("ORA", ABY, false), OP("NOP", IMP, true),  OP("SLO", ABY, true),
    /* 1C */ OP("NOP", ABX, true),  OP("ORA", ABX, false), OP("ASL", ABX, false), OP("SLO", ABX, true),
    /* 20 */ OP("JSR", ABS_J, false), OP("AND", IZX, false), OP("KIL", IMP, true), OP("RLA", IZX, true),
    /* 24 */ OP("BIT", ZP,  false), OP("AND", ZP,  false), OP("ROL", ZP,  false), OP("RLA", ZP,  true),
    /* 28 */ OP("PLP", IMP, false), OP("AND", IMM, false), OP("ROL", ACC, false), OP("ANC", IMM, true),
    /* 2C */ OP("BIT", ABS, false), OP("AND", ABS, false), OP("ROL", ABS, false), OP("RLA", ABS, true),
    /* 30 */ OP("BMI", REL, false), OP("AND", IZY, false), OP("KIL", IMP, true),  OP("RLA", IZY, true),
    /* 34 */ OP("NOP", ZPX, true),  OP("AND", ZPX, false), OP("ROL", ZPX, false), OP("RLA", ZPX, true),
    /* 38 */ OP("SEC", IMP, false), OP("AND", ABY, false), OP("NOP", IMP, true),  OP("RLA", ABY, true),
    /* 3C */ OP("NOP", ABX, true),  OP("AND", ABX, false), OP("ROL", ABX, false), OP("RLA", ABX, true),
    /* 40 */ OP("RTI", IMP, false), OP("EOR", IZX, false), OP("KIL", IMP, true),  OP("SRE", IZX, true),
    /* 44 */ OP("NOP", ZP,  true),  OP("EOR", ZP,  false), OP("LSR", ZP,  false), OP("SRE", ZP,  true),
    /* 48 */ OP("PHA", IMP, false), OP("EOR", IMM, false), OP("LSR", ACC, false), OP("ALR", IMM, true),
    /* 4C */ OP("JMP", ABS_J, false), OP("EOR", ABS, false), OP("LSR", ABS, false), OP("SRE", ABS, true),
    /* 50 */ OP("BVC", REL, false), OP("EOR", IZY, false), OP("KIL", IMP, true),  OP("SRE", IZY, true),
    /* 54 */ OP("NOP", ZPX, true),  OP("EOR", ZPX, false), OP("LSR", ZPX, false), OP("SRE", ZPX, true),
    /* 58 */ OP("CLI", IMP, false), OP("EOR", ABY, false), OP("NOP", IMP, true),  OP("SRE", ABY, true),
    /* 5C */ OP("NOP", ABX, true),  OP("EOR", ABX, false), OP("LSR", ABX, false), OP("SRE", ABX, true),
    /* 60 */ OP("RTS", IMP, false), OP("ADC", IZX, false), OP("KIL", IMP, true),  OP("RRA", IZX, true),
    /* 64 */ OP("NOP", ZP,  true),  OP("ADC", ZP,  false), OP("ROR", ZP,  false), OP("RRA", ZP,  true),
    /* 68 */ OP("PLA", IMP, false), OP("ADC", IMM, false), OP("ROR", ACC, false), OP("ARR", IMM, true),
    /* 6C */ OP("JMP", IND, false), OP("ADC", ABS, false), OP("ROR", ABS, false), OP("RRA", ABS, true),
    /* 70 */ OP("BVS", REL, false), OP("ADC", IZY, false), OP("KIL", IMP, true),  OP("RRA", IZY, true),
    /* 74 */ OP("NOP", ZPX, true),  OP("ADC", ZPX, false), OP("ROR", ZPX, false), OP("RRA", ZPX, true),
    /* 78 */ OP("SEI", IMP, false), OP("ADC", ABY, false), OP("NOP", IMP, true),  OP("RRA", ABY, true),
    /* 7C */ OP("NOP", ABX, true),  OP("ADC", ABX, false), OP("ROR", ABX, false), OP("RRA", ABX, true),
    /* 80 */ OP("NOP", IMM, true),  OP("STA", IZX, false), OP("NOP", IMM, true),  OP("SAX", IZX, true),
    /* 84 */ OP("STY", ZP,  false), OP("STA", ZP,  false), OP("STX", ZP,  false), OP("SAX", ZP,  true),
    /* 88 */ OP("DEY", IMP, false), OP("NOP", IMM, true),  OP("TXA", IMP, false), OP("XAA", IMM, true),
    /* 8C */ OP("STY", ABS, false), OP("STA", ABS, false), OP("STX", ABS, false), OP("SAX", ABS, true),
    /* 90 */ OP("BCC", REL, false), OP("STA", IZY, false), OP("KIL", IMP, true),  OP("AHX", IZY, true),
    /* 94 */ OP("STY", ZPX, false), OP("STA", ZPX, false), OP("STX", ZPY, false), OP("SAX", ZPY, true),
    /* 98 */ OP("TYA", IMP, false), OP("STA", ABY, false), OP("TXS", IMP, false), OP("TAS", ABY, true),
    /* 9C */ OP("SHY", ABX, true),  OP("STA", ABX, false), OP("SHX", ABY, true),  OP("AHX", ABY, true),
    /* A0 */ OP("LDY", IMM, false), OP("LDA", IZX, false), OP("LDX", IMM, false), OP("LAX", IZX, true),
    /* A4 */ OP("LDY", ZP,  false), OP("LDA", ZP,  false), OP("LDX", ZP,  false), OP("LAX", ZP,  true),
    /* A8 */ OP("TAY", IMP, false), OP("LDA", IMM, false), OP("TAX", IMP, false), OP("LAX", IMM, true),
    /* AC */ OP("LDY", ABS, false), OP("LDA", ABS, false), OP("LDX", ABS, false), OP("LAX", ABS, true),
    /* B0 */ OP("BCS", REL, false), OP("LDA", IZY, false), OP("KIL", IMP, true),  OP("LAX", IZY, true),
    /* B4 */ OP("LDY", ZPX, false), OP("LDA", ZPX, false), OP("LDX", ZPY, false), OP("LAX", ZPY, true),
    /* B8 */ OP("CLV", IMP, false), OP("LDA", ABY, false), OP("TSX", IMP, false), OP("LAS", ABY, true),
    /* BC */ OP("LDY", ABX, false), OP("LDA", ABX, false), OP("LDX", ABY, false), OP("LAX", ABY, true),
    /* C0 */ OP("CPY", IMM, false), OP("CMP", IZX, false), OP("NOP", IMM, true),  OP("DCP", IZX, true),
    /* C4 */ OP("CPY", ZP,  false), OP("CMP", ZP,  false), OP("DEC", ZP,  false), OP("DCP", ZP,  true),
    /* C8 */ OP("INY", IMP, false), OP("CMP", IMM, false), OP("DEX", IMP, false), OP("AXS", IMM, true),
    /* CC */ OP("CPY", ABS, false), OP("CMP", ABS, false), OP("DEC", ABS, false), OP("DCP", ABS, true),
    /* D0 */ OP("BNE", REL, false), OP("CMP", IZY, false), OP("KIL", IMP, true),  OP("DCP", IZY, true),
    /* D4 */ OP("NOP", ZPX, true),  OP("CMP", ZPX, false), OP("DEC", ZPX, false), OP("DCP", ZPX, true),
    /* D8 */ OP("CLD", IMP, false), OP("CMP", ABY, false), OP("NOP", IMP, true),  OP("DCP", ABY, true),
    /* DC */ OP("NOP", ABX, true),  OP("CMP", ABX, false), OP("DEC", ABX, false), OP("DCP", ABX, true),
    /* E0 */ OP("CPX", IMM, false), OP("SBC", IZX, false), OP("NOP", IMM, true),  OP("ISB", IZX, true),
    /* E4 */ OP("CPX", ZP,  false), OP("SBC", ZP,  false), OP("INC", ZP,  false), OP("ISB", ZP,  true),
    /* E8 */ OP("INX", IMP, false), OP("SBC", IMM, false), OP("NOP", IMP, false), OP("SBC", IMM, true),
    /* EC */ OP("CPX", ABS, false), OP("SBC", ABS, false), OP("INC", ABS, false), OP("ISB", ABS, true),
    /* F0 */ OP("BEQ", REL, false), OP("SBC", IZY, false), OP("KIL", IMP, true),  OP("ISB", IZY, true),
    /* F4 */ OP("NOP", ZPX, true),  OP("SBC", ZPX, false), OP("INC", ZPX, false), OP("ISB", ZPX, true),
    /* F8 */ OP("SED", IMP, false), OP("SBC", ABY, false), OP("NOP", IMP, true),  OP("ISB", ABY, true),
    /* FC */ OP("NOP", ABX, true),  OP("SBC", ABX, false), OP("INC", ABX, false), OP("ISB", ABX, true),
};

#undef OP

// Disassembly-only memory peek.
//
// Nintendulator's nestest.log disassembler annotates "= VV" using a side-effect
// free peek that returns open bus (0xFF) for the register range $2000-$401F: it
// never reads a live PPU/APU register (which would have side effects and isn't
// what a paper disassembler can see). This matches the golden log's "STA $4015 =
// FF" etc. at the tail of nestest. Everywhere else (RAM, ROM) we peek normally.
uint8_t trace_peek(Bus& bus, uint16_t address) {
    if (address >= 0x2000 && address < 0x4020) {
        return 0xFF;  // open bus for memory-mapped registers
    }
    return bus.cpu_peek(address);
}

} // namespace

CPUTrace::~CPUTrace() {
    if (m_owns_file && m_out) {
        std::fclose(m_out);
    }
}

CPUTrace* CPUTrace::instance() {
    static bool initialized = false;
    static CPUTrace* tracer = nullptr;
    if (!initialized) {
        initialized = true;
        const char* trace_env = std::getenv("TRACE");
        if (trace_env && trace_env[0] != '\0' && std::strcmp(trace_env, "0") != 0) {
            tracer = new CPUTrace();
            const char* path = std::getenv("TRACE_FILE");
            if (path && path[0] != '\0') {
                tracer->m_out = std::fopen(path, "w");
                tracer->m_owns_file = (tracer->m_out != nullptr);
            }
            if (!tracer->m_out) {
                tracer->m_out = stdout;
                tracer->m_owns_file = false;
            }
        }
    }
    return tracer;
}

void CPUTrace::emit(const CPU& cpu, const PPU& ppu, Bus& bus, uint64_t cpu_cycles) {
    if (!m_out) return;

    const uint16_t pc = cpu.get_pc();
    const uint8_t opcode = trace_peek(bus, pc);
    const OpInfo& info = kOps[opcode];
    const uint8_t b1 = trace_peek(bus, pc + 1);
    const uint8_t b2 = trace_peek(bus, pc + 2);

    // --- column 1: PC and the 1-3 instruction bytes ---
    char bytes[12];
    switch (info.len) {
        case 1:  std::snprintf(bytes, sizeof(bytes), "%02X      ", opcode); break;
        case 2:  std::snprintf(bytes, sizeof(bytes), "%02X %02X   ", opcode, b1); break;
        default: std::snprintf(bytes, sizeof(bytes), "%02X %02X %02X", opcode, b1, b2); break;
    }

    // --- column 2: disassembly with addressing-mode operands ---
    char dis[48];
    const char* mark = info.illegal ? "*" : " ";
    const uint16_t abs_addr = static_cast<uint16_t>(b1 | (b2 << 8));

    switch (info.mode) {
        case Mode::IMP:
            std::snprintf(dis, sizeof(dis), "%s%s", mark, info.name);
            break;
        case Mode::ACC:
            std::snprintf(dis, sizeof(dis), "%s%s A", mark, info.name);
            break;
        case Mode::IMM:
            std::snprintf(dis, sizeof(dis), "%s%s #$%02X", mark, info.name, b1);
            break;
        case Mode::ZP: {
            uint8_t v = trace_peek(bus, b1);
            std::snprintf(dis, sizeof(dis), "%s%s $%02X = %02X", mark, info.name, b1, v);
            break;
        }
        case Mode::ZPX: {
            uint8_t ea = static_cast<uint8_t>(b1 + cpu.get_x());
            uint8_t v = trace_peek(bus, ea);
            std::snprintf(dis, sizeof(dis), "%s%s $%02X,X @ %02X = %02X",
                          mark, info.name, b1, ea, v);
            break;
        }
        case Mode::ZPY: {
            uint8_t ea = static_cast<uint8_t>(b1 + cpu.get_y());
            uint8_t v = trace_peek(bus, ea);
            std::snprintf(dis, sizeof(dis), "%s%s $%02X,Y @ %02X = %02X",
                          mark, info.name, b1, ea, v);
            break;
        }
        case Mode::ABS: {
            uint8_t v = trace_peek(bus, abs_addr);
            std::snprintf(dis, sizeof(dis), "%s%s $%04X = %02X", mark, info.name, abs_addr, v);
            break;
        }
        case Mode::ABS_J:
            std::snprintf(dis, sizeof(dis), "%s%s $%04X", mark, info.name, abs_addr);
            break;
        case Mode::ABX: {
            uint16_t ea = static_cast<uint16_t>(abs_addr + cpu.get_x());
            uint8_t v = trace_peek(bus, ea);
            std::snprintf(dis, sizeof(dis), "%s%s $%04X,X @ %04X = %02X",
                          mark, info.name, abs_addr, ea, v);
            break;
        }
        case Mode::ABY: {
            uint16_t ea = static_cast<uint16_t>(abs_addr + cpu.get_y());
            uint8_t v = trace_peek(bus, ea);
            std::snprintf(dis, sizeof(dis), "%s%s $%04X,Y @ %04X = %02X",
                          mark, info.name, abs_addr, ea, v);
            break;
        }
        case Mode::IND: {
            // JMP ($NNNN): the 6502 page-wrap bug on the high-byte fetch.
            uint8_t lo = trace_peek(bus, abs_addr);
            uint16_t hi_addr = (abs_addr & 0xFF00) | ((abs_addr + 1) & 0x00FF);
            uint8_t hi = trace_peek(bus, hi_addr);
            uint16_t target = static_cast<uint16_t>(lo | (hi << 8));
            std::snprintf(dis, sizeof(dis), "%s%s ($%04X) = %04X",
                          mark, info.name, abs_addr, target);
            break;
        }
        case Mode::IZX: {
            uint8_t zp = static_cast<uint8_t>(b1 + cpu.get_x());
            uint8_t lo = trace_peek(bus, zp);
            uint8_t hi = trace_peek(bus, static_cast<uint8_t>(zp + 1));
            uint16_t ea = static_cast<uint16_t>(lo | (hi << 8));
            uint8_t v = trace_peek(bus, ea);
            std::snprintf(dis, sizeof(dis), "%s%s ($%02X,X) @ %02X = %04X = %02X",
                          mark, info.name, b1, zp, ea, v);
            break;
        }
        case Mode::IZY: {
            uint8_t lo = trace_peek(bus, b1);
            uint8_t hi = trace_peek(bus, static_cast<uint8_t>(b1 + 1));
            uint16_t base = static_cast<uint16_t>(lo | (hi << 8));
            uint16_t ea = static_cast<uint16_t>(base + cpu.get_y());
            uint8_t v = trace_peek(bus, ea);
            std::snprintf(dis, sizeof(dis), "%s%s ($%02X),Y = %04X @ %04X = %02X",
                          mark, info.name, b1, base, ea, v);
            break;
        }
        case Mode::REL: {
            uint16_t target = static_cast<uint16_t>(pc + 2 + static_cast<int8_t>(b1));
            std::snprintf(dis, sizeof(dis), "%s%s $%04X", mark, info.name, target);
            break;
        }
    }

    // --- column 3: register / PPU / cycle state (pre-instruction) ---
    // PPU column is "<scanline>,<dot>" with each field right-justified width 3,
    // matching Nintendulator's "PPU:%3d,%3d".
    std::fprintf(m_out,
                 "%04X  %s %-31s A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu\n",
                 pc, bytes, dis,
                 cpu.get_a(), cpu.get_x(), cpu.get_y(), cpu.get_status(), cpu.get_sp(),
                 ppu.get_scanline(), ppu.get_cycle(),
                 static_cast<unsigned long long>(cpu_cycles));
}

} // namespace nes
