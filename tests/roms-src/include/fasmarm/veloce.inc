; ---------------------------------------------------------------------------
; tests/roms-src/include/fasmarm/veloce.inc  (GBA-2)
;
; VELOCE-RESULT/1 emitter for FASMARM ROMs (docs/testing/VELOCE-RESULT.md,
; sections 5-7). Targets the mGBA debug registers that gba::TestChannel
; already forwards one flush at a time:
;
;   0x4FFF600  REG_DEBUG_STRING  256-byte line buffer, written directly
;   0x4FFF700  REG_DEBUG_FLAGS   write (level & 7) | 0x100 to flush one line
;   0x4FFF780  REG_DEBUG_ENABLE  write 0xC0DE, reads back 0x1DEA when a
;                                debugger (mGBA or Veloce) is attached
;
; All three are unmapped on real hardware (open bus on read, ignored on
; write), so every macro here is a no-op there and identical to how
; mgba-emu/suite's libgba/mgba.c talks to the same registers.
;
; Grammar produced (docs/testing/VELOCE-RESULT.md section 2): every macro
; builds exactly one line directly inside REG_DEBUG_STRING and flushes it
; with one write to REG_DEBUG_FLAGS -- one flush is one line, matching
; gba::TestChannel::debug_flush() (cores/gba/src/test_channel.cpp), which
; appends '\n' itself, so no macro here writes one.
;
; Usage: two files, matching the existing lib/macros.inc + lib/text.asm
; split (macros vs. real code) so this can sit ahead of the GBA header
; without pushing it off offset 0:
;   - `include 'veloce.inc'` (this file) near the top, after `format binary
;     as 'gba'` / `org 0x08000000` and before the header -- it defines
;     macros and constants only and emits zero bytes, so it cannot collide
;     with the header at offset 0. Macros must be defined before use,
;     unlike labels, so this goes before any m_vt_* call.
;   - `include 'veloce_code.inc'` once, anywhere AFTER the header (the ROM's
;     idle loop is the conventional place, same as lib/text.asm) -- it is
;     real code (vt_puts/vt_puthex32/vt_flush) and must not be the first
;     bytes in the file.
; See tests/roms-src/gba/veloce_smoke_fasmarm/ for a complete ROM.
;
; `console`/`suite`/`id`/`name` are compile-time string literals -- the
; testkit's detector counts CHECK lines for pass/total, it does not parse
; `id` numerically, so an FASMARM string constant (e.g. '3') is enough and
; needs no runtime decimal conversion. `pass`/`total` in m_vt_end ARE parsed
; numerically by the detector, so pass a literal decimal string that matches
; how many CHECK lines the ROM actually emits (e.g. '2','2').
; `exp_reg`/`got_reg`/`reg` are real ARM registers (r0-r12): their runtime
; 32-bit value is hex-formatted (8 lowercase digits, matching the shared
; spec's "hex is lowercase without prefix").
;
; Register convention: every m_vt_* macro saves and restores every register
; it touches (r0-r3, r9-r11, lr), so it can be dropped between arbitrary
; test code with no register-allocation contract, the same guarantee
; lib/macros.inc's m_vsync/m_test_eval already give callers. r9 is the
; write cursor into REG_DEBUG_STRING while a line is being built; r10/r11
; hold a second and third runtime argument (m_vt_fail's exp/got) copied out
; before r0-r3 are reused as scratch, so aliasing (e.g. exp_reg == r0) is
; safe.
;
; Known gap: the shared spec's optional `CHECK ... FAIL ... mask=<hex>`
; field (docs/testing/VELOCE-RESULT.md section 2/6, `VT_FAIL id,name,exp,
; got[,mask]`) has no m_vt_fail_mask counterpart here yet -- no GBA suite
; needs a partial-register comparison today. Whoever first does (e.g. a
; flags-only check) should add it following the m_vt_fail pattern rather
; than work around its absence.
; ---------------------------------------------------------------------------

VT_DEBUG_STRING = 0x4FFF600
VT_DEBUG_FLAGS  = 0x4FFF700
VT_DEBUG_ENABLE = 0x4FFF780
VT_LEVEL_INFO   = 3

; None of VT_DEBUG_* is encodable as a single ARM `mov`/`orr` immediate (the
; 8-bit-rotated-by-an-even-amount restriction), so every load goes through
; this byte-at-a-time construction -- the same trick lib/macros.inc's
; m_word already uses for arbitrary 32-bit constants.
macro m_vt_load32 reg, value {
        mov     reg, value and 0xFF
        orr     reg, value and 0xFF00
        orr     reg, value and 0xFF0000
        orr     reg, value and 0xFF000000
}

; ---- emitter macros (docs/testing/VELOCE-RESULT.md section 6 vocabulary) --

; Optional probe: write the mGBA handshake and read it back. Sets the Z flag
; (`beq`/`bne` after the macro) when a debugger answered 0x1DEA, exactly
; like mgba_open(). Inert and harmless to call even when nothing is
; listening -- every m_vt_* macro flushes unconditionally regardless of
; this probe's result, so calling it is informational, never required.
; Clobbers r0-r1.
macro m_vt_open {
        stmfd   sp!, \{r0-r1,lr\}
        m_vt_load32 r0, VT_DEBUG_ENABLE
        m_vt_load32 r1, 0xC0DE
        strh    r1, [r0]
        ldrh    r1, [r0]
        m_vt_load32 r0, 0x1DEA
        cmp     r1, r0                  ; Z set (EQ) when a debugger answered
        ldmfd   sp!, \{r0-r1,lr\}        ; a plain memory load: flags survive
}

; "VELOCE 1 <console> <suite>" -- always the first VELOCE-RESULT/1 line.
macro m_vt_begin console, suite {
local .str
local .after
        stmfd   sp!, \{r0-r3,r9,lr\}
        mov     r9, 0
        adr     r0, .str
        bl      vt_puts
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9,lr\}
        b       .after
.str:
        db      'VELOCE 1 ',console,' ',suite,0
align 4
.after:
}

; "TEST <id> <name>" -- progress marker before a sub-test's checks.
macro m_vt_test id, name {
local .str
local .after
        stmfd   sp!, \{r0-r3,r9,lr\}
        mov     r9, 0
        adr     r0, .str
        bl      vt_puts
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9,lr\}
        b       .after
.str:
        db      'TEST ',id,' ',name,0
align 4
.after:
}

