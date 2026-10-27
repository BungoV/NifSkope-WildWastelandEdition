#!/usr/bin/env python3
"""Lane VOLRAY1 (2026-10-27) checker: `NifSkope -no-gui volcheck` (src/probevolcheck.cpp), the ray check of the
Space stage's shapes against the soup's real triangles.

  synth <exe> <work> [--red <name>] [--rays cpu|gpu|both]
    known     hand-written .space files with KNOWN answers, so the checker is calibrated before it judges the fitter:
                lroom_exact   two boxes that fill the L room to its walls: PASS, gaps and pokes 0
                lroom_poke    the long box grown 48 past its +x wall: FAIL, a poke 16..64 deep at x > 816
                lroom_gap     the short leg's box missing: FAIL, a gap blob in the leg (x < 300, y > 300)
                twins_over    A's box runs 64 into B through the doorway wall: FAIL, a cross-wall overlap
                twins_exact   both rooms filled, the doorway a threshold: PASS
    fitted    the fitter's own output (`volumes --interior`) on lroom, church (sloped roof), thin (a 2-unit wall
              between two rooms), twins (a doorway), house (door + window): its numbers per scene, and each
              must PASS (0 gap blobs over the tolerance, 0 pokes, 0 overlaps)
    mutate    the fitted lroom / twins / church with --mutate grow:24, shift:48, drop: each must FAIL
    gpu       with --rays both: the GPU and the CPU answers equal on every lattice point of lroom (skipped, named,
              when no RT device)
  Reds (each must FAIL): known (the checker's known-answer rows read as the opposite verdict), nomutate (the mutate
  rows run unmutated), norefine (the fitter without its ray refine of the faces: the cell-wide inset is a gap).
"""

import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cell_volumes_check as cv   # noqa: E402  (scenes, soup writer)

T = cv.T


def scene_thin():
    """Two rooms side by side, a 2-unit wall between them, each with its own outer door."""
    tris = cv.ground(-800, -800, 2000, 1300)
    h = 300
    # A: x 0..600, B: x 602..1200, y 0..500; the shared wall x 600..602
    tris += cv.wall('x', -T, 0, -T, 1200 + T, 0, h, [(200, 328, 0, 220), (800, 928, 0, 220)])
    tris += cv.wall('x', 500, 500 + T, -T, 1200 + T, 0, h)
    tris += cv.wall('y', -T, 0, 0, 500, 0, h)
    tris += cv.wall('y', 1200, 1200 + T, 0, 500, 0, h)
    tris += cv.sbox(600, 0, 0, 602, 500, h)
    tris += cv.sbox(-T, -T, h, 1200 + T, 500 + T, h + T)
    return dict(tris=tris, rooms={'A': [(300, 250, 150)], 'B': [(900, 250, 150)]}, outside=[(300, -400, 150)])


def space_file(path, key, rooms, thresholds=()):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    doc = dict(format='space', version=1, cell=key, rooms=[], thresholds=[])
    for rid, anchor, shapes in rooms:
        doc['rooms'].append(dict(room=rid, anchor=list(anchor), source='test', shapes=[
            dict(type=t, center=list(c), size=list(s), rotation=[0, 0, 0]) for t, c, s in shapes]))
    for i, (a, b, c, s) in enumerate(thresholds):
        doc['thresholds'].append(dict(threshold='T%04d' % (i + 1), between=[a, b], opening='door',
                                      shape=dict(type='box', center=list(c), size=list(s), rotation=[0, 0, 0])))
    with open(path, 'w') as f:
        json.dump(doc, f, indent=1)


def volcheck(exe, soup, space, out, extra=()):
    os.makedirs(out, exist_ok=True)
    cmd = [exe, '-no-gui', 'volcheck', '--soup', soup, '--space', space, '--out', out,
           '--png', os.path.join(out, 'volcheck.png')] + list(extra)
    p = subprocess.run(cmd, capture_output=True, text=True)
    text = p.stdout + p.stderr
    with open(os.path.join(out, 'volcheck.txt'), 'w') as f:
        f.write(text)
    return p.returncode, text


