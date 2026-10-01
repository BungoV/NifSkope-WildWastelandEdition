"""THE IMAGESPACE, REBUILT (lane IMGS1, 2026-10-01; src/gl/celllights.h, docs/PRTP_PLAN.md 2j).

Independent of NifSkope's reader: this walks Fallout4.esm itself (CELL XCIM -> IMGS HNAM/CNAM/TNAM/TX00),
reads the LUT strip out of the Misc archive itself, and runs the game's chain (transcribed from
Shaders011.fxp: the tonemap PS, the luminance downsample, the LUT PS) in numpy over the HDR frame
NifSkope dumped (WW_CELL_IS_DUMP, the measure pass at full size).

  A  the adapted luminance and the exposure NifSkope echoed = the mean luma of its own dump over the pixels
     a cell-lit fragment reached, the exposure clamp from OUR read of HNAM (rel 1e-4)
  P  the picture = our chain over the dump, on the pixels that end on an opaque cell-lit surface: >= 97%
     within 3/255 of the rebuild's 3x3 neighbourhood range (the shot is antialiased, the dump is not)

usage  cell_is_check.py <Fallout4.esm> <cell EDID> <run dir> [nolut|noexp|nograde]
       (run dir holds on.png, on.hdr, on.hdr.txt; the optional word rebuilds WITHOUT that stage: a self-check
       that the picture really needs it)
"""
import os
import re
import struct
import sys
import zlib

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import walk, record  # noqa: E402

LUMA = np.array([0.2125, 0.7154, 0.0721])


def imgs_of(esm, cell):
    buf = open(esm, 'rb').read()
    imgs, want = {}, None
    for t, form, off, stack in walk(buf):
        if t == b'IMGS':
            imgs[form] = off
        elif t == b'CELL' and want is None and all(g[2] != 1 for g in stack):
            _, f = record(buf, off)
            if f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace') == cell:
                want = struct.unpack_from('<I', f[b'XCIM'])[0] if b'XCIM' in f else 0
    if not want or want not in imgs:
        return None
    _, f = record(buf, imgs[want])
    return dict(edid=f[b'EDID'].split(b'\0')[0].decode(), hdr=struct.unpack_from('<9f', f[b'HNAM']),
                cine=struct.unpack_from('<3f', f[b'CNAM']) if b'CNAM' in f else (1.0, 1.0, 1.0),
                tint=struct.unpack_from('<4f', f[b'TNAM']) if b'TNAM' in f else (0.0, 1.0, 1.0, 1.0),
                lut=f.get(b'TX00', b'').split(b'\0')[0].decode())


def lut_of(data_dir, path):
    """the 16^3 LUT [b, g, r, c] from the strip in the Misc archive (256x16 B8G8R8, x = r + 16 b, y = g)"""
    want = ('textures\\' + path).lower()
    p = os.path.join(data_dir, 'Fallout4 - Misc.ba2')
    f = open(p, 'rb')
    _, _, _, n, nto = struct.unpack('<4sI4sIQ', f.read(24))
    recs = [struct.unpack('<I4sIIQIII', f.read(36)) for _ in range(n)]
    f.seek(nto)
    for r in recs:
        ln, = struct.unpack('<H', f.read(2))
        nm = f.read(ln).decode('latin1').lower()
        if nm == want:
            here = f.tell()
            f.seek(r[4])
            d = f.read(r[5] or r[6])
            d = zlib.decompress(d) if r[5] else d
            f.seek(here)
            px = np.frombuffer(d[128:128 + 256 * 16 * 3], np.uint8).reshape(16, 16, 16, 3)   # [g, b, r, BGR]
            return px[..., ::-1].transpose(1, 0, 2, 3).astype(np.float64) / 255.0              # [b, g, r, RGB]
    return None


def lut_sample(lut, c):
    """trilinear at texel coordinate c * 15 (the PS's c * 15/16 + 1/32 on 16 texels), clamped"""
    t = np.clip(c, 0.0, 1.0) * 15.0
    i0 = np.minimum(np.floor(t).astype(int), 14)
    w = t - i0
    out = np.zeros_like(c)
    for db in (0, 1):
        for dg in (0, 1):
            for dr in (0, 1):
                wt = ((w[..., 2] if db else 1 - w[..., 2]) * (w[..., 1] if dg else 1 - w[..., 1])
                      * (w[..., 0] if dr else 1 - w[..., 0]))
                out += wt[..., None] * lut[i0[..., 2] + db, i0[..., 1] + dg, i0[..., 0] + dr]
    return out


