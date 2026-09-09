"""Write an UNCOMPRESSED A8R8G8B8 DDS with a full mip chain.

Only used to build a CONTROL: our own `_msn` content with the block codec taken
out of it, so a render can say whether what is left of the square pattern is the
compressor or the data.  Nothing in the generator writes this format.
"""
import struct

import numpy as np

DDSD = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000        # CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT
DDPF_RGB = 0x40
DDPF_ALPHAPIXELS = 0x1


def mipchain(rgb):
    """Box-downsample the NORMALS (not the bytes), renormalising at each level,
    which is what a normal map's mip chain has to do."""
    n = np.asarray(rgb, dtype=np.float64) / 255.0 * 2.0 - 1.0
    out = []
    while True:
        v = n / np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-9)
        out.append(np.clip(np.rint((v * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8))
        h, w, _ = n.shape
        if h == 1 or w == 1:
            break
        n = 0.25 * (n[0:h:2, 0:w:2] + n[1:h:2, 0:w:2]
                    + n[0:h:2, 1:w:2] + n[1:h:2, 1:w:2])
    return out


def write(path, rgb):
    mips = mipchain(rgb)
    h, w, _ = mips[0].shape
    hdr = bytearray(128)
    hdr[0:4] = b'DDS '
    struct.pack_into('<I', hdr, 4, 124)
    struct.pack_into('<I', hdr, 8, DDSD)
    struct.pack_into('<I', hdr, 12, h)
    struct.pack_into('<I', hdr, 16, w)
    struct.pack_into('<I', hdr, 20, w * 4)               # pitch
    struct.pack_into('<I', hdr, 24, 0)                   # depth
    struct.pack_into('<I', hdr, 28, len(mips))
    struct.pack_into('<I', hdr, 76, 32)                  # pixel format size
    struct.pack_into('<I', hdr, 80, DDPF_RGB | DDPF_ALPHAPIXELS)
    struct.pack_into('<I', hdr, 84, 0)                   # fourCC = 0
    struct.pack_into('<I', hdr, 88, 32)                  # bit count
    struct.pack_into('<I', hdr, 92, 0x00FF0000)          # R
    struct.pack_into('<I', hdr, 96, 0x0000FF00)          # G
    struct.pack_into('<I', hdr, 100, 0x000000FF)         # B
    struct.pack_into('<I', hdr, 104, 0xFF000000)         # A
    struct.pack_into('<I', hdr, 108, 0x1000 | 0x8 | 0x400000)   # TEXTURE|COMPLEX|MIPMAP
    with open(path, 'wb') as f:
        f.write(bytes(hdr))
        for m in mips:
            a = np.dstack([m[:, :, 2], m[:, :, 1], m[:, :, 0],
                           np.full(m.shape[:2], 255, np.uint8)])   # BGRA
            f.write(a.astype(np.uint8).tobytes())
    return path
