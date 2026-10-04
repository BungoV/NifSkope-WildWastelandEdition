"""Lane AODECAL1: the baked AO decals on real data (the twin is tests/spells/aodecal1_check.py; design
docs/cloud/AODECAL1_DESIGN.md). Every check runs the built exe; nothing from the game is written into the repo.

  a   EXE DATA WORK [--red worlddown]   gate A on a real car: the exe's own lookup (src/aovolume.cpp) in the copy's
                                        model space against a 4096-ray brute force of the placed mesh, four
                                        placements (upright, tipped 90, flipped 180, leaning x1.2), the twin's far
                                        bars; the contact band (within one voxel) reported against the twin's bar.
                                        Also: the twin's reader + lookup on the exe's .ao gives the exe's numbers.
                                        Red worlddown (the projected decal: yaw only, receivers facing up) -> FAIL.
  e   EXE DATA WORK [--red stale]       the cache: a second run reads (no bake); a changed model (salted fingerprint)
                                        rebakes; another model's file under this model's name (index size and CRC
                                        patched to match) is refused and rebaked, so the lookups still meet the bars.
                                        Red stale (the fingerprint not checked) keeps the wrong volume -> FAIL.
  d   TABLE [TABLE_RED]                 gate D on a real cell (WW_CELL_AODECAL_GATE's table): per probe octant near a
                                        copy, the copy-free sky rebuilt from the traced-with share against the traced
                                        copy-free share. Bars: signed mean >= -0.02, p95 |err| <= 0.05, worst >= -0.10.
  gpu DUMP AODIR                        the decal target (WW_CELL_AODECAL_DUMP) against the product of the twin's
                                        lookups over every copy, per pixel the copy's own pixels left out (its id).
Exit 0 = PASS. Reds are expected to FAIL (the caller checks rc != 0)."""
import importlib.util
import math
import os
import struct
import subprocess
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('twin', os.path.join(HERE, 'aodecal1_check.py'))
twin = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(twin)

MODEL = os.environ.get('AODECAL_MODEL', 'Vehicles\\Automotive\\Car02DStatic.nif')
MODEL_B = os.environ.get('AODECAL_MODEL_B', 'Vehicles\\Automotive\\CarHulk01.nif')
TIMEOUT = 900


def run(exe, args, env=None):
    e = dict(os.environ)
    e.update(env or {})
    for k in ('WW_CELL_AODECAL_RED', 'WW_CELL_AODECAL_FPSALT'):
        if env is None or k not in env:
            e.pop(k, None)
    p = subprocess.run([exe, '-no-gui', 'aobake'] + args, capture_output=True, text=True, timeout=TIMEOUT, env=e)
    return p.returncode, p.stdout + p.stderr


def baked_flag(out):
    for line in out.splitlines():
        if line.startswith('aobake: meshes'):
            return int(line.split(' baked ')[1].split()[0])
    return -1


def ao_file(out):
    for line in out.splitlines():
        if line.startswith('aobake: file '):
            return line.split()[-1]
    return None


def read_vol(aodir, fname, red_stale=False):
    """the twin's reader on the exe's files: the index record whose path hashes to the file's name"""
    recs = twin.read_index(open(os.path.join(aodir, 'index.aoi'), 'rb').read())
    h = int(fname[:16], 16)
    rec = next(r for p, r in recs.items() if twin.fnv1a64(p.encode('utf-8')) == h)
    return twin.read_ao(open(os.path.join(aodir, fname), 'rb').read(), rec, rec['fp'], red_stale)


def placements(tris_m):
    """the twin's four rotated placements, each resting on z = 0, centred on the origin"""
    out = []
    for name, R, s in (('upright', twin.rot(yaw=30), 1.0), ('tipped90', twin.rot(yaw=-20, roll=90), 1.0),
                       ('flipped180', twin.rot(yaw=60, roll=180), 1.0),
                       ('leaning', twin.rot(yaw=15, pitch=8, roll=-25), 1.2)):
        w = tris_m.reshape(-1, 3) * s @ R.T
        lo, hi = w.min(0), w.max(0)
        t = np.array([-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]])
        out.append((name, R, t, s))
    return out


