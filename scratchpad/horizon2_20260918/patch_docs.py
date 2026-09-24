#!/usr/bin/env python3
"""Lane HORIZON2's corrections to the four documents that carry the old march's
claims. Byte splices, unique anchors, line endings measured before and after
(all four files are LF-only and stay so)."""

R = 'E:/Projects/NifskopeWildWastelandEdition/'
EDITS = {}


def add(path, old, new):
    EDITS.setdefault(path, []).append((old, new))


# ===================================================================== 4.11
D = 'docs/LODGEN_NATIVE_LODO_LODI.md'

add(D, """own terrain, at a 5° sun), first step 32 u growing **×1.5** for **22 steps**,
receiver raised **4 u** off its own surface.

Two deliberate biases, both toward OVER-occlusion, because a shadow that is a
little too long reads as a shadow and a shadow that is missing reads as a bug:

* the elevation of a segment is measured to its **NEAR end**, so a distant
  occluder is reported slightly higher than it stands;
* the lattice sampled is the **coarsest mip the bin width allows**, so a tall
  thing standing beside the ray is caught rather than missed.

Two rules bound those biases, and both were **corrected against a measurement**
during this lane rather than reasoned into place:""",
    """own terrain, at a 5° sun), first step 32 u growing **×1.3** for **33 steps**,
receiver raised **4 u** off its own surface.

**×1.3 and 33 steps since lane HORIZON2 (2026-09-18); it was ×1.5 and 22 steps
before.** The elevation of a segment is measured to its NEAR end, so an occluder
standing at a segment's FAR end over-states `tan(elevation)` by up to `growth` —
at 1.5 that is half. Measured on ten receivers of chunk 4.4.-12 against a witness
that shares no code with this file, the residual bias after the footprint fix
below is **+2.74° at growth 1.5 and +0.86° at 1.3**, for 32 steps a bin instead
of 21; 1.2 would give −0.46° at 46 steps, which overshoots to the other side for
44% more work (`scratchpad/horizon2_20260918/candidate_out.txt`).

ONE deliberate bias remains, toward OVER-occlusion, because a shadow that is a
little too long reads as a shadow and a shadow that is missing reads as a bug:
the elevation of a segment is measured to its **NEAR end**, so a distant occluder
is reported slightly higher than it stands.

**There used to be a second, and it was the defect lane HORIZON2 exists to fix.**
The lattice sampled was "the coarsest mip the bin width allows", and each tap
read a 2×2 block on top of that, on the argument that a tall thing standing
beside the ray is then caught rather than missed. What that actually made was a
stored byte that is the **maximum over the bin's whole 22.5° sector** — and the
runtime does not read a sector maximum, it LERPS the two bins either side of the
sun's azimuth (ruling (i) of `docs/FO4CS_IMPROVED_LOD_PLAN.md` §9), which is only
meaningful if each byte is the skyline in ITS OWN direction. Measured price:
**+8.52° of systematic bias**, and 0.0% of chunk 4.4.-12's terrain texels lit at
a sun of azimuth 240 elevation 15 where the third witness says 10.2%. The
footprint now comes from the SEGMENT and the tap budget
(`LODGEN_HORIZON_MAX_TAPS`, 64), each tap reads THE ONE SQUARE IT LANDS IN, and
the taps are spaced HALF a square so nothing is stepped over.

Two rules bound what is left, and both were **corrected against a measurement**
rather than reasoned into place:""")

