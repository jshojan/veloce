;=============================================================================
; tests/roms-src/include/gb/veloce-test-rgbds.inc
;
; VELOCE-RESULT/1 emitter for Game Boy test ROMs assembled with RGBDS.
; See docs/testing/VELOCE-RESULT.md sections 5-6 (same GB serial channel,
; same emitter vocabulary as tests/roms-src/include/gb/veloce-test.inc, the
; wla-gb dialect of this file).
;
; INCLUDE this file once, inside whatever SECTION the caller already has
; open (it defines no SECTION of its own so it can be dropped into an
; existing suite's shell, e.g. SameSuite's include/base.inc).
;
; id / name / exp / got / mask / pass / total / code are ASCII TEXT the
; caller has already formatted -- see the wla-gb include's header comment
; for the rationale (no runtime decimal/hex conversion in the emitter).
;=============================================================================

IF !DEF(rSB)
rSB EQU $FF01
ENDC
IF !DEF(rSC)
rSC EQU $FF02
ENDC

; one byte (A) out over the real serial port as internal-clock master.
; Veloce's serial adapter (cores/gb/src/test_channel.cpp) accepts the byte
; immediately and clears SC.7 the same frame it was set, so this never
; actually spins under Veloce.
vt_putc::
    ldh [rSB], a
    ld a, $81
    ldh [rSC], a
.spin
    ldh a, [rSC]
    bit 7, a
    jr nz, .spin
    ret

; HL -> null-terminated string, out over serial. Clobbers A, HL.
vt_puts::
.loop
    ld a, [hl]
    inc hl
    and a
    ret z
    call vt_putc
    jr .loop

MACRO VT_BEGIN
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "VELOCE 1 gb ", \1, 10, 0
.after\@
ENDM

MACRO VT_TEST
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "TEST ", \1, " ", \2, 10, 0
.after\@
ENDM

MACRO VT_PASS
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "CHECK ", \1, " PASS ", \2, 10, 0
.after\@
ENDM

MACRO VT_FAIL
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "CHECK ", \1, " FAIL ", \2, " exp=", \3, " got=", \4, 10, 0
.after\@
ENDM

MACRO VT_FAIL_MASK
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "CHECK ", \1, " FAIL ", \2, " exp=", \3, " got=", \4, " mask=", \5, 10, 0
.after\@
ENDM

MACRO VT_REG
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "REG ", \1, 10, 0
.after\@
ENDM

MACRO VT_LOG
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "LOG ", \1, 10, 0
.after\@
ENDM

MACRO VT_END
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "END ", \1, "/", \2, 10, 0
.after\@
ENDM

MACRO VT_END_CODE
    ld hl, .data\@
    call vt_puts
    jp .after\@
.data\@
    db "END ", \1, "/", \2, " code=", \3, 10, 0
.after\@
ENDM
