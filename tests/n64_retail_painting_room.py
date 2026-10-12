#!/usr/bin/env python3
"""Grade a private SM64 US route through the zero-star door to Bob-omb's painting."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from n64_retail_castle_entry import ROM_SHA, inspect, require

ROOT = Path(__file__).resolve().parents[1]
WALKING = 0x04000440
DOOR_PUSH = 0x1321


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--checkpoint', type=Path, required=True,
                        help='Private ground-floor state at VI field 74040')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/retail/painting-room')
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
    x, y, z = start['position']
    require(start['frame'] == 74040 and start['action'] == WALKING and
            -821 < x < -819 and y == -50.0 and 641 < z < 643,
            'wrong ground-floor checkpoint')
    observations = {'ground_floor': start}
    records = {}

    def run(name, prior, count, value):
        before = inspect(args.inspector, args.rom, prior)
        script = args.artifacts / (name + '.input')
        script.write_text(f"{before['frame']} {value:08x}\n")
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

    left = run('left', args.checkpoint, 40, 0x00b0c000)
    stairs = run('stairs', left, 80, 0x5000c000)
    landing = run('landing', stairs, 120, 0x5000c000)
    door = run('door_approach', landing, 120, 0x5000c000)
    x, y, z = observations['door_approach']['position']
    require(observations['door_approach']['action'] == WALKING and
            -3022 < x < -3020 and y == 205.0 and -590 < z < -587,
            'Mario did not reach the zero-star door approach')
    run('door_no_stick', door, 80, 0x0000c000)
    require(observations['door_no_stick']['position'] ==
            observations['door_approach']['position'] and
            observations['door_no_stick']['action'] != DOOR_PUSH,
            'neutral control entered the painting room')
    opened = run('door_push', door, 80, 0x5000c000)
    x, y, z = observations['door_push']['position']
    require(observations['door_push']['action'] == DOOR_PUSH and
            -3100 < x < -3075 and y == 205.0 and -830 < z < -780,
            'guest did not pull the zero-star door')
    run('painting_room', opened, 160, 0x0000c000)
    x, y, z = observations['painting_room']['position']
    require(-3450 < x < -3400 and y == 205.0 and -820 < z < -770 and
            observations['painting_room']['camera_cutscene'] == 0 and
            observations['painting_room']['terrain'] == start['terrain'],
            'Mario did not enter the Bob-omb painting room')
    require(all(digest(Path(path)) == value for path, value in sources.items()),
            'ROM, checkpoint, source or executable changed during capture')
    report = {'status': 'painting-room-pass', 'private_rom_sha256': ROM_SHA,
              'sources_sha256': sources, 'observations': observations,
              'artifacts': records, 'controls': ['door_no_stick'],
              'course_entry_verified': False, 'full_title_playability': False}
    (args.artifacts / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Private SM64 painting room PASS: zero-star door push and painting room; neutral control rejected')


if __name__ == '__main__':
    main()
