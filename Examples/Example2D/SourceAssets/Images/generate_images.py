"""Draws the images of the 2D example with plain Python (no libraries), so they are ours
and can be made again: python generate_images.py, from any folder."""

import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def write_png(name, width, height, pixel):
    """pixel(x, y) gives (r, g, b, a) in 0..255; y = 0 is the top row."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(bytes(max(0, min(255, int(c))) for c in pixel(x, y)))

    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))

    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(bytes(rows), 9))
    data += chunk(b'IEND', b'')
    with open(os.path.join(HERE, name), 'wb') as file:
        file.write(data)


CLEAR = (0, 0, 0, 0)


def inside_triangle(px, py, a, b, c):
    def side(p, q, r):
        return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1])
    p = (px, py)
    d1, d2, d3 = side(p, a, b), side(p, b, c), side(p, c, a)
    negative = d1 < 0 or d2 < 0 or d3 < 0
    positive = d1 > 0 or d2 > 0 or d3 > 0
    return not (negative and positive)


def ship(x, y):
    """The player's ship, pointing up: a cyan arrow with an orange engine."""
    cx, cy = x + 0.5, y + 0.5
    if inside_triangle(cx, cy, (16, 2), (3, 28), (29, 28)):
        if not inside_triangle(cx, cy, (16, 5), (6, 26.5), (26, 26.5)):
            return (20, 70, 110, 255)
        if abs(cx - 16) < 2.5 and 10 < cy < 17:
            return (230, 250, 255, 255)
        return (60, 200, 240, 255)
    if 12 <= x <= 19 and 28 <= y <= 31:
        heat = 1.0 - (y - 28) / 4.0
        return (255, 140 + 80 * heat, 40, 255)
    return CLEAR


def enemy_frame(frame):
    """A red saucer whose four lights take turns."""
    def pixel(x, y):
        cx, cy = x + 0.5 - 16, y + 0.5 - 16
        dome = (cx / 7.0) ** 2 + ((cy + 3) / 7.0) ** 2
        body = (cx / 14.0) ** 2 + ((cy - 3) / 6.0) ** 2
        if dome <= 1.0 and cy < 1:
            return (255, 200, 210, 255) if dome < 0.35 else (190, 120, 220, 255)
        if body <= 1.0:
            for light in range(4):
                lx = -9 + light * 6
                if (cx - lx) ** 2 + (cy - 4) ** 2 <= 2.3:
                    on = light == frame
                    return (255, 255, 120, 255) if on else (90, 40, 40, 255)
            if body > 0.75:
                return (100, 20, 30, 255)
            return (220, 50, 60, 255)
        return CLEAR
    return pixel


def enemy_sheet(x, y):
    return enemy_frame(x // 32)(x % 32, y)


def player_bullet(x, y):
    """Long along +x, the way the bullet flies."""
    cx, cy = x + 0.5 - 8, y + 0.5 - 4
    shape = (cx / 7.5) ** 2 + (cy / 3.2) ** 2
    if shape > 1.0:
        return CLEAR
    core = 1.0 - shape
    return (255, 230 + 25 * core, 120 + 135 * core, 255)


def glow(radius, inner, outer):
    def pixel(x, y):
        size = radius * 2
        cx, cy = x + 0.5 - size / 2, y + 0.5 - size / 2
        distance = math.sqrt(cx * cx + cy * cy) / radius
        if distance >= 1.0:
            return CLEAR
        t = distance
        color = [inner[i] * (1 - t) + outer[i] * t for i in range(3)]
        alpha = 255 * (1.0 - t ** 3)
        return (*color, alpha)
    return pixel


write_png('ship.png', 32, 32, ship)
write_png('enemy_sheet.png', 128, 32, enemy_sheet)
write_png('bullet_player.png', 16, 8, player_bullet)
write_png('bullet_enemy.png', 12, 12, glow(6, (255, 255, 230), (255, 90, 40)))
write_png('spark.png', 8, 8, glow(4, (255, 255, 255), (255, 170, 60)))
write_png('star.png', 4, 4, glow(2, (255, 255, 255), (180, 200, 255)))
print('Images written to', HERE)