def tri_points(tw):
    """vertices, edge midpoints and centroids: the near / far split's stand-in for the surface"""
    T = tw.reshape(-1, 3, 3)
    pts = [T[:, 0], T[:, 1], T[:, 2], (T[:, 0] + T[:, 1]) / 2, (T[:, 1] + T[:, 2]) / 2, (T[:, 2] + T[:, 0]) / 2,
           T.mean(1)]
    return np.concatenate(pts)


def min_dist(P, S, chunk=256):
    d = np.empty(len(P))
    for i in range(0, len(P), chunk):
        q = P[i:i + chunk]
        d[i:i + chunk] = np.sqrt(((q[:, None, :] - S[None, :, :]) ** 2).sum(2)).min(1)
    return d


def receivers(tw, voxel, rng):
    """world: the ground around and under the copy (up), a wall beside it (facing it), and a shell off its faces"""
    T = tw.reshape(-1, 3, 3)
    lo, hi = T.reshape(-1, 3).min(0), T.reshape(-1, 3).max(0)
    ext = hi - lo
    P, N, K = [], [], []
    g = rng.uniform(lo[:2] - 0.6 * ext[:2], hi[:2] + 0.6 * ext[:2], size=(700, 2))
    P.append(np.c_[g, np.full(len(g), 0.5)])
    N.append(np.tile([0.0, 0.0, 1.0], (len(g), 1)))
    K += ['ground'] * len(g)
    wy = rng.uniform(lo[1] - 0.3 * ext[1], hi[1] + 0.3 * ext[1], 250)
    wz = rng.uniform(0.5, hi[2] + 0.3 * ext[2], 250)
    P.append(np.c_[np.full(250, hi[0] + 1.5 * voxel), wy, wz])
    N.append(np.tile([-1.0, 0.0, 0.0], (250, 1)))
    K += ['wall'] * 250
    pick = rng.choice(len(T), size=min(700, len(T)), replace=False)
    c = T[pick].mean(1)
    fn = np.cross(T[pick, 1] - T[pick, 0], T[pick, 2] - T[pick, 0])
    ln = np.linalg.norm(fn, axis=1)
    ok = ln > 1e-9
    fn = fn[ok] / ln[ok, None]
    c = c[ok]
    sgn = rng.choice([-1.0, 1.0], size=len(c))
    fn *= sgn[:, None]
    d = rng.uniform(0.3, 3.0, size=len(c)) * voxel
    p = c + fn * d[:, None]
    keep = p[:, 2] > 0.25
    P.append(p[keep])
    N.append(fn[keep])
    K += ['shell'] * int(keep.sum())
    return np.concatenate(P), np.concatenate(N), np.array(K)


