"""One sheet, told apart from its own bytes: what format, how many mips, what
is in the alpha, and -- for an `_msn` -- whether the normal is unit length.

Used for gates F2 and F5. It reads the file, not the writer's intentions: the
mip count is the number of levels the BYTES hold, checked against the number
the header declares, and a mismatch is printed rather than swallowed.

BC1/BC3 decoding comes from dds_np.py (cross-checked against the tree's own
pure-Python reader in f1_corpus.py). Uncompressed B8G8R8A8 behind a DX10
header is decoded here, because nothing else in this lane writes one.

    python sheet_probe.py <file.dds> [more.dds ...]
"""
import os
import struct
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402

DXGI = {71: 'BC1_UNORM', 72: 'BC1_UNORM_SRGB', 77: 'BC3_UNORM',
        78: 'BC3_UNORM_SRGB', 87: 'B8G8R8A8_UNORM', 98: 'BC7_UNORM'}


def probe(path):
    raw = open(path, 'rb').read()
    if raw[:4] != b'DDS ':
        raise ValueError('not a DDS: %s' % path)
    h = struct.unpack_from('<31I', raw, 4)
    height, width, declared = h[2], h[3], h[6]
    fourcc = raw[84:88]
    out = {'file': os.path.basename(path), 'bytes': len(raw),
           'w': width, 'h': height, 'declared_mips': declared,
           'fourcc': fourcc.decode('latin1'),
           'reserved1': [int(v) for v in h[7:18]]}
    if fourcc == b'DX10':
        dxgi, dim, misc, arr, misc2 = struct.unpack_from('<5I', raw, 128)
        out['dxgi'] = dxgi
        out['dxgi_name'] = DXGI.get(dxgi, '?')
        out['array_size'] = arr
        if dxgi != 87:
            out['note'] = 'DX10 but not B8G8R8A8; not decoded here'
            return out, None
        off, w, hh, present, levels = 148, width, height, 0, []
        while True:
            n = w * hh * 4
            if off + n > len(raw):
                break
            levels.append((off, w, hh))
            off += n
            present += 1
            if w == 1 and hh == 1:
                break
            w, hh = max(1, w // 2), max(1, hh // 2)
        out['present_mips'] = present
        out['trailing_bytes'] = len(raw) - off
        o, w, hh = levels[0]
        a = np.frombuffer(raw, np.uint8, w * hh * 4, o).reshape(hh, w, 4)
        # B, G, R, A in memory order
        rgb = a[:, :, [2, 1, 0]]
        alpha = a[:, :, 3]
    else:
        d = Dds(path)
        out['present_mips'] = d.mips
        rgb = d.rgb(0)
        alpha = d.alpha(0)
        last = d.levels[-1]
        out['trailing_bytes'] = len(raw) - (last[0] + last[3] * last[4] * d.blockBytes)
    out['alpha_min'] = int(alpha.min())
    out['alpha_max'] = int(alpha.max())
    out['alpha_distinct'] = int(len(np.unique(alpha)))
    # R = east, G = up, B = north (src/lodgen.cpp lodgenTerrainMsnPixel)
    v = rgb.astype(np.float64) / 255.0 * 2.0 - 1.0
    L = np.sqrt((v * v).sum(axis=2))
    out['unit_len_mean'] = float(L.mean())
    out['unit_len_maxdev_levels'] = float(np.abs(L - 1.0).max() * 127.5)
    out['chan_mean'] = [float(rgb[:, :, k].mean()) for k in range(3)]
    return out, rgb


if __name__ == '__main__':
    for p in sys.argv[1:]:
        o, _ = probe(p)
        print('%-42s %-12s %5dx%-5d mips %2d/%-2d %10d B  alpha %d..%d (%d '
              'distinct)  |n| %.4f maxdev %.2f/255  RGB means %.1f/%.1f/%.1f'
              % (o['file'], o.get('dxgi_name', o['fourcc']), o['w'], o['h'],
                 o['present_mips'], o['declared_mips'], o['bytes'],
                 o['alpha_min'], o['alpha_max'], o['alpha_distinct'],
                 o['unit_len_mean'], o['unit_len_maxdev_levels'],
                 o['chan_mean'][0], o['chan_mean'][1], o['chan_mean'][2]))
        if o['trailing_bytes']:
            print('    TRAILING BYTES AFTER THE LAST MIP: %d' % o['trailing_bytes'])
        if any(o['reserved1']):
            print('    dwReserved1 non-zero: %s' % o['reserved1'][:2])
