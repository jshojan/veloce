#!/usr/bin/env python3
"""Render one explicit completion ledger into front-page and every core README."""
import argparse,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
START='<!-- completion:start -->'
END='<!-- completion:end -->'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
    data=json.loads((ROOT/'docs/core-completion.json').read_text())
    milestones={m['id']:m['acceptance'] for m in data['milestones']}
    assert data['schema']==1 and len(milestones)==20
    def percent(row):
        assert len(set(row['delivered']))==len(row['delivered']) and set(row['delivered'])<=milestones.keys()
        return 100*len(row['delivered'])//len(milestones)
    def table(rows):
        lines=['| Platform | Delivery checklist completion | Delivered / total |','|---|---:|---:|']
        for row in rows:lines.append(f"| {row['name']} | **{percent(row)}%** | {len(row['delivered'])}/20 |")
        return '\n'.join(lines)
    by_file={}
    for row in data['platforms']:by_file.setdefault(row['readme'],[]).append(row)
    legend='These percentages count the 20 explicit delivery milestones in [the completion ledger]({link}); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.'
    blocks={'README.md':'## Core completion percentages\n\n'+table(data['platforms'])+'\n\n'+legend.format(link='docs/CORE-COMPLETION.md')+'\n\nSuper Mario 64 (USA) was tested locally on 2026-10-11. The six-billion-attempt intro reaches the courtyard. A shared private checkpoint now verifies **analog walking, A jumping and B punching through actual RAM fields**, with stationary neutral controls and no terminal CPU/RSP/DP stop during those continuations. **180 legacy quantum movie frames and six seek boundaries match complete native states**; an executed neutral movie leaves Mario standing. New native sessions advance on **VI field boundaries**, with an original **5,160-case file oracle** and **121 VI movie frames / six exact seeks**; legacy states retain the old budget. Castle entry, menu/level progression, practical normal-speed play, full-title playability and native firmware boot remain unverified. **N64 now reaches 50%** of the fixed delivery checklist. Its original signed-stick corpus grades 65,536 cases / 1,048,576 RAM/register words; the original one-cycle rectangle corpus grades 1,048,576 cases / 13,631,488 words. All twelve integer trap instructions are now verified by an original **15,728,640-case ROM / 188,743,680 RAM/CP0 words**. Linked word/doubleword loads and conditional stores now have an original **1,048,576-case ROM / 10,485,760 RAM words**, including ERET failures and a live-link checkpoint. Disabled COP2 exception priority now has an original **9,056,256-case ROM / 108,675,072 RAM/CP0 words**, plus an exhaustive encoded-payload component gate. Four local controller ports now pass **194 ordinary frontend cases / 1,164 guest RAM words**, including all 24 device assignments and an executed wrong-route rejection. Independent extra-player profiles pass **3,026 remap/persistence cases / 18,156 guest RAM words** and remapped four-port movies. CP0 physical Watch traps now have an original **4,194,816-case ROM / 58,727,424 RAM/CP0 words**, with armed-checkpoint replay and R/W/EXL controls.\n\nTwenty-eight doubleword integer/LWU families now enforce User/Supervisor mode restrictions: **5,768,792 original guest cases / 69,225,504 RAM/CP0 words**, with executed NOP rejection and native checkpoint/rollback. Nine wide integer memory families now verify Kernel32 address formation: **4,718,592 original guest cases / 56,623,104 RAM/CP0 words**, with extended-address/alignment controls and native rollback. CP0 BadVAddr/PRId/CacheErr write protection now has an original **12,582,912-case cartridge / 150,994,944 actual RAM/register words**, with hardware exception seeding, executed wrong-read rejection and native rollback. Config fixed/writable fields and actual K0 cache/bypass reads now have an original **16,777,216-case cartridge / 201,326,592 actual RAM/register words**, with EP/BE defaults held and clock-field preservation controls. This remains partial CPU coverage; N64 stays 50%. Physical controller calibration/peripherals, two-cycle graphics/LOD, enabled expansion-coprocessor behavior, precise atomic/cache interference and full CPU/hardware/audio/timing remain unfinished. See [checkpoint control and movie evidence](cores/n64/README.md#private-courtyard-controls-and-movie-checkpoints).'}
    for path,rows in by_file.items():
        blocks[path]='## Completion percentage\n\n'+table(rows)+'\n\n'+legend.format(link='../../docs/CORE-COMPLETION.md')
        for row in rows:
            blocks[path]+='\n\n'+row['name']+' delivered milestones: '+', '.join(milestones[k] for k in row['delivered'])+'.' if row['delivered'] else '\n\nNo delivery milestones have been completed.'
            blocks[path]+='\n\nRemaining: '+row['remaining']
    blocks['docs/CORE-COMPLETION.md']='## Current completion\n\n'+table(data['platforms'])+'\n\n'+data['method']+'\n\n### Fixed acceptance checklist\n\n| ID | Acceptance |\n|---|---|\n'+'\n'.join('| '+key+' | '+value+' |' for key,value in milestones.items())+'\n\nNo native hardware subsystem is declared exhaustive from a bounded original ROM. The ledger credits the delivered register harness separately. Native analog peripherals, complete CPUs, hardware timing, full corpora and playable retail titles stay pending until evidence meets their full scope. A missing optional peripheral is not required on a platform that never had it; that platform must still fulfill the other native controller or CPU behavior in the same milestone.\n\nThe scores are conservative delivery tracking, with equal milestone weights rather than estimates of work hours. A high test pass rate can coexist with a low delivery percentage. An uncredited milestone can have substantial partial implementation. Detailed passing domains and remaining hardware are in [HARDWARE-COVERAGE.md](HARDWARE-COVERAGE.md) and [COMPLETENESS.md](../COMPLETENESS.md).\n\nEvidence entry points: each core README links its code, original-ROM sources, component gates and result artifacts. The scored `netplay` milestone requires a production two-process network gate, not just an interface declaration. `saves` requires the native media scope, so partial persistence is uncredited. Update [core-completion.json](core-completion.json) when new evidence completes a milestone; run `python3 tests/update_completion.py`, then `python3 tests/update_completion.py --check`. CI/CD remains disabled.'
    target=ROOT/'docs/CORE-COMPLETION.md'
    if not target.exists() and not args.check:target.write_text('# Core delivery completion ledger\n')
    bad=[]
    for path,block in blocks.items():
        target=ROOT/path;text=target.read_text();replacement=START+'\n'+block+'\n'+END
        if START in text:
            assert text.count(START)==text.count(END)==1
            begin=text.index(START);end=text.index(END)+len(END);updated=text[:begin]+replacement+text[end:]
        else:
            # Put core percentages immediately after the title; front page below introduction.
            position=text.index('\n')+1;updated=text[:position]+'\n'+replacement+'\n'+text[position:]
        if updated!=text:
            if args.check:bad.append(path)
            else:target.write_text(updated)
    if bad:raise SystemExit('Completion docs stale: '+', '.join(bad))
    print('Completion ledger and 12 core profiles plus Switch documentation synchronized')
if __name__=='__main__':main()
