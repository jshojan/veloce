; header.s -- minimal LoROM SNES header + vector table for veloce_smoke.sfc.
; Checksum/complement are left as a fixed placeholder (0000/FFFF): the core
; does not validate them and a real checksum would have to be computed after
; layout is final, which nothing here depends on.

.import main

.segment "HEADER"
    .byte "VELOCE SMOKE TEST    "  ; 21-byte title, space-padded
    .byte $20                      ; maptype: LoROM, slow
    .byte $00                      ; romtype: ROM only
    .byte $08                      ; romsize (bucket only; ROM is 32KB)
    .byte $00                      ; sramsize: none
    .byte $01                      ; country: USA/NTSC
    .byte $00                      ; licensee (unused/homebrew)
    .byte $00                      ; version
    .word $FFFF                    ; checksum complement (placeholder)
    .word $0000                    ; checksum (placeholder)
    .res $FFFC - $FFE0, $00        ; pad from end of header body to $FFFC
    .word main                     ; $FFFC RESET vector (emulation-mode entry)
    .word main                     ; $FFFE IRQ vector (unused; defensive)
