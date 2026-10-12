"""Independent CC1 and two-cycle PASS blender oracle for guest RAM rows."""


def channels(word):
    return tuple((word >> shift) & 255 for shift in (24, 16, 8, 0))


def expected(case):
    raw, (profile, source) = case
    seed, opacity = divmod(raw, 256)
    primitive = (seed, seed ^ 255, seed * 17 % 256, opacity)
    shade_alpha = seed ^ 127
    blend = channels(0xe143297f)
    shade_rgb = (100, 200, 50)
    # CC0 forwards primitive RGBA; CC1 modulates its RGB with shade.
    # BL0 PASS forwards the resultant CC1 RGB into BL1's source zero.
    passed = [((primitive[lane] * shade_rgb[lane] + 128) // 256) % 256
              for lane in range(3)]
    memory = bytearray([0x5a] * 80)
    # Original fixed triangle covers this explicit quarter-pixel point grid.
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
            count = len(covered)
            if (0, 0) not in covered:
                continue
            second_alpha = opacity if profile == 0 else shade_alpha
            second_color = (0x5a, 0x5a, 0x5a) if profile == 0 else blend
            if profile == 0 and opacity == 255:
                rgb = passed
            else:
                a, b = second_alpha // 8, (255 - second_alpha) // 8
                rgb = [((passed[lane] * a + second_color[lane] * (b + 1)) // 32) % 256
                       for lane in range(3)]
            # Force blend with 0x5a background coverage (two eighths).
            coverage = 7 if count + 2 >= 8 else count + 2
            at = 8 + (row * 4 + x) * 4
            memory[at:at + 4] = bytes(rgb + [coverage * 32])
    words = [int.from_bytes(memory[8 + n:16 + n], 'big') for n in range(0, 64, 8)]
    words.extend((int.from_bytes(memory[:8], 'big'),
                  int.from_bytes(memory[72:80], 'big')))
    return tuple(words + [(0xff0 if source else 0x700100) + 160,
                          128 + source, 32])
