; veloce_smoke.gba (FASMARM) -- GBA-2
;
; Exercises every macro in tests/roms-src/include/fasmarm/veloce.inc (probe,
; BEGIN, TEST, PASS, FAIL with runtime hex, REG, LOG, END) with fixed,
; compile-time-known values, so the transcript it writes is exactly the
; same on every run. tests/veloce_testkit's determinism selftest runs this
; ROM twice under VELOCE_TEST_OUT and byte-diffs the two result files
; (docs/testing/VELOCE-RESULT.md's determinism gate, plan section 3).
;
; This is Veloce's own ROM, not an upstream test: it is not "passing" or
; "failing" anything real, so it deliberately reports 1/2 (one PASS, one
; FAIL) to prove the FAIL/exp/got path renders correctly too, matching
; docs/testing/transcripts/port_fail_partial.result's shape.
format binary as 'gba'
org 0x08000000

include '../../../include/fasmarm/veloce.inc'

header:
        include 'header.asm'

main:
        m_vt_open                       ; probe; result unused (see veloce.inc)

        m_vt_begin 'gba','veloce_smoke'

        m_vt_test '1','smoke_pass'
        m_vt_pass '1','smoke_pass'

        m_vt_test '2','smoke_fail'
        mov     r0, 0xAB
        lsl     r0, 24                  ; r0 = 0xAB000000 (exp)
        mov     r1, 0x12
        lsl     r1, 24                  ; r1 = 0x12000000 (got)
        m_vt_fail '2','smoke_fail',r0,r1

        m_vt_reg 'r0', r0
        m_vt_log 'fixed transcript for the determinism selftest'
        m_vt_end '1','2'

idle:
        b       idle

include '../../../include/fasmarm/veloce_code.inc'
