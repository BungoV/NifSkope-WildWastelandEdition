#!/usr/bin/env python3
"""The octahedral impostor's drawing mapping, written from the SPEC and not
from the C++.

`docs/LODGEN_IMPOSTOR_SPEC.md` lines 226..231 give the forward mapping; this
file inverts it, splits the grid cell and weights the three frames, and prints
the result so `tests/spells/impostor_draw.sh` can diff it against the oracle
built out of `src/impostoroct.cpp`. Two implementations that agree on a table
of directions are a mapping; one implementation is a hope.

It is also the file that answers row (a) of the brief's gate: the frame index
chosen for eight named camera azimuths.

    python3 impostor_oct_ref.py table N      one row per probe direction
    python3 impostor_oct_ref.py selftest     the properties, no oracle needed
"""

import math
import sys

MIN_GRID, MAX_GRID = 2, 16


def frame_dir(i, j, n):
    """Spec 226..231, forward. Unit direction frame (i, j) was shot from."""
    if not (MIN_GRID <= n <= MAX_GRID) or not (0 <= i < n) or not (0 <= j < n):
        return None
    u = i / (n - 1) * 2.0 - 1.0
    v = j / (n - 1) * 2.0 - 1.0
    x = (u + v) * 0.5
    y = (u - v) * 0.5
    z = 1.0 - abs(x) - abs(y)
    m = math.sqrt(x * x + y * y + z * z)
    if m <= 1e-20:
        return None
    return (x / m, y / m, z / m)


def dir_to_grid(d, n):
    """SPEC GAP #1: the algebraic inverse, plus the hemisphere clamp."""
    m = math.sqrt(sum(c * c for c in d))
    if m <= 1e-20:
        return None
    e = [c / m for c in d]
    if e[2] < 0.0:                       # no view from below the horizon
        e[2] = 0.0
        m = math.sqrt(sum(c * c for c in e))
        if m <= 1e-20:
            return None
        e = [c / m for c in e]
    L = abs(e[0]) + abs(e[1]) + e[2]
    if L <= 1e-20:
        return None
    x, y = e[0] / L, e[1] / L
    u, v = x + y, x - y
    hi = float(n - 1)
    fi = min(max((u + 1.0) * 0.5 * (n - 1), 0.0), hi)
    fj = min(max((v + 1.0) * 0.5 * (n - 1), 0.0), hi)
    return fi, fj


def pick_frames(d, n):
    """SPEC GAP #2: the cell diagonal (i+1,j)--(i,j+1), barycentric in grid.

    Returns [(index, gi, gj, weight)] x 3, index = gi + gj*n."""
    g = dir_to_grid(d, n)
    if g is None:
        return None
    fi, fj = g
    i0 = min(max(int(math.floor(fi)), 0), n - 2)
    j0 = min(max(int(math.floor(fj)), 0), n - 2)
    a, b = fi - i0, fj - j0
    if a + b <= 1.0:
        tri = [(i0, j0, 1.0 - a - b), (i0 + 1, j0, a), (i0, j0 + 1, b)]
    else:
        tri = [(i0 + 1, j0 + 1, a + b - 1.0), (i0, j0 + 1, 1.0 - a), (i0 + 1, j0, 1.0 - b)]
    return [(gi + gj * n, gi, gj, max(w, 0.0)) for (gi, gj, w) in tri]


def frame_rect(i, j, n):
    """Spec 225: frame (i,j) at pixel (i*frameW, j*frameH) of an NxN sheet."""
    s = 1.0 / n
    return (i * s, j * s, s, s)


def height_to_units(h, depth_span):
    """Spec 284..286."""
    return (h - 0.5) * depth_span


def frame_angles(d):
    """The bake's own two angles, src/nifskope_ui.cpp:22748."""
    m = math.sqrt(sum(c * c for c in d))
    e = [c / m for c in d]
    return (math.degrees(math.asin(min(max(e[2], -1.0), 1.0))),
            math.degrees(math.atan2(e[1], e[0])))


