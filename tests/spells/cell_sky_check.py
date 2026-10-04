#!/usr/bin/env python3
"""THE SKY AND THE SUN IN THE BOUNCE ROW, REBUILT WITHOUT NIFSKOPE (lane SKY1, 2026-10-02; src/probesky.h).

  python tests/spells/cell_sky_check.py <Fallout4.esm> <run dir> [--weather W] [--hour H]
         [--view name=cx,cy,cz ...] [--red NAME] [--fresh]
  python tests/spells/cell_sky_check.py --interior <dir with before/ and after/>

The run dir is what tests/spells/cell_sky.sh writes for one exterior cell: bake/*.tbk, soup.psp, dump/ (the
relight's numbers, WW_CELL_GI_DUMP), lit.notes and views/<name>/probe{2,3,4,5,90}.png. With --red NAME the
subject is <run>/red_NAME/ (its own dump/ and views/<name>/probe5.png, probe90.png); the positions, the
normals and everything expected stay the green run's.

Nothing here calls NifSkope. The stages, each from the files on disk:
  W  the weather light: this file's own walk of the plugin's weather and climate records -> the six ambient
     colors a surface facing each axis takes, the sun's color and the sun's direction, against gi_sky.txt
  U  the sun at every surfel: one ray toward the sun through the soup (this file's own tracer), against gi_sun.bin
  S  the sky at every probe: the .tbk's eight visibilities x eight tints x the ambient, against gi_sky.bin
  T  the same on the probes whose sky comes through glass (SKIP when the cell has none)
  B  every probe's total: this file's own gather of the links (own sun part + the dump's placed-light part)
     + its own sky, against gi_probes.bin
  C  the voxel grid at the voxels the views look at, blended from this file's own probe totals with its own
     sight lines, against gi_grid.bin. With rooms (gi_slots.bin; lane ROOMCLAMP1) cell_gi_check's stage C
     rule instead (a slot gathers its room's probes only; the eye, twice the radius, the neighbours), fed this
     file's own probe totals
  D  per view, the picture: probe 5 (the bounce a surface takes / pi) and probe 90 (the share of the weather
     ambient the grid replaced) against this file's own grid: agree %, and the viewer's total over the
     expected total (0.95 .. 1.05). With rooms the expected is cell_rooms_check's gi_sample (the shader's
     room blend) of the dumped grid, which C has checked against this file's own totals
  O  open against covered: the sky the probes around each view's look-at point see, straight from the .tbk
  R  the replacement in the finished picture: lit.png against lit_keepamb.png (the weather ambient left in)

What is NOT rebuilt here: the placed lights' own light outdoors (tests/spells/cell_gi.sh gates that on
interiors); its share of the total is printed in B. The grid's place and size (origin, voxel, radius) are read
from the dump's header.
"""
import hashlib
import math
import os
import re
import struct
import sys
import zlib

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_gi_check import Soup, read_dump  # noqa: E402   (the checker's own soup and dump readers)
from probe_bake import read_tbk  # noqa: E402              (the checker's own .tbk reader)
import cell_gi_check  # noqa: E402   (lane ROOMCLAMP1: stage C's rooms rule)
import cell_rooms_check  # noqa: E402   (lane ROOMCLAMP1: the shader's room blend)

AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)
SURF_OFF = 2.0          # the sun ray starts this far off the surface
SUN_REACH = 400000.0    # ... and runs to beyond anything loaded

# bars, pre-registered from the first green and red measurements (see the gate's header)
BAR_AGREE = 0.95
BAR_SHOWS = 0.93
BAR_RATIO = (0.95, 1.05)
MIN_SHOW = 300


# ---------------------------------------------------------------- W: the plugin's weather, read here
ROWS = ["SkyUpper", "FogNear", "Unused", "Ambient", "Sunlight", "Sun", "Stars", "SkyLower", "Horizon",
        "EffectLighting", "CloudLODDiffuse", "CloudLODAmbient", "FogFar", "SkyStatics", "WaterMult", "SunGlare",
        "MoonGlare", "FogNearHigh", "FogFarHigh"]
TODS = ["Sunrise", "Day", "Sunset", "Night", "EarlySunrise", "LateSunrise", "EarlySunset", "LateSunset"]
FOLD4 = {0: 0, 1: 1, 2: 2, 3: 3, 4: 0, 5: 0, 6: 2, 7: 2}


def plugin_records(data, label):
    """(formID, formVersion, [(type, bytes)]) of every record in one top-level group."""
    hsz = struct.unpack_from('<I', data, 4)[0]
    pos = 24 + hsz
    while pos < len(data):
        typ, size, lab = struct.unpack_from('<4sI4s', data, pos)
        if typ != b'GRUP':
            return
        if lab == label:
            q = pos + 24
            while q < pos + size:
                t, sz, flags, fid = struct.unpack_from('<4sIII', data, q)
                if t == b'GRUP':
                    q += sz
                    continue
                fv = struct.unpack_from('<H', data, q + 20)[0]
                body = data[q + 24:q + 24 + sz]
                if flags & 0x00040000:
                    body = zlib.decompress(body[4:])
                fl, b, big = [], 0, None
                while b + 6 <= len(body):
                    ft, n = struct.unpack_from('<4sH', body, b)
                    b += 6
                    if ft == b'XXXX':
                        big = struct.unpack_from('<I', body, b)[0]
                        b += n
                        continue
                    if big is not None:
                        n, big = big, None
                    fl.append((ft, body[b:b + n]))
                    b += n
                yield fid, fv, fl
                q += 24 + sz
            return
        pos += size


def tod_keys(h, tn):
    """(a, b, t): the two times of day the hour sits between (the half-hour extension at both ends)."""
    r0, r1, s0, s1 = (x / 6.0 for x in tn)
    a0, a1 = r0 - 0.5, r1
    b0, b1 = s0, s1 + 0.5
    if h < a0 or h >= b1:
        return 3, 3, 0.0
    if h < a1:
        seq = [3, 4, 0, 5, 1]
        q = (a1 - a0) / 4.0
        i = min(int((h - a0) / q), 3)
        return seq[i], seq[i + 1], (h - a0 - i * q) / q
    if h <= b0:
        return 1, 1, 0.0
    seq = [1, 6, 2, 7, 3]
    q = (b1 - b0) / 4.0
    i = min(int((h - b0) / q), 3)
    return seq[i], seq[i + 1], (h - b0 - i * q) / q


def lin(byte):
    return np.clip(np.asarray(byte, float) / 255.0, 0, 1) ** 2.2


def sun_to(hour, tn):
    """The tent arc: x from +400 at sunrise to -400 at sunset, y 25, height 400 - |x|; then the light is
    lowered 15 degrees and never stands under 30 (the numbers in src/esmweather.cpp's header)."""
    rise0, set1 = tn[0] / 6.0, tn[3] / 6.0
    ramp = 1.0 - 2.0 * (hour - rise0) / max(1e-6, set1 - rise0)
    p = np.array([ramp * 400.0, 25.0, 400.0 - abs(ramp * 400.0)])
    p /= np.linalg.norm(p)
    deg = 0.0174532924
    z = max(p[2] - 15.0 * deg, 30.0 * deg)
    v = np.array([p[0], p[1], z])
    return v / np.linalg.norm(v)


