#!/usr/bin/env python3
"""The interior cube map check (lane CUBE1, 2026-10-01; tests/spells/cell_cube.sh).

  cell_cube_check.py <Fallout4.esm> <shot dir> <label> <camera target x,y,z> [legacy|pbr]

Reads <label>.probe2/3/4/50/51/52/53.png, their .cam dumps, <label>.probe50.notes (the cell centre) and
<label>.probe51.cubetags (the run's "cubetag=N|material" lines) from the shot dir. Everything below is
rebuilt here, from the files, without the viewer's code:
  * the material: its BGSM parsed again (env map, env map scale, specular switch, specular multiplier,
    smoothness, the _s map), from the loose Materials folder ($DATA, default the unpacked Data);
  * the textures: read from the game's .ba2 archives in the order the Game Manager lists them (base game
    first, each group by descending name), the cube map's six faces and the _s map's BC5 blocks decoded here;
  * per pixel: the world position (probes 2/3 + the cell centre), the world normal (probe 4 + its rounding
    residual, probe 53), the texture coordinate (probes 51/52, 16 bits) and the material (51/52's blue tag);
  * the game's interior env term (the deferred composite, read from the game's shader; notes in the lane):
      g = sat(smoothness x _s.g), spec = sat(specular multiplier x _s.r) (0 when the specular switch is off)
      K = 3 x spec x min(sqrt(sat(g - 0.3)), 1) x clamp(env map scale, 0, 50) x lin(cube(R, mip))
      mip = (1 - g) x 6 + view depth / 512 (on a 128 cube), lin = the sRGB decode, R = the mirrored view
  * the PBR program (pbr) takes the same term for a shape with no .pbrm (a .pbrm shape keeps the PBR law
    and writes no material number, so it is not judged); a material with no _s map reads _s.r = _s.g = 1.
Probe 50 = K / 4 (light-free: the light it is multiplied by is cell_lit.sh's). A pixel is judged only where
the _s map's mips 0-4 give the same answer and the cube footprint stays inside one face; the viewer's
value must sit inside the expected range +- 3/255 + 5%.
One verdict line; exit 0 on PASS.
"""
import glob
import math
import os
import re
import struct
import sys
import zlib

import numpy as np
from PIL import Image

BS = chr(92)


# ------------------------------------------------------------------ files

def read_bgsm(b):
    """The BGSM fields the env term needs (versions 1-2: the Vault's materials are version 2)."""
    o = [4]

    def u(fmt):
        v = struct.unpack_from('<' + fmt, b, o[0])
        o[0] += struct.calcsize('<' + fmt)
        return v if len(v) > 1 else v[0]

    def s():
        n = u('I')
        t = b[o[0]:o[0] + n].split(b'\0')[0].decode('latin1')
        o[0] += n
        return t
    if b[:4] != b'BGSM':
        return None
    m = {'ver': u('I')}
    if m['ver'] > 2:
        return m            # the newer layouts carry no env map scale in this place: not judged here
    u('I'); u('4f'); u('f'); u('B'); u('2I')
    for _ in range(12):
        u('B')
    u('f')
    m['env'] = u('B')
    m['envScale'] = u('f')
    u('B')
    tex = [s() for _ in range(9)]
    m['smoothspec'], m['envmap'] = tex[2], tex[4]
    u('B')
    u('B'); u('f'); u('f'); u('B'); u('f')
    m['specOn'] = u('B')
    u('3f')
    m['specMult'] = u('f')
    m['smooth'] = u('f')
    return m


class Archive:
    def __init__(self, path):
        self.path = path
        self.f = open(path, 'rb')
        magic, ver, kind, n, nto = struct.unpack('<4sIIIQ', self.f.read(24))
        self.dx10 = kind.to_bytes(4, 'little') == b'DX10'
        self.recs = []
        if self.dx10:
            for _ in range(n):
                h = struct.unpack('<I4sIBBHHHBBH', self.f.read(24))
                self.recs.append((h, [struct.unpack('<QIIHHI', self.f.read(24)) for _ in range(h[4])]))
        self.names = {}
        if self.dx10:
            self.f.seek(nto)
            for i in range(n):
                ln = struct.unpack('<H', self.f.read(2))[0]
                self.names[self.f.read(ln).decode('latin1').lower().replace('/', BS)] = i

    def texture(self, name):
        i = self.names.get(name)
        if i is None:
            return None
        h, chunks = self.recs[i]
        data = b''
        for off, packed, unpacked, _m0, _m1, _al in chunks:
            self.f.seek(off)
            raw = self.f.read(packed if packed else unpacked)
            data += zlib.decompress(raw) if packed else raw
        return dict(h=h[6], w=h[7], mips=h[8], fmt=h[9], cube=bool(h[10] & 1), data=data, src=os.path.basename(self.path))