add(D, """* **the footprint is the bin, not twice the bin.** `maxAlong` reads a 2×2 block
  of squares at every tap, so choosing the level at `cell ≤ wantCell` blurred
  `2 × cell` — one whole bin wider than the bins, which is exactly what the
  same function's comment promised it would not do. The level is now chosen at
  `2 × cell ≤ wantCell`. Measured on 4,100 terrain samples: mean |march −
  reference| fell 4.103° → 3.651°, and the "the bins say HIGHER" column of the
  cause table fell 681 → 351.

**The refuter's reference is a CONE, and that is not a detail.** The stored
byte is the maximum over the bin's angular sector — the march reads a lattice
footprint as wide as the bin — so a reference made of one pencil ray measures a
different quantity, and the difference is the entire bin. Measured: with a
pencil reference the terrain half scored 50.00 balanced agreement and filed
1,621 disagreements under "the bins say HIGHER"; with the reference spread over
`LODGEN_HORIZON_REFUTE_CONE_RAYS = 9` pencils across the bin's own width
(±180/A degrees) the same bake scored 51.69–100 per sun position with 351 there.
The reference keeps all three of its independences (every pencil at its own
EXACT azimuth, a constant 32-unit step, mip 0); what changed is that it now
measures the quantity the byte holds. The census prints both — `…MeanRefDeg`
(cone) and `…MeanRefPencilDeg` (centre ray) — and their gap, 63.565° against
60.041° on the terrain and 18.875° against 18.249° on the objects, **is the
price of binning to 16 azimuths**, stated as a number.""",
    """* **the footprint is the SEGMENT, not the bin** (lane HORIZON2, 2026-09-18).
  The level is chosen so the segment fits in `LODGEN_HORIZON_MAX_TAPS` squares
  and each tap reads one square. The rule before it chose the level against
  `binWidthFraction × d` and took a 2×2 block, which is where the sector-maximum
  byte came from. An intermediate correction on the same day — "the footprint is
  the bin, not twice the bin", level chosen at `2 × cell ≤ wantCell` — moved mean
  |march − reference| from 4.103° to 3.651° and is **superseded**: it made the
  dilation exactly one bin instead of two, when the right answer was that the
  dilation should not be there at all. It is recorded because it is the shape of
  the mistake: a measured improvement inside a convention nobody had measured.

**THE REFUTER'S REFERENCE IS NOT INDEPENDENT OF THE MARCH, and that is the
lesson.** `lodgenHorizonReferenceElev` calls `LodgenHorizonField::maxAlong`
(`src/lodghorizonrefute.h:107` and `:112`, `wantCell` 1.0, mip 0) — the same
function the march calls. So when lane HORIZON2 changed `maxAlong`'s tap, the
REFERENCE moved too: its cone mean on chunk 4.4.-12's terrain went 63.565° →
58.995° and its pencil mean 60.041° → 54.226° in the same bake that moved the
sheet. A floor scored between two witnesses that share a function cannot fail
when that function is wrong, and that is exactly how a sheet calling a downtown
chunk 0.0% lit at a 15° sun held a 97% pre-registered floor for a day. The gate
now also carries **G6**, which scores the sheet against ten receivers whose
skylines were computed from the raw BTD heightmap and the placements as exact
world boxes — `tests/spells/lodgen_horizon_witness.json`, no line of
`src/lodghorizon.h` in it — with the pre-fix sheet as its red control.

The reference is still a CONE of `LODGEN_HORIZON_REFUTE_CONE_RAYS = 9` pencils
across the bin's own width, and **that choice is now questionable in the same
way**: it was made when the stored byte WAS a sector maximum, so that the two
measured the same quantity. The byte is now directional, so the centre pencil is
the like-for-like comparison and the cone over-states the reference by the
binning gap the census still prints — `…MeanRefDeg` minus `…MeanRefPencilDeg`,
4.77° on the terrain and 0.75° on the objects after the fix. Narrowing the cone
is a change to the refuter and therefore a change to a gate's own numbers; it was
left alone deliberately and is named as owed.""")

# ===================================================================== 3.5
T = 'docs/LODGEN_TERRAIN_VT.md'