def weather_light(esm, edid, hour):
    data = open(esm, 'rb').read()
    assert data[:4] == b'TES4', esm
    rec = None
    for fid, fv, fl in plugin_records(data, b'WTHR'):
        ed = next((v.rstrip(b'\0').decode('latin1') for t, v in fl if t == b'EDID'), '')
        if ed.lower() == edid.lower():
            rec = (fid, fv, fl)
            break
    if rec is None:
        raise SystemExit('weather %s is not in %s' % (edid, esm))
    tn, clim = None, ''
    for fid, fv, fl in plugin_records(data, b'CLMT'):
        if fid == 0x15F or tn is None:
            t = next((v for ty, v in fl if ty == b'TNAM'), None)
            if t is not None and len(t) >= 4:
                tn = tuple(t[:4])
                clim = next((v.rstrip(b'\0').decode('latin1') for ty, v in fl if ty == b'EDID'), '')
            if fid == 0x15F:
                break
    if tn is None:
        tn, clim = (30, 54, 102, 126), 'fallback'
    fid, fv, fl = rec
    n0 = next(v for t, v in fl if t == b'NAM0')
    rows = 19 if fv >= 119 else 17
    tods = 8 if fv >= 111 else 4
    assert len(n0) >= rows * tods * 4, 'NAM0 is %d bytes' % len(n0)

    def row(name, tod):
        tt = tod if tods == 8 else FOLD4[tod]
        o = (ROWS.index(name) * tods + tt) * 4
        return np.array(tuple(n0[o:o + 3]), float)
    a, b, t = tod_keys(hour, tn)
    sun = lin(row('Sunlight', a) * (1 - t) + row('Sunlight', b) * t)
    flat = lin(row('Ambient', a) * (1 - t) + row('Ambient', b) * t)
    dal = [v for ty, v in fl if ty == b'DALC']
    named = np.zeros((6, 3))            # the record's own order: X+ X- Y+ Y- Z+ Z-
    for ax in range(6):
        if dal:
            da = dal[a] if len(dal) == 8 else dal[FOLD4[a]]
            db = dal[b] if len(dal) == 8 else dal[FOLD4[b]]
            ca = np.array(tuple(da[ax * 4:ax * 4 + 3]), float)
            cb = np.array(tuple(db[ax * 4:ax * 4 + 3]), float)
            named[ax] = lin(ca * (1 - t) + cb * t)
        else:
            named[ax] = flat
    # a color is named for the way its light TRAVELS: "Z-" comes down from the sky, so a surface facing up
    # takes it (measured on the game's own picture by lane PBRR3; the viewer's red "dalcflip" is the other way)
    amb = np.stack([named[ax ^ 1] for ax in range(6)])
    return dict(amb=amb, sun=sun, sunTo=sun_to(hour, tn), keys=(TODS[a], TODS[b], t), tnam=tn, climate=clim,
                form=fid, dalc=len(dal), named=named)


def read_sky_txt(path):
    d = {'amb': np.zeros((6, 3))}
    for ln in open(path, encoding='utf-8', errors='replace'):
        w = ln.split()
        if not w:
            continue
        if w[0] == 'amb':
            d['amb'][int(w[1])] = [float(x) for x in w[2:5]]
        elif w[0] in ('sunTo', 'sun'):
            d[w[0]] = np.array([float(x) for x in w[1:4]])
        else:
            d[w[0]] = ' '.join(w[1:])
    return d


def close_rows(a, b, rel=0.02, ab=1e-3):
    """per row: every number within ab + rel x the larger"""
    a2, b2 = a.reshape(len(a), -1), b.reshape(len(b), -1)
    return np.all(np.abs(a2 - b2) <= ab + rel * np.maximum(np.abs(a2), np.abs(b2)), axis=1)


def stage_w(W, sub):
    p = os.path.join(sub, 'dump', 'gi_sky.txt')
    if not os.path.exists(p):
        return 'W FAIL weather light: the viewer wrote no gi_sky.txt (no sky in its relight)'
    v = read_sky_txt(p)
    okA = bool(np.all(close_rows(v['amb'], W['amb'], 0.01, 1e-4)))
    okS = bool(np.all(np.abs(v['sun'] - W['sun']) <= 1e-4 + 0.01 * W['sun']))
    ang = math.degrees(math.acos(min(1.0, float(v['sunTo'] @ W['sunTo']))))
    ok = okA and okS and ang < 0.1
    return ('W %s weather light: %s form %08X, %d DALC, climate %s %s, keys %s/%s t=%.2f; up-facing ambient '
            '%.4f %.4f %.4f (viewer %.4f %.4f %.4f), sun %.3f %.3f %.3f (viewer %.3f %.3f %.3f), sun direction '
            '%.1f deg up, %.3f deg from the viewer\'s'
            % (('PASS' if ok else 'FAIL'), v.get('label', '?'), W['form'], W['dalc'], W['climate'], W['tnam'],
               W['keys'][0], W['keys'][1], W['keys'][2], *W['amb'][4], *v['amb'][4], *W['sun'], *v['sun'],
               math.degrees(math.asin(W['sunTo'][2])), ang))


# ---------------------------------------------------------------- U: the sun at the surfels
class SunShade:
    """Every triangle flattened along the sun's direction and binned in that plane: a point is in shade when
    a triangle covers it there and lies farther toward the sun."""
    CELL = 96.0

    def __init__(self, tris, to):
        w = to / np.linalg.norm(to)
        u = np.cross(w, [0.0, 0.0, 1.0])
        if np.linalg.norm(u) < 1e-6:
            u = np.array([1.0, 0.0, 0.0])
        u /= np.linalg.norm(u)
        v = np.cross(w, u)
        self.M = np.stack([u, v, w])
        self.T = tris @ self.M.T                     # (n, 3 corners, [u v t])
        lo = np.floor(self.T[:, :, 0:2].min(1) / self.CELL).astype(np.int64)
        hi = np.floor(self.T[:, :, 0:2].max(1) / self.CELL).astype(np.int64)
        span = hi - lo + 1
        cnt = span.prod(1)
        tri = np.repeat(np.arange(len(tris)), cnt)
        start = np.repeat(np.cumsum(cnt) - cnt, cnt)
        k = np.arange(cnt.sum()) - start
        ix = lo[tri, 0] + k % span[tri, 0]
        iy = lo[tri, 1] + k // span[tri, 0]
        key = self.key(ix, iy)
        order = np.argsort(key, kind='stable')
        self.keys, first = np.unique(key[order], return_index=True)
        self.first = np.append(first, len(order))
        self.tris = tri[order]

    @staticmethod
    def key(ix, iy):
        return ((ix + (1 << 30)) << 32) | (iy + (1 << 30))

    def blocked(self, O):
        q = O @ self.M.T
        ck = self.key(np.floor(q[:, 0] / self.CELL).astype(np.int64), np.floor(q[:, 1] / self.CELL).astype(np.int64))
        out = np.zeros(len(O), bool)
        order = np.argsort(ck, kind='stable')
        uk, st = np.unique(ck[order], return_index=True)
        st = np.append(st, len(order))
        pos = np.searchsorted(self.keys, uk)
        for gi in range(len(uk)):
            p = pos[gi]
            if p >= len(self.keys) or self.keys[p] != uk[gi]:
                continue
            T = self.T[self.tris[self.first[p]:self.first[p + 1]]]
            a = T[:, 0, :]
            e1 = T[:, 1, :] - a
            e2 = T[:, 2, :] - a
            det = e1[:, 0] * e2[:, 1] - e1[:, 1] * e2[:, 0]
            good = np.abs(det) > 1e-9
            idet = np.where(good, 1.0 / np.where(good, det, 1.0), 0.0)
            rows = order[st[gi]:st[gi + 1]]
            for c0 in range(0, len(rows), 256):
                r = rows[c0:c0 + 256]
                dx = q[r, 0][:, None] - a[None, :, 0]
                dy = q[r, 1][:, None] - a[None, :, 1]
                uu = (dx * e2[None, :, 1] - dy * e2[None, :, 0]) * idet[None, :]
                vv = (e1[None, :, 0] * dy - e1[None, :, 1] * dx) * idet[None, :]
                th = a[None, :, 2] + uu * e1[None, :, 2] + vv * e2[None, :, 2]
                dt = th - q[r, 2][:, None]
                hit = good[None, :] & (uu >= 0) & (vv >= 0) & (uu + vv <= 1) & (dt > 1e-4) & (dt <= SUN_REACH)
                sp = getattr(self, 'soup', None)
                if sp is not None and sp.am is not None and hit.any():   # lane ALPHATEST1
                    ti = self.tris[self.first[p]:self.first[p + 1]]
                    rr, cc = np.nonzero(hit & (sp.amOf[ti] >= 0)[None, :])
                    if len(rr):
                        h = sp.holes(ti[cc], uu[rr, cc], vv[rr, cc])
                        hit[rr[h], cc[h]] = False
                out[r] = hit.any(1)
        return out


