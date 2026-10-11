; veloce_smoke.s -- ca65 self-test ROM for tests/roms-src/include/veloce-test.inc.
;
; Emits a fixed, entirely compile-time-known VELOCE-RESULT/1 transcript over
; the $401E/$401F port (docs/testing/VELOCE-RESULT.md), then halts. Used two
; ways:
;   1. Every VT_* macro this repo ships is exercised at least once, so a
;      change to veloce-test.inc that breaks the emitter breaks this ROM's
;      known-good transcript, not a real test ROM's.
;   2. The determinism gate (docs/testing/VELOCE-RESULT.md / plan section 3):
;      run this ROM's build twice and byte-diff the two output files. There
;      is no PPU/APU use, no RAM read before it is written, and no clock or
;      random source, so both runs must be identical.
;
; Does nothing when $401E/$401F are open bus / ignored (real hardware): the
; visible behaviour is "power on, do nothing forever."

.include "veloce-test.inc"

.segment "ZEROPAGE"
smoke_id:  .res 1          ; proves VT_TEST/VT_PASS's id can be a runtime operand
smoke_got: .res 1          ; proves VT_FAIL's got can be a runtime operand

.segment "HEADER"
    .byte "NES", $1A
    .byte 2         ; 32K PRG (2 x 16K), matches ROM segment size $7FE0 + header
    .byte 0         ; no CHR
    .byte 0, 0
    .byte 0, 0, 0, 0, 0, 0, 0, 0

.segment "CODE"

.proc reset
    sei
    cld
    ldx #$FF
    txs
    ; PPU/APU are never touched, so no further hardware init is needed for
    ; this ROM's own purposes; a real console still boots into a stable,
    ; silent state (interrupts off, PPU rendering off at power-on).
    jsr main
forever:
    jmp forever
.endproc

.proc main
    VT_BEGIN "veloce_smoke"

    ; Every VT_* numeric parameter is a plain `lda` source, not a value: the
    ; caller picks the addressing mode. id=1/exp=$2a below use `#imm`
    ; (compile-time constants, the common case); id=2/got prove the OTHER
    ; case a real suite needs -- a runtime value already in RAM (an
    ; AccuracyCoin-style loop counter for id, an observed CRC/register for
    ; got) -- by round-tripping through smoke_id/smoke_got instead.
    lda #1
    sta smoke_id
    VT_TEST smoke_id, "putc_and_hex"
    VT_REG "x", #$7f
    VT_PASS smoke_id, "putc_and_hex"

    VT_TEST #2, "fail_line"
    lda #$15
    sta smoke_got
    VT_FAIL #2, "fail_line", #$2a, smoke_got

    VT_LOG "smoke ok"
    VT_END #1, #2
    rts
.endproc

.proc nmi
    rti
.endproc

.proc irq
    rti
.endproc

.segment "VECTORS"
    .addr nmi
    .addr reset
    .addr irq
