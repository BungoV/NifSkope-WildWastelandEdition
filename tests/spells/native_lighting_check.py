#!/usr/bin/env python
"""The arithmetic behind tests/spells/native_lighting.sh.

  python native_lighting_check.py <shotdir> <baselinedir> [<fixture manifest> [<own cache>]]

Writes one `  ok  ` / `  FAIL` line per check, then the count line, then
PASS/FAIL and `done`, the way a WW harness log reads.  Every floor carries the
number it was measured at and the number the RUNG (the exe before the
model-space path existed) gave, so a reader can see that the floor can fire.
"""
import hashlib
import math
import os
import struct
import sys

import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], np.int16)
DARK = 40.0
BLK = 32
BORDER = 8          # the sheet tiles store 256 content texels inside a border of 8
TWIN = "scram"      # the phase-randomised twin arm the fixture builder writes

checks = 0
fails = 0
skips = 0
OUT = []


def say(s):
    OUT.append(s)


def check(ok, text):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
    say(("  ok   " if ok else "  FAIL ") + text)


def skip(text):
    global skips
    skips += 1
    say("  SKIP " + text)


def load(d, n):
    p = os.path.join(d, n + ".png")
    a = np.array(Image.open(p).convert("RGB")).astype(np.int16)
    cov = (np.abs(a - BG).sum(axis=2) > 0)
    Y = (0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]).astype(np.float64)
    return cov, Y


def blocks(Y, cov):
    h, w = Y.shape
    bh, bw = h // BLK, w // BLK
    ys = Y[:bh * BLK, :bw * BLK].reshape(bh, BLK, bw, BLK)
    cs = cov[:bh * BLK, :bw * BLK].reshape(bh, BLK, bw, BLK)
    full = cs.all(axis=(1, 3))
    return ys.mean(axis=(1, 3))[full], int(full.sum())


def darkpc(d, n):
    cov, Y = load(d, n)
    return 100.0 * (Y[cov] < DARK).mean()


def sha1(p):
    return hashlib.sha1(open(p, "rb").read()).hexdigest()


def bc1(p):
    """Decode one BC1 mip-0 DDS tile to float RGB.  Lane GATEFIX2, 2026-09-19."""
    b = open(p, "rb").read()
    h = struct.unpack_from("<7I", b, 4)
    hh, w, mips = h[2], h[3], max(1, h[6])
    off = 148 if b[84:88] == b"DX10" else 128
    bw, bh = (w + 3) // 4, (hh + 3) // 4
    if mips != 1 or off + bw * bh * 8 != len(b):
        raise SystemExit("%s: %d B / %d mips is not a %dx%d BC1 mip-0 tile" %
                         (p, len(b), mips, w, hh))
    d = np.frombuffer(b, dtype=np.uint8, offset=off).reshape(bh, bw, 8)
    c0 = d[..., 0].astype(np.uint16) | (d[..., 1].astype(np.uint16) << 8)
    c1 = d[..., 2].astype(np.uint16) | (d[..., 3].astype(np.uint16) << 8)
    idx = (d[..., 4].astype(np.uint32) | (d[..., 5].astype(np.uint32) << 8)
           | (d[..., 6].astype(np.uint32) << 16) | (d[..., 7].astype(np.uint32) << 24))

    def rgb(c):
        return np.stack([((c >> 11) & 31) * 255.0 / 31.0,
                         ((c >> 5) & 63) * 255.0 / 63.0,
                         (c & 31) * 255.0 / 31.0], -1)

    e0, e1 = rgb(c0), rgb(c1)
    four = (c0 > c1)[..., None]
    pal = np.empty(e0.shape[:2] + (4, 3))
    pal[..., 0, :], pal[..., 1, :] = e0, e1
    pal[..., 2, :] = np.where(four, (2 * e0 + e1) / 3.0, (e0 + e1) / 2.0)
    pal[..., 3, :] = np.where(four, (e0 + 2 * e1) / 3.0, 0.0)
    out = np.empty((bh * 4, bw * 4, 3))
    for j in range(4):
        for i in range(4):
            s = ((idx >> (2 * (4 * j + i))) & 3)[..., None, None].repeat(3, -1)
            out[j::4, i::4, :] = np.take_along_axis(pal, s, axis=2)[:, :, 0, :]
    return out


