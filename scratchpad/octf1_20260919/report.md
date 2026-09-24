# OCTF1 -- gate lodgen_octahedral.sh row F1, red at 1.69 texels

Lane OCTF1, read-only, offline. Sat Sep 19 16:45-16:58 CEDT 2026.
No build, no exe run, no src/res/tests edit, no commit.

VERDICT: F1's EXPECTATION is wrong -- not the bake, not the instrument. F1 has
never been green, and none of today's repairs touched it.

## 1. What F1 measures and where its floor came from

tests/spells/lodgen_octahedral.sh, bake 4 (lines 925-1200). A 512-unit cube is
baked into an 8x8 octahedral sheet of 64x64 frames twice -- orthographic, and a
PERSPECTIVE CONTROL. Per frame the script predicts the silhouette's full span

    px = 2*half*(|r.x|+|r.y|+|r.z|)*tw / (2*fw)      (and py with `up`)

(the support function of a cube; `half` is the cube measured through the pinned
ortho camera, 256.2811 units; fw = fh = 456.40; tw = th = 64) and measures
mx = x1-x0+1 of the alpha mask's bounding box. The same measurement runs at two
alpha thresholds:

  * thr 16  -- the bake's coverage floor. Bar 2.0 texels. Line 1141. GREEN at 1.69.
  * thr 128 -- the reader's threshold. Bar 1.0 texel. Line 1158. F1. RED at 1.69.

Floor provenance, from the script's own comment (lines 1119-1127) and
scratchpad/lane_cardwidth_report.md section 8: lane CARDWIDTH, 2026-09-10,
pre-registered before the block was written. Its stated reason is not "the bake
is accurate to one texel" -- it is "before the coverage re-encoding a bare crown
lost up to 5.41 texels of half-width between those two sets; a cube is solid, so
this is the arithmetic half of that fixture and the bar is ONE texel".

## 2. When it went red: 2026-09-10, the day the row was written

scratchpad/lane_cardwidth_report.md section 15.2, that lane's own gate table:

    | F1 | cube within 1.0 texel at the reader's threshold | 1.78 | REFUSED |

and section 15.3: "F1 is refused at 1.78 texels against a pre-registered 1.0, and
the SAME instrument read 1.78 on the pre-fix build ... The bar was carried from
the brief and is a hypothesis about the instrument, not about the bake ... It is
left RED rather than moved."

The row was refused on the day it landed and deliberately left red. It has never
passed.

Today's evidence brackets it further. Three gate runs, three different exes:

    scratchpad/impostorshow_20260919/oct_gate.txt                 10:06   F1 FAIL 1.69
    scratchpad/impostorfix3_20260919/gate_lodgen_oct_before.txt   16:37 (exe af457755) F1 FAIL 1.69
    scratchpad/impostorfix3_20260919/gate_lodgen_oct.txt          16:35 (exe 220662f1) F1 FAIL 1.69

The 16:35 and 16:37 logs are BYTE-IDENTICAL (md5 0cfbd3e48b88e22d4cdfc044c69f6516),
and the 10:06 log differs from them by one line -- a trailing "exit 1". The 10:06
run predates every impostor repair of today. The cube's bake is bit-stable across
all three, including the whole 64-frame predicted/measured table.

None of the three candidates can move this number, and here is why each cannot:

  * the 180-degree azimuth repair (rz = 270 - azim, conv spec1) -- a cube is
    invariant under azim -> azim+180: r = (sin azim, -cos azim, 0) flips sign and
    its L1 norm, which is all px uses, is unchanged; the measured silhouette is
    likewise its own 180-degree rotation (the gate's central-symmetry row reads
    0.037). A cube cannot see this repair.
  * lodgenRepairOctHeight / the 8-ring dilate -- src/lodgen.cpp:2464, the
    function's own contract: "Coverage itself (the alpha of `coverage`) is never
    touched ... `img` may be the coverage image itself ... in which case only its
    RGB moves." F1 reads only that alpha.
  * frameOffset handling (src/lodgen.cpp ~2999) -- would move the frame's recorded
    extents fw/fh, which the gate reads from the sidecar and which are identical
    across the three logs (456.40 x 456.40).

## 3. Which it is: the gate's EXPECTATION, proved by a known-answer control

The coverage contract F5 pins is `coverage 16 128 160`: a texel's alpha is 0, or
remapped into [160,255]. So a consumer testing alpha >= 128 selects exactly the
texels whose measured coverage reached 16/255 = 6.27 % -- NOT half coverage. The
gate proves this itself, on the same sheet, in the row after F1: "the set the
consumer tests at 128 is exactly the set the bake floored at 16", GREEN; and F1b
reports 1.69 vs 1.69.

F1's 1.0-texel bar was written for a half-coverage reading. The coverage contract
-- shipped by the same lane, in the same build -- made alpha >= 128 into a
6.27 %-coverage reading, which admits a texel 6 % covered on each side of the true
edge.

