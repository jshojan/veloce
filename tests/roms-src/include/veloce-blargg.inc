; veloce-blargg.inc (ca65) -- VELOCE-RESULT/1 reporting for test sources
; built on blargg's *modern* NES shell (instr_test-v5/source/common:
; shell.inc -> build_rom.s -> shell.s, text_out.s, testing.s, crc.s).
;
; A source adopts it with three edits (see tests/roms-src/nes/*/patches/):
;
;     CUSTOM_EXIT = 1                 ; BEFORE .include "shell.inc": this file
;                                     ; supplies the shell's `exit` label
;     VB_RESULT_ID = n                ; optional, see "result check" below
;     .define VB_RESULT_NAME "name"   ;   (both or neither)
;     .include "shell.inc"
;     .include "veloce-blargg.inc"    ; AFTER shell.inc (needs its macros)
;
;     main: VB_BEGIN "suite.test"     ; first thing main does -- before any
;                                     ; print, so the port claims the run
;                                     ; before blargg6000 sees $6004 text
;
; Emitters (each is one VELOCE-RESULT/1 line over $401E, via veloce-test.inc):
;
;   VB_BEGIN "suite"                  "VELOCE 1 nes <suite>"
;   VB_CRC id, "name", c1[,c2,c3,c4]  CHECK: the shell's running CRC-32 (the
;                                     same value print_crc prints) equals one
;                                     of the listed values; FAIL prints
;                                     exp=<c1> got=<crc> as 8 hex digits.
;                                     Resets the CRC afterwards, like
;                                     check_crc. Does not stop the test.
;   VB_FLAG id, "name", var           CHECK: byte `var` is non-zero
;                                     (FAIL: exp=01 got=00).
;   VB_REG  "name", var               REG <name>=<hex>, no verdict.
;
; Exit / END. Every way the shell ends a program goes through `exit` with the
; blargg result code in A (0 = passed; tests_passed, test_failed, check_crc_,
; internal_error, main returning). This file's `exit` emits the END line once,
; then continues into the shell's own std_exit, so the $6000 status byte and
; the on-screen text still report the same result as before:
;
;   * VB_RESULT_ID/VB_RESULT_NAME defined: the blargg code is itself a check
;     -- "CHECK <id> PASS <name>", or "CHECK <id> FAIL <name> exp=00 got=<code>
;     code=<code>". Used where upstream's own pass/fail logic (tests_passed /
;     test_failed) is the verdict.
;   * otherwise a non-zero code (a failure path the patch did not turn into a
;     VB_* check, e.g. internal_error) becomes "CHECK <n> FAIL exit exp=00
;     got=<code> code=<code>" with n = checks so far + 1, so it can never be
;     silently dropped; code 0 adds no check.
;
;   Then "END <pass>/<total>". A code-0 run whose VB_* checks failed exits
;   the shell with code 1, so $6000 never says "passed" for a run the result
;   file fails.
;
; Determinism: plain CPU code, no clocks, no uninitialised reads (the shell's
; clear_ram zeroes the vb_* bytes before main).

.ifndef VELOCE_BLARGG_INC
VELOCE_BLARGG_INC = 1

.ifndef CUSTOM_EXIT
    .error "veloce-blargg.inc: set CUSTOM_EXIT = 1 before including shell.inc"
.endif

; Everything below lives in the shell's library segment (CODE2), after the
; shell's own routines: the test's CODE bytes -- and so every cycle-counted
; loop, .align and page-crossing branch in it -- stay exactly where the
; unpatched source put them. Only the VB_* macro expansions a patch places in
; main add bytes to CODE. VT_PTR is set here so veloce-test.inc does not
; switch segments to reserve its own pointer.
.pushseg
.segment "CODE2"

zp_res  vb_ptr,2
VT_PTR = vb_ptr
.include "veloce-test.inc"

zp_byte vb_pass         ; CHECK lines that passed
zp_byte vb_total        ; CHECK lines emitted
zp_byte vb_id           ; id of the CHECK being emitted
zp_byte vb_code         ; blargg result code passed to exit
zp_byte vb_ended        ; END already written

; ---- helpers --------------------------------------------------------------

; Prints "CHECK <vb_id> " and counts the check.
vb_check_head:
    inc vb_total
    vt_str "CHECK "
    lda vb_id
    jsr vt_dec8
    lda #' '
    jmp vt_putc

; A = 0..255 -> 1-3 decimal digits, no leading zeros.
vb_dec_u8:
    ldx #0
@hundreds:
    cmp #100
    bcc @have_hundreds
    sbc #100
    inx
    jmp @hundreds
@have_hundreds:
    pha
    txa
    beq @below_100
    clc
    adc #'0'
    jsr vt_putc
    pla
    ldx #0
@tens:                          ; hundreds printed: tens digit is mandatory
    cmp #10
    bcc @have_tens
    sbc #10
    inx
    jmp @tens
@have_tens:
    pha
    txa
    clc
    adc #'0'
    jsr vt_putc
    pla
    clc
    adc #'0'
    jmp vt_putc
@below_100:
    pla
    jmp vt_dec8

; Prints the shell's running CRC-32 (checksum+3 .. checksum+0) as 8 hex
; digits: the same digits print_crc shows on screen.
vb_hex_crc:
    lda checksum+3
    jsr vt_hex8
    lda checksum+2
    jsr vt_hex8
    lda checksum+1
    jsr vt_hex8
    lda checksum
    jmp vt_hex8

; " exp=00 got=<vb_code hex> code=<vb_code dec>\n"
vb_code_tail:
    vt_str " exp=00 got="
    lda vb_code
    jsr vt_hex8
    vt_str " code="
    lda vb_code
    jsr vb_dec_u8
    jmp vt_newline

; ---- emitters -------------------------------------------------------------

.macro VB_BEGIN suite
    VT_BEGIN suite
.endmacro

.macro VB_REG name, var
    VT_REG name, var
.endmacro

.macro VB_CRC id, name, c1, c2, c3, c4
    .local ok, fin, exp1
    .pushseg
    .rodata
exp1: .dword c1
    .popseg
    is_crc c1
    jeq ok
    .ifnblank c2
        is_crc c2
        jeq ok
    .endif
    .ifnblank c3
        is_crc c3
        jeq ok
    .endif
    .ifnblank c4
        is_crc c4
        jeq ok
    .endif
    lda id
    sta vb_id
    jsr vb_check_head
    vt_str "FAIL "
    vt_str name
    vt_str " exp="
    lda exp1+3
    jsr vt_hex8
    lda exp1+2
    jsr vt_hex8
    lda exp1+1
    jsr vt_hex8
    lda exp1
    jsr vt_hex8
    vt_str " got="
    jsr vb_hex_crc
    jsr vt_newline
    jmp fin
ok:
    lda id
    sta vb_id
    jsr vb_check_head
    inc vb_pass
    vt_str "PASS "
    vt_line name
fin:
    jsr reset_crc
.endmacro

.macro VB_FLAG id, name, var
    .local ok, fin
    lda id
    sta vb_id
    jsr vb_check_head
    lda var
    bne ok
    vt_str "FAIL "
    vt_str name
    vt_line " exp=01 got=00"
    jmp fin
ok:
    inc vb_pass
    vt_str "PASS "
    vt_line name
fin:
.endmacro

; ---- exit hook (the shell's `exit`, see CUSTOM_EXIT in shell.s) -----------

exit:
    sta vb_code
    init_cpu_regs               ; sei, fresh stack: exit may come from deep
                                ; inside a test (test_failed, internal_error)
    lda vb_ended
    jne @to_shell
    inc vb_ended

.ifdef VB_RESULT_ID
    lda #VB_RESULT_ID
    sta vb_id
    jsr vb_check_head
    lda vb_code
    jne @result_fail
    inc vb_pass
    vt_str "PASS "
    vt_line VB_RESULT_NAME
    jmp @end_line
@result_fail:
    vt_str "FAIL "
    vt_str VB_RESULT_NAME
    jsr vb_code_tail
.else
    lda vb_code
    jeq @end_line
    lda vb_total
    clc
    adc #1
    sta vb_id
    jsr vb_check_head
    vt_str "FAIL exit"
    jsr vb_code_tail
.endif

@end_line:
    VT_END vb_pass, vb_total
    ; A code-0 exit whose VB_* checks did not all pass must not leave
    ; "passed" in the $6000 status byte either.
    lda vb_code
    bne @to_shell
    lda vb_pass
    cmp vb_total
    beq @to_shell
    lda #1
    sta vb_code

@to_shell:
    lda vb_code
    jmp std_exit

.popseg

.endif ; VELOCE_BLARGG_INC
