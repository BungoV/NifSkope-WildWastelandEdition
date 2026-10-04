"""EMISSIVEGI1 cell twin: do the glowing surfaces of a real cell light its bake, once and masked? (2026-10-03)

Independent of the exe's sampler. Reads a run dir (soup.psp with its 'EMT1' tail, bake/*.tbk with their emissive
tails, dump/gi_probes.bin) and, with --off, the same cell baked with WW_CELL_EMISSIVE_PIN=off:

  MAPS   each glow map named in EMT1 is decoded HERE from the loose DDS (impostor_bc_decode's BC1 colour decoder;
         BC3's colour half; Pillow for BC7 at mip 0) at the rule's mip (the largest of at most 1024 texels) and
         compared with the RGB bytes the exe wrote: >= 99% of texels within 3 levels, per map read here
  SURF   every surfel of the bake next to a glowing triangle (its 70-unit cell within one cell of one), glowing
         in the bake or not (the bake writes Le only where a hit glowed and its bins hold a few hits, so the glowing
         ones alone are a sample biased high), re-estimated here: the soup's triangles in the surfel's cell sampled
         on a 6 x 6 barycentric grid, each sample's Le = (glowColor x glowMult x glowMap.rgb)^2 sampled HERE
         (nearest texel, wrapped), 0 for a triangle that does not glow; alpha-test holes dropped; each sample
         weighted as the nearest 16 probes' uniform rays land on it (area x |cos| / d^2, in the surfel's normal
         bin = dominant axis of the face normal turned toward the probe, the segment to the probe tested against
         the neighbourhood's triangles with rays through their holes). Gate on the sums: sum(bake) / sum(twin) in
         [0.6, 1.6]; and no glowing surfel lies away from every glowing triangle (ORPHAN = 0)
  GAIN   the probes within 400 units of a glowing triangle: their irradiance (the 6-axis sum) with the glow
         against --off: the share that gains > 0.1% and the mean gain; must gain (share >= 0.5)
  OFF    the --off run: no 'EMT1' tail, no glowing surfel in any .tbk
Red: --red <run dir> (WW_CELL_EMISSIVE_RED=nomask) judged against the green soup's masks: must FAIL SURF (the
whole quad glows: the sum ratio leaves [0.6, 1.6]); printed with its numbers.

usage: python emissive_cell_check.py <run dir> --off <run dir> [--red <run dir>] [--data <data root>]
"""
import glob
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import impostor_bc_decode as bcd          # noqa: E402
from probe_bake import read_tbk           # noqa: E402

SOUP, ALB, GLS, TWO, AMK, EMT = 0x31505350, 0x31424C41, 0x31534C47, 0x314F5754, 0x314B4D41, 0x31544D45
CELL = 70.0
NEAR = 400.0
GRID = 6
DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'


def read_soup(path):
    b = open(path, 'rb').read()
    magic, ntri, ndoor = struct.unpack_from('<3I', b, 0)
    assert magic == SOUP, 'not a PSP1 soup'
    tris = np.frombuffer(b, '<f4', ntri * 9, 12).reshape(ntri, 9).astype(np.float64)
    p = 12 + ntri * 36 + ndoor * 28
    am = em = None

    def s32():
        nonlocal p
        n, = struct.unpack_from('<I', b, p)
        p += 4
        s = b[p:p + n].decode()
        p += n
        return s
    while p + 8 <= len(b):
        m, n = struct.unpack_from('<II', b, p)
        p += 8
        if m == ALB:
            p += n * 3
        elif m == GLS:
            p += n * 39
        elif m == TWO:
            p += n
        else:
            # lane LAND5: every tail after TWO1 is sized (u32 body bytes); one this reader does not use is skipped
            nb, = struct.unpack_from('<I', b, p)
            p += 4
            end = p + nb
            if end > len(b):
                break
            if m in (AMK, 0x324B4D41):   # AMK1, or lane ALPHATEST2's AMK2 in its slot (LAND6)
                am = {'maps': []}
                nm, = struct.unpack_from('<I', b, p); p += 4
                for _ in range(nm):
                    s32()
                    w, h = struct.unpack_from('<II', b, p); p += 8
                    am['maps'].append(np.frombuffer(b, 'u1', w * h, p).reshape(h, w)); p += w * h
                nmod, = struct.unpack_from('<I', b, p); p += 4
                for _ in range(nmod):
                    s32()
                from alphatest_check import amk_records
                am['rec'] = amk_records(b, p, n, m == 0x324B4D41)
                p += n * (52 if m == 0x324B4D41 else 40)
                assert p == end, 'AMK tail does not fill its %d bytes' % nb
            elif m == EMT:
                em = {'maps': [], 'names': []}
                nm, = struct.unpack_from('<I', b, p); p += 4
                for _ in range(nm):
                    em['names'].append(s32())
                    w, h = struct.unpack_from('<II', b, p); p += 8
                    em['maps'].append(np.frombuffer(b, 'u1', w * h * 3, p).reshape(h, w, 3)); p += w * h * 3
                ne, = struct.unpack_from('<I', b, p); p += 4
                em['emitters'] = np.frombuffer(b, np.dtype([('e', '<f4', 3), ('map', '<i4')]), ne, p); p += 16 * ne
                em['rec'] = np.frombuffer(b, np.dtype([('tri', '<u4'), ('emitter', '<i4'), ('uv', '<f4', 6)]), n, p)
                p += 32 * n
                assert p == end, 'EMT1 tail does not fill its %d bytes' % nb
            p = end
    return tris, am, em


