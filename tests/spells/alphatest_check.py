"""ALPHATEST1 twin: does the probe bake honour alpha-test masks? (lane ALPHATEST1, 2026-10-03)

Independent of the exe's sampler: the maps named in the soup's AMK1 tail are decoded here from the
loose DDS files (tests/spells/impostor_bc_decode.py's BC1/BC3 decoder, Pillow for BC7), at the mip the
rule names (the largest of at most 1024 texels across), and compared with the bytes the exe wrote. Then
every pixel of a cube dump (WW_CELL_BAKE_CUBE_DUMP, the bake's own cube way) is re-intersected with the
masked triangles only (Moller-Trumbore in float64), and the mask is sampled HERE at the twin's own hit:

  HOLEHIT   the pixel's hit (|t - dist| <= EPS) is a masked triangle on a hole texel   -> must be 0
  PASSSOLID the ray crossed a masked triangle on a solid texel before its hit          -> must be 0
            (both ignore texels whose 3x3 neighbourhood mixes holes and solid: the exe's barycentrics
             and these round differently there; the count of those is printed)
  FRONT     rays through a hole of the model matching --front end on a shape whose material or model
            matches --front-behind (from the WW_CELL_PROBE_SOUP_SHAPES list), within --front-gap units
  CORNER    rays through a hole of the model matching --corner travel more than --corner-reach units on

The red (WW_CELL_ALPHATEST_PIN=off) soup carries no AMK1 tail: pass --masks <green soup.psp>; the masked
triangles' geometry must equal the red soup's (checked).

usage: python alphatest_check.py <run dir> [--masks <soup.psp>] [--data <data root>] [--front S --front-behind S
       --front-gap G --corner S --corner-reach R] [--expect green|red]
"""
import os, struct, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import impostor_bc_decode as bcd

EPS = 0.05
SOUP, ALB, GLS, TWO, AMK = 0x31505350, 0x31424C41, 0x31534C47, 0x314F5754, 0x314B4D41


def read_soup(path, want_tris=None):
    b = open(path, 'rb').read()
    magic, ntri, ndoor = struct.unpack_from('<3I', b, 0)
    assert magic == SOUP, 'not a PSP1 soup'
    tris = np.frombuffer(b, '<f4', ntri * 9, 12).reshape(ntri, 9)
    p = 12 + ntri * 36 + ndoor * 28
    am = None
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
            if m == AMK:
                am = {'maps': [], 'names': [], 'models': []}
                nm, = struct.unpack_from('<I', b, p); p += 4
                for _ in range(nm):
                    ln, = struct.unpack_from('<I', b, p); p += 4
                    am['names'].append(b[p:p + ln].decode()); p += ln
                    w, h = struct.unpack_from('<II', b, p); p += 8
                    am['maps'].append(np.frombuffer(b, 'u1', w * h, p).reshape(h, w)); p += w * h
                nmod, = struct.unpack_from('<I', b, p); p += 4
                for _ in range(nmod):
                    ln, = struct.unpack_from('<I', b, p); p += 4
                    am['models'].append(b[p:p + ln].decode()); p += ln
                rec = np.frombuffer(b, np.dtype([('tri', '<u4'), ('map', '<i4'), ('model', '<i4'), ('thr', '<u4'),
                                                 ('uv', '<f4', 6)]), n, p)
                p += n * 40
                am['rec'] = rec
                assert p == end, 'AMK1 tail does not fill its %d bytes' % nb
            p = end
    return ntri, tris, am


