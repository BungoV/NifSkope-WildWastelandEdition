"""Lane SPLAT1 -- the instruments.

Everything here is READ-ONLY against shipped and already-baked files. Nothing
imports the generator; the DDS decoding is its own, vectorised, and is checked
against `tests/spells/lodgen_terrain_model.py`'s independent decoder in
`s0_selftest.py` before any verdict is read off it.

Two metrics:

  * `local_var(L, r)` -- the per-texel variance inside a (2r+1)^2 window, the
    thing the eye calls "speckle". Reported as the MEAN over the texels, in
    squared 8-bit units, and as its square root beside it.
  * `radial_power(L)` -- the radially averaged power spectrum of the
    mean-removed, Hann-windowed field, normalised so that the sum over all
    bins equals the field's variance. A band table then says how much of the
    variance sits at each SCALE, which is what separates "the codec" (one
    scale, period 4) from "the content" (broadband) from "the 17-grid"
    (period 4 texels = 128 world units) from "the tiling" (period 64).
"""

import os
import struct

import numpy as np


# ---------------------------------------------------------------- DDS reading

def _u565(c):
    r = ((c >> 11) & 31).astype(np.float32) * (255.0 / 31.0)
    g = ((c >> 5) & 63).astype(np.float32) * (255.0 / 63.0)
    b = (c & 31).astype(np.float32) * (255.0 / 31.0)
    return np.stack([r, g, b], -1)


