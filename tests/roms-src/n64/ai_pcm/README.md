# Original N64 PCM playback and register captures

The MIT licensed assembler emits original guest MIPS code and a stereo sample
ramp containing every 16-bit signed code. It uses no SDK, firmware or commercial
bytes. The guest copies its samples into uncached RDRAM, programs two AI buffers,
waits for playback and emits nine actual FIFO/MI register values. No expected
results or pass verdicts are present. Buffers avoid 8 KiB end boundaries; delayed
carry has separate component evidence.

The guest register oracle matches all **nine observations**. An executed length
mutation, nine field mutations and seven malformed streams reject. The additional
production ABI harness writes observed `get_audio()` floats back to 16-bit
`REG r=<left><right>` records in a file. It observes **66,169 stereo pairs**,
covering **all 65,536 signed sample values per channel** in order. The channels
use complementary patterns so swaps and corruption fail. The independent Python
oracle checks every channel value, every distinct code, ordered progression,
limited repetition, bounded resampling count, identity and termination. It does
not certify an exact physical phase relationship between CPU and DAC clocks.
An executed sample mutation fails every observed pair; a channel mutation and
five malformed streams reject. Source/image hashes are in the manifest.

Guest files: `build/artifacts/owned/n64_ai_pcm/`.
ABI PCM register files: `build/artifacts/owned/n64_ai_pcm_audio/`.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_audio_capture n64_ai_test -j
ctest --test-dir build -R '^n64_(ai_pcm|ai_pcm_audio|ai_test)$' --output-on-failure
```

The component checks all signed input codes directly at the AI output, every
length/address/rate field, all dividers, FIFO/IRQ/carry, playback-time RAM/cache
visibility, pause, malformed states, bounded host output, and identical buffered/
streaming plugin samples and states. The implementation currently uses an NTSC
crystal and an approximate interpreter clock relationship, with deterministic
sample holding to 48 kHz. RSP mixing/decompression, physical FIFO/IRQ timing,
serial/analog behavior, PAL/MPAL and retail playability remain incomplete. This
original cartridge has not been measured on physical hardware. See the
[core profile](../../../../cores/n64/README.md#ai-playback-and-plugin-audio).
