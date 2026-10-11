; veloce_smoke.s -- SNES-2 / SH-5
;
; Emits a fixed VELOCE-RESULT/1 transcript exercising every macro in
; tests/roms-src/include/snes/veloce-test.ca65.inc: the port probe read, a
; genuine pass (from a real runtime comparison, not a hardcoded PASS line), a
; deliberate fixed-value FAIL (to prove the FAIL/exp/got formatting), a REG
; line built from two live register loads (8-bit and 16-bit), a LOG line, and
; the terminating END. Two runs of this ROM under VELOCE_TEST_OUT must produce
; byte-identical result files (SH-5's determinism gate) -- everything here is
; a compile-time constant or a value computed the same way every run, with no
; host-time, RNG or uninitialized read anywhere.
;
; Not a scored test (no tests/test_config.json entry): this is a build/plumb-
; ing fixture, and its transcript is deliberately a mixed PASS/FAIL (1/2) so
; both code paths in the include are actually exercised, not a "does the ROM
; run" smoke check that would only ever exercise the PASS path.

.include "veloce-test.ca65.inc"

.segment "CODE"
.export main

main:
    sei
    clc
    xce                 ; enter native mode
    sep #$20            ; 8-bit A
    rep #$10            ; 16-bit X/Y
    .a8
    .i16
    ldx #$1FFF
    txs

    VT_BEGIN "smoke"

    ; --- check 1: the $21FF probe read really returns $56 ('V') ---
    ; Standard long-branch idiom: BEQ/BNE only ever cover the one JMP right
    ; after them (a fixed, tiny distance); every longer hop is an
    ; unconditional JMP, which has no +-127 range limit. The macro-expanded
    ; PASS/FAIL bodies below are each well past what a short branch can reach.
    VT_TEST "1", "port probe read"
    lda VT_PORT_CTRL
    cmp #$56
    beq probe_is_ok
    jmp probe_fail
probe_is_ok:
    jmp probe_ok
probe_fail:
    ; Runtime FAIL path: VT_FAIL needs compile-time exp/got, so a genuinely
    ; unexpected probe value is reported by hand with the real primitives.
    vt_putc_str "CHECK 1 FAIL port probe read exp="
    vt_putc_str {.sprintf("%x", $56)}
    vt_putc_str " got="
    jsr vt_hex8
    vt_putc_imm $0A
    jmp check2
probe_ok:
    VT_PASS "1", "port probe read"
    jmp check2

check2:
    ; --- check 2: a deliberate, fixed-value FAIL to exercise VT_FAIL's
    ; exp=/got=/mask= formatting. Not a real assertion -- see file header.
    VT_TEST "2", "synthetic fail case"
    VT_FAIL "2", "synthetic fail case (fixed exp/got)", $01, $02

    ; --- REG line: one 8-bit value (the probe byte, still live in A from the
    ; check above -- reload explicitly so the REG line stands on its own) and
    ; one 16-bit value, both real register loads, not compile-time constants.
    VT_REG_BEGIN
    lda VT_PORT_CTRL
    VT_REG8 "probe"
    ldx #$1234
    VT_REG16 "x"
    VT_REG_END

    VT_LOG_STR "veloce_smoke transcript complete"

    ; 1 of the 2 CHECK lines above passed.
    VT_END 1, 2

forever:
    bra forever