def frame_basis(d):
    """Matrix::fromEuler(rotX, 0, rotZ) with the bake's angles.

    Its ROWS are the camera axes in world space -- checked against all six of
    GLView::viewRotations, see the block at the top of src/impostoroct.cpp.
    row2 is where the camera SITS.

    rz = 270 - azim since the repair of 2026-09-19, which makes row2 == d. The
    old rz = 90 - azim put the camera at (-d.x, -d.y, +d.z) -- the 180-degree
    azimuth turn -- and AS_BAKED selects it so a legacy set can still be read
    and so the repair has a red control."""
    elev, azim = frame_angles(d)
    rx = math.radians(-90.0 + elev)
    rz = math.radians((90.0 if AS_BAKED else 270.0) - azim)
    sx, cx = math.sin(rx), math.cos(rx)
    sz, cz = math.sin(rz), math.cos(rz)

    def unit(v):
        m = math.sqrt(sum(c * c for c in v))
        return tuple(c / m for c in v) if m > 1e-20 else v

    return (unit((cz, -sz, 0.0)),                 # right
            unit((sz * cx, cx * cz, -sx)),        # up
            unit((sx * sz, sx * cz, cx)))         # fwd: where the camera sits


def bake_camera_dir(d):
    """Where the bake's camera stood for spec direction d.

    Since the repair that IS d. Under AS_BAKED (a legacy set) it is the azimuth
    turned by 180 degrees."""
    return (-d[0], -d[1], d[2]) if AS_BAKED else tuple(d)


# Mirrors ImpostorOct::g_convention, whose default is SpecLiteral since the
# azimuth repair of 2026-09-19. True here = a LEGACY set, baked before it.
AS_BAKED = False


def grid_lookup_dir(cam):
    return (-cam[0], -cam[1], cam[2]) if AS_BAKED else tuple(cam)


def pick_frames_for_camera(cam, n):
    return pick_frames(grid_lookup_dir(cam), n)


# The eight named camera azimuths of the brief's gate row (a), at two
# elevations. Named so a failure says WHICH view disagreed.
AZIMUTHS = [("E", 0), ("NE", 45), ("N", 90), ("NW", 135),
            ("W", 180), ("SW", 225), ("S", 270), ("SE", 315)]
ELEVATIONS = [("low", 10.0), ("high", 55.0)]


def probe_dirs():
    for aname, adeg in AZIMUTHS:
        for ename, edeg in ELEVATIONS:
            a, e = math.radians(adeg), math.radians(edeg)
            yield ("%s-%s" % (aname, ename),
                   (math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)))
    # the rim and the pole, the two places a mapping breaks
    yield ("horizon-E", (1.0, 0.0, 0.0))
    yield ("horizon-N", (0.0, 1.0, 0.0))
    yield ("top", (0.0, 0.0, 1.0))
    yield ("below", (0.6, 0.2, -0.8))   # must clamp to the horizon, not wrap


def table(n):
    out = []
    for name, d in probe_dirs():
        f = pick_frames(d, n)
        if f is None:
            out.append("%s REFUSED" % name)
            continue
        parts = ["%s" % name]
        for (idx, gi, gj, w) in f:
            parts.append("%d %d %d %.6f" % (idx, gi, gj, w))
        out.append(" ".join(parts))
    return out


