// veloce_smoke.gba (devkitARM C) -- GBA-2
//
// Exercises every function in tests/roms-src/include/devkitarm/veloce_test.h
// (vt_begin/vt_test/vt_pass/vt_fail/vt_reg/vt_log/vt_end) with fixed,
// compile-time-known values, so the transcript it writes is exactly the
// same on every run. tests/veloce_testkit's determinism selftest runs this
// ROM twice under VELOCE_TEST_OUT and byte-diffs the two result files
// (docs/testing/VELOCE-RESULT.md's determinism gate, plan section 3).
//
// This is Veloce's own ROM, not an upstream test: it deliberately reports
// 1/2 (one PASS, one FAIL) to prove the FAIL/exp/got path renders
// correctly too, matching the FASMARM smoke ROM's shape
// (tests/roms-src/gba/veloce_smoke_fasmarm/) and
// docs/testing/transcripts/port_fail_partial.result.
#include "veloce_test.h"

int main(void) {
    vt_begin("gba", "veloce_smoke");

    vt_test("1", "smoke_pass");
    vt_pass("1", "smoke_pass");

    vt_test("2", "smoke_fail");
    vt_fail("2", "smoke_fail", 0xab000000u, 0x12000000u);

    vt_reg("r0", 0xab000000u);
    vt_log("fixed transcript for the determinism selftest");
    vt_end(1, 2);

    while (1) {
        // idle
    }
    return 0;
}
