; veloce-test.asar.inc -- SNES-2
;
; STATUS: built and run end to end against the real veloce binary (unlike
; the bass include, asar itself works fine in tools/rom-toolchain) -- every
; macro below, the port write, the $21FF probe, both hex printers, and two
; runs' byte-identical determinism were all verified. See
; tests/roms-src/include/PROVENANCE.md for the exact repro (it also covers
; this file's test, despite the name -- that is where the asar/bass split
; was discovered).
;
; asar (v1.91) emitter for the VELOCE-RESULT/1 test-result channel
; (docs/testing/VELOCE-RESULT.md section 6), for asar-built byuu/jonasquinn
; xkas-source patches (SNES-6). Same vocabulary as every other console's
; veloce-test.inc; see tests/roms-src/include/snes/veloce-test.ca65.inc for
; the full grammar table -- this mirrors it:
;
;   %VT_BEGIN(suite) / %VT_TEST(id,name) / %VT_PASS(id,name) /
;   %VT_FAIL(id,name,exp,got) / %VT_REG_BEGIN() / %VT_REG8(name) /
;   %VT_REG16(name) / %VT_REG_END() / %VT_LOG_STR(text) / %VT_END(pass,total)
;
; asar has no compile-time string-character-extraction function (unlike
; ca65's `.strat`), so every compile-time string here uses the standard
; 6502/65816 "inline string after JSR" idiom instead: `jsr vt_puts` is
; immediately followed by the string's own bytes (`db <text>, 0`), and
; vt_puts reads its own return address off the stack (non-destructively, via
; stack-relative addressing) to find them, prints up to the NUL, then
; overwrites the stacked return address so RTS resumes right after the
; terminator. This needs one small zero-page pointer, !VT_PTR_ZP (2 bytes,
; default $EE-$EF) -- `!VT_PTR_ZP = $nn` before including this file to move
; it if that collides with a suite's own scratch page.
;
; Preconditions: DBR in $00-$3F/$80-$BF, native mode, 16-bit X/Y whenever any
; VT_* macro or vt_puts/vt_hex8/vt_hex16 runs. Each routine that needs 8-bit A
; forces it for real with SEP #$20 and restores the caller's actual width
; with PLP afterwards (never just a dialect-level assumption) -- see the
; ca65 include for why that distinction matters on the 65816.

!VT_PORT_DATA ?= $21FE
!VT_PORT_CTRL ?= $21FF
!VT_PTR_ZP ?= $EE

; ---------------------------------------------------------------------------
; vt_puts -- print the NUL-terminated string placed right after the JSR that
; calls this routine, then resume just past the terminator.
; Call as:  jsr vt_puts : db "text", 0
; In: none (the string comes from the return address). Out: none. Preserves
; A, X, Y, P (all restored) -- safe to call from anywhere satisfying the
; preconditions above.
; ---------------------------------------------------------------------------
vt_puts:
    php
    sep #$20
    ; Stack-relative, non-destructive: JSR pushed (return_addr - 1) as
    ; [hi][lo] with lo on top, so lo sits at S+2 and hi at S+3 (S+1 is the
    ; PHP flags byte we just pushed on top of both).
    lda $02,s
    sta !VT_PTR_ZP
    lda $03,s
    sta !VT_PTR_ZP+1
    rep #$20
    lda !VT_PTR_ZP
    inc a               ; the string's first byte is at (ret_addr-1)+1
    sta !VT_PTR_ZP
    sep #$20
    ldy.w #0    ; .w: this LDY runs with 16-bit index registers (this
                ; file's precondition); asar would otherwise guess 8-bit
                ; from the value 0 alone, misreading the next opcode byte
                ; as this instruction's second immediate byte.
vt_puts_loop:
    lda (!VT_PTR_ZP),y
    beq vt_puts_done
    sta !VT_PORT_DATA
    iny
    bra vt_puts_loop
vt_puts_done:
    rep #$20
    tya
    clc
    adc !VT_PTR_ZP       ; A = address of the NUL byte
    sta $02,s            ; one 16-bit store overwrites both stacked bytes
    plp
    rts

; ---------------------------------------------------------------------------
; Runtime hex printers -- for a value only known at runtime. Real
; subroutines (jsr), not macros.
; ---------------------------------------------------------------------------

; vt_hex8: print A as two hex ASCII chars.
; In: A = value (either width; only the low byte is used).
; Out: none. Preserves X, Y, P. Trashes A.
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
    cmp.b #10
    bcc vt_hex8_nibble_digit
    adc.b #$26          ; carry is set (cmp #10 with a nibble >= 10): adds $27,
                        ; 'a'-'9'-1, so digits 10-15 print as lowercase 'a'..'f'
vt_hex8_nibble_digit:
    adc.b #'0'
    sta !VT_PORT_DATA
    rts

; vt_hex16: print X as four hex ASCII chars, high byte first.
; In: X = value, 16-bit index registers.
; Out: none. Preserves P (restored) and X. Trashes A, Y.
vt_hex16:
    php
    phx
    sep #$30            ; 8-bit A and 8-bit X/Y so the pulled bytes and
                        ; TAY/TYA below each move exactly one byte
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

macro VT_BEGIN(suite)
    jsr vt_puts
    db "VELOCE 1 snes <suite>", 10, 0
endmacro

macro VT_TEST(id, name)
    jsr vt_puts
    db "TEST <id> <name>", 10, 0
endmacro

macro VT_PASS(id, name)
    jsr vt_puts
    db "CHECK <id> PASS <name>", 10, 0
endmacro

; exp/got are the literal hex text you want printed (asar has no
; compile-time numeric-to-hex-string function, unlike ca65's `.sprintf`), so
; pass them as strings, e.g. %VT_FAIL("2","name","01","02") not numeric $01.
; For a runtime value, print "CHECK id FAIL name " with %VT_LOG_STR (it has
; no trailing newline), then " exp=" + jsr vt_hex8/vt_hex16 + " got=" + jsr
; vt_hex8/vt_hex16 by hand, then a final `jsr vt_puts : db 10, 0`.
macro VT_FAIL(id, name, exp, got)
    jsr vt_puts
    db "CHECK <id> FAIL <name> exp=<exp> got=<got>", 10, 0
endmacro

; REG line: %VT_REG_BEGIN(), then any mix of %VT_REG8/%VT_REG16 (each prints
; "name=hex "), then %VT_REG_END(). VT_REG8 expects an 8-bit value already in
; A; VT_REG16 expects a 16-bit value already in X -- load the value
; immediately before each call (these macros stash it on the stack while
; they print "name=", exactly like the ca65 include, since vt_puts also
; needs A to move each character).
macro VT_REG_BEGIN()
    jsr vt_puts
    db "REG ", 0
endmacro

macro VT_REG8(name)
    pha
    jsr vt_puts
    db "<name>=", 0
    pla
    jsr vt_hex8
    jsr vt_puts
    db " ", 0
endmacro

macro VT_REG16(name)
    phx
    jsr vt_puts
    db "<name>=", 0
    plx
    jsr vt_hex16
    jsr vt_puts
    db " ", 0
endmacro

macro VT_REG_END()
    jsr vt_puts
    db 10, 0
endmacro

macro VT_LOG_STR(text)
    jsr vt_puts
    db "LOG <text>", 10, 0
endmacro

macro VT_END(pass, total)
    jsr vt_puts
    db "END <pass>/<total>", 10, 0
endmacro
