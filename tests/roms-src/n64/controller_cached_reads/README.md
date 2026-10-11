# Original cached SI controller-read observations

Original MIT MIPS cartridge, assembler and independent host integer oracle. The
cartridge formats its padded four-controller read packet **once**, then repeatedly
issues PIF-to-RDRAM SI DMA. It yields between two reads so the frontend changes
port-zero input from each 14-button combination to its complement. No guest
computes expected buttons, compares results, or prints a verdict.

Full domain: **16,384 cases / 262,144 actual captured 64-bit words**. Every case
captures two complete sets of four controller packets, the untouched tail/control
word, SI_STATUS and MI_INTR, then both registers after acknowledgment. The 16
words are emitted as strict **7-word and 9-word chunks**, fitting the test sink's
255-byte line limit. The host supplies 32,768 real input transitions. Other ports
are neutral in this guest corpus; all four active digital ports are independently
covered by the component test. Analog, detached ports/accessories/EEPROM, CIC and
physical SI/Joybus timing remain incomplete.

The **16-case** control passes. A separately executed wrong-capture cartridge
must fail every case; all **16 individual field faults** must fail their changed
case; **seven malformed streams** must be errors. The grader requires identity,
exact domain/chunk grammar, completion, trailer and EOF. It also requires that
the production frontend actually consumed the input schedule.

```sh
python3 tests/roms-src/build.py n64/controller_cached_reads --original-only --check
python3 tests/roms-src/n64/controller_cached_reads/run.py
ctest --test-dir build -R '^n64_pif_(joybus_test|state_test|checkpoint_test|joybus_mutants)$' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_controller_cached_reads/` and
`build/artifacts/component/n64_pif_joybus/`. The production plugin checkpoint
restores a configured S029 layout and replays actual guest output/full state with
changing host/client input. S029 appends five validated channel-offset words to
S028, for **9,527,616 bytes**. Older states cannot contain the missing layout;
they import with channels disabled until the guest formats a new packet. Restart
older retail sessions if their guest only issues cached reads afterward.

The separation of formatting and read-time communication is corroborated by
[Mupen64Plus SI](https://github.com/mupen64plus/mupen64plus-core/blob/master/src/device/rcp/si/si_controller.c),
[ares PIF](https://github.com/ares-emulator/ares/blob/master/ares/n64/pif/hle.cpp)
and [libultra's cached read path](https://github.com/n64decomp/libreultra/blob/master/src/io/contreaddata.c).
Those implementations remain outside this repo; the original finite register
corpora establish the described implemented profile, not full hardware accuracy.