Recomputed here, offline, with no exe. The cube sheet is NOT on disk (the gate's
workdir is mktemp -d with trap 'rm -rf "$W"' EXIT, lines 49-50), so the control is
analytic: the exact convex hull of the eight cube corners projected on each
frame's (r, up), rasterised 64x64 over the same +-456.40 units at 12x12 samples
per texel, thresholded, bounding box, the same px prediction, the script's own
axes(i,j,n). Scripts: f1sim.py and f1tab.py in this folder.

    an IDEAL, defect-free cube bake, measured by F1's OWN instrument
      at alpha>=128  ==  coverage >= 16/255 = 6.27 %      worst 1.78 texels
      at a TRUE half-coverage test                        worst 0.71 texels
      at the pending 0.20 alpha-cut ruling                worst 1.29 texels

    the ideal mask vs the real bake's mask, frame by frame:
      identical on 56 of 64 frames; on the other 8 the IDEAL is one texel WIDER
      worst ideal 1.78    worst real bake 1.69

The bar of 1.0 is unreachable by any bake: an analytically exact one scores 1.78,
and the shipped bake scores 1.69, i.e. slightly tighter than exact. The 0.71
figure is the same measurement at the threshold the bar was written for, and it
clears 1.0 -- the measured demonstration that the old floor encodes a defect in
the EXPECTATION and not in the bake.

The instrument is sound: the PERSPECTIVE CONTROL reads 19.62 on the same run and
the two bakes' recorded half-extents differ, so the row still separates.

## 4. The proposed repair, and its red control

File tests/spells/lodgen_octahedral.sh, lines 1157-1159.

BEFORE (exact, as on disk):

    # F1 and its floor F2
    check('F1: every frame of the cube spans its predicted texels within 1 AT THE READER THRESHOLD (worst %.2f)' % dOR, dOR <= 1.0)
    check('F2 (floor): the PERSPECTIVE CONTROL does not (worst %.2f, and it must exceed 1)' % dPR, dPR > 1.0)

AFTER:

    # F1 and its floor F2. The bar is 2.0, not the 1.0 lane CARDWIDTH pre-registered
    # on 2026-09-10. That 1.0 assumed `alpha >= 128` is a HALF-coverage reading; the
    # coverage contract the same lane shipped in the same build made 128 select
    # exactly the texels at or above the 16/255 floor -- the F3 row below proves it
    # on this very sheet -- which admits a 6%-covered texel on each side of the true
    # edge. Lane OCTF1, 2026-09-19, recomputed the analytic cube (exact convex hull,
    # 12x12 samples per texel, THIS instrument): an IDEAL bake scores 1.78 at this
    # threshold and 0.71 at a true half-coverage one, so 1.0 is unreachable by any
    # bake. 2.0 is the neighbouring row's bar and the same arithmetic.
    check('F1: every frame of the cube spans its predicted texels within 2 AT THE READER THRESHOLD (worst %.2f)' % dOR, dOR <= 2.0)
    check('F2 (floor): the PERSPECTIVE CONTROL does not (worst %.2f, and it must exceed 2)' % dPR, dPR > 2.0)

The number is not lowered on taste: 1.78 is measured and it is above 1.0.

RED CONTROL, so the new bar can still fail. F1 exists to catch "the tree changed
size"; one texel of extra silhouette on each side is that defect. Added after F2,
using orthoR's own masks -- no new bake, no exe, pure Python inside the block:

    # F2b (floor): the SAME measurement on the mask DILATED one 8-neighbour ring --
    # a silhouette one texel too wide on every side, the defect F1 exists to catch --
    # must exceed the 2-texel bar, so the row responds at its new bar.

Measured here on the analytic cube: one-ring dilation -> worst 3.78 texels,
comfortably above 2.0. F2 (the perspective control) survives the move on its own:
19.62 > 2.0.

A build lane must run `bash tests/spells/lodgen_octahedral.sh` to confirm F1 reads
green at 1.69 and F2b fires near 3.7 on the real sheet. Nothing about the bake
needs to change, and nothing here is called fixed until that run exists.

## 5. If the control is ever to be repeated on the REAL sheet

bake4/cube512_oct_albedo.png and bake4/cube512.txt are deleted by the gate's own
EXIT trap. A KEEP=1 escape that skips `rm -rf "$W"` (line 50) would let a
read-only lane re-run this control on the shipped sheet instead of on an analytic
twin. Not needed for the verdict above.

## 6. Incidental

- release/NifSkope.exe changed under this lane at 16:48 (23,505,408 B), newer than
  the 15:57 220662f1 build IMPOSTORFIX3 left. Another lane owns that slot; OCTF1
  never ran it.
- The pending, unapplied hookup_ruling_alpha.py in impostorfix3 ("alpha cut
  0.0627 -> 0.20") is the same 16/255 constant seen from the renderer's side. If it
  is ever ruled in, F1's worst moves from 1.69 toward the simulated 1.29 -- still
  above 1.0, so it is not an alternative to this repair.
