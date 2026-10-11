// tests/roms-src/include/devkitarm/veloce_test.h  (GBA-2)
//
// VELOCE-RESULT/1 emitter for devkitARM C ROMs (docs/testing/VELOCE-RESULT.md,
// sections 5-7), built on the vendored mgba_printf() (vendor/mgba.c, MIT --
// see vendor/PROVENANCE.md). Every vt_* call is one mgba_printf() call, so
// it is one line, one flush, matching gba::TestChannel::debug_flush()
// (cores/gba/src/test_channel.cpp) exactly: mgba_printf() writes the
// formatted text into REG_DEBUG_STRING (0x4FFF600) and flushes it with one
// write to REG_DEBUG_FLAGS (0x4FFF700). Inert on real hardware, same as the
// FASMARM side (tests/roms-src/include/fasmarm/veloce.inc).
//
// Unlike the FASMARM macros, ids/names here can be any printf-able value
// (the testkit's detector counts CHECK lines for pass/total; it does not
// parse `id` numerically) and exp/got/reg values are formatted with the
// libc `%08x` you'd expect, no hand-rolled hex conversion needed.
//
// Known gap: no vt_fail_mask() for the shared spec's optional
// `mask=<hex>` field yet (no GBA suite needs a partial-register comparison
// today) -- add one following vt_fail()'s shape when one does, rather than
// work around its absence.
//
// Usage:
//   #include "veloce_test.h"
//   ...
//   vt_begin("gba", "veloce_smoke");
//   vt_test("1", "smoke_pass");
//   vt_pass("1", "smoke_pass");
//   vt_fail("2", "smoke_fail", 0xab000000u, 0x12000000u);
//   vt_reg("r0", value);
//   vt_log("free text");
//   vt_end(1, 2);
#ifndef VELOCE_TEST_H
#define VELOCE_TEST_H

#include "vendor/mgba.h"

static inline void vt_begin(const char* console, const char* suite) {
    mgba_open();  // optional handshake probe; mgba_printf() below works either way
    mgba_printf(MGBA_LOG_INFO, "VELOCE 1 %s %s", console, suite);
}

static inline void vt_test(const char* id, const char* name) {
    mgba_printf(MGBA_LOG_INFO, "TEST %s %s", id, name);
}

static inline void vt_pass(const char* id, const char* name) {
    mgba_printf(MGBA_LOG_INFO, "CHECK %s PASS %s", id, name);
}

static inline void vt_fail(const char* id, const char* name,
                            unsigned int exp, unsigned int got) {
    mgba_printf(MGBA_LOG_INFO, "CHECK %s FAIL %s exp=%08x got=%08x", id, name, exp, got);
}

static inline void vt_reg(const char* name, unsigned int value) {
    mgba_printf(MGBA_LOG_INFO, "REG %s=%08x", name, value);
}

static inline void vt_log(const char* text) {
    mgba_printf(MGBA_LOG_INFO, "LOG %s", text);
}

// pass/total are parsed numerically by the detector (unlike every other
// vt_* id/name argument above), so these really are the counts, not labels.
static inline void vt_end(int pass, int total) {
    mgba_printf(MGBA_LOG_INFO, "END %d/%d", pass, total);
}

#endif // VELOCE_TEST_H
