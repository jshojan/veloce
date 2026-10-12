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
        yield port, 0, 0, 'status'
        for block in range(40):
            yield port, block, 2, 'fresh'
        note_blocks = tuple(range(40, 48)) + (8, 16, 24)
        for block in note_blocks:
            yield port, block, 3, 'note'
        for block in note_blocks:
            yield port, block, 2, 'note'
        for block in range(1024):
            yield port, block, 3, 'bulk'
            yield port, block, 2, 'bulk'
        for block in range(1024):
            yield port, block, 2, 'bulk'


def fresh_pak(port):
    pak = bytearray(32768)
    for sector in (1, 3, 4, 6):
        at = sector * 32
        pak[at + 1] = 0x21 + port
        pak[at + 4:at + 8] = b'VLO' + bytes((0x30 + port,))
        pak[at + 8:at + 12] = b'PAK' + bytes((0x31 + port,))
        pak[at + 0x19:at + 0x1b] = b'\x01\x01'
        words = [int.from_bytes(pak[at + i:at + i + 2], 'big')
                 for i in range(0, 28, 2)]
        pak[at + 28:at + 30] = (sum(words) & 0xffff).to_bytes(2, 'big')
        pak[at + 30:at + 32] = (sum((~word) & 0xffff for word in words) & 0xffff).to_bytes(2, 'big')
    for page in (1, 2):
        for slot in range(5, 128):
            pak[page * 256 + slot * 2 + 1] = 3
        pak[page * 256 + 1] = (123 * 3) & 255
    return pak


def oracle_note_data(port, block):
    if 40 <= block <= 47:
        return bytes((7 * block + 19 * lane + 23 * port) % 256 for lane in range(32))
    if block == 8 or block == 16:
        data = bytearray(32)
        for slot in range(5, 16):
            data[slot * 2 + 1] = 3
        data[11] = 1
        data[1] = (sum(data[slot * 2 + 1] for slot in range(5, 16)) + 3 * 112) % 256
        return bytes(data)
    if block == 24:
        data = bytearray(32)
        data[0:3] = b'VLO'
        data[3] = ord('E')
        data[4:6] = b'PK'
        data[7] = 5
        data[8:10] = b'\x02\x03'
        data[12] = 0x1a
        data[16:20] = bytes(0x1a + ord(letter) - ord('A') for letter in 'TEST')
        return bytes(data)
    raise ValueError(block)


def validate_fresh_system(system):
    # Libdragon's validate_mempak path checks four header copies and at least
    # one valid inode table; also require both tables and 123 free pages.
    require(len(system) == 1280, 'incomplete fresh Pak system area')
    headers = [system[sector * 32:(sector + 1) * 32] for sector in (1, 3, 4, 6)]
    require(all(header[:16] == headers[0][:16] for header in headers),
            'Pak header copies disagree')
    for header in headers:
        checksum = sum(int.from_bytes(header[i:i + 2], 'big')
                       for i in range(0, 28, 2)) & 0xffff
        stored = int.from_bytes(header[28:30], 'big')
        inverted = int.from_bytes(header[30:32], 'big')
        require(stored == checksum and (stored + inverted) & 0xffff == 0xfff2,
                'Pak header checksum invalid')
        require(header[0x19] == 1 and header[0x1a] == 1,
                'Pak device ID or bank count invalid')
    for page in (1, 2):
        inode = system[page * 256:(page + 1) * 256]
        entries = [inode[slot * 2 + 1] for slot in range(5, 128)]
        require(all(value == 3 for value in entries) and
                inode[1] == sum(entries) & 255,
                'Pak inode table is not a valid empty 123-page filesystem')
    require(all(value == 0 for value in system[3 * 256:]),
            'unexpected note-directory bytes in fresh Pak')


