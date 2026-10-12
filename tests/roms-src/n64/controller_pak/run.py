#!/usr/bin/env python3
"""Grade original guest SI Pak replies and final four 32 KiB banks."""
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
    # Independent byte transform from libdragon's accessory CRC reference.
    table = (0x89, 0x86, 0x43, 0xe3, 0xb3, 0x9b, 0x8f, 0x85)
    crc = 0
    for byte in data:
        mixed = crc ^ byte
        crc = 0
        for bit, value in enumerate(table):
            if mixed & (0x80 >> bit):
                crc ^= value
    return crc


def oracle_payload(port, block):
    content = bytearray(32)
    content[0] = block % 256
    content[1] = block // 256
    content[2] = port
    for lane in range(3, 32):
        content[lane] = (71 * port + 17 * block + 43 * (block // 256) + 29 * lane) % 256
    return bytes(content)


def oracle_address_crc(offset):
    # Position contributions from libdragon's five-bit address CRC.
    contributions = (0x15, 0x1f, 0x0b, 0x16, 0x19, 0x07,
                     0x0e, 0x1c, 0x0d, 0x1a, 0x01)
    result = 0
    for bit in range(5, 16):
        if offset & (1 << bit):
            result ^= contributions[bit - 5]
    return result


def oracle_cases():
    for port in range(4):
        yield port, 0, 0
        for block in range(1024):
            yield port, block, 3
            yield port, block, 2
        for block in range(1024):
            yield port, block, 2


def expected_rows():
    for port, block, command in oracle_cases():
        data = bytearray([0xaa] * 64)
        data[:port] = bytes(port)
        tx, rx = {0: (1, 3), 2: (3, 33), 3: (35, 1)}[command]
        data[port:port + 3] = bytes((tx, rx, command))
        if command:
            address = block * 32 + oracle_address_crc(block * 32)
            data[port + 3:port + 5] = address.to_bytes(2, 'big')
        if command == 3:
            data[port + 5:port + 37] = oracle_payload(port, block)
        reply = port + 2 + tx
        if command == 0:
            data[reply:reply + 3] = bytes((5, 0, 3))
        elif command == 2:
            content = oracle_payload(port, block)
            data[reply:reply + 32] = content
            data[reply + 32] = data_crc(content)
        else:
            data[reply] = data_crc(oracle_payload(port, block))
        data[reply + rx] = 254
        data[63] = 0
        yield bytes(data)


def expected_banks():
    return b''.join(oracle_payload(port, block)
                    for port in range(4) for block in range(1024))


def evaluate(registers, banks, image):
    report = dict(status='error', suite=SUITE, expected_cases=CASES,
                  graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(image).hexdigest())
    try:
        with registers.open(encoding='ascii') as stream:
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() == f'BEGIN pak cases={CASES} words=8\n', 'wrong domain')
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
        actual_banks = banks.read_bytes()
        expected = expected_banks()
        report['pak_bytes'] = len(actual_banks)
        report['pak_mismatches'] = sum(a != b for a, b in zip(actual_banks, expected))
        require(len(actual_banks) == len(expected), 'wrong final Pak image extent')
        report['status'] = 'fail' if report['mismatches'] or report['pak_mismatches'] else 'pass'
    except (OSError, UnicodeError, ValueError) as error:
        report['detail'] = str(error)
    return report


def execute(capture, plugin, image, registers, banks):
    result = subprocess.run([str(capture.resolve()), str(plugin.resolve()),
                             str(image.resolve()), str(registers.resolve()),
                             str(banks.resolve())], cwd=ROOT,
                            capture_output=True, text=True, timeout=180)
    require(result.returncode == 0, result.stdout + result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=ROOT / 'build/bin/n64_controller_pak_capture')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'build/bin/cores/n64.so')
    parser.add_argument('--artifacts', type=Path,
                        default=ROOT / 'build/artifacts/owned/n64_controller_pak')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require((HERE / 'controller_pak.z64').read_bytes() == rom,
            'archived original cartridge differs from source')
    with tempfile.TemporaryDirectory(prefix='n64_pak_', dir=args.artifacts) as temporary:
        image = Path(temporary) / 'pak.z64'
        image.write_bytes(rom)
        registers = args.artifacts / 'first.result'
        banks = args.artifacts / 'first.paks'
        execute(args.capture, args.plugin, image, registers, banks)
        report = evaluate(registers, banks, rom)
        (args.artifacts / 'first.json').write_text(json.dumps(report, indent=2) + '\n')
        require(report['status'] == 'pass', json.dumps(report))
        mutant = build(wrong_capture=True)
        mutated_image = Path(temporary) / 'mutant.z64'
        mutated_image.write_bytes(mutant)
        bad_registers = args.artifacts / 'mutant.result'
        bad_banks = args.artifacts / 'mutant.paks'
        execute(args.capture, args.plugin, mutated_image, bad_registers, bad_banks)
        rejected = evaluate(bad_registers, bad_banks, mutant)
        require(rejected['status'] == 'fail' and rejected['mismatches'] == CASES and
                rejected['pak_mismatches'] == 0, 'executed wrong-register mutant escaped oracle')
        (args.artifacts / 'mutant.json').write_text(json.dumps(rejected, indent=2) + '\n')
        text = registers.read_text()
        row = next(line for line in text.splitlines() if line.startswith('REG r='))
        for name, damaged in {
            'missing-row': text.replace(row + '\n', '', 1),
            'bad-row': text.replace(row, 'REG r=invalid', 1),
            'false-end': text.replace('END observations_complete', 'END false'),
        }.items():
            path = args.artifacts / (name + '.result')
            path.write_text(damaged)
            require(evaluate(path, banks, rom)['status'] == 'error',
                    name + ' damaged register file passed')
    print(f'N64 Controller Pak PASS: {CASES} guest packets / {CASES * 8} '
          'register words, 131072 Pak bytes, executed mutant and damaged files')


if __name__ == '__main__':
    main()