def own_sun(S, soup, W):
    """per surfel: the sun's part of what it sends back (albedo x sun color x N.L where the sun reaches)"""
    n, p = S[:, 3:6], S[:, 0:3]
    nl = n @ W['sunTo']
    sun = np.zeros((len(S), 3))
    faces = nl > 0
    shade = np.zeros(len(S), bool)
    if W['sunTo'][2] > 0 and W['sun'].max() > 0:
        sh = SunShade(soup.t, W['sunTo'])
        sh.soup = soup   # lane ALPHATEST1: hits on an alpha-test hole pass
        idx = np.nonzero(faces)[0]
        shade[idx] = sh.blocked(p[idx] + n[idx] * SURF_OFF)
        lit = faces & ~shade
        sun[lit] = S[lit, 6:9] * W['sun'][None, :] * nl[lit][:, None]
    return sun, faces, shade


def read_f32(path, per):
    b = open(path, 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    return np.frombuffer(b, '<f4', n * per, 4).reshape(n, per).astype(np.float64)


def stage_u(S, sunOwn, faces, shade, sub):
    p = os.path.join(sub, 'dump', 'gi_sun.bin')
    if not os.path.exists(p):
        return 'U FAIL sun: the viewer wrote no gi_sun.bin', None
    V = read_f32(p, 3)
    if len(V) != len(S):
        return 'U FAIL sun: the dump holds %d surfels, gi_sun.bin %d' % (len(S), len(V)), None
    good = close_rows(V, sunOwn)
    vlit = V.max(1) > 1e-6
    olit = sunOwn.max(1) > 1e-6
    through = int(np.sum(vlit & shade))          # the viewer lights a surfel this file's ray says is shaded
    lost = int(np.sum(~vlit & olit))
    nf = int(faces.sum())
    share = float(good[faces].mean()) if nf else 0.0
    ratio = float(V.sum() / max(sunOwn.sum(), 1e-9))
    ok = share >= 0.97 and int(shade.sum()) >= 50 and int(olit.sum()) >= 50 and 0.97 <= ratio <= 1.03
    return ('U %s sun at the surfels: %d face the sun, %d of them shaded here, %d sunlit; the viewer lights %d '
            'shaded ones and misses %d sunlit ones; agree %.1f%%, the viewer\'s total over this file\'s %.3f'
            % ('PASS' if ok else 'FAIL', nf, int(shade.sum()), int(olit.sum()), through, lost, 100 * share, ratio)), V


# ---------------------------------------------------------------- S, T: the sky at the probes
def own_sky(tbks, W, tint=True, vis=True):
    """per probe 6 x rgb: pi x the ambient a surface facing that axis takes x the mean over the four octants
    on that side of (the sky's share of the octant x the glass tint of its sky rays)"""
    out, flags = [], []
    for _, t in tbks:
        sky = np.clip(t['probes']['sky'].astype(np.float64), 0, 1)                # (n, 8)
        tn = t['pext']['skytint'].astype(np.float64) / 255.0                       # (n, 8, 3)
        if not vis:
            sky = np.ones_like(sky)
        if not tint:
            tn = np.ones_like(tn)
        E = np.zeros((len(sky), 6, 3))
        for a in range(6):
            ax, neg = a >> 1, a & 1
            octs = [o for o in range(8) if ((o >> ax) & 1) == neg]                 # bit 0 x<0, bit 1 y<0, bit 2 z<0
            E[:, a, :] = math.pi * W['amb'][a][None, :] * (sky[:, octs, None] * tn[:, octs, :]).mean(1)
        out.append(E)
        raw = t['pext']['skytint']
        flags.append(np.any((raw != 255).any(2) & (t['probes']['sky'] > 0), axis=1))
    return np.concatenate(out), np.concatenate(flags)


def stage_s(tbks, W, sub):
    own, tinted = own_sky(tbks, W)
    p = os.path.join(sub, 'dump', 'gi_sky.bin')
    if not os.path.exists(p):
        line = ('S FAIL sky at the probes: the viewer wrote no gi_sky.bin: it adds no sky, where %d of %d probes '
                'see sky here (mean up-facing sky %.4f)' % (int((own.max((1, 2)) > 1e-6).sum()), len(own), own[:, 4].mean()))
        return line, 'T FAIL sky through glass: no gi_sky.bin', own, None
    V = read_f32(p, 18).reshape(-1, 6, 3)
    if len(V) != len(own):
        return 'S FAIL sky at the probes: the bake holds %d probes, gi_sky.bin %d' % (len(own), len(V)), 'T FAIL', own, None
    good = close_rows(V, own, 0.01, 1e-4)
    sees = own.max((1, 2)) > 1e-6
    ratio = float(V.sum() / max(own.sum(), 1e-9))
    ok = good.mean() >= 0.99 and sees.sum() >= 20 and 0.99 <= ratio <= 1.01
    s = ('S %s sky at the probes: %d probes, %d see sky; agree %.1f%%, the viewer\'s total over this file\'s %.3f'
         % ('PASS' if ok else 'FAIL', len(own), int(sees.sum()), 100 * good.mean(), ratio))
    plain, _ = own_sky(tbks, W, tint=False)
    matters = tinted & ~close_rows(plain, own, 0.01, 1e-4)
    if matters.sum() < 10:
        t = ('T SKIP sky through glass: %d probes store a tint, on %d it moves the sky by over 1%% (under 10: '
             'this cell has no outdoor glass to test)' % (int(tinted.sum()), int(matters.sum())))
    else:
        g = good[matters]
        rt = float(V[matters].sum() / max(own[matters].sum(), 1e-9))
        ru = float(plain[matters].sum() / max(own[matters].sum(), 1e-9))
        okT = g.mean() >= 0.99 and 0.99 <= rt <= 1.01
        t = ('T %s sky through glass: %d probes take their sky through glass; agree %.1f%%, the viewer\'s total '
             'over this file\'s %.3f (with the tint ignored it would be %.3f)'
             % ('PASS' if okT else 'FAIL', int(matters.sum()), 100 * g.mean(), rt, ru))
    return s, t, own, V


# ---------------------------------------------------------------- B: the probes' totals
def unpack_dirs(d):
    x, y = d[:, 0] / 32767.0, d[:, 1] / 32767.0
    z = 1.0 - np.abs(x) - np.abs(y)
    neg = z < 0
    x2 = np.where(neg, (1.0 - np.abs(y)) * np.where(x >= 0, 1.0, -1.0), x)
    y2 = np.where(neg, (1.0 - np.abs(x)) * np.where(y >= 0, 1.0, -1.0), y)
    v = np.stack([x2, y2, z], 1)
    return v / np.linalg.norm(v, axis=1)[:, None]


def cell_keys(pos, cs):
    k = np.floor(pos.astype(np.float32) / np.float32(cs)).astype(np.int64)
    return k


def pack3(k):
    return ((k[:, 0] + (1 << 20)) << 42) | ((k[:, 1] + (1 << 20)) << 21) | (k[:, 2] + (1 << 20))


def dump_rows(tbks, S):
    """each .tbk surfel's row in the dump (one per position + normal); None when the dump is not the bake's"""
    got = {}
    for i in range(len(S)):
        got.setdefault(np.float32(S[i, 0:3]).astype('<f4').tobytes(), []).append(i)
    rows = []
    for _, t in tbks:
        pair = []
        for arr in (t['surfels'], t['back']):
            r = np.full(len(arr), -1, np.int64)
            nn = arr['nrm'].astype(np.float64)
            nn /= np.maximum(np.linalg.norm(nn, axis=1), 1e-9)[:, None]
            for j in range(len(arr)):
                for i in got.get(arr['pos'][j].astype('<f4').tobytes(), ()):
                    if np.allclose(S[i, 3:6], nn[j], atol=1e-5):
                        r[j] = i
                        break
            pair.append(r)
        rows.append(pair)
    return rows


def own_gather(tbks, rows, Bown, sky):
    """every probe's six-axis total: its links' surfels (tinted by the glass on the way, each weighted by the
    solid angle it stands for, the unlinked share filled with the linked mean) + its sky"""
    cubes, unresolved, k0 = [], 0, 0
    for (name, t), (rf, rb) in zip(tbks, rows):
        cs = float(t['cell'])
        maps = []
        for arr in (t['surfels'], t['back']):
            key = pack3(cell_keys(arr['pos'], cs)) if len(arr) else np.zeros(0, np.int64)
            uk, first = np.unique(key, return_index=True)      # the first surfel of a cell, as the reader keeps it
            maps.append((uk, first))
        pr = t['probes']
        n = len(pr)
        lp = np.full(len(t['links']), -1, np.int64)
        for i in range(n):
            lp[int(pr['off'][i]):int(pr['off'][i]) + int(pr['cnt'][i])] = i
        use = lp >= 0
        lk, lx, lpi = t['links'][use], t['lext'][use], lp[use]
        pk = cell_keys(pr['pos'], cs)
        key = pack3(pk[lpi] + lk['delta'].astype(np.int64))
        side = lx['side'].astype(bool)
        srow = np.full(len(lk), -1, np.int64)
        for sd, (uk, first), rr in ((False, maps[0], rf), (True, maps[1], rb)):
            m = side == sd
            if not m.any() or not len(uk):
                continue
            pos = np.searchsorted(uk, key[m])
            pos = np.clip(pos, 0, len(uk) - 1)
            hit = uk[pos] == key[m]
            r = np.where(hit, rr[first[pos]], -1)
            srow[np.nonzero(m)[0]] = r
        ok = srow >= 0
        unresolved += int((~ok).sum())
        B = Bown[srow[ok]] * (lx['tint'][ok].astype(np.float64) / 255.0)
        d = unpack_dirs(lk['dir'][ok].astype(np.float64))
        w = lk['w'][ok].astype(np.float64) * pr['scale'][lpi[ok]].astype(np.float64)
        cosA = np.maximum(d @ AXES.T, 0.0)                                  # (links, 6)
        Bw = B * (w * 4 * math.pi)[:, None]
        E = np.zeros((n, 6, 3))
        for a in range(6):
            for c in range(3):
                E[:, a, c] = np.bincount(lpi[ok], weights=cosA[:, a] * Bw[:, c], minlength=n)
        linked = np.bincount(lpi[ok], weights=w, minlength=n)
        unl = pr['unl'].astype(np.float64)
        k = np.where((linked > 0) & (unl > 0), (linked + unl) / np.maximum(linked, 1e-30), 1.0)
        E *= k[:, None, None]
        cubes.append(E + sky[k0:k0 + n])
        k0 += n
    return np.concatenate(cubes), unresolved


def stage_b(cubes, unresolved, P, sky, placedShare):
    V = P[:, 3:21].reshape(-1, 6, 3)
    if len(V) != len(cubes):
        return 'B FAIL probe totals: the dump holds %d probes, the bake %d' % (len(V), len(cubes))
    good = close_rows(V, cubes, 0.03, 2e-3)
    ratio = float(V.sum() / max(cubes.sum(), 1e-9))
    skyShare = float(sky.sum() / max(cubes.sum(), 1e-9))
    ok = good.mean() >= 0.97 and unresolved == 0 and 0.98 <= ratio <= 1.02
    return ('B %s probe totals: %d probes, %d links unresolved; agree %.1f%%, the viewer\'s total over this file\'s '
            '%.3f; of this file\'s total the sky is %.1f%%, the placed lights (taken from the dump) %.2f%%'
            % ('PASS' if ok else 'FAIL', len(V), unresolved, 100 * good.mean(), ratio, 100 * skyShare, 100 * placedShare))


# ---------------------------------------------------------------- C: the grid, at the voxels the views use
def tris_near(soup, lo, hi):
    lo = np.floor(lo / soup.CELL).astype(np.int64)
    hi = np.floor(hi / soup.CELL).astype(np.int64)
    g = np.stack(np.meshgrid(*[np.arange(lo[a], hi[a] + 1) for a in range(3)], indexing='ij'), -1).reshape(-1, 3)
    kk = soup.key(g[:, 0], g[:, 1], g[:, 2])
    pos = np.searchsorted(soup.keys, kk)
    inside = pos < len(soup.keys)
    pos, kk = pos[inside], kk[inside]
    pos = pos[soup.keys[pos] == kk]
    if not len(pos):
        return None
    ti = np.unique(np.concatenate([soup.tris[soup.first[p]:soup.first[p + 1]] for p in pos]))
    return soup.t[ti], ti


def segs_blocked(soup, c, Q):
    """c -> each row of Q: is a triangle in the way (double-sided, hits in (1e-4, length])"""
    out = np.zeros(len(Q), bool)
    near = tris_near(soup, np.minimum(c, Q.min(0)), np.maximum(c, Q.max(0)))
    if near is None:
        return out
    T, ti = near
    p0, e1, e2 = T[:, 0], T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]
    tv = c[None, :] - p0
    qv = np.cross(tv, e1)
    for r0 in range(0, len(Q), max(1, 2000000 // max(len(T), 1))):
        r = slice(r0, r0 + max(1, 2000000 // max(len(T), 1)))
        d = Q[r] - c[None, :]
        L = np.linalg.norm(d, axis=1)
        d = d / np.maximum(L, 1e-9)[:, None]
        pv = np.cross(d[:, None, :], e2[None, :, :])
        det = np.einsum('tk,rtk->rt', e1, pv)
        good = np.abs(det) >= 1e-12
        idt = np.where(good, 1.0 / np.where(good, det, 1.0), 0.0)
        u = np.einsum('tk,rtk->rt', tv, pv) * idt
        v = (d @ qv.T) * idt
        tt = np.einsum('tk,tk->t', e2, qv)[None, :] * idt
        hit = good & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (tt > 1e-4) & (tt <= L[:, None])
        if soup.am is not None and hit.any():   # lane ALPHATEST1: a hit on an alpha-test hole passes
            rr, cc = np.nonzero(hit & (soup.amOf[ti] >= 0)[None, :])
            if len(rr):
                h = soup.holes(ti[cc], u[rr, cc], v[rr, cc])
                hit[rr[h], cc[h]] = False
        out[r] = hit.any(1) & (L > 1e-3)
    return out


def near_mask(S, G):
    v, o, dims = G['voxel'], G['origin'], G['dims']
    g = np.floor((S[:, 0:3] + S[:, 3:6] * v * 0.5 - o) / v).astype(int)
    near = np.zeros((dims[2], dims[1], dims[0]), bool)
    for dz in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                q = g + np.array([dx, dy, dz])
                m = np.all((q >= 0) & (q < np.array(dims)), 1)
                near[q[m, 2], q[m, 1], q[m, 0]] = True
    return near


def own_voxels(ids, G, near, soup, pp):
    """for each voxel id (flat: (z * dy + y) * dx + x): the probes it can see and their blend weights"""
    v, o, dims, rad = G['voxel'], G['origin'], G['dims'], G['radius']
    ptr, idx, wts = [0], [], []
    rays = blocked = 0
    for f in ids:
        x, y, z = int(f % dims[0]), int((f // dims[0]) % dims[1]), int(f // (dims[0] * dims[1]))
        k = 0
        if near[z, y, x]:
            c = o + (np.array([x, y, z], float) + 0.5) * v
            d2 = np.sum((pp - c) ** 2, 1)
            j = np.nonzero(d2 < rad * rad)[0]
            if len(j):
                b = segs_blocked(soup, c, pp[j])
                rays += len(j)
                blocked += int(b.sum())
                j = j[~b]
                w = (1 - d2[j] / (rad * rad)) ** 2
                if w.sum() > 0:
                    idx.append(j)
                    wts.append(w / w.sum())
                    k = len(j)
        ptr.append(ptr[-1] + k)
    return (np.array(ptr, np.int64), np.concatenate(idx) if idx else np.zeros(0, np.int64),
            np.concatenate(wts) if wts else np.zeros(0), rays, blocked)


def voxel_values(ptr, idx, wts, cubes):
    """(n, 6, 3) E and (n,) valid for the voxels of own_voxels()"""
    n = len(ptr) - 1
    E = np.zeros((n, 6, 3))
    row = np.repeat(np.arange(n), np.diff(ptr))
    np.add.at(E, row, wts[:, None, None] * cubes[idx])
    return E, (np.diff(ptr) > 0).astype(np.float64)


def corner_ids(G, Pw, Nw):
    """the eight texels (per pixel) a linear sample at P + N x half a voxel reads, the same in every slab"""
    v, o, dims = G['voxel'], G['origin'], np.array(G['dims'], float)
    g = (Pw + Nw * (0.5 * v) - o) / v
    z = np.clip(g[:, 2], 0.5, dims[2] - 0.5)
    cx, cy, cz = g[:, 0] - 0.5, g[:, 1] - 0.5, z - 0.5
    ids, ws = [], []
    for ox in (0, 1):
        for oy in (0, 1):
            for oz in (0, 1):
                ix = np.clip(np.floor(cx).astype(np.int64) + ox, 0, int(dims[0]) - 1)
                iy = np.clip(np.floor(cy).astype(np.int64) + oy, 0, int(dims[1]) - 1)
                iz = np.clip(np.floor(cz).astype(np.int64) + oz, 0, int(dims[2]) - 1)
                fx, fy, fz = cx - np.floor(cx), cy - np.floor(cy), cz - np.floor(cz)
                ws.append((fx if ox else 1 - fx) * (fy if oy else 1 - fy) * (fz if oz else 1 - fz))
                ids.append((iz * int(dims[1]) + iy) * int(dims[0]) + ix)
    return np.stack(ids, 1), np.stack(ws, 1)


def sample_own(G, Pw, Nw, vid, vE, vA):
    """rgb = E x valid share, a = valid share: the three slabs a normal faces, weighted by n squared"""
    ids, ws = corner_ids(G, Pw, Nw)
    row = np.searchsorted(vid, ids)
    row = np.clip(row, 0, len(vid) - 1)
    assert np.all(vid[row] == ids), 'a texel outside the voxel set'
    a = np.sum(ws * vA[row], 1)
    n2 = Nw * Nw
    rgb = np.zeros((len(Pw), 3))
    for ax in range(3):
        slab = np.where(Nw[:, ax] >= 0, 2 * ax, 2 * ax + 1)
        e = vE[row, slab[:, None], :] * vA[row][:, :, None]             # (n, 8, 3)
        rgb += n2[:, ax:ax + 1] * np.sum(ws[:, :, None] * e, 1)
    return rgb, a * n2.sum(1)       # the valid share is the same in every slab


# ---------------------------------------------------------------- D: the pictures
def load_view(run, name):
    d = os.path.join(run, 'views', name)
    img = {p: np.asarray(Image.open(os.path.join(d, 'probe%d.png' % p)).convert('RGB'), float) for p in (2, 3, 4)}
    notes = open(os.path.join(run, 'lit.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        raise SystemExit('no "center=" in %s/lit.notes' % run)
    center = np.array([float(x) for x in m.groups()])
    P = np.round(img[2]) * 256 + np.round(img[3]) + 0.5 - 32768.0 + center
    N = img[4] / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    ok = np.abs(nlen - 1) < 0.06
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    ok &= np.any(img[2] != img[3], axis=2) | np.any(img[3] != img[4], axis=2)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
        ok &= np.linalg.norm(N - np.roll(N, (dy, dx), (0, 1)), axis=2) < 0.06 * 4
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    ys, xs = np.nonzero(ok)
    rng = np.random.default_rng(3)
    pick = rng.choice(len(ys), size=min(SAMPLE, len(ys)), replace=False) if len(ys) else np.zeros(0, int)
    ys, xs = ys[pick], xs[pick]
    return ys, xs, P[ys, xs], N[ys, xs] / np.maximum(nlen[ys, xs], 1e-9)[:, None], int(ok.sum())


SAMPLE = 4000


def variants(Pp, Np):
    """the pixel as read, then half a step of every rounded input: each position axis, each normal axis"""
    out = [(Pp, Np)]
    for ax in range(3):
        for s in (-1, 1):
            P2 = Pp.copy()
            P2[:, ax] += s * 0.5
            out.append((P2, Np))
            N2 = Np.copy()
            N2[:, ax] += s * 0.5 / 127.5
            N2 /= np.linalg.norm(N2, axis=1)[:, None]
            out.append((Pp, N2))
    return out


def view_expect(run, name, G, near, soup, pp, cubes, fresh):
    """what probe 5 and probe 90 must show at this view's clean pixels, and the rounding spread of each"""
    ys, xs, Pp, Np, clean = load_view(run, name)
    if len(ys) == 0:
        return None
    var = variants(Pp, Np)
    ids = np.unique(np.concatenate([corner_ids(G, p_, n_)[0].ravel() for p_, n_ in var]))
    h = hashlib.sha1()
    for f in ['soup.psp'] + ['views/%s/probe%d.png' % (name, k) for k in (2, 3, 4)] + ['dump/gi_grid.bin']:
        st = os.stat(os.path.join(run, f))
        h.update(('%s %d %d;' % (f, st.st_size, int(st.st_mtime))).encode())
    h.update(ids.tobytes())
    cache = os.path.join(run, 'views', name, 'own_voxels.npz')
    z = None
    if os.path.exists(cache) and not fresh:
        z = np.load(cache)
        if str(z['key']) != h.hexdigest():
            z = None
    if z is None:
        ptr, idx, wts, rays, blocked = own_voxels(ids, G, near, soup, pp)
        np.savez(cache, key=h.hexdigest(), ptr=ptr, idx=idx, wts=wts, rays=rays, blocked=blocked)
        z = dict(ptr=ptr, idx=idx, wts=wts, rays=rays, blocked=blocked)
    vE, vA = voxel_values(z['ptr'], z['idx'], z['wts'], cubes)
    res = []
    for p_, n_ in var:
        rgb, a = sample_own(G, p_, n_, ids, vE, vA)
        res.append((np.clip(rgb / math.pi, 0, 1), np.clip(a, 0, 1)))
    e5, e90 = res[0]
    s5 = np.max([np.abs(r[0] - e5) for r in res[1:]], 0)
    s90 = np.max([np.abs(r[1] - e90) for r in res[1:]], 0)
    return dict(ys=ys, xs=xs, P=Pp, N=Np, clean=clean, e5=e5, e90=e90, s5=s5, s90=s90, ids=ids, vE=vE, vA=vA,
                rays=int(z['rays']), blocked=int(z['blocked']))


def room_expect(X, G):
    """lane ROOMCLAMP1: with rooms, what probe 5 and probe 90 show is the shader's room blend of the dumped grid
    (cellGiSample: the surface's room's slots, renormalized; no room with weight: the plain trilinear). The sky
    grid's sample keeps rgb = E x share (no division), a = the share."""
    GG = {'origin': G['origin'], 'voxel': G['voxel'], 'dims': G['dims'], 'g': G['grid'], 'g2': G['grid2'],
          'slots': G['slots']}
    res = []
    for p_, n_ in variants(X['P'], X['N']):
        s = np.array([cell_rooms_check.gi_sample(GG, G['R'], p_[i], n_[i], fill=False) for i in range(len(p_))])   # a sky grid: no gap fill
        res.append((np.clip(np.maximum(s[:, 0:3], 0) / math.pi, 0, 1), np.clip(s[:, 3], 0, 1)))
    e5, e90 = res[0]
    X = dict(X)
    X['e5'], X['e90'] = e5, e90
    X['s5'] = np.max([np.abs(r[0] - e5) for r in res[1:]], 0)
    X['s90'] = np.max([np.abs(r[1] - e90) for r in res[1:]], 0)
    return X


def judge(got, exp, spread):
    tol = 3.0 / 255 + 0.05 * exp + spread
    good = np.abs(got - exp) <= tol
    if good.ndim == 2:
        good = good.all(1)
        shows = exp.max(1) > 0.02
    else:
        shows = exp > 0.02
    ratio = float(got[shows].sum() / max(exp[shows].sum(), 1e-9)) if shows.any() else 0.0
    return float(good.mean()), (float(good[shows].mean()) if shows.any() else 0.0), ratio, int(shows.sum())


def stage_d(name, X, sub):
    d = os.path.join(sub, 'views', name)
    lines = []
    for probe, exp, spread, what in ((5, X['e5'], X['s5'], 'bounce'), (90, X['e90'], X['s90'], 'ambient share')):
        f = os.path.join(d, 'probe%d.png' % probe)
        tag = 'D %s probe %d' % (name, probe)
        if not os.path.exists(f):
            lines.append('%s SKIP no picture' % tag)
            continue
        img = np.asarray(Image.open(f).convert('RGB'), float)[X['ys'], X['xs']] / 255.0
        got = img if probe == 5 else img[:, 0]
        share, sshare, ratio, nshow = judge(got, exp, spread)
        if nshow < MIN_SHOW:
            lines.append('%s SKIP %s: %d pixels show it, under %d' % (tag, what, nshow, MIN_SHOW))
            continue
        ok = share >= BAR_AGREE and sshare >= BAR_SHOWS and BAR_RATIO[0] <= ratio <= BAR_RATIO[1]
        lines.append('%s %s %s: %d clean pixels (%d judged, %d show it); agree %.1f%% (where it shows %.1f%%), '
                     'the viewer\'s total over the expected %.3f; mean expected %.4f, mean shown %.4f'
                     % (tag, 'PASS' if ok else 'FAIL', what, X['clean'], len(X['ys']), nshow, 100 * share,
                        100 * sshare, ratio, float(exp.mean()), float(got.mean())))
    return lines


def stage_c(views, G):
    """the dumped grid at the voxels the views read, against this file's own blend"""
    grid = G['grid']
    dims = G['dims']
    tot = good = lit = rays = blocked = 0
    gsum = osum = 0.0
    for X in views.values():
        ids = X['ids']
        x, y, z = ids % dims[0], (ids // dims[0]) % dims[1], ids // (dims[0] * dims[1])
        got = np.stack([grid[a, z, y, x, 0:3] for a in range(6)], 1)       # (n, 6, 3)
        gv = grid[0, z, y, x, 3]
        own = X['vE'] * X['vA'][:, None, None]
        ok = close_rows(got, own, 0.03, 2e-3) & ((gv > 0.5) == (X['vA'] > 0.5))
        tot += len(ids)
        good += int(ok.sum())
        lit += int((X['vA'] > 0.5).sum())
        rays += X['rays']
        blocked += X['blocked']
        gsum += float(got.sum())
        osum += float(own.sum())
    if tot == 0:
        return 'C SKIP voxel grid: no view'
    ratio = gsum / max(osum, 1e-9)
    ok = good / tot >= 0.97 and blocked >= 20 and 0.98 <= ratio <= 1.02
    return ('C %s voxel grid: %d voxels under the views (%d hold light), %d sight lines, %d blocked; agree %.1f%%, '
            'the viewer\'s total over this file\'s %.3f' % ('PASS' if ok else 'FAIL', tot, lit, rays, blocked,
                                                           100 * good / tot, ratio))


# ---------------------------------------------------------------- O: open against covered
def stage_o(tbks, W, sky, Vsky, specs):
    pr = np.concatenate([t['probes'] for _, t in tbks])
    pos = pr['pos'].astype(np.float64)
    up = np.clip(pr['sky'][:, 0:4].astype(np.float64), 0, 1).mean(1)        # the four octants with z > 0
    openv = float((math.pi * W['amb'][4]).sum())
    lines, vals = [], {}
    for name, c in specs:
        near = np.linalg.norm(pos[:, 0:2] - c[None, 0:2], axis=1) < O_RADIUS
        near &= np.abs(pos[:, 2] - c[2]) < 250
        if near.sum() < 3:
            lines.append('O SKIP %s: %d probes within %d units of the look-at point' % (name, int(near.sum()), O_RADIUS))
            continue
        own = float((sky[near, 4].sum(1) / openv).mean())
        viewer = float((Vsky[near, 4].sum(1) / openv).mean()) if Vsky is not None else 0.0
        vals[name] = (float(up[near].mean()), own, viewer)
        lines.append('O      %s: %d probes within %d units of the look-at point; their upper half sees %.3f sky (.tbk); '
                     'up-facing sky, as a share of the open sky: expected %.3f, the viewer %.3f'
                     % (name, int(near.sum()), O_RADIUS, up[near].mean(), own, viewer))
    if 'open' in vals and 'covered' in vals:
        o, c = vals['open'], vals['covered']
        ok = c[1] < 0.6 * o[1] and abs(c[2] - c[1]) < 0.02 and abs(o[2] - o[1]) < 0.02
        lines.append('O %s covered against open: the sky part must drop with the probes\' visibility: expected %.3f '
                     'against %.3f, the viewer %.3f against %.3f (bars: covered under 0.6 of the open, the viewer '
                     'within 0.02 of the expected at both)' % ('PASS' if ok else 'FAIL', c[1], o[1], c[2], o[2]))
    return lines


O_RADIUS = 300


# ---------------------------------------------------------------- R: the weather ambient, replaced
def stage_r(run, sub, name, X, tag='R'):
    d, ds = os.path.join(run, 'views', name), os.path.join(sub, 'views', name)
    lit, keep, p90 = os.path.join(ds, tag.lower() + '_lit.png'), os.path.join(d, tag.lower() + '_keepamb.png'), \
        os.path.join(d, 'probe90.png')
    if not (os.path.exists(lit) and os.path.exists(keep) and os.path.exists(p90)):
        return None
    a = np.asarray(Image.open(lit).convert('RGB'), float)[X['ys'], X['xs']]
    b = np.asarray(Image.open(keep).convert('RGB'), float)[X['ys'], X['xs']]
    s = X['e90']
    hi, lo = s > 0.9, s < 0.004
    drop = (b - a).sum(1)                                     # levels, summed over rgb
    if hi.sum() < MIN_SHOW:
        return '%s SKIP %s: %d pixels where the grid replaced the ambient' % (tag, name, int(hi.sum()))
    dh = float(drop[hi].mean())
    darker = float((drop[hi] > 0).mean())
    same = float((np.abs(a[lo] - b[lo]).max(1) <= 1).mean()) if lo.sum() >= 50 else float('nan')
    ok = darker >= 0.9 and dh >= 6.0 and (lo.sum() < 50 or same >= 0.97)
    return ('%s %s %s the weather ambient replaced in the finished picture: where the grid stands in (%d pixels) the '
            'picture is darker than with the ambient left in on %.1f%% of them, by %.1f levels (rgb sum) on average; '
            'where it does not (%d pixels) %s'
            % (tag, 'PASS' if ok else 'FAIL', name, int(hi.sum()), 100 * darker, dh, int(lo.sum()),
               'too few to judge' if lo.sum() < 50 else 'the two pictures are equal on %.1f%%' % (100 * same)))


# ---------------------------------------------------------------- lane SKYINT1: an interior's own flags
SHOW_SKY, USE_SKY_LIGHTING, SUNLIGHT_SHADOWS = 0x80, 0x100, 0x800
MIN_SKY_PROBES = 20     # as stage S outdoors: a Show Sky interior must have at least this many probes seeing sky


def cell_flags(esm, edid):
    """the interior CELL's DATA flags, this file's own walk of the plugin (nested groups, compressed records)"""
    data = open(esm, 'rb').read()
    pos = 24 + struct.unpack_from('<I', data, 4)[0]
    while pos + 24 <= len(data):
        typ, size = struct.unpack_from('<4sI', data, pos)
        if typ == b'GRUP':
            pos += 24
            continue
        if typ == b'CELL':
            flags = struct.unpack_from('<I', data, pos + 8)[0]
            body = data[pos + 24:pos + 24 + size]
            if flags & 0x00040000:
                body = zlib.decompress(body[4:])
            fl, q = {}, 0
            while q + 6 <= len(body):
                t, sz = struct.unpack_from('<4sH', body, q)
                fl.setdefault(t, body[q + 6:q + 6 + sz])
                q += 6 + sz
            if fl.get(b'EDID', b'').rstrip(b'\0').decode('latin1').lower() == edid.lower():
                d = fl.get(b'DATA', b'\0')
                return d[0] | (d[1] << 8 if len(d) > 1 else 0)
        pos += 24 + size
    raise SystemExit('no CELL %s in %s' % (edid, esm))


def flag_words(f):
    return 'flags %04X: Show Sky %s, Use Sky Lighting %s, Sunlight Shadows %s' % (
        f, 'yes' if f & SHOW_SKY else 'no', 'yes' if f & USE_SKY_LIGHTING else 'no', 'yes' if f & SUNLIGHT_SHADOWS else 'no')


def stage_v(tbks, f):
    """V: the .tbk's sky shares against the cell's Show Sky flag. Closed: every share exactly 0. Show Sky: at
    least MIN_SKY_PROBES probes see sky. The mean is the share of a probe's sphere that met nothing."""
    sky = np.concatenate([np.clip(t['probes']['sky'].astype(np.float64), 0, 1) for _, t in tbks])
    sphere = sky.sum(1) / 8.0
    sees = int((sky.max(1) > 0).sum())
    nums = ('%d probes, %d see sky; sphere share mean %.4f, p95 %.4f, worst %.4f'
            % (len(sky), sees, sphere.mean(), np.percentile(sphere, 95), sphere.max()))
    if f & SHOW_SKY:
        ok = sees >= MIN_SKY_PROBES
        return 'V %s sky in a Show Sky interior (%s): %s (at least %d must)' % (
            'PASS' if ok else 'FAIL', flag_words(f), nums, MIN_SKY_PROBES), ok
    ok = sees == 0
    return 'V %s no sky in a closed interior (%s): %s (none may)' % ('PASS' if ok else 'FAIL', flag_words(f), nums), ok


def sky_files(d):
    return [f for f in ('gi_sky.bin', 'gi_sun.bin', 'gi_sky.txt') if os.path.exists(os.path.join(d, 'dump', f))]


def stage_u_off(S, sub, f):
    """U for an interior whose flags keep the sun out (Use Sky Lighting + Sunlight Shadows not both set)"""
    p = os.path.join(sub, 'dump', 'gi_sun.bin')
    if not os.path.exists(p):
        return 'U FAIL sun: the viewer wrote no gi_sun.bin', None
    V = read_f32(p, 3)
    lit = int((V.max(1) > 1e-9).sum()) if len(V) == len(S) else -1
    ok = lit == 0
    return ('U %s sun kept out (%s): the viewer lights %d of %d surfels with the sun (none may)'
            % ('PASS' if ok else 'FAIL', flag_words(f), lit, len(S))), V


def main_interior(esm, run, edid, weather, hour):
    """lane SKYINT1: one interior run (bake/, soup.psp, dump/, lit.notes), judged against its own flags"""
    f = cell_flags(esm, edid)
    bake = os.path.join(run, 'bake')
    tbks = [(n, read_tbk(os.path.join(bake, n))) for n in sorted(os.listdir(bake)) if n.endswith('.tbk')]
    v, okV = stage_v(tbks, f)
    lines = [v]
    notes = os.path.join(run, 'lit.notes')
    said = open(notes, encoding='utf-8', errors='replace').read() if os.path.exists(notes) else ''
    m = re.search(r'cell flags 0x([0-9a-fA-F]{4})', said)
    okN = bool(m) and int(m.group(1), 16) == f
    lines.append('N %s the viewer read the cell\'s flags: %s, this file %04X' % (
        'PASS' if okN else 'FAIL', ('0x' + m.group(1)) if m else 'no "cell flags" line', f))
    if not f & SHOW_SKY:
        extra = sky_files(run)
        lines.append('I %s no sky file in a closed interior\'s dump%s' % (
            'PASS' if not extra else 'FAIL', (': ' + ' '.join(extra)) if extra else ''))
    else:
        S, P, G, _ = read_dump(os.path.join(run, 'dump'))
        soup = Soup(os.path.join(run, 'soup.psp'))
        W = weather_light(esm, weather, hour)
        lines.append(stage_w(W, run))
        sunIn = (f & (USE_SKY_LIGHTING | SUNLIGHT_SHADOWS)) == (USE_SKY_LIGHTING | SUNLIGHT_SHADOWS)
        if sunIn:
            sunOwn, faces, shade = own_sun(S, soup, W)
            u, Vsun = stage_u(S, sunOwn, faces, shade, run)
        else:
            sunOwn = np.zeros((len(S), 3))
            u, Vsun = stage_u_off(S, run, f)
        lines.append(u)
        s, t, sky, Vsky = stage_s(tbks, W, run)
        lines += [s, t]
        rows = dump_rows(tbks, S)
        missing = sum(int((r < 0).sum()) for pair in rows for r in pair)
        if missing:
            lines.append('B FAIL probe totals: %d of the bake\'s surfels are not in the dump' % missing)
        else:
            placed = S[:, 9:12] - (Vsun if Vsun is not None else 0.0)
            cubes, unresolved = own_gather(tbks, rows, placed + sunOwn, sky)
            onlyPlaced, _ = own_gather(tbks, rows, placed, np.zeros_like(sky))
            lines.append(stage_b(cubes, unresolved, P, sky, float(onlyPlaced.sum() / max(cubes.sum(), 1e-9))))
    for ln in lines:
        print(ln)
    bad = [ln for ln in lines if ' FAIL' in ln.split(':')[0]]
    judged = [ln for ln in lines if ' PASS' in ln.split(':')[0]]
    need = 6 if f & SHOW_SKY else 3
    ok = not bad and len(judged) >= need
    print('sky %s %s  (%d stages pass, %d fail)' % (edid, 'PASS' if ok else 'FAIL', len(judged), len(bad)))
    return 0 if ok else 1


# ---------------------------------------------------------------- an interior: nothing may move
def interior(d, flags=None):
    lines, ok = [], True
    a, b = os.path.join(d, 'before'), os.path.join(d, 'after')
    for f in ('probe5.png', 'lit.png'):
        pa, pb = os.path.join(a, f), os.path.join(b, f)
        if not (os.path.exists(pa) and os.path.exists(pb)):
            lines.append('I FAIL %s: missing' % f)
            ok = False
            continue
        ia, ib = np.asarray(Image.open(pa).convert('RGB')), np.asarray(Image.open(pb).convert('RGB'))
        diff = int(np.any(ia != ib, axis=2).sum()) if ia.shape == ib.shape else -1
        lit = int(np.any(ia != ia[0, 0], axis=2).sum())
        good = diff == 0 and lit >= 2000
        ok &= good
        lines.append('I %s %s: %d of %d pixels differ between the exe before the lane and after (%d pixels are not background)'
                     % ('PASS' if good else 'FAIL', f, diff, ia.shape[0] * ia.shape[1], lit))
    for f in ('gi_surfels.bin', 'gi_probes.bin', 'gi_grid.bin'):
        pa, pb = os.path.join(a, 'dump', f), os.path.join(b, 'dump', f)
        if not (os.path.exists(pa) and os.path.exists(pb)):
            lines.append('I FAIL %s: missing' % f)
            ok = False
            continue
        ba, bb = open(pa, 'rb').read(), open(pb, 'rb').read()
        good = ba == bb and len(ba) > 64
        ok &= good
        lines.append('I %s %s: %d bytes, %s' % ('PASS' if good else 'FAIL', f, len(ba),
                                                'byte for byte the same' if ba == bb else 'DIFFERENT (%d bytes after)' % len(bb)))
    # lane SKYINT1: no sky file only where the cell's Show Sky flag is clear (this arm's cell must be closed)
    extra = sky_files(b)
    closed = flags is None or not flags & SHOW_SKY
    ok &= not extra and closed
    lines.append('I %s no sky file in a closed interior\'s dump%s%s' % (
        'PASS' if not extra and closed else 'FAIL', (': ' + ' '.join(extra)) if extra else '',
        '' if flags is None else ' (%s%s)' % (flag_words(flags), '' if closed else ': this arm needs a closed cell')))
    return lines, ok


# ---------------------------------------------------------------- main
def main(argv):
    if argv and argv[0] == '--interior':   # --interior <dir> [<esm> <edid>]
        lines, ok = interior(argv[1], cell_flags(argv[2], argv[3]) if len(argv) >= 4 else None)
        for ln in lines:
            print(ln)
        print('sky interior %s' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1
    esm, run = argv[0], argv[1]
    weather, hour, red, fresh, specs, cell = 'CommonwealthClear', 12.0, '', False, [], ''
    i = 2
    while i < len(argv):
        if argv[i] == '--weather':
            weather = argv[i + 1]
            i += 2
        elif argv[i] == '--hour':
            hour = float(argv[i + 1])
            i += 2
        elif argv[i] == '--red':
            red = argv[i + 1]
            i += 2
        elif argv[i] == '--view':
            n, c = argv[i + 1].split('=')
            specs.append((n, np.array([float(x) for x in c.split(',')])))
            i += 2
        elif argv[i] == '--fresh':
            fresh = True
            i += 1
        elif argv[i] == '--cell':   # lane SKYINT1: an interior run, judged by its own flags
            cell = argv[i + 1]
            i += 2
        else:
            raise SystemExit('unknown argument ' + argv[i])
    if cell:
        return main_interior(esm, run, cell, weather, hour)
    sub = os.path.join(run, 'red_' + red) if red else run
    bake = os.path.join(run, 'bake')
    tbks = [(f, read_tbk(os.path.join(bake, f))) for f in sorted(os.listdir(bake)) if f.endswith('.tbk')]
    S, P, G, _ = read_dump(os.path.join(sub, 'dump'))
    soup = Soup(os.path.join(run, 'soup.psp'))
    W = weather_light(esm, weather, hour)
    lines = [stage_w(W, sub)]

    sunOwn, faces, shade = own_sun(S, soup, W)
    u, Vsun = stage_u(S, sunOwn, faces, shade, sub)
    lines.append(u)
    s, t, sky, Vsky = stage_s(tbks, W, sub)
    lines += [s, t]

    rows = dump_rows(tbks, S)
    missing = sum(int((r < 0).sum()) for pair in rows for r in pair)
    if missing:
        lines.append('B FAIL probe totals: %d of the bake\'s surfels are not in the dump' % missing)
        cubes = None
    else:
        placed = S[:, 9:12] - (Vsun if Vsun is not None else 0.0)        # the placed lights' part: the dump's
        Bown = placed + sunOwn
        cubes, unresolved = own_gather(tbks, rows, Bown, sky)
        onlyPlaced, _ = own_gather(tbks, rows, placed, np.zeros_like(sky))
        lines.append(stage_b(cubes, unresolved, P, sky, float(onlyPlaced.sum() / max(cubes.sum(), 1e-9))))

    views = {}
    if cubes is not None and specs:
        near = near_mask(S, G)
        for name, _ in specs:
            if not os.path.exists(os.path.join(run, 'views', name, 'probe4.png')):
                lines.append('D %s SKIP no pictures' % name)
                continue
            X = view_expect(run, name, G, near, soup, P[:, 0:3], cubes, fresh)
            if X is None:
                lines.append('D %s FAIL no clean pixel' % name)
                continue
            views[name] = X
        if 'R' in G:
            # lane ROOMCLAMP1: the grid by the rooms rule from this file's own totals, the pictures by the room blend
            sp = S[:, 0:3] + S[:, 3:6] * G['voxel'] * 0.5
            g = np.floor((sp - G['origin']) / G['voxel']).astype(int)
            lines.append(cell_gi_check.stage_c_rooms(S, P, G, soup, sp, g, True, vals=cubes))
            views = {name: room_expect(X, G) for name, X in views.items()}
        else:
            lines.append(stage_c(views, G))
        for name, X in views.items():
            lines += stage_d(name, X, sub)
            for tag in ('R', 'RP'):
                r = stage_r(run, sub, name, X, tag)
                if r:
                    lines.append(r)
    lines += stage_o(tbks, W, sky, Vsky, specs)
    for ln in lines:
        print(ln)
    bad = [ln for ln in lines if ' FAIL' in ln.split(':')[0]]
    judged = [ln for ln in lines if ' PASS' in ln.split(':')[0]]
    # lane GICAL1: a cell given no camera (VIEWS_<tag> unset) runs no picture stage; W U S B are all it can judge
    # (T only with outdoor glass), so it needs 4, not 5 -- Goodneighbor gn:5,-3 read FAIL with 4 PASS and 0 FAIL
    need = 5 if specs else 4
    shot = [ln for ln in judged if ln.startswith('D ')]   # a camera given must be judged on its pictures
    ok = not bad and len(judged) >= need and (not specs or bool(shot))
    print('sky %s  (%d stages pass, %d fail%s)' % ('PASS' if ok else 'FAIL', len(judged), len(bad),
                                                   '' if specs else '; no camera given, picture stages not run'))
    return 0 if ok else 1


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