def archives(game):
    """The Game Manager's order: the base game's archives first, then the rest, each by descending name."""
    every = [p for p in glob.glob(os.path.join(game, '*.ba2'))]
    base = sorted([p for p in every if os.path.basename(p).lower().startswith('fallout4 - ')],
                  key=lambda p: os.path.basename(p).lower(), reverse=True)
    rest = sorted([p for p in every if p not in base], key=lambda p: os.path.basename(p).lower(), reverse=True)
    return [Archive(p) for p in base + rest if 'textures' in os.path.basename(p).lower()]


def find_texture(arcs, rel):
    rel = rel.lower().replace('/', BS)
    if not rel.startswith('textures' + BS):
        rel = 'textures' + BS + rel
    for a in arcs:
        t = a.texture(rel)
        if t is not None:
            return t
    return None


def decode_bc4(blk):
    """(n, 8) uint8 BC4 blocks -> (n, 16) floats 0..1 (texel order row by row)."""
    r0 = blk[:, 0].astype(float)
    r1 = blk[:, 1].astype(float)
    bits = np.zeros(len(blk), np.uint64)
    for k in range(6):
        bits |= blk[:, 2 + k].astype(np.uint64) << np.uint64(8 * k)
    idx = np.stack([((bits >> np.uint64(3 * t)) & np.uint64(7)).astype(int) for t in range(16)], 1)
    pal = np.zeros((len(blk), 8))
    pal[:, 0], pal[:, 1] = r0, r1
    big = r0 > r1
    for k in range(2, 8):
        pal[:, k] = np.where(big, ((8 - k) * r0 + (k - 1) * r1) / 7.0, 0.0)
    for k in range(2, 6):
        pal[:, k] = np.where(big, pal[:, k], ((6 - k) * r0 + (k - 1) * r1) / 5.0)
    pal[:, 6] = np.where(big, pal[:, 6], 0.0)
    pal[:, 7] = np.where(big, pal[:, 7], 255.0)
    return np.take_along_axis(pal, idx, 1) / 255.0


def mip_chain(t, faces=1):
    """[face][mip] -> (h, w, 3) floats 0..1, RGB."""
    fmt, d = t['fmt'], t['data']
    out, off = [], 0
    for _f in range(faces):
        lv = []
        for m in range(t['mips']):
            w, h = max(1, t['w'] >> m), max(1, t['h'] >> m)
            if fmt in (87, 88, 28, 29, 91):         # B8G8R8A8 / B8G8R8X8 / R8G8B8A8 (UNORM; an _SRGB read as raw)
                n = w * h * 4
                a = np.frombuffer(d, np.uint8, n, off).reshape(h, w, 4)[..., :3].astype(float) / 255.0
                if fmt in (87, 88, 91):
                    a = a[..., ::-1]
                off += n
            elif fmt == 83:                         # BC5: red block then green block
                bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
                n = bw * bh * 16
                blk = np.frombuffer(d, np.uint8, n, off).reshape(-1, 16)
                r, g = decode_bc4(blk[:, :8]), decode_bc4(blk[:, 8:])
                img = np.zeros((bh * 4, bw * 4, 3))
                for ch, v in ((0, r), (1, g)):
                    img[..., ch] = v.reshape(bh, bw, 4, 4).transpose(0, 2, 1, 3).reshape(bh * 4, bw * 4)
                a = img[:h, :w]
                off += n
            else:
                return None
            lv.append(a)
        out.append(lv)
    return out


# ------------------------------------------------------------------ sampling

def bilinear_wrap(img, u, v):
    h, w = img.shape[:2]
    x, y = u * w - 0.5, v * h - 0.5
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[:, None], (y - y0)[:, None]
    xa, xb, ya, yb = x0 % w, (x0 + 1) % w, y0 % h, (y0 + 1) % h
    return ((img[ya, xa] * (1 - fx) + img[ya, xb] * fx) * (1 - fy) + (img[yb, xa] * (1 - fx) + img[yb, xb] * fx) * fy)


def cube_face(d):
    """The GL face table: face index (+x -x +y -y +z -z), s, t in 0..1."""
    ax = np.abs(d)
    face = np.where((ax[:, 0] >= ax[:, 1]) & (ax[:, 0] >= ax[:, 2]), np.where(d[:, 0] > 0, 0, 1),
                    np.where(ax[:, 1] >= ax[:, 2], np.where(d[:, 1] > 0, 2, 3), np.where(d[:, 2] > 0, 4, 5)))
    x, y, z = d[:, 0], d[:, 1], d[:, 2]
    sc = np.choose(face, [-z, z, x, x, x, -x])
    tc = np.choose(face, [-y, -y, z, -z, -y, -y])
    ma = np.choose(face, [ax[:, 0], ax[:, 0], ax[:, 1], ax[:, 1], ax[:, 2], ax[:, 2]])
    return face, (sc / ma + 1) / 2, (tc / ma + 1) / 2