add(T, """**How high a terrain horizon really is, since the number surprises.** On chunk
4.4.-12 the mean over every texel and every bin is **63.2°**, and the
independent reference ray agrees (60.0° as a pencil, 63.6° over the
bin's width). It is not a bug: a lattice square is 128 units and a Fallout 4
unit is about 1.4 cm, so “128 units away” is 1.8 m, and a tree 15 m
away and 20 m tall stands 53° up. A forest floor at first light is dark,
and the sheet says so. `horizonZeroBins` counts EXACT zeros and is 0 here for
the same reason: distant terrain 31 cells out still subtends more than the
0.353° that one stored step is worth, so almost no bin is empty. Read the
mean, not the zero count.""",
    """**How high a terrain horizon really is — and the version of this paragraph that
shipped on 2026-09-18 was wrong.** It said: the mean over every texel and every
bin of chunk 4.4.-12 is **63.2°**, the independent reference agrees (60.0° as a
pencil, 63.6° over the bin's width), *"it is not a bug"*, and `horizonZeroBins`
is 0 because distant terrain still subtends more than one stored step. Two of
those numbers were instruments agreeing with each other: the in-bake reference
calls `LodgenHorizonField::maxAlong`, the same function the march calls
(`src/lodghorizonrefute.h:107`), so it could not disagree about a footprint.

Lane HORIZON2 built a witness that shares no code with either — a 1° pencil over
the raw BTD heightmap plus every `.lodi`/`.lodo` placement as an exact world box —
and measured the sheet **8.51° above it in the mean, +8.59° of one-sided bias**
over ten receivers, with the sheet and the reference agreeing with each other
(5.59°) better than either agreed with the truth. The cause was the march's
footprint, not the lattice's coarseness: see `docs/LODGEN_NATIVE_LODO_LODI.md`
§4.11. **After the fix the same chunk's mean is 55.02°, `horizonZeroBins` is 776
rather than 0, and the viewer's own note line goes from 0.0% to 5.3% of the
chunk lit at a sun of azimuth 120 elevation 15.**

What survives from the old paragraph is the arithmetic, and it is worth keeping:
a lattice square is 128 units, a Fallout 4 unit is about 1.4 cm, so "128 units
away" is 1.8 m, and a tree 15 m away and 20 m tall really does stand 53° up. A
downtown chunk's skyline IS steep — the third witness puts chunk 4.4.-12's true
mean at about 54° — and a forest floor at first light really is dark. The error
was never that the number was large. It was that nothing had measured how much
of it the march was inventing, and the document said "it is not a bug" on the
strength of a witness that could not have said otherwise. `horizonZeroBins` is
still the wrong thing to read: read the mean, and read G6.""")

add(T, """**The own-square rule.** The march never reads a lattice closer than one of its
own squares (`--horizon-near-skip`, default 1 square, and `0` is the way back
to the bytes before that rule). Off, the mean above is 69.78°; at one
square 63.22°; at eight, 40.95° — a square's top is simply never
attributed to a distance shorter than the square is wide.""",
    """**The own-square rule.** The march never reads a lattice closer than one of its
own squares (`--horizon-near-skip`, default 1 square, and `0` is the way back
to the bytes before that rule). Off, the mean above is 69.78°; at one
square 63.22°; at eight, 40.95° — a square's top is simply never
attributed to a distance shorter than the square is wide.
**That sweep is a fact about the 2026-09-18 21:59:46 build and not about the
switch** (root `MISTAKES.md` records why), and lane HORIZON2 changed the march
underneath it: the default point is now 55.02°, and the 0 and 8 points have not
been re-measured. The rule itself is unaffected — it is a distance floor, not a
footprint — and it was checked as a candidate cause and cleared: sweeping
`nearSkipCells` moved the error against the third witness by 0.1°
(`scratchpad/horizon2_20260918/variants_out.txt`).""")

# ===================================================================== census
C = 'docs/LODGEN_CENSUS.md'

add(C, """`…MeanRefPencilDeg` and `…ConeRays` (the reference is the max of 9 pencils spanning one bin's width, because the stored byte is a sector maximum and comparing it with a single ray measures binning rather than error; `…MeanRefDeg` minus `…MeanRefPencilDeg` — 63.565 vs 60.041 on terrain, 18.843 vs 18.217 on objects — IS the price of 16 azimuths, as a number).""",
    """`…MeanRefPencilDeg` and `…ConeRays` (the reference is the max of 9 pencils spanning one bin's width; `…MeanRefDeg` minus `…MeanRefPencilDeg` is the gap between the two). **READ THESE TWO AS A PAIR WITH A WARNING SINCE LANE HORIZON2 (2026-09-18).** The cone was chosen because the stored byte WAS a maximum over the bin's sector, so a single ray measured binning rather than error; HORIZON2 removed the sector convention from the march — the byte is now the skyline in the bin's OWN direction — so the CENTRE PENCIL is now the like-for-like comparison and the cone over-states the reference. Worse, the reference is not independent of the march at all: `lodgenHorizonReferenceElev` calls `LodgenHorizonField::maxAlong` (`src/lodghorizonrefute.h:107`), so every one of these tokens moves when the march moves. On chunk 4.4.-12 they did, in the same bake that fixed the sheet: terrain 63.565/60.041 → **58.995/54.226**, objects 18.875/18.249 → **17.738/16.987**. A pre-registered floor between the sheet and this reference cannot fail when the shared function is wrong, which is how `horizonRefuteA240E15` sat at 51.69 with nothing else red. The check that can fail is `tests/spells/lodgen_horizon.sh` **G6**, scored against raw-input receivers in `tests/spells/lodgen_horizon_witness.json`.""")