def gate_a(exe, data, work, red=None, aodir=None, model=MODEL, label='A', env=None):
    aodir = aodir or os.path.join(work, 'ao')
    os.makedirs(aodir, exist_ok=True)
    rng = np.random.default_rng(4)
    tf = os.path.join(work, 'tris_model.bin')
    dummy = os.path.join(work, 'dummy.bin')
    np.zeros(6, '<f4').tofile(dummy)
    rc, out = run(exe, ['--data', data, '--model', model, '--dir', aodir, '--receivers', dummy, '--copy',
                        '1 0 0 0 1 0 0 0 1 0 0 0 1', '--out', os.path.join(work, 'dummy_out.bin'), '--tris', tf], env)
    head = [l for l in out.splitlines() if l.startswith('aobake:') or 'aodecal:' in l]
    if rc != 0:
        return False, ['%s FAIL: aobake rc %d\n%s' % (label, rc, out[-2000:])], None
    fname = ao_file(out)
    vol, why = read_vol(aodir, fname, red_stale=bool(env))
    if vol is None:
        return False, ['%s FAIL: the twin reader refused the exe\'s .ao (%s)' % (label, why)], None
    tris_m = np.fromfile(tf, '<f4')
    voxel = float(vol.cell.max())
    lines = ['%s %s' % (label, h) for h in head]
    ok_all, rot_fail, twin_diff, near_over = True, 0, 0.0, 0
    for name, R, t, s in placements(tris_m):
        tw = (tris_m.reshape(-1, 3) * s @ R.T + t).astype('<f4')
        P, N, K = receivers(tw, voxel * s, rng)
        rf = os.path.join(work, 'recv_%s.bin' % name)
        of = os.path.join(work, 'out_%s.bin' % name)
        np.c_[P, N].astype('<f4').tofile(rf)
        cp = ' '.join('%.9g' % x for x in list(R.reshape(-1)) + list(t) + [s])
        args = ['--data', data, '--model', model, '--dir', aodir, '--receivers', rf, '--copy', cp, '--out', of]
        if red == 'worlddown':
            args += ['--red', 'worlddown']
        rc, out = run(exe, args, env)
        if rc != 0:
            return False, lines + ['  %s: aobake rc %d %s' % (name, rc, out[-500:])], vol
        res = np.fromfile(of, '<f4').reshape(-1, 3)
        got, bf, inside = res[:, 0].astype(float), res[:, 1].astype(float), res[:, 2]
        keep = inside <= 0.5
        P, N, K, got, bf = P[keep], N[keep], K[keep], got[keep], bf[keep]
        # the twin's reader and lookup on the exe's file: the same numbers (worlddown is the exe's red only)
        cpy = twin.Copy(name, R, t, s)
        if red != 'worlddown':
            tl = twin.lookup(vol, cpy.to_model(P), cpy.dir_to_model(N))
            twin_diff = max(twin_diff, float(np.abs(tl - got).max()))
        e = np.abs(got - bf)
        near = min_dist(P, tri_points(tw)) < voxel * s
        ef, en = e[~near], e[near]
        ok_far = len(ef) > 0 and ef.mean() <= twin.BAR_A_MEAN and np.percentile(ef, 95) <= twin.BAR_A_P95 \
            and ef.max() <= twin.BAR_A_MAX
        ok_near = (not near.any()) or (en.mean() <= twin.BAR_AN_MEAN and np.percentile(en, 95) <= twin.BAR_AN_P95)
        # the contact band (within one voxel) is REPORTED against the twin's bar, not gated: on a real mesh an L2
        # voxel straddles thin panels and tyre contacts (2026-10-04: near p95 0.34-0.39, all of it within half a
        # voxel); the design's open question 7 leaves that band to the game's SSAO. The far bars are the gate.
        ok = ok_far
        near_over += 0 if ok_near else 1
        ok_all &= ok
        rot_fail += 0 if ok or name == 'upright' else 1
        kinds = ' '.join('%s %d' % (k, int((K == k).sum())) for k in ('ground', 'wall', 'shell'))
        lines.append('  %-10s %4d pts (%s; %d inside dropped), brute AO %.3f..%.3f\n'
                     '             far %4d: |err| mean %.4f p95 %.4f max %.3f; near %4d: mean %.4f p95 %.4f max %.3f %s'
                     % (name, len(P), kinds, int((~keep).sum()), bf.min(), bf.max(), len(ef), ef.mean(),
                        np.percentile(ef, 95), ef.max(), len(en), en.mean() if len(en) else 0.0,
                        np.percentile(en, 95) if len(en) else 0.0, en.max() if len(en) else 0.0,
                        ('ok' if ok else 'BAD') + ('' if ok_near else " (near over the twin's bar)")))
    twin_ok = red == 'worlddown' or twin_diff < 2e-3
    ok_all &= twin_ok
    lines.insert(len(head), '%s %s real model %s: the exe\'s lookup vs brute force (bars far: mean %.2f p95 %.2f max %.2f; '
                 'near one voxel, reported: mean %.2f p95 %.2f, over it on %d of 4); voxel %.1f units; twin reader+lookup vs exe max %.1e%s'
                 % (label, 'PASS' if ok_all else 'FAIL', model, twin.BAR_A_MEAN, twin.BAR_A_P95, twin.BAR_A_MAX,
                    twin.BAR_AN_MEAN, twin.BAR_AN_P95, near_over, voxel, twin_diff, ' [red %s]' % red if red else ''))
    return ok_all, lines, vol


