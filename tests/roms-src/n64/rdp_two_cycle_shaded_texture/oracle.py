"""Independent TEXEL0/SHADE combiner and two-cycle fog/blend oracle."""


def expected(case):
    raw, (format_code, size, source), s, t = case
    assert size == 2 and (s, t) == (7, 23)
    if format_code == 0:
        channels = ((raw >> 11) & 31, (raw >> 6) & 31,
                    (raw >> 1) & 31)
        texel = [value * 8 + value // 4 for value in channels]
        opacity = 255 if raw & 1 else 0
    else:
        texel = [raw >> 8] * 3
        opacity = raw & 255
    shade_alpha = ((raw >> 8) & 255) ^ 127
    fog = (0x17, 0xc9, 0x6b)
    # CC0 scales TEXEL0 by the constant shade RGB; CC1 forwards COMBINED.
    combined = [(value * 128 + 128) // 256 for value in texel]
    a, b = shade_alpha // 8, (255 - shade_alpha) // 8
    fogged = [((fog[lane] * a + combined[lane] * (b + 1)) // 32) % 256
              for lane in range(3)]
    if opacity == 255:
        blended = fogged
    else:
        a, b = opacity // 8, (255 - opacity) // 8
        blended = [((value * a + 0x5a * (b + 1)) // 32) % 256
                   for value in fogged]
    memory = bytearray([0x5a] * 80)
    # Quarter-pixel grid for the original 4x4 right triangle.
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
    return tuple(words + [(0xff0 if source else 0x700100) + 240,
                          128 + source, 32])
