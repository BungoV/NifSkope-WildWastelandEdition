# Lane TILING4 — the repeat fix, made shippable

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B (TILING3's DONE exe).
Rung taken once at 00:06: `release/NifSkope.before_tiling4.exe`, 21,484,032 B,
sha1 `a795c5886aa73bd203405c5d9d138b72fc859725`, byte-identical to the launch exe.

Every number below has the floor or the ceiling it is judged against printed beside it.
Where something did not pass, it says so and by how much.

---

## 0. Pre-registered gates and the frozen split

### The gates, as the brief set them before this lane started

- **F1** the swirl instrument calibrated with known answers before any candidate is
  scored; the selection/validation split frozen before the pick.
- **F2** switches off == rung bytes; copied vanilla sheets == vanilla at every setting;
  `.lodl` / `_data` / BTR / BTO untouched; 1 vs 16 threads byte-identical; C++ ==
  prototype at fifteen positions.
- **F3** 7 of 7 **and** 7 of 7 on repeat, grain, swirl, band table — all on the same
  bake. A green on one bought by losing another is a red.
- **F4** chain at baseline; exe newer than every changed file; drivers rebuilt;
  rung == launch bytes; no NifSkope left running.

### The pool, and why it is not simply "a disjoint 7 of TILING2's 22"

The brief asks the validation seven to come from TILING2's 22 shipped sheets. It cannot,
and the reason is worth stating plainly because it changes what the validation set means.

TILING2 picked its 22 off the **shipped sheets** alone: mip-3 luminance SD ≥ 5.25, the
trough midpoint in the histogram of all 2,304 sheets, which separates land from the
ocean/void filler. That rule says nothing about whether the cells under a sheet carry
**land paint** — and the sampler this lane changes only ever runs on painted ground.
Under TILING3's shipped default (`--land-detail-source vanilla`) a chunk with no land
paint copies vanilla's colour byte for byte and never calls the sampler at all. SPLAT1's
offline model reproduces exactly that: an unpainted area comes back as one flat grey
(127.5). That is why TILING3 could only use **7 of the 22**.

So the pool is TILING2's population rule **and** "the sampler actually runs here":

| rule | what it requires | floor | survivors |
|---|---|---|---|
| P1 | a shipped `Commonwealth.4.<cx>.<cy>.DDS` exists | — | 2,304 |
| P2 | TILING2's population rule, mip-3 luminance SD | ≥ 5.25 | 1,113 |
| P3 | every one of the 16 cells has a LAND record with ≥ 1 quadrant base texture | all 16 | 139 |
| P4 | the offline rung bake is not flat (TILING3's `refuse_flat`) | SD ≥ 1.0 | lazily, per pick |
| P5 | the flat 127.5 stand-in fraction of the offline rung bake | ≤ 2 % | lazily, per pick |

139 chunks pass P1–P3; 138 of them lie outside the selection, in the bounding box
cx -32..28, cy -40..24.

**P5 was added before any candidate of this lane was scored, and it was added because
the first lattice run picked a bad sheet.** P3 does not bound the flat-fill fraction: a
quadrant whose base texture is absent falls back to the chunk's dominant base, and where
that is absent too the model leaves the 127.5 stand-in. The first run picked (4,8),
which is 23.0 % stand-in — a flat patch that size is a real term in every periodicity
and every variance statistic this lane grades on. Refusals, named:

| chunk | refused because |
|---|---|
| (4,8) | P5, 23.0 % flat stand-in > 2 % |
| (0,8) | P5, 26.4 % |
| (4,4) | P5, 10.7 % |
| (4,12) | P5, 6.6 % |
| (8,12) | P5, 3.2 % |

### The split, frozen 00:11, before any candidate number existed

**SELECTION** — TILING3's seven, unchanged and in its order. Frozen by that lane;
re-picking it here would let this lane choose the ground it is graded on.

**VALIDATION** — this lane's seven, by a rule written down before it was run: a
deterministic 4×2 lattice over the bounding box of P1–P3 outside the selection, each
lattice point snapped to the nearest pool chunk by chunk distance (ties by `(cx,cy)`),
P4/P5 applied per pick with every refusal named, duplicates taking the next-nearest.
Nothing about any candidate was looked at.

| set | sheets |
|---|---|
| selection | (-20,24) (-20,20) (-36,-20) (-4,-20) (28,-20) (-4,16) (24,16) |
| validation | (-24,-24) (-12,-20) (4,-24) (20,-24) (-20,4) (-8,4) (12,8) |

Disjoint: yes, 0 shared.

### How much of each sheet is real

Because six of TILING3's seven carry some flat stand-in, the fraction is printed here
once and every later number is read beside it.

| selection | stand-in | offline rung bake SD (floor 1.0) | | validation | stand-in | bake SD |
|---|---|---|---|---|---|---|
| (-20,24) | 1.1 % | 16.81 | | (-24,-24) | 0.0 % | 9.28 |
| (-20,20) | 0.0 % | 14.32 | | (-12,-20) | 0.0 % | 14.73 |
| (-36,-20) | 0.0 % | 3.46 | | (4,-24) | 0.0 % | 25.98 |
| (-4,-20) | 0.0 % | 23.34 | | (20,-24) | 0.0 % | 10.20 |
| (28,-20) | 0.0 % | 9.16 | | (-20,4) | 0.0 % | 8.35 |
| (-4,16) | **8.5 %** | 19.86 | | (-8,4) | 0.0 % | 7.65 |
| (24,16) | 0.0 % | 12.12 | | (12,8) | 0.0 % | 18.26 |

Scripts: `scratchpad/tiling4_20260912/p0_pool.py`, `p0b_coverage.py`;
logs `logs/p0_pool.txt`, `logs/p0b_coverage.txt`; `pool.json`, `coverage.json`.

---

## 1. The swirl instrument and its controls

### What had to be built, and why none of the existing instruments would do

bungo's complaint about TILING3's proposal is that the fix is **visible as swirls**.
TILING2 and TILING3 between them own the repeat, the edges, the grain and the band
table, and **none of those can see a swirl**, because a smooth domain warp changes
neither the amplitude spectrum, nor the histogram, nor the local variance of the texture
it resamples — it only bends where things are. What a warp leaves behind is an
**orientation** field: the texture's own grain is stretched along the warp's local
principal direction, and that direction is constant over a whole lattice cell, so the
picture grows smears that are coherent over tens of texels and that curve.

### The instrument, as frozen

`scratchpad/tiling4_20260912/t4_lib.py`, `swirl()`:

1. notch out the land repeat's own frequency family (10.667 texels, TILING2's
   `notch_repeat`) — a repeat is coherent and axis-aligned and would otherwise be
   counted as a swirl;
2. keep the **grain**: a band pass from 1 to 5 texels;
3. the structure tensor of that band's gradient, each term box-averaged over a
   **17-texel window**;
4. per-texel coherence (λ1-λ2)/(λ1+λ2), energy-weighted mean over the sheet's interior.

**Floor** = the sheet's own phase twin (same amplitude spectrum, random phase). A phase
twin is worthless as a floor for a periodicity or a spectrum — TILING2 wrote that rule —
but orientation coherence is exactly the kind of structure statistic it *is* the floor
for: the twin has the same energy at the same scales and no orientation structure at all.

**Ceiling** = the **same chunk's** shipped vanilla sheet, +20 %, with vanilla's
worst-of-22 as an absolute backstop.

### The law, frozen 00:27 and not touched again by this lane

```
r(sheet) = swirl(sheet) / swirl(phase twin of that sheet)
PASS iff  r <= 1.20 * r(the SAME CHUNK's shipped vanilla sheet)
    and   r <= 3.2846                      (worst of vanilla's 22)
```

Both halves are per-sheet, which is the direct answer to TILING3's own mistake entry
("a per-sheet gate graded without a per-sheet floor"). The same chunk's vanilla sheet is
the right ceiling because under TILING3's shipped default our bake is meant to be a
substitute for exactly that sheet. The 20 % margin is the brief's own margin, the one it
sets for the grain and the band table. The backstop stops a chunk whose vanilla happens
to be extreme from licensing anything.

### The instrument was wrong twice first, and both refused designs are on disk

This is the part that matters most, so it is not buried.

| step | design | what it did | verdict |
|---|---|---|---|
| `s1_controls.py` | band 9..33 texels, 33-texel window, energy-weighted mean, worst-of-22 absolute ceiling | grating 1.0000 ✓, isotropic 0.2015 vs twin 0.2031 ✓ — but the proposal read **0.391** against its own rung's **0.403** | **REFUSED**: it could not tell the warped sheet from the unwarped one |
| `s1b_design.py` | diagnosis and sweep | a warp does not add features at its own lattice scale, it **stretches the 1–4 texel grain**; the first design band-passed that grain away and then measured the terrain's own shape. Swept band and window on the known defect only | band 1..5 texels, 17-texel window: the warp moves the reading **+3.19 vanilla SDs** |
| `s1d_readout.py` | the summary and the ceiling | a **worst-of-22** ceiling (ratio 3.2846) cannot fail the proposal, because vanilla's own population contains coastlines and drainage lines more oriented than any warp we would ship. A ceiling that cannot fail the known defect is not a gate | replaced by the same chunk's vanilla sheet +20 % |

Nothing in the sweep was fitted to any candidate of this lane: no candidate existed yet.
It was fitted to the **known defect** — a synthetic warp of known strain, and TILING3's
already-shipped proposal — which is what gate F1 asks for.

### The known answers, each registered before it was run

**A1 — a pure sinusoid must read near 1.0.** Reads **1.0000**. Pass.

**A2 — isotropic noise in the same band must sit at its own floor.** Reads **0.0905**,
its own twin **0.0909**, ratio **0.995** against a law that starts convicting at ~2.0.
Pass.

**A3 — a synthetic warp of known strain must read its strain.** TILING3's warp field
verbatim, applied to a real vanilla sheet (-20,24) whose ceiling is r ≤ 2.3927:

| warp amp (texels) | RMS strain | swirl | ratio r | verdict |
|---|---|---|---|---|
| 0.00 | 0.000 | 0.1712 | 1.9940 | inside |
| 2.00 | 0.069 | 0.2043 | 2.0981 | inside |
| 5.00 | 0.172 | 0.2161 | 2.2154 | inside |
| 10.00 | 0.344 | 0.2658 | 2.7007 | **OVER** |
| 21.34 | 0.733 | 0.3614 | 3.7805 | **OVER** |
| 42.70 | 1.467 | 0.3444 | 3.6975 | **OVER** |

Monotone from strain 0 to 0.733, which spans TILING3's own 0.718. The sheet's ceiling is
first crossed between strain **0.172** and **0.344** — so this instrument starts
objecting well below the 0.5 "visible wobble" line `a5_tune.py` wrote down by eye, which
is a check on that line rather than a contradiction of it.

**A3 was registered wrong and it is corrected here.** I registered "monotone" with no
limit. At strain 1.467 the reading **turns over** (3.6975, down from 3.7805): past about
one texel of stretch per texel the resample blurs the very grain it is stretching, so the
instrument **saturates**. It is monotone up to strain ≈ 0.75 and flat above. Every
candidate this lane measures sits far below that, so the saturation does not touch any
number here — but the registration was wrong and the correction is on the record, not
quietly dropped.

**A4 — the notch must separate the repeat's grid from a swirl.** A repeat of known
amplitude injected into a real sheet:

| injected amp (8-bit) | un-notched | notched |
|---|---|---|
| 0.0 | 0.1714 | 0.1712 |
| 0.5 | 0.1714 | 0.1712 |
| 2.0 | 0.1709 | 0.1712 |
| 8.0 | 0.1722 | 0.1712 |

The notched reading moves **0.0000** across a 16× range of injected repeat, and TILING2's
entire repeat law lives inside that range (its ceiling is 0.264; the rung reads about
2.0). The grid is not counted as a swirl. Pass.

**A4's first version was a bad control and it was replaced.** `s1c_freeze.py` used
TILING3's `average` bake as the no-repeat reference and read it *above* the rung —
0.28..0.70 against 0.15..0.32. That bake replaces every land texture with one flat
colour, so its fine band is nothing but alpha-blend edges and VCLR ramps: highly
oriented and almost energy-free. It is a valid no-repeat control for a **periodicity**,
which is what TILING3 correctly used it for, and a useless one for an **orientation**.
The injected repeat above is a known answer; the `average` bake was an assumption.

**A5 — the confound. `--land-sample stochastic` switched on two things at once.** The
warp (amp 683, lattice 1024) *and* a -1.00 mip bias. A sharper mip puts more energy in
exactly the band this instrument reads, so the two were baked apart on all seven
selection sheets:

| moved off the rung by | median | in vanilla SDs |
|---|---|---|
| the -1.00 mip bias alone | -0.0660 | -1.29 |
| the warp alone | +0.1636 | **+3.19** |

The warp moves the reading and the sharpening does not — the instrument reads stretch,
not sharpness. It also explains something about TILING3's shipped setting: **the
sharpening partly masks the swirl in this statistic**, which is why the shipped
combination reads lower than the warp alone.

### What the law sees on the known defect

Ratios against each sheet's own ceiling. "warp" is amp 683 with no mip bias — the defect
isolated. "proposal" is what TILING3 shipped behind the switch, warp and bias together.

| sheet | ceiling | vanilla | rung | warp only | proposal | law convicts |
|---|---|---|---|---|---|---|
| (-20,24) | 2.3927 | 1.9940 | 1.4558 | 3.5319 | 2.7754 | warp, proposal |
| (-20,20) | 2.0209 | 1.6841 | 1.1546 | 2.5904 | 2.1740 | warp, proposal |
| (-36,-20) | 1.9610 | 1.6342 | 1.0153 | 3.4623 | 3.0012 | warp, proposal |
| (-4,-20) | 2.1764 | 1.8136 | 1.1616 | 3.0540 | 2.8332 | warp, proposal |
| (28,-20) | 2.5432 | 2.1193 | 1.1341 | 2.6082 | 2.4080 | warp only |
| (-4,16) | 2.2962 | 1.9135 | 2.1651 | 3.3518 | 3.0455 | warp, proposal |
| (24,16) | 3.2846 | 3.1973 | 1.0654 | 3.3286 | 2.7634 | warp only |

- **acquits the rung on 7 of 7** — an instrument that convicts the unwarped bake is useless;
- **convicts the isolated warp on 7 of 7**;
- **convicts TILING3's shipped proposal on 5 of 7**.

### The instrument's blind spot, stated

The two sheets it cannot convict are **(28,-20)**, which is TILING3's own declared
outlier, and **(24,16)**, whose vanilla sheet is the most oriented of all 22 (r 3.20, so
its ceiling is pinned to the 3.2846 backstop). On ground whose vanilla is already streaky
this instrument cannot separate a warp from the terrain. No number in this lane will
pretend otherwise, and a candidate that passes only on those two sheets has not been
shown to be clean there.

Scripts and logs, all on disk including both refused designs:
`s1_controls.py` / `logs/s1_controls.txt` (refused), `s1b_design.py` /
`logs/s1b_design.txt`, `s1c_freeze.py` / `logs/s1c_freeze.txt`, `s1d_readout.py` /
`logs/s1d_readout.txt`, `s1e_law.py` / `logs/s1e_law.txt`, `swirl_law.json`.

**Gate F1: passed.** The split was frozen at 00:11, the instrument's law at 00:27, and
no candidate of this lane had been baked or scored at either moment.

---

## 2. H1 / H2 / H3 on selection and validation

### The grain and band gates had to be re-decided first, and why

Gate F3 as the brief wrote it asks the grain of every sheet to sit within 20 %
of **that same chunk's vanilla sheet**. Measured (`g0_grain.py`,
`logs/g0_grain.txt`), that is not a gate on the sampler at all:

