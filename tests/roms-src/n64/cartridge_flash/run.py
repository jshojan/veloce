#!/usr/bin/env python3
"""Grade original N64 FlashRAM guest command and PI register captures."""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

from build import CASES, SUITE, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SIZE = 128 * 1024


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def bank_bytes():
    return bytes((17 * (offset // 128) + 37 * (offset % 128) +
                  (offset // 128 // 256)) % 256 for offset in range(SIZE))


def expected_rows():
    bank = bank_bytes()
    for page in range(CASES):
        content = bank[page * 128:(page + 1) * 128]
        regs = [int.from_bytes(content[i:i + 8], 'big') for i in range(0, 128, 8)]
        regs += [0x00700180, 0x08000000 + (page + 1) * 128, 127, 8]
        yield ''.join(f'{value:016x}' for value in regs)


def evaluate(registers, saved, image):
    report = dict(status='error', suite=SUITE, expected_cases=CASES,
                  graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(image).hexdigest())
    try:
        with registers.open(encoding='ascii') as stream:
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() == f'BEGIN flash pages={CASES} words=20\n', 'wrong domain')
            require(stream.readline() == 'REG id=1111800100c2001d\n', 'wrong FlashRAM silicon ID')
            for index, want in enumerate(expected_rows()):
                line = stream.readline()
                require(re.fullmatch(r'REG r=[0-9a-f]{320}\n', line),
                        f'malformed guest register row {index}')
                got = line[6:326]
                report['graded_cases'] += 1
                if got != want:
                    report['mismatches'] += 1
                    if len(report['examples']) < 5:
                        report['examples'].append(dict(index=index, expected=want, observed=got))
            require(stream.readline() == f'LOG complete pages={CASES}\n', 'missing count')
            require(stream.readline() == 'END observations_complete\n', 'missing terminator')
            require(not stream.read(), 'trailing register data')
        actual = saved.read_bytes()
        expected = bank_bytes()
        require(len(actual) == SIZE, 'wrong FlashRAM bank length')
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
    parser.add_argument('--capture', type=Path, default=ROOT / 'build/bin/n64_cartridge_flash_capture')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_cartridge_flash')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require((HERE / 'cartridge_flash.z64').read_bytes() == rom,
            'archived original cartridge differs from source')
    with tempfile.TemporaryDirectory(prefix='n64_flash_', dir=args.artifacts) as temporary:
        root = Path(temporary)
        image = root / 'flash.z64'
        image.write_bytes(rom)
        registers = args.artifacts / 'first.result'
        saved = args.artifacts / 'first.sav'
        execute(args.capture, args.plugin, image, registers, saved)
        report = evaluate(registers, saved, rom)
        (args.artifacts / 'first.json').write_text(json.dumps(report, indent=2) + '\n')
        require(report['status'] == 'pass', json.dumps(report))
        mutant = build(wrong_capture=True)
        mutated = root / 'mutant.z64'
        mutated.write_bytes(mutant)
        bad_registers = args.artifacts / 'mutant.result'
        bad_save = args.artifacts / 'mutant.sav'
        execute(args.capture, args.plugin, mutated, bad_registers, bad_save)
        rejected = evaluate(bad_registers, bad_save, mutant)
        require(rejected['status'] == 'fail' and rejected['mismatches'] == CASES and
                rejected['bank_mismatches'] == 0, 'executed wrong-register mutant escaped oracle')
        (args.artifacts / 'mutant.json').write_text(json.dumps(rejected, indent=2) + '\n')
        content = registers.read_text()
        row = next(line for line in content.splitlines() if line.startswith('REG r='))
        for name, damaged in {
            'missing-row': content.replace(row + '\n', '', 1),
            'bad-row': content.replace(row, 'REG r=invalid', 1),
            'false-end': content.replace('END observations_complete', 'END false'),
        }.items():
            path = args.artifacts / (name + '.result')
            path.write_text(damaged)
            require(evaluate(path, saved, rom)['status'] == 'error',
                    name + ' damaged register file passed')
    print('N64 FlashRAM PASS: 1,024 guest pages / 20,480 register words, '
          '131,072 saved bytes, silicon ID, executed mutant and damaged files')


if __name__ == '__main__':
    main()
