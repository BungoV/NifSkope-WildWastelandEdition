"""Honest DDS reader for FO4 terrain LOD: full BC1/BC3/BC5/BC7-header parse,
real block decode with indices, per-mip means. No PIL.

Usage:
  python dds.py info  <file>
  python dds.py mean  <file> [mipIndex]     # decode that mip fully, print mean
  python dds.py dump  <file> <mipIndex> <out.ppm>
"""
import struct, sys, os, colorsys

DXGI = {
    71: 'BC1_UNORM', 72: 'BC1_UNORM_SRGB',
    74: 'BC2_UNORM', 75: 'BC2_UNORM_SRGB',
    77: 'BC3_UNORM', 78: 'BC3_UNORM_SRGB',
    80: 'BC4_UNORM', 81: 'BC4_SNORM',
    83: 'BC5_UNORM', 84: 'BC5_SNORM',
    98: 'BC7_UNORM', 99: 'BC7_UNORM_SRGB',
    28: 'R8G8B8A8_UNORM', 29: 'R8G8B8A8_UNORM_SRGB',
}

class DDS:
    def __init__(self, path):
        b = open(path, 'rb').read()
        assert b[:4] == b'DDS ', 'not a DDS: %r' % b[:4]
        self.raw = b
        self.path = path
        self.height, self.width = struct.unpack_from('<II', b, 12)
        self.mips = struct.unpack_from('<I', b, 28)[0] or 1
        pfFlags = struct.unpack_from('<I', b, 80)[0]
        self.fourcc = b[84:88]
        self.dx10 = None
        self.dataOff = 128
        if self.fourcc == b'DX10':
            self.dxgi, resDim, misc, arr, misc2 = struct.unpack_from('<IIIII', b, 128)
            self.dataOff = 148
            self.arraySize = arr
            self.fmt = DXGI.get(self.dxgi, 'DXGI_%d' % self.dxgi)
        else:
            self.dxgi = None
            self.arraySize = 1
            m = {b'DXT1': 'BC1_UNORM', b'DXT3': 'BC2_UNORM', b'DXT5': 'BC3_UNORM',
                 b'ATI2': 'BC5_UNORM', b'BC5U': 'BC5_UNORM', b'ATI1': 'BC4_UNORM'}
            if self.fourcc in m:
                self.fmt = m[self.fourcc]
            elif pfFlags & 0x40:
                self.fmt = 'RGB%d' % struct.unpack_from('<I', b, 88)[0]
            else:
                self.fmt = 'FOURCC_%r' % self.fourcc
        self.blockBytes = {'BC1_UNORM': 8, 'BC1_UNORM_SRGB': 8, 'BC4_UNORM': 8, 'BC4_SNORM': 8}.get(self.fmt, 16)
        if self.fmt.startswith('RGB') or self.fmt.startswith('R8G8'):
            self.blockBytes = None

    def mip(self, i):
        """(w, h, offset, size) of mip i in slice 0."""
        off = self.dataOff
        for j in range(self.mips):
            mw, mh = max(1, self.width >> j), max(1, self.height >> j)
            if self.blockBytes:
                sz = max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4) * self.blockBytes
            else:
                sz = mw * mh * 4
            if j == i:
                return mw, mh, off, sz
            off += sz
        raise IndexError(i)

    def sliceBytes(self):
        tot = 0
        for j in range(self.mips):
            mw, mh = max(1, self.width >> j), max(1, self.height >> j)
            tot += max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4) * self.blockBytes
        return tot

    def decode(self, i=0):
        """Decode mip i to a list of (r,g,b,a) rows -> flat bytearray RGBA."""
        mw, mh, off, sz = self.mip(i)
        out = bytearray(mw * mh * 4)
        bw, bh = max(1, (mw + 3) // 4), max(1, (mh + 3) // 4)
        bb = self.blockBytes
        raw = self.raw
        isBC1 = self.fmt.startswith('BC1')
        isBC3 = self.fmt.startswith('BC3')
        isBC5 = self.fmt.startswith('BC5')
        for by in range(bh):
            for bx in range(bw):
                o = off + (by * bw + bx) * bb
                if isBC5:
                    px = decode_bc5(raw, o)
                else:
                    alpha = None
                    co = o
                    if isBC3:
                        alpha = decode_bc4_block(raw, o)
                        co = o + 8
                    px = decode_bc1_block(raw, co, punchthrough=isBC1)
                    if alpha:
                        px = [(p[0], p[1], p[2], alpha[k]) for k, p in enumerate(px)]
                for k in range(16):
                    x, y = bx * 4 + (k % 4), by * 4 + (k // 4)
                    if x >= mw or y >= mh:
                        continue
                    d = (y * mw + x) * 4
                    p = px[k]
                    out[d] = p[0]; out[d + 1] = p[1]; out[d + 2] = p[2]; out[d + 3] = p[3]
        return mw, mh, out


def decode_bc1_block(b, o, punchthrough=False):
    c0, c1 = struct.unpack_from('<HH', b, o)
    bits = struct.unpack_from('<I', b, o + 4)[0]
    def rgb(c):
        r = (c >> 11) & 31; g = (c >> 5) & 63; bl = c & 31
        return ((r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (bl * 527 + 23) >> 6)
    a, cc = rgb(c0), rgb(c1)
    if c0 > c1 or not punchthrough:
        c2 = tuple((2 * a[i] + cc[i]) // 3 for i in range(3))
        c3 = tuple((a[i] + 2 * cc[i]) // 3 for i in range(3))
        tbl = [a + (255,), cc + (255,), c2 + (255,), c3 + (255,)]
    else:
        c2 = tuple((a[i] + cc[i]) // 2 for i in range(3))
        tbl = [a + (255,), cc + (255,), c2 + (255,), (0, 0, 0, 0)]
    return [tbl[(bits >> (2 * k)) & 3] for k in range(16)]


def decode_bc4_block(b, o):
    a0, a1 = b[o], b[o + 1]
    bits = int.from_bytes(b[o + 2:o + 8], 'little')
    # 6 interpolants between a0 and a1: weights (6-i):(1+i) over 7, i = 0..5.
    # (An earlier version used (7-i):(1+i), which is off by one and can exceed
    # 255 -- caught by the lens 2 agent. RGB was never affected; alpha was.)
    if a0 > a1:
        t = [a0, a1] + [((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)]
    else:
        t = [a0, a1] + [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255]
    return [t[(bits >> (3 * k)) & 7] for k in range(16)]


def decode_bc5(b, o):
    r = decode_bc4_block(b, o)
    g = decode_bc4_block(b, o + 8)
    return [(r[k], g[k], 0, 255) for k in range(16)]


def stats(path, mipIndex=0):
    d = DDS(path)
    mw, mh, px = d.decode(mipIndex)
    n = mw * mh
    sr = sg = sb = 0
    for i in range(n):
        sr += px[i * 4]; sg += px[i * 4 + 1]; sb += px[i * 4 + 2]
    m = (sr / n, sg / n, sb / n)
    lum = 0.2126 * m[0] + 0.7152 * m[1] + 0.0722 * m[2]
    sat = colorsys.rgb_to_hsv(m[0] / 255, m[1] / 255, m[2] / 255)[1]
    # per-pixel saturation mean (chroma), and luminance stddev (flatness)
    ps = 0.0
    lsum = 0.0; l2 = 0.0
    for i in range(n):
        r, g, bl = px[i * 4], px[i * 4 + 1], px[i * 4 + 2]
        mx, mn = max(r, g, bl), min(r, g, bl)
        ps += 0.0 if mx == 0 else (mx - mn) / mx
        L = 0.2126 * r + 0.7152 * g + 0.0722 * bl
        lsum += L; l2 += L * L
    ps /= n
    lmean = lsum / n
    lstd = max(0.0, l2 / n - lmean * lmean) ** 0.5
    return dict(w=mw, h=mh, fmt=d.fmt, mips=d.mips, mean=m, lum=lum,
                satOfMean=sat, meanSat=ps, lumStd=lstd)


if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'info':
        for p in sys.argv[2:]:
            d = DDS(p)
            print('%-34s %dx%d %s mips=%d fourcc=%r dataOff=%d fileSize=%d expect=%d'
                  % (os.path.basename(p), d.width, d.height, d.fmt, d.mips,
                     d.fourcc, d.dataOff, len(d.raw), d.dataOff + d.sliceBytes()))
    elif cmd == 'mean':
        mi = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 0
        files = sys.argv[3:] if (len(sys.argv) > 2 and sys.argv[2].isdigit()) else sys.argv[2:]
        for p in files:
            s = stats(p, mi)
            print('%-34s mip%d %3dx%-3d %-12s rgb %6.1f %6.1f %6.1f  lum %5.1f  satOfMean %.3f  meanSat %.3f  lumStd %5.1f'
                  % (os.path.basename(p), mi, s['w'], s['h'], s['fmt'],
                     s['mean'][0], s['mean'][1], s['mean'][2], s['lum'],
                     s['satOfMean'], s['meanSat'], s['lumStd']))
    elif cmd == 'dump':
        p, mi, out = sys.argv[2], int(sys.argv[3]), sys.argv[4]
        d = DDS(p)
        mw, mh, px = d.decode(mi)
        with open(out, 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (mw, mh))
            for i in range(mw * mh):
                f.write(bytes(px[i * 4:i * 4 + 3]))
        print('wrote', out, mw, mh)