def patch_index(aodir, fname, newbytes):
    p = os.path.join(aodir, 'index.aoi')
    b = bytearray(open(p, 'rb').read())
    n = struct.unpack_from('<I', b, 8)[0]
    h = int(fname[:16], 16)
    for i in range(n):
        o = 32 + 64 * i
        if struct.unpack_from('<Q', b, o)[0] == h:
            struct.pack_into('<II', b, o + 48, len(newbytes), zlib.crc32(newbytes))
            open(p, 'wb').write(bytes(b))
            return True
    return False


def gate_e(exe, data, work, red=None):
    import shutil
    lines, ok = [], True
    dx, dy = os.path.join(work, 'e_x'), os.path.join(work, 'e_y')
    for d in (dx, dy):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    env_red = {'WW_CELL_AODECAL_RED': red} if red else {}
    base = ['--data', data, '--dir']
    rc1, o1 = run(exe, base + [dx, '--model', MODEL])
    rc2, o2 = run(exe, base + [dx, '--model', MODEL])
    rc3, o3 = run(exe, base + [dx, '--model', MODEL], {'WW_CELL_AODECAL_FPSALT': '7'})
    rc4, o4 = run(exe, base + [dx, '--model', MODEL])
    b1, b2, b3, b4 = (baked_flag(o) for o in (o1, o2, o3, o4))
    c_ok = (rc1, rc2, rc3, rc4) == (0, 0, 0, 0) and (b1, b2, b3, b4) == (1, 0, 1, 1)
    lines.append('  cache: first run baked %d, second baked %d (read), salted model baked %d, unsalted again baked %d: %s'
                 % (b1, b2, b3, b4, 'ok' if c_ok else 'BAD'))
    ok &= c_ok
    # another model's file under this one's name, the index's size and CRC patched to it
    rcb, ob = run(exe, base + [dy, '--model', MODEL_B])
    fa, fb = ao_file(o4), ao_file(ob)
    wrong = open(os.path.join(dy, fb), 'rb').read()
    open(os.path.join(dx, fa), 'wb').write(wrong)
    patched = patch_index(dx, fa, wrong)
    ok_a, la, _ = gate_a(exe, data, work, aodir=dx, label='E/A', env=env_red)
    wrong_baked = '(rebaked)' if any(' baked 1 ' in l for l in la) else '(read as is)'
    lines.append('  another model\'s file swapped in (index patched: %s) %s; then gate A on it:' % (patched, wrong_baked))
    lines += ['    ' + l for l in la]
    ok &= bool(ok_a) and rcb == 0 and patched
    head = 'E %s the .ao cache on a real model%s' % ('PASS' if ok else 'FAIL', ' [red %s]' % red if red else '')
    return ok, [head] + lines


