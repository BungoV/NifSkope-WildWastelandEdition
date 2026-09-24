"""Two synthetic COLOUR sheets, so the render's own colour response is measured
rather than guessed.

The lit render of vanilla's own shipped sheets comes out blue-purple even though
vanilla's colour sheet averages a warm brown (88.1/79.9/68.9). Either the colour
sheet barely reaches the pixel, or the viewer's lighting dominates it. A flat
grey sheet and a flat red sheet answer that directly: if the red arm's render
does not go red, the colour sheet is not what decides the hue.

The container is the same uncompressed B8G8R8A8 behind a DX10 header that
`lodgenWriteDdsBgra8` writes -- the header is COPIED from a sheet the
application itself wrote, so nothing here depends on my reading of the spec --
with the size fields and the mip chain rewritten for 512x512.
"""
import os
import struct
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sheet_probe import probe                                     # noqa: E402

SRC = os.path.join(HERE, 'bake', 'cache', 'tex', 'Commonwealth.4.-20.24_msn.DDS')
raw = open(SRC, 'rb').read()
head = bytearray(raw[:148])          # 'DDS ' + 124 header + 20 DX10 bytes


def write(path, rgb):
    n, w = 0, 512
    body = bytearray()
    lvl = np.zeros((512, 512, 4), np.uint8)
    lvl[:, :, 0] = rgb[2]            # B
    lvl[:, :, 1] = rgb[1]            # G
    lvl[:, :, 2] = rgb[0]            # R
    lvl[:, :, 3] = 255
    while True:
        body += lvl.tobytes()
        n += 1
        if w == 1:
            break
        w //= 2
        lvl = lvl[:w * 2:2, :w * 2:2] if False else np.zeros((w, w, 4), np.uint8)
        lvl[:, :, 0] = rgb[2]
        lvl[:, :, 1] = rgb[1]
        lvl[:, :, 2] = rgb[0]
        lvl[:, :, 3] = 255
    h = bytearray(head)
    struct.pack_into('<I', h, 4 + 8, 512)      # dwHeight
    struct.pack_into('<I', h, 4 + 12, 512)     # dwWidth
    struct.pack_into('<I', h, 4 + 16, 512 * 4)  # pitch
    struct.pack_into('<I', h, 4 + 24, n)       # dwMipMapCount
    open(path, 'wb').write(bytes(h) + bytes(body))
    o, _ = probe(path)
    print('%-10s %s %dx%d mips %d/%d %d B alpha %d..%d' %
          (os.path.basename(path), o['dxgi_name'], o['w'], o['h'],
           o['present_mips'], o['declared_mips'], o['bytes'],
           o['alpha_min'], o['alpha_max']))


VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
CH = 'Commonwealth.4.-20.24'
for name, rgb in (('grey', (128, 128, 128)), ('red', (255, 0, 0))):
    d = os.path.join(HERE, 'roots', 'syn_' + name, 'Textures', 'Terrain', 'Commonwealth')
    os.makedirs(d, exist_ok=True)
    write(os.path.join(d, CH + '.DDS'), rgb)
    for suf in ('_msn.DDS', '_data.DDS'):
        src = os.path.join(VAN, CH + suf) if suf == '_msn.DDS' else \
            os.path.join(HERE, 'bake', 'rung', 'tex', CH + suf)
        open(os.path.join(d, CH + suf), 'wb').write(open(src, 'rb').read())
