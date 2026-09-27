; veloce-test-asm6f.inc (asm6f) -- VELOCE-RESULT/1 emitter over the NES
; $401E/$401F port. asm6f dialect of tests/roms-src/include/veloce-test.inc
; (ca65); see that file's header comment for the wire format, the full
; vocabulary table and why the port is hardware-inert. This file mirrors it
; instruction-for-instruction; only the assembler syntax differs (MACRO/ENDM
; instead of .macro/.endmacro, DB instead of .byte, no ca65 "segments").
;
; Unlike the ca65 version, this file does NOT auto-reserve its zero-page
; pointer: asm6f has no segment/zeropage abstraction to hook into safely, so
; the including source must define VT_PTR (2 free, consecutive zero-page
; bytes) BEFORE including this file, e.g.:
;
;   VT_PTR = $F0        ; 2 bytes, $F0-$F1, free in this suite's zero page
;   INCLUDE "veloce-test-asm6f.inc"
;
; This is exactly the 2005-era asm6f suites' own convention (they all
; reserve their own zero page by hand already; NES-6/7 patch one of their
; existing free bytes in here rather than growing the ROM's zero page).
;
; Deliberately no whole-file IFNDEF/ENDIF double-include guard here (unlike
; the ca65 version): this pinned asm6f build (freem/asm6f fd03633e) hits a
; real assembler bug when a forward branch (BCC/BCS/BEQ/BNE to a
; not-yet-defined label -- vt_nibble, vt_dec8 and vt_puts all have one) sits
; inside an IFNDEF/ENDIF block together with any MACRO invocation anywhere
; in the same file: pass 2 fails with a spurious "Illegal instruction" at
; the macro call site. Reproduced down to a two-instruction repro (`CMP
; #10 / BCC label` followed later by any `MACRO`/`ENDM` pair and one call);
; removing either the enclosing IFNDEF/ENDIF or the forward branch makes it
; go away. Filed nowhere upstream yet -- flag it if freem/asm6f ever lands
; a fix and this workaround can be dropped. Include this file exactly once
; per ROM (every consuming suite already does; nothing here is idempotent
; if included twice).

IFNDEF VT_PTR
    FAIL ; VT_PTR (2 free zero-page bytes) must be defined before including veloce-test-asm6f.inc
ENDIF

VT_PORT_DATA    = $401E
VT_PORT_CONTROL = $401F

; ---- primitives ----------------------------------------------------------

vt_putc:                       ; A = byte to emit. Preserves X, Y.
    STA VT_PORT_DATA
    RTS

vt_nibble:                     ; A = 0..15 -> one lowercase hex digit
    AND #$0F
    CMP #10
    BCC vt_nibble_digit
    CLC
    ADC #'a'-10
    JMP vt_putc
vt_nibble_digit:
    CLC
    ADC #'0'
    JMP vt_putc

vt_hex8:                       ; A = byte -> 2 lowercase hex digits
    PHA
    LSR A
    LSR A
    LSR A
    LSR A
    JSR vt_nibble
    PLA
    JMP vt_nibble

vt_hex16:                      ; A = hi byte, X = lo byte -> 4 hex digits
    JSR vt_hex8
    TXA
    JMP vt_hex8

vt_dec8:                       ; A = 0..99 -> 1-2 decimal digits, no leading zero
    LDX #0
vt_dec8_tens:
    CMP #10
    BCC vt_dec8_ones
    SBC #10
    INX
    JMP vt_dec8_tens
vt_dec8_ones:
    PHA
    TXA
    BEQ vt_dec8_skip_tens
    CLC
    ADC #'0'
    JSR vt_putc
vt_dec8_skip_tens:
    PLA
    CLC
    ADC #'0'
    JMP vt_putc

vt_newline:
    LDA #$0A
    JMP vt_putc

; Prints the zero-terminated string inlined right after the call, e.g.
;   JSR vt_puts
;   DB "hello",0
; Execution resumes with the instruction after the DB block. Does NOT
; preserve A (every VT_* macro below reloads A right after a vt_str/vt_line);
; X is used as the stack-relative index and is restored to its entry value
; by the time RTS runs.
vt_puts:
    TSX
    LDA $0101,x             ; return addr - 1, low byte (pushed by JSR)
    STA VT_PTR
    LDA $0102,x
    STA VT_PTR+1
vt_puts_advance:
    INC VT_PTR
    BNE vt_puts_read
    INC VT_PTR+1
vt_puts_read:
    LDY #0
    LDA (VT_PTR),y
    BEQ vt_puts_done               ; VT_PTR now points AT the terminator
    JSR vt_putc
    JMP vt_puts_advance
vt_puts_done:
    LDA VT_PTR+1            ; store (terminator address) as the new
    STA $0102,x             ; return-addr-minus-1: RTS's automatic +1
    LDA VT_PTR              ; lands exactly on the byte after the
    STA $0101,x             ; terminator.
    RTS

MACRO vt_str s
    JSR vt_puts
    DB s, 0
ENDM

MACRO vt_line s
    JSR vt_puts
    DB s, 10, 0
ENDM

; ---- VELOCE-RESULT/1 vocabulary ------------------------------------------
; Every numeric parameter below (id, pass, total, exp, got, VT_REG's operand)
; is a plain LDA SOURCE, not a value: the caller supplies the addressing
; mode, e.g. "VT_TEST #1,"name"" for a compile-time id, "VT_TEST
; current_page,"name"" for a runtime loop counter, "VT_FAIL id,"name",#$2a,
; crc_lo" for a fixed expected value against an observed one read out of
; RAM/a register. Decimal params (id/pass/total) print as 0-99, byte params
; (exp/got/operand) print as 2 lowercase hex digits.

MACRO VT_BEGIN suite
    vt_str "VELOCE 1 nes "
    vt_line suite
ENDM

MACRO VT_TEST id, name
    vt_str "TEST "
    LDA id
    JSR vt_dec8
    LDA #$20        ; ' '
    JSR vt_putc
    vt_line name
ENDM

MACRO VT_PASS id, name
    vt_str "CHECK "
    LDA id
    JSR vt_dec8
    vt_str " PASS "
    vt_line name
ENDM

MACRO VT_FAIL id, name, exp, got
    vt_str "CHECK "
    LDA id
    JSR vt_dec8
    vt_str " FAIL "
    vt_str name
    vt_str " exp="
    LDA exp
    JSR vt_hex8
    vt_str " got="
    LDA got
    JSR vt_hex8
    JSR vt_newline
ENDM

MACRO VT_REG name, operand
    vt_str "REG "
    vt_str name
    LDA #$3D        ; '='
    JSR vt_putc
    LDA operand
    JSR vt_hex8
    JSR vt_newline
ENDM

MACRO VT_LOG text
    vt_str "LOG "
    vt_line text
ENDM

MACRO VT_END pass, total
    vt_str "END "
    LDA pass
    JSR vt_dec8
    LDA #$2F        ; '/'
    JSR vt_putc
    LDA total
    JSR vt_dec8
    JSR vt_newline
ENDM

