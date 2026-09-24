#!/usr/bin/env python
"""Build native_lighting's four SYNTHETIC sheet caches, every run, from the
cache the exe just filled from the real container.

  python native_lighting_fixtures.py <own> <flat> <tilt> <tiltw> <scram> <container> <manifest>

WHY THIS EXISTS (lane GATEFIX1, 2026-09-19).  The three synthetic caches used to
be artefacts on disk, written once by lane NATIVEVIEW2 on 2026-09-12
(scratchpad/nativeview2_20260912/work/make_tiltcache.py, make_tiltw.py, and the
flat one inline in shots.sh).  src/lodtsheets.cpp:501-506 reuses a cached tile
only when it is NEWER than the .lodt container it came out of -- a correct rule,
added 2026-09-18 -- and the container was re-baked on 2026-09-16 16:44.  So the
first gate run after that rule landed (2026-09-18 09:12:47..09:13:19, one arm
every ten seconds) found all four caches stale and refilled every one of them
from the SAME container.  The four "different" fixtures became one file four
times over: `Commonwealth.VT.2.0.4.n.DDS` read sha1 ad7f8084e3fb3653 in all four
caches, and the four renders came out byte-identical (b39e407cc0a7 at the
oblique).  Gate (b) and gate (d) were then comparing a picture with itself.

A fixture that the thing under test is allowed to overwrite is not a fixture.
So the gate now BUILDS them, after the `own` arm has refreshed the own cache,
and their mtimes are therefore newer than the container by construction.

WHAT IT WRITES.  Each synthetic cache is a copy of the own cache with every BC1
block of every `*.n.DDS` replaced by one constant colour and zero indices -- the
`.c.DDS` colour tiles are copied through untouched, which is the control that
only the normal sheet moved.  The constants, in the sheet's own channel order
(R east, G up, B north), are NATIVEVIEW2's:

    flat   (128, 255, 128)   straight up
    tilt   (191, 238, 128)   28.94 deg towards EAST
    tiltw  ( 64, 238, 128)   28.94 deg towards WEST, the exact mirror

They are read back through the 5/6/5 quantisation and printed, because the
read-back value -- not the value asked for -- is the one the gate's arithmetic
uses.

THE FOURTH ONE IS THE TWIN (lane GATEFIX2, 2026-09-19).  `scram` is the own
cache with the 8-byte BC1 blocks of every `*.n.DDS` PERMUTED within their tile,
seed 20260919 -- the same endpoint words, the same index bytes, the same value
histogram, the same codec, the right normals in the wrong places.  It is the
floor for gate (b)'s claim that a real slope signal survives block averaging
while an uncorrelated one does not (ww-control-calibration part 4: a floor built
from the subject's own data, carrying the signal's own amplitude and spectrum
through the same lossy pipeline).  Measured 2026-09-19: the twin's tilt-from-
vertical distribution is identical to own's to every printed decimal (mean 20.83
deg, median 19.53, p90 36.11), and the block-averaged difference it makes reads
1.00 / 1.07 / 1.04 over three seeds against own's 2.28.

The manifest it writes is read by native_lighting_check.py, which turns it into
the pairwise-distinctness rows.  Exit is non-zero when the four normal sheets
are not pairwise distinct, or when the colour sheets are not identical, so the
collapse of 2026-09-18 cannot recur silently.
"""
import hashlib
import math
import os
import shutil
import struct
import sys

import numpy as np

# (R east, G up, B north), NATIVEVIEW2's three known-answer normals.
WANT = {
    "flat": (128, 255, 128),
    "tilt": (191, 238, 128),
    "tiltw": (64, 238, 128),
}

ARMS = ("own", "flat", "tilt", "tiltw", "scram")
SCRAM_SEED = 20260919


def to565(r, g, b):
    return ((round(r / 255.0 * 31) << 11) | (round(g / 255.0 * 63) << 5)
            | round(b / 255.0 * 31))


def from565(c):
    r = (c >> 11) & 31
    g = (c >> 5) & 63
    bl = c & 31
    return ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (bl * 255 + 15) // 31)


def sha1(p):
    return hashlib.sha1(open(p, "rb").read()).hexdigest()


