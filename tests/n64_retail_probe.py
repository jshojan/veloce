#!/usr/bin/env python3
"""Probe a user-supplied commercial ROM locally; never archives its bytes."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path)
    p.add_argument('--probe',type=Path,default=ROOT/'build/bin/retail_core_probe')
    p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/n64.so')
    p.add_argument('--trace',type=Path,default=ROOT/'build/bin/n64_boot_trace')
    p.add_argument('--output',type=Path,default=ROOT/'build/artifacts/retail/n64.json')
    args=p.parse_args()
    result=subprocess.run([str(args.probe.resolve()),str(args.core.resolve()),str(args.rom.resolve())],capture_output=True,text=True,timeout=90)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr or 'probe failed before ROM observation')
    report=json.loads(result.stdout)
    report.update(console='N64',rom_sha256=hashlib.sha256(args.rom.read_bytes()).hexdigest(),rom_size=args.rom.stat().st_size,
                  core_sha256=hashlib.sha256(args.core.read_bytes()).hexdigest(),status='execution-smoke' if report['loaded'] else 'blocked',
                  detail='Execution signals do not establish boot or playability.' if report['loaded'] else 'The image is malformed or its native boot variant is unsupported; retail boot remains unverified.')
    if report['loaded']:
        traced=subprocess.run([str(args.trace.resolve()),str(args.rom.resolve())],capture_output=True,text=True,timeout=90)
        if traced.returncode:raise RuntimeError(traced.stdout+traced.stderr or 'boot trace failed')
        report['boot_trace']=json.loads(traced.stdout)
        report['trace_sha256']=hashlib.sha256(args.trace.read_bytes()).hexdigest()
        if report['boot_trace'].get('exception_observed'):
            report['status']='boot-blocked'
            report['detail']='The cartridge loads; IPL3 reaches a CPU exception. Read the recorded EPC, Cause and fault address; retail boot/playability remain unverified.'
        elif report['boot_trace'].get('rsp_blocked'):
            report['status']='boot-blocked'
            report['detail']='The CPU trace reaches its bound; the RSP stopped on an unsupported operation. Read sp_pc and rsp_blocked_word; retail boot/playability remain unverified.'
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
