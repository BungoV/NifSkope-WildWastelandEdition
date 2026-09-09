"""Vectorised numpy BC1/BC3 colour decode, for whole-worldspace sweeps.

dds.py is the trusted reference decoder but it loops in Python over every
texel, which is far too slow for 2,304 tiles.  This decodes an entire mip's
block array at once.  It decodes ONLY the RGB channels (the terrain LOD
diffuse alpha is a measured constant 255 and carries nothing).

rec_decode_check.py asserts this agrees with dds.py texel-for-texel.
"""
import numpy as np

from dds import DDS


def decode_rgb(dds, mip):
    """Decode mip `mip` of a BC1/BC3 DDS to a (h, w, 3) uint8 array."""
    w, h, off, size = dds.mip(mip)
    assert w % 4 == 0 and h % 4 == 0, 'padded mip %dx%d not supported' % (w, h)
    bw, bh = w // 4, h // 4
    bb = dds.blockBytes
    buf = np.frombuffer(dds.raw, dtype=np.uint8, count=size, offset=off)
    blocks = buf.reshape(bh * bw, bb)
    cstart = 8 if dds.fmt.startswith('BC3') else 0     # BC3 puts colour after alpha

    c = blocks[:, cstart:cstart + 4].view(np.uint16).reshape(-1, 2).astype(np.uint32)
    bits = blocks[:, cstart + 4:cstart + 8].copy().view(np.uint32).reshape(-1)

    # 565 -> 888 with the standard bit-replication rounding dds.py uses
    r = ((c >> 11) & 31) * 527 + 23
    g = ((c >> 5) & 63) * 259 + 33
    b = (c & 31) * 527 + 23
    ends = np.stack([r >> 6, g >> 6, b >> 6], axis=2).astype(np.int32)  # (N,2,3)

    n = ends.shape[0]
    pal = np.empty((n, 4, 3), dtype=np.int32)
    pal[:, 0] = ends[:, 0]
    pal[:, 1] = ends[:, 1]
    if dds.fmt.startswith('BC1'):
        # punch-through mode when c0 <= c1
        opaque = (c[:, 0] > c[:, 1])[:, None]
        thirds = np.stack([(2 * ends[:, 0] + ends[:, 1]) // 3,
                           (ends[:, 0] + 2 * ends[:, 1]) // 3], axis=1)
        halfs = np.stack([(ends[:, 0] + ends[:, 1]) // 2,
                          np.zeros_like(ends[:, 0])], axis=1)
        pal[:, 2:] = np.where(opaque[:, :, None], thirds, halfs)
    else:
        pal[:, 2] = (2 * ends[:, 0] + ends[:, 1]) // 3
        pal[:, 3] = (ends[:, 0] + 2 * ends[:, 1]) // 3

    shifts = (2 * np.arange(16, dtype=np.uint32))
    idx = ((bits[:, None] >> shifts[None, :]) & np.uint32(3)).astype(np.intp)  # (N,16)
    px = np.take_along_axis(pal, idx[:, :, None].repeat(3, axis=2), axis=1)    # (N,16,3)

    img = px.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)
    return img.astype(np.uint8)


def open_decode(path, mip):
    return decode_rgb(DDS(path), mip)