def validate_note(port, observed):
    require(set(observed) == set(range(40, 48)) | {8, 16, 24},
            'incomplete guest note readback')
    for block in (8, 16):
        inode = fresh_pak(port)[(block // 8) * 256:(block // 8 + 1) * 256]
        inode[:32] = observed[block]
        links = [inode[slot * 2 + 1] for slot in range(5, 128)]
        require(inode[1] == sum(links) & 255 and links[0] == 1 and
                all(link == 3 for link in links[1:]),
                'one-page note inode chain or free-space count invalid')
    note = observed[24]
    require(note[0:3] == b'VLO' and note[3] == 0x45 and
            note[4:6] == b'PK' and int.from_bytes(note[6:8], 'big') == 5 and
            note[8:10] == b'\x02\x03' and note[12] == 0x1a and
            note[16:20] == bytes(0x1a + ord(letter) - ord('A') for letter in 'TEST'),
            'note directory entry invalid')
    for block in range(40, 48):
        require(observed[block] == oracle_note_data(port, block),
                'guest note data page mismatch')


def expected_rows():
    media = [fresh_pak(port) for port in range(4)]
    for port, block, command, phase in oracle_cases():
        data = bytearray([0xaa] * 64)
        data[:port] = bytes(port)
        tx, rx = {0: (1, 3), 2: (3, 33), 3: (35, 1)}[command]
        data[port:port + 3] = bytes((tx, rx, command))
        if command:
            address = block * 32 + oracle_address_crc(block * 32)
            data[port + 3:port + 5] = address.to_bytes(2, 'big')
        if command == 3:
            content = oracle_note_data(port, block) if phase == 'note' else oracle_payload(port, block)
            data[port + 5:port + 37] = content
            media[port][block * 32:(block + 1) * 32] = content
        reply = port + 2 + tx
        if command == 0:
            data[reply:reply + 3] = bytes((5, 0, 3))
        elif command == 2:
            content = media[port][block * 32:(block + 1) * 32]
            data[reply:reply + 32] = content
            data[reply + 32] = data_crc(content)
        else:
            data[reply] = data_crc(content)
        data[reply + rx] = 254
        data[63] = 0
        yield bytes(data), (port, block, reply, phase) if phase in ('fresh', 'note') and command == 2 else None


def expected_banks():
    return b''.join(oracle_payload(port, block)
                    for port in range(4) for block in range(1024))


def evaluate(registers, banks, image):
    report = dict(status='error', suite=SUITE, expected_cases=CASES,
                  graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(image).hexdigest())
    try:
        fresh_system = [bytearray(1280) for _ in range(4)]
        notes = [{} for _ in range(4)]
        with registers.open(encoding='ascii') as stream:
            require(stream.readline() == f'VELOCE 1 n64 {SUITE}\n', 'wrong suite')
            require(stream.readline() == f'BEGIN pak cases={CASES} words=8\n', 'wrong domain')
            for index, (want, fresh) in enumerate(expected_rows()):
                line = stream.readline()
                require(re.fullmatch(r'REG r=[0-9a-f]{128}\n', line),
                        f'malformed guest register row {index}')
                got = bytes.fromhex(line[6:134])
                if fresh:
                    port, block, reply, phase = fresh
                    if phase == 'fresh':
                        fresh_system[port][block * 32:(block + 1) * 32] = got[reply:reply + 32]
                    else:
                        notes[port][block] = got[reply:reply + 32]
                report['graded_cases'] += 1
                if got != want:
                    report['mismatches'] += 1
                    if len(report['examples']) < 5:
                        report['examples'].append(dict(index=index,
                                                       expected=want.hex(), observed=got.hex()))
            require(stream.readline() == f'LOG complete cases={CASES}\n', 'missing count')
            require(stream.readline() == 'END observations_complete\n', 'missing terminator')
            require(not stream.read(), 'trailing register data')
        try:
            for system in fresh_system:
                validate_fresh_system(system)
            report['formatted_paks'] = 4
        except ValueError as error:
            report['formatted_paks'] = 0
            report['format_error'] = str(error)
        try:
            for port, note in enumerate(notes):
                validate_note(port, note)
            report['notes'] = 4
        except ValueError as error:
            report['notes'] = 0
            report['note_error'] = str(error)
        actual_banks = banks.read_bytes()
        expected = expected_banks()
        report['pak_bytes'] = len(actual_banks)
        report['pak_mismatches'] = sum(a != b for a, b in zip(actual_banks, expected))
        require(len(actual_banks) == len(expected), 'wrong final Pak image extent')
        report['status'] = ('fail' if report['mismatches'] or report['pak_mismatches'] or
                            report['formatted_paks'] != 4 or report['notes'] != 4 else 'pass')
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
