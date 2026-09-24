"""BC3/BC1 decode. Same as tests/spells/impostor_bc_decode.py EXCEPT the BC3
alpha ramp, which that file gets wrong -- see report section 0c."""
import numpy as np, struct
from bcdec import _bc1_colour          # the colour block is correct there

def _bc3_alpha(blk):
    n = blk.shape[0]
    a0 = blk[:, 0].astype(np.float64); a1 = blk[:, 1].astype(np.float64)
    pal = np.zeros((n, 8)); pal[:, 0] = a0; pal[:, 1] = a1
    big = a0 > a1
    for k in range(1, 7):                       # a2..a7, EIGHT-level mode
        pal[:, k + 1] = np.where(big, ((7 - k) * a0 + k * a1) / 7, 0.0)
    for k in range(1, 5):                       # a2..a5, SIX-level mode
        pal[:, k + 1] = np.where(big, pal[:, k + 1], ((5 - k) * a0 + k * a1) / 5)
    pal[:, 6] = np.where(big, pal[:, 6], 0.0)
    pal[:, 7] = np.where(big, pal[:, 7], 255.0)
    bits = np.zeros(n, np.uint64)
    for i in range(6): bits |= blk[:, 2 + i].astype(np.uint64) << np.uint64(8 * i)
    idx = np.stack([((bits >> np.uint64(3 * k)) & np.uint64(7)).astype(np.int64) for k in range(16)], -1)
    return np.take_along_axis(pal, idx, 1).reshape(n, 4, 4) / 255.0

def load_dds(path):
    b = open(path, 'rb').read(); assert b[:4] == b'DDS '
    h = struct.unpack('<31I', b[4:128]); ht, wd = h[2], h[3]; fourcc = b[84:88]
    bw, bh = (wd + 3) // 4, (ht + 3) // 4
    if fourcc == b'DXT5':
        d = np.frombuffer(b[128:128 + bw * bh * 16], np.uint8).reshape(-1, 16)
        img = np.concatenate([_bc1_colour(d[:, 8:]), _bc3_alpha(d[:, :8])[..., None]], -1)
    elif fourcc == b'DXT1':
        d = np.frombuffer(b[128:128 + bw * bh * 8], np.uint8).reshape(-1, 8)
        C = _bc1_colour(d); img = np.concatenate([C, np.ones(C.shape[:3] + (1,))], -1)
    else: raise RuntimeError('fourcc ' + repr(fourcc))
    img = img.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return img[:ht, :wd], (wd, ht), fourcc
