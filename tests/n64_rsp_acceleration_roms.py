#!/usr/bin/env python3
"""Grade bounded original RSP register ROMs and executed capture mutants."""
import argparse
import hashlib
import importlib
import json
import subprocess
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SUITES=('rsp_multiply_pairs','rsp_accumulate_edges','rsp_integer_pairs',
        'rsp_integer_edges','rsp_comparison_pairs','rsp_comparison_edges')

def child(args):
    source=ROOT/'tests/roms-src/n64'/args.suite
    sys.path.insert(0,str(source))
    original=importlib.import_module('run')
    args.artifacts.mkdir(parents=True,exist_ok=True)
    reports=[]
    for name,mutant in (('control',False),('executed-mutant',True)):
        rom=original.build(short=True,wrong_capture=mutant)
        image=args.artifacts/f'{args.suite}-{name}.z64'
        result=args.artifacts/f'{args.suite}-{name}.result'
        image.write_bytes(rom);original.run(args.emulator,image,result)
        report=original.evaluate(result,rom,True)
        (args.artifacts/f'{args.suite}-{name}.json').write_text(json.dumps(report,indent=2)+'\n')
        reports.append(report)
    good,bad=reports
    if good['status']!='pass' or bad['status']!='fail' or bad['mismatches']!=bad['expected_cases']:
        raise RuntimeError(json.dumps(reports))
    result=args.artifacts/f'{args.suite}-control.result'
    with result.open() as f:
        f.readline();f.readline();words=int(f.readline().split('words=')[1])
    return {'suite':args.suite,'cases':good['graded_cases'],'words':good['graded_cases']*words,
            'executed_mutant_rejected':True}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/n64_rsp_acceleration')
    parser.add_argument('--suite',choices=SUITES,help=argparse.SUPPRESS)
    args=parser.parse_args()
    if args.suite:
        print(json.dumps(child(args)));return 0
    args.artifacts.mkdir(parents=True,exist_ok=True)
    observations=[]
    for suite in SUITES:
        result=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--suite',suite,
            '--emulator',str(args.emulator.resolve()),'--artifacts',str(args.artifacts.resolve())],
            capture_output=True,text=True,timeout=300)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        observations.append(json.loads(result.stdout))
    sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
        for suite in SUITES for p in (ROOT/'tests/roms-src/n64'/suite).glob('*.py')}
    report={'status':'pass','suites':observations,'cases':sum(x['cases'] for x in observations),
            'actual_words':sum(x['words'] for x in observations),'source_sha256':sources,
            'complete_hardware_corpus':False}
    (args.artifacts/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"N64 RSP acceleration ROMs: PASS {report['cases']} cases / {report['actual_words']} actual register words; six executed mutants rejected")
    return 0

if __name__=='__main__':
    raise SystemExit(main())