def cube_sample(chain, d, lod):
    """Trilinear inside one face; 'inside' is False where a bilinear footprint crosses a face edge."""
    face, s, t = cube_face(d)
    nm = len(chain[0])
    lod = np.clip(lod, 0, nm - 1)
    l0 = np.floor(lod).astype(int)
    l1 = np.minimum(l0 + 1, nm - 1)
    fr = (lod - l0)[:, None]
    res = np.zeros((len(d), 3))
    inside = np.ones(len(d), bool)
    for f in range(6):
        for m in range(nm):
            img = chain[f][m]
            n = img.shape[0]
            for sel, wgt in ((l0 == m, 1 - fr), (l1 == m, fr)):
                k = (face == f) & sel
                if not k.any():
                    continue
                x, y = s[k] * n - 0.5, t[k] * n - 0.5
                inside[k] &= (x >= 0) & (x <= n - 1) & (y >= 0) & (y <= n - 1)
                x, y = np.clip(x, 0, n - 1), np.clip(y, 0, n - 1)
                x0, y0 = np.minimum(np.floor(x).astype(int), n - 2 if n > 1 else 0), np.minimum(np.floor(y).astype(int), n - 2 if n > 1 else 0)
                x1, y1 = np.minimum(x0 + 1, n - 1), np.minimum(y0 + 1, n - 1)
                fx, fy = (x - x0)[:, None], (y - y0)[:, None]
                v = (img[y0, x0] * (1 - fx) + img[y0, x1] * fx) * (1 - fy) + (img[y1, x0] * (1 - fx) + img[y1, x1] * fx) * fy
                res[k] += v * wgt[k]
    return res, inside


def srgb_lin(c):
    return np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)


# ------------------------------------------------------------------ the check

