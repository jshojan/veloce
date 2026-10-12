#!/usr/bin/env python3
"""Grade private SM64 US lobby dialogue and movement from a castle checkpoint."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from n64_retail_castle_entry import ROM_SHA, inspect, require

ROOT = Path(__file__).resolve().parents[1]
AUTO_DIALOG = 0x20001305
WALKING = 0x04000440


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--checkpoint', type=Path, required=True,
                        help='Private castle-entry interior state at quantum 72760')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/retail/lobby')
    parser.add_argument('--probe', type=Path,
                        default=ROOT / 'build/bin/retail_core_probe')
    parser.add_argument('--inspector', type=Path,
                        default=ROOT / 'build/bin/n64_state_inspect')
    parser.add_argument('--core', type=Path,
                        default=ROOT / 'build/bin/cores/n64.so')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    sources = {str(path.resolve()): digest(path) for path in
               (args.rom, args.checkpoint, args.probe, args.inspector, args.core,
                Path(__file__), ROOT / 'tests/n64_retail_castle_entry.py')}
    require(sources[str(args.rom.resolve())] == ROM_SHA,
            'this guest layout applies only to SM64 US')
    start = inspect(args.inspector, args.rom, args.checkpoint)
    require(start['frame'] == 72760 and start['action'] == 0x1322 and
            start['position'] == [-946.0, -0.0, 1902.0],
            'wrong castle interior checkpoint')
    observations = {'interior': start}
    records = {}

    def run(name, prior, count, transitions):
        before = inspect(args.inspector, args.rom, prior)
        script = args.artifacts / (name + '.input')
        script.write_text(''.join(f'{frame} {value:08x}\n'
                                  for frame, value in transitions))
        state = args.artifacts / (name + '.state')
        capture = args.artifacts / (name + '.ppm')
        command = [str(args.probe.resolve()), str(args.core.resolve()),
                   str(args.rom.resolve()), str(count), str(capture.resolve()),
                   str(script.resolve()), '--scheduling', 'video-field',
                   '--capture-every', str(count), '--load-state',
                   str(prior.resolve()), '--save-state', str(state.resolve())]
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        require(result.returncode == 0,
                result.stdout + result.stderr or name + ' failed')
        record = json.loads(result.stdout)
        require(record['loaded'] and record['scheduling'] == 'video-field' and
                record['starting_quantum'] == before['frame'] and
                record['frames'] == before['frame'] + count and
                record['state_written'] and record['capture_written'] and
                record['nonuniform_video_observed'], name + ' incomplete')
        observed = inspect(args.inspector, args.rom, state)
        require(observed['frame'] == record['frames'], name + ' frame mismatch')
        observations[name] = observed
        records[name] = {'input_sha256': digest(script),
                         'state_sha256': digest(state),
                         'capture_sha256': digest(capture),
                         'attempts': record['executed_attempts']}
        return state

    ready = run('ready', args.checkpoint, 120, [(72760, 0x0000c000)])
    require(observations['ready']['action'] == AUTO_DIALOG,
            'castle welcome dialogue did not start')
    run('no_a', ready, 300, [(72880, 0x0000c000)])
    require(observations['no_a']['action'] == AUTO_DIALOG and
            observations['no_a']['position'] == observations['ready']['position'],
            'neutral input dismissed or moved during dialogue')
    presses = [(q + offset, value) for q in range(72880, 73151, 30)
               for offset, value in ((0, 0x0000c001), (2, 0x0000c000))]
    dismissed = run('dismiss', ready, 300, presses)
    require(observations['dismiss']['action'] != AUTO_DIALOG,
            'A pulses did not dismiss the castle welcome dialogue')
    approach = run('approach', dismissed, 280, [(73180, 0xb000c000)])
    require(observations['approach']['action'] == WALKING and
            observations['approach']['position'][2] < 1500,
            'stick input did not move into the lobby')
    cross = run('cross', approach, 140, [(73460, 0x0050c000)])
    require(observations['cross']['position'][0] > -500,
            'stick input did not cross the lobby')
    hall = run('hall', cross, 160, [(73600, 0x5000c000)])
    require(observations['hall']['action'] == WALKING and
            observations['hall']['position'][0] > 900 and
            observations['hall']['position'][1] == 205.0 and
            observations['hall']['position'][2] < 100,
            'Mario did not reach the raised main hall')
    require(all(digest(Path(path)) == value for path, value in sources.items()),
            'ROM, checkpoint, source or executable changed during capture')
    report = {'status': 'castle-lobby-pass', 'private_rom_sha256': ROM_SHA,
              'sources_sha256': sources, 'observations': observations,
              'artifacts': records, 'controls': ['no_a'],
              'course_entry_verified': False, 'full_title_playability': False}
    (args.artifacts / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Private SM64 lobby PASS: welcome dialogue, A dismissal and raised hall movement; neutral control rejected')


if __name__ == '__main__':
    main()
