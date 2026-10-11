# Original N64 controller/netplay guest

MIT source, built without an SDK, firmware or commercial bytes. The guest issues
actual SI write/read DMA and reads four native PIF controller replies, SI status
and status after acknowledgement. It emits six actual register/RAM values per
scheduling quantum through `v64debug`, and retains them starting at RAM 0x2000.
It yields after each row so the host supplies the next input. It also runs real
AI PCM output to exercise the production network replay-audio gate.

The archived image has 16,384 rows. Short and deliberately wrong-capture variants
are generated separately. The production ABI probe feeds each of four ports a different odd permutation
of all 16,384 digital masks, including unused virtual bits on alternate rows.
All **16,384 rows / 98,304 actual captured words** match the independent oracle.
An executed wrong-capture guest fails all 16 short rows; six field mutations
and seven malformed streams are rejected. The component separately measures
65,536 isolated port/input combinations and state guards.

The production TCP gate launches two separate processes with independently
owned ports 0/1 and neutral ports 2/3. Its 100-row baseline and both peers match
all **600 actual big-endian guest words** and complete **8,478,744-byte S023
states** after different prior histories. Late input triggers rollback and AI
replay callbacks. The executed wrong-capture guest differs in exactly 100 words;
six field mutations, four malformed register streams, four fragmented invalid
wire packets, ROM mismatch and core/frame replacement checks are rejected.
This tests two network participants; four-participant networking is pending.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target n64_controller_probe n64_controller_ports_test n64_netplay_state_test netplay_plugin_peer -j
ctest --test-dir build -R '^n64_(controller_ports_test|controller_netplay_guest|netplay_state_test|netplay_plugin_test)$' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_controller_netplay/` and
`build/artifacts/netplay-n64/`. Network JSON reports state extent, oracle counts
and controls. Each peer records replay counts and actual guest readbacks in
`.result` files, and complete serialized state in `.state` files.

Scope: four virtual digital pads and zero stick coordinates. Analog sticks,
controller connection detection/accessories, PIF ROM/CIC execution and physical
SI timing remain unfinished. Scheduling quanta are not measured VI game frames.
No physical-console run is claimed.

N64 input word v1 reserves bits 14/15 together as the analog codec tag. The
legacy ignored-bit variation excludes these two reserved bits; every valid
fourteen-button mask still has a neutral stick. The separate analog corpus
covers all signed coordinate pairs. Production TCP now grades signed axes
alongside digital buttons; see the current N64 core README.
