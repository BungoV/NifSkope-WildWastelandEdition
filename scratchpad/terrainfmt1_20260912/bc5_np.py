"""BC5U (`ATI2`) reader -- the format every Fallout 4 landscape `_n` ships in.

A BC5 block is two BC4 blocks back to back, X then Y, and a BC4 block is
bit-for-bit a BC3 alpha block. The decode below is therefore the SAME
arithmetic dds_np.Dds.alpha() uses, applied at byte offsets 0 and 8 of each
16-byte block; it was checked against that function by running it over a DXT5
sheet's alpha bytes and requiring equality.

Only what gate F3 needs: any mip level, X and Y planes, no Z reconstruction.
"""
import struct
import numpy as np


def _bc4(raw, off, bw, bh, stride, sub):
    blk = np.frombuffer(raw, np.uint8, bw * bh * stride, off)
    blk = blk.reshape(bh, bw, stride)[:, :, sub:sub + 8]
    a0 = blk[:, :, 0].astype(np.int32)
    a1 = blk[:, :, 1].astype(np.int32)
    bits = np.zeros((bh, bw), np.uint64)
    for k in range(6):
        bits |= blk[:, :, 2 + k].astype(np.uint64) << np.uint64(8 * k)
    idx = np.zeros((bh, bw, 16), np.uint8)
    for i in range(16):
        idx[:, :, i] = ((bits >> np.uint64(3 * i)) & np.uint64(7)).astype(np.uint8)
    pal = np.zeros((bh, bw, 8), np.int32)
    pal[:, :, 0], pal[:, :, 1] = a0, a1
    gt = a0 > a1
    for k in range(1, 7):
        pal[:, :, k + 1] = np.where(gt, ((7 - k) * a0 + k * a1) // 7, 0)
    for k in range(1, 5):
        pal[:, :, k + 1] = np.where(gt, pal[:, :, k + 1],
                                    ((5 - k) * a0 + k * a1) // 5)
    pal[:, :, 6] = np.where(gt, pal[:, :, 6], 0)
    pal[:, :, 7] = np.where(gt, pal[:, :, 7], 255)
    out = np.take_along_axis(pal, idx.astype(np.int64), axis=2).astype(np.uint8)
    return out.reshape(bh, bw, 4, 4).transpose(0, 2, 1, 3).reshape(bh * 4, bw * 4)


class Bc5(object):
    def __init__(self, path):
        self.raw = open(path, 'rb').read()
        if self.raw[:4] != b'DDS ':
            raise ValueError('not a DDS: %s' % path)
        h = struct.unpack_from('<31I', self.raw, 4)
        self.height, self.width, self.declaredMips = h[2], h[3], h[6]
        self.fourcc = self.raw[84:88]
        if self.fourcc not in (b'ATI2', b'BC5U'):
            raise ValueError('not BC5: %r' % self.fourcc)
        off, w, hh, self.levels = 128, self.width, self.height, []
        for _ in range(max(1, self.declaredMips)):
            bw, bh = max(1, (w + 3) // 4), max(1, (hh + 3) // 4)
            n = bw * bh * 16
            if off + n > len(self.raw):
                break
            self.levels.append((off, w, hh, bw, bh))
            off += n
            w, hh = max(1, w // 2), max(1, hh // 2)
        self.mips = len(self.levels)

    def xy(self, m=0):
        """X and Y planes of mip m as uint8 (h, w, 2)."""
        off, w, h, bw, bh = self.levels[m]
        x = _bc4(self.raw, off, bw, bh, 16, 0)[:h, :w]
        y = _bc4(self.raw, off, bw, bh, 16, 8)[:h, :w]
        return np.stack([x, y], -1)