def sheet_up(cache):
    """Mean clamped UP component of the container's own normals, over the CONTENT
    texels of every *.n.DDS in the cache the exe just filled.  The sheet's channel
    order is R east, G up, B north (measured by NATIVEVIEW2 on six vanilla sheets)."""
    ps = sorted(os.path.join(r, f) for r, _, fl in os.walk(cache) for f in fl
                if f.lower().endswith(".n.dds"))
    if not ps:
        return None, 0
    ups = []
    for p in ps:
        a = bc1(p)
        e = a[..., 0] / 255.0 * 2 - 1
        u = a[..., 1] / 255.0 * 2 - 1
        n = a[..., 2] / 255.0 * 2 - 1
        L = np.sqrt(e * e + u * u + n * n)
        L[L == 0] = 1.0
        ups.append(np.clip((u / L)[BORDER:-BORDER, BORDER:-BORDER], 0, None).ravel())
    v = np.concatenate(ups)
    return float(v.mean()), len(ps)


def main():
    shot, base = sys.argv[1], sys.argv[2]
    manifest = sys.argv[3] if len(sys.argv) > 3 else ""
    owncache = sys.argv[4] if len(sys.argv) > 4 else ""

    need = ["t_own_obl", "t_own_top", "t_flat_obl", "t_flat_top", "t_tilt_obl",
            "t_tilt_top", "t_tiltw_obl", "t_tiltw_top", "legacy_bto_top",
            "legacy_bto_obl", "legacy_btr_top", "legacy_btr_obl"]
    missing = [n for n in need if not os.path.exists(os.path.join(shot, n + ".png"))]
    if missing:
        say("  the render pass did not produce: " + " ".join(missing))

    # ---- (e) THE FIXTURES ARE FOUR FIXTURES, not one photograph four times.
    #
    # Lane GATEFIX1, 2026-09-19.  On 2026-09-18 the exe legitimately refilled all
    # four sheet caches from the same .lodt container (src/lodtsheets.cpp:501-506
    # reuses a tile only when it is newer than the container, and the container
    # had been re-baked on 2026-09-16 16:44).  The three synthetic caches of
    # 2026-09-12 were overwritten, `Commonwealth.VT.2.0.4.n.DDS` read sha1
    # ad7f8084e3fb3653 in all four, and t_own/flat/tilt/tiltw_obl.png all came out
    # b39e407cc0a7.  Gate (b) and gate (d) were then comparing a picture with
    # itself, and reported IoU 1.000 / blockSD 0.00 / ordering 0.00 % for it.
    #
    # Nothing downstream is worth reading unless these rows are green, so they
    # come first: the INPUT sheets differ, the colour sheets do NOT (the control
    # that only the normal sheet moved), and the OUTPUT pictures differ.
    ARMS = ("own", "flat", "tilt", "tiltw")
    if not manifest or not os.path.exists(manifest):
        check(False, "gate (e): the fixture manifest %s exists, so the four sheet "
                     "caches can be proved distinct" % (manifest or "<not passed>"))
    else:
        nall, call = {}, {}
        for l in open(manifest, encoding="utf-8"):
            if l.startswith("#"):
                continue
            f = l.split()
            if len(f) >= 4:
                (nall if f[1] == "n" else call)[f[0]] = f[3]
        # The twin (gate (f)) is a fifth arm and is judged by its own rows, so
        # gate (e) keeps reading exactly the four it was written for.
        nsh = {k: v for k, v in nall.items() if k in ARMS}
        csh = {k: v for k, v in call.items() if k in ARMS}
        say("  fixture normal sheets: " + "  ".join("%s=%s" % (k, nsh.get(k, "-")[:12]) for k in ARMS))
        say("  fixture colour sheets: " + "  ".join("%s=%s" % (k, csh.get(k, "-")[:12]) for k in ARMS))
        check(len(nsh) == 4 and len(set(nsh.values())) == 4,
              "gate (e): the four arms' normal sheets are pairwise distinct (%d distinct "
              "sha1 of %d; 1 of 4 on 2026-09-18, which is the collapse this row exists "
              "to catch)" % (len(set(nsh.values())), len(nsh)))
        check(len(csh) == 4 and len(set(csh.values())) == 1,
              "gate (e) (control): the four arms' COLOUR sheets are identical (%d distinct "
              "sha1 of %d), so the fixture builder moved the normal sheet and nothing else"
              % (len(set(csh.values())), len(csh)))
        # ---- (f) THE TWIN IS A TWIN: the own tiles' own blocks, in the wrong places.
        say("  twin sheet: %s normal=%s colour=%s (own normal=%s)"
            % (TWIN, nall.get(TWIN, "-")[:12], call.get(TWIN, "-")[:12],
               nall.get("own", "-")[:12]))
        check(TWIN in nall and nall[TWIN] != nall.get("own")
              and TWIN in call and call[TWIN] == call.get("own"),
              "gate (f): the twin's normal sheet differs from own's and its COLOUR sheet "
              "is own's own, so the shuffle moved the positions and nothing else")

    obl = {a: os.path.join(shot, "t_%s_obl.png" % a) for a in ARMS}
    if all(os.path.exists(p) for p in obl.values()):
        h = {a: sha1(p) for a, p in obl.items()}
        say("  oblique frames: " + "  ".join("%s=%s" % (a, h[a][:12]) for a in ARMS))
        check(len(set(h.values())) == 4,
              "gate (e): the four OBLIQUE frames are pairwise distinct (%d distinct sha1 "
              "of 4; all four were b39e407cc0a7 on 2026-09-18)" % len(set(h.values())))
        # A hash says "not identical"; this says by HOW MUCH, on the weakest pair.
        # Pre-registered floor 2.00 luma levels, from the gate's own algebra and
        # not from a measurement: the closest pair the arithmetic predicts is
        # west vs flat, N.L 0.7258 vs 0.4434, a 0.28 gap of full scale.  Two 8-bit
        # levels is far below any real ordering and far above encoder noise.
        L = {a: load(shot, "t_%s_obl" % a) for a in ARMS}
        worst, wpair = 1e9, ""
        for x in range(4):
            for y in range(x + 1, 4):
                a, b = ARMS[x], ARMS[y]
                mm = L[a][0] & L[b][0]
                d = float(np.abs(L[a][1] - L[b][1])[mm].mean()) if mm.any() else 0.0
                say("    mean|dY| %-5s vs %-5s  %6.2f over %d px" % (a, b, d, int(mm.sum())))
                if d < worst:
                    worst, wpair = d, "%s vs %s" % (a, b)
        check(worst >= 2.00,
              "gate (e): the weakest oblique pair (%s) still differs by mean|dY| %.2f luma "
              "levels, floor 2.00 pre-registered (0.00 on 2026-09-18)" % (wpair, worst))

    # gate (d)'s top-view row asks for two frames to be THE SAME.  That row was
    # green on 2026-09-18 only because every frame was the same, so it carries
    # its own floor here: two arms whose top-view N.L provably DIFFERS (flat
    # 0.9988 up against tilt 0.8751) must not be the same picture.
    if not ({"t_flat_top", "t_tilt_top"} & set(missing)):
        pf = os.path.join(shot, "t_flat_top.png")
        pt = os.path.join(shot, "t_tilt_top.png")
        cf2, Yf2 = load(shot, "t_flat_top")
        ct2, Yt2 = load(shot, "t_tilt_top")
        mm = cf2 & ct2
        dd = float(np.abs(Yf2 - Yt2)[mm].mean()) if mm.any() else 0.0
        check(sha1(pf) != sha1(pt) and dd > 0.0,
              "gate (d) (floor): at the TOP view flat (N.L 0.9988) and an east tilt "
              "(0.8751) are NOT the same picture (mean|dY| %.2f), so the row below that "
              "asks east == west at the top is a row that can fail" % dd)

    # ---- (a) a shape the model-space path must not touch renders byte-identical
    for n in ("legacy_bto_top", "legacy_bto_obl", "legacy_btr_top", "legacy_btr_obl"):
        b = os.path.join(base, n + ".png")
        s = os.path.join(shot, n + ".png")
        if not os.path.exists(b):
            skip("no baseline for %s (gate (a) not exercised for it)" % n)
            continue
        if not os.path.exists(s):
            check(False, "gate (a): %s was not rendered" % n)
            continue
        check(open(b, "rb").read() == open(s, "rb").read(),
              "gate (a): %s is byte-identical to its baseline" % n)

    # ---- (c) the dark fraction of the native oblique terrain
    if "t_own_obl" not in missing:
        v = darkpc(shot, "t_own_obl")
        check(v <= 3.0, "gate (c): native oblique terrain dark<40 is %.2f%%, "
                        "bar 3.00%% (measured 0.58 here, 20.01 on the rung)" % v)
    if "t_flat_obl" not in missing:
        v = darkpc(shot, "t_flat_obl")
        check(v <= 1.0, "gate (c): FLAT-tile oblique terrain dark<40 is %.2f%%, "
                        "bar 1.00%% (measured 0.00 here, 20.07 on the rung)" % v)

    # ---- (b) own tiles and flat tiles must now differ, and flat must be even
    if not ({"t_own_obl", "t_flat_obl", "t_flat_top"} & set(missing)):
        c1, Y1 = load(shot, "t_own_obl")
        c2, Y2 = load(shot, "t_flat_obl")
        m = c1 & c2
        # THE DARKEST-FIFTH IoU ROW WAS RETIRED HERE, lane GATEFIX2, 2026-09-19,
        # and it was not retired for being red.  It was measured against the
        # defects it exists to catch, on this container, with the four arms
        # rendered from caches that differ only in their normal tiles:
        #
        #     the sheet is not read at all (own against a copy of own)   1.000
        #     the sheet's UP and NORTH channels are swapped              0.887
        #     THE HEALTHY STATE (own against flat)                       0.850
        #     the right normals in the wrong places (the twin)     0.821 / 0.822 / 0.821
        #
        # The healthy value sits INSIDE the broken population, so no threshold
        # of the form "IoU <= bar" admits the healthy state and rejects all
        # three defects; the old bar of 0.800 rejects all four.  A statistic
        # that cannot be thresholded is not tuned, it is replaced (the brief's
        # own rule), and the job this row was written for -- own and flat must
        # not be the same picture -- is already done with a pre-registered
        # floor by gate (e)'s weakest-pair row (mean |dY| 3.10, floor 2.00).
        # Its replacements are the two rows below and gate (g).
        D = Y1 - Y2
        dbm, dnb = blocks(D, m)
        own_sd = float(dbm.std())
        # The absolute floor exists ONLY so that a total collapse -- every arm
        # rendering the same frame, which reads 0.00 -- cannot satisfy the ratio
        # row below as 0/0.  It is deliberately far under the healthy reading.
        check(own_sd >= 0.50,
              "gate (b): own-minus-flat blockSD is %.2f over %d blocks, floor 0.50 "
              "(2.28 on 2026-09-19; 0.00 when the sheet is not read at all).  The "
              "old floor of 3.50 was pinned on 2026-09-16 11:54:47 against a .lodt "
              "container that was re-baked at 16:44:04 the same day and no longer "
              "exists; a bar that only one generation of an untracked artefact can "
              "clear is not a bar" % (own_sd, dnb))
        # THE RATIO IS THE REAL ROW.  The twin carries the container's own normals
        # -- the same BC1 blocks, the same histogram, the same codec -- shuffled
        # into the wrong places, so it answers the question this row has always
        # asked ("does a REAL slope signal survive block averaging") on the
        # subject's own data, in the same run, whatever container is on disk.
        if "t_%s_obl" % TWIN in missing:
            check(False, "gate (f): the twin arm t_%s_obl was not rendered, so the "
                         "block-averaging row has no floor" % TWIN)
        else:
            c3, Y3 = load(shot, "t_%s_obl" % TWIN)
            mt = c3 & c2
            tbm, tnb = blocks(Y3 - Y2, mt)
            twin_sd = float(tbm.std())
            r = own_sd / twin_sd if twin_sd > 1e-9 else float("inf")
            check(r >= 1.60,
                  "gate (f): the container's own normals make %.2fx as much block-scale "
                  "shading as the SAME normals shuffled into the wrong places (own %.2f "
                  "vs twin %.2f over %d blocks), bar 1.60 pre-registered.  Measured "
                  "2026-09-19: 2.28 against 1.00 / 1.07 / 1.04 on three shuffle seeds, "
                  "i.e. 2.13x at the worst seed; 1.54x for a sheet whose UP and NORTH "
                  "channels are swapped" % (r, own_sd, twin_sd, tnb))
        fo, no = blocks(Y2, c2)
        ct, Yt = load(shot, "t_flat_top")
        ft, nt = blocks(Yt, ct)
        check(fo.std() <= ft.std(),
              "gate (b): flat tiles at the oblique have blockSD %.2f, no more than the "
              "%.2f the SAME tiles give at the top view where N.L is provably equal at "
              "every texel (measured 15.04 vs 17.57 here; 31.91 vs 6.14 on the rung)"
              % (fo.std(), ft.std()))

    # ---- (d) the slope test with a known answer
    rx, rz = math.radians(-63.5593), math.radians(133.3081)
    sx, cx, sz, cz = math.sin(rx), math.cos(rx), math.sin(rz), math.cos(rz)
    # Matrix::fromEuler's bottom row with y = 0 (src/data/niftypes.cpp:230-232):
    #   m[2][0] = sinX sinZ - cosX sinY cosZ,  m[2][1] = cosX sinY sinZ + sinX cosZ,
    #   m[2][2] = cosX cosY.  That row IS the view's +Z axis written in world axes,
    #   and the headlight points down it.
    Lw = (sx * sz, sx * cz, cx)
    N = {"flat": (0.0353, 0.0353, 0.9988),
         "tilt": (0.4827, 0.0353, 0.8751),
         "tiltw": (-0.4827, 0.0353, 0.8751)}
    dot = {k: sum(p * q2 for p, q2 in zip(v, Lw)) for k, v in N.items()}
    say("  the light: headlight (0,0,1) in VIEW space; the oblique view rotation "
        "(-63.5593, 0, 133.3081) through Matrix::fromEuler puts it along "
        "(%+.4f, %+.4f, %+.4f) in world axes" % Lw)
    say("  so N.L is  west %.4f  >  flat %.4f  >  east %.4f  at the oblique, and "
        "east == west (%.4f) at the top"
        % (dot["tiltw"], dot["flat"], dot["tilt"], N["tilt"][2]))

    if not ({"t_tilt_top", "t_tiltw_top"} & set(missing)):
        p1 = os.path.join(shot, "t_tilt_top.png")
        p2 = os.path.join(shot, "t_tiltw_top.png")
        c1, Y1 = load(shot, "t_tilt_top")
        c2, Y2 = load(shot, "t_tiltw_top")
        mm = c1 & c2
        dY = float(np.abs(Y1 - Y2)[mm].max())
        check(open(p1, "rb").read() == open(p2, "rb").read(),
              "gate (d): at the TOP view an east tilt and a west tilt have the same "
              "N.L, so the two frames are the same picture (max|dY| %.2f; the rung "
              "gave max 81.18, mean 47.06)" % dY)

    if not ({"t_tilt_obl", "t_tiltw_obl", "t_flat_obl"} & set(missing)):
        cw, Yw = load(shot, "t_tiltw_obl")
        cf, Yf = load(shot, "t_flat_obl")
        ce, Ye = load(shot, "t_tilt_obl")
        mm = cw & cf & ce
        pc = 100.0 * ((Yw > Yf) & (Yf > Ye))[mm].mean()
        check(pc >= 99.0,
              "gate (d): at the oblique, luma orders west > flat > east on %.2f%% of "
              "%d covered pixels, bar 99.00%% (measured 100.00 here, 46.23 on the "
              "rung -- chance)" % (pc, int(mm.sum())))

    # ---- (g) THE TOP VIEW AGAINST THE CONTAINER'S OWN NORMALS, lane GATEFIX2.
    #
    # At the top view the headlight points straight down the world's up axis, so
    # N.L IS the up component of the normal -- no camera algebra, no fitting.
    # Two arms in this very run have an exactly known up (flat 0.9988, tilt
    # 0.8751), and they calibrate luma against up on the same frames, the same
    # colour sheet and the same lighting the terrain is rendered with.  Read the
    # container's own mean up out of the sheet cache the exe just filled and the
    # frame's mean luma is PREDICTED before it is measured.
    #
    # This is the row that catches a sheet whose axes are wrong -- the channel
    # order in a normal sheet is measured, not declared, and getting it wrong is
    # a live defect class.  Measured 2026-09-19 on this container: the healthy
    # sheet lands 0.09 luma from its prediction, and the same sheet with UP and
    # NORTH swapped lands 31.03 away.  The linear model is calibrated over a
    # 0.1237 lever arm and is only trusted NEAR it, which is why the row is
    # one-sided: a small error is a pass, a large one is a failure, and the 23.99
    # is not itself a luma prediction, only a distance.
    if not ({"t_own_top", "t_flat_top", "t_tilt_top"} & set(missing)):
        cO, YO = load(shot, "t_own_top")
        cF, YF = load(shot, "t_flat_top")
        cT, YT = load(shot, "t_tilt_top")
        mt = cO & cF & cT
        Lo, Lf, Lt = float(YO[mt].mean()), float(YF[mt].mean()), float(YT[mt].mean())
        up, ntiles = sheet_up(owncache) if owncache else (None, 0)
        if up is None:
            check(False, "gate (g): the own sheet cache %s carries no *.n.DDS, so the "
                         "top view has nothing to be predicted from"
                         % (owncache or "<not passed>"))
        elif abs(Lf - Lt) < 0.10:
            check(False, "gate (g): the known-answer pair is degenerate (flat %.3f vs "
                         "tilt %.3f mean luma), so luma cannot be calibrated against up "
                         "in this run" % (Lf, Lt))
        else:
            slope = (Lf - Lt) / (N["flat"][2] - N["tilt"][2])
            pred = Lf - slope * (N["flat"][2] - up)
            err = abs(pred - Lo)
            say("  the top view: light straight down, so N.L is the normal's UP; the "
                "two known arms give %.2f luma per unit up (flat %.3f at up %.4f, tilt "
                "%.3f at up %.4f)" % (slope, Lf, N["flat"][2], Lt, N["tilt"][2]))
            check(err <= 3.00,
                  "gate (g): the container's own normals (mean up %.4f over the content "
                  "texels of %d sheet tiles) predict a top-view mean luma of %.3f and the "
                  "frame reads %.3f -- %.3f apart, bar 3.00 (0.09 on 2026-09-19; 31.03 "
                  "for the same sheet with UP and NORTH swapped)"
                  % (up, ntiles, pred, Lo, err))

    # ---- the census: which program lights what
    def census(n):
        p = os.path.join(shot, n + ".census.txt")
        return open(p, encoding="utf-8").read() if os.path.exists(p) else None

    c = census("t_own_obl")
    if c is None:
        skip("no WW_PROGRAM_CENSUS for t_own_obl")
    else:
        rows = [l for l in c.splitlines() if l.startswith("shape=")]
        ok = bool(rows) and all("msn=1" in l and "prog=fo4_default.prog" in l
                                for l in rows)
        check(ok, "census: all %d .lodl terrain shapes carry Shader Flags 1 bit 12 "
                  "and are lit by fo4_default.prog" % len(rows))
    c = census("legacy_bto_top")
    if c is None:
        skip("no WW_PROGRAM_CENSUS for legacy_bto_top")
    else:
        rows = [l for l in c.splitlines() if l.startswith("shape=")]
        check(bool(rows) and all("msn=0" in l for l in rows),
              "census: all %d .BTO object shapes are msn=0, so the model-space branch "
              "is switched off for them" % len(rows))
    c = census("legacy_btr_top")
    if c is None:
        skip("no WW_PROGRAM_CENSUS for legacy_btr_top")
    else:
        check("prog=sk_msn.prog" in c and "msn=1" in c and "lodland=1" in c,
              "census: the legacy .BTR 'Land' shape is msn=1, Shader Type 18, and is "
              "lit by sk_msn.prog -- NOT by the FO4 program this lane changed, which "
              "is why its pictures do not move")

    for l in OUT:
        print(l)
    print("%d checks, %d failures, %d skips" % (checks, fails, skips))
    if skips:
        print("--- skips (a SKIP is never a pass) ---")
        for l in OUT:
            if l.startswith("  SKIP"):
                print(l)
    print("PASS" if fails == 0 else "FAIL")
    print("done")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