def dds_mip(path, cap=1024):
    """The map's alpha (0..255 floats) at the rule's mip, decoded here; None = a format this twin lacks."""
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
        if dxgi == 98 and off == 0:
            from PIL import Image
            img = np.asarray(Image.open(path).convert('RGBA'))
            return img[..., 3].astype(np.float64), 'BC7'
        if dxgi not in (77, 71):
            return None, 'dxgi %d' % dxgi
        fourcc = b'DXT5' if dxgi == 77 else b'DXT1'
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
    if fourcc == b'DXT5':
        A = bcd._bc3_alpha(d[:, :8]) * 255.0
    else:
        # BC1 with c0 <= c1 and index 3 is transparent
        c0 = d[:, 0].astype(np.uint16) | (d[:, 1].astype(np.uint16) << 8)
        c1 = d[:, 2].astype(np.uint16) | (d[:, 3].astype(np.uint16) << 8)
        bits = (d[:, 4].astype(np.uint32) | (d[:, 5].astype(np.uint32) << 8) | (d[:, 6].astype(np.uint32) << 16)
                | (d[:, 7].astype(np.uint32) << 24))
        idx = np.stack([(bits >> (2 * k)) & 3 for k in range(16)], -1)
        A = np.where((c0 <= c1)[:, None] & (idx == 3), 0.0, 255.0).reshape(-1, 4, 4)
    A = A.reshape(bh, bw, 4, 4).transpose(0, 2, 1, 3).reshape(bh * 4, bw * 4)
    return A[:hh, :w], fourcc.decode()


def tex_path(data, name):
    n = name.replace('\\', '/')
    if not n.lower().startswith('textures/'):
        n = 'textures/' + n
    p = os.path.join(data, n)
    if os.path.exists(p):
        return p
    # case-insensitive walk (the loose tree's case differs from the material's)
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


def cube_dirs(F):
    NP = 6 * F * F
    px = np.arange(NP)
    face, r = px // (F * F), px % (F * F)
    x, y = r % F, r // F
    u = 2.0 * (x + 0.5) / F - 1.0
    v = 1.0 - 2.0 * (y + 0.5) / F
    FW = np.array([[1, 0, 0], [0, -1, 0], [-1, 0, 0], [0, 1, 0]], float)
    f = np.zeros((NP, 3)); rr = np.zeros((NP, 3)); up = np.zeros((NP, 3))
    s = face < 4
    f[s] = FW[face[s]]
    rr[s, 0] = f[s, 1]; rr[s, 1] = -f[s, 0]; up[s, 2] = 1
    s = face == 4
    f[s] = [0, 0, 1]; rr[s] = [0, -1, 0]; up[s] = [-1, 0, 0]
    s = face == 5
    f[s] = [0, 0, -1]; rr[s] = [0, -1, 0]; up[s] = [1, 0, 0]
    q = f + u[:, None] * rr + v[:, None] * up
    return q / np.linalg.norm(q, axis=1)[:, None]


def read_dump(run):
    meta = [l.split('\t') for l in open(os.path.join(run, 'cube.dump.txt')).read().split('\n') if l.strip()]
    b = open(os.path.join(run, 'cube.dump'), 'rb').read()
    out, p = [], 0
    for m in meta:
        F = int(m[4]); NP = 6 * F * F
        rgb = np.frombuffer(b, 'u1', NP * 3, p).reshape(NP, 3); p += NP * 3
        dist = np.frombuffer(b, '<f4', NP, p).astype(np.float64); p += NP * 4
        p += NP * 4 + NP * 12
        out.append((int(m[0]), np.array([float(m[1]), float(m[2]), float(m[3])]), F, rgb, dist))
    return out


def read_shapes(run):
    """WW_CELL_PROBE_SOUP_SHAPES rows: first triangle, count, model, material, map -> (starts, rows)"""
    p = os.path.join(run, 'soup_shapes.tsv')
    if not os.path.exists(p):
        return None
    rows = [l.split('\t') for l in open(p, encoding='utf-8').read().split('\n')[1:] if l.strip()]
    st = np.array([int(r[0]) for r in rows])
    order = np.argsort(st)
    return st[order], [rows[i] for i in order]


def shape_of(shapes, tri):
    st, rows = shapes
    i = np.searchsorted(st, tri, side='right') - 1
    if i < 0:
        return None
    r = rows[i]
    return r if tri < int(r[0]) + int(r[1]) else None


