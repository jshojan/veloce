# Original CGB KEY1/STOP cartridge

`build.py` emits the checked-in 32 KiB CGB cartridge from original LR35902
machine code. The cartridge reads KEY1 ($FF4D) before and after arming STOP,
switches to double speed and back, and records all 256 possible writes and
readbacks at each speed. The 512 domain readbacks and five transition readbacks
are written through the guest serial port into `VELOCE_TEST_OUT`.

`run.py` checks every observed byte against an independent KEY1 bit oracle,
requires the guest's passing check, and runs an altered cartridge that leaves
the first switch disarmed. The altered cartridge must report the actual
unchanged KEY1 speed bit and fail its guest check.

Run with `ctest --test-dir build -R '^gb_cgb_speed_switch$' --output-on-failure`.
The result files and JSON summary are under
`build/artifacts/owned/gb_cgb_speed_switch/`.

The [CGB KEY1 specification](https://gbdev.io/pandocs/CGB_Registers.html)
describes the writable prepare bit and STOP transition. The
[rendering timing specification](https://gbdev.io/pandocs/Rendering.html)
describes the two LCD dots per CPU M-cycle in double speed. This cartridge
grades KEY1 readback and switching; it does not directly grade LCD phase,
the transient bus-access restrictions during the speed switch, or all CGB
peripherals. Those remain separate accuracy work.
