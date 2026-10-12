#!/usr/bin/env python3
"""Grade native N64 FlashRAM silicon ID register files independently."""
import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

from build import build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
IDS = ('NCC', 'NCV', 'NDA', 'NAF', 'NJF', 'NKJ', 'NZS', 'NM6', 'NCK',
       'NMQ', 'NPN', 'NPF', 'NPO', 'CP2', 'NP3', 'NRH', 'NSQ', 'NT9', 'NW4', 'NDP')
OLD = frozenset(('NCC', 'NAF', 'NJF', 'NZS', 'NM6', 'NCK', 'NPN', 'NPF'))


def grade(path):
    try:
        with path.open(encoding='ascii') as stream:
            if stream.readline() != 'VELOCE 1 n64 native_flash_identity\n':
                raise ValueError('suite header')
            if stream.readline() != 'BEGIN ids=22 words=2\n':
                raise ValueError('domain header')
            mismatches = 0
            for label in IDS + ('NCV-v64', 'NCV-n64'):
                line = stream.readline()
                match = re.fullmatch(
                    r'REG case=([A-Za-z0-9-]+) r14=([0-9a-f]{8}) '
                    r'r15=([0-9a-f]{8}) battery_bytes=131072\n', line)
                if not match or match.group(1) != label:
                    raise ValueError(f'malformed register row {label}')
                model = '00c2001e' if label in OLD else '00c2001d'
                mismatches += (match.group(2) != '11118001' or
                               match.group(3) != model)
            if stream.readline() != 'END observations_complete\n' or stream.read():
                raise ValueError('terminator or trailing data')
        return {'status': 'pass' if mismatches == 0 else 'fail',
                'graded_cases': 22, 'graded_register_words': 44,
                'mismatches': mismatches}
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
    parser.add_argument('--capture', type=Path, default=ROOT / 'build/bin/n64_native_flash_identity_test')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_native_flash_identity')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    original = build()
    if (HERE / 'native_flash_identity.z64').read_bytes() != original:
        raise RuntimeError('archived native Flash cartridge differs from source')
    with tempfile.TemporaryDirectory(prefix='native_flash_', dir=args.artifacts) as temporary:
        image = Path(temporary) / 'native.z64'
        image.write_bytes(original)
        registers = args.artifacts / 'registers.txt'
        capture(args.capture, args.plugin, image, registers)
        result = grade(registers)
        (args.artifacts / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
        if result['status'] != 'pass':
            raise RuntimeError(str(result))
        image.write_bytes(build(wrong_capture=True))
        mutant = args.artifacts / 'mutant.txt'
        capture(args.capture, args.plugin, image, mutant)
        rejected = grade(mutant)
        if rejected['status'] != 'fail' or rejected['mismatches'] != 22:
            raise RuntimeError('executed wrong-register cartridge escaped oracle')
        lines = registers.read_text().splitlines(keepends=True)
        damaged = args.artifacts / 'missing-row.txt'
        damaged.write_text(''.join(lines[:3] + lines[4:]))
        if grade(damaged)['status'] != 'error':
            raise RuntimeError('damaged register file escaped oracle')
    print('Native N64 FlashRAM PASS: 20 IDs, two swapped formats, '
          '44 guest register words, executed mutant and damaged file')


if __name__ == '__main__':
    main()