def rewrite(dst, want):
    """Every BC1 block of every *.n.DDS under dst -> one constant colour."""
    c = to565(*want)
    blk = struct.pack("<HHI", c, c, 0)
    n = 0
    for root, _, files in os.walk(dst):
        for f in files:
            if not f.lower().endswith(".n.dds"):
                continue
            p = os.path.join(root, f)
            b = bytearray(open(p, "rb").read())
            h = struct.unpack_from("<7I", b, 4)
            w, hh, mips = h[3], h[2], max(1, h[6])
            off = 148 if bytes(b[84:88]) == b"DX10" else 128
            if mips != 1:
                raise SystemExit("%s carries %d mips; this rewriter handles mip 0 only"
                                 % (p, mips))
            nb = ((w + 3) // 4) * ((hh + 3) // 4)
            if off + nb * 8 != len(b):
                raise SystemExit("%s is %d B, not the %d B a %dx%d BC1 mip-0 tile is "
                                 "-- the sheet format moved under this fixture builder"
                                 % (p, len(b), off + nb * 8, w, hh))
            b[off:] = blk * nb
            open(p, "wb").write(bytes(b))
            n += 1
    return n, c, from565(c)


def shuffle(dst, seed):
    """Permute the BC1 blocks of every *.n.DDS under dst, within each tile."""
    rng = np.random.default_rng(seed)
    n = 0
    for root, _, files in os.walk(dst):
        for f in sorted(files):
            if not f.lower().endswith(".n.dds"):
                continue
            p = os.path.join(root, f)
            b = bytearray(open(p, "rb").read())
            off = 148 if bytes(b[84:88]) == b"DX10" else 128
            nb = (len(b) - off) // 8
            if off + nb * 8 != len(b):
                raise SystemExit("%s is not a whole number of BC1 blocks past its "
                                 "header -- the sheet format moved under the twin" % p)
            blocks = np.frombuffer(bytes(b[off:]), dtype=np.uint8).reshape(nb, 8)
            b[off:] = bytes(blocks[rng.permutation(nb)].tobytes())
            open(p, "wb").write(bytes(b))
            n += 1
    return n


def main():
    if len(sys.argv) != 8:
        print(__doc__)
        return 2
    own, flat, tilt, tiltw, scram, container, manifest = sys.argv[1:8]
    dst = {"flat": flat, "tilt": tilt, "tiltw": tiltw}

    lines = []

    def say(s):
        print(s)
        lines.append(s)

    if not os.path.isdir(own):
        say("FIXTURES: the own cache %s does not exist" % own)
        return 1

    # The own cache is only a usable SOURCE when the exe has already refreshed it
    # from the container; src/lodtsheets.cpp reuses a tile only when it is newer
    # than the container, so that is exactly the test.
    cmt = os.path.getmtime(container)
    stale = []
    for root, _, files in os.walk(own):
        for f in files:
            if f.lower().endswith((".n.dds", ".c.dds")) and os.path.getmtime(os.path.join(root, f)) <= cmt:
                stale.append(os.path.join(root, f))
    say("FIXTURES: container %s mtime %.0f; own-cache tiles older than it: %d"
        % (os.path.basename(container), cmt, len(stale)))
    if stale:
        say("FIXTURES: the own cache was not refreshed before this step -- %s"
            % stale[0])
        return 1

    for k in ("flat", "tilt", "tiltw"):
        if os.path.isdir(dst[k]):
            shutil.rmtree(dst[k])
        shutil.copytree(own, dst[k])
        n, word, got = rewrite(dst[k], WANT[k])
        e = got[0] / 255.0 * 2 - 1
        u = got[1] / 255.0 * 2 - 1
        nn = got[2] / 255.0 * 2 - 1
        L = math.sqrt(e * e + u * u + nn * nn)
        say("FIXTURES: %-5s %d normal tiles <- RGB %s, 565 word 0x%04X, read back %s; "
            "unit normal east %+.4f north %+.4f up %.4f (%.2f deg from vertical)"
            % (k, n, WANT[k], word, got, e / L, nn / L, u / L,
               math.degrees(math.acos(u / L))))

    if os.path.isdir(scram):
        shutil.rmtree(scram)
    shutil.copytree(own, scram)
    say("FIXTURES: scram %d normal tiles <- the own tiles' own BC1 blocks, permuted "
        "within each tile, seed %d (same words, same histogram, wrong places)"
        % (shuffle(scram, SCRAM_SEED), SCRAM_SEED))

    # The manifest the checker turns into rows: one line per cache per role, the
    # sha1 of the FIRST tile of that role in name order.
    caches = {"own": own, "flat": flat, "tilt": tilt, "tiltw": tiltw, "scram": scram}
    rows = []
    for k in ARMS:
        for role in ("n", "c"):
            fs = sorted(f for _, _, fl in os.walk(caches[k]) for f in fl
                        if f.lower().endswith("." + role + ".dds"))
            if not fs:
                say("FIXTURES: %s carries no *.%s.DDS" % (k, role))
                return 1
            first = fs[0]
            p = [os.path.join(r, first) for r, _, fl in os.walk(caches[k]) if first in fl][0]
            rows.append((k, role, first, sha1(p), len(fs)))
    with open(manifest, "w", encoding="utf-8") as fh:
        for r in rows:
            fh.write("%s %s %s %s %d\n" % r)
        for l in lines:
            fh.write("# " + l + "\n")

    nsh = {k: s for k, role, _, s, _ in rows if role == "n"}
    csh = {k: s for k, role, _, s, _ in rows if role == "c"}
    say("FIXTURES: normal-sheet sha1 " + "  ".join("%s=%s" % (k, v[:12]) for k, v in nsh.items()))
    say("FIXTURES: colour-sheet sha1 " + "  ".join("%s=%s" % (k, v[:12]) for k, v in csh.items()))
    bad = 0
    if len(set(nsh.values())) != len(ARMS):
        say("FIXTURES: the %d normal sheets are NOT pairwise distinct -- "
            "the collapse of 2026-09-18 has recurred" % len(ARMS))
        bad = 1
    if len(set(csh.values())) != 1:
        say("FIXTURES: the colour sheets are not identical across the four caches, "
            "so more than the normal sheet moved")
        bad = 1
    return bad


if __name__ == "__main__":
    sys.exit(main())
