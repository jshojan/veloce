#!/usr/bin/env python3
"""Grade guest SI EEPROM register files and production frontend persistence."""
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
import zlib
from pathlib import Path

from build import BLOCKS, CASES, PATTERNS, SUITE, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def reference(block, pattern):
    # Host arithmetic is independent of the MIPS table-reader and SI packet path.
    result = 0
    for lane in range(8):
        arithmetic = (37 * block + 59 * pattern + 101 * lane) % 256
        result = (result << 8) | (arithmetic ^ ((pattern * (2 ** (lane % 4))) % 256))
    return result


def expected(preloaded):
    first = PATTERNS - 1 if preloaded else None
    for block in range(BLOCKS):
        yield ((1 << 64) - 1 if first is None else reference(block, first), 0)
    for block in range(BLOCKS):
        for pattern in range(PATTERNS):
            yield (reference(block, pattern), 0)
    for block in range(BLOCKS):
        yield (reference(block, PATTERNS - 1), 0)


def evaluate(path, rom, preloaded=False):
    report = dict(status='error', suite=SUITE, expected_cases=CASES,
                  observed_cases=0, graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(rom).hexdigest(),
                  result_file=str(path))
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline() ==
                    f'#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n',
                    'wrong ROM or result identity')
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() == f'BEGIN eeprom cases={CASES} words=2\n',
                    'wrong domain')
            for index, want in enumerate(expected(preloaded)):
                line = stream.readline()
                require(re.fullmatch(r'REG r=[0-9a-f]{32}\n', line),
                        'missing or malformed guest register row')
                got = (int(line[6:22], 16), int(line[22:38], 16))
                report['observed_cases'] += 1
                report['graded_cases'] += 1
                if got != want:
                    report['mismatches'] += 1
                    if len(report['examples']) < 8:
                        report['examples'].append(dict(index=index,
                                                       expected=want, observed=got))
            require(stream.readline() == f'LOG complete cases={CASES}\n',
                    'missing completion count')
            require(stream.readline() == 'END observations_complete\n',
                    'false completion marker')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',
                                 stream.readline()), 'missing termination trailer')
            require(not stream.read(), 'trailing data')
        report['status'] = 'fail' if report['mismatches'] else 'pass'
    except (OSError, UnicodeError, ValueError) as error:
        report['detail'] = str(error)
    return report


def run(emulator, image, result):
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
    return process.stdout + process.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, default=ROOT / 'build/bin/veloce')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_eeprom_4k')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require((HERE / 'eeprom_4k.z64').read_bytes() == rom,
            'archived original cartridge differs from source')
    paths = json.loads((ROOT / 'config/paths.json').read_text())
    save_dir = Path(paths['save_directory'])
    if not save_dir.is_absolute():
        save_dir = ROOT / save_dir
    save_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='n64_eeprom_', dir=args.artifacts) as temp:
        temp = Path(temp)
        image = temp / (temp.name + '_probe.z64')
        image.write_bytes(rom)
        save_path = save_dir / (image.stem + '.sav')
        # An isolated image name is used for each run; only its own save is removed.
        require(not save_path.exists(), 'unexpected save-file collision')
        try:
            first = args.artifacts / 'first.result'
            log = run(args.emulator, image, first)
            report = evaluate(first, rom)
            (args.artifacts / 'first.json').write_text(json.dumps(report, indent=2) + '\n')
            require(report['status'] == 'pass', json.dumps(report))
            require(save_path.read_bytes() == b''.join(
                reference(block, PATTERNS - 1).to_bytes(8, 'big')
                for block in range(BLOCKS)), 'frontend saved wrong EEPROM bytes')
            require('Saved battery data' in log, 'frontend did not persist EEPROM')
            replay = args.artifacts / 'reloaded.result'
            log = run(args.emulator, image, replay)
            report = evaluate(replay, rom, preloaded=True)
            (args.artifacts / 'reloaded.json').write_text(json.dumps(report, indent=2) + '\n')
            require(report['status'] == 'pass' and 'Loaded battery save' in log,
                    'fresh frontend process did not restore EEPROM')
            save_path.write_bytes(b'\x01')
            corrupt = args.artifacts / 'corrupt-save.result'
            log = run(args.emulator, image, corrupt)
            report = evaluate(corrupt, rom)
            (args.artifacts / 'corrupt-save.json').write_text(
                json.dumps(report, indent=2) + '\n')
            require(report['status'] == 'pass' and
                    'Failed to load battery save data' in log,
                    'malformed save was accepted or changed guest reads')
            mutant = build(wrong_capture=True)
            mutant_image = temp / (temp.name + '_mutant.z64')
            mutant_image.write_bytes(mutant)
            mutant_result = args.artifacts / 'executed-mutant.result'
            run(args.emulator, mutant_image, mutant_result)
            bad = evaluate(mutant_result, mutant)
            require(bad['status'] == 'fail' and bad['mismatches'] == BLOCKS * PATTERNS,
                    'executed wrong-read mutant escaped the oracle')
            (args.artifacts / 'executed-mutant.json').write_text(
                json.dumps(bad, indent=2) + '\n')
        finally:
            save_path.unlink(missing_ok=True)
            (save_dir / (temp.name + '_mutant.sav')).unlink(missing_ok=True)
    # Reject malformed files and a wrong field in a genuine short capture.
    content = (args.artifacts / 'first.result').read_text()
    row = next(line for line in content.splitlines() if line.startswith('REG r='))
    broken = {
        'missing-row': content.replace(row + '\n', '', 1),
        'malformed-row': content.replace(row, 'REG r=bad', 1),
        'wrong-domain': content.replace(f'cases={CASES}', f'cases={CASES + 1}', 1),
        'false-end': content.replace('END observations_complete', 'END false'),
        'missing-trailer': content[:content.index('#VELOCE end ')],
        'trailing': content + 'extra\n',
    }
    for name, data in broken.items():
        path = args.artifacts / (name + '.result')
        path.write_text(data)
        require(evaluate(path, rom)['status'] == 'error',
                name + ' file passed structural validation')
    field = row[:6] + format(int(row[6:22], 16) ^ 1, '016x') + row[22:]
    path = args.artifacts / 'wrong-field.result'
    path.write_text(content.replace(row, field, 1))
    report = evaluate(path, rom)
    require(report['status'] == 'fail' and report['mismatches'] == 1,
            'wrong guest register value escaped the oracle')
    print(f'N64 4 Kbit EEPROM PASS: {CASES}/{CASES} guest rows, cross-process save, malformed-save rejection, executed mutant and 7 file controls')


if __name__ == '__main__':
    main()
