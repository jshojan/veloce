; veloce-test-nesasm.inc (ClusterM/nesasm v3.6, aka NESASM3, pinned commit
; 2b39ec669082566aae043a04ed8f14d78824d3b9) -- VELOCE-RESULT/1 emitter over
; the NES $401E/$401F port. nesasm dialect of
; tests/roms-src/include/veloce-test.inc (ca65); see that file's header
; comment for the wire format, the full vocabulary table and why the port is
; hardware-inert. Mirrors it instruction-for-instruction; only the assembler
; syntax differs (.macro/.endm with \1.. \9 parameters instead of
; .macro/.endmacro with named ones, DB instead of .byte, no ca65 segments).
;
; Like the asm6f dialect (see its own file for why), the including source
; must define VT_PTR (2 free, consecutive zero-page bytes) BEFORE including
; this file, e.g.:
;
;   VT_PTR EQU $F0      ; 2 bytes, $F0-$F1, free in this ROM's zero page
;   start:              ; nesasm's ".include" needs a global label already
;       .INCLUDE "veloce-test-nesasm.inc"   ; in scope -- see below
;
; Two nesasm-specific quirks, found by hand-assembling this file against the
; pinned toolchain (this pin has no test suite of its own to catch these):
;   - the include directive is ".INCLUDE" (with the dot; this pinned build's
;     directive-list doc names it bare "INCLUDE", but that spelling is an
;     "Unknown instruction"), and it errors "Local symbol not allowed here"
;     if no global label has appeared yet in the file -- put it after your
;     ROM's own first label, never as literally the first line.
;   - indirect addressing is "[label],y", not "(label),y" like ca65/asm6f;
;     "(VT_PTR),y" silently assembles (no error) as something else entirely
;     (garbage `LDA` at runtime, not the intended indirect load) -- vt_puts
;     below uses "[VT_PTR],y".
;
; Include this file exactly once per ROM; nothing here is idempotent if
; included twice (AccuracyCoin, NES-4's consumer, only includes it once).

    IFNDEF VT_PTR
        FAIL ; VT_PTR (2 free zero-page bytes) must be defined before including veloce-test-nesasm.inc
    ENDIF

VT_PORT_DATA    EQU $401E
VT_PORT_CONTROL EQU $401F

; ---- primitives -----------------------------------------------------------

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
    LDA [VT_PTR],y          ; nesasm uses [..] for indirect addressing, not (..)
    BEQ vt_puts_done               ; VT_PTR now points AT the terminator
    JSR vt_putc
    JMP vt_puts_advance
vt_puts_done:
    LDA VT_PTR+1            ; store (terminator address) as the new
    STA $0102,x             ; return-addr-minus-1: RTS's automatic +1
    LDA VT_PTR              ; lands exactly on the byte after the
    STA $0101,x             ; terminator.
    RTS

vt_str  .macro                 ; \1 = string literal, no trailing newline
    JSR vt_puts
    DB \1, 0
    .endm

vt_line .macro                 ; \1 = string literal, trailing newline
    JSR vt_puts
    DB \1, 10, 0
    .endm

; ---- VELOCE-RESULT/1 vocabulary -------------------------------------------
; Every numeric parameter below (id, pass, total, exp, got, VT_REG's operand)
; is a plain LDA SOURCE, not a value: the caller supplies the addressing
; mode, e.g. "VT_TEST #1,"name"" for a compile-time id, "VT_TEST
; current_page,"name"" for a runtime loop counter (AccuracyCoin's
; per-page/per-test id, NES-4), "VT_FAIL id,"name",#$2a,crc_lo" for a fixed
; expected value against an observed one read out of RAM/a register. Names
; (\2 in VT_FAIL/VT_REG) are string literals, decimal params print as 0-99,
; byte params print as 2 lowercase hex digits.

VT_BEGIN .macro                ; \1 = suite name (string literal)
    vt_str "VELOCE 1 nes "
    vt_line \1
    .endm

VT_TEST .macro                 ; \1 = id, \2 = name
    vt_str "TEST "
    LDA \1
    JSR vt_dec8
    LDA #$20        ; ' '
    JSR vt_putc
    vt_line \2
    .endm

VT_PASS .macro                 ; \1 = id, \2 = name
    vt_str "CHECK "
    LDA \1
    JSR vt_dec8
    vt_str " PASS "
    vt_line \2
    .endm

VT_FAIL .macro                 ; \1 = id, \2 = name, \3 = exp, \4 = got
    vt_str "CHECK "
    LDA \1
    JSR vt_dec8
    vt_str " FAIL "
    vt_str \2
    vt_str " exp="
    LDA \3
    JSR vt_hex8
    vt_str " got="
    LDA \4
    JSR vt_hex8
    JSR vt_newline
    .endm

VT_REG  .macro                 ; \1 = name, \2 = operand (a plain LDA source)
    vt_str "REG "
    vt_str \1
    LDA #$3D        ; '='
    JSR vt_putc
    LDA \2
    JSR vt_hex8
    JSR vt_newline
    .endm

VT_LOG  .macro                 ; \1 = text
    vt_str "LOG "
    vt_line \1
    .endm

VT_END  .macro                 ; \1 = pass, \2 = total
    vt_str "END "
    LDA \1
    JSR vt_dec8
    LDA #$2F        ; '/'
    JSR vt_putc
    LDA \2
    JSR vt_dec8
    JSR vt_newline
    .endm
