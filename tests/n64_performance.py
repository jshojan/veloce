#!/usr/bin/env python3
"""Compare N64 plugin throughput from a local checkpoint and grade full states."""
import argparse
import hashlib
import json
import statistics
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--before-core', type=Path, required=True)
    parser.add_argument('--core', type=Path, default=ROOT/'build/bin/cores/n64.so')
    parser.add_argument('--probe', type=Path, default=ROOT/'build/bin/retail_core_probe')
    parser.add_argument('--fields', type=int, default=60)
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--min-speedup', type=float, default=0)
    parser.add_argument('--artifacts', type=Path, default=ROOT/'build/artifacts/performance/n64')
    args = parser.parse_args()
    if not 1 <= args.fields <= 100000 or not 1 <= args.runs <= 9 or args.min_speedup < 0:
        parser.error('invalid fields, runs or speedup')
    paths = (args.rom,args.checkpoint,args.before_core,args.core,args.probe,Path(__file__))
    sources = {str(p.resolve()):digest(p) for p in paths}
    args.artifacts.mkdir(parents=True,exist_ok=True)
    observations = {'before': [], 'after': []}
    for index in range(args.runs):
        states = {}
        # Alternate order to reduce systematic warmup and host-load bias.
        for name in (('before','after') if index%2==0 else ('after','before')):
            core = args.before_core if name=='before' else args.core
            state = args.artifacts/f'{name}-{index}.state'
            command = [str(args.probe.resolve()),str(core.resolve()),str(args.rom.resolve()),
                       str(args.fields),'-','-','--scheduling','video-field',
                       '--load-state',str(args.checkpoint.resolve()),'--save-state',str(state.resolve())]
            start = time.perf_counter()
            result = subprocess.run(command,capture_output=True,text=True,timeout=600)
            elapsed = time.perf_counter()-start
            if result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            record = json.loads(result.stdout)
            if not record['loaded'] or not record['state_written'] or record['requested_quanta']!=args.fields:
                raise RuntimeError('incomplete benchmark continuation')
            record.update(wall_seconds=elapsed,fields_per_second=args.fields/elapsed,state_sha256=digest(state))
            observations[name].append(record)
            states[name] = state
        if states['before'].read_bytes()!=states['after'].read_bytes():
            raise RuntimeError(f'complete native state changed in pair {index}')
        for field in ('frames','starting_quantum','executed_attempts','audio_sample_pairs_observed'):
            if observations['before'][-1][field]!=observations['after'][-1][field]:
                raise RuntimeError('emulated work changed: '+field)
    if any(digest(Path(p))!=value for p,value in sources.items()):
        raise RuntimeError('benchmark source changed during measurement')
    medians = {name:statistics.median(r['wall_seconds'] for r in rows) for name,rows in observations.items()}
    speedup = medians['before']/medians['after']
    report = {'status':'pass','fields':args.fields,'runs':args.runs,'sources_sha256':sources,
              'observations':observations,'median_seconds':medians,'speedup':speedup,
              'median_fields_per_second':{name:args.fields/value for name,value in medians.items()},
              'complete_states_identical':True,'physical_timing_verified':False}
    if speedup < args.min_speedup:
        report['status']='below-requested-speedup'
    (args.artifacts/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:report[key] for key in ('status','median_fields_per_second','speedup','complete_states_identical')}))
    return 0 if report['status']=='pass' else 1

if __name__ == '__main__':
    raise SystemExit(main())
