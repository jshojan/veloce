#!/usr/bin/env python3
"""Grade full guest SI packet rows for the no-accessory Joybus profile."""
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
import zlib
from pathlib import Path

from build import CASES, SUITE, WORDS, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def data_crc(data):
    # Independent host oracle from libdragon's eight-entry byte transform.
    table = (0x89, 0x86, 0x43, 0xe3, 0xb3, 0x9b, 0x8f, 0x85)
    crc = 0
    for byte in data:
        mixed = crc ^ byte
        crc = 0
        for bit, value in enumerate(table):
            if mixed & (0x80 >> bit):
                crc ^= value
    return crc


def expected():
    for port in range(4):
        for seed in range(256):
            for command in (2, 3):
                packet = bytearray([0xaa] * 64)
                for channel in range(port):
                    packet[channel] = 0
                tx, rx = ((3, 33) if command == 2 else (35, 1))
                packet[port] = tx
                packet[port + 1] = rx
                packet[port + 2] = command
                packet[port + 3] = seed
                packet[port + 4] = seed * 73 & 255
                reply = port + 2 + tx
                if command == 2:
                    packet[reply:reply + 32] = bytes(32)
                    packet[reply + 32] = 255
                else:
                    payload = bytes((seed * 17 + lane * 53) & 255
                                    for lane in range(32))
                    packet[port + 5:port + 37] = payload
                    packet[reply] = data_crc(payload) ^ 255
                packet[reply + rx] = 254
                packet[63] = 0
                yield bytes(packet)


def evaluate(path, rom):
    report = dict(status='error', suite=SUITE, expected_cases=CASES,
                  graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(rom).hexdigest(),
                  result_file=str(path))
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline() ==
                    f'#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n',
                    'wrong ROM or result identity')
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() ==
                    f'BEGIN accessory cases={CASES} words={WORDS}\n',
                    'wrong domain')
            for index, want in enumerate(expected()):
                line = stream.readline()
                require(re.fullmatch(r'REG r=[0-9a-f]{128}\n', line),
                        'missing or malformed guest register row')
                got = bytes.fromhex(line[6:134])
                report['graded_cases'] += 1
                if got != want:
                    report['mismatches'] += 1
                    if len(report['examples']) < 8:
                        report['examples'].append(dict(index=index,
                                                       expected=want.hex(),
                                                       observed=got.hex()))
            require(stream.readline() == f'LOG complete cases={CASES}\n',
                    'missing completion count')
            require(stream.readline() == 'END observations_complete\n',
                    'missing completion marker')
            require(re.fullmatch(
                r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',
                stream.readline()), 'missing termination trailer')
            require(not stream.read(), 'trailing data')
        report['status'] = 'fail' if report['mismatches'] else 'pass'
    except (OSError, UnicodeError, ValueError) as error:
        report['detail'] = str(error)
    return report


def execute(emulator, image, result):
    env = os.environ.copy()
    for key in ('INPUT', 'INPUT_FILE', 'DEBUG', 'TRACE', 'TRACE_FILE',
                'VELOCE_TEST_RESETS'):
        env.pop(key, None)
    env.update(HEADLESS='1', FRAMES='1000',
               VELOCE_TEST_OUT=str(result.resolve()), VELOCE_TEST_EXIT='1')
    process = subprocess.run([str(emulator.resolve()), str(image.resolve())],
                             cwd=ROOT, env=env, capture_output=True,
                             text=True, timeout=120)
    require(process.returncode == 0, process.stdout + process.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, default=ROOT / 'build/bin/veloce')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_accessory_absent')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require((HERE / 'accessory_absent.z64').read_bytes() == rom,
            'archived original cartridge differs from source')
    with tempfile.TemporaryDirectory(prefix='n64_accessory_', dir=args.artifacts) as temp:
        image = Path(temp) / 'probe.z64'
        image.write_bytes(rom)
        first = args.artifacts / 'first.result'
        execute(args.emulator, image, first)
        report = evaluate(first, rom)
        (args.artifacts / 'first.json').write_text(json.dumps(report, indent=2) + '\n')
        require(report['status'] == 'pass', json.dumps(report))
        mutant = build(wrong_capture=True)
        bad_image = Path(temp) / 'mutant.z64'
        bad_image.write_bytes(mutant)
        bad_file = args.artifacts / 'executed-mutant.result'
        execute(args.emulator, bad_image, bad_file)
        rejected = evaluate(bad_file, mutant)
        require(rejected['status'] == 'fail' and
                rejected['mismatches'] == CASES,
                'executed wrong-register mutant escaped the oracle')
        (args.artifacts / 'executed-mutant.json').write_text(
            json.dumps(rejected, indent=2) + '\n')
    content = first.read_text()
    row = next(line for line in content.splitlines() if line.startswith('REG r='))
    for name, broken in {
        'missing-row': content.replace(row + '\n', '', 1),
        'bad-row': content.replace(row, 'REG r=invalid', 1),
        'false-end': content.replace('END observations_complete', 'END false'),
    }.items():
        path = args.artifacts / (name + '.result')
        path.write_text(broken)
        require(evaluate(path, rom)['status'] == 'error',
                name + ' register file passed structural validation')
    print(f'N64 absent accessory PASS: {CASES}/{CASES} guest SI packets, '
          'executed wrong-register mutant and three damaged-file controls')


if __name__ == '__main__':
    main()
