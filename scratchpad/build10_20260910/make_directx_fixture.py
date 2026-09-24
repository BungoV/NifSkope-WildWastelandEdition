# -*- coding: utf-8 -*-
"""make_directx_fixture.py -- write `tests/fixtures/flowmap_directx_4x4.png`.

Lane WATER6 (BUILD10).  This is gate X5c's INPUT, and its whole value is that
it shares no code with NifSkope's own codec: it is written here from the RULE
as the rule is written down, so a codec that round-trips itself perfectly and
is nevertheless wrong still fails X5c.

THE RULE (the DirectX normal-map convention, as the director stated it):

    R = +X, centred on 128:      R = 128 + round(127 * cos(theta))
    G = +Y toward the image BOTTOM, centred on 128:
                                 G = 128 + round(127 * -sin(theta))
    B = the speed nibble  * 17
    A = the confidence nibble * 17, floored at 1 on a wet texel

where theta is the flow direction in WORLD coordinates: theta = 0 is +X (east)
and theta = 90 degrees is +Y (north).  So east is (255, 128), north is
(128, 1) -- green DARK -- and south is (128, 255) -- green BRIGHT.

The image is 4 x 4 and carries the sixteen directions 0, 16, 32, ... 240 in
reading order, row 0 being the image's TOP row.  Speed and confidence are 15
everywhere (B = A = 255) so the picture tests only the two channels under
test.  The same sixteen numbers are `kDirectXFixtureDirs` in
`src/watermark.cpp`, and the table below is printed so the report can quote it.
"""
import math
import os
import struct
import zlib

OUT = os.path.join(r'E:\Projects\NifskopeWildWastelandEdition',
                   'tests', 'fixtures', 'flowmap_directx_4x4.png')
DIRS = [i * 16 for i in range(16)]
TWO_PI = 2.0 * math.pi


def rgba(direction):
    theta = direction / 256.0 * TWO_PI
    r = 128 + int(round(127.0 * math.cos(theta)))
    g = 128 + int(round(127.0 * -math.sin(theta)))
    return max(0, min(255, r)), max(0, min(255, g)), 255, 255


rows = []
print('dir  R    G    B    A   what')
for row in range(4):
    line = bytearray()
    for col in range(4):
        d = DIRS[row * 4 + col]
        r, g, b, a = rgba(d)
        name = {0: 'east', 64: 'north', 128: 'west', 192: 'south'}.get(d, '')
        print('%3d  %3d  %3d  %3d  %3d  %s' % (d, r, g, b, a, name))
        line += bytes((r, g, b, a))
    rows.append(bytes(line))

# a PNG written by hand: no image library decides anything about this file
raw = b''.join(b'\x00' + r for r in rows)


def chunk(tag, data):
    return (struct.pack('>I', len(data)) + tag + data
            + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))


png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw, 9))
       + chunk(b'IEND', b''))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, 'wb').write(png)
print('\nwrote %s (%d bytes)' % (OUT, len(png)))

# the refuter for this script itself: read the file back and re-derive
try:
    from PIL import Image
    im = Image.open(OUT).convert('RGBA')
    assert im.size == (4, 4)
    bad = 0
    for row in range(4):
        for col in range(4):
            want = rgba(DIRS[row * 4 + col])
            got = im.getpixel((col, row))
            if tuple(got) != tuple(want):
                bad += 1
                print('  MISMATCH row %d col %d: %s against %s' % (row, col, got, want))
    print('read back with an independent decoder: %d of 16 wrong' % bad)
except ImportError:
    print('PIL not available; the file was not read back')
