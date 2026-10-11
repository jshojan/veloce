#!/usr/bin/env python3
"""Probe a user-supplied commercial ROM locally; never archives its bytes."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path)
    p.add_argument('--input-script',type=Path,help='Sorted absolute decimal quantum / hexadecimal port-zero core input transitions.')
    p.add_argument('--load-state',type=Path,help='Private native checkpoint to continue; script quanta remain absolute.')
    p.add_argument('--save-state',type=Path,help='Write the final native checkpoint locally, never committed.')
    p.add_argument('--capture-every',type=int,default=10000,help='Capture interval in absolute quanta (1..100000).')
    p.add_argument('--capture',type=Path,help='Local PPM framebuffer capture; never committed.')
    p.add_argument('--frames',type=int,default=120,help='Frontend scheduling quanta (1..100000); trace uses 100000 nominal CPU attempts per quantum.')
    p.add_argument('--probe',type=Path,default=ROOT/'build/bin/retail_core_probe')
    p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/n64.so')
    p.add_argument('--trace',type=Path,default=ROOT/'build/bin/n64_boot_trace')
    p.add_argument('--output',type=Path,default=ROOT/'build/artifacts/retail/n64.json')
    args=p.parse_args()
    if not 1<=args.frames<=100000:p.error('--frames must be 1..100000')
    if not 1<=args.capture_every<=100000:p.error('--capture-every must be 1..100000')
    if args.load_state and args.save_state and args.load_state.resolve()==args.save_state.resolve():p.error('Use separate input and output checkpoints')
    inputs={'rom':args.rom,'core':args.core,'probe':args.probe,'trace':args.trace}
    if args.input_script:inputs['input_script']=args.input_script
    if args.load_state:inputs['load_state']=args.load_state
    if args.save_state and args.save_state.resolve() in {path.resolve() for path in inputs.values()}:p.error('The output checkpoint cannot replace an input file or executable')
    fingerprints={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in inputs.items()}
    command=[str(args.probe.resolve()),str(args.core.resolve()),str(args.rom.resolve()),str(args.frames)]
    if args.capture:
        args.capture.parent.mkdir(parents=True,exist_ok=True)
        command.append(str(args.capture.resolve()))
    else:command.append('-')
    command.append(str(args.input_script.resolve()) if args.input_script else '-')
    command+=['--capture-every',str(args.capture_every)]
    if args.load_state:command+=['--load-state',str(args.load_state.resolve())]
    if args.save_state:
        args.save_state.parent.mkdir(parents=True,exist_ok=True)
        command+=['--save-state',str(args.save_state.resolve())]
    result=subprocess.run(command,capture_output=True,text=True,timeout=900)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr or 'probe failed before ROM observation')
    report=json.loads(result.stdout)
    report.update(requested_quanta=args.frames,capture_every=args.capture_every,console='N64',rom_sha256=fingerprints['rom'],rom_size=args.rom.stat().st_size,
                  core_sha256=fingerprints['core'],probe_sha256=fingerprints['probe'],status='execution-smoke' if report['loaded'] else 'blocked',
                  detail='Execution signals do not establish boot or playability.' if report['loaded'] else 'The image is malformed or its native boot variant is unsupported; retail boot remains unverified.')
    if report['loaded']:
        trace_command=[str(args.trace.resolve()),str(args.rom.resolve()),str(args.frames*100000)]
        if args.input_script or args.load_state:trace_command.append(str(args.input_script.resolve()) if args.input_script else '-')
        if args.load_state:trace_command+=['--load-state',str(args.load_state.resolve())]
        traced=subprocess.run(trace_command,capture_output=True,text=True,timeout=900)
        if traced.returncode:raise RuntimeError(traced.stdout+traced.stderr or 'boot trace failed')
        report['boot_trace']=json.loads(traced.stdout)
        report['trace_sha256']=fingerprints['trace']
        if report['boot_trace'].get('exception_observed'):
            report['status']='boot-blocked'
            report['detail']='The cartridge loads; IPL3 reaches a CPU exception. Read the recorded EPC, Cause and fault address; retail boot/playability remain unverified.'
        elif report['boot_trace'].get('rsp_blocked'):
            report['status']='boot-blocked'
            report['detail']='The CPU trace reaches its bound; the RSP stopped on an unsupported operation. Read sp_pc and rsp_blocked_word; retail boot/playability remain unverified.'
        elif report['boot_trace'].get('dp_blocked'):
            report['status']='boot-blocked'
            report['detail']='The display processor stopped on an unsupported command/profile. Read dp_blocked_word and reason; retail boot/playability remain unverified.'
    for name,path in inputs.items():
        if hashlib.sha256(path.read_bytes()).hexdigest()!=fingerprints[name]:raise RuntimeError(f'{name} changed during the probe; observations have no stable provenance')
    if args.input_script:report['input_script']=dict(path=str(args.input_script.resolve()),sha256=fingerprints['input_script'],format='absolute decimal quantum / hexadecimal port-zero core word; retained until next transition')
    if args.load_state:report['load_state']=dict(path=str(args.load_state.resolve()),sha256=fingerprints['load_state'])
    if args.save_state and report.get('state_written'):report['save_state']=dict(path=str(args.save_state.resolve()),sha256=hashlib.sha256(args.save_state.read_bytes()).hexdigest(),size=args.save_state.stat().st_size)
    if args.capture and report.get('capture_written'):report['capture']=dict(path=str(args.capture.resolve()),sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest())
    if args.capture:
        report['checkpoint_captures']=[]
        for quantum in report.get('capture_checkpoints',[]):
            checkpoint=Path(str(args.capture.resolve())+f'.q{quantum}.ppm')
            report['checkpoint_captures'].append(dict(quantum=quantum,path=str(checkpoint),sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest()))
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