| over the seven selection sheets | hp SD spans | local variance spans |
|---|---|---|
| **vanilla's own sheets** | 1.37 to 6.93, a factor of **5.1** | 2.0 to 46.8, a factor of **23.8** |
| anything our compositor produces | 2.88 to 3.73, a factor of 1.3 | 7.2 to 16.0 |

Vanilla's grain on a sheet is set by what terrain is *there*; ours is set by
which land textures are painted. No change to the **sampler** can move one
sheet's grain to that sheet's vanilla value, and neither the rung (**1 of 7**)
nor TILING3's shipped proposal (**4 of 7**) meets the clause. The director
replaced it (`brief_tiling4b.md`) with two gates a sampler can actually move,
and the same treatment for the band table. They live in one place,
`t4_gates.py`, so selection and validation are graded by the same code:

| gate | what it asks | ceiling |
|---|---|---|
| **G1** | the **median** hp SD over the set vs vanilla's median over the same set | plus/minus 20 % (TILING3's own criterion) |
| **G2** | **per sheet**, hp SD vs **the rung's** hp SD on that sheet | plus/minus 20 % - a per-sheet gate with a per-sheet floor |
| **G1-band** | each of the six radial bands, median share vs vanilla's median share | plus/minus 20 %, pass = 6 of 6 |
| **G2-band** | **per sheet**, mean absolute band error no worse than the rung's on that sheet | at most the rung's |
| repeat, swirl | unchanged, per sheet, ceilings frozen 00:11 / 00:27 | 0.264 absolute (or the sheet's own control) and 0.448 ratio; r <= 1.20 x the same chunk's vanilla r and <= 3.2846 |

The brief's literal per-sheet-vs-vanilla counts are printed in the `litG` /
`litB` column of **every** table below, so the change of gate hides nothing.

### G1-band is refused as well, and the refusal is measured, not asserted

`h2_rescore.py` re-scored all 25 variants of the H1/H2 sweep under the decided
gates without re-baking anything (every per-sheet number was already in
`h1_sweep.json`; vanilla's own values recover exactly from the stored errors).
G1-band came out **0 of 25** - and so did **the rung**, at 2 of 6. The miss is
entirely in the three finest bands, and it is the same miss whatever the sampler
does:

| radial band | vanilla's median share | the rung | TILING3's proposal | H1 hex 256 |
|---|---|---|---|---|
| 0 | 0.2133 | +34.6 % | +20.1 % | +33.6 % |
| 1 | 0.4284 | -5.2 % | -9.6 % | -5.4 % |
| 2 | 0.1557 | +13.6 % | +9.8 % | +22.0 % |
| 3 | 0.0520 | **-46.1 %** | -27.2 % | -36.9 % |
| 4 | 0.1023 | **-74.6 %** | -42.6 % | -72.9 % |
| 5 | 0.0482 | **-96.8 %** | -69.9 % | -95.9 % |

Stage C of `h3_sweep.py` priced closing them: the only thing that moves bands
3-5 is a much sharper mip, and at bias -1.50 it buys **5 of 6** and costs G2
(**0 of 7**) and G1 (**+27.2 %**); at -2.00, still 5 of 6. Never 6 of 6 at any
bias. So G1-band is reported in its own column and is **not** part of the
verdict, for the same reason the literal grain clause is not: the rung fails it
and no sampler can move it. **G2-band stays** - the rung passes it by
construction and candidates move it.

### What was binding: one sheet's repeat

Under the decided gates, `h2_rescore.py` gave: swirl 24 of 25 variants at 7 of
7, G1 20 of 25, **repeat 0 of 25**. Every geometry lands 5 or 6 of 7 and the
sheet that fails is the same one - **(-4,-20)**, 0.303 to 0.320 against its
0.264 ceiling. H1's per-hex offsets break the phase *between* tiles; they do
nothing to the land texture's own 10.667-texel period *inside* one tap.

### H3 (the warp capped at strain 0.5) - tested, and refused by its own numbers

The brief named H3 as the next rung and it is exactly the right instrument for
an *intra-tap* period, so it was built (`h3_sweep.py`, `make_tap_h3`: TILING3's
own warp field applied to the world position first, then H1's hex tiling on the
warped position - both deterministic in world position, so still seamless and
still byte-identical at 1 and 16 threads by construction). The amplitude comes
from a re-measured strain constant, not an assumed one; lattice 1,024, one
octave, as TILING3 shipped.

| H1 + warp at strain | repeat | swirl | G2 |
|---|---|---|---|
| 0.00 (H1 alone) | 6/7 | **7/7** | 7/7 |
| 0.10 | 5-6/7 | **7/7** | 5/7 |
| 0.20 | 6/7 | 6/7 | 6/7 |
| 0.30 | 5-6/7 | 6/7 | 6/7 |
| 0.50 (the brief's cap) | 5-6/7 | **2/7** | 7/7 |

The warp buys **nothing** on the repeat - 6 of 7 with it, 6 of 7 without it -
and it costs the swirl, which is the whole complaint this lane exists to fix.
**The brief's cap of 0.5 is about two and a half times looser than this lane's
instrument allows**: the swirl reading starts failing a sheet at strain 0.20 and
fails five of seven at 0.50. That is a measured check on `a5_tune.py`'s
by-eye "over 0.5 is a visible wobble" line, in the same direction as control A3.
H3 is refused.

### The mip-bias window G1 and G2 leave, which had never been sampled

G2 allows at most +20 % of the rung's grain; the rung sits 26.7 % below
vanilla's median, so G1 needs at least +9.1 % of it. The sweep had only ever
sampled bias 0.00 (+1.8 % of the rung, G1 red at -25.4 %) and -0.50 (+22.9 %,
G2 red). Stages A and D sampled between them:

| H1 hex 256 at bias | repeat | swirl | G1 (+/-20 %) | G2 | G2-band |
|---|---|---|---|---|---|
| -0.20 | 6/7 | 7/7 | -20.5 % | 7/7 | 5/7 |
| **-0.22** | **6/7** | **7/7** | **-19.9 %** | **7/7** | **6/7** |
| -0.25 | 6/7 | 7/7 | -19.1 % | 7/7 | 6/7 |
| -0.27 | 6/7 | 7/7 | -18.5 % | 6/7 | 6/7 |
| -0.30 | 6/7 | 7/7 | -17.6 % | 5/7 | 5/7 |

### The pick

The rule, written into `h4_pick.py` before the stage D table existed: eligible =
swirl 7 of 7 **and** G1 inside **and** G2 7 of 7 (the gates a sampler can move),
then rank by the most repeat-passing sheets, then the lowest worst-sheet repeat,
then the lowest worst-sheet swirl. The repeat is ranked rather than gated
because no candidate of any family reaches 7 of 7 on it, and the report must
name the sheet and the margin instead of picking a variant that hides it.

**PICK: H1 histogram-preserving hex tiling, tile 256 world units, mip bias
-0.22.**

| set | repeat | swirl | G1 (+/-20 %) | G2 | G2-band | G1-band (refused) | litG |
|---|---|---|---|---|---|---|---|
| selection 7 | **6/7** | **7/7** | **-19.9 %** | **7/7** | 6/7 | 2/6 | 3/7 |
| validation 7 | **6/7** | **7/7** | -44.4 % (rung -50.7 %) | **7/7** | 6/7 | 1/6 | 0/7 |

### The repeat, sheet by sheet, beside each sheet's own vanilla

| selection sheet | ours | ceiling | ratio (<=0.448) | that sheet's vanilla | the rung | verdict |
|---|---|---|---|---|---|---|
| (-20,24) | 0.259 | 0.264 | 0.144 | 0.201 | 1.152 | ok |
| (-20,20) | 0.350 | 0.366 (own control) | 0.281 | 0.032 | 1.364 | ok |
| (-36,-20) | 0.056 | 0.264 | 0.389 | 0.080 | 1.557 | ok |
| **(-4,-20)** | **0.308** | **0.264** | 0.155 | 0.225 | 1.200 | **RED, +17 % over** |
| (28,-20) | 0.213 | 0.264 | 0.205 | 0.034 | 1.707 | ok |
| (-4,16) | 0.224 | 0.264 | 0.123 | 0.264 | 1.100 | ok |
| (24,16) | 0.143 | 0.264 | 0.123 | 0.237 | 1.084 | ok |

The validation seven's one red is **(-12,-20)**, 0.279 against 0.264 - **+6 %
over**. Both red sheets pass the *ratio* half of TILING2's law with a wide
margin (0.155 and 0.298 against 0.448), so what fails on them is the absolute
amplitude against vanilla's worst-of-22, not a repeat visible over that sheet's
own broadband energy. The ceiling was **not** moved to accommodate them.

### Does the pick turn on one sheet? Partly - and here is the number

Re-running the rule with each selection sheet dropped: the same pick on **6 of
7** drops (the floor is 7 of 7, so this is a red). Dropping **(-36,-20)** flips
the *tile size* from 256 to 341 - same family, same bias, a near-tie broken by
the worst-sheet repeat. Both sizes were then scored on both sets
(`h4b_flip.py`):

| | selection repeat | validation repeat | swirl | G2 |
|---|---|---|---|---|
| hex 256 (the pick) | 6/7, worst 0.350 | 6/7, worst 0.279 | 7/7 and 7/7 | 7/7 and 7/7 |
| hex 341 (the flip) | 5/7, worst 0.345 | **7/7**, worst 0.233 | 7/7 and 7/7 | 7/7 and 7/7 |

Neither size passes both sets: 256 is 6/7 and 6/7, 341 is 5/7 and 7/7. The pick
is made on the selection set only - using the validation numbers to choose would
destroy the split - so **256 stands**, and the fact that 341 would have gated
the validation seven is on the record as the cost of that rule.

### Gate F3: NOT MET, and by how much

Repeat is 6 of 7 on both sets (short by one sheet, +17 % and +6 % over the
absolute ceiling); G2-band is 6 of 7 on both; G1 passes on the selection seven
(-19.9 % against plus/minus 20 %) and fails on the validation seven (-44.4 %,
where the rung itself is -50.7 % - our sampler improves that set's median grain
by 6.3 points and the remaining gap belongs to the compositor, not the sampler).
G1-band is a refused gate (the rung reads 2 of 6 and 1 of 6).

**So `stochastic` does NOT become the default in this lane.** What the lane
does instead is replace what the switch *means*: the swirl goes from **2 of 7**
(TILING3's warp, the thing bungo saw) to **7 of 7 and 7 of 7**, with the repeat
count unchanged at 6 of 7 and the grain no worse per sheet (G2 7 of 7 on both
sets). The warp stays reachable for the record.

Scripts and logs: `h2_rescore.py` / `logs/h2_rescore.txt`, `t4_gates.py`,
`h3_sweep.py` / `logs/h3_sweep.txt`, `h4_pick.py` / `logs/h4_pick.txt`,
`h4b_flip.py` / `logs/h4b_flip.txt`; `h2_rescore.json`, `h3_sweep.json`,
`h4_pick.json`, `h4b_flip.json`.

---

## 3. The change and its parity

### What landed, and where

`scratchpad/tiling4_20260912/c0_patch.py` (a refusing script, not four hand
edits: `--check` counts every anchor and writes nothing, every anchor is
asserted exact-once or exact-twice, and the script refuses if a file carries a
CRLF). Two of the edits are at **both** sampling sites, which is the whole
reason it is a script - `src/lodgen.cpp`'s own comment records that TILING2 lost
a relink by editing one `sampleLtex` and not the other.

| file | what |
|---|---|
| `src/lodgen.cpp` | `g_landHexSize`, `lodgenLandHexCell` (the triangle lattice), `lodgenLandHexOffset` (the same hash the warp already uses - one hash in the file), `lodgenLandHexTap` (the three-tap variance-preserving blend), and the size getter/setter |
| `src/lodgen.cpp` | both sampling sites: the single tap becomes one call to `lodgenLandHexTap`, which **off** evaluates the identical expression the line used to hold (`tex->getPixelT( u, v, mip )`) |
| `src/lodgen.h` | the declarations, and what they were measured at - including the two red sheets by name and margin |
| `src/nifcli.cpp` | `--land-sample stochastic` now means the hex tiling (256 units, bias -0.22); `--land-sample warp` is TILING3's warp, kept reachable so its measurement can be repeated; `--land-hex <units>` individually; the usage block |

Three decisions inside that code, each with its reason on the line:

- **Off is a `return`, not a multiply by zero.** At size 0 the tap function
  returns the caller's own expression, so the default bake cannot be anything
  but the rung's bytes, and that does not depend on arithmetic cancelling.
- **The tap takes the warp-offset coordinate**, so setting both switches gives
  warp-then-hex - which is exactly H3, measured and refused as a default.
  Nothing silently composes into a third sampler nobody picked: each
  `--land-sample` word turns the other geometry off.
- **Alpha is never blended.** Weights summing to one over a variance-preserving
  denominator would push a constant 1.0 alpha to about 1.07. The alpha comes
  from the largest-weight tap - a tap, not an average.

### The parity check, and the proof it can fail

`hex_parity.cpp` copies `lodgenLandHexCell` and `lodgenLandHexOffset` verbatim
out of `src/lodgen.cpp` with Qt's typedefs spelled out, replaces the texture
with a closed form (`s(u,v) = 255u, 255v, 255uv`) so no DDS, Qt or link is
needed, and prints, at **fifteen world positions x five settings**: the three
lattice vertex indices, the three weights and the blended colour. A wrong
vertex triple with right weights would otherwise hide. `hex_parity.py`
evaluates **h_cand.py's own** `tri_grid` and `hash01` - imported, not re-typed,
so if the prototype is edited the check follows it - and diffs.

| block | disagreements (floor 0) | positions the sampler moved off the plain tap (floor 8 of 15) |
|---|---|---|
| OFF | **0 of 15** | - |
| size 256 | **0 of 15** | 15 |
| size 341.333 | **0 of 15** | 15 |
| size 512 | **0 of 15** | 15 |
| size 682.667 | **0 of 15** | 15 |

**PASS: 0 disagreements over 75 comparisons**, and every setting moves the
sample (a check that passes on a sampler doing nothing is not a check).

**And the check is shown to be able to fail.** `python hex_parity.py
--sabotage` flips the sign of the skew on the prototype side - one character of
the transcription, the kind of mistake this probe exists for - and the same
comparison returns **FAIL, 56 disagreements over 75**, with the wrong lattice
cells printed beside the right ones.

### What the parity check cannot reach, stated

Both sides run in **double**; in the product the three taps come back from
`getPixelT` as float32 and the accumulator is a `FloatVector4`, so the shipped
blend runs in float. This proves the geometry, the hash, the vertex triple and
the blend formula agree - not the product's last float bit. The positions
include negatives (the hash's int-to-uint wrap), a point one ulp under a lattice
line and +/-1,999,999 units, the far edge of the worldspace where a float lattice
index would have quantised and where the C++ uses double for that reason.

Files: `c0_patch.py`, `hex_parity.cpp`, `hex_parity.py`,
`logs/hex_parity_cpp.txt`, `logs/hex_parity.txt`,
`logs/hex_parity_sabotage.txt`.

---

## 4. Build and gates

### The build, once

`Fallout4.exe` down (checked 02:07:01, and again inside the chain immediately
before the link, which is where the check belongs). bungo's NifSkope window is
his **installed** copy - `E:\Tools\Fallout 4\Nifskope\NifSkope.exe`, pid 60864,
no `--port` - so it does not hold our exe at all; the brief's pid 60820 is gone.
The build ran through `tools/ww_build.sh` anyway, which renames
`release/NifSkope.exe` aside inside the chain rather than before it, and it
renamed one aside (`release/NifSkope_inuse_60864.exe`, the launch exe's bytes).
Nothing was killed.

Two objects were deleted by hand first, and the reason is the skill's own
warning about qmake's frozen dependency lists: `nativeemit.cpp` and
`nifskope_ui.cpp` both `#include "lodgen.h"` and **`Makefile.Release` does not
name `src/lodgen.h` as a dependency of either** (nor of `nifcli.o`, which was
going to rebuild regardless because its own source changed). Nine other
mentions of the header are listed. A header that grew two declarations would
not have broken those two objects, but the check is cheap and the failure it
guards against is silent.

| | |
|---|---|
| `make -j2` | **BUILD-RC=0**, one build, no relinks |
| `release/NifSkope.exe` | **2026-09-12 02:08:57**, **21,487,616 B** (rung 21,484,032 B, +3,584 B) |
| exe newer than the sources | yes, checked against all three changed files |
| `release/style.qss`, the shaders | in step (`cmp` clean) |
| rung | `release/NifSkope.before_tiling4.exe` 00:06:03, 21,484,032 B, **== the launch exe's size**, md5 `8ec07038d14f4a973921d8a1c39a33b1`, untouched |
| new exe md5 | `8a1a1e718c6d6822ad0d60b90803fd69` |

### Gate F2: PASS (`f2_gate.py`, `logs/f2_gate.txt`)

Every arm is a whole-tree byte comparison of two bakes made by `t4_bake.sh`,
which is TILING3's bake command line with one switch added. **The comparator
was shown red first**, on a flipped byte in the middle file and on a removed
file, and the script refuses to grade any arm if either control comes back
green.

| arm | what | result | floor |
|---|---|---|---|
| A | the new exe with **no switch** vs the rung, both tiles | **9 files both, 0 differ** x2 | the default must not have moved |
| B | `--land-sample stochastic --land-hex 0 --land-mip-bias 0` vs the rung | **9 files both, 0 differ** x2 | CONSTITUTION 7's exact way back, measured |
| C | stochastic vs the rung | **3 of 9 differ, and exactly the right three** | the colour DDS **and both VT `.lodt`** must differ; `_data`, `_msn`, BTR, BTO, manifest, `.lodm` must not |
| D | `_msn` vs Bethesda's own file, at **five** settings x two tiles | **10 of 10 cmp == vanilla** (349,680 B each) | TILING3's ruling, untouched by this lane |
| E | `--land-sample warp` vs **TILING3's own `stoch` tree** of 2026-09-11 | **9 files both, 0 differ** x2 | "the warp is kept reachable" is a byte claim |
| F | `--chunk-threads 1` vs `16`, stochastic, 16-chunk block | **97 files both, 0 differ** | a sampler that is a pure function of world position |

**Arm C is the gate TILING2 failed and it is worth naming.** The two `.lodt`
files are the virtual-texture pyramid, and they are written by the **second**
`sampleLtex` call site. A change that reached only the chunk path would leave
them at the rung's bytes and everything else would still look right. They
moved. And under the rung-vs-`warp` comparison they move to **exactly**
TILING3's bytes, which is the same claim from the other side.

### Gate F3 on the real bakes, as a cross-check (`f3_real.py`, `logs/f3_real.txt`)

Section 2's fourteen-sheet verdicts come from `offline_bake.py`, a python
re-implementation of the compositor. It is the only way 25 variants x 14 sheets
could be swept at all, and it is **not** what the exe writes: it models the
land-texture composite and nothing else, with no crevice term and no vanilla
reuse. So the same instruments (`h1_sweep.score`, imported unchanged) were run
on the DDS the exe actually wrote, on the two chunks a bake of this size
reaches:

| arm | chunk | repeat | ceiling | swirl r | its ceiling | grain vs vanilla |
|---|---|---|---|---|---|---|
| rung | (-20,24) | 1.042 | 0.264 | 0.938 | 2.393 | -24.0 % |
| **def** | (-20,24) | **1.042** | 0.264 | **0.938** | 2.393 | **-24.0 %** |
| **stoch** | (-20,24) | **0.148** | 0.264 | **1.113** | 2.393 | -14.3 % |
| warp | (-20,24) | 0.183 | 0.264 | 2.221 | 2.393 | +11.3 % |
| rung | (-20,20) | 1.242 | 0.366 | 2.186 | 2.021 | -20.6 % |
| **def** | (-20,20) | **1.242** | 0.366 | **2.186** | 2.021 | **-20.6 %** |
| **stoch** | (-20,20) | 0.623 | 0.366 | 2.085 | 2.021 | -15.9 % |
| warp | (-20,20) | 0.593 | 0.366 | 2.279 | 2.021 | -0.8 % |

`def` reads the rung **exactly**, to every digit, on every instrument and both
chunks - which is gate F2 again through a second set of eyes, on numbers rather
than hashes.

**Two things in that table are worse than the prototype said and this report
does not bury them.** On (-20,20) the product's repeat under the hex tiling is
**0.623**, where the prototype had 0.350 - the prototype UNDER-stated it; on
(-20,24) the product reads 0.148 where the prototype had 0.259, so it
over-stated that one. Both directions, and the swirl readings sit a whole unit
above the prototype's on the same chunks (the rung reads 2.186 here and 1.155
there). The cause is the part of the real sheet the offline model does not
have - the crevice term and vanilla's own relief - and it means **the 6-of-7
and 6-of-7 verdicts of section 2 are the prototype's verdicts**. On the two
chunks where the product can be measured directly, the hex tiling is 1 of 2 on
the absolute repeat and 1 of 2 on the swirl (both on (-20,24)), and on (-20,20)
it is nonetheless **below the rung's own swirl** (2.085 against 2.186) and at
half the rung's repeat (0.623 against 1.242).

What survives both instruments, on the product, is the comparison this lane
exists to make: **the hex tiling reads less oriented than the warp on both
chunks** (1.113 vs 2.221, 2.085 vs 2.279) at a repeat that is no worse (0.148
vs 0.183, 0.623 vs 0.593).

### Gate F4: PASS - the chain at TILING2/TILING3's baselines

Run 02:12-02:18 on the new exe, `chain.sh`, `logs/chain.txt`:

| harness | baseline | this lane |
|---|---|---|
| lodl_open | 23 / 0 | **23 / 0** |
| lodgen_terrain | 26 / 0 | **26 / 0** |
| lodgen_terrain_vt | 41 / 1 | **41 / 1** |
| lodgen_roads | 11 / 0 | **11 / 0** |
| lodgen_ground_cover | 29 / 5 | **29 / 5** |
| lodgen_terrain_pbrm | 14 / 0 | **14 / 0** |
| lodgen_native | 18 / 0 | **18 / 0** |
| lodgen_panel_run | 125 / 0 | **125 / 0** |
| lod_generation | 116 / 0 | **116 / 0** |
| ui_align | 11 / 0 | **11 / 0** |
| water_ui | 82 / 0 | **82 / 0** |

Eleven for eleven, line for line. `lodgen_ground_cover` stayed at 29 / 5
without touching it, as the brief required - TILING3 taught it to pass
`--land-detail-source none`, which pins the switch off, and this lane's switch
is off there too.

No standalone gate driver executes in this chain (`grep` for
`release/*.exe` in the harnesses this change reaches returns only
`NifSkope.exe`), so the exe-newer rule has one binary to cover and it covers
it. No NifSkope process of ours was left running: the only one alive is
bungo's installed copy, which was never touched.

---

## 5. Pictures

`make_pics4.py` writes two, both from **real DDS off disk** written by
`release/NifSkope.exe` in gate F2's bakes, differing by one switch word.

**`images/cmp_tiling4.png`** - vanilla | `--land-sample warp` (what bungo
judged) | `--land-sample stochastic` (this lane), the same 128 texels at
(224,96) on chunk (-20,24) at 4x nearest neighbour, the same crop as
`cmp_tiling2.png` and `cmp_tiling3.png`, with repeat, grain and swirl r burned
into each panel.

| on the crop | repeat | grain | swirl r |
|---|---|---|---|
| vanilla | 0.456 | 4.416 | 1.433 |
| TILING3's warp | 0.598 | 5.135 | 1.695 |
| **this lane's hex tiling** | **0.252** | 3.965 | **0.755** |

**`images/sheet_tiling4.png`** - the same three sheets **whole, at 1:1**, 512
texels each, because a swirl is a 30-100 texel feature and a 128-texel crop can
hide one. These are the whole-sheet numbers, the ones the gates grade:

| whole sheet | repeat | grain | swirl r | local variance |
|---|---|---|---|---|
| vanilla | 0.201 | 4.476 | 1.994 | 19.81 |
| TILING3's warp | 0.183 | 4.981 | 2.221 | 25.12 |
| **this lane's hex tiling** | **0.148** | 3.834 | **1.113** | 15.45 |

At 1:1 the warp panel carries the feathered, curved streaking bungo called an
improvement worth making; the hex panel does not. **What the hex panel has
instead is a soft blotchiness at its own 256-unit cell scale** - visible in the
1:1 image as irregular light and dark patches a few dozen texels across. No
instrument in this lane gates that, and it is bungo's call whether it is better
or worse than the swirls; it is named here and in the doc amendment rather than
left for him to find. Its grain is also 14 % under vanilla's where the warp was
11 % over, which is the same trade seen from the other side.

---

## 6. Gate F3 on the PRODUCT - fourteen real bakes, and it changes the numbers

Section 2 swept 25 variants x 14 sheets with `offline_bake.py`, which is the
only way a sweep that size could be run at all, and section 4 then found it
diverging from the exe on the two chunks it could check - in both directions,
by as much as 0.27 on the repeat. So the fourteen-sheet verdict was re-taken on
the product: `f3_full.sh` baked **all fourteen sheets of the frozen split in
three arms** with the real exes (42 bakes, every one `rc=0`, 02:21-02:24, 3-6 s
each), and `f3_full.py` scored them with `h1_sweep.score` and
`t4_gates.decided` **imported unchanged**, so this table and section 2's are
graded by the same code. `logs/f3_full.txt`, `f3_full.json`,
`f3_full_per.json`.

| set | arm | repeat | swirl | G1 | G2 | G1-band | G2-band | litG |
|---|---|---|---|---|---|---|---|---|
| selection | rung (the floor) | **0/7** | 6/7 | -24.0 % | 7/7 | 2/6 | 7/7 | 1/7 |
| selection | **stochastic = hex 256** | **4/7** | **6/7** | **-14.3 %** | **7/7** | 2/6 | 5/7 | 4/7 |
| selection | warp (TILING3) | 5/7 | 3/7 | +11.3 % | **0/7** | 3/6 | 5/7 | 4/7 |
| validation | rung (the floor) | **0/7** | 6/7 | -42.9 % | 7/7 | 1/6 | 7/7 | 1/7 |
| validation | **stochastic = hex 256** | **5/7** | **7/7** | -36.3 % | **7/7** | 1/6 | 6/7 | 1/7 |
| validation | warp (TILING3) | 6/7 | 4/7 | -19.5 % | 1/7 | 1/6 | 6/7 | 3/7 |

**These supersede section 2's counts. Three of them are worse than the
prototype said and one of them reverses a claim this lane made earlier.**

1. **The hex tiling's repeat is 4/7 and 5/7 on the product, not 6/7 and 6/7.**
   Five of the fourteen sheets are red: (-20,20) 0.623, (-4,-20) 0.338,
   (-12,-20) 0.301, (4,-24) 0.324 on the amplitude, and (-36,-20) on the ratio.
   The prototype named two of those five and understated both.
2. **On the product the WARP passes the repeat on MORE sheets than the hex
   tiling** - 11 of 14 against 9 of 14. Section 2's "the hex buys the swirl at
   no cost in repeat" was the prototype's finding and it does not survive the
   real bakes: it costs two sheets.
3. **The rung - the shipped default - passes the repeat on 0 of 14.** That is
   TILING2's defect, unchanged and re-measured here at 0.531 to 1.618 against a
   0.264 ceiling, and it is the reason this work exists.

| how the repeat reads, sheet by sheet | rung | hex | warp | ceiling |
|---|---|---|---|---|
| (-20,24) | 1.042 | **0.148** | 0.183 | 0.264 |
| (-20,20) | 1.242 | 0.623 RED | 0.593 RED | 0.366 |
| (-36,-20) | 1.501 | 0.073, ratio 0.595 RED | 0.087, ratio 0.686 RED | 0.264 / 0.448 |
| (-4,-20) | 0.948 | 0.338 RED | **0.262** | 0.264 |
| (28,-20) | 1.618 | 0.203 | 0.102 | 0.264 |
| (-4,16) | 0.855 | **0.146** | 0.162 | 0.264 |
| (24,16) | 1.037 | **0.165** | 0.183 | 0.264 |
| (-24,-24) | 1.163 | **0.160** | 0.206 | 0.264 |
| (-12,-20) | 1.342 | 0.301 RED | **0.188** | 0.264 |
| (4,-24) | 1.336 | 0.324 RED | **0.253** | 0.264 |
| (20,-24) | 0.916 | **0.205** | 0.280 RED | 0.264 |
| (-20,4) | 0.962 | 0.171 | **0.164** | 0.264 |
| (-8,4) | 1.065 | **0.118** | 0.206 | 0.264 |
| (12,8) | 0.531 | **0.123** | 0.165 | 0.264 |

**(-36,-20) is a ratio failure worth reading twice before believing.** Its
amplitude goes 1.501 -> 0.073, a factor of twenty, the largest reduction on any
sheet in the set - and it still fails, because the ratio law divides the
amplitude at 10.667 texels by that sheet's own no-repeat floor, and when the
amplitude collapses the floor collapses with it. The rung reads a ratio of
**8.516** there; the hex reads 0.595 and the warp 0.686, both over 0.448. Both
samplers fail it, by the same mechanism, and the law is frozen from TILING2, so
it is counted as a failure and not argued with.

### What the product says the hex tiling actually buys

| | hex | warp | rung |
|---|---|---|---|
| swirl, of 14 sheets | **13 / 14** | 7 / 14 | 12 / 14 |
| G2 (grain within 20 % of the rung's, per sheet) | **14 / 14** | 1 / 14 | 14 / 14 |
| repeat, of 14 sheets | 9 / 14 | **11 / 14** | 0 / 14 |

**The swirl result holds on the product and it is the whole point of the
lane.** Every sheet: the warp's `r` is 2.048-2.737 on all fourteen, over the
ceiling on seven; the hex tiling is 0.859-2.084, and its **only** red sheet is
(-20,20), where the **rung itself is already red** (2.186 against a 2.021
ceiling) and the hex reads **below** it at 2.084. So across fourteen shipped
sheets the hex tiling does not make a single sheet's swirl worse than the
build bungo has; the warp makes eleven of them worse.

**G2 is the other number that survives, and it is stark.** Per sheet, grain
within 20 % of the rung's grain on that sheet: hex **14 of 14**, warp **1 of
14**. The warp's mip bias of -1.00 is doing that - it over-sharpens to
compensate for the blur it introduces - where the hex needs only -0.22.

**And G1 does not discriminate, for a reason that is not this lane's doing.**
The decided G1 is the set's median grain within 20 % of vanilla's median over
the same sheets. The **rung fails it on both sets** (-24.0 %, -42.9 %), so the
shipped default is already outside the gate; the hex tiling is **closer to
vanilla than the rung on both sets** (-14.3 %, -36.3 %) and still misses the
validation set. Same for G1-band: the rung reads 2/6 and 1/6 where the gate
wants 6/6. Two of the four grain gates cannot separate the arms because the
build they are measured against does not pass them either. That is worth
saying plainly rather than reporting 1/6 as if it were this lane's regression.

### The verdict, on the product

**Gate F3 is NOT MET** - the brief wants 7 of 7 on both sets on every gate, and
the hex tiling is 4/7 and 5/7 on the repeat, 6/7 and 7/7 on the swirl, and past
G1 on one set of two. **So the default does not change.** `--land-sample`
stays `footprint`, and with the switch off the bake is byte-identical to the
previous build's (gate F2 arm A, 9 files, 0 differing, both tiles).

What the lane delivers is narrower than "the repeat fix ships" and wider than
nothing: **the experimental switch now means a sampler that breaks the repeat
without straining the ground.** Against the warp bungo judged, on fourteen
real bakes: the swirl from 7/14 to 13/14, the grain-against-the-previous-build
from 1/14 to 14/14, at 9/14 of the repeat instead of 11/14. Whether that trade
is the one he wants is his call, and the blotchiness named in section 5 is part
of what he is trading for.

---

## 7. Owed, red, and the calls that are bungo's

(The brief numbers these 6/7/8; section 6 became the product-side F3 table,
which had to exist before any of this could be written honestly, so they are
7/8/9 here.)

### What is red

1. **Gate F3 is not met, so the default did not change.** Repeat 4/7 and 5/7
   where the gate wants 7/7 and 7/7; G1 missed on the validation seven
   (-36.3 % against a 20 % margin). `--land-sample` is still `footprint` and
   with the switch off the bake is the previous build's bytes exactly.
2. **The warp still breaks the repeat on more sheets than the hex tiling does**
   - 11 of 14 against 9 of 14, measured on the product. This lane's earlier
   claim that the swap cost no repeat came from the offline prototype and is
   withdrawn in section 6.
3. **The blotchiness is ungated.** The hex tiling substitutes, for the swirls,
   soft light-and-dark patches at its own 256-unit cell scale. No instrument in
   this lane measures it. It is visible at 1:1 in
   `images/sheet_tiling4.png`, and it is the reason a picture is a deliverable.
4. **`offline_bake.py` is not an absolute instrument and must not be used as
   one again.** It models the land-texture composite and has no crevice term
   and no vanilla reuse; against the product it was wrong in both directions by
   up to 0.27 on the repeat and by about a unit on the swirl. Comparative
   sweeps with it are still worth running - it found the right two failing
   sheets - but any number that reaches a document or a gate has to be re-read
   off a real bake. Section 6 is that re-reading.
5. **G1 and G1-band cannot separate the arms**, because the shipped default
   fails both (-24.0 % / -42.9 %, and 2/6 and 1/6 bands against a 6/6 gate).
   That is a gate-design problem inherited from the brief, not a regression
   here; the hex tiling is closer to vanilla than the rung on both sets.
6. **(-36,-20) fails the ratio law while improving twentyfold.** 1.501 ->
   0.073 amplitude, and a ratio of 0.595 against a 0.448 ceiling, because the
   ratio's denominator is that sheet's own no-repeat floor and it collapses with
   the amplitude. The law is TILING2's and frozen; it is counted as a failure
   and named as a possible artefact, not argued away.
7. **A NifSkope GUI of this build is running and this lane did not start it.**
   `release/NifSkope.exe`, pid **59040**, created **02:24:09**, no arguments,
   no `--port`. Every bake this lane ran was `-no-gui lodgen` and every one
   exited (42 of 42, `rc=0`). It was left alone, as the rules require. If it is
   bungo's, it is already the new build; if it belongs to another lane, that
   lane holds an exe and nobody should relink until it closes.

### What is owed, and to whom

| | |
|---|---|
| `BAKE_INSTRUCTION.md` | **not owed** - the default did not change, so no existing bake needs redoing |
| `WW_CHANGES.md` | text is in `WW_CHANGES_ENTRY.md`, corrected to the product's numbers; **the overseer splices it**, this lane never edits the file |
| `MISTAKES.md` | entries in `MISTAKES_ENTRIES.md`, same rule |
| `HANDOFF.md` | block in `HANDOFF_BLOCK.md`, same rule |
| `docs/LODGEN_TERRAIN_VT.md` | **done and then corrected**: `d0_doc.py` wrote 2.5e at 02:19 with the prototype's counts, `d1_doc.py` replaced both count paragraphs with the product's at 02:29 (115,570 -> 116,939 bytes, sha256 `b5fff76b436404d0`, CR 0, and it refuses on a second run) |
| commit | **never** - nothing was committed, nothing stashed; the tree carries other lanes' edits and this lane touched only its own three sources, the one doc and its own scratchpad |

### The calls that are bungo's, stated as questions

1. **Swirls or blotches?** The warp streaks; the hex tiling patches. Both are
   the sampler admitting it is breaking a repeat. `images/sheet_tiling4.png`
   at 1:1 is the comparison, and no number in this lane decides it.
2. **Which meaning should the experimental switch carry?** As measured: hex
   = repeat 9/14, swirl 13/14, grain-vs-previous 14/14. Warp = repeat 11/14,
   swirl 7/14, grain-vs-previous 1/14. Both are behind the same two words if he
   wants them (`stochastic`, `warp`); the question is which one a future lane
   should be trying to make the default.
3. **Is the cell size worth a sweep on real bakes?** 256 units was picked on
   the prototype's readings. Four of the five repeat failures are amplitude
   failures inside one tap, which a cell size does not touch - but a second,
   larger-cell octave might, and that was never measured on a real bake. It
   is a lane, not an afternoon: 14 sheets x N settings x a 4-second bake.
4. **Should TILING2's ratio law keep its floor?** On a sheet where the
   amplitude falls twentyfold the ratio can still read over the ceiling. Either
   the law wants a floor under its denominator or (-36,-20) is genuinely still
   periodic and the amplitude is lying. That is a measurement question and the
   next lane in this area should settle it before it grades anything.

---

## 8. Mistakes

1. **I put a modelled number into a shipped document.** `d0_doc.py` wrote
   2.5e - and `WW_CHANGES_ENTRY.md` its bullet - saying the hex tiling passes
   the repeat on "6 of 7 and 6 of 7", which was `offline_bake.py`'s answer. I
   already knew from `f3_real.py`, written **before** the doc, that the
   prototype disagreed with the product on both chunks it could check. The
   right order was: bake the fourteen, then write the document. The measurement
   cost four minutes. Corrected by `d1_doc.py` and section 6, both of which
   state the withdrawn claim rather than quietly replacing it. The lesson is not
   "the model was wrong" - models are wrong, that is what makes them models -
   it is **do not let a modelled number cross into a document while a
   measurement is affordable**.
2. **Gate F2 arm C's expectation was wrong and the harness caught it, not
   me.** I expected the stochastic bake to move exactly one file, the colour
   DDS. It moved three; the other two are the VT pyramid's `.lodt` pair, which
   is written by the *second* `sampleLtex` site - the site TILING2 missed. My
   list was the error, the product was right, and the gate reported FAIL until
   I had proved which of the two it was (digests across TILING3's trees and
   this lane's). The expectation is now three files and the reason is written
   beside it.
3. **`f3_real.py` crashed on a key that `h1_sweep.score` does not return**
   (`abs_ceil`). I had written the ceiling from memory instead of from the
   function. Fixed by computing it the way `score()` does.
4. **Two round trips lost to CRLF in a heredoc**, which is a hazard this repo
   has a memory note about. A multi-line anchor typed into a `<<'EOF'` heredoc
   arrives with CRLF here, so it never matches an LF-only file and the patch
   refuses. No damage - the scripts refuse rather than write - but the fix is
   to normalise the anchor inside the script (`s.replace('\r\n', '\n')`)
   rather than to trust the heredoc.
5. **Not mine, recorded because the brief asks:** the first TILING4 agent died
   silently at 00:40. The director's mistake was an hour with no liveness
   check on it. Everything that agent had written was on disk and usable,
   which is the only reason this lane is a resume and not a restart.

---

## 9. What the skills did, and the one that is missing

Used, and what each one actually caught:

| skill | what it did here |
|---|---|
| `ww-anchored-hookup` | `c0_patch.py` (7 anchors, exact counts 1,2,2,1,1,1 - the 2 is **both** `sampleLtex` sites), `d0_doc.py`, `d1_doc.py`. All three refuse on a second run; the doc scripts were shown refusing. |
| `ww-module-off-is-identical` | gate F2 arms A and B. Off is the rung's bytes, 9 of 9 files, both tiles; and the documented way back (`--land-hex 0 --land-mip-bias 0`) is too. The comparator was shown red on a flipped byte and on a deleted file before any arm was graded. |
| `ww-contract-provenance` | the provenance block in the doc: three source hashes, and every claim re-found from its own anchor text, including the one whose anchor must occur **twice**. |
| `nifskope-ww-build-verify` | the game check before the link, the exe-newer-than-sources check, style.qss and the shaders in step, one build and no relinks, and the rename-aside instead of a kill. It also carried the warning that caught the stale-object hazard: `Makefile.Release` does not list `src/lodgen.h` as a dependency of three TUs that include it. |
| `ww-control-calibration` | the swirl instrument's controls in section 1, and the reason the instrument has a per-sheet floor at all. |
| `nifskope-ww-resume-pending` | `PENDING.md` early and updated, which is why the brief could tell a resuming agent not to re-litigate the verdict. |

**The gap this lane found, offered as a skill for the overseer to splice** (it
is one paragraph and it would have saved mistake 1):

> ### ww-prototype-is-not-the-product
> A python re-implementation of a bake is the only way to sweep a design space,
> and its ABSOLUTE numbers are not the product's. Before any count from it
> reaches a gate, a document, a changelog entry or a handoff: bake the same
> sheets with the real exe, score them with the **same** scorer imported
> unchanged, and put that table in the report. If the two disagree, the
> product wins and the report says which claim was withdrawn. Budget it: a
> dim-4 chunk bakes in 3-6 seconds, so fourteen sheets in three arms is four
> minutes, and there is no sweep large enough to make that unaffordable for the
> final answer. Red flag: a document being written while the only measurement
> of the shipped code is two chunks wide.

`.claude/skills/ww-crlf-line-endings` (or the memory note that stands in for
it) is worth one added line as well: *a multi-line anchor typed into a
`<<'EOF'` heredoc arrives CRLF on this machine; normalise inside the script.*

---

## Addendum, 02:35 - two things that changed after section 7 was written

1. **The stray GUI closed by itself.** The `release/NifSkope.exe` pid 59040 of
   section 7 item 7 (started 02:24:09, no arguments, not this lane's) was gone
   by 02:35. At 02:35 `tasklist` shows **no NifSkope process and no
   `Fallout4.exe`**, so the exe is free for the next build and nothing was
   killed to make that true.

2. **The closing files are on disk**, all LF-only: `DONE` (replacing
   `BUILDING`), `PENDING.md` rewritten as the closing state with the
   superseded prototype line named as superseded, `HANDOFF_BLOCK.md`,
   `MISTAKES_ENTRIES.md`, and `WW_CHANGES_ENTRY.md` re-written to the
   product's counts. The three shared files (`WW_CHANGES.md`, `MISTAKES.md`,
   `HANDOFF.md`) are the overseer's to splice and were not touched.
