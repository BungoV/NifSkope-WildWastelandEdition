"""SKYDISC1: build the cell view's moon textures from NASA's CGI Moon Kit (SVS 4720), as loose files.

Inputs (download from https://svs.gsfc.nasa.gov/4720 only):
  lroc_color_poles_2k.tif  LRO LROC WAC colour mosaic, 2048x1024, equirectangular, lon -180 at the left edge
  ldem_4.tif               LRO LOLA elevation, 1440x720 float km above 1737.4 km, same layout
Outputs (loose DDS, legacy headers the game also uses):
  Moon_d.dds  BC1 (DXT1) sRGB albedo, 2048x1024 + mips, scaled so its linear mean is KMEAN (the shader divides it out)
  Moon_n.dds  BC5 (ATI2) tangent normal (R = east, G = north), 1024x512 + mips, from the LOLA slopes
Credit: NASA's Scientific Visualization Studio (CGI Moon Kit), LRO LROC and LOLA teams.
Install (bungo 10-27): loose files in the MO2 mod FO4CS beside its other files, data path Textures/Sky/Moon/Moon_d.dds
and Moon_n.dds;
the cell view finds them through the resource lookup like any loose texture (no BA2, nothing in the exe).
Usage: python -I tools/moon_texture.py <dir with the two tifs> [out dir] [--relief 1.0]
Pure numpy + PIL; the BC encoders are below, so the output is the same bytes on every machine."""
import argparse
import os
import struct
import sys

import numpy as np
from PIL import Image

KMEAN = 0.30          # linear mean of the stored albedo; skydisc.h kAlbedoMean must match
R_MOON_KM = 1737.4
DEFAULT_OUT = r'E:/Projects/Fallout 4 Mods/mods/FO4CS/Textures/Sky/Moon'


