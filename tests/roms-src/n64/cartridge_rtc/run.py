#!/usr/bin/env python3
"""Independent file oracle for original N64 cartridge RTC guest packets."""
import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

from build import build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
EXPECTED = bytes.fromhex('001000005959a3280302240100')


def grade(path):
    try:
        lines = path.read_text(encoding='ascii').splitlines()
        if len(lines) != 4 or lines[0] != 'VELOCE 1 n64 cartridge_rtc' or \
           lines[1] != 'BEGIN joybus words=13' or \
           lines[3] != 'END observations_complete':
            raise ValueError('RTC file framing')
        match = re.fullmatch(r'REG bytes=([0-9a-f]{26})', lines[2])
        if not match:
            raise ValueError('RTC register row')
        actual = bytes.fromhex(match.group(1))
        return {'status': 'pass' if actual == EXPECTED else 'fail',
                'graded_register_bytes': 13,
                'mismatches': sum(a != b for a, b in zip(actual, EXPECTED))}
    except (OSError, UnicodeError, ValueError) as error:
        return {'status': 'error', 'detail': str(error)}


def capture(executable, plugin, image, result):
    run = subprocess.run([str(executable.resolve()), str(plugin.resolve()),
                          str(image.resolve()), str(result.resolve())],
                         cwd=ROOT, capture_output=True, text=True, timeout=90)
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=ROOT / 'build/bin/n64_cartridge_rtc_capture')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    parser.add_argument('--artifacts', type=Path, default=ROOT / 'build/artifacts/owned/n64_cartridge_rtc')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    original = build()
    if (HERE / 'cartridge_rtc.z64').read_bytes() != original:
        raise RuntimeError('archived RTC ROM differs from source')
    with tempfile.TemporaryDirectory(prefix='cartridge_rtc_', dir=args.artifacts) as temporary:
        image = Path(temporary) / 'rtc.z64'
        image.write_bytes(original)
        output = args.artifacts / 'registers.txt'
        capture(args.capture, args.plugin, image, output)
        result = grade(output)
        (args.artifacts / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
        if result['status'] != 'pass':
            raise RuntimeError(str(result))
        image.write_bytes(build(wrong_capture=True))
        mutant = args.artifacts / 'mutant.txt'
        capture(args.capture, args.plugin, image, mutant)
        if grade(mutant)['status'] != 'fail':
            raise RuntimeError('executed wrong-register RTC cartridge escaped oracle')
        damaged = args.artifacts / 'damaged.txt'
        damaged.write_text(output.read_text().replace('END observations_complete', 'END false'))
        if grade(damaged)['status'] != 'error':
            raise RuntimeError('damaged RTC file escaped oracle')
    print('N64 RTC PASS: guest Joybus status/write/read, 13 register bytes, '
          'S038 rollback, executed mutant and damaged file')


if __name__ == '__main__':
    main()
