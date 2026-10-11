# Local N64 retail input probes

These original text schedules drive a user-supplied ROM through the production
plugin and a separate CPU/device trace. Names describe the investigation target;
none of the scripts awards a boot or playability verdict. Commercial ROMs,
framebuffers, trace data and screenshots remain local under `build/artifacts/`.

| Script | Purpose |
|---|---|
| `n64-title-start.txt` | Title Start pulses |
| `n64-title-menu.txt` | Later Start/A pulses; observed Peach intro |
| `n64-gameplay-start.txt` | Extend Start/A pulses through quantum 26,100; three-billion-attempt diagnostic |
| `n64-gameplay-extended.txt` | Later A pulses through quantum 55,100; six-billion-attempt diagnostic |

Each noncomment row is a zero-based decimal scheduling quantum and hexadecimal
port-zero virtual digital mask. The mask remains held until the next transition.
Rows must be strictly increasing and less than 100,000. This is the frontend
virtual-button ABI, not the native N64 wire mask. Analog input is still absent.

```sh
cmake --build build --target retail_core_probe n64_boot_trace n64_plugin
python3 tests/n64_retail_probe.py /path/to/private/sm64.z64 \
  --frames 60000 --input-script tests/input/n64-gameplay-extended.txt \
  --capture build/artifacts/retail/n64-gameplay-extended.ppm \
  --output build/artifacts/retail/n64-gameplay-extended.json
```

`--frames` accepts 1–100,000 scheduling quanta; each corresponds to 100,000
nominal CPU attempts in the separate trace. They are not physical game frames
or a wall-clock duration. The default is 120. Each subprocess has a 900-second
timeout; a timeout or error does not establish a guest hardware blocker.

With `--capture`, the probe saves the last valid framebuffer, including a
uniform framebuffer, and reports its **one-based** completed `capture_quantum`.
For runs longer than 10,000 quanta it also writes each 10,000-quantum checkpoint
to `<capture>.q<quantum>.ppm`. The JSON lists checkpoint quanta, paths and SHA256,
as well as final-capture, ROM, binary and input-script fingerprints. A nonuniform
frame observed earlier remains a separate smoke signal; it no longer determines
which frame is saved. A checkpoint cannot be claimed from a failed framebuffer
read. All captures stay private.

The helper's original-ROM regression uses the uniform framebuffer from
`integer_registers.z64` over 10,001 calls: capture succeeds at quantum 10,001,
checkpoint 10,000 is present, and both PPMs match exactly. This checks capture
semantics, not hardware video accuracy. The normal original register-output ROM
oracles remain separate from these retail diagnostics.
