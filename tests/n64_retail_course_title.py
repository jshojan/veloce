#!/usr/bin/env python3
"""Capture private SM64 US Bob-omb course selection and its current DP stop."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from n64_retail_castle_entry import ROM_SHA, inspect, require

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--checkpoint', type=Path, required=True,
                        help='Private Bob-omb painting-room state at VI field 74640')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/retail/course-title')
    parser.add_argument('--probe', type=Path,
                        default=ROOT / 'build/bin/retail_core_probe')
    parser.add_argument('--inspector', type=Path,
                        default=ROOT / 'build/bin/n64_state_inspect')
    parser.add_argument('--trace', type=Path,
                        default=ROOT / 'build/bin/n64_boot_trace')
    parser.add_argument('--core', type=Path,
                        default=ROOT / 'build/bin/cores/n64.so')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    sources = {str(path.resolve()): digest(path) for path in
               (args.rom, args.checkpoint, args.probe, args.inspector, args.trace,
                args.core, Path(__file__), ROOT / 'tests/n64_retail_castle_entry.py')}
    require(sources[str(args.rom.resolve())] == ROM_SHA,
            'this guest layout applies only to SM64 US')
    start = inspect(args.inspector, args.rom, args.checkpoint)
    x, y, z = start['position']
    require(start['frame'] == 74640 and -3450 < x < -3400 and y == 205.0 and
            -820 < z < -770, 'wrong painting-room checkpoint')
    observations = {'painting_room': start}
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
        observations[name] = inspect(args.inspector, args.rom, state)
        records[name] = {'input_sha256': digest(script),
                         'state_sha256': digest(state),
                         'capture_sha256': digest(capture),
                         'attempts': record['executed_attempts']}
        return state

    aligned = run('align', args.checkpoint, 30, [(74640, 0x50b0c000)])
    west = run('west', aligned, 60, [(74670, 0x5000c000)])
    face = run('stair_face', west, 60, [(74730, 0x0050c000)])
    airborne = run('stair_jump', face, 30, [(74790, 0x0050c001)])
    platform = run('platform', airborne, 50, [(74820, 0x5000c000)])
    painting = run('painting', platform, 50, [(74870, 0x50b0c000)])
    x, y, z = observations['painting']['position']
    require(-5180 < x < -5160 and y == 307.0 and -400 < z < -380,
            'Mario did not reach the center of the painting platform')
    entered = run('enter_painting', painting, 100, [(74920, 0x5000c001)])
    require(observations['enter_painting']['action'] == 0x1300,
            'painting jump did not enter the disappeared action')
    title = run('course_title', entered, 220, [(75020, 0x0000c000)])
    require(observations['course_title']['action'] == 0x1300 and
            observations['course_title']['terrain'] == 0 and
            observations['course_title']['camera'] == 0,
            'guest did not leave the castle for course selection')
    still = run('title_no_a', title, 240, [(75240, 0x0000c000)])
    require(observations['title_no_a']['action'] == 0x1300 and
            observations['title_no_a']['position'] ==
            observations['course_title']['position'],
            'neutral control dismissed the course title')

    # A separate probe follows selection because inspect() rightly rejects a
    # blocked display processor. This captures the current implementation gap.
    script = args.artifacts / 'select.input'
    script.write_text('75480 0000c001\n75482 0000c000\n')
    state = args.artifacts / 'select.state'
    capture = args.artifacts / 'select.ppm'
    command = [str(args.probe.resolve()), str(args.core.resolve()),
               str(args.rom.resolve()), '180', str(capture.resolve()),
               str(script.resolve()), '--scheduling', 'video-field',
               '--capture-every', '180', '--load-state', str(still.resolve()),
               '--save-state', str(state.resolve())]
    result = subprocess.run(command, capture_output=True, text=True, timeout=300)
    require(result.returncode == 0, result.stdout + result.stderr or 'select failed')
    probe = json.loads(result.stdout)
    require(probe['loaded'] and probe['frames'] == 75660 and
            probe['state_written'] and probe['capture_written'],
            'course selection probe incomplete')
    diagnostic = subprocess.run([str(args.trace.resolve()), str(args.rom.resolve()),
                                 '1', '-', '--load-state', str(state.resolve())],
                                capture_output=True, text=True, timeout=30)
    require(diagnostic.returncode == 0, diagnostic.stdout + diagnostic.stderr)
    stop = json.loads(diagnostic.stdout)
    require(stop['dp_blocked'] and not stop['rsp_blocked'] and
            stop['dp_blocked_reason'] == 1 and
            stop['dp_blocked_word'] == 'cf000146013d012b' and
            stop['dp_packet_count'] == stop['dp_packet_needed'] == 22 and
            ((int(stop['dp_other_mode'], 16) >> 52) & 3) == 1,
            'expected two-cycle DP stop changed; regrade course selection')
    records['select'] = {'input_sha256': digest(script),
                         'state_sha256': digest(state),
                         'capture_sha256': digest(capture),
                         'attempts': probe['executed_attempts']}
    require(all(digest(Path(path)) == value for path, value in sources.items()),
            'ROM, checkpoint, source or executable changed during capture')
    report = {'status': 'course-title-pass-selection-blocked',
              'private_rom_sha256': ROM_SHA, 'sources_sha256': sources,
              'observations': observations, 'selection_stop': stop,
              'artifacts': records, 'controls': ['title_no_a'],
              'course_gameplay_verified': False, 'full_title_playability': False}
    (args.artifacts / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Private SM64 course title PASS; selecting Course 1 blocks on a two-cycle DP triangle')


if __name__ == '__main__':
    main()
