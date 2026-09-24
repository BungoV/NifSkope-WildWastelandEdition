#!/usr/bin/env python3
"""Compare two clip TSVs row for row (lane HKX5's round-trip metric).

Columns: frame track bone tx ty tz qx qy qz qw sx sy sz, root motion as
track -1 (tx ty tz yaw).  The angle metric is the ww-hkx-animation skill's
2*asin(|q1 -+ q2|/2) -- never acos(dot), which has no resolution below 0.03 deg.

usage: python tsvcmp.py A.tsv B.tsv [--tol-t 1e-4] [--tol-deg 0.01] [--label X]
                        [--map-track] [--worst N]
--map-track keys rows by BONE instead of track index, for a comparison across
a mapping that reordered the tracks.
exit 0 when every bar is met, 1 otherwise.
"""
import math, sys


def load(path, bykey='track'):
    rows, root = {}, {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        p = line.rstrip('\n').split('\t')
        f, t, b = int(p[0]), int(p[1]), int(p[2])
        if t == -1:
            root[f] = tuple(float(x) for x in p[3:7])
            continue
        key = (f, b if bykey == 'bone' else t)
        rows[key] = tuple(float(x) for x in p[3:13])
    return rows, root


def qang(a, b):
    """Degrees of ROTATION between two unit quaternions, on the shortest arc.

    4*asin(|q1 -+ q2|/2), NOT the 2*asin(...) the ww-hkx-animation skill gives:
    for q2 = R(theta)*q1 the chord is |q1-q2| = 2*sin(theta/4), so 2*asin(d/2)
    is theta/2 and reads a 45-degree difference as 22.5.  Measured against
    known angles 0.5 / 5 / 45 / 120 deg on 2026-09-10 (lane HKX5, root
    MISTAKES.md).  asin is kept over 2*acos(|dot|), which agrees exactly but
    has no resolution below about 0.03 deg in float.
    """
    d1 = math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(4)))
    d2 = math.sqrt(sum((a[i] + b[i]) ** 2 for i in range(4)))
    d = min(d1, d2)
    return 4.0 * math.degrees(math.asin(min(1.0, d / 2.0)))


def compare(pa, pb, tol_t=1e-4, tol_deg=0.01, label="", bykey='track', worst=3,
            only_common=False, ignore_root=False):
    A, RA = load(pa, bykey)
    B, RB = load(pb, bykey)
    ok = True
    if only_common:
        common = set(A) & set(B)
        dropped_a, dropped_b = len(set(A) - common), len(set(B) - common)
        print("%s   restricted to the %d rows both sides have (A dropped %d, B dropped %d)"
              % (label, len(common), dropped_a, dropped_b))
        if not common:
            print("%s FAIL: the two sides share no row" % label)
            return False, {}
        A = {k: A[k] for k in common}
        B = {k: B[k] for k in common}
    if ignore_root:
        RA, RB = {}, {}
    if set(A) != set(B):
        only_a, only_b = len(set(A) - set(B)), len(set(B) - set(A))
        print("%s FAIL: %d rows in A only, %d in B only (A %d rows, B %d)"
              % (label, only_a, only_b, len(A), len(B)))
        return False, {}
    mt = md = ms = 0.0
    wt = wd = None
    for k in A:
        a, b = A[k], B[k]
        for i in range(3):
            e = abs(a[i] - b[i])
            if e > mt:
                mt, wt = e, k
        ang = qang(a[3:7], b[3:7])
        if ang > md:
            md, wd = ang, k
        for i in range(7, 10):
            ms = max(ms, abs(a[i] - b[i]))
    rt = rd = 0.0
    if RA or RB:
        if set(RA) != set(RB):
            print("%s FAIL: root motion has %d frames in A, %d in B" % (label, len(RA), len(RB)))
            ok = False
        else:
            for f in RA:
                for i in range(3):
                    rt = max(rt, abs(RA[f][i] - RB[f][i]))
                rd = max(rd, abs(math.degrees(RA[f][3] - RB[f][3])))
    passed = mt <= tol_t and md <= tol_deg and ms <= tol_t and rt <= tol_t and rd <= tol_deg and ok
    print("%s %s: %d rows; max |dT| %.3e (bar %.0e) at %s, max angle %.4e deg (bar %g) at %s, "
          "max |dScale| %.3e, root motion max |dT| %.3e max |dYaw| %.3e deg"
          % (label, "PASS" if passed else "FAIL", len(A), mt, tol_t, wt, md, tol_deg, wd, ms, rt, rd))
    return passed, dict(rows=len(A), maxT=mt, maxDeg=md, maxS=ms, rootT=rt, rootDeg=rd)


if __name__ == '__main__':
    a = sys.argv[1:]
    def opt(k, d):
        if k in a:
            i = a.index(k)
            v = a[i + 1]
            del a[i:i + 2]
            return v
        return d
    tol_t = float(opt('--tol-t', '1e-4'))
    tol_deg = float(opt('--tol-deg', '0.01'))
    label = opt('--label', '')
    worst = int(opt('--worst', '3'))
    bykey = 'bone' if '--map-track' in a else 'track'
    only_common = '--only-common' in a
    ignore_root = '--ignore-root' in a
    a = [x for x in a if x not in ('--map-track', '--only-common', '--ignore-root')]
    ok, _ = compare(a[0], a[1], tol_t, tol_deg, label, bykey, worst, only_common, ignore_root)
    sys.exit(0 if ok else 1)
