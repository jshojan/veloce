SECTION "Header", ROM0[$100]
    nop
    jp Start

SECTION "Main", ROM0[$150]
Start:
    call VeloceSmokeMain
.hang
    jr .hang

INCLUDE "../../include/gb/veloce-test-rgbds.inc"

VeloceSmokeMain:
    VT_BEGIN "veloce_smoke_rgbds"
    VT_TEST "1", "putc"
    VT_REG "a=1f p=30"
    VT_PASS "1", "putc"
    VT_TEST "2", "fail_demo"
    VT_FAIL "2", "fail_demo", "01a0", "00a0"
    VT_FAIL_MASK "3", "fail_demo_masked", "30", "31", "c3"
    VT_LOG "smoke transcript complete"
    VT_END_CODE "1", "3", "2"
    ret