def main(argv):
    run = argv[0]
    opt = dict(zip(argv[1::2][: len(argv[1:]) // 2], argv[2::2]))
    data = opt.get('--data', 'E:/Tools/Fallout 4/DataUnpacked/Data')
    ntri, tris, am = read_soup(os.path.join(run, 'soup.psp'))
    lines, fails = [], []
    say = lines.append
    if '--masks' in opt:
        mt, mtris, am = read_soup(opt['--masks'])
        if mt != ntri:
            fails.append('mask soup has %d triangles, this soup %d' % (mt, ntri))
        elif am is not None:
            sel = am['rec']['tri'].astype(np.int64)
            same = np.array_equal(mtris[sel], tris[sel])
            say('masks from %s: %d masked triangles, geometry %s' % (opt['--masks'], len(sel), 'identical' if same else 'DIFFERS'))
            if not same:
                fails.append('masked triangles differ between the soups')
    if am is None:
        say('VERDICT FAIL: no AMK1 tail (no masked triangle)')
        return '\n'.join(lines), False
    rec = am['rec']
    say('soup %d triangles, masked %d, maps %d, models %d' % (ntri, len(rec), len(am['maps']), len(am['models'])))
    # ---- 1. the maps, decoded here
    twin = []
    for i, (name, mp) in enumerate(zip(am['names'], am['maps'])):
        path = tex_path(data, name)
        A, fmt = (None, 'missing') if path is None else dds_mip(path)
        if A is None or A.shape != mp.shape:
            say('  map %d %s: twin cannot decode (%s, shape %s vs %s) -- its triangles judged by the exe bytes'
                % (i, name, fmt, None if A is None else A.shape, mp.shape))
            twin.append(mp.astype(np.float64))
            continue
        a8 = np.clip(np.rint(A), 0, 255)
        diff = np.abs(a8 - mp.astype(np.float64))
        say('  map %d %s %s %dx%d: texels equal %.4f%%, max diff %d, holes(<128) %.1f%%'
            % (i, name, fmt, mp.shape[1], mp.shape[0], 100.0 * (diff == 0).mean(), diff.max(), 100.0 * (mp < 128).mean()))
        if (diff > 1).mean() > 0.001:
            fails.append('map %s: twin and exe disagree on %.3f%% of texels' % (name, 100.0 * (diff > 1).mean()))
        twin.append(a8)
    # ---- 2. the cube pixels against the masked triangles
    T = tris[rec['tri'].astype(np.int64)].astype(np.float64).reshape(-1, 3, 3)
    uv = rec['uv'].astype(np.float64).reshape(-1, 3, 2)
    thr = rec['thr'].astype(np.float64)
    mapi = rec['map'].astype(np.int64)
    modi = rec['model'].astype(np.int64)
    shapes = read_shapes(run)
    front, corner = opt.get('--front', '').lower(), opt.get('--corner', '').lower()
    cmap = opt.get('--corner-map', '').lower()   # only holes of this map count for CORNER (its glass)
    behind = opt.get('--front-behind', '').lower()
    gap, reach = float(opt.get('--front-gap', 8)), float(opt.get('--corner-reach', 100))
    tot = dict(hole=0, holeedge=0, passsolid=0, passedge=0, crossed=0)
    fr = dict(n=0, ok=0, beh=0); co = dict(n=0, ok=0)
    FACES = [((1, 0, 0), (0, -1, 0), (0, 0, 1)), ((0, -1, 0), (-1, 0, 0), (0, 0, 1)),
             ((-1, 0, 0), (0, 1, 0), (0, 0, 1)), ((0, 1, 0), (1, 0, 0), (0, 0, 1)),
             ((0, 0, 1), (0, -1, 0), (-1, 0, 0)), ((0, 0, -1), (0, -1, 0), (1, 0, 0))]
    for pi, P, F, rgb, dist in read_dump(run):
        D = cube_dirs(F)
        hitd = np.where(dist > 0, dist, np.inf)
        first_t = np.full(len(D), np.inf); first_k = np.full(len(D), -1)
        # every (triangle, pixel) pair whose pixel can see the triangle: per face, the bbox of the
        # vertices' projections when all three lie in front of P (exact: the projection is convex);
        # the whole face when the triangle straddles the face's plane through P
        W = T - P[None, None, :]
        hk, hn, hb1, hb2, ht_ = [], [], [], [], []
        npairs = 0
        CH = 1 << 19   # pairs per chunk: the pair list is streamed, never held whole (19:05 guard kill at 8.3 GB)

        def test(k, n):
            v0 = T[k, 0]; e1 = T[k, 1] - v0; e2 = T[k, 2] - v0; d = D[n]
            pv = np.cross(d, e2)
            det = (e1 * pv).sum(1)
            ok = np.abs(det) > 1e-12
            idt = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
            tv = P - v0
            bu = (tv * pv).sum(1) * idt
            qv = np.cross(tv, e1)
            bv = (d * qv).sum(1) * idt
            t = (e2 * qv).sum(1) * idt
            hit = ok & (bu >= 0) & (bu <= 1) & (bv >= 0) & (bu + bv <= 1) & (t > 1e-3) & (t <= hitd[n] + 8 * EPS)
            hk.append(k[hit]); hn.append(n[hit]); hb1.append(bu[hit]); hb2.append(bv[hit]); ht_.append(t[hit])

        for f, (fw, rr, up) in enumerate(FACES):
            fw, rr, up = np.array(fw, float), np.array(rr, float), np.array(up, float)
            z = W @ fw
            front_all = (z > 1e-9).all(1)
            some = (z > 1e-9).any(1)
            zz = np.where(z > 1e-9, z, 1.0)
            u = (W @ rr) / zz; v = (W @ up) / zz
            x = (u + 1) * 0.5 * F - 0.5; y = (1 - v) * 0.5 * F - 0.5
            x0 = np.where(front_all, np.floor(x.min(1)) - 1, 0).clip(0, F - 1).astype(np.int64)
            x1 = np.where(front_all, np.ceil(x.max(1)) + 1, F - 1).clip(0, F - 1).astype(np.int64)
            y0 = np.where(front_all, np.floor(y.min(1)) - 1, 0).clip(0, F - 1).astype(np.int64)
            y1 = np.where(front_all, np.ceil(y.max(1)) + 1, F - 1).clip(0, F - 1).astype(np.int64)
            inside = some & ~(front_all & ((x.max(1) < -1) | (x.min(1) > F) | (y.max(1) < -1) | (y.min(1) > F)))
            ks = np.nonzero(inside)[0]
            if not len(ks):
                continue
            areas = (x1[ks] - x0[ks] + 1) * (y1[ks] - y0[ks] + 1)
            npairs += int(areas.sum())
            # groups of triangles whose pairs fit one chunk (a triangle bigger than a chunk is a group alone)
            cum = np.cumsum(areas)
            starts = [0]
            while starts[-1] < len(ks):
                base = cum[starts[-1] - 1] if starts[-1] else 0
                nxt = int(np.searchsorted(cum, base + CH, side='right'))
                starts.append(max(nxt, starts[-1] + 1))
            for g0, g1 in zip(starts[:-1], starts[1:]):
                k = ks[g0:g1]; area = areas[g0:g1]
                wd = x1[k] - x0[k] + 1
                tk = np.repeat(k, area)
                off = np.arange(area.sum()) - np.repeat(np.cumsum(area) - area, area)
                wr = np.repeat(wd, area)
                px = np.repeat(x0[k], area) + off % wr
                py = np.repeat(y0[k], area) + off // wr
                test(tk, f * F * F + py * F + px)
        say('probe %d at %s: %d triangle-pixel pairs to test' % (pi, P.tolist(), npairs))
        kk = np.concatenate(hk); nn = np.concatenate(hn); b1 = np.concatenate(hb1); b2 = np.concatenate(hb2)
        tt = np.concatenate(ht_)
        b0 = 1 - b1 - b2
        U = b0 * uv[kk, 0, 0] + b1 * uv[kk, 1, 0] + b2 * uv[kk, 2, 0]
        V = b0 * uv[kk, 0, 1] + b1 * uv[kk, 1, 1] + b2 * uv[kk, 2, 1]
        a = np.zeros(len(kk), bool); edge = np.zeros(len(kk), bool)
        for mi in np.unique(mapi[kk]):
            s = mapi[kk] == mi
            mp = twin[mi]; h_, w_ = mp.shape
            th = thr[kk[s]]
            x = np.clip(((U[s] - np.floor(U[s])) * w_).astype(np.int64), 0, w_ - 1)
            y = np.clip(((V[s] - np.floor(V[s])) * h_).astype(np.int64), 0, h_ - 1)
            a[s] = mp[y, x] < th
            anyh = np.zeros(s.sum(), bool); allh = np.ones(s.sum(), bool)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    q = mp[(y + dy) % h_, (x + dx) % w_] < th
                    anyh |= q; allh &= q
            edge[s] = anyh != allh
        # the soup stores float32 world positions (about 0.004 units of rounding at 60000): at a grazing ray the
        # depth the exe and this twin compute for one plane can differ by that over the incidence cosine
        tn = np.cross(T[kk, 1] - T[kk, 0], T[kk, 2] - T[kk, 0])
        cosi = np.abs((tn * D[nn]).sum(1)) / np.maximum(np.linalg.norm(tn, axis=1), 1e-12)
        tol = EPS + 0.008 / np.maximum(cosi, 1e-3)
        fr_ = tt <= hitd[nn] + tol          # crossings behind the exe's hit are not this ray's business
        kk, nn, b1, b2, tt, a, edge, cosi, tol = (q[fr_] for q in (kk, nn, b1, b2, tt, a, edge, cosi, tol))
        at_hit = np.abs(tt - hitd[nn]) <= tol
        tot['crossed'] += len(kk)
        # a hole crossing at the hit is the exe's fault only when nothing solid lies at that depth: a two-sided
        # card is two coincident triangles with mirrored UVs (Concord's street banners), so a hole on one face
        # can sit on a solid texel of the other; and an unmasked triangle can coincide with a masked one
        solid_at = np.zeros(len(D), bool)
        solid_at[nn[at_hit & (~a | edge)]] = True
        hh = at_hit & a & ~edge
        cand = np.nonzero(hh & ~solid_at[nn])[0]
        unm = unmasked_at(tris, rec['tri'], P, D[nn[cand]], hitd[nn[cand]]) if len(cand) else np.zeros(0, bool)
        tot['coincident'] = tot.get('coincident', 0) + int((hh & solid_at[nn]).sum()) + int(unm.sum())
        cand = cand[~unm]
        tot['hole'] += len(cand); tot['holeedge'] += int((at_hit & a & edge).sum())
        tot.setdefault('holepx', []).extend(nn[cand].tolist())
        tot['passsolid'] += int((~at_hit & ~a & ~edge).sum()); tot['passedge'] += int((~at_hit & ~a & edge).sum())
        for j in np.nonzero(~at_hit & ~a & ~edge)[0][:8]:
            tot.setdefault('passlist', []).append('px %d tri %d b %.5f,%.5f t %.3f hit %.3f cos %.4f' % (
                nn[j], rec['tri'][kk[j]], b1[j], b2[j], tt[j], hitd[nn[j]], cosi[j]))
        sel = np.nonzero(a & ~edge)[0]
        order = sel[np.argsort(-tt[sel])]          # last write wins = the nearest hole crossing per pixel
        first_t[nn[order]] = tt[order]; first_k[nn[order]] = kk[order]
        # rays that passed a hole: where did they end?
        for n in np.nonzero(first_k >= 0)[0]:
            model = am['models'][modi[first_k[n]]].lower() if modi[first_k[n]] >= 0 else ''
            beyond = hitd[n] - first_t[n]
            if front and front in model:
                fr['n'] += 1
                if 0 < beyond <= gap:
                    fr['ok'] += 1
                    if shapes is not None and behind:
                        # the twin's own trace of the end triangle among the soup shapes' rows
                        pt = P + D[n] * hitd[n]
                        lo = tris.reshape(-1, 3, 3)
                        fr.setdefault('pts', []).append(pt)
            if corner and corner in model and cmap in am['names'][mapi[first_k[n]]].lower():
                co['n'] += 1
                co['ok'] += int(beyond > reach)
                co.setdefault('beyond', []).append(beyond)
                co.setdefault('pts', []).append(P + D[n] * hitd[n] if np.isfinite(hitd[n]) else None)
    say('crossings of masked triangles %d: HOLEHIT %d (+%d at a hole/solid edge, +%d with a solid face at the same'
        ' depth), PASSSOLID %d (+%d at an edge)' % (tot['crossed'], tot['hole'], tot['holeedge'], tot.get('coincident', 0),
                                                   tot['passsolid'], tot['passedge']))
    for l in tot.get('passlist', []):
        say('  PASSSOLID ' + l)
    if tot.get('holepx'):
        say('  HOLEHIT pixels (first 12): %s' % tot['holepx'][:12])
    expect = opt.get('--expect', 'green')
    if front:
        beh_ok = None
        if shapes is not None and behind and fr.get('pts'):
            beh_ok = end_shapes(tris, shapes, np.array(fr['pts']), behind)
        say('FRONT %s: %d rays through its holes, %d end within %.0f units behind%s' % (
            front, fr['n'], fr['ok'], gap, '' if beh_ok is None else ', on a "%s" shape %d' % (behind, beh_ok)))
    if corner and co.get('beyond'):
        bb = np.array(co['beyond'])
        say('  CORNER distance past the glass: sky %d, percentiles 10/50/90 %s' % (
            int(np.isinf(bb).sum()), np.percentile(bb[np.isfinite(bb)], [10, 50, 90]).round(1).tolist()
            if np.isfinite(bb).any() else '-'))
        if shapes is not None:
            names = name_ends(tris, shapes, [q for q in co['pts'] if q is not None])
            say('  CORNER rays end on: %s' % ', '.join('%s x%d' % kv for kv in names.most_common(6)))
    if corner:
        say('CORNER %s %s: %d rays through its holes, %d travel past %.0f units' % (corner, cmap, co['n'], co['ok'], reach))
    if expect == 'green':
        if tot['hole']:
            fails.append('HOLEHIT %d (must be 0)' % tot['hole'])
        if tot['passsolid']:
            fails.append('PASSSOLID %d (must be 0)' % tot['passsolid'])
        if front and (fr['n'] < 20 or fr['ok'] < 0.95 * fr['n']):
            fails.append('FRONT %d of %d' % (fr['ok'], fr['n']))
        if front and behind and shapes is not None and fr.get('pts') is not None and beh_ok < 0.95 * fr['ok']:
            fails.append('FRONT ends on "%s" %d of %d' % (behind, beh_ok, fr['ok']))
        # 0.75: a window frame and the piece's own trim sit within 100 units behind some holes (measured on
        # Concord -16,17: 12.5% of the rays end on Bld02CornerBrickACom01RR's own trim or the rubble at its foot)
        if corner and (co['n'] < 20 or co['ok'] < 0.75 * co['n']):
            fails.append('CORNER %d of %d' % (co['ok'], co['n']))
    else:   # the red must show the defect
        if tot['hole'] < 50:
            fails.append('red HOLEHIT %d (a mask-off bake must hit holes)' % tot['hole'])
    ok = not fails
    say('VERDICT %s%s' % ('PASS' if ok else 'FAIL', '' if ok else ': ' + '; '.join(fails)))
    return '\n'.join(lines), ok


def name_ends(tris, shapes, pts):
    """Which soup shape (model | material) each end point lies on."""
    from collections import Counter
    from scipy.spatial import cKDTree
    A = tris.reshape(-1, 3, 3).astype(np.float64)
    ctr = A.mean(1); rad = np.linalg.norm(A - ctr[:, None], axis=2).max(1)
    kd = cKDTree(ctr); rmax = min(rad.max(), 2000.0)
    out = Counter()
    for p in pts:
        c = np.array(kd.query_ball_point(p, rmax + 1.0), dtype=np.int64)
        c = c[np.linalg.norm(ctr[c] - p, axis=1) <= rad[c] + 0.5] if len(c) else c
        best, bd = None, 0.3
        for i in c:
            a, b, cc = A[i]
            nrm = np.cross(b - a, cc - a); ln = np.linalg.norm(nrm)
            if ln == 0:
                continue
            d = abs((p - a) @ nrm) / ln
            if d < bd:
                best, bd = i, d
        r = shape_of(shapes, best) if best is not None else None
        out[(os.path.basename(r[2].replace('\\', '/')) + ' | ' + os.path.basename(r[3].replace('\\', '/'))) if r else '?'] += 1
    return out


def unmasked_at(tris, masked_tris, P, D, hd):
    """For each ray (P, D[i]): does an UNMASKED soup triangle cross it within EPS of hd[i]?"""
    from scipy.spatial import cKDTree
    A = tris.reshape(-1, 3, 3)
    keep = np.ones(len(A), bool); keep[np.asarray(masked_tris, np.int64)] = False
    ids = np.nonzero(keep)[0]
    B = A[ids].astype(np.float64)
    ctr = B.mean(1); rad = np.linalg.norm(B - ctr[:, None], axis=2).max(1)
    big = rad > 400            # a few huge triangles (the ground): tested always, not through the tree
    kd = cKDTree(ctr[~big]); small = np.nonzero(~big)[0]; bigi = np.nonzero(big)[0]
    out = np.zeros(len(D), bool)
    for i in range(len(D)):
        hp = P + D[i] * hd[i]
        c = small[np.array(kd.query_ball_point(hp, 401.0), dtype=np.int64)]
        c = np.concatenate([c[np.linalg.norm(ctr[c] - hp, axis=1) <= rad[c] + 0.1], bigi])
        v0 = B[c, 0]; e1 = B[c, 1] - v0; e2 = B[c, 2] - v0
        pv = np.cross(D[i], e2); det = (e1 * pv).sum(1)
        ok = np.abs(det) > 1e-12; idt = np.where(ok, 1 / np.where(ok, det, 1), 0)
        tv = P - v0; b1 = (tv * pv).sum(1) * idt; qv = np.cross(tv, e1); b2 = (qv @ D[i]) * idt
        t = (e2 * qv).sum(1) * idt
        out[i] = bool((ok & (b1 >= 0) & (b2 >= 0) & (b1 + b2 <= 1) & (np.abs(t - hd[i]) <= EPS)).any())
    return out


def end_shapes(tris, shapes, pts, behind):
    """How many end points lie on a triangle of a shape whose model/material/map matches `behind`."""
    st, rows = shapes
    sel = [r for r in rows if any(behind in c.lower() for c in r[2:])]
    if not sel:
        return 0
    k = np.concatenate([np.arange(int(r[0]), int(r[0]) + int(r[1])) for r in sel])
    T = tris[k].astype(np.float64).reshape(-1, 3, 3)
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    nl = np.linalg.norm(n, axis=1); good = nl > 0
    T, n = T[good], n[good] / nl[good, None]
    hits = 0
    for p in pts:
        d = np.abs(((p - T[:, 0]) * n).sum(1))
        c = d < 0.25
        if not c.any():
            continue
        # inside test for the close planes
        for i in np.nonzero(c)[0]:
            a, b, cc = T[i]
            v0, v1, v2 = b - a, cc - a, p - a
            d00, d01, d11 = v0 @ v0, v0 @ v1, v1 @ v1
            d20, d21 = v2 @ v0, v2 @ v1
            den = d00 * d11 - d01 * d01
            if den == 0:
                continue
            vv = (d11 * d20 - d01 * d21) / den; ww = (d00 * d21 - d01 * d20) / den
            if vv >= -1e-3 and ww >= -1e-3 and vv + ww <= 1 + 1e-3:
                hits += 1
                break
    return hits


if __name__ == '__main__':
    text, ok = main(sys.argv[1:])
    print(text)
    sys.exit(0 if ok else 1)
