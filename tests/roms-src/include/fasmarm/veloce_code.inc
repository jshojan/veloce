; tests/roms-src/include/fasmarm/veloce_code.inc  (GBA-2)
;
; Real code for veloce.inc's m_vt_* macros (vt_puts/vt_puthex32/vt_flush).
; `include` this exactly once, anywhere AFTER the GBA header -- unlike
; veloce.inc's macro definitions, this emits actual bytes at the point it
; is included, so it must not be the first thing in the file. The
; conventional place is right after the ROM's idle loop, exactly where
; lib/text.asm is included in jsmolka/alyosha's own ROMs. `bl` has a
; +-32MB range, so where exactly this lands doesn't otherwise matter.

code32
align 4

; r0: nul-terminated string pointer (in ROM). r9: cursor into
; REG_DEBUG_STRING, updated in place. Clobbers r0-r2.
vt_puts:
        stmfd   sp!, {r1-r2, lr}
        m_vt_load32 r2, VT_DEBUG_STRING
.loop:
        ldrb    r1, [r0], 1
        cmp     r1, 0
        beq     .done
        strb    r1, [r2, r9]
        add     r9, 1
        b       .loop
.done:
        ldmfd   sp!, {r1-r2, pc}

; r0: 32-bit value. r9: cursor, updated in place. Appends 8 lowercase hex
; digits, most significant nibble first. Clobbers r0-r3.
vt_puthex32:
        stmfd   sp!, {r0-r3, lr}
        m_vt_load32 r2, VT_DEBUG_STRING
        mov     r3, 28                  ; shift of the current (MS-first) nibble
.loop:
        mov     r1, r0
        lsr     r1, r3
        and     r1, 0xF
        cmp     r1, 10
        addlt   r1, '0'
        addge   r1, 'a' - 10
        strb    r1, [r2, r9]
        add     r9, 1
        subs    r3, 4
        bge     .loop
        ldmfd   sp!, {r0-r3, pc}

; r9: line length (the cursor built by vt_puts/vt_puthex32 calls before this).
; r0: mGBA level (0-4, FATAL..DEBUG). Flushes REG_DEBUG_STRING[0..r9) as one
; line. Every m_vt_* macro starts its own line at cursor 0, so nothing here
; needs to reset it. Clobbers r0-r2.
vt_flush:
        stmfd   sp!, {r1-r2, lr}
        m_vt_load32 r1, VT_DEBUG_FLAGS
        orr     r0, 0x100
        strh    r0, [r1]
        ldmfd   sp!, {r1-r2, pc}
