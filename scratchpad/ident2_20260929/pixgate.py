"""IDENT2 follow-up: THE PIXEL GATE, offline from the FILE's bytes.
usage: python pixgate.py <lod base (no extension)> <dump> [landmarks.txt] [--px 16] [--png prefix]
For every landmark: the outline = convex hull (X/Y) of the name-matched pieces' box corners (the emitter's).
Every non-tree file instance in the chunks the outline touches is placed (level-0 triangles of its drawn mesh);
a triangle is kept when its centroid lies inside the outline. The kept triangles are point-splatted into a
z-buffer (orthographic) from 5 views: straight down, and 35 degrees down from azimuth 45/135/225/315. Every
covered pixel carries the nearest instance's FILE group id -- what the identity view colours. The decoder
hands every version's ids over FILE-WIDE (v7..v12 per-chunk ids offset chunk by chunk; v13 writes them so).
A pixel is WRONG when its id is not the landmark's ONE id: the id most of its named pieces carry, over the
whole file (IDENT2 third job: a landmark cut by a chunk line in two ids FAILS, the violet strip). Prints per landmark and view
the covered and wrong pixel counts, then each instance owning a wrong pixel, by name. Exit 1 when any wrong.
Trees and plants are never drawn (tree flag in the dump, or a vegetation model path)."""
import sys, os, math, collections
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
import lodgen_native_decode as ND
import lodi_occluder_building as OB
import groups, fp_hull as FH

argv = list(sys.argv[1:])
opt = {}
for key in ('--px', '--png'):
    if key in argv:
        k = argv.index(key); opt[key] = argv[k + 1]; del argv[k:k + 2]
base, dump = argv[0], argv[1]
lmf = argv[2] if len(argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'res', 'lodgen_landmarks.txt')
PX = float(opt.get('--px', 16))

h, P, C = groups.load(dump)
rules = FH.load_rules(lmf)
L = ND.read_lodo(base + '.lodo')
T = ND.read_lodi(base + '.lodi')
inst, cold, grp, chunks = T['instances'], T['cold'], T['group'], T['chunks']
H0 = T['header'] if 'header' in T else T['h']
w = H0['chunkEast'] - H0['chunkWest'] + 1
chunk_of = [None] * len(cold)
for ci, ch in enumerate(chunks):
    cx, cy = H0['chunkWest'] + ci % w, H0['chunkNorth'] - ci // w
    for ii in range(ch['instanceFirst'], ch['instanceFirst'] + ch['instanceCount']):
        chunk_of[ii] = (cx, cy)
pkey = {(p['ref'], p['part']): p for p in P}
key_to_ii = collections.defaultdict(list)
for ii, c in enumerate(cold):
    key_to_ii[(c['refFormId'], c['scolPart'])].append(ii)


def name_of(ii):
    p = pkey.get((cold[ii]['refFormId'], cold[ii]['scolPart']))
    return p['name'] if p else '0x%x part %d (not in dump)' % (cold[ii]['refFormId'], cold[ii]['scolPart'])


def is_tree(ii):
    p = pkey.get((cold[ii]['refFormId'], cold[ii]['scolPart']))
    if p is None:
        return False
    return p['tree'] or FH.model(p['name']).startswith(FH.VEG)


