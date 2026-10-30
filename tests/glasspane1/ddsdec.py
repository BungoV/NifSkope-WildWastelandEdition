"""GLASSPANE1: small numpy DDS decoder (BC1 / BC3 / BC4 / BC5 / B8G8R8A8 / R8G8B8A8), any face and mip.
decode(dds_bytes, mip=0, face=0) -> float array (h, w, 4) in 0..1 (BC5: R, G, 0, 1; BC4: R, R, R, 1)."""
import struct
import sys

import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/lanes/glasspane1/tests/spells')
from impostor_bc_decode import _bc1_colour, _bc3_alpha  # noqa: E402

FOURCC = {b'DXT1': 71, b'DXT3': 74, b'DXT5': 77, b'ATI1': 80, b'BC4U': 80, b'ATI2': 83, b'BC5U': 83}
BPB = {70: 8, 71: 8, 72: 8, 73: 16, 74: 16, 75: 16, 76: 16, 77: 16, 78: 16, 79: 8, 80: 8, 81: 8, 82: 16, 83: 16, 84: 16}


def facts(b):
    h, w = struct.unpack_from('<II', b, 12)
    mips = max(1, struct.unpack_from('<I', b, 28)[0])
    pf = struct.unpack_from('<I', b, 80)[0]
    fourcc = b[84:88]
    caps2 = struct.unpack_from('<I', b, 112)[0]
    cube = bool(caps2 & 0x200)
    off = 128
    if fourcc == b'DX10':
        dxgi = struct.unpack_from('<I', b, 128)[0]
        misc = struct.unpack_from('<I', b, 136)[0]
        cube = cube or bool(misc & 4)
        off = 148
    elif pf & 4:
        dxgi = FOURCC.get(fourcc, 0)
    else:
        bits = struct.unpack_from('<I', b, 88)[0]
        rmask = struct.unpack_from('<I', b, 92)[0]
        dxgi = (87 if rmask == 0x00ff0000 else 28) if bits == 32 else 0
    return dxgi, w, h, mips, cube, off


def level_size(dxgi, w, h):
    if dxgi in BPB:
        return max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * BPB[dxgi]
    if dxgi in (87, 88, 28, 29, 91):
        return w * h * 4
    raise ValueError('dxgi %d not handled' % dxgi)


def decode(b, mip=0, face=0):
    dxgi, w, h, mips, cube, off = facts(b)
    faces = 6 if cube else 1
    chain = sum(level_size(dxgi, max(1, w >> m), max(1, h >> m)) for m in range(mips))
    off += face * chain
    for m in range(mip):
        off += level_size(dxgi, max(1, w >> m), max(1, h >> m))
    lw, lh = max(1, w >> mip), max(1, h >> mip)
    n = level_size(dxgi, lw, lh)
    body = np.frombuffer(b[off:off + n], np.uint8)
    if dxgi in (87, 88, 28, 29, 91):
        a = body.reshape(lh, lw, 4).astype(np.float64) / 255.0
        if dxgi in (87, 88, 91):
            a = a[..., [2, 1, 0, 3]]
        return a
    bw, bh = max(1, (lw + 3) // 4), max(1, (lh + 3) // 4)
    blk = body.reshape(bh * bw, BPB[dxgi])
    out = np.ones((bh * bw, 4, 4, 4))
    if dxgi in (70, 71, 72):
        out[..., :3] = _bc1_colour(blk)
    elif dxgi in (76, 77, 78):
        out[..., :3] = _bc1_colour(blk[:, 8:])
        out[..., 3] = _bc3_alpha(blk[:, :8])
    elif dxgi in (79, 80, 81):
        r = _bc3_alpha(blk)
        out[..., 0] = r
        out[..., 1] = r
        out[..., 2] = r
    elif dxgi in (82, 83, 84):
        out[..., 0] = _bc3_alpha(blk[:, :8])
        out[..., 1] = _bc3_alpha(blk[:, 8:])
        out[..., 2] = 0.0
    else:
        raise ValueError('dxgi %d not decoded' % dxgi)
    img = out.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return img[:lh, :lw]


def srgb2lin(c):
    c = np.asarray(c, np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
