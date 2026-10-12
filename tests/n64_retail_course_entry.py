#!/usr/bin/env python3
"""Grade private SM64 US entry and walking in Bob-omb Battlefield."""
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
                        default=ROOT / 'build/artifacts/retail/course-entry-graded')
    parser.add_argument('--probe', type=Path,
                        default=ROOT / 'build/bin/retail_core_probe')
    parser.add_argument('--inspector', type=Path,
                        default=ROOT / 'build/bin/n64_state_inspect')
    parser.add_argument('--core', type=Path,
                        default=ROOT / 'build/bin/cores/n64.so')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    sources = {str(path.resolve()): digest(path) for path in
               (args.rom, args.checkpoint, args.probe, args.inspector,
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

    selected = run('select', still, 180,
                   [(75480, 0x0000c001), (75482, 0x0000c000)])
    x, y, z = observations['select']['position']
    require(observations['select']['action'] == 0x0c400201 and
            observations['select']['terrain'] != start['terrain'] and
            observations['select']['terrain'] != 0 and
            observations['select']['camera'] != 0 and
            -6550 < x < -6500 and y == 0.0 and 6400 < z < 6460,
            'Bob-omb Battlefield did not load with Mario spawned')
    intro = run('intro_no_a', selected, 120, [(75660, 0x0000c000)])
    require(observations['intro_no_a']['position'] ==
            observations['select']['position'] and
            observations['intro_no_a']['action'] == observations['select']['action'],
            'neutral control dismissed the course introduction')
    presses = [(q + offset, value) for q in range(75780, 76051, 30)
               for offset, value in ((0, 0x0000c001), (2, 0x0000c000))]
    dismissed = run('dismiss_intro', intro, 300, presses)
    require(observations['dismiss_intro']['action'] !=
            observations['intro_no_a']['action'],
            'A pulses did not dismiss the course introduction')
    walk = run('walk', dismissed, 120, [(76080, 0x5000c000)])
    x, y, z = observations['walk']['position']
    require(observations['walk']['action'] == 0x04000440 and
            -5850 < x < -5750 and y == 128.0 and 5700 < z < 5850 and
            observations['walk']['terrain'] == observations['select']['terrain'],
            'Mario did not walk in Bob-omb Battlefield')
    run('course_neutral', walk, 300, [(76200, 0x0000c000)])
    run('course_walk', walk, 300, [(76200, 0x5000c000)])
    nx, ny, nz = observations['course_neutral']['position']
    x, y, z = observations['course_walk']['position']
    require(observations['course_neutral']['action'] == 0x0c400201 and
            abs(nx - observations['walk']['position'][0]) < 100 and
            ny == 128.0 and abs(nz - observations['walk']['position'][2]) < 100,
            'neutral course control kept Mario moving')
    require(observations['course_walk']['action'] == 0x04000440 and
            -2100 < x < -1900 and y == 0.0 and 3300 < z < 3500 and
            observations['course_walk']['terrain'] == observations['select']['terrain'],
            'sustained stick did not advance Mario across the course')
    require(all(digest(Path(path)) == value for path, value in sources.items()),
            'ROM, checkpoint, source or executable changed during capture')
    report = {'status': 'bob-omb-battlefield-walk-pass',
              'private_rom_sha256': ROM_SHA, 'sources_sha256': sources,
              'observations': observations, 'artifacts': records,
              'controls': ['title_no_a', 'intro_no_a', 'course_neutral'],
              'bounded_course_gameplay_verified': True,
              'full_title_playability': False}
    (args.artifacts / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Private SM64 Bob-omb Battlefield PASS: course load, intro controls and walking')


if __name__ == '__main__':
    main()
