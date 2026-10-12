#!/usr/bin/env python3
"""Build an original CPU-store/SP-DMA RSP IMEM rewrite cartridge."""
import importlib.util
import argparse
from pathlib import Path

HERE = Path(__file__).resolve().parent
source = HERE.parent / "rsp_scalar_alu" / "build.py"
spec = importlib.util.spec_from_file_location("rsp_scalar_emitter", source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
Program = module.Program


def build(wrong_instruction=False, no_dma=False):
    p = Program()
    p.data[:4] = bytes.fromhex("80371240")
    p.data[8:12] = bytes.fromhex("80000400")
    p.data[0x20:0x34] = b"VELOCE RSP IMEM".ljust(20, b" ")
    p.data[0x34:0x3c] = b"V64HB001"
    p.i(15, 0, 30, 0xbff0)  # Debug-port byte output.
    p.i(15, 0, 24, 0xa400)  # SP DMEM/IMEM, uncached KSEG1.
    p.i(15, 0, 25, 0xa404)  # SP DMA/status registers.
    p.i(15, 0, 26, 0xa408)  # SP program counter.
    p.i(15, 0, 9, 0xa001)   # Uncached RDRAM DMA source $00010000.
    p.i(15, 0, 22, 0xac02)  # RSP SW r2,0(r0).
    p.i(43, 24, 22, 4100)
    p.i(43, 9, 22, 4)      # Same second word in the DMA source.
    p.i(9, 0, 23, 13)      # RSP BREAK.
    p.i(43, 24, 23, 4104)
    p.i(9, 0, 18, 0)       # Mode 0 CPU store, mode 1 SP DMA.
    p.puts("header")
    p.mark("mode")
    p.i(9, 0, 16, 0)       # Immediate byte.
    p.i(9, 0, 17, 256)
    p.mark("sample")
    p.i(15, 0, 8, 0x2402)  # RSP ADDIU r2,r0,imm.
    p.r(37, 8, 16, 8)
    if wrong_instruction:
        p.i(14, 8, 8, 1)   # Executed mutant changes the guest instruction.
    p.branch(5, 18, 0, "dma")
    p.i(43, 24, 8, 4096)
    p.branch(4, 0, 0, "start")
    p.mark("dma")
    p.i(43, 9, 8, 0)
    p.i(9, 0, 10, 4096)
    p.i(43, 25, 10, 0)     # SP_MEM_ADDR = IMEM start.
    p.i(15, 0, 10, 1)
    p.i(43, 25, 10, 4)     # SP_DRAM_ADDR = $00010000.
    p.i(9, 0, 10, 7)
    if no_dma:
        p.word(0)            # Executed negative control: disarm DMA.
    else:
        p.i(43, 25, 10, 8) # Eight-byte DRAM -> IMEM DMA.
    p.mark("start")
    p.i(43, 24, 0, 0)      # Clear observed DMEM word.
    p.i(43, 26, 0, 0)      # Restart at RSP PC 0.
    p.i(9, 0, 8, 0xad)
    p.i(43, 25, 8, 16)     # Clear halt/broke, run RSP.
    p.mark("wait")
    p.i(39, 25, 8, 16)
    p.i(12, 8, 8, 1)
    p.branch(4, 8, 0, "wait")
    p.i(39, 24, 19, 0)     # Actual RSP store to DMEM.
    p.i(39, 24, 20, 4096)  # Actual IMEM instruction readback.
    p.i(39, 25, 21, 16)   # Actual HALT/BROKE status.
    p.i(39, 26, 27, 0)    # Actual SP PC after BREAK.
    p.puts("row")
    for reg in (19, 20, 21, 27):
        p.r(37, reg, 0, 5)
        p.jump(3, "hex")
    p.i(9, 0, 8, 10)
    p.i(40, 30, 8, 0)
    p.i(9, 16, 16, 1)
    p.i(9, 17, 17, -1)
    p.branch(5, 17, 0, "sample")
    p.branch(5, 18, 0, "sweep_setup")
    p.i(9, 0, 18, 1)
    p.branch(4, 0, 0, "mode")
    # Prime every aligned IMEM slot with a different instruction, then rewrite
    # the same slot by CPU store or aligned eight-byte DMA. The RSP executes
    # SW and BREAK in the following two slots, wrapping at the bank boundary.
    p.mark("sweep_setup")
    p.i(9, 0, 18, 0)
    p.mark("sweep_mode")
    p.i(9, 0, 16, 0)
    p.i(9, 0, 17, 1024)
    p.mark("sweep_sample")
    p.r(0, 0, 16, 11, 2)  # Slot byte offset.
    p.r(33, 24, 11, 12)  # CPU-visible target address, minus IMEM bank offset.
    p.i(9, 11, 13, 4)
    p.i(12, 13, 13, 4095)
    p.r(33, 24, 13, 13)
    p.i(43, 13, 22, 4096)
    p.i(9, 11, 14, 8)
    p.i(12, 14, 14, 4095)
    p.r(33, 24, 14, 14)
    p.i(43, 14, 23, 4096)
    p.i(12, 16, 8, 255)
    p.i(14, 8, 8, 255)
    p.i(15, 0, 10, 0x2402)
    p.r(37, 8, 10, 8)
    p.i(43, 12, 8, 4096)  # Seed stale fetched word.
    p.i(43, 24, 0, 0)
    p.i(43, 26, 11, 0)
    p.i(9, 0, 8, 0xad)
    p.i(43, 25, 8, 16)
    p.mark("sweep_warm_wait")
    p.i(39, 25, 8, 16)
    p.i(12, 8, 8, 1)
    p.branch(4, 8, 0, "sweep_warm_wait")
    p.i(12, 16, 8, 255)
    p.i(15, 0, 10, 0x2402)
    p.r(37, 8, 10, 8)
    if wrong_instruction:
        p.i(14, 8, 8, 1)
    p.branch(5, 18, 0, "sweep_dma")
    p.i(43, 12, 8, 4096)
    p.branch(4, 0, 0, "sweep_start")
    p.mark("sweep_dma")
    p.i(12, 11, 15, 4088)
    p.i(9, 15, 15, 4096)
    p.i(43, 25, 15, 0)
    p.i(15, 0, 10, 1)
    p.i(43, 25, 10, 4)
    p.i(12, 16, 10, 1)
    p.branch(5, 10, 0, "sweep_dma_odd")
    p.i(43, 9, 8, 0)
    p.i(43, 9, 22, 4)
    p.branch(4, 0, 0, "sweep_dma_go")
    p.mark("sweep_dma_odd")
    p.i(43, 9, 0, 0)
    p.i(43, 9, 8, 4)
    p.mark("sweep_dma_go")
    p.i(9, 0, 10, 7)
    if no_dma:
        p.word(0)
    else:
        p.i(43, 25, 10, 8)
    p.mark("sweep_start")
    p.i(43, 24, 0, 0)
    p.i(43, 26, 11, 0)
    p.i(9, 0, 8, 0xad)
    p.i(43, 25, 8, 16)
    p.mark("sweep_wait")
    p.i(39, 25, 8, 16)
    p.i(12, 8, 8, 1)
    p.branch(4, 8, 0, "sweep_wait")
    p.i(39, 24, 19, 0)
    p.i(39, 12, 20, 4096)
    p.i(39, 25, 21, 16)
    p.i(39, 26, 27, 0)
    p.puts("row")
    for reg in (19, 20, 21, 27):
        p.r(37, reg, 0, 5)
        p.jump(3, "hex")
    p.i(9, 0, 8, 10)
    p.i(40, 30, 8, 0)
    p.i(9, 16, 16, 1)
    p.i(9, 17, 17, -1)
    p.branch(5, 17, 0, "sweep_sample")
    p.branch(5, 18, 0, "complete")
    p.i(9, 0, 18, 1)
    p.branch(4, 0, 0, "sweep_mode")
    p.mark("complete")
    p.puts("done")
    p.i(43, 30, 0, 4)
    p.mark("halt")
    p.branch(4, 0, 0, "halt")

    p.mark("puts")
    p.i(36, 4, 8, 0)
    p.branch(4, 8, 0, "puts_return")
    p.i(40, 30, 8, 0)
    p.i(9, 4, 4, 1)
    p.branch(4, 0, 0, "puts")
    p.mark("puts_return")
    p.r(8, rs=31)
    p.word(0)
    p.mark("hex")
    p.i(9, 0, 6, 28)
    p.mark("hex_loop")
    p.r(0x16, 6, 5, 7)
    p.i(12, 7, 7, 15)
    p.i(11, 7, 8, 10)
    p.branch(5, 8, 0, "digit")
    p.i(9, 7, 7, 39)
    p.mark("digit")
    p.i(9, 7, 7, 48)
    p.i(40, 30, 7, 0)
    p.i(9, 6, 6, -4)
    p.branch(1, 6, 1, "hex_loop")
    p.r(8, rs=31)
    p.word(0)
    p.string("header", "VELOCE 1 n64 rsp_imem_rewrite\nBEGIN data cases=2560 words=4\n")
    p.string("row", "REG r=")
    p.string("done", "LOG complete cases=2560\nEND observations_complete\n")
    while p.pc & 3:
        p.data.append(0)
    p.resolve()
    return bytes(p.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path,
                        default=HERE / "rsp_imem_rewrite.z64")
    output = parser.parse_args().output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(build())
    print(output)
