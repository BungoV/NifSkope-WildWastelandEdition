"""The statistics gate F3 is measured with, in ONE place.

They lived in f3_fit.py and f3_struct.py first, and the next script that wanted
one imported it from there -- which re-ran that module's whole 22-texture
measurement as an import side effect, twice in this lane before it was noticed.
A module that MEASURES is not a module to import from. Everything importable
lives here; f3_fit.py, f3_struct.py and f3_picture.py import from this file and
from nothing else of each other.
"""
import numpy as np

RES, TILE, CHUNKW = 512, 341.333, 16384.0
STEP = (CHUNKW / RES) / TILE          # 0.09375 repeats per sheet texel


def rough(a, lag=1):
    """mean |neighbour difference| at `lag` texels, averaged over x and y."""
    a = a.astype(np.float64)
    return float(0.5 * (np.abs(a[:, lag:] - a[:, :-lag]).mean()
                        + np.abs(a[lag:, :] - a[:-lag, :]).mean()))


def hp(a):
    """high pass: the texel minus its 3x3 box mean -- the scale `rough` sees."""
    a = a.astype(np.float64)
    pad = np.pad(a, 1, mode='reflect')
    sm = sum(pad[i:i + a.shape[0], j:j + a.shape[1]]
             for i in range(3) for j in range(3)) / 9.0
    return a - sm


def corr(a, b):
    a = a - a.mean()
    b = b - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d else 0.0


def phase_twin(a, rng):
    """same power spectrum, random phase -- structure destroyed."""
    F = np.fft.rfft2(np.asarray(a, np.float64))
    return np.fft.irfft2(np.abs(F) * np.exp(1j * rng.uniform(-np.pi, np.pi, F.shape)),
                         s=a.shape)


def tiled(plane):
    """a land `_n` mip sampled on the 512 sheet grid, bilinear, wrapping."""
    n = plane.shape[0]
    u = (np.arange(RES) * STEP * n) % n
    i0 = np.floor(u).astype(int) % n
    i1 = (i0 + 1) % n
    t = (u - np.floor(u))[None, :]
    row = plane[:, i0] * (1 - t) + plane[:, i1] * t
    v = (np.arange(RES) * STEP * n) % n
    j0 = np.floor(v).astype(int) % n
    j1 = (j0 + 1) % n
    s = (v - np.floor(v))[:, None]
    return row[j0] * (1 - s) + row[j1] * s
