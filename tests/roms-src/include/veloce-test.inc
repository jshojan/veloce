; veloce-test.inc (ca65) -- VELOCE-RESULT/1 emitter over the NES $401E/$401F
; port. docs/testing/VELOCE-RESULT.md sections 5-6 define the wire format;
; this file is the "one include per assembler" section 6 calls for.
;
; The only hardware primitive is vt_putc (STA $401E): $4018-$401F are the
; 2A03's disabled CPU test registers, so a ROM built with this include still
; runs unmodified on real NES/Famicom hardware (writes ignored, $401F reads
; open bus). Everything else -- decimal/hex digit printing, the VT_* line
; macros -- is built from vt_putc alone, register-only except for one 2-byte
; zero-page pointer (VT_PTR) used to print string literals; see below.
;
; Vocabulary (one line each; docs/testing/VELOCE-RESULT.md section 6):
;   VT_BEGIN "suite"                    -> "VELOCE 1 nes <suite>\n"
;   VT_TEST  id, "name"                 -> "TEST <id> <name>\n"
;   VT_PASS  id, "name"                 -> "CHECK <id> PASS <name>\n"
;   VT_FAIL  id, "name", exp, got       -> "CHECK <id> FAIL <name> exp=<hex> got=<hex>\n"
;   VT_REG   "name", operand            -> "REG <name>=<hex>\n"
;   VT_LOG   "text"                     -> "LOG <text>\n"
;   VT_END   pass, total                -> "END <pass>/<total>\n"
;
; id/pass/total are plain decimal 0-99; exp/got/the REG operand are one byte
; printed as 2 lowercase hex digits. Every one of these is a plain `lda`
; SOURCE, not a value: the caller supplies the addressing mode, exactly like
; VT_REG's `operand`. Write `VT_TEST #1, "name"` for a compile-time id,
; `VT_TEST current_page, "name"` for a runtime loop counter (AccuracyCoin's
; per-page/per-test id, NES-4); `VT_FAIL id, "name", #$2a, crc_lo` for a
; fixed expected value against an observed one read out of RAM/a register.
; VT_HEX16 prints a 16-bit value (A=hi, X=lo) as 4 hex digits for suites
; that need it (e.g. a CRC-32 low/high word); it is not wired into a macro
; here since no vocabulary line needs it yet.
;
; VT_PTR: printing a string literal needs one 2-byte zero-page pointer (the
; classic "JSR with the string inlined right after it" trick: vt_puts pops
; its own return address, which points at the bytes emitted by vt_str/vt_line
; right after the `jsr vt_puts`, streams them through vt_putc up to the
; terminating zero, then pushes back the address just past the string so the
; enclosing RTS resumes past it). If the including source has not already
; defined VT_PTR, this file reserves its own 2 bytes in the ZEROPAGE segment.
; A source patching an existing suite (NES-5/6/7) should instead set VT_PTR
; to two bytes that suite already reserves, to avoid growing its zero page.

.ifndef VELOCE_TEST_INC
VELOCE_TEST_INC = 1

VT_PORT_DATA    := $401E
VT_PORT_CONTROL := $401F

.ifndef VT_PTR
    .segment "ZEROPAGE"
vt_ptr_lo: .res 1
vt_ptr_hi: .res 1
    .segment "CODE"
VT_PTR = vt_ptr_lo
.endif

; ---- primitives ---------------------------------------------------------

.proc vt_putc                  ; A = byte to emit. Preserves X, Y.
    sta VT_PORT_DATA
    rts
.endproc

.proc vt_nibble                ; A = 0..15 -> one lowercase hex digit
    and #$0F
    cmp #10
    bcc @digit
    clc
    adc #'a'-10
    jmp vt_putc
@digit:
    clc
    adc #'0'
    jmp vt_putc
.endproc

.proc vt_hex8                  ; A = byte -> 2 lowercase hex digits
    pha
    lsr a
    lsr a
    lsr a
    lsr a
    jsr vt_nibble
    pla
    jmp vt_nibble
.endproc

.proc vt_hex16                 ; A = hi byte, X = lo byte -> 4 hex digits
    jsr vt_hex8
    txa
    jmp vt_hex8
.endproc

.proc vt_dec8                  ; A = 0..99 -> 1-2 decimal digits, no leading zero
    ldx #0
@tens:
    cmp #10
    bcc @ones
    sbc #10
    inx
    jmp @tens
@ones:
    pha
    txa
    beq @skip_tens
    clc
    adc #'0'
    jsr vt_putc
@skip_tens:
    pla
    clc
    adc #'0'
    jmp vt_putc
.endproc

.proc vt_newline
    lda #$0A
    jmp vt_putc
.endproc

; Prints the zero-terminated string inlined right after the call, e.g.
;   jsr vt_puts
;   .byte "hello",0
; Execution resumes with the instruction after the .byte block. Does NOT
; preserve A (every VT_* macro reloads A right after a vt_str/vt_line, so
; this stays register-cheap on purpose); X is used as the stack-relative
; index and is restored to its entry value by the time RTS runs.
.proc vt_puts
    tsx
    lda $0101,x             ; return addr - 1, low byte (pushed by JSR)
    sta VT_PTR
    lda $0102,x
    sta VT_PTR+1
@advance:
    inc VT_PTR
    bne @read
    inc VT_PTR+1
@read:
    ldy #0
    lda (VT_PTR),y
    beq @done               ; VT_PTR now points AT the terminator
    jsr vt_putc
    jmp @advance
@done:
    lda VT_PTR+1            ; store (terminator address) as the new
    sta $0102,x             ; return-addr-minus-1: RTS's automatic +1
    lda VT_PTR               ; lands exactly on the byte after the
    sta $0101,x              ; terminator.
    rts
.endproc

.macro vt_str s
    jsr vt_puts
    .byte s, 0
.endmacro

.macro vt_line s
    jsr vt_puts
    .byte s, 10, 0
.endmacro

; ---- VELOCE-RESULT/1 vocabulary -----------------------------------------

.macro VT_BEGIN suite
    vt_str "VELOCE 1 nes "
    vt_line suite
.endmacro

.macro VT_TEST id, name
    vt_str "TEST "
    lda id
    jsr vt_dec8
    lda #' '
    jsr vt_putc
    vt_line name
.endmacro

.macro VT_PASS id, name
    vt_str "CHECK "
    lda id
    jsr vt_dec8
    vt_str " PASS "
    vt_line name
.endmacro

.macro VT_FAIL id, name, exp, got
    vt_str "CHECK "
    lda id
    jsr vt_dec8
    vt_str " FAIL "
    vt_str name
    vt_str " exp="
    lda exp
    jsr vt_hex8
    vt_str " got="
    lda got
    jsr vt_hex8
    jsr vt_newline
.endmacro

.macro VT_REG name, operand
    vt_str "REG "
    vt_str name
    lda #'='
    jsr vt_putc
    lda operand
    jsr vt_hex8
    jsr vt_newline
.endmacro

.macro VT_LOG text
    vt_str "LOG "
    vt_line text
.endmacro

.macro VT_END pass, total
    vt_str "END "
    lda pass
    jsr vt_dec8
    lda #'/'
    jsr vt_putc
    lda total
    jsr vt_dec8
    jsr vt_newline
.endmacro

.endif ; VELOCE_TEST_INC
