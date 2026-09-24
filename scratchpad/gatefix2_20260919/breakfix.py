#!/usr/bin/env python
"""Build the BROKEN-state sheet caches for GATEFIX2's calibration.

  python breakfix.py <own cache> <outdir>

Three arms, each a copy of the own cache with only the *.n.DDS tiles touched
(the *.c.DDS colour tiles are copied through -- the control that one variable
moved):

  ign    an exact copy of the own cache.  Renders a frame identical to the own
         arm, which is what a shader that does not read the normal sheet at all
         would make of ANY sheet.  The pair (own, ign) is therefore the gate's
         own-vs-flat comparison as a non-reading shader would return it -- and
         it doubles as a determinism check on the renderer.

  ts     every texel's UP and NORTH channels swapped.  This is the historical
         defect in fixture form: fo4_default.frag read the model-space sheet as
         a TANGENT-space one and sent its "up" along an arbitrary bitangent, so
         the sheet's dominant up component left along a horizontal axis.  The
         endpoints are re-encoded in 565 and the block MODE is preserved -- when
         the swap reverses c0 > c1 the endpoints are exchanged and the indices
         remapped (0<->1, 2<->3), so the decoded palette is the same set.

  scram  the 8-byte BC1 blocks permuted within each tile, seed 20260919.  Same
         endpoint words, same index bytes, same value histogram, same codec --
         only the POSITIONS are destroyed.  This is the phase-randomised twin
         that ww-control-calibration asks for: it is the floor for any claim of
         the form "a real slope signal survives block averaging, uncorrelated
         noise does not".

Lane GATEFIX2, 2026-09-19.
"""
import hashlib
import os
import shutil
import struct
import sys

import numpy as np

SEED = 20260919


def tiles(d):
    return sorted(os.path.join(r, f) for r, _, fl in os.walk(d) for f in fl
                  if f.lower().endswith(".n.dds"))


def split(p):
    b = bytearray(open(p, "rb").read())
    off = 148 if bytes(b[84:88]) == b"DX10" else 128
    return b, off


def to565(r, g, bl):
    return ((int(round(r / 255.0 * 31)) << 11) | (int(round(g / 255.0 * 63)) << 5)
            | int(round(bl / 255.0 * 31)))


def from565(c):
    return (((c >> 11) & 31) * 255.0 / 31.0, ((c >> 5) & 63) * 255.0 / 63.0,
            (c & 31) * 255.0 / 31.0)


def swap_ub(p):
    """UP (G) <-> NORTH (B) on both endpoints of every block, mode preserved."""
    b, off = split(p)
    n = (len(b) - off) // 8
    for k in range(n):
        o = off + k * 8
        c0, c1, idx = struct.unpack_from("<HHI", b, o)
        r0, g0, b0 = from565(c0)
        r1, g1, b1 = from565(c1)
        n0, n1 = to565(r0, b0, g0), to565(r1, b1, g1)
        if (c0 > c1) != (n0 > n1):
            # keep the 4-colour / 3-colour mode: exchange the endpoints and
            # remap the indices so the palette entry each texel points at is
            # the same colour it pointed at before.
            m = 0
            for t in range(16):
                s = (idx >> (2 * t)) & 3
                s = {0: 1, 1: 0, 2: 3, 3: 2}[s]
                m |= s << (2 * t)
            idx = m
            n0, n1 = n1, n0
        if n0 == n1 and c0 != c1:
            n1 = max(0, n1 - 1) if n0 > 0 else n1 + 1
        struct.pack_into("<HHI", b, o, n0, n1, idx)
    open(p, "wb").write(bytes(b))
    return n


def scramble(p, rng):
    b, off = split(p)
    n = (len(b) - off) // 8
    blocks = np.frombuffer(bytes(b[off:]), dtype=np.uint8).reshape(n, 8)
    b[off:] = bytes(blocks[rng.permutation(n)].tobytes())
    open(p, "wb").write(bytes(b))
    return n


def sha1(p):
    return hashlib.sha1(open(p, "rb").read()).hexdigest()


def main():
    own, out = sys.argv[1], sys.argv[2]
    rng = np.random.default_rng(SEED)
    made = {}
    for arm in ("ign", "ts", "scram"):
        d = os.path.join(out, arm + "cache")
        if os.path.isdir(d):
            shutil.rmtree(d)
        shutil.copytree(own, d)
        if arm == "ts":
            for p in tiles(d):
                swap_ub(p)
        elif arm == "scram":
            for p in tiles(d):
                scramble(p, rng)
        made[arm] = d
        h = [sha1(p)[:12] for p in tiles(d)]
        print("BROKEN %-5s %d normal tiles -> %s" % (arm, len(h), " ".join(h)))
    h = [sha1(p)[:12] for p in tiles(own)]
    print("GOOD   own   %d normal tiles -> %s" % (len(h), " ".join(h)))
    for arm in ("ign", "ts", "scram"):
        c = [sha1(p)[:12] for _, _, fl in os.walk(made[arm]) for p in
             sorted(os.path.join(r, f) for r, _, fl2 in os.walk(made[arm]) for f in fl2
                    if f.lower().endswith(".c.dds"))][:4]
        break
    co = sorted(os.path.join(r, f) for r, _, fl in os.walk(own) for f in fl
                if f.lower().endswith(".c.dds"))
    for arm in ("ign", "ts", "scram"):
        ca = sorted(os.path.join(r, f) for r, _, fl in os.walk(made[arm]) for f in fl
                    if f.lower().endswith(".c.dds"))
        same = all(sha1(a) == sha1(b) for a, b in zip(co, ca))
        print("CONTROL %-5s colour sheets identical to own: %s" % (arm, same))
        if not same:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