def main(esm, shots, label, target, mode='legacy'):
    game = os.path.dirname(esm)
    data = os.environ.get('DATA', 'E:/Tools/Fallout 4/DataUnpacked/Data')
    tags = (2, 3, 4, 50, 51, 52, 53)
    img = {}
    for p in tags:
        f = os.path.join(shots, '%s.probe%d.png' % (label, p))
        if not os.path.exists(f):
            return 'cube FAIL %s: no probe %d picture' % (label, p)
        img[p] = np.asarray(Image.open(f).convert('RGB'), float)
    notes = open(os.path.join(shots, label + '.probe50.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        return 'cube FAIL %s: no "cell lighting ... center=" line in the notes' % label
    center = np.array([float(v) for v in m.groups()])
    cams = set()
    for p in tags:
        f = os.path.join(shots, '%s.probe%d.cam' % (label, p))
        c = re.search(r'cam=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', open(f).read()) if os.path.exists(f) else None
        if not c:
            return 'cube FAIL %s: no camera dump for probe %d' % (label, p)
        cams.add(c.groups())
    if len(cams) != 1:
        return 'cube FAIL %s: the probes saw %d different cameras' % (label, len(cams))
    cam = np.array([float(v) for v in cams.pop()])
    fwd = np.array([float(v) for v in target.split(',')]) - cam
    fwd /= np.linalg.norm(fwd)
    mats = {}
    tf = os.path.join(shots, label + '.probe51.cubetags')
    for ln in (open(tf, encoding='utf-8', errors='replace').read().splitlines() if os.path.exists(tf) else []):
        t = re.match(r'cubetag=(\d+)\|(.*)$', ln.strip())
        if t:
            mats[int(t.group(1))] = t.group(2)
    if not mats:
        return 'cube FAIL %s: no material tags dumped (probe 51 run)' % label

    q = np.round(img[2]) * 256 + np.round(img[3])
    P = q + 0.5 - 32768.0 + center
    N = (np.round(img[4]) + img[53] / 255.0 - 0.5) / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    U = (np.round(img[51][..., 0]) * 256 + np.round(img[51][..., 1])) / 65535.0
    Vt = (np.round(img[52][..., 0]) * 256 + np.round(img[52][..., 1])) / 65535.0
    tag = np.round(img[51][..., 2]).astype(int)
    ok = (tag > 0) & (tag == np.round(img[52][..., 2]).astype(int)) & (np.abs(nlen - 1) < 0.02)
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
        ok &= np.linalg.norm(N - np.roll(N, (dy, dx), (0, 1)), axis=2) < 0.06
        ok &= tag == np.roll(tag, (dy, dx), (0, 1))
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False

    arcs = archives(game)
    exp_lo, exp_hi, got_all, used, info = [], [], [], 0, []
    skipped = []
    rng = np.random.default_rng(1)
    for tg, path in sorted(mats.items()):
        ys, xs = np.nonzero(ok & (tag == tg))
        if len(ys) < 20:
            continue
        cand = [os.path.join(data, path), os.path.join(data, 'materials', path)]
        f = next((c for c in cand if os.path.exists(c)), None)
        mt = read_bgsm(open(f, 'rb').read()) if f else None
        if not mt or 'envmap' not in mt or not mt['envmap']:
            skipped.append('%s (no v1-2 BGSM with an env map)' % os.path.basename(path))
            continue
        ct = find_texture(arcs, mt['envmap'])
        cube = mip_chain(ct, 6) if ct and ct['cube'] else None
        if cube is None:
            skipped.append('%s (cube %s unreadable)' % (os.path.basename(path), mt['envmap']))
            continue
        if not mt['smoothspec']:
            sch = None
        else:
            st = find_texture(arcs, mt['smoothspec'])
            sch = mip_chain(st) if st else None
            if sch is None:
                skipped.append('%s (_s %s unreadable)' % (os.path.basename(path), mt['smoothspec']))
                continue
        pick = rng.choice(len(ys), size=min(4000, len(ys)), replace=False)
        ys, xs = ys[pick], xs[pick]
        Pp, Np = P[ys, xs], N[ys, xs] / nlen[ys, xs][:, None]
        Vd = cam[None, :] - Pp
        Vd /= np.linalg.norm(Vd, axis=1)[:, None]
        R = 2 * np.einsum('ij,ij->i', Np, Vd)[:, None] * Np - Vd
        d = R * np.array([1.0, 1.0, -1.0])          # the viewer's cube frame: world with z flipped (reflMatrix)
        depth = (Pp - cam) @ fwd
        lods = range(5) if sch is not None else range(1)
        vals = []
        inside_all = np.ones(len(ys), bool)
        for lv in lods:
            if sch is not None:
                sv = bilinear_wrap(sch[0][min(lv, len(sch[0]) - 1)], U[ys, xs], Vt[ys, xs])
                sr, sg = sv[:, 0], sv[:, 1]
            else:
                sr = sg = np.ones(len(ys))
            g = np.clip(mt['smooth'] * sg, 0, 1)
            spec = np.clip((mt['specMult'] if mt['specOn'] else 0.0) * sr, 0, 1)
            k = 3 * spec * np.minimum(np.sqrt(np.clip(g - 0.3, 0, 1)), 1) * min(max(mt['envScale'], 0), 50)
            lod = (1 - g) * 6 + depth / 512.0 + math.log2(len(cube[0][0]) / 128.0)
            c, inside = cube_sample(cube, d, lod)
            inside_all &= inside
            vals.append(np.clip(srgb_lin(c) * k[:, None] / 4.0, 0, 1))
        vals = np.stack(vals)
        lo, hi = vals.min(0), vals.max(0)
        steady = inside_all & np.all(hi - lo <= 6.0 / 255 + 0.05 * hi, axis=1)
        got = img[50][ys, xs] / 255.0
        exp_lo.append(lo[steady]); exp_hi.append(hi[steady]); got_all.append(got[steady])
        used += int(steady.sum())
        info.append('%s:%d' % (os.path.basename(path).rsplit('.', 1)[0], int(steady.sum())))
    if used < 500:
        return 'cube SKIP %s: %d steady env-mapped pixels, under 500 (%s; skipped %s)' % (
            label, used, ' '.join(info) or 'none', '; '.join(skipped) or 'none')
    lo, hi, got = np.concatenate(exp_lo), np.concatenate(exp_hi), np.concatenate(got_all)
    tol = 3.0 / 255 + 0.05 * hi
    err = np.maximum(np.maximum(lo - tol - got, got - hi - tol), 0)
    good = np.all(err <= 0, axis=1)
    lit = (hi.max(axis=1) > 0.02) | (got.max(axis=1) > 0.02)
    share = good.mean()
    lit_share = good[lit].mean() if lit.any() else 0.0
    mid = (lo + hi) / 2
    ratio = got[lit].sum() / max(mid[lit].sum(), 1e-6)
    verdict = share >= 0.97 and lit_share >= 0.95 and lit.sum() >= 200
    return ('cube %s %s (%s): %d steady pixels, %d lit; agree %.1f%% (lit %.1f%%); viewer/expected sum %.3f; '
            'mean |err| %.4f, p99 %.4f; materials %s%s'
            % ('PASS' if verdict else 'FAIL', label, mode, used, lit.sum(), 100 * share, 100 * lit_share, ratio,
               np.abs(got - mid).mean(), np.percentile(np.abs(got - mid), 99), ' '.join(info),
               ('; skipped ' + '; '.join(skipped)) if skipped else ''))


if __name__ == '__main__':
    if len(sys.argv) < 5:
        print(__doc__)
        sys.exit(2)
    line = main(*sys.argv[1:6])
    print(line)
    sys.exit(0 if ' PASS ' in line else 1)
