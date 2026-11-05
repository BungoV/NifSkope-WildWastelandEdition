"""Lane FARWATER4: an own decode of a v3/v4 .lodl's body-id and flow planes (plane container: 16-byte header
tilesX, tilesY, tile edge, bytes a sample; u64 directory offset; per tile u64 offset, u32 compressed size (0 =
uniform, the u32 is the value), u32 size; zlib tiles), plus F5's instrument over a body.
Flow word: bits 0..7 direction, 8..11 speed (x |body mean flow| / 8), 12..15 confidence (8 = writer-solved)."""
import struct
import zlib
import numpy as np


class Lodl:
    def __init__(self, path):
        f = self.f = open(path, 'rb')
        h = f.read(0x100)
        self.version = struct.unpack_from('<I', h, 4)[0]
        assert h[:4] == b'LODT' and self.version in (3, 4), 'not a v3/v4 .lodl: ' + path
        x0, y0, x1, y1, _spc = struct.unpack_from('<iiiiI', h, 8)
        self.cx, self.cy = x1 - x0 + 1, y1 - y0 + 1
        (_oBody, self.nBody, _recB, _oN, _nN, self.bodyS, self.oBodyP, self.flowS, _enc,
         self.oFlow) = struct.unpack_from('<QIIQIIQIIQ', h, 0xA0)
        self.ids = self.plane(self.oBodyP, 2)
        self.flow = self.plane(self.oFlow, 2)
        f.close()

    def plane(self, off, bps):
        f = self.f
        f.seek(off)
        tX, tY, te, b = struct.unpack('<IIII', f.read(16))
        dOff, _ = struct.unpack('<QQ', f.read(16))
        assert b == bps and tX == self.cx and tY == self.cy
        f.seek(dOff)
        dirb = f.read(tX * tY * 16)
        dt = {1: np.uint8, 2: '<u2', 4: '<u4'}[b]
        full = np.zeros((tY * te, tX * te), dtype=np.uint32)
        for j in range(tY):
            for i in range(tX):
                o, cs, us = struct.unpack_from('<QII', dirb, (j * tX + i) * 16)
                if cs == 0:
                    full[j * te:(j + 1) * te, i * te:(i + 1) * te] = us
                else:
                    f.seek(o)
                    raw = zlib.decompress(f.read(cs))
                    full[j * te:(j + 1) * te, i * te:(i + 1) * te] = np.frombuffer(raw, dtype=dt).reshape(te, te)
        return full


def f5(m, d):
    """F5's instrument: 99th percentile of the 4-neighbour angle difference over pairs inside `m` of the direction
    byte `d`, the share of pairs over 10 degrees (seams), the pair count"""
    qs = []
    for (a, b, ma) in ((d[:, :-1], d[:, 1:], m[:, :-1] & m[:, 1:]), (d[:-1, :], d[1:, :], m[:-1, :] & m[1:, :])):
        q = np.abs(a.astype(np.int64) - b.astype(np.int64))
        q = np.minimum(q, 256 - q) * 360.0 / 256.0
        qs.append(q[ma])
    q = np.sort(np.concatenate(qs))
    if not len(q):
        return 0.0, 0.0, 0
    return float(q[min(len(q) - 1, int(len(q) * 0.99))]), float((q > 10).mean()), len(q)


def rivers(L, min_texels=1000):
    """[(body id, y0, y1, x0, x1, solved mask in that box)] of the writer-solved rivers of >= min_texels"""
    w = L.flow
    solved = (((w >> 12) & 0xF) == 8) & (L.ids > 0)
    out = []
    for b in np.unique(L.ids[solved]):
        ys, xs = np.nonzero((L.ids == b) & solved)
        if len(ys) < min_texels:
            continue
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        m = (L.ids[y0:y1, x0:x1] == b) & solved[y0:y1, x0:x1]
        out.append((int(b), y0, y1, x0, x1, m))
    return out
