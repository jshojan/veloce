"""Independent source-pixel, fog feedback and framebuffer oracle."""


def expected(case):
    raw, (format_code, size, source), s, t = case
    assert size == 2 and (s, t) == (7, 23)
    if format_code == 0:
        channels = ((raw >> 11) & 31, (raw >> 6) & 31,
                    (raw >> 1) & 31)
        color = [value * 8 + value // 4 for value in channels]
        alpha = 255 if raw & 1 else 0
    else:
        color = [raw >> 8] * 3
        alpha = raw & 255
    # The textured triangle has no shade coefficients. Shade alpha is zero,
    # so BL0's fog/combiner equation forwards the combiner result exactly.
    fogged = color
    if alpha == 255:
        blended = fogged
    else:
        a, b = alpha // 8, (255 - alpha) // 8
        blended = [((value * a + 0x5a * (b + 1)) // 32) % 256
                   for value in fogged]
    memory = bytearray([0x5a] * 80)
    points = ((0, 0), (2, 0), (1, 1), (3, 1),
              (0, 2), (2, 2), (1, 3), (3, 3))
    for row in range(4):
        for x in range(4):
            covered = []
            for px, py in points:
                quarter = row * 4 + py
                right = quarter if quarter < 8 else 16 - quarter
                if x * 4 + px < right:
                    covered.append((px, py))
            if (0, 0) not in covered:
                continue
            count = len(covered)
            coverage = 7 if count + 2 >= 8 else count + 2
            at = 8 + (row * 4 + x) * 4
            memory[at:at + 4] = bytes(blended + [coverage * 32])
    words = [int.from_bytes(memory[8 + n:16 + n], 'big')
             for n in range(0, 64, 8)]
    words.extend((int.from_bytes(memory[:8], 'big'),
                  int.from_bytes(memory[72:80], 'big')))
    return tuple(words + [(0xff0 if source else 0x700100) + 168,
                          128 + source, 32])
