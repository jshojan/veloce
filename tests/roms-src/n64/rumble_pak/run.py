#!/usr/bin/env python3
"""Grade original guest Rumble Pak SI replies against a separate host oracle."""
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


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def data_crc(data):
    table = (0x89, 0x86, 0x43, 0xe3, 0xb3, 0x9b, 0x8f, 0x85)
    crc = 0
    for byte in data:
        mixed = crc ^ byte
        crc = 0
        for bit, value in enumerate(table):
            if mixed & (0x80 >> bit):
                crc ^= value
    return crc


def oracle_address_crc(offset):
    contributions = (0x15, 0x1f, 0x0b, 0x16, 0x19, 0x07,
                     0x0e, 0x1c, 0x0d, 0x1a, 0x01)
    result = 0
    for bit in range(5, 16):
        if offset & (1 << bit):
            result ^= contributions[bit - 5]
    return result


def oracle_cases():
    for port in range(4):
        yield port, 0, 0, 0, 0
        yield port, 0, 0, 0, 1
        for phase in range(3):
            yield port, 0xc000, 3, phase, 0
            for block in range(2048):
                yield port, block * 32, 2, phase, 0


def expected_rows():
    for port, offset, command, phase, status_index in oracle_cases():
        data = bytearray([0xaa] * 64)
        data[:port] = bytes(port)
        tx, rx = {0: (1, 3), 2: (3, 33), 3: (35, 1)}[command]
        data[port:port + 3] = bytes((tx, rx, command))
        if command:
            address = offset | oracle_address_crc(offset)
            data[port + 3:port + 5] = address.to_bytes(2, 'big')
        if command == 3:
            payload = bytes((phase & 1,)) + bytes([port * 17 + phase] * 31)
            data[port + 5:port + 37] = payload
        reply = port + 2 + tx
        if command == 0:
            data[reply:reply + 3] = bytes((5, 0, 3 if status_index == 0 else 1))
        elif command == 2:
            byte = 0 if offset < 0x8000 else 0x80 if offset < 0x9000 else 0xff if phase == 1 else 0
            payload = bytes([byte] * 32)
            data[reply:reply + 32] = payload
            data[reply + 32] = data_crc(payload)
        else:
            data[reply] = data_crc(payload)
        data[reply + rx] = 254
        data[63] = 0
        yield bytes(data)


def evaluate(registers, image):
    report = dict(status='error', suite=SUITE, expected_cases=CASES,
                  graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(image).hexdigest())
    try:
        with registers.open(encoding='ascii') as stream:
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() == f'BEGIN rumble cases={CASES} words=8\n', 'wrong domain')
            for index, want in enumerate(expected_rows()):
                line = stream.readline()
                require(re.fullmatch(r'REG r=[0-9a-f]{128}\n', line),
                        f'malformed guest register row {index}')
                got = bytes.fromhex(line[6:134])
                report['graded_cases'] += 1
                if got != want:
                    report['mismatches'] += 1
                    if len(report['examples']) < 5:
                        report['examples'].append(dict(index=index,
                                                       expected=want.hex(), observed=got.hex()))
            require(stream.readline() == f'LOG complete cases={CASES}\n', 'missing count')
            require(stream.readline() == 'END observations_complete\n', 'missing terminator')
            require(not stream.read(), 'trailing register data')
        report['status'] = 'fail' if report['mismatches'] else 'pass'
    except (OSError, UnicodeError, ValueError) as error:
        report['detail'] = str(error)
    return report


def execute(capture, plugin, image, registers):
    result = subprocess.run([str(capture.resolve()), str(plugin.resolve()),
                             str(image.resolve()), str(registers.resolve())], cwd=ROOT,
                            capture_output=True, text=True, timeout=180)
    require(result.returncode == 0, result.stdout + result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=ROOT / 'build/bin/n64_rumble_pak_capture')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_rumble_pak')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require((HERE / 'rumble_pak.z64').read_bytes() == rom,
            'archived original cartridge differs from source')
    with tempfile.TemporaryDirectory(prefix='n64_rumble_', dir=args.artifacts) as temporary:
        image = Path(temporary) / 'rumble.z64'
        image.write_bytes(rom)
        registers = args.artifacts / 'first.result'
        execute(args.capture, args.plugin, image, registers)
        report = evaluate(registers, rom)
        (args.artifacts / 'first.json').write_text(json.dumps(report, indent=2) + '\n')
        require(report['status'] == 'pass', json.dumps(report))
        mutant = build(wrong_capture=True)
        mutated_image = Path(temporary) / 'mutant.z64'
        mutated_image.write_bytes(mutant)
        bad_registers = args.artifacts / 'mutant.result'
        execute(args.capture, args.plugin, mutated_image, bad_registers)
        rejected = evaluate(bad_registers, mutant)
        require(rejected['status'] == 'fail' and rejected['mismatches'] == CASES,
                'executed wrong-register mutant escaped oracle')
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
            require(evaluate(path, rom)['status'] == 'error', name + ' damaged register file passed')
    print(f'N64 Rumble Pak PASS: {CASES} guest packets / {CASES * 8} '
          'register words, executed mutant and damaged files')


if __name__ == '__main__':
    main()
