#!/usr/bin/env python3
"""THE TRANSITION GATE, camera-free -- extended to PER-FRAME positioning.

bungo: "the tree must be positioned correctly, so that when a 3d tree
transitions to an imposter, the tree won't change position".

`card.center` claims to be the offset from the object's PIVOT (the NIF root) to
the card's centre.  This checks that claim against a SECOND, INDEPENDENT reader
of the same model -- NifSkope's headless `dump`, reading each shape's own
`Bounding Sphere` out of the file -- and never against the bake that produced
the number.

  centre : |card.center - the model's own merged bound centre| must be small
  radius : card.depthSpan must be 3 * max(bound radius, 1024)
  extent : the card's INNER half-extents must lie inside the model's bound
           sphere (a silhouette cannot exceed it) and must not be small
           compared with it (a frame that is mostly air)

  frames : since 2026-09-09 evening each frame carries its own offset
           (`card.frameOffset`), so its quad sits at `center + ox*right +
           oy*up`.  Whatever the view basis that centre is within |offset| of
           `center`, so the MOST DISPLACED FRAME is what the centre and extent
           tests have to survive -- both are re-run against it.

  CONTROLS, which must FAIL: `center` treated as zero, i.e. the quad at the
  pivot; and, for the per-frame half, the most displaced frame measured against
  the pivot rather than against `center`.

  transition_bounds.py <exe> <dataroot> <cardsdir> <id>:<nif> ...
"""
import sys, os, json, struct, subprocess, math