def selftest():
    """Properties that hold whatever the oracle says, so a green diff between
    two implementations of the same mistake is still caught."""
    fails = []
    for n in (2, 3, 4, 5, 8, 12, 16):
        # 1. round trip: a frame's own direction picks that frame with weight 1
        for i in range(n):
            for j in range(n):
                d = frame_dir(i, j, n)
                f = pick_frames(d, n)
                best = max(f, key=lambda t: t[3])
                if best[1] != i or best[2] != j or abs(best[3] - 1.0) > 1e-4:
                    fails.append("N=%d roundtrip (%d,%d) -> (%d,%d) w=%.4f"
                                 % (n, i, j, best[1], best[2], best[3]))
        # 2. weights sum to 1 everywhere
        for name, d in probe_dirs():
            f = pick_frames(d, n)
            s = sum(t[3] for t in f)
            if abs(s - 1.0) > 1e-5:
                fails.append("N=%d weights sum %.6f at %s" % (n, s, name))
        # 3. corners are exact horizon directions (spec 231)
        for (i, j) in ((0, 0), (n - 1, 0), (0, n - 1), (n - 1, n - 1)):
            d = frame_dir(i, j, n)
            if abs(d[2]) > 1e-5:
                fails.append("N=%d corner (%d,%d) z=%.6f not on the horizon" % (n, i, j, d[2]))
        # 4. continuity across the cell diagonal: two directions a hair apart
        #    must not swap to a disjoint frame set with a big weight
        if n >= 3:
            for t in (0.4999, 0.5001):
                d1 = frame_dir(0, 0, n)
                d2 = frame_dir(1, 1, n)
                mid = [d1[k] * (1 - t) + d2[k] * t for k in range(3)]
                f = pick_frames(mid, n)
                if f is None or abs(sum(x[3] for x in f) - 1.0) > 1e-5:
                    fails.append("N=%d diagonal t=%s" % (n, t))
        # 5. the spec's own claim, checked rather than repeated: the centre of
        #    the grid is the top, and IS a frame only when N is odd.
        d = pick_frames((0.0, 0.0, 1.0), n)
        exact = max(d, key=lambda x: x[3])[3] > 1.0 - 1e-5
        if exact != (n % 2 == 1):
            fails.append("N=%d centre-frame-is-top exact=%s odd=%s (SPEC GAP #3)"
                         % (n, exact, n % 2 == 1))
    # 6. the basis is orthonormal and right-handed for every probe
    for name, d in probe_dirs():
        r, u, f = frame_basis(d)
        for (an, a) in (("r.u", sum(r[k] * u[k] for k in range(3))),
                        ("r.f", sum(r[k] * f[k] for k in range(3))),
                        ("u.f", sum(u[k] * f[k] for k in range(3)))):
            if abs(a) > 1e-4:
                fails.append("basis %s at %s = %.6f" % (an, name, a))
    # 7. the horizon frames are upright: up == world +Z
    for d in ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, -1.0, 0.0)):
        _, u, _ = frame_basis(d)
        if abs(u[2] - 1.0) > 1e-4:
            fails.append("horizon up not +Z: %s -> %s" % (d, u))
    # 8. THE REPAIR, stated as a property rather than as prose (2026-09-19,
    #    bungo's "fix the 180 issue"): the camera the bake drives for a frame
    #    SITS AT THAT FRAME'S OWN SPEC DIRECTION. This is the whole content of
    #    the repair, and it is the row that fails on the old rz = 90 - azim.
    #
    #    The second half is the red control, run here rather than described:
    #    force AS_BAKED (the legacy formula) and the same equality must BREAK
    #    by 180 degrees of azimuth on everything off the pole. A property that
    #    cannot be made to fail is not measuring anything.
    global AS_BAKED
    saved_conv = AS_BAKED
    try:
        AS_BAKED = False
        for name, d in probe_dirs():
            if d[2] < 0:
                continue
            m = math.sqrt(sum(c * c for c in d))
            e = [c / m for c in d]
            _, _, f = frame_basis(d)
            if max(abs(f[k] - e[k]) for k in range(3)) > 1e-4:
                fails.append("repaired bake camera at %s: %s, expected the frame's own"
                             " direction %s" % (name, f, e))
        AS_BAKED = True
        for name, d in probe_dirs():
            if d[2] < 0 or abs(d[0]) + abs(d[1]) < 1e-3:
                continue        # the pole has no azimuth to turn
            m = math.sqrt(sum(c * c for c in d))
            e = [c / m for c in d]
            _, _, f = frame_basis(d)
            want = (-e[0], -e[1], e[2])
            if max(abs(f[k] - want[k]) for k in range(3)) > 1e-4:
                fails.append("RED CONTROL BROKEN: the legacy formula at %s gave %s,"
                             " not the 180-degree turn %s" % (name, f, want))
            if max(abs(f[k] - e[k]) for k in range(3)) < 1e-4:
                fails.append("RED CONTROL BROKEN: the legacy formula at %s agrees with"
                             " the repaired one, so property 8 proves nothing" % name)
    finally:
        AS_BAKED = saved_conv
    # 9. up is the world +Z projected perpendicular to fwd -- roll zero, which
    #    is what lets a consumer unproject a texel at all
    for name, d in probe_dirs():
        r, u, f = frame_basis(d)
        dot = f[2]
        proj = [(0.0, 0.0, 1.0)[k] - dot * f[k] for k in range(3)]
        m = math.sqrt(sum(c * c for c in proj))
        if m < 1e-4:
            continue                       # straight up/down: roll is free
        proj = [c / m for c in proj]
        if max(abs(u[k] - proj[k]) for k in range(3)) > 1e-4:
            fails.append("up not projected +Z at %s: %s vs %s" % (name, u, proj))
    # 10. right == up x fwd exactly (no sign left to guess)
    for name, d in probe_dirs():
        r, u, f = frame_basis(d)
        c = (u[1] * f[2] - u[2] * f[1], u[2] * f[0] - u[0] * f[2], u[0] * f[1] - u[1] * f[0])
        if max(abs(r[k] - c[k]) for k in range(3)) > 1e-4:
            fails.append("right != up x fwd at %s: %s vs %s" % (name, r, c))
    # 11. the convention round-trips UNDER BOTH: looking from where that bake's
    #     camera stood picks the frame back, with weight 1. Run for the legacy
    #     vintage too, because opening an old set is the one thing AsBaked is
    #     still for and a broken round trip there is a broken diagnostic.
    saved_conv = AS_BAKED
    try:
        for conv in (False, True):
            AS_BAKED = conv
            label = "AsBaked" if conv else "SpecLiteral"
            for n in (4, 8):
                for i in range(n):
                    for j in range(n):
                        d = frame_dir(i, j, n)
                        cam = bake_camera_dir(d)
                        best = max(pick_frames_for_camera(cam, n), key=lambda t: t[3])
                        if best[1] != i or best[2] != j or abs(best[3] - 1.0) > 1e-4:
                            fails.append("N=%d %s round trip (%d,%d) -> (%d,%d) w=%.4f"
                                         % (n, label, i, j, best[1], best[2], best[3]))
    finally:
        AS_BAKED = saved_conv
    return fails


