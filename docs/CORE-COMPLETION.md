# Core delivery completion ledger

<!-- completion:start -->
## Current completion

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| NES/Famicom | **50%** | 10/20 |
| SNES | **40%** | 8/20 |
| Game Boy / Game Boy Color | **40%** | 8/20 |
| Game Boy Advance | **40%** | 8/20 |
| Nintendo 64 | **30%** | 6/20 |
| Virtual Boy | **20%** | 4/20 |
| Nintendo DS | **30%** | 6/20 |
| Nintendo DSi | **30%** | 6/20 |
| Nintendo 3DS | **30%** | 6/20 |
| GameCube | **25%** | 5/20 |
| Wii | **25%** | 5/20 |
| Wii U | **25%** | 5/20 |
| Nintendo Switch | **0%** | 0/20 |

Equal-weight delivery milestones: 100 * delivered / 20. Partial or unverified milestones count zero. These are delivery checklist percentages, not hardware accuracy, exhaustive domain coverage or retail compatibility.

### Fixed acceptance checklist

| ID | Acceptance |
|---|---|
| plugin | Separate core plugin and ABI entry points |
| native_media | Parse a native cartridge/executable format (boot scored separately) |
| cpu | Complete CPU instruction families, including architectural extensions |
| memory | Complete native RAM/ROM map and ownership rules |
| exceptions | Complete exceptions, privilege and address translation |
| devices | Complete peripheral registers, interrupt producers and DMA |
| boot | Native firmware/retail boot procedure |
| retail | At least one locally tested retail title verified playable |
| graphics | Guest graphics produce rendered video |
| audio | Guest sound produces audio samples |
| timing | Hardware-referenced CPU/device/display/audio timing |
| digital | Native digital controller/keypad registers consume frontend input |
| controller | Complete native controller/touch/motion/analog peripheral behavior |
| multiplayer | Native local multiplayer/link input routing |
| netplay | Actual production-plugin TCP peers pass original guest oracle and whole-state comparison |
| saves | Complete native persistent media types and persistence |
| states | Complete states for the implemented profile and deterministic replay |
| original_rom | Repository-owned ROM source and reproducible archived image |
| register_oracle | Actual guest register-file output independently graded on the host, with negative controls |
| full_corpus | Original corpus covers the full native hardware, with physical reference validation |

No native hardware subsystem is declared exhaustive from a bounded original ROM. The ledger credits the delivered register harness separately. Native analog peripherals, complete CPUs, hardware timing, full corpora and playable retail titles stay pending until evidence meets their full scope. A missing optional peripheral is not required on a platform that never had it; that platform must still fulfill the other native controller or CPU behavior in the same milestone.

The scores are conservative delivery tracking, with equal milestone weights rather than estimates of work hours. A high test pass rate can coexist with a low delivery percentage. An uncredited milestone can have substantial partial implementation. Detailed passing domains and remaining hardware are in [HARDWARE-COVERAGE.md](HARDWARE-COVERAGE.md) and [COMPLETENESS.md](../COMPLETENESS.md).

Evidence entry points: each core README links its code, original-ROM sources, component gates and result artifacts. The scored `netplay` milestone requires a production two-process network gate, not just an interface declaration. `saves` requires the native media scope, so partial persistence is uncredited. Update [core-completion.json](core-completion.json) when new evidence completes a milestone; run `python3 tests/update_completion.py`, then `python3 tests/update_completion.py --check`. CI/CD remains disabled.
<!-- completion:end -->