def verdict(text):
    m = re.search(r'VERDICT (PASS|FAIL): gap blobs (\d+), unclaimed blobs (\d+), wall pokes (\d+) u3, '
                  r'cross-wall overlaps (\d+) u3', text)
    if not m:
        return None
    return dict(verdict=m.group(1), blobs=int(m.group(2)) + int(m.group(3)), unclaimed=int(m.group(3)),
                poke=int(m.group(4)), overlap=int(m.group(5)))


def rooms_tsv(out):
    rows = []
    with open(os.path.join(out, 'volcheck_rooms.tsv')) as f:
        head = f.readline().rstrip('\n').split('\t')
        for line in f:
            rows.append(dict(zip(head, line.rstrip('\n').split('\t'))))
    return rows


def parse_at(s):
    m = re.match(r'\((-?\d+), (-?\d+), (-?\d+)\)', s or '')
    return tuple(int(v) for v in m.groups()) if m else None


def synth(exe, work, red, rays):
    fails, log = [], []
    os.makedirs(work, exist_ok=True)
    rx = ['--rays', rays] if rays else []

    def soup_of(name, s):
        p = os.path.join(work, name + '.psp')
        cv.write_soup(p, s)
        return p

    def known(name, scene, rooms, thresholds, want_pass, check=None):
        soup = soup_of('known_' + name, scene)
        sp = os.path.join(work, 'known_' + name, 'space', name + '.space')
        space_file(sp, name, rooms, thresholds)
        out = os.path.join(work, 'known_' + name, 'check')
        rc, text = volcheck(exe, soup, sp, out, rx)
        v = verdict(text)
        log.append('known %s: rc %d %s' % (name, rc, v))
        for l in text.splitlines():
            if l.startswith('volcheck: cell') or l.startswith('volcheck: spot'):
                log.append('  ' + l)
        if v is None:
            fails.append('known %s: no verdict (rc %d): %s' % (name, rc, text[-400:]))
            return
        want = want_pass if red != 'known' else not want_pass
        if (v['verdict'] == 'PASS') != want:
            fails.append('known %s: VERDICT %s, want %s' % (name, v['verdict'], 'PASS' if want else 'FAIL'))
        if check:
            msg = check(v, rooms_tsv(out))
            if msg:
                fails.append('known %s: %s' % (name, msg))

    # ---- known answers: the L room
    L = cv.scene_lroom()
    long_box = ('box', (400, 150, 150), (800, 300, 300))
    leg_box = ('box', (150, 550, 150), (300, 500, 300))
    door_lr = [('0x0001', 'outside', (164, -8, 110), (128, 16, 220))]
    known('lroom_exact', L, [('0x0001', (600, 150, 150), [long_box, leg_box])], door_lr, True,
          lambda v, r: None if v['blobs'] == 0 and v['poke'] == 0 else 'gaps %d pokes %d u3' % (v['blobs'], v['poke']))

    def poke_check(v, rows):
        r = rows[0]
        at = parse_at(r['poke_at'])
        d = float(r['poke_max'])
        if not (16 <= d <= 64) or at is None or at[0] <= 800:
            return 'poke depth %.1f at %s (want 16..64 at x > 800)' % (d, at)
        return None
    known('lroom_poke', L, [('0x0001', (600, 150, 150), [('box', (424, 150, 150), (848, 300, 300)), leg_box])], door_lr,
          False, poke_check)

    def gap_check(v, rows):
        r = rows[0]
        at = parse_at(r['worst_blob_at'])
        if v['blobs'] < 1 or at is None or not (at[0] < 300 and at[1] > 300):
            return 'gap blob at %s (want one in the leg, x < 300, y > 300)' % (at,)
        return None
    known('lroom_gap', L, [('0x0001', (600, 150, 150), [long_box])], door_lr, False, gap_check)

    # ---- known answers: the twins (A x 0..600, B x 616..1216, the doorway y 200..328 in A's +x wall)
    TW = cv.scene_twins()
    a_box = ('box', (300, 250, 150), (600, 500, 300))
    b_box = ('box', (916, 250, 150), (600, 500, 300))
    thr = [('0x0001', '0x0002', (608, 264, 110), (16, 128, 220)), ('0x0001', 'outside', (264, -8, 110), (128, 16, 220))]
    known('twins_exact', TW, [('0x0001', (300, 250, 150), [a_box]), ('0x0002', (900, 250, 150), [b_box])], thr, True)

    def over_check(v, rows):
        if v['overlap'] <= 0:
            return 'no cross-wall overlap reported'
        return None
    known('twins_over', TW, [('0x0001', (300, 250, 150), [('box', (332, 250, 150), (664, 500, 300))]),
                             ('0x0002', (900, 250, 150), [b_box])], thr, False, over_check)

    # ---- the fitter's output
    scenes = dict(lroom=cv.scene_lroom(), church=cv.scene_church(), thin=scene_thin(), twins=cv.scene_twins(),
                  house=cv.scene_house())
    fitted = {}
    for name, s in scenes.items():
        soup = soup_of('fit_' + name, s)
        out = os.path.join(work, 'fit_' + name)
        if os.path.isdir(out):
            shutil.rmtree(out)
        rc, text = cv.run(exe, soup, out, name, 'norefine' if red == 'norefine' else '')
        if rc != 0:
            fails.append('fitted %s: volumes rc %d: %s' % (name, rc, text[-300:]))
            continue
        space = os.path.join(out, 'Interiors', name)
        rc, text = volcheck(exe, soup, space, os.path.join(out, 'check'), rx)
        v = verdict(text)
        shapes = [r['shapes'] for r in rooms_tsv(os.path.join(out, 'check'))] if v else []
        fitted[name] = (soup, space, shapes)
        log.append('fitted %s: rc %d %s shapes per room %s' % (name, rc, v, shapes))
        for l in text.splitlines():
            if l.startswith('volcheck: cell') or l.startswith('volcheck: spot') or 'volume pass cost' in l:
                log.append('  ' + l)
        if v is None or v['verdict'] != 'PASS':
            fails.append('fitted %s: %s' % (name, v))

    # ---- mutations of the fitted shapes: each must FAIL
    for name in ('lroom', 'twins', 'church'):
        if name not in fitted:
            continue
        soup, space, shapes = fitted[name]
        for m in ('grow:24', 'shift:48', 'drop'):
            extra = rx + ([] if red == 'nomutate' else ['--mutate', m])
            rc, text = volcheck(exe, soup, space, os.path.join(work, 'mut_%s_%s' % (name, m.replace(':', ''))), extra)
            v = verdict(text)
            log.append('mutate %s %s: %s' % (name, m, v))
            if m == 'drop' and all(s == '1' for s in shapes):
                log.append('  (named skip: every room is one shape, --mutate drop leaves it whole)')
                continue
            if v is None or v['verdict'] != 'FAIL':
                fails.append('mutate %s %s: %s (a broken fit must FAIL)' % (name, m, v))

    # ---- GPU == CPU on every point
    if 'lroom' in fitted:
        soup, space = fitted['lroom'][:2]
        rc, text = volcheck(exe, soup, space, os.path.join(work, 'both_lroom'), ['--rays', 'both'])
        m = re.search(r'GPU vs CPU: rays (\d+) \+ (\d+) each way; GPU (\d+) ms \+ (\d+) ms, CPU (\d+) ms \+ (\d+) ms '
                      r'\(edges \+ sphere\); points whose edge answers differ (\d+), whose sphere answers differ (\d+) of (\d+)', text)
        if m:
            log.append('gpu lroom: ' + m.group(0))
            if int(m.group(7)) or int(m.group(8)):
                fails.append('gpu: GPU and CPU answers differ: ' + m.group(0))
        else:
            path = re.search(r'rays \d+ on ([^:]+):', text)
            log.append('gpu lroom: SKIPPED, no GPU path (%s)' % (path.group(1) if path else text[-200:]))
    return fails, log


def main():
    a = sys.argv[1:]
    if len(a) < 3 or a[0] != 'synth':
        print(__doc__)
        return 2
    red = a[a.index('--red') + 1] if '--red' in a else ''
    rays = a[a.index('--rays') + 1] if '--rays' in a else ''
    fails, log = synth(a[1], a[2], red, rays)
    for l in log:
        print(l)
    for f in fails:
        print('FAIL ' + f)
    print('VERDICT %s (%d failures)%s' % ('PASS' if not fails else 'FAIL', len(fails), ' red ' + red if red else ''))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