def compare(oracle_path, n, wtol=2e-4):
    """Oracle rows against this file's rows. Frame INDICES must match exactly
    -- a mapping that picks a different frame is a different mapping -- and
    weights within `wtol`, because the oracle is float and this is double and
    the last digit of a barycentric weight is not a fact about the spec."""
    def blend(fields):
        """A row as the only thing that is a fact about it: frame index ->
        weight, with zero-weight frames dropped.

        The ORDER of the three is not a fact, and neither is which frames a
        direction exactly on a cell's diagonal is said to sit between. On
        a + b == 1 the two triangles share an edge and the third frame carries
        weight 0, so float (the oracle) and double (this file) land on opposite
        branches and name different third frames -- and DRAW THE SAME PICTURE,
        because the weights are continuous there. Selftest property 4 is what
        proves that continuity; this comparison must not fail on it, or the
        gate reports a difference in arithmetic precision as a difference in
        the mapping."""
        out = {}
        for k in range(3):
            w = float(fields[k * 4 + 3])
            if w <= wtol:
                continue
            out[int(fields[k * 4 + 0])] = out.get(int(fields[k * 4 + 0]), 0.0) + w
        return out

    mine = {r.split()[0]: r.split()[1:] for r in table(n)}
    fails = []
    seen = 0
    with open(oracle_path, "r") as fh:
        for raw in fh:
            raw = raw.strip()
            if not raw:
                continue
            tok = raw.split()
            name = tok[0]
            seen += 1
            if name not in mine:
                fails.append("%s: oracle has a row the reference does not" % name)
                continue
            if len(tok) - 1 != len(mine[name]):
                fails.append("%s: %d fields vs %d" % (name, len(tok) - 1, len(mine[name])))
                continue
            a, b = blend(tok[1:]), blend(mine[name])
            for idx in sorted(set(a) | set(b)):
                wa, wb = a.get(idx, 0.0), b.get(idx, 0.0)
                if abs(wa - wb) > wtol:
                    fails.append("%s frame %d: oracle %.6f, reference %.6f (tol %g)"
                                 % (name, idx, wa, wb, wtol))
            for label, s in (("oracle", a), ("reference", b)):
                if abs(sum(s.values()) - 1.0) > 1e-4:
                    fails.append("%s %s weights sum %.6f" % (name, label, sum(s.values())))
    if seen != len(mine):
        fails.append("row count: oracle %d, reference %d" % (seen, len(mine)))
    return seen, fails


if __name__ == "__main__":
    if len(sys.argv) >= 4 and sys.argv[1] == "compare":
        rows, bad = compare(sys.argv[2], int(sys.argv[3]))
        for line in bad:
            print("FAIL " + line)
        print("compare N=%s rows=%d %s (%d failures)"
              % (sys.argv[3], rows, "PASS" if not bad else "FAIL", len(bad)))
        sys.exit(0 if not bad else 1)
    if len(sys.argv) >= 2 and sys.argv[1] == "selftest":
        bad = selftest()
        for line in bad:
            print("FAIL " + line)
        print("selftest %s (%d failures)" % ("PASS" if not bad else "FAIL", len(bad)))
        sys.exit(0 if not bad else 1)
    n = int(sys.argv[2]) if len(sys.argv) >= 3 else 8
    for line in table(n):
        print(line)