class Dds(object):
    """BC1 / BC3 / BC5U, every mip, decoded with numpy."""

    def __init__(self, path):
        with open(path, 'rb') as f:
            self.raw = f.read()
        if self.raw[:4] != b'DDS ':
            raise ValueError('not a DDS: %s' % path)
        h = struct.unpack_from('<31I', self.raw, 4)
        self.height, self.width = h[2], h[3]
        mips = max(1, h[6])
        self.fourcc = self.raw[84:88]
        off = 148 if self.fourcc == b'DX10' else 128
        if self.fourcc == b'DXT1':
            self.blockBytes = 8
        elif self.fourcc in (b'DXT3', b'DXT5', b'BC5U', b'ATI2'):
            self.blockBytes = 16
        else:
            raise ValueError('unsupported fourCC %r in %s' % (self.fourcc, path))
        self.levels = []
        w, h2 = self.width, self.height
        for _ in range(mips):
            bw, bh = max(1, (w + 3) // 4), max(1, (h2 + 3) // 4)
            size = bw * bh * self.blockBytes
            if off + size > len(self.raw):
                break
            self.levels.append((off, w, h2))
            off += size
            w, h2 = max(1, w // 2), max(1, h2 // 2)
        self.maxMip = len(self.levels) - 1
        self.path = path
        self._c = {}

    def level(self, m):
        """(h, w, 4) float32 in 0..255."""
        if m in self._c:
            return self._c[m]
        off, w, h = self.levels[m]
        bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
        n = bw * bh
        buf = np.frombuffer(self.raw, dtype=np.uint8,
                            count=n * self.blockBytes, offset=off)
        buf = buf.reshape(n, self.blockBytes)
        out = np.zeros((n, 16, 4), np.float32)
        if self.fourcc in (b'BC5U', b'ATI2'):
            out[:, :, 0] = _bc4(buf[:, 0:8])
            out[:, :, 1] = _bc4(buf[:, 8:16])
            out[:, :, 3] = 255.0
        else:
            co = 8 if self.blockBytes == 16 else 0
            cb = buf[:, co:co + 8]
            c0 = cb[:, 0].astype(np.uint32) | (cb[:, 1].astype(np.uint32) << 8)
            c1 = cb[:, 2].astype(np.uint32) | (cb[:, 3].astype(np.uint32) << 8)
            bits = np.zeros(n, np.uint32)
            for k in range(4):
                bits |= cb[:, 4 + k].astype(np.uint32) << (8 * k)
            e0, e1 = _u565(c0), _u565(c1)
            four = (c0 > c1) | (self.blockBytes == 16)
            pal = np.zeros((n, 4, 3), np.float32)
            pal[:, 0] = e0
            pal[:, 1] = e1
            pal[:, 2] = np.where(four[:, None], (2 * e0 + e1) / 3.0, (e0 + e1) / 2.0)
            pal[:, 3] = np.where(four[:, None], (e0 + 2 * e1) / 3.0, 0.0)
            idx = np.stack([((bits >> np.uint32(2 * k)) & np.uint32(3))
                            for k in range(16)], 1).astype(np.intp)
            out[:, :, :3] = np.take_along_axis(pal, idx[:, :, None], 1)
            a = np.full((n, 16), 255.0, np.float32)
            if self.blockBytes == 16 and self.fourcc == b'DXT5':
                a = _bc4(buf[:, 0:8])
            elif self.blockBytes == 16 and self.fourcc == b'DXT3':
                ab = np.zeros((n, 16), np.float32)
                for k in range(16):
                    byte = buf[:, k // 2].astype(np.uint32)
                    nib = np.where(k % 2 == 0, byte & 15, byte >> 4)
                    ab[:, k] = nib.astype(np.float32) * 17.0
                a = ab
            elif not (self.blockBytes == 16):
                a = np.where(four[:, None], 255.0,
                             np.where(idx == 3, 0.0, 255.0)).astype(np.float32)
            out[:, :, 3] = a
        img = out.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4)
        img = img.reshape(bh * 4, bw * 4, 4)[:h, :w]
        self._c[m] = np.ascontiguousarray(img)
        return self._c[m]


def _bc4(blocks):
    """(n,8) uint8 BC4 blocks -> (n,16) float 0..255."""
    n = blocks.shape[0]
    a0 = blocks[:, 0].astype(np.float32)
    a1 = blocks[:, 1].astype(np.float32)
    bits = np.zeros(n, np.uint64)
    for k in range(6):
        bits |= blocks[:, 2 + k].astype(np.uint64) << np.uint64(8 * k)
    tab = np.zeros((n, 8), np.float32)
    tab[:, 0], tab[:, 1] = a0, a1
    gt = a0 > a1
    for i in range(1, 7):
        tab[:, 1 + i] = np.where(gt, ((7 - i) * a0 + i * a1) / 7.0, 0.0)
    for i in range(1, 5):
        tab[:, 1 + i] = np.where(gt, tab[:, 1 + i], ((5 - i) * a0 + i * a1) / 5.0)
    tab[:, 6] = np.where(gt, tab[:, 6], 0.0)
    tab[:, 7] = np.where(gt, tab[:, 7], 255.0)
    idx = np.stack([((bits >> np.uint64(3 * k)) & np.uint64(7))
                    for k in range(16)], 1).astype(np.intp)
    return np.take_along_axis(tab, idx, 1)


# ------------------------------------------------------------------- metrics

def lum(img):
    return (img[:, :, 0] * 0.2126 + img[:, :, 1] * 0.7152 + img[:, :, 2] * 0.0722)


def _box(a, r):
    """(2r+1)^2 box mean, edge-clamped, no scipy."""
    p = np.pad(a, r, mode='edge').astype(np.float64)
    c = np.cumsum(np.cumsum(p, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    k = 2 * r + 1
    h, w = a.shape
    s = (c[k:k + h, k:k + w] - c[0:h, k:k + w]
         - c[k:k + h, 0:w] + c[0:h, 0:w])
    return s / float(k * k)


def local_var(a, r=1):
    """Per-texel variance in a (2r+1)^2 window. Returns the map."""
    m = _box(a, r)
    m2 = _box(a.astype(np.float64) ** 2, r)
    return np.maximum(m2 - m * m, 0.0)


def radial_power(a):
    """(freq_bins, power) with sum(power) == var(windowed field).

    Frequencies are in cycles per texel, 0 .. 0.5 (Nyquist).
    """
    h, w = a.shape
    win = np.outer(np.hanning(h), np.hanning(w))
    f = a.astype(np.float64) - a.mean()
    f = f * win
    F = np.fft.fft2(f)
    P = (np.abs(F) ** 2) / float(h * w) ** 2
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    rad = np.sqrt(fy ** 2 + fx ** 2)
    nb = 128
    edges = np.linspace(0.0, 0.5 * np.sqrt(2.0), nb + 1)
    idx = np.clip(np.digitize(rad.ravel(), edges) - 1, 0, nb - 1)
    tot = np.bincount(idx, weights=P.ravel(), minlength=nb)
    ctr = 0.5 * (edges[:-1] + edges[1:])
    return ctr, tot


BANDS = [
    ('>=128 tx  (>=4096 u)', 0.0, 1.0 / 128.0),
    ('32..128tx (1k..4k u)', 1.0 / 128.0, 1.0 / 32.0),
    ('8..32 tx  (256..1k u)', 1.0 / 32.0, 1.0 / 8.0),
    ('4..8 tx   (128..256u)', 1.0 / 8.0, 1.0 / 4.0),
    ('2..4 tx   (64..128 u)', 1.0 / 4.0, 0.5),
    ('<=2 tx    (<=64 u)', 0.5, 10.0),
]


def band_table(ctr, pw):
    out = []
    for name, lo, hi in BANDS:
        m = (ctr >= lo) & (ctr < hi)
        out.append((name, float(pw[m].sum())))
    return out


def peak_at(ctr, pw, period):
    """Power in the two bins straddling 1/period, over the local median."""
    f = 1.0 / float(period)
    i = int(np.argmin(np.abs(ctr - f)))
    lo, hi = max(0, i - 6), min(len(pw), i + 7)
    nb = np.concatenate([pw[lo:max(lo, i - 1)], pw[min(hi, i + 2):hi]])
    med = float(np.median(nb)) if nb.size else 0.0
    return float(pw[i]), med, (float(pw[i]) / med if med > 0 else float('inf'))


# ------------------------------------------------------------------ controls

def phase_twin(a, seed=1):
    """Same amplitude spectrum, random phase -- no structure, same energy."""
    rng = np.random.default_rng(seed)
    F = np.fft.fft2(a.astype(np.float64) - a.mean())
    n = rng.standard_normal(a.shape)
    N = np.fft.fft2(n)
    ph = N / np.maximum(np.abs(N), 1e-30)
    out = np.real(np.fft.ifft2(np.abs(F) * ph))
    return out + a.mean()


def smooth_field(h, w, cells=32, seed=3, amp=20.0, mean=90.0):
    """Known answer, smooth: band-limited noise, nothing above 1/cells."""
    rng = np.random.default_rng(seed)
    F = np.zeros((h, w), complex)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    rad = np.sqrt(fy ** 2 + fx ** 2)
    m = rad <= (1.0 / float(cells))
    N = np.fft.fft2(rng.standard_normal((h, w)))
    F[m] = N[m]
    o = np.real(np.fft.ifft2(F))
    o = o / o.std() * amp + mean
    return o


def checker(h, w, period=6, amp=12.0):
    """Square checker, full cycle `period` texels on each axis.

    Its fundamental therefore sits at (1/p, 1/p), RADIUS sqrt(2)/p in the
    radially averaged spectrum -- not 1/p. `checker_radius(p)` says so, and
    the self-test uses it, because looking for the peak at the wrong radius is
    how a working spectrum instrument gets reported as broken.
    """
    yy, xx = np.mgrid[0:h, 0:w]
    return amp * (((yy // (period // 2)) + (xx // (period // 2))) % 2 * 2.0 - 1.0)


def checker_radius(period):
    return float(period) / np.sqrt(2.0)


# -------------------------------------------------------------- BC1 emulation

def bc1_roundtrip(img):
    """Encode RGB (h,w,3) 0..255 to BC1 four-colour and decode again.

    Min/max endpoints on the block's principal axis, nearest index. Used ONLY
    as a floor: how much local variance a block codec ADDS to a field that
    already has none of its own. Checked on vanilla's own bytes in
    `s0_selftest.py`.
    """
    h, w = img.shape[:2]
    bh, bw = h // 4, w // 4
    b = img[:bh * 4, :bw * 4, :3].reshape(bh, 4, bw, 4, 3).transpose(0, 2, 1, 3, 4)
    b = b.reshape(bh * bw, 16, 3).astype(np.float64)
    mean = b.mean(1, keepdims=True)
    d = b - mean
    # principal axis by one power iteration from the mean-removed spread
    ax = d[:, 0, :].copy()
    bad = np.linalg.norm(ax, axis=1) < 1e-9
    ax[bad] = np.array([1.0, 1.0, 1.0])
    for _ in range(6):
        t = np.einsum('nkc,nc->nk', d, ax)
        ax = np.einsum('nk,nkc->nc', t, d)
        nrm = np.linalg.norm(ax, axis=1, keepdims=True)
        ax = np.where(nrm > 1e-12, ax / np.maximum(nrm, 1e-12),
                      np.array([1.0, 0.0, 0.0]))
    t = np.einsum('nkc,nc->nk', d, ax)
    i0 = np.argmin(t, 1)
    i1 = np.argmax(t, 1)
    n = b.shape[0]
    e0 = b[np.arange(n), i0]
    e1 = b[np.arange(n), i1]
    q = lambda c: np.stack([
        np.round(c[:, 0] / 255.0 * 31.0) * (255.0 / 31.0),
        np.round(c[:, 1] / 255.0 * 63.0) * (255.0 / 63.0),
        np.round(c[:, 2] / 255.0 * 31.0) * (255.0 / 31.0)], 1)
    e0, e1 = q(e0), q(e1)
    pal = np.stack([e0, e1, (2 * e0 + e1) / 3.0, (e0 + 2 * e1) / 3.0], 1)
    dist = ((b[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)
    idx = np.argmin(dist, 2)
    out = np.take_along_axis(pal, idx[:, :, None], 1)
    out = out.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 3)
    full = img[:, :, :3].copy()
    full[:bh * 4, :bw * 4] = out
    return full


# --------------------------------------------------------------------- paths

VAN = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OURS = {
    (-20, 24): os.path.join(REPO, 'scratchpad', 'terrain_r_20260911', 'out',
                            'stock_new2', 'tex', 'Commonwealth.4.-20.24.DDS'),
    (-20, 20): os.path.join(REPO, 'scratchpad', 'roads1_20260911', 'out',
                            'after', 'tex', 'Commonwealth.4.-20.20.DDS'),
}
OURS_NOROAD = {
    (-20, 20): os.path.join(REPO, 'scratchpad', 'roads1_20260911', 'out',
                            'noroad', 'tex', 'Commonwealth.4.-20.20.DDS'),
}
COVER = {
    (-20, 24): os.path.join(REPO, 'scratchpad', 'terrain_r_20260911', 'out',
                            'stock_new2', 'tex', 'Commonwealth.4.-20.24_data.DDS'),
    (-20, 20): os.path.join(REPO, 'scratchpad', 'roads1_20260911', 'out',
                            'after', 'tex', 'Commonwealth.4.-20.20_data.DDS'),
}


def van_sheet(cx, cy, suffix=''):
    return os.path.join(VAN, 'Commonwealth.4.%d.%d%s.DDS' % (cx, cy, suffix))


# ----------------------------------------------------- the bake's own sampler

def _bilerp_wrap(img, u, v):
    """The C++ `getPixelB_Wrap` on normalised coords, vectorised.

    Power-of-two masks, `x*w - 0.5`, floor, wrap by AND -- re-typed from
    `lib/libfo76utils/src/ddstxt16.hpp` convertTexCoord + getPixelB_Wrap.
    """
    h, w = img.shape[:2]
    xf = u * w - 0.5
    yf = v * h - 0.5
    x0 = np.floor(xf).astype(np.int64)
    y0 = np.floor(yf).astype(np.int64)
    tx = (xf - x0)[..., None]
    ty = (yf - y0)[..., None]
    xm, ym = w - 1, h - 1
    a = img[(y0 & ym), (x0 & xm)]
    b = img[(y0 & ym), ((x0 + 1) & xm)]
    c = img[((y0 + 1) & ym), (x0 & xm)]
    d = img[((y0 + 1) & ym), ((x0 + 1) & xm)]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def sample_trilinear(dds, u, v, mip):
    """`DDSTexture16::getPixelT` -- two mips blended by the fraction."""
    mip = float(min(max(mip, 0.0), float(dds.maxMip)))
    m0 = int(mip)
    c0 = _bilerp_wrap(dds.level(m0), u, v)
    f = mip - m0
    if f <= 0.0 or m0 >= dds.maxMip:
        return c0
    c1 = _bilerp_wrap(dds.level(m0 + 1), u, v)
    return c0 * (1.0 - f) + c1 * f


def bake_mip(dds, tile, upt):
    """THE MIP THE CODE PICKS. src/lodgen.cpp:6551-6555 (and 7665, 7703, 7722):

        texelWorld = TILE / tex->getWidth()
        mip = clamp( log2( max(1, footprint / texelWorld) ), 0, maxMipLevel )
    """
    texelWorld = tile / float(dds.width)
    return min(max(np.log2(max(1.0, upt / texelWorld)), 0.0), float(dds.maxMip))


def footprint_box(dds, wx, wy, tile, upt):
    """The TRUE footprint average: the exact box mean of mip 0 over the
    upt x upt world square at (wx, wy). No mip chain, no bilinear -- the
    thing a footprint-matched tap is an approximation OF."""
    img = dds.level(0)
    h, w = img.shape[:2]
    # texels per world unit at mip 0
    s = float(w) / tile
    n = int(round(upt * s))
    n = max(1, n)
    x0 = np.floor((wx * s) - n / 2.0).astype(np.int64)
    y0 = np.floor((wy * s) - n / 2.0).astype(np.int64)
    acc = np.zeros(np.shape(wx) + (4,), np.float64)
    for j in range(n):
        for i in range(n):
            acc += img[((y0 + j) % h), ((x0 + i) % w)]
    return acc / float(n * n)