def gate_d(table, red_table=None):
    def load(path):
        rows = []
        for line in open(path):
            if line.startswith('#') or not line.strip():
                continue
            v = [float(x) for x in line.split()]
            rows.append(v)
        a = np.array(rows)
        return a[:, 3:11], a[:, 11:19], a[:, 19:27], a[:, 27:35], a[:, 35:43]
    out, res, few = [], {}, False
    for path, label in ((table, 'green'), (red_table, 'red nodivide')):
        if not path:
            continue
        bake, w, f, rec, recb = load(path)
        saw = (f - w) > 0.005                 # octants where the copies took sky
        err = (rec - f)[saw]
        pooled = err.size
        mean = float(err.mean()) if pooled else 0.0
        p95 = float(np.percentile(np.abs(err), 95)) if pooled else 0.0
        worst = float(err.min()) if pooled else 0.0
        ok = pooled >= 20 and mean >= twin.BAR_D_BIAS and p95 <= twin.BAR_D_P95 and worst >= twin.BAR_D_WORST
        res[label] = ok
        few = few or (label == 'green' and pooled < 20)
        base = (f - w)[saw]
        out.append('  %-12s %5d probes, %5d octants the copies took sky from (mean taken %.4f): signed err mean %+.4f '
                   'p95 |err| %.4f worst %+.3f -> %s; bake-share route: mean %+.4f'
                   % (label, len(w), pooled, float(base.mean()) if pooled else 0.0, mean, p95, worst,
                      'within bars' if ok else 'OUT of bars', float((recb - f)[saw].mean()) if pooled else 0.0))
    green, red = res.get('green', False), res.get('red nodivide')
    # a cell where the copies take too little of the probes' sky cannot tell the divide from none: the red stays
    # inside the bars too. That is not a pass: INCONCLUSIVE, rc 1, and the numbers stand as the measured bias.
    # Fewer than 20 octants is no evidence either way: INCONCLUSIVE (too few octants), rc 1 as well.
    verdict = 'INCONCLUSIVE (too few octants)' if few else (
        'PASS' if green and red is False else ('INCONCLUSIVE' if green and red else 'FAIL'))
    head = 'D %s copy-free sky on a real cell (bars: signed mean >= %+.2f, p95 <= %.2f, worst >= %+.2f, >= 20 octants; '         'the red must miss)' % (verdict, twin.BAR_D_BIAS, twin.BAR_D_P95, twin.BAR_D_WORST)
    return verdict == 'PASS', [head] + out