def srgb_to_lin(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def blocks(img):
    """HxWxC -> (H/4*W/4, 16, C) in row-major block order"""
    H, W, C = img.shape
    return img.reshape(H // 4, 4, W // 4, 4, C).transpose(0, 2, 1, 3, 4).reshape(-1, 16, C)


def bc1(img8):
    """img8 HxWx3 float 0..255 -> BC1 bytes (4-colour mode, principal-axis endpoints)"""
    b = blocks(img8)
    mean = b.mean(axis=1, keepdims=True)
    d = b - mean
    cov = np.einsum('nki,nkj->nij', d, d)
    v = np.ones((len(b), 3)) / np.sqrt(3)
    for _ in range(8):
        v = np.einsum('nij,nj->ni', cov, v)
        v /= np.maximum(np.linalg.norm(v, axis=1, keepdims=True), 1e-9)
    t = np.einsum('nki,ni->nk', d, v)
    lo = mean[:, 0] + v * t.min(axis=1, keepdims=True)
    hi = mean[:, 0] + v * t.max(axis=1, keepdims=True)

    def q565(c):
        c = np.clip(c, 0, 255)
        r = np.round(c[:, 0] * 31 / 255).astype(np.uint32)
        g = np.round(c[:, 1] * 63 / 255).astype(np.uint32)
        bl = np.round(c[:, 2] * 31 / 255).astype(np.uint32)
        return (r << 11) | (g << 5) | bl

    def dq(p):
        r = (p >> 11) & 31
        g = (p >> 5) & 63
        bl = p & 31
        return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (bl << 3) | (bl >> 2)], axis=1).astype(np.float64)

    c0, c1 = q565(hi), q565(lo)
    sw = c0 < c1
    c0, c1 = np.where(sw, c1, c0), np.where(sw, c0, c1)
    e0, e1 = dq(c0), dq(c1)
    pal = np.stack([e0, e1, (2 * e0 + e1) / 3, (e0 + 2 * e1) / 3], axis=1)        # n,4,3
    dist = ((b[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(axis=3)            # n,16,4
    idx = dist.argmin(axis=2).astype(np.uint32)
    idx[c0 == c1] = 0
    bits = (idx << (2 * np.arange(16, dtype=np.uint32))).sum(axis=1).astype(np.uint32)
    out = np.zeros((len(b), 2), np.uint32)
    out[:, 0] = c0 | (c1 << 16)
    out[:, 1] = bits
    return out.astype('<u4').tobytes()


def bc4(ch8):
    """one channel HxW float 0..255 -> BC4 blocks (n, 8 bytes) in the 8-value mode"""
    b = blocks(ch8[:, :, None])[:, :, 0]
    a0 = np.round(b.max(axis=1)).astype(np.int64)
    a1 = np.round(b.min(axis=1)).astype(np.int64)
    pal = np.stack([a0, a1] + [((7 - k) * a0 + k * a1) / 7 for k in range(1, 7)], axis=1).astype(np.float64)
    idx = np.abs(b[:, :, None] - pal[:, None, :]).argmin(axis=2).astype(np.uint64)
    idx[a0 == a1] = 0
    bits = (idx << (3 * np.arange(16, dtype=np.uint64))).sum(axis=1).astype(np.uint64)
    out = np.zeros((len(b), 8), np.uint8)
    out[:, 0] = a0
    out[:, 1] = a1
    for k in range(6):
        out[:, 2 + k] = (bits >> np.uint64(8 * k)) & np.uint64(255)
    return out


def bc5(rg8):
    return np.concatenate([bc4(rg8[:, :, 0]), bc4(rg8[:, :, 1])], axis=1).tobytes()


def dds(path, w, h, levels, fourcc, payload, blockbytes):
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000       # caps height width pixfmt mipcount linearsize
    hdr = struct.pack('<4sIIIIIII', b'DDS ', 124, flags, h, w, max(1, w // 4) * max(1, h // 4) * blockbytes, 0, levels)
    hdr += b'\0' * 44
    hdr += struct.pack('<II4sIIIII', 32, 0x4, fourcc, 0, 0, 0, 0, 0)
    hdr += struct.pack('<IIIII', 0x1000 | 0x400000 | 0x8, 0, 0, 0, 0)
    assert len(hdr) == 128
    with open(path, 'wb') as f:
        f.write(hdr + payload)
    return len(hdr) + len(payload)


def mips_lin(img, stop=4):
    out = [img]
    while min(out[-1].shape[:2]) > stop:
        a = out[-1]
        out.append(0.25 * (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2]))
    return out


def resample(a, W, H):
    """bilinear on an equirect grid (wraps in longitude, clamps in latitude); cell centres kept"""
    h0, w0 = a.shape
    x = (np.arange(W) + 0.5) * w0 / W - 0.5
    y = np.clip((np.arange(H) + 0.5) * h0 / H - 0.5, 0, h0 - 1)
    x0 = np.floor(x).astype(int)
    fx = x - x0
    y0 = np.floor(y).astype(int)
    fy = y - y0
    y1 = np.minimum(y0 + 1, h0 - 1)
    xa, xb = x0 % w0, (x0 + 1) % w0
    r0 = a[y0][:, xa] * (1 - fx) + a[y0][:, xb] * fx
    r1 = a[y1][:, xa] * (1 - fx) + a[y1][:, xb] * fx
    return r0 * (1 - fy)[:, None] + r1 * fy[:, None]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('out', nargs='?', default=DEFAULT_OUT)
    ap.add_argument('--relief', type=float, default=1.0, help='slope scale; 1 = the real LOLA slopes')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    col = np.asarray(Image.open(os.path.join(a.src, 'lroc_color_poles_2k.tif')).convert('RGB'), np.float64) / 255.0
    assert col.shape == (1024, 2048, 3), col.shape
    lin = srgb_to_lin(col)
    lum = lin @ np.array([0.2126, 0.7152, 0.0722])
    k = KMEAN / lum.mean()
    lin = lin * k
    clip = float((lin > 1.0).any(axis=2).mean())
    levels = mips_lin(lin)
    pay = b''.join(bc1(lin_to_srgb(m) * 255.0) for m in levels)
    nd = dds(os.path.join(a.out, 'Moon_d.dds'), 2048, 1024, len(levels), b'DXT1', pay, 8)
    print('Moon_d.dds %d bytes (%.2f MB) %d levels, linear mean %.4f -> %.2f (x%.3f), clipped %.4f%%'
          % (nd, nd / 2**20, len(levels), lum.mean(), KMEAN, k, 100 * clip))

    hk = np.asarray(Image.open(os.path.join(a.src, 'ldem_4.tif')), np.float64)
    assert hk.shape == (720, 1440), hk.shape
    W, H = 1024, 512
    h = resample(hk, W, H)
    lat = np.radians(90.0 - (np.arange(H) + 0.5) * 180.0 / H)
    dx = 2 * np.pi * R_MOON_KM * np.maximum(np.cos(lat), 0.02) / W     # km per texel east
    dy = np.pi * R_MOON_KM / H                                          # km per texel north
    ge = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) / (2 * dx[:, None])
    hn = np.vstack([h[:1], h, h[-1:]])
    gn = (hn[:-2] - hn[2:]) / (2 * dy)                                  # row 0 = north, so north = up the rows
    n = np.stack([-ge * a.relief, -gn * a.relief, np.ones_like(h)], axis=2)
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    levels = mips_lin(n)
    pay = b''
    for m in levels:
        m = m / np.linalg.norm(m, axis=2, keepdims=True)
        pay += bc5((m[:, :, :2] * 0.5 + 0.5) * 255.0)
    nn = dds(os.path.join(a.out, 'Moon_n.dds'), W, H, len(levels), b'ATI2', pay, 16)
    tilt = np.degrees(np.arccos(n[:, :, 2]))
    print('Moon_n.dds %d bytes (%.2f MB) %d levels, slope p50 %.2f p99 %.2f max %.2f deg'
          % (nn, nn / 2**20, len(levels), np.percentile(tilt, 50), np.percentile(tilt, 99), tilt.max()))
    print('total %.2f MB' % ((nd + nn) / 2**20))


if __name__ == '__main__':
    sys.exit(main())