; "CHECK <id> PASS <name>"
macro m_vt_pass id, name {
local .str
local .after
        stmfd   sp!, \{r0-r3,r9,lr\}
        mov     r9, 0
        adr     r0, .str
        bl      vt_puts
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9,lr\}
        b       .after
.str:
        db      'CHECK ',id,' PASS ',name,0
align 4
.after:
}

; "CHECK <id> FAIL <name> exp=<hex8> got=<hex8>"
macro m_vt_fail id, name, exp_reg, got_reg {
local .s1,.s2
local .after
        stmfd   sp!, \{r0-r3,r9-r11,lr\}
        mov     r9, 0
        mov     r10, exp_reg            ; copied out before r0-r3 become scratch
        mov     r11, got_reg            ; (safe even if exp_reg/got_reg alias r0-r3)
        adr     r0, .s1
        bl      vt_puts
        mov     r0, r10
        bl      vt_puthex32
        adr     r0, .s2
        bl      vt_puts
        mov     r0, r11
        bl      vt_puthex32
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9-r11,lr\}
        b       .after
.s1:    db      'CHECK ',id,' FAIL ',name,' exp=',0
.s2:    db      ' got=',0
align 4
.after:
}

; "REG <name>=<hex8>" -- raw observed state, no verdict. Call once per
; register; multiple REG lines are as valid as one line with several pairs.
macro m_vt_reg name, reg {
local .s1
local .after
        stmfd   sp!, \{r0-r3,r9-r10,lr\}
        mov     r9, 0
        mov     r10, reg
        adr     r0, .s1
        bl      vt_puts
        mov     r0, r10
        bl      vt_puthex32
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9-r10,lr\}
        b       .after
.s1:    db      'REG ',name,'=',0
align 4
.after:
}

; "LOG <text>" -- free text, compile-time literal.
macro m_vt_log text {
local .str
local .after
        stmfd   sp!, \{r0-r3,r9,lr\}
        mov     r9, 0
        adr     r0, .str
        bl      vt_puts
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9,lr\}
        b       .after
.str:
        db      'LOG ',text,0
align 4
.after:
}

; "END <pass>/<total>" -- terminator. `pass`/`total` are decimal-text
; literals (e.g. '1','2'): the detector parses them numerically, unlike
; every other macro's id/name arguments. Once the sink sees a line starting
; "END" it is the terminator by itself; no separate finish() call is needed.
macro m_vt_end pass, total {
local .s1,.s2
local .after
        stmfd   sp!, \{r0-r3,r9,lr\}
        mov     r9, 0
        adr     r0, .s1
        bl      vt_puts
        mov     r0, VT_LEVEL_INFO
        bl      vt_flush
        ldmfd   sp!, \{r0-r3,r9,lr\}
        b       .after
.s1:    db      'END ',pass,'/',total,0
align 4
.after:
}