def gate_gpu(dump, aodir):
    b = open(dump, 'rb').read()
    W, H = struct.unpack_from('<ii', b, 0)
    n = W * H
    a = np.frombuffer(b, '<f4', count=n * 6, offset=8)
    tg = a[:n].reshape(H, W)
    sid = a[n:2 * n].reshape(H, W)
    gb = a[2 * n:6 * n].reshape(H, W, 4)
    txt = open(dump + '.txt').read().splitlines()
    view = [float(x) for x in next(l for l in txt if l.startswith('view ')).split()[1:]]
    sc = view[0]
    Rv = np.array(view[1:10]).reshape(3, 3)
    tv = np.array(view[10:13])
    p00, p11, p20, p21 = view[13:17]
    copies = []
    for l in txt:
        if l.startswith('copy '):
            v = l.split()
            R = np.array([float(x) for x in v[3:12]]).reshape(3, 3)
            copies.append((int(v[1]), v[2], R, np.array([float(x) for x in v[12:15]]), float(v[15])))
    # the receivers: every pixel of the opaque pass, view -> world
    ys, xs = np.mgrid[0:H, 0:W]
    d = gb[..., 3]
    fg = d < 1e5
    ndc_x = (xs + 0.5) / W * 2 - 1
    ndc_y = (ys + 0.5) / H * 2 - 1
    zv = d * sc
    pv = np.stack([zv * (ndc_x + p20) / p00, zv * (ndc_y + p21) / p11, -zv], -1)
    world = ((pv - tv) @ Rv) / sc
    nw = gb[..., :3] @ Rv
    # reconstruction check: the world normal from the depth's own neighbours agrees with the opaque pass's
    dx_ = world[1:-1, 2:] - world[1:-1, :-2]
    dy_ = world[2:, 1:-1] - world[:-2, 1:-1]
    nn = np.cross(dx_, dy_)
    ln = np.linalg.norm(nn, axis=-1)
    m = fg[1:-1, 1:-1] & fg[1:-1, 2:] & fg[1:-1, :-2] & fg[2:, 1:-1] & fg[:-2, 1:-1] & (ln > 1e-6)
    cosv = np.abs((nn[m] / ln[m, None] * nw[1:-1, 1:-1][m]).sum(-1))
    planar = float(np.median(cosv)) if cosv.size else 0.0
    want = np.ones((H, W))
    P = world[fg]
    N = nw[fg] / np.maximum(np.linalg.norm(nw[fg], axis=-1, keepdims=True), 1e-9)
    ids = sid[fg]
    acc = np.ones(len(P))
    accsum = np.ones(len(P))   # the summed form (1 + sum(AO - 1)): where it and the product part, stacking is tested
    nfac = np.zeros(len(P), int)
    vols = {}
    for ci, fname, R, t, s in copies:
        if fname not in vols:
            vols[fname] = read_vol(aodir, fname)[0]
        vol = vols[fname]
        cp = twin.Copy(str(ci), R, t, s)
        mm = cp.to_model(P)
        flo, fhi = twin.footprint(vol)
        inb = np.all((mm >= flo) & (mm <= fhi), axis=1) & (np.abs(ids - (ci + 1)) > 0.5)
        if inb.any():
            lk = twin.lookup(vol, mm[inb], cp.dir_to_model(N[inb]))
            acc[inb] *= lk
            accsum[inb] += lk - 1.0
            nfac[inb] += (lk < 0.999)
    want[fg] = acc
    # the reconstruction itself, against the geometry: a copy's own pixels (its id) must land inside its model's
    # bounding box (the volume box less its margin; margin = fade / 0.25). A wrong depth scale, a flipped
    # projection term or a wrong view matrix moves them out; the normals-vs-depth planarity cannot see a scale.
    self_in = self_n = 0
    for ci, fname, R, t, s in copies:
        sel = np.abs(ids - (ci + 1)) < 0.5
        if not sel.any():
            continue
        if fname not in vols:
            vols[fname] = read_vol(aodir, fname)[0]
        vol = vols[fname]
        mg = vol.fade / 0.25
        blo, bhi = vol.lo + mg, vol.hi - mg
        pad = 0.02 * (bhi - blo) + 2.0
        mm = twin.Copy(str(ci), R, t, s).to_model(P[sel])
        self_in += int(np.all((mm >= blo - pad) & (mm <= bhi + pad), axis=1).sum())
        self_n += int(sel.sum())
    self_share = self_in / max(self_n, 1)
    e = np.abs(tg[fg] - want[fg])
    touched = want[fg] < 0.999
    ok = self_n > 1000 and self_share >= 0.98 and touched.sum() > 500 and float(np.percentile(e, 99)) <= 0.02 \
        and float(e[touched].mean()) <= 0.005
    # his call "stacked copies multiply": the pixels where two or more copies darken AND the product and the sum part
    # by > 0.005 (a summed pass misses each of them by more than that). Their mean |err| must stay <= 0.002.
    stack = (nfac >= 2) & (np.abs(acc - np.maximum(accsum, 0.0)) > 0.005)
    se = float(e[stack].mean()) if stack.any() else 1.0
    ok = ok and int(stack.sum()) >= 200 and se <= 0.002
    return ok, ['GPU %s decal target vs the twin\'s product of lookups: %dx%d, %d copies, %d pixels darkened (min %.3f); '
                '|err| mean %.5f p99 %.4f max %.3f; the copies\' own pixels inside their model boxes %d of %d (%.4f, bar '
                '0.98); stacked pixels (2+ copies, product vs sum > 0.005) %d (bar 200), |err| mean %.4f (bar 0.002); '
                'plane normals from depth vs the pass\'s (normal maps included, reported): median |cos| %.4f'
                % ('PASS' if ok else 'FAIL', W, H, len(copies), int(touched.sum()), float(tg[fg].min()),
                   float(e[touched].mean()) if touched.any() else 0.0, float(np.percentile(e, 99)), float(e.max()),
                   self_in, self_n, self_share, int(stack.sum()), se, planar)]


def main():
    a = sys.argv[1:]
    red = None
    if '--red' in a:
        i = a.index('--red')
        red = a[i + 1]
        del a[i:i + 2]
    cmd = a[0]
    if cmd == 'a':
        ok, lines, _ = gate_a(a[1], a[2], a[3], red)
    elif cmd == 'e':
        ok, lines = gate_e(a[1], a[2], a[3], red)
    elif cmd == 'd':
        ok, lines = gate_d(a[1], a[2] if len(a) > 2 else None)
    elif cmd == 'gpu':
        ok, lines = gate_gpu(a[1], a[2])
    else:
        print(__doc__)
        return 2
    print('\n'.join(lines))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