add(C, """the measured sweep on chunk 4.4.-12's terrain is 69.78° at 0, 63.22° at 1, 40.95° at 8 (every point re-measured on the exe that ships, 2026-09-18 21:59:46; the pair this replaced was measured before the `maxAlong` mip fix and root `MISTAKES.md` records why a sweep is a fact about a build and not about a switch)""",
    """the measured sweep on chunk 4.4.-12's terrain is 69.78° at 0, 63.22° at 1, 40.95° at 8 (every point measured on the 2026-09-18 21:59:46 exe; the pair this replaced was measured before the `maxAlong` mip fix and root `MISTAKES.md` records why a sweep is a fact about a build and not about a switch — **and it happened AGAIN: lane HORIZON2 changed the march that evening, so on the exe that ships now the default point is `horizonMeanElev` 55.02° and the 0 and 8 points are unmeasured**)""")

# ===================================================================== plan s9
P = 'docs/FO4CS_IMPROVED_LOD_PLAN.md'

add(P, """* the sun's azimuth is measured the way the bins are. A runtime that measures
  azimuth from east, or counts anticlockwise, produces a picture that is
  perfectly smooth, perfectly stable and wrong by a quarter turn — which is why
  the rotated field is a named control on both sides (`WW_HORIZON_BIN_ROT`,
  `…ControlWorst`).""",
    """* the sun's azimuth is measured the way the bins are. A runtime that measures
  azimuth from east, or counts anticlockwise, produces a picture that is
  perfectly smooth, perfectly stable and wrong by a quarter turn — which is why
  the rotated field is a named control on both sides (`WW_HORIZON_BIN_ROT`,
  `…ControlWorst`);
* **the `lerp` is a constraint on the BAKE, not only on the runtime, and it was
  violated for a day.** Blending the two bins either side of the sun is only
  meaningful if each stored byte is the skyline in ITS OWN direction. The march
  that first shipped this field instead widened its lattice footprint to the
  azimuth bin's own width and stored a MAXIMUM OVER THE BIN'S SECTOR, which the
  source called a resolution invariant that "errs toward MORE occlusion, never
  less, which is the safe side for a shadow" and whose price nobody had measured.
  Measured by lane HORIZON2 against raw-input receivers: **+8.52° of systematic
  bias**, and a whole downtown chunk reported 0.0% lit at a 15° sun where the
  witness says 10.2%. Fixed 2026-09-18 in `src/lodghorizon.h`; the contract for
  any future baker is **one stored byte, one direction**.""")

add(P, """| bake time | one ray a bin a receiver, 22 steps, against a max-Z mip pyramid — the same order as the existing vertex-AO pass |""",
    """| bake time | one ray a bin a receiver, **33 steps** (22 before lane HORIZON2 took `growth` from ×1.5 to ×1.3), against a max-Z mip pyramid — the same order as the existing vertex-AO pass. A step is now about `LODGEN_HORIZON_MAX_TAPS` array reads rather than four times that, because a tap reads one square and not a 2×2 block; the measured bake of chunk 4.4.-12 with both halves and the refuter armed took 50 s against 47 s before |""")

add(P, """`tests/spells/lodgen_horizon.sh`. Lane report:
`scratchpad/horizon1_20260918/lane_horizon1_report.md`.""",
    """`tests/spells/lodgen_horizon.sh` (G6 and
`tests/spells/lodgen_horizon_witness.py` are the raw-input check, added by lane
HORIZON2 because G3's reference shares `maxAlong` with the march and cannot
falsify it). Lane reports:
`scratchpad/horizon1_20260918/lane_horizon1_report.md` and
`scratchpad/horizon2_20260918/lane_horizon2_report.md`.""")

# ===================================================================== apply
for path, subs in EDITS.items():
    p = R + path
    b = open(p, 'rb').read()
    s = b.decode('utf-8')
    crlf, lf = b.count(b'\r\n'), b.count(b'\n')
    assert crlf == 0, '%s: expected LF-only, found %d CRLF' % (path, crlf)
    for old, new in subs:
        n = s.count(old)
        assert n == 1, '%s: anchor matched %d times: %r' % (path, n, old[:80])
        s = s.replace(old, new)
    out = s.encode('utf-8')
    open(p, 'wb').write(out)
    print('%-38s %d edit(s)  CRLF %d of %d LF  %d -> %d bytes'
          % (path, len(subs), out.count(b'\r\n'), out.count(b'\n'), len(b), len(out)))
