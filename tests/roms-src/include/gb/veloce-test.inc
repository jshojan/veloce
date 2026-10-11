;=============================================================================
; tests/roms-src/include/gb/veloce-test.inc
;
; VELOCE-RESULT/1 emitter for Game Boy test ROMs assembled with wla-gb
; (WLA-DX). See docs/testing/VELOCE-RESULT.md sections 5-6: the GB channel
; is the real SB/SC serial link (SB=$FF01, SC=$FF02), unchanged from
; hardware; VELOCE_TEST_OUT captures every byte the app's TestFileSink
; forwards from cores/gb/src/test_channel.cpp.
;
; .INCLUDE this file once, after SB/SC are defined by the caller's own
; hardware.inc/gbhw.inc, or rely on the fallback .define below.
;
; id / name / exp / got / mask / pass / total / code are ASCII TEXT the
; caller has already formatted (e.g. VT_TEST "1", "adc_bin8_imm"): this
; emitter never does runtime decimal/hex conversion, the same convention
; every other console's shell already uses to build its result text
; (blargg's text_out, Mooneye's magic-breakpoint dump, ...).
;=============================================================================

.IFNDEF SB
.DEFINE SB $FF01
.ENDIF
.IFNDEF SC
.DEFINE SC $FF02
.ENDIF

; one byte (A) out over the real serial port as internal-clock master.
; Veloce's serial adapter (cores/gb/src/test_channel.cpp) accepts the byte
; immediately and clears SC.7 the same frame it was set, so this never
; actually spins under Veloce; on real hardware (no listener) it would spin
; forever, which is fine -- this include is Veloce-only.
vt_putc:
    ldh (<SB), a
    ld a, $81
    ldh (<SC), a
  - ldh a, (<SC)
    bit 7, a
    jr nz, -
    ret

; HL -> null-terminated string, out over serial. Clobbers A, HL.
vt_puts:
  - ld a, (hl)
    inc hl
    and a
    ret z
    call vt_putc
    jr -

.MACRO VT_BEGIN ARGS suite
    ld hl, _vtb_\@
    call vt_puts
    jp _vta_\@
_vtb_\@:
    .db "VELOCE 1 gb ", \1, 10, 0
_vta_\@:
.ENDM

.MACRO VT_TEST ARGS id, name
    ld hl, _vtt_\@
    call vt_puts
    jp _vtta_\@
_vtt_\@:
    .db "TEST ", \1, " ", \2, 10, 0
_vtta_\@:
.ENDM

.MACRO VT_PASS ARGS id, name
    ld hl, _vtp_\@
    call vt_puts
    jp _vtpa_\@
_vtp_\@:
    .db "CHECK ", \1, " PASS ", \2, 10, 0
_vtpa_\@:
.ENDM

.MACRO VT_FAIL ARGS id, name, exp, got
    ld hl, _vtf_\@
    call vt_puts
    jp _vtfa_\@
_vtf_\@:
    .db "CHECK ", \1, " FAIL ", \2, " exp=", \3, " got=", \4, 10, 0
_vtfa_\@:
.ENDM

.MACRO VT_FAIL_MASK ARGS id, name, exp, got, mask
    ld hl, _vtfm_\@
    call vt_puts
    jp _vtfma_\@
_vtfm_\@:
    .db "CHECK ", \1, " FAIL ", \2, " exp=", \3, " got=", \4, " mask=", \5, 10, 0
_vtfma_\@:
.ENDM

.MACRO VT_REG ARGS text
    ld hl, _vtr_\@
    call vt_puts
    jp _vtra_\@
_vtr_\@:
    .db "REG ", \1, 10, 0
_vtra_\@:
.ENDM

.MACRO VT_LOG ARGS text
    ld hl, _vtl_\@
    call vt_puts
    jp _vtla_\@
_vtl_\@:
    .db "LOG ", \1, 10, 0
_vtla_\@:
.ENDM

.MACRO VT_END ARGS pass, total
    ld hl, _vte_\@
    call vt_puts
    jp _vtea_\@
_vte_\@:
    .db "END ", \1, "/", \2, 10, 0
_vtea_\@:
.ENDM

.MACRO VT_END_CODE ARGS pass, total, code
    ld hl, _vtec_\@
    call vt_puts
    jp _vteca_\@
_vtec_\@:
    .db "END ", \1, "/", \2, " code=", \3, 10, 0
_vteca_\@:
.ENDM