def tex_path(data, name):
    n = name.replace('\\', '/')
    if not n.lower().startswith('textures/'):
        n = 'textures/' + n
    cur = data
    for part in n.split('/'):
        try:
            hit = [e for e in os.listdir(cur) if e.lower() == part.lower()]
        except OSError:
            return None
        if not hit:
            return None
        cur = os.path.join(cur, hit[0])
    return cur


def dds_rgb(path, cap=1024):
    """RGB 0..255 at the rule's mip, decoded here; None = a format this twin lacks."""
    b = open(path, 'rb').read()
    h = struct.unpack('<31I', b[4:128])
    ht, wd, mips = h[2], h[3], max(h[6], 1)
    fourcc = b[84:88]
    off = 0
    while off + 1 < mips and (max(wd, ht) >> (off + 1)) >= cap:
        off += 1
    hdr = 128
    if fourcc == b'DX10':
        dxgi = struct.unpack_from('<I', b, 128)[0]
        hdr = 148
        if dxgi in (98, 99) and off == 0:
            from PIL import Image
            return np.asarray(Image.open(path).convert('RGB')).astype(np.float64), 'BC7'
        if dxgi not in (77, 78, 71, 72):
            return None, 'dxgi %d' % dxgi
        fourcc = b'DXT5' if dxgi in (77, 78) else b'DXT1'
    if fourcc not in (b'DXT5', b'DXT1'):
        return None, repr(fourcc)
    bs = 16 if fourcc == b'DXT5' else 8
    p = hdr
    w, hh = wd, ht
    for _ in range(off):
        p += max(1, (w + 3) // 4) * max(1, (hh + 3) // 4) * bs
        w, hh = max(1, w // 2), max(1, hh // 2)
    bw, bh = (w + 3) // 4, (hh + 3) // 4
    d = np.frombuffer(b[p:p + bw * bh * bs], np.uint8).reshape(-1, bs)
    C = bcd._bc1_colour(np.ascontiguousarray(d[:, 8:16] if bs == 16 else d)) * 255.0
    C = C.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 3)
    return C[:hh, :w], fourcc.decode()


def sample(img, u, v):
    """nearest texel, wrapped (the exe's rule, written here)"""
    h, w = img.shape[:2]
    u = u - np.floor(u)
    v = v - np.floor(v)
    x = np.clip((u * w).astype(np.int64), 0, w - 1)
    y = np.clip((v * h).astype(np.int64), 0, h - 1)
    return img[y, x]


def surfels_of(run, every=False):
    """the glowing surfels (deduplicated by position + normal); every=True: every surfel, Le 0 where none glowed"""
    glow = {}
    for f in sorted(glob.glob(os.path.join(run, 'bake', '*.tbk'))):
        t = read_tbk(f)
        if every:
            for lst in (t['surfels'], t['back']):
                for s in lst:
                    glow[(tuple(np.round(s['pos'], 2)), tuple(int(x) for x in s['nrm']))] = np.zeros(3)
        for e in t['emits']:
            lst = t['back'] if e['surfel'] & 0x80000000 else t['surfels']
            s = lst[e['surfel'] & 0x7fffffff]
            k = (tuple(np.round(s['pos'], 2)), tuple(int(x) for x in s['nrm']))
            glow[k] = np.array(e['le'], np.float64)
    return glow


def has_emt(run):
    return read_soup(os.path.join(run, 'soup.psp'))[2] is not None


def probes(run):
    b = open(os.path.join(run, 'dump', 'gi_probes.bin'), 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    return np.frombuffer(b, '<f4', n * 21, 4).reshape(n, 21).astype(np.float64)


def main():
    a = sys.argv[1:]
    run = a[0]
    opt = {a[i]: a[i + 1] for i in range(1, len(a) - 1, 2)}
    data = opt.get('--data', DATA)
    lines, fails = [], []
    say = lambda s: (lines.append(s), print(s, flush=True))
    tris, am, em = read_soup(os.path.join(run, 'soup.psp'))
    if em is None:
        print('VERDICT FAIL: no EMT1 tail (nothing glows in this soup)')
        return 1
    rec, emi = em['rec'], em['emitters']
    say('soup %d triangles, glowing %d, emitters %d, glow maps %d' % (len(tris), len(rec), len(emi), len(em['maps'])))

    # ---- MAPS
    for name, mp in zip(em['names'], em['maps']):
        path = tex_path(data, name)
        C, fmt = (None, 'missing') if path is None else dds_rgb(path)
        if C is None:
            say('  MAPS %s: not read here (%s)' % (name, fmt))
            continue
        if C.shape[:2] != mp.shape[:2]:
            fails.append('MAPS size %s' % name)
            say('  MAPS FAIL %s: twin %s exe %s' % (name, C.shape[:2], mp.shape[:2]))
            continue
        ok = float(np.mean(np.abs(C - mp).max(-1) <= 3.0))
        say('  MAPS %s %s %dx%d: texels within 3 levels %.4f' % ('PASS' if ok >= 0.99 else 'FAIL', name, mp.shape[1],
                                                              mp.shape[0], ok))
        if ok < 0.99:
            fails.append('MAPS %s' % name)

    # ---- the twin's Le on soup triangles
    n_t = len(tris)
    em_of = np.full(n_t, -1, np.int64)
    em_of[rec['tri']] = np.arange(len(rec))
    am_of = None
    if am is not None:
        am_of = np.full(n_t, -1, np.int64)
        am_of[am['rec']['tri']] = np.arange(len(am['rec']))
    gb = np.array([(i + 0.5) / GRID for i in range(GRID)])
    B1, B2 = np.meshgrid(gb, gb)
    keep = (B1 + B2) <= 1.0
    B1, B2 = B1[keep], B2[keep]       # barycentrics inside the triangle
    B0 = 1.0 - B1 - B2

    def twin_le(sel):
        """per triangle in sel: sample points (k, 3), weights (area / k), Le (k, 3), kept (not a hole)"""
        T = tris[sel].reshape(-1, 3, 3)
        P = B0[None, :, None] * T[:, None, 0] + B1[None, :, None] * T[:, None, 1] + B2[None, :, None] * T[:, None, 2]
        nrm = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
        area = 0.5 * np.linalg.norm(nrm, axis=1)
        nrm = nrm / np.maximum(2 * area[:, None], 1e-12)
        L = np.zeros(P.shape)
        solid = np.ones(P.shape[:2], bool)
        for j, ti in enumerate(sel):
            r = em_of[ti]
            if r >= 0:
                uv = rec['uv'][r].astype(np.float64)
                u = B0 * uv[0] + B1 * uv[2] + B2 * uv[4]
                v = B0 * uv[1] + B1 * uv[3] + B2 * uv[5]
                e = emi['e'][rec['emitter'][r]].astype(np.float64)
                mi = emi['map'][rec['emitter'][r]]
                g = sample(em['maps'][mi], u, v) / 255.0 if mi >= 0 else np.ones((len(u), 3))
                L[j] = (e[None] * g) ** 2
            if am_of is not None and am_of[ti] >= 0:
                ar = am['rec'][am_of[ti]]
                uv = ar['uv'].astype(np.float64)
                u = B0 * uv[0] + B1 * uv[2] + B2 * uv[4]
                v = B0 * uv[1] + B1 * uv[3] + B2 * uv[5]
                sc = B0 * float(ar['as'][0]) + B1 * float(ar['as'][1]) + B2 * float(ar['as'][2])   # lane ALPHATEST2
                solid[j] = sample(am['maps'][ar['map']], u, v) * sc >= ar['thr']
        return P, area / len(B0), L, solid, nrm

    # triangle centroid keys, to find each surfel cell's triangles
    cen = tris.reshape(-1, 3, 3).mean(1)
    ck = np.floor(cen / CELL).astype(np.int64)
    glow_keys = {tuple(k) for k in ck[rec['tri']]}

    Q = probes(run)[:, :3]
    REACH, NPROBE = 2000.0, 16

    def blocked_local(O, q, Ts, own, tsel):
        """segments O -> q against the triangles Ts (= tris[tsel], Moller-Trumbore); own = each point's own
        triangle (index into Ts), skipped; a hit on an alpha-test hole goes on through, as in the bake"""
        out = np.zeros(len(O), bool)
        e1 = Ts[:, 1] - Ts[:, 0]
        e2 = Ts[:, 2] - Ts[:, 0]
        for k in range(0, len(O), 512):
            o = O[k:k + 512]
            d = q[None] - o                                         # t in (eps, 1)
            pv = np.cross(d[:, None], e2[None])                    # (n, T, 3)
            det = (e1[None] * pv).sum(-1)
            okd = np.abs(det) > 1e-9
            inv = np.where(okd, 1.0 / np.where(okd, det, 1.0), 0.0)
            tv = o[:, None] - Ts[None, :, 0]
            u = (tv * pv).sum(-1) * inv
            qv = np.cross(tv, e1[None])
            v = (d[:, None] * qv).sum(-1) * inv
            t = (e2[None] * qv).sum(-1) * inv
            h = okd & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-3) & (t < 1 - 1e-3)
            h[np.arange(len(o)), own[k:k + 512]] = False
            if am_of is not None:
                r, c = np.nonzero(h & (am_of[tsel] >= 0)[None, :])
                if len(r):
                    ar = am['rec'][am_of[tsel[c]]]
                    uv = ar['uv'].astype(np.float64)
                    b1, b2 = u[r, c], v[r, c]
                    b0 = 1.0 - b1 - b2
                    uu = b0 * uv[:, 0] + b1 * uv[:, 2] + b2 * uv[:, 4]
                    vv = b0 * uv[:, 1] + b1 * uv[:, 3] + b2 * uv[:, 5]
                    hole = np.zeros(len(r), bool)
                    for mi in np.unique(ar['map']):
                        g = ar['map'] == mi
                        sc = b0 * ar['as'][:, 0] + b1 * ar['as'][:, 1] + b2 * ar['as'][:, 2]   # lane ALPHATEST2
                        hole[g] = sample(am['maps'][mi], uu[g], vv[g]) * sc[g] < ar['thr'][g]
                    h[r[hole], c[hole]] = False
            out[k:k + 512] = h.any(1)
        return out

    gk = np.array(sorted(glow_keys), np.int64)

    def judge(runx, tag):
        # every surfel next to a glowing triangle, glowing in the bake or not: the bake writes Le only where a
        # hit glowed, and its bins hold a handful of hits, so judging the glowing ones alone is biased high
        glow = surfels_of(runx, every=True)
        if not any(le.any() for le in glow.values()):
            say('  SURF %s: no glowing surfel in the bake' % tag)
            return None, 0
        bake_sum = np.zeros(3)
        twin_sum = np.zeros(3)
        orphan = judged = 0
        for (pos, nq), le in glow.items():
            key = np.floor(np.array(pos) / CELL).astype(np.int64)
            n = np.array(nq, np.float64)
            n /= max(np.linalg.norm(n), 1e-9)
            if not np.any(np.all(np.abs(gk - key) <= 1, axis=1)):
                if le.any():
                    orphan += 1
                    bake_sum += le
                continue
            judged += 1
            near = np.all(np.abs(ck - key) <= 1, axis=1)
            sel = np.nonzero(near)[0]
            P, w, L, solid, tn = twin_le(sel)
            inside = np.all(np.floor(P / CELL).astype(np.int64) == key, axis=2)
            # the bake bins a hit by the dominant axis of the face normal turned toward the probe, and its
            # hits fall on a point as a probe's uniform rays do: |cos| / d^2 from each probe (no occlusion here)
            ax = int(np.argmax(np.abs(n)))
            sg = 1.0 if n[ax] >= 0 else -1.0
            tax = np.argmax(np.abs(tn), axis=1)
            Wp = np.zeros(P.shape[:2])
            dq = np.linalg.norm(Q - np.array(pos), axis=1)
            near_p = Q[np.argsort(dq)[:NPROBE]]
            near_p = near_p[np.linalg.norm(near_p - np.array(pos), axis=1) < REACH]
            Ts = tris[sel].reshape(-1, 3, 3)
            live = inside & solid
            for q in near_p:
                D = P - q[None, None]
                d2 = np.maximum((D * D).sum(-1), 1.0)
                c = (D * tn[:, None]).sum(-1)                       # tn . (P - q)
                fn_ax = -np.sign(c) * tn[np.arange(len(tn)), tax][:, None]   # the turned normal's sign on its axis
                ok_bin = (tax[:, None] == ax) & (fn_ax * sg > 0) & live
                ww = np.where(ok_bin, np.abs(c) / np.sqrt(d2) / d2, 0.0)
                if ok_bin.any():   # the cell's own neighbourhood occludes (a tube in front of its backing board)
                    ii = np.nonzero(ok_bin)
                    ww[ii] *= ~blocked_local(P[ii], q, Ts, ii[0], sel)
                Wp += ww
            m = inside & solid
            W = (w[:, None] * m * Wp)
            if W.sum() <= 0:
                orphan += int(le.any())
                bake_sum += le
                continue
            tw = (W[..., None] * L).sum((0, 1)) / W.sum()
            if os.environ.get('EMC_DEBUG'):
                gshare = (W * (L.sum(-1) > 0)).sum() / W.sum()
                print('    surfel %s n%s bake %s twin %s glowing share %.3f samples %d'
                      % (np.round(pos), nq, np.round(le, 3), np.round(tw, 3), gshare, int((W > 0).sum())))
            twin_sum += tw
            bake_sum += le
        ratio = bake_sum.sum() / max(twin_sum.sum(), 1e-12)
        say('  SURF %s: surfels next to a glowing triangle %d (glowing in the bake %d), Le summed bake %.4g twin %.4g, '
            'ratio %.3f, ORPHAN %d' % (tag, judged, sum(1 for le in glow.values() if le.any()), bake_sum.sum(),
                                       twin_sum.sum(), ratio, orphan))
        return ratio, orphan

    ratio, orphan = judge(run, 'green')
    ok = ratio is not None and 0.6 <= ratio <= 1.6 and orphan == 0
    say('  SURF %s' % ('PASS' if ok else 'FAIL'))
    if not ok:
        fails.append('SURF')

    # ---- GAIN and OFF
    if '--off' in opt:
        off = opt['--off']
        Pg, Po = probes(run), probes(off)
        if len(Pg) != len(Po) or not np.allclose(Pg[:, :3], Po[:, :3]):
            fails.append('GAIN probes differ')
            say('  GAIN FAIL: the two runs placed different probes (%d, %d)' % (len(Pg), len(Po)))
        else:
            gp = tris[rec['tri']].reshape(-1, 3, 3).reshape(-1, 3)
            d = np.full(len(Pg), np.inf)
            for i in range(0, len(gp), 4096):
                d = np.minimum(d, np.sqrt(((Pg[:, None, :3] - gp[None, i:i + 4096]) ** 2).sum(-1)).min(1))
            nearp = d <= NEAR
            Eg, Eo = Pg[:, 3:].sum(1), Po[:, 3:].sum(1)
            rel = (Eg - Eo) / np.maximum(Eo, 1e-6)
            share = float(np.mean(rel[nearp] > 1e-3)) if nearp.any() else 0.0
            darker = int(np.sum(rel < -1e-4))
            say('  GAIN %s: probes within %d of a glowing triangle %d of %d; gain > 0.1%% %.3f; mean gain %.4f; '
                'median %.4f; far probes (> %d) mean gain %.5f; probes darker anywhere %d'
                % ('PASS' if share >= 0.5 and darker == 0 else 'FAIL', NEAR, int(nearp.sum()), len(Pg), share,
                   float(rel[nearp].mean()) if nearp.any() else 0, float(np.median(rel[nearp])) if nearp.any() else 0,
                   int(NEAR), float(rel[~nearp].mean()) if (~nearp).any() else 0, darker))
            if share < 0.5 or darker:
                fails.append('GAIN')
        g_off = surfels_of(off)
        e_off = has_emt(off)
        say('  OFF %s: EMT1 tail %s, glowing surfels %d' % ('PASS' if not g_off and not e_off else 'FAIL',
                                                            'present' if e_off else 'absent', len(g_off)))
        if g_off or e_off:
            fails.append('OFF')

    # ---- red
    if '--red' in opt:
        r, o = judge(opt['--red'], 'red')
        red_fails = r is None or not (0.6 <= r <= 1.6) or o
        say('  RED nomask %s SURF (%s)' % ('FAILS' if red_fails else 'PASSES -- the gate cannot see it',
                                          'ratio %.3f' % r if r is not None else 'no surfel'))
        if not red_fails:
            fails.append('RED not caught')
    say('VERDICT %s%s' % ('PASS' if not fails else 'FAIL', '' if not fails else ': ' + ', '.join(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
