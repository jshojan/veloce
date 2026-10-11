#!/usr/bin/env python3
"""Grade a private SM64 US courtyard-to-castle continuation through real guest state."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM_SHA = '17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91'
NPC_DIALOG = 0x20001306
PULLING_DOOR = 0x00001320
WARP_DOOR_SPAWN = 0x00001322


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect(inspector, rom, state):
    def words(address, count):
        result = subprocess.run([str(inspector), str(rom), str(state), f'{address:x}', str(count)],
                                capture_output=True, text=True, timeout=30)
        require(result.returncode == 0, result.stdout + result.stderr or 'state inspection failed')
        parsed = json.loads(result.stdout)
        require(not parsed['rsp_blocked'] and not parsed['dp_blocked'], 'RSP or DP stopped')
        return parsed['frames'], {row['address']: row['cpu_view'] for row in parsed['words']}

    frame, mario = words(0x33b170, 40)
    def floating(address):
        return struct.unpack('>f', struct.pack('>I', mario[address]))[0]
    area = mario[0x33b200] & 0x7fffff
    _, area_words = words(area, 16)
    camera = area_words[area + 0x24] & 0x7fffff
    _, camera_words = words(camera + 0x30, 1)
    used = mario[0x33b1f0] & 0x7fffff
    _, object_words = words(used + 0x90, 1)
    return {
        'frame': frame,
        'action': mario[0x33b17c],
        'action_state': mario[0x33b188] >> 16,
        'position': [floating(a) for a in (0x33b1ac, 0x33b1b0, 0x33b1b4)],
        'terrain': area_words[area + 0x08],
        'camera': area_words[area + 0x24],
        'camera_cutscene': camera_words[camera + 0x30] >> 24,
        'used_object_dialog_state': object_words[used + 0x90] & 0xffff,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--checkpoint', type=Path, required=True,
                        help='Private standing courtyard S029 checkpoint at quantum 64000')
    parser.add_argument('--artifacts', type=Path, default=ROOT / 'build/artifacts/retail/castle-entry')
    parser.add_argument('--probe', type=Path, default=ROOT / 'build/bin/retail_core_probe')
    parser.add_argument('--inspector', type=Path, default=ROOT / 'build/bin/n64_state_inspect')
    parser.add_argument('--core', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    sources = {str(path.resolve()): sha(path) for path in
               (args.rom, args.checkpoint, args.probe, args.inspector, args.core, Path(__file__))}
    require(sources[str(args.rom.resolve())] == ROM_SHA, 'this guest layout applies only to SM64 US')
    start = inspect(args.inspector, args.rom, args.checkpoint)
    require(start['frame'] == 64000 and start['action'] == 0x0c400201 and
            start['position'] == [-1328.0, 260.0, 4354.0], 'wrong standing checkpoint')
    observations = {'standing': start}
    records = {}

    def run(name, prior, count, transitions, video=False):
        script = args.artifacts / (name + '.input')
        script.write_text(''.join(f'{frame} {value:08x}\n' for frame, value in transitions))
        state = args.artifacts / (name + '.state')
        capture = args.artifacts / (name + '.ppm')
        command = [str(args.probe.resolve()), str(args.core.resolve()), str(args.rom.resolve()),
                   str(count), str(capture.resolve()), str(script.resolve()), '--capture-every',
                   str(min(count, 1000)), '--load-state', str(prior.resolve()),
                   '--save-state', str(state.resolve())]
        if video:
            command += ['--scheduling', 'video-field']
        result = subprocess.run(command, capture_output=True, text=True, timeout=900)
        require(result.returncode == 0, result.stdout + result.stderr or name + ' failed')
        record = json.loads(result.stdout)
        expected = observations['standing']['frame'] if name == 'approach' else inspect(args.inspector, args.rom, prior)['frame']
        require(record['loaded'] and record['starting_quantum'] == expected and
                record['frames'] == expected + count and record['state_written'] and
                record['capture_written'] and record['nonuniform_video_observed'] and
                record['scheduling'] == ('video-field' if video else 'instruction-quantum'),
                name + ' incomplete')
        observation = inspect(args.inspector, args.rom, state)
        require(observation['frame'] == record['frames'], name + ' state frame mismatch')
        observations[name] = observation
        records[name] = {'input_sha256': sha(script), 'state_sha256': sha(state),
                         'capture_sha256': sha(capture), 'attempts': record['executed_attempts']}
        return state

    # The stick route is measured in old 100,000-attempt diagnostic quanta.
    approach = run('approach', args.checkpoint, 7700, [
        (64000, 0x5000c000), (66500, 0x5050c000), (67300, 0x5000c000),
        (69100, 0x00b0c000), (69700, 0x5050c000), (70300, 0x5000c000)])
    require(observations['approach']['action'] == NPC_DIALOG and
            observations['approach']['action_state'] == 8, 'Lakitu dialogue did not start')
    opened = run('dialog_open', approach, 420, [(71700, 0x0000c000)], True)
    require(observations['dialog_open']['action'] == NPC_DIALOG and
            observations['dialog_open']['camera_cutscene'] == 162 and
            observations['dialog_open']['used_object_dialog_state'] == 2,
            'guest camera/dialog cutscene did not open')
    neutral = run('dialog_no_a', opened, 300, [(72120, 0x0000c000)], True)
    require(observations['dialog_no_a']['action'] == NPC_DIALOG,
            'neutral dialogue control ended NPC action')
    presses = [(q + offset, value) for q in range(72120, 72391, 30)
               for offset, value in ((0, 0x0000c001), (2, 0x0000c000))]
    dismissed = run('dialog_dismiss', opened, 300, presses, True)
    require(observations['dialog_dismiss']['action'] != NPC_DIALOG and
            observations['dialog_dismiss']['camera_cutscene'] == 0,
            'A pulses did not dismiss the dialogue')
    stopped = run('door_no_stick', dismissed, 220, [(72420, 0x0000c000)], True)
    require(observations['door_no_stick']['action'] != PULLING_DOOR and
            observations['door_no_stick']['terrain'] == observations['approach']['terrain'],
            'neutral door control entered castle')
    door = run('door_pull', dismissed, 220, [(72420, 0x5000c000)], True)
    require(observations['door_pull']['action'] == PULLING_DOOR and
            observations['door_pull']['position'][2] < -2500,
            'forward input did not pull the castle door')
    inside = run('interior', door, 120, [(72640, 0x0000c000)], True)
    require(observations['interior']['action'] == WARP_DOOR_SPAWN and
            observations['interior']['terrain'] != observations['approach']['terrain'] and
            observations['interior']['camera'] != observations['approach']['camera'],
            'guest did not switch to the castle interior')
    require(all(sha(Path(path)) == digest for path, digest in sources.items()),
            'ROM, checkpoint, source or executable changed during capture')
    report = {'status': 'castle-interior-pass', 'private_rom_sha256': ROM_SHA,
              'sources_sha256': sources, 'observations': observations, 'artifacts': records,
              'controls': ['dialog_no_a', 'door_no_stick'], 'full_title_playability': False}
    (args.artifacts / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Private SM64 castle entry PASS: Lakitu dialogue, A dismissal, door pull and interior warp; two executed controls rejected')


if __name__ == '__main__':
    main()