def lodm(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'LODM'
    n = struct.unpack('<I', b[8:12])[0]
    return json.loads(b[12:12 + n].decode('utf-8'))


def blocks(exe, nif):
    out = subprocess.run([exe, '-no-gui', 'list', nif], capture_output=True, text=True).stdout
    ids = []
    for ln in out.splitlines():
        ln = ln.strip()
        if not ln.startswith('['):
            continue
        i = int(ln[1:ln.index(']')])
        ty = ln[ln.index(']') + 1:].strip().split()[0]
        ids.append((i, ty))
    return ids


def bound_of(exe, nif, b):
    out = subprocess.run([exe, '-no-gui', 'dump', nif, '-b', str(b), '-d', '3'],
                         capture_output=True, text=True).stdout
    c, r = None, None
    lines = out.splitlines()
    for k, ln in enumerate(lines):
        if 'Bounding Sphere' in ln:
            for m in lines[k + 1:k + 4]:
                if 'Center' in m and '=' in m:
                    t = m.split('=')[1].split()
                    c = (float(t[1]), float(t[3]), float(t[5]))
                if 'Radius' in m and '=' in m:
                    r = float(m.split('=')[1].strip())
            break
    return c, r


def merge(a, b):
    """the smallest sphere containing two spheres"""
    if a is None:
        return b
    if b is None:
        return a
    (ac, ar), (bc, br) = a, b
    d = math.dist(ac, bc)
    if d + br <= ar:
        return a
    if d + ar <= br:
        return b
    if d < 1e-6:
        return (ac, max(ar, br))
    R = (d + ar + br) / 2.0
    t = (R - ar) / d
    c = tuple(ac[i] + (bc[i] - ac[i]) * t for i in range(3))
    return (c, R)


def main():
    exe, data, cards = sys.argv[1], sys.argv[2], sys.argv[3]
    fails = 0
    for spec in sys.argv[4:]:
        cid, rel = spec.split(':', 1)
        nif = os.path.join(data, 'meshes', rel)
        m = lodm(os.path.join(cards, cid + '_oct.lodm'))['card']
        C, HW, HH = m['center'], m['half'][0], m['half'][1]
        # PER-FRAME POSITIONING: the largest displacement any one frame's quad
        # carries, as a scalar, since the view basis is not known here and the
        # bound is |offset| whatever it is
        fo = m.get('frameOffset') or []
        worst = max((math.hypot(fo[2 * k], fo[2 * k + 1]) for k in range(len(fo) // 2)),
                    default=0.0)
        fw, fh = m['frame']
        px, py = m.get('pad', [max(4, max(fw, fh) // 16)] * 2)
        span = m['depthSpan']
        sph = None
        for b, ty in blocks(exe, nif):
            if 'TriShape' not in ty:
                continue
            c, r = bound_of(exe, nif, b)
            if c and r:
                sph = merge(sph, (c, r))
        if sph is None:
            print('%s: no TriShape bounds in %s' % (cid, rel)); fails += 1; continue
        bc, br = sph
        d = math.dist(C, bc)
        # the same distance if a reader ignored `center` and used the pivot
        d0 = math.dist((0.0, 0.0, 0.0), bc)
        # the card's inner half extents: the silhouette's own rectangle
        ihw, ihh = HW * (fw - 2 * px) / fw, HH * (fh - 2 * py) / fh
        wantSpan = 3.0 * max(br, 1024.0)
        # THE TOLERANCE, and why it is not one texel.
        #
        # The independent instrument here is the model's OWN declared per-shape
        # `Bounding Sphere` fields, merged. The renderer does not use those: it
        # recomputes each shape's bound FROM THE VERTICES (glmesh.cpp,
        # `boundSphere = BoundSphere( verts )`), which is tighter and differently
        # centred. So this can only bound `card.center`, never confirm it to a
        # texel -- and the honest gate is a DISCRIMINATION against its own
        # control rather than an absolute tolerance:
        #
        #   `center` as shipped must be at least four times closer to the
        #   model's own bound centre than the pivot is.
        #
        # A card whose centre were wrong by the amount a zeroed field is wrong
        # by -- most of a tree's height -- cannot pass this, and the ratio is
        # reported so a tighter instrument can replace it later.
        RATIO = 4.0
        okC = d0 > 0 and d * RATIO <= d0
        # THE MOST DISPLACED FRAME. `worst` is a scalar, because the view basis is
        # not known here, so `d + worst` is the pessimistic case: every frame's
        # offset pointing straight away from the model's bound centre.
        dF = d + worst
        # it must still be CLOSER to the bound centre than the pivot is (reported
        # as a ratio, not thresholded -- the 4x discrimination belongs to
        # `card.center`, which is a point, not to this upper bound)
        okF = d0 > 0 and dF < d0
        # and its quad must still lie inside the model's own bound sphere...
        inner = max(HW * (fw - 2 * px) / fw, HH * (fh - 2 * py) / fh)
        okFin = dF + inner <= br * 1.25
        # ...which is the check, and THIS is its control: the same quad anchored
        # at the pivot instead of at `center` must NOT fit
        okFinCtl = d0 + inner > br * 1.25
        okS = abs(span - wantSpan) <= 0.05 * wantSpan
        okIn = max(ihw, ihh) <= br * 1.02
        okBig = max(ihw, ihh) >= 0.60 * br
        okCtl = d0 > 4.0 * d
        print('%-9s %-34s bound centre (%.1f, %.1f, %.1f) r %.1f' % (cid, rel, bc[0], bc[1], bc[2], br))
        print('           card.center  (%.1f, %.1f, %.1f)   |delta| %.1f units = %.1f%% of r;'
              ' the pivot is %.1f units = %.1f%% of r away (%.1fx further)   %s'
              % (C[0], C[1], C[2], d, 100.0 * d / br, d0, 100.0 * d0 / br,
                 d0 / max(d, 1e-6), 'ok' if okC else 'FAIL'))
        print('           depthSpan %.1f, 3*max(r,1024) = %.1f (r from the file-declared spheres, looser than the renderer)   %s'
              % (span, wantSpan, 'ok' if okS else 'FAIL'))
        print('           inner half extents %.1f x %.1f  (%.0f%% of the bound radius)   %s'
              % (ihw, ihh, 100.0 * max(ihw, ihh) / br, 'ok' if okIn and okBig else 'FAIL'))
        print('           CONTROL, center zeroed: the quad would sit %.1f units away   %s'
              % (d0, 'ok (the control fails, as it must)' if okCtl else 'FAIL'))
        print('           per frame: largest quad displacement %.1f units;'
              ' the most displaced frame is %.1f units = %.1f%% of r from the bound centre'
              ' (%.1fx closer than the pivot), quad %.1f + %.1f against r %.1f   %s'
              % (worst, dF, 100.0 * dF / br, d0 / max(dF, 1e-6), dF, inner, br,
                 'ok' if okF and okFin else 'FAIL'))
        print('           CONTROL, the most displaced frame anchored at the PIVOT:'
              ' %.1f + %.1f units against a bound radius of %.1f   %s'
              % (d0, inner, br, 'ok (it does not fit, as it must not)' if okFinCtl else 'FAIL'))
        if not fo:
            print('           NOTE: this set carries no frameOffset -- a bake from before'
                  ' per-frame positioning, and the per-frame rows above are the fixed-centre case')
        for f in (okC, okS, okIn, okBig, okCtl, okF, okFin, okFinCtl):
            if not f:
                fails += 1
    print()
    print('TRANSITION (bounds) GATE:', 'PASS' if fails == 0 else 'FAIL (%d)' % fails)
    return 0 if fails == 0 else 1


sys.exit(main())