VIEWS = [('top', 0.0, 90.0)] + [('az%d' % a, float(a), 35.0) for a in (45, 135, 225, 315)]
bad_total = 0
for rule in rules:
    name, pre, cen = rule
    mem = [p for p in P if p['tris'] and not p['tree'] and FH.model(p['name']).startswith(tuple(pre))
           and (cen is None or math.hypot(p['x'] - cen[0], p['y'] - cen[1]) <= cen[2])]
    if not mem:
        print('%s: no named pieces in this bake' % name)
        continue
    Hl = FH.hull([q for p in mem for q in FH.corners(p)])
    E = np.array(Hl + [Hl[0]], dtype=np.float64)
    ea, eb = E[:-1], E[1:]
    ex, ey = eb[:, 0] - ea[:, 0], eb[:, 1] - ea[:, 1]
    el = np.sqrt(ex * ex + ey * ey)

    def inside(xy):
        d = ((xy[:, None, 0] - ea[None, :, 0]) * ey[None, :] - (xy[:, None, 1] - ea[None, :, 1]) * ex[None, :]) / el[None, :]
        return d.max(1) < 0.0
    # the landmark's own id per chunk: the ids its named pieces carry in the file
    own = collections.defaultdict(collections.Counter)
    for p in mem:
        for ii in key_to_ii.get((p['ref'], p['part']), []):
            own[chunk_of[ii]][grp[ii]] += 1
    whole = collections.Counter()
    for c in own.values():
        whole.update(c)
    one = whole.most_common(1)[0][0]
    own_id = {ch: one for ch in own}
    split = dict(whole) if len(whole) > 1 else {}
    hx0, hy0 = min(q[0] for q in Hl), min(q[1] for q in Hl)
    hx1, hy1 = max(q[0] for q in Hl), max(q[1] for q in Hl)
    tris, owner = [], []
    for ii in range(len(cold)):
        ch = chunk_of[ii]
        if ch is None or is_tree(ii):
            continue
        p = pkey.get((cold[ii]['refFormId'], cold[ii]['scolPart']))
        if p is not None:
            if p['hi'][0] < hx0 or p['lo'][0] > hx1 or p['hi'][1] < hy0 or p['lo'][1] > hy1:
                continue
        else:
            x, y = inst[ii]['x'], inst[ii]['y']
            if x < hx0 - 8192 or x > hx1 + 8192 or y < hy0 - 8192 or y > hy1 + 8192:
                continue
        seen = (p['tris'], np.array(p['lo']), np.array(p['hi'])) if p else None
        me = OB.drawn_mesh(L, inst[ii], seen)
        if me == OB.NO_MESH or me >= len(L['meshes']):
            continue
        t = OB.placed_tris(L, inst[ii], me)
        if not len(t):
            continue
        t = t[inside(t.mean(1)[:, :2])]
        if len(t):
            tris.append(t); owner.append(np.full(len(t), ii, dtype=np.int64))
    if not tris:
        print('%s: no triangles inside the outline' % name)
        continue
    tris = np.concatenate(tris); owner = np.concatenate(owner)
    ok_inst = np.array([grp[ii] == one for ii in range(len(cold))])
    print('\n%s: outline %d points, %d named pieces, %d triangles inside from %d instances; own id per chunk %s%s' % (
        name, len(Hl), len(mem), len(tris), len(set(owner.tolist())),
        ' '.join('%s:%d' % (ch, g) for ch, g in sorted(own_id.items())),
        ('; NAMED PIECES SPLIT over ids ' + str(split)) if split else ''))
    rng = np.random.default_rng(1)
    wrong_by = collections.Counter()
    lm_bad = 0
    for vname, az, el_deg in VIEWS:
        a, e = math.radians(az), math.radians(el_deg)
        d = np.array([-math.cos(e) * math.cos(a), -math.cos(e) * math.sin(a), -math.sin(e)])   # looking direction
        up = np.array([0.0, 0.0, 1.0]) if el_deg < 89.9 else np.array([0.0, 1.0, 0.0])
        u = np.cross(d, up); u /= np.linalg.norm(u)
        v = np.cross(u, d)
        A, B, Cc = tris[:, 0], tris[:, 1], tris[:, 2]
        pa = np.stack([A @ u, A @ v], 1); pb = np.stack([B @ u, B @ v], 1); pc = np.stack([Cc @ u, Cc @ v], 1)
        area = 0.5 * np.abs((pb[:, 0] - pa[:, 0]) * (pc[:, 1] - pa[:, 1]) - (pb[:, 1] - pa[:, 1]) * (pc[:, 0] - pa[:, 0])) / (PX * PX)
        n = np.clip(np.ceil(area * 3).astype(np.int64), 3, 20000)
        idx = np.repeat(np.arange(len(tris)), n)
        r1 = rng.random(len(idx)); r2 = rng.random(len(idx))
        s1 = np.sqrt(r1)
        w0, w1, w2 = 1 - s1, s1 * (1 - r2), s1 * r2
        pts = A[idx] * w0[:, None] + B[idx] * w1[:, None] + Cc[idx] * w2[:, None]
        pu, pv, pd = pts @ u, pts @ v, pts @ d
        iu = np.floor((pu - pu.min()) / PX).astype(np.int64); iv = np.floor((pv - pv.min()) / PX).astype(np.int64)
        W = iu.max() + 1
        pix = iv * W + iu
        order = np.lexsort((pd, pix))
        pix_s = pix[order]
        first = np.ones(len(order), bool); first[1:] = pix_s[1:] != pix_s[:-1]
        win = order[first]
        wo = owner[idx[win]]
        bad = ~ok_inst[wo]
        lm_bad += int(bad.sum())
        for ii, k in collections.Counter(wo[bad].tolist()).items():
            wrong_by[ii] += k
        print('  view %-5s: %7d pixels covered, %6d not the landmark\'s colour' % (vname, len(win), int(bad.sum())))
        if '--png' in opt:
            from PIL import Image
            Hh = iv.max() + 1
            img = np.full((Hh * W, 3), 255, np.uint8)
            img[pix[win]] = np.where(bad[:, None], np.array([220, 30, 30], np.uint8), np.array([90, 90, 90], np.uint8))
            Image.fromarray(img.reshape(Hh, W, 3)[::-1]).save('%s_%s_%s.png' % (opt['--png'], name, vname))
    bad_total += lm_bad
    print('  %s: %d wrong pixels over 5 views' % (name, lm_bad))
    for ii, k in wrong_by.most_common():
        print('    %6d px  chunk %s id %d (own %d)  %s' % (k, chunk_of[ii], grp[ii], one, name_of(ii)))
print('\nPIXEL GATE %s: %d wrong pixels' % ('PASS' if bad_total == 0 else 'FAIL', bad_total))
sys.exit(0 if bad_total == 0 else 1)
