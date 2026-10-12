#!/usr/bin/env python3
"""Grade original N64 SRAM DMA register replies and complete saved media."""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

from build import SUITE, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def bank_bytes(size):
    return bytes((17 * (offset // 64) + 37 * (offset % 64) +
                  (offset // 64 // 256)) % 256 for offset in range(size))


def expected_rows(size):
    bank = bank_bytes(size)
    for block in range(size // 64):
        content = bank[block * 64:(block + 1) * 64]
        regs = [int.from_bytes(content[i:i + 8], 'big') for i in range(0, 64, 8)]
        regs += [0x00700140, 0x08000000 + (block + 1) * 64, 127, 8]
        yield ''.join(f'{value:016x}' for value in regs)


def evaluate(registers, saved, image, size):
    count = size // 64
    report = dict(status='error', suite=SUITE, sram_bytes=size,
                  expected_cases=count, graded_cases=0, mismatches=0,
                  examples=[], rom_sha256=hashlib.sha256(image).hexdigest())
    try:
        with registers.open(encoding='ascii') as stream:
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() == f'BEGIN sram bytes={size} cases={count} words=12\n',
                    'wrong domain')
            for index, want in enumerate(expected_rows(size)):
                line = stream.readline()
                require(re.fullmatch(r'REG r=[0-9a-f]{192}\n', line),
                        f'malformed guest register row {index}')
                got = line[6:198]
                report['graded_cases'] += 1
                if got != want:
                    report['mismatches'] += 1
                    if len(report['examples']) < 5:
                        report['examples'].append(dict(index=index, expected=want, observed=got))
            require(stream.readline() == f'LOG complete cases={count}\n', 'missing count')
            require(stream.readline() == 'END observations_complete\n', 'missing terminator')
            require(not stream.read(), 'trailing register data')
        actual = saved.read_bytes()
        expected = bank_bytes(size)
        require(len(actual) == size, 'wrong SRAM bank length')
        report['bank_mismatches'] = sum(a != b for a, b in zip(actual, expected))
        report['status'] = 'fail' if report['mismatches'] or report['bank_mismatches'] else 'pass'
    except (OSError, UnicodeError, ValueError) as error:
        report['detail'] = str(error)
    return report


def execute(capture, plugin, image, registers, saved):
    result = subprocess.run([str(capture.resolve()), str(plugin.resolve()),
                             str(image.resolve()), str(registers.resolve()),
                             str(saved.resolve())], cwd=ROOT,
                            capture_output=True, text=True, timeout=180)
    require(result.returncode == 0, result.stdout + result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=ROOT / 'build/bin/n64_cartridge_sram_capture')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_cartridge_sram')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    archived = build(98304)
    require((HERE / 'cartridge_sram.z64').read_bytes() == archived,
            'archived original cartridge differs from source')
    with tempfile.TemporaryDirectory(prefix='n64_sram_', dir=args.artifacts) as temporary:
        root = Path(temporary)
        for size in (32768, 98304):
            image = build(size)
            path = root / f'sram-{size}.z64'
            path.write_bytes(image)
            registers = args.artifacts / f'{size}.result'
            saved = args.artifacts / f'{size}.sav'
            execute(args.capture, args.plugin, path, registers, saved)
            report = evaluate(registers, saved, image, size)
            (args.artifacts / f'{size}.json').write_text(json.dumps(report, indent=2) + '\n')
            require(report['status'] == 'pass', json.dumps(report))
        mutant = build(98304, wrong_capture=True)
        mutated = root / 'mutant.z64'
        mutated.write_bytes(mutant)
        bad_registers = args.artifacts / 'mutant.result'
        bad_save = args.artifacts / 'mutant.sav'
        execute(args.capture, args.plugin, mutated, bad_registers, bad_save)
        rejected = evaluate(bad_registers, bad_save, mutant, 98304)
        require(rejected['status'] == 'fail' and rejected['mismatches'] == 1536 and
                rejected['bank_mismatches'] == 0, 'executed wrong-register mutant escaped oracle')
        (args.artifacts / 'mutant.json').write_text(json.dumps(rejected, indent=2) + '\n')
        content = (args.artifacts / '98304.result').read_text()
        row = next(line for line in content.splitlines() if line.startswith('REG r='))
        for name, damaged in {
            'missing-row': content.replace(row + '\n', '', 1),
            'bad-row': content.replace(row, 'REG r=invalid', 1),
            'false-end': content.replace('END observations_complete', 'END false'),
        }.items():
            path = args.artifacts / (name + '.result')
            path.write_text(damaged)
            require(evaluate(path, args.artifacts / '98304.sav', archived, 98304)['status'] == 'error',
                    name + ' damaged register file passed')
    print('N64 SRAM PASS: 2,048 guest DMA cases / 24,576 register words, '
          '131,072 saved bytes, executed mutant and damaged files')


if __name__ == '__main__':
    main()
