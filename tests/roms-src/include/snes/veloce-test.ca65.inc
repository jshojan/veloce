; veloce-test.ca65.inc -- SNES-2
;
; ca65 (--cpu 65816) emitter for the VELOCE-RESULT/1 test-result channel
; (docs/testing/VELOCE-RESULT.md section 6). Every SNES suite this include
; serves speaks the same vocabulary as every other console's veloce-test.inc:
;
;   VT_BEGIN suite            -> "VELOCE 1 snes <suite>\n"
;   VT_TEST  id,name           -> "TEST id name\n"
;   VT_PASS  id,name           -> "CHECK id PASS name\n"
;   VT_FAIL  id,name,exp,got[,mask]  -> "CHECK id FAIL name exp=.. got=..[ mask=..]\n"
;                                       (exp/got/mask must be assemble-time constants;
;                                       for a runtime value see VT_REG_BEGIN below)
;   VT_REG_BEGIN / VT_REG8 name / VT_REG16 name / VT_REG_END
;                               -> "REG name=v name=v ...\n" (runtime register values)
;   VT_LOG_STR text             -> "LOG text\n" (text is an assemble-time string)
;   VT_END pass,total           -> "END pass/total\n"
;
; The primitive is `sta $21FE` (docs/testing/VELOCE-RESULT.md section 5): any
; bank $00-$3F/$80-$BF, DMA-able, unused B-bus register. $21FF write is the
; control byte (unused by this include -- the END line's terminator is what the
; application's TestFileSink acts on); $21FF read returns $56 ('V'), a probe a
; ROM can use to tell whether Veloce's channel is present before it starts
; emitting to it (real hardware and other emulators return the register's true
; open-bus/no-op read value there instead).
;
; ---------------------------------------------------------------------------
; Why every macro here does its own PHP/SEP/PLP (65816 width correctness)
; ---------------------------------------------------------------------------
; ca65's .a8/.a16 (and .i8/.i16) directives only tell the ASSEMBLER how to
; encode an immediate operand; they never emit a SEP/REP, so they only match
; reality when the calling code's own SEP/REP already put the CPU in that
; width. Since every VT_* macro here can be invoked from code in *either*
; accumulator width (a test shell may be mid-16-bit-A when it hits a FAIL),
; every macro that needs an 8-bit immediate forces it for real with SEP #$20,
; and restores the caller's actual width with PLP afterwards -- it never
; touches ca65's own .a8/.a16 tracking, so the caller's surrounding assembler
; mode declaration (whatever it is) stays correct for the code that follows.
; This is the same reason vt_hex8/vt_hex16 below open with PHP+SEP and close
; with PLP rather than assuming a width.
;
; Preconditions: DBR (data bank register) in $00-$3F or $80-$BF when any VT_*
; macro executes (true whenever code is running from LoROM/HiROM banks in that
; range, which is where these test shells run). Native mode (not 6502
; emulation mode) -- SEP/REP #$30 semantics assumed throughout.

.ifndef VT_PORT_DATA
VT_PORT_DATA := $21FE
VT_PORT_CTRL := $21FF
.endif

; ---------------------------------------------------------------------------
; vt_putc_imm char -- write one assemble-time-constant byte to the port.
; Self-contained: safe to call in any accumulator width, any index width.
; ---------------------------------------------------------------------------
.macro vt_putc_imm char
    php
    sep #$20            ; real M=1 (8-bit A) -- SEP's own operand is always
                        ; one byte, unaffected by any width ambiguity.
    .byte $A9, (char) & $FF   ; LDA #char, raw-encoded: ca65's tracked .a8/.a16
                        ; state is irrelevant here since we hand-encode the
                        ; one immediate opcode that's ever width-ambiguous.
    sta VT_PORT_DATA   ; absolute addressing: never width-ambiguous.
    plp
.endmacro

; vt_putc_str s -- write an assemble-time string constant, character by
; character, via vt_putc_imm. `.strat`/`.strlen` are both compile-time-only
; ca65 pseudo-functions, so this never touches a runtime string pointer.
.macro vt_putc_str str1
    .repeat .strlen(str1), I
        ; {...} groups the argument: ca65's classic-macro call splits on a
        ; bare top-level comma, which .strat(str1, I) has one of.
        vt_putc_imm {.strat(str1, I)}
    .endrepeat
.endmacro

; ---------------------------------------------------------------------------
; Runtime hex printers -- for a value that is only known at runtime (a
; register, a computed result). These are real subroutines: JSR to them, not
; macros, so a suite that prints many REG/CHECK lines doesn't pay for a copy
; of the conversion logic at every call site.
; ---------------------------------------------------------------------------

; vt_hex8: print A as two hex ASCII chars.
; In: A = value (either width; only the low byte is used).
; Out: none. Preserves X, Y, P (restored). Trashes A.
vt_hex8:
    php
    sep #$20
    pha
    lsr a
    lsr a
    lsr a
    lsr a
    jsr vt_hex8_nibble
    pla
    and #$0F
    jsr vt_hex8_nibble
    plp
    rts
vt_hex8_nibble:
    cmp #10
    bcc :+
    adc #$26            ; carry is set here (cmp #10 with a nibble >= 10), so
                        ; this adds $27 total -- 'a'-'9'-1, landing on 'a'..'f'
                        ; (VELOCE-RESULT/1 hex is lowercase; SNES-3 fix)
:   adc #'0'
    sta VT_PORT_DATA
    rts

; vt_hex16: print X as four hex ASCII chars, high byte first.
; In: X = value, 16-bit index registers in effect (rep #$10) -- matches
;     however the caller loaded X in the first place.
; Out: none. Preserves P (restored) and X (PHX only copies it). Trashes A, Y.
vt_hex16:
    php
    phx
    sep #$30            ; 8-bit A *and* 8-bit X/Y, so the two pulled bytes
                        ; land cleanly and TAY/TYA below move a single byte.
    pla                 ; low byte of X (pushed last by PHX, popped first)
    tay
    pla                 ; high byte of X
    jsr vt_hex8
    tya
    jsr vt_hex8
    plp
    rts

; ---------------------------------------------------------------------------
; Compile-time vocabulary macros
; ---------------------------------------------------------------------------

.macro VT_BEGIN suite
    vt_putc_str "VELOCE 1 snes "
    vt_putc_str suite
    vt_putc_imm $0A
.endmacro

.macro VT_TEST id, name
    vt_putc_str "TEST "
    vt_putc_str id
    vt_putc_imm ' '
    vt_putc_str name
    vt_putc_imm $0A
.endmacro

.macro VT_PASS id, name
    vt_putc_str "CHECK "
    vt_putc_str id
    vt_putc_str " PASS "
    vt_putc_str name
    vt_putc_imm $0A
.endmacro

; exp/got/mask must be assemble-time constants (formatted here as lowercase
; hex, no leading zero-padding, matching detect.py's parser). For a value that
; is only known at runtime, print "CHECK id FAIL name " with VT_LOG_STR (no
; trailing newline -- there isn't one), then " exp=" + jsr vt_hex8/vt_hex16 +
; " got=" + jsr vt_hex8/vt_hex16 by hand, then vt_putc_imm $0A.
.macro VT_FAIL id, name, exp, got, mask
    vt_putc_str "CHECK "
    vt_putc_str id
    vt_putc_str " FAIL "
    vt_putc_str name
    vt_putc_str " exp="
    vt_putc_str {.sprintf("%x", exp)}
    vt_putc_str " got="
    vt_putc_str {.sprintf("%x", got)}
    .ifnblank mask
        vt_putc_str " mask="
        vt_putc_str {.sprintf("%x", mask)}
    .endif
    vt_putc_imm $0A
.endmacro

; REG line: VT_REG_BEGIN, then any mix of VT_REG8/VT_REG16 (each prints
; "name=hex "), then VT_REG_END. VT_REG8 expects an 8-bit value already in A;
; VT_REG16 expects a 16-bit value already in X (rep #$10 in effect) -- load
; the register/value into A or X yourself immediately before each call.
.macro VT_REG_BEGIN
    vt_putc_str "REG "
.endmacro

; Load the value into A (VT_REG8) or X (VT_REG16) immediately before calling
; these -- the macro itself stashes it on the stack while it prints "name="
; (which, like every vt_putc_*, needs A itself to move each character), then
; restores it right before the hex-print subroutine consumes it. Every
; vt_putc_imm/vt_putc_str call is its own balanced PHP/PLP pair, so nothing
; here disturbs a value already sitting deeper on the stack.
.macro VT_REG8 name
    pha
    vt_putc_str name
    vt_putc_imm '='
    pla
    jsr vt_hex8
    vt_putc_imm ' '
.endmacro

.macro VT_REG16 name
    phx
    vt_putc_str name
    vt_putc_imm '='
    plx
    jsr vt_hex16
    vt_putc_imm ' '
.endmacro

.macro VT_REG_END
    vt_putc_imm $0A
.endmacro

.macro VT_LOG_STR text
    vt_putc_str "LOG "
    vt_putc_str text
    vt_putc_imm $0A
.endmacro

.macro VT_END pass, total
    vt_putc_str "END "
    vt_putc_str {.sprintf("%d", pass)}
    vt_putc_imm '/'
    vt_putc_str {.sprintf("%d", total)}
    vt_putc_imm $0A
.endmacro
