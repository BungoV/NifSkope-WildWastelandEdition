#!/usr/bin/env python
# ---------------------------------------------------------------------------
# Read a WW_IMPOSTOR_PREVIEW=pair log and answer ONE question per set:
#
#   do the three frames this set chose, addressed ON THIS SET'S OWN GRID,
#   reconstruct the direction the camera was actually pointing?
#
# The blend the spec describes is barycentric over the (N-1)^2 triangle mesh
# whose vertices are the frame directions, so the weighted sum of the three
# chosen frame directions must point back at the lookup direction to within
# about half a grid cell -- and half a cell is a number that DEPENDS ON N,
# which is the whole point of the row this feeds. A set read on somebody
# else's grid still yields three frames and three weights that sum to one; it
# just points somewhere else. That is what the error measures.
#
# The reconstruction uses tests/spells/impostor_oct_ref.py -- the Python
# reference the C++ mapping is already gated against -- and never a second copy
# of the arithmetic written here.
#
#   python impostor_pair_check.py <pair log>
#
# prints, and exits 1 if it cannot parse:
#   set A oct 4 mean 3.91 worst 7.72 cellhalf 24.09
#   set B oct 12 mean 1.02 worst 2.55 cellhalf 7.51
# ---------------------------------------------------------------------------
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import impostor_oct_ref as ref


def norm(v):
    n = math.sqrt(sum(c * c for c in v))
    return [c / n for c in v] if n > 1e-9 else [0.0, 0.0, 0.0]


def angle_between(a, b):
    a, b = norm(a), norm(b)
    d = max(-1.0, min(1.0, sum(x * y for x, y in zip(a, b))))
    return math.degrees(math.acos(d))


def parse(path):
    """-> ({letter: [ (lookdir, n, [(i,j,w)x3]) ]}, true_oct_of_B_or_None)

    THE RED CONTROL IS NOT MEASURED ON ITS OWN TERMS. When the harness was told
    to read set B on somebody else's grid it says so, in one line, naming both
    numbers. The frames it then chooses are addressed at cells of the sheet that
    was baked at B's TRUE N, and the direction actually fetched from cell (i,j)
    is frame_dir(i, j, TRUE N) -- not frame_dir(i, j, the grid the selection
    used). Reconstructing on the forced grid would reproduce the selection's own
    mistake and score it perfect, which is how a red control is made unable to
    fail.
    """
    rx_look = re.compile(r"^\s*([AB]) lookdir (\S+) (\S+) (\S+)")
    rx_cell = re.compile(r"^\s*([AB]) cell \S+ \S+ of (\d+)")
    rx_frame = re.compile(r"^\s*([AB]) frame \d+ index \d+ \(i (\d+) j (\d+)\) weight (\S+)")
    rx_forced = re.compile(r"^FORCED N: set B's grid is (\d+) but is being read as (\d+)")
    out = {"A": [], "B": []}
    cur = {"A": None, "B": None}
    true_b = None
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            m = rx_forced.match(line)
            if m:
                true_b = int(m.group(1))
                continue
            m = rx_look.match(line)
            if m:
                L = m.group(1)
                cur[L] = [[float(m.group(2)), float(m.group(3)), float(m.group(4))], 0, []]
                continue
            m = rx_cell.match(line)
            if m and cur[m.group(1)]:
                cur[m.group(1)][1] = int(m.group(2))
                continue
            m = rx_frame.match(line)
            if m and cur[m.group(1)]:
                L = m.group(1)
                cur[L][2].append((int(m.group(2)), int(m.group(3)), float(m.group(4))))
                if len(cur[L][2]) == 3:
                    out[L].append(tuple(cur[L]))
                    cur[L] = None
    return out, true_b


def score(entries, true_grid=None):
    """mean and worst reconstruction error in degrees, and the half-cell scale."""
    errs = []
    n = 0
    for look, grid, frames in entries:
        n = true_grid if true_grid else grid
        acc = [0.0, 0.0, 0.0]
        for i, j, w in frames:
            d = ref.frame_dir(i, j, n)
            if d is None:
                return None
            for k in range(3):
                acc[k] += w * d[k]
        errs.append(angle_between(acc, look))
    if not errs:
        return None
    # Half a cell, as an angle: the grid spans a hemisphere (2*pi sr) over
    # (n-1) steps per axis, so one step subtends roughly 90/(n-1) degrees and
    # a point can sit half a step from the nearest vertex in each axis.
    cellhalf = 90.0 / max(1, (n - 1))
    return n, sum(errs) / len(errs), max(errs), cellhalf, len(errs)


def main():
    if len(sys.argv) < 2:
        print("usage: impostor_pair_check.py <pair log>")
        return 1
    got, true_b = parse(sys.argv[1])
    rc = 0
    if true_b:
        print("forced-n: set B was baked at %d and read on another grid" % true_b)
    for letter in ("A", "B"):
        s = score(got[letter], true_b if letter == "B" else None)
        if not s:
            print("set %s NO DATA -- the log has no '%s lookdir/cell/frame' triples"
                  % (letter, letter))
            rc = 1
            continue
        n, mean, worst, cellhalf, count = s
        print("set %s oct %d mean %.3f worst %.3f cellhalf %.3f views %d"
              % (letter, n, mean, worst, cellhalf, count))
    return rc


if __name__ == "__main__":
    sys.exit(main())
