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
    blocks={'README.md':'## Core completion percentages\n\n'+table(data['platforms'])+'\n\n'+legend.format(link='docs/CORE-COMPLETION.md')+'\n\nSuper Mario 64 (USA) was tested locally on 2026-10-10: **loads and passes SET_Z_IMAGE and actual color/depth-buffer clears**. The current private trace reaches 200 million CPU attempts without a terminal fault. RDP TMEM block/tile/palette transfers pass, and the reader now retains the complete **22/22-word triangle packet** at **0x227160**, advancing fetched CURRENT to **0x227210**. Fill-cycle and functional one-cycle RGBA32 shade/primitive/environment triangles now have independent framebuffer oracles. Mario’s textured/depth-tested triangle profile remains blocked; video stays uniform and boot/playability are unverified. See the [current packet evidence and repeatable private probe](cores/n64/README.md#one-cycle-rgba32-triangle-pixels).'}
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