def chain(hdr, im, adapted, lut, red=''):
    h = im['hdr']
    e = 1.0 if red == 'noexp' else min(max(h[8] / (adapted + 0.001), h[5]), h[4])
    x = 2.0 * e * np.maximum(hdr, 0.0)
    E = h[1]
    c = (x * (0.15 * x + 0.05) + 0.2 * E) / (x * (0.15 * x + 0.5) + 0.06) - E / 0.3
    w = 11.2
    c /= (w * (0.15 * w + 0.05) + 0.2 * E) / (w * (0.15 * w + 0.5) + 0.06) - E / 0.3
    sat, bright, contrast = im['cine']
    amt, tr, tg, tb = im['tint']
    if red != 'nograde':
        luma = (c @ LUMA)[..., None]
        c = luma + sat * (c - luma)
        c = c + amt * (luma * np.array([tr, tg, tb]) - c)
        c = contrast * (bright * c - adapted) + adapted
    c = np.maximum(c, 0.0) ** (1.0 / 2.2)
    if lut is not None and red != 'nolut':
        c = lut_sample(lut, c)
    return c


def main(esm, cell, run, red=''):
    im = imgs_of(esm, cell)
    if im is None:
        print('imagespace FAIL: %s has no IMGS' % cell)
        return 1
    lut = lut_of(os.path.dirname(esm), im['lut']) if im['lut'] else None
    print('imgs %s hdr %s cine %s tint %s lut %s (%s)' % (im['edid'], ','.join('%g' % v for v in im['hdr']),
          ','.join('%g' % v for v in im['cine']), ','.join('%g' % v for v in im['tint']), im['lut'] or '-',
          'read' if lut is not None else 'NONE'))
    raw = open(os.path.join(run, 'on.hdr'), 'rb').read()
    w, h = struct.unpack_from('<2i', raw)
    px = np.frombuffer(raw, np.float32, w * h * 4, 8).reshape(h, w, 4)[::-1].astype(np.float64)
    rgb = np.where(np.isfinite(px[..., :3]), px[..., :3], 0.0)
    # the stencil after the floats: bit 0 = a cell-lit fragment landed (the pixels the mean counts), bit 1 =
    # the last fragment was blended or not cell-lit (glass, effects: the picture blends AFTER the chain there)
    cover = np.frombuffer(raw, np.uint8, w * h, 8 + w * h * 16).reshape(h, w)[::-1]
    lit = (cover & 1) == 1
    echo = open(os.path.join(run, 'on.hdr.txt')).read()
    ea = float(re.search(r'adapted=(\S+)', echo).group(1))
    ee = float(re.search(r'exposure=(\S+)', echo).group(1))
    mine = float((rgb[lit] @ LUMA).mean()) if lit.any() else 0.0
    h9 = im['hdr']
    my_e = min(max(h9[8] / (mine + 0.001), h9[5]), h9[4])
    okA = abs(ea - mine) <= 1e-4 * max(mine, 1e-6) and abs(ee - my_e) <= 1e-4 * my_e
    print('A %s  adapted %.7g (echo %.7g) over %d of %dx%d px, exposure %.6g (echo %.6g)' % (
        'PASS' if okA else 'FAIL', mine, ea, int(lit.sum()), w, h, my_e, ee))
    pic = np.asarray(Image.open(os.path.join(run, 'on.png')).convert('RGB'), np.float64) / 255.0
    if pic.shape[:2] != (h, w):
        print('P FAIL  the picture is %dx%d, the dump %dx%d' % (pic.shape[1], pic.shape[0], w, h))
        return 1
    # compared where the pixel ends on an opaque cell-lit surface; the picture is antialiased and the dump is
    # not, so a pixel passes inside the 3x3 range of the rebuilt neighbours (+-3/255): an edge pixel is a mix
    geo = cover == 1
    want = chain(rgb, im, mine, lut, red)
    pad = np.pad(want, ((1, 1), (1, 1), (0, 0)), mode='edge')
    nb = [pad[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] for dy in (-1, 0, 1) for dx in (-1, 0, 1)]
    lo, hi = np.min(nb, 0), np.max(nb, 0)
    d = np.maximum(np.maximum(lo - pic, pic - hi), 0.0).max(-1)[geo]
    strict = np.abs(want - pic).max(-1)[geo]
    frac = float((d <= 3.0 / 255.0).mean()) if d.size else 0.0
    okP = frac >= 0.97 and d.size >= 5000
    print('P %s  %.2f%% of %d opaque cell-lit pixels (%.0f%% of the frame) within 3/255 of the rebuild '
          '(strict per pixel %.2f%%, median %.2f/255)' % (
              'PASS' if okP else 'FAIL', 100 * frac, d.size, 100.0 * d.size / (w * h),
              100 * float((strict <= 3.0 / 255.0).mean()) if d.size else 0.0,
              255 * float(np.median(strict)) if d.size else 0.0))
    ok = okA and okP
    print('imagespace %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:5]))
