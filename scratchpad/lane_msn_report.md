# LANE MSN — is vanilla's terrain `_msn` detail integrable?

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, uncommitted
("Not yet"). No `src/` edit, no build, no commit. `Fallout4.exe` down (checked
with `Get-Process Fallout4`, no process). `release/NifSkope.exe` was NOT run —
nothing in this lane needed it.

Everything below is offline Python over vanilla's shipped sheets and our own
generated ones. All scripts under `scratchpad/mountains_20260907/`.

---

## 1. Where the test was found / what was written

**It did not exist.** `grep -rl "curl\|integrab"` over
`scratchpad/mountains_20260907/` returns nothing. WW_CHANGES 2026-09-07 says it
"was written and not run"; what was written was the *idea*, in that paragraph.
There is no script.

Written this lane:

| file | what it is |
|---|---|
| `scratchpad/mountains_20260907/msn_curl.py` | the whole test: two integrability metrics, five controls, the codec model, the band breakdown, the refuter |
| `scratchpad/mountains_20260907/curl_out.txt` | its output, mip 0, the three unpainted far tiles |
| `scratchpad/mountains_20260907/curl_out_mip1.txt` | the same at mip 1 (robustness) |
| `scratchpad/mountains_20260907/curl_out_painted.txt` | the same on three **painted** tiles |
| `scratchpad/mountains_20260907/skill_draft_control_calibration.md` | a skill this lane needed and could not write into the live tree (section 6) |

Inputs: vanilla from `E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\
Commonwealth` (the corpus, never a mod folder); ours from
`C:\Users\bungo\AppData\Local\Temp\claude\laneb\gen\peak\tex`, four tiles,
written 2026-09-07 12:48 by the current exe — i.e. **after** the bilinear fix,
confirmed by the block-edge statistic (ours now differs from its left neighbour
at 29–32% of within-block columns; before the fix it was 0.0%).

### What the test computes

A normal map baked from a height field is curl-free. In texture coordinates
(`u` east, `v` **south** — row 0 is the north edge):

    P = dh/du = -n_east / n_up          R = dh/dv = +n_north / n_up

and `h` exists iff `dP/dv == dR/du`. Two metrics, because neither is above
suspicion alone:

* **CURL** — `rms(dP/dv - dR/du) / rms(dP/dv + dR/du)`, one-texel central
  differences. Local, no transform. Its floor is **not** zero: a finite stencil
  has truncation error at these frequencies.
* **NON-INT** — the Helmholtz split. Mirror the field so it is periodic (the
  standard even extension of `h`: `P` odd in `u`, `R` even in `u`, reversed in
  `v`), FFT, and at each frequency split the vector into the part parallel to
  **k** (a gradient) and the part perpendicular (not). NON-INT is the
  perpendicular share of the energy. 0 for a height field, ~0.5 for an isotropic
  random field.

Both are applied to the **high-frequency residual**: the slope field minus its
own 9x9 box mean, the same local blur `msn_features.py` used for the 6.6–8.2x
measurement. Everything is cropped 8 texels so no edge-padded value is counted.

Sanity, before any tile: an exact analytic gradient reads CURL 0.048 /
NON-INT 0.0000; the same with its components swapped reads 0.666 / 0.518; white
noise reads 0.997 / 0.498. The metrics do what they claim.

---

## 2. Controls

### 2.1 The first floor failed, and why that matters

The brief's positive control — our own shipped bilinear `_msn` — **does not
work, and the reason is worth keeping.** Our high-frequency residual is 1.9–2.4
bytes against vanilla's 13.3–14.5, so nearly everything ours has in this band
**is the block codec**, and a codec's error is not integrable. Our sheet reads
CURL 0.937–0.941, indistinguishable from its own swapped copy (0.958–0.965) —
separation 1.0x. Measured against that floor, nothing could be concluded. This
was run first and is in the record; it is not a fault in our generator, it is
that our sheet has almost no signal in the band being asked about.

So the floor had to be built to carry **vanilla's own amplitude and spectrum
through the same encoder**. Three were built, weakest assumption last:

* **INT-CTRL (paired floor)** — vanilla's own residual Helmholtz-projected onto
  its curl-free part in float, recombined with vanilla's low-frequency field,
  re-encoded to 8-bit normals and run through a 4-colour block codec.
* **PHASE-CTRL (independent floor)** — a field with the *same 2D amplitude
  spectrum* as vanilla's residual but **random phase**, then projected
  curl-free, same amplitude, same encoder. Keeps the spectrum, throws away
  vanilla's structure, so it cannot be called self-serving.
* **SOL-CTRL (ceiling)** — the solenoidal part instead, rescaled to the same
  rms. Same size, same spectrum, no height behind it.

All controls are matched on **slope-residual rms**, not on byte energy (the
encoding saturates: a much steeper field can carry the same byte energy — see
Mistakes).

The codec is emulated (`bc_roundtrip`: principal-axis endpoints per 4x4 block,
RGB565, 4-entry palette, nearest index) and checked before use: re-encoding
vanilla's own texels adds **1.6–2.1 bytes rms** and moves vanilla's own reading
by **+0.7 to +1.0%** (NON-INT 0.2728 → 0.2746). The floor decomposes as
float 0.0013 → 8-bit 0.0014 → block codec 0.0984: the entire floor is the block
codec, which is exactly what it is there to measure.

### 2.2 The numbers (mip 0, 512x512, tile `Commonwealth.4.-12.44` first)

NON-INT:

| tile | paired floor | indep. floor | ceiling | **ceiling/floor** | vanilla |
|---|---|---|---|---|---|
| 4.-12.44 | 0.0984 | 0.1641 | 0.8010 | **8.14x PASS** | 0.2728 |
| 4.-16.44 | 0.0838 | 0.1495 | 0.8078 | **9.64x PASS** | 0.2500 |
| 4.-12.48 | 0.0899 | 0.1560 | 0.8081 | **8.98x PASS** | 0.2737 |

CURL, same tiles: floor 0.394–0.412, ceiling 1.52–1.56, **separation
3.69–3.97x — FAILS the 5x gate**. CURL is reported and not used for the
verdict. NON-INT passes on all three tiles and is what the verdict rests on.

Gate satisfied: controls separated by at least 5x, on every tile, on the metric
used. Numbers above.

At mip 1 (256x256) the same three tiles give paired floor 0.088–0.103,
independent floor 0.158–0.178, ceiling 0.79–0.82, separation 7.7–9.2x, vanilla
0.247–0.269 — 2.62–2.80x the paired floor, 1.48–1.56x the independent one, and
38–42% on the mixture. The reading does not depend on the mip.

---

## 3. Vanilla, per tile

Placed between floor and ceiling. "Excess" is vanilla divided by each floor.

| tile | mip | size | HF energy (bytes) van / ours | vanilla NON-INT | vs paired floor | vs indep. floor | % of the way to ceiling |
|---|---|---|---|---|---|---|---|
| `Commonwealth.4.-12.44` | 0 | 512 | 14.46 / 2.25 (6.41x) | 0.2728 | 2.77x | 1.66x | +25% |
| `Commonwealth.4.-16.44` | 0 | 512 | 13.57 / 2.41 (5.64x) | 0.2500 | 2.98x | 1.67x | +23% |
| `Commonwealth.4.-12.48` | 0 | 512 | 13.27 / 1.93 (6.87x) | 0.2737 | 3.04x | 1.75x | +26% |

`4.-12.44` is the tile of the 6.6–8.2x measurement; the high-frequency ratio
reproduces at 6.41x here (WW_CHANGES quotes 15.33/2.02 = 7.6x on the *pre*-fix
sheet; ours is now 2.25 post-fix on the same metric, so 6.41x is the current,
correct figure).

### 3.1 Where in the spectrum it sits — not the encoder

`Commonwealth.4.-12.44`, NON-INT restricted to each band of spatial period:

| period (texels) | share of residual energy | floor | VANILLA | ceiling |
|---|---|---|---|---|
| > 16 | 0.6% | 0.0313 | 0.0476 | 0.1632 |
| 8 – 16 | 9.2% | 0.0284 | 0.0871 | 0.7170 |
| 4 – 8 | 36.1% | 0.0551 | 0.1776 | 0.8181 |
| 2.7 – 4 | 29.6% | 0.1019 | 0.2713 | 0.8018 |
| 2 – 2.7 | 24.4% | 0.1667 | 0.3951 | 0.8059 |

Vanilla runs at **2.4–3.2x the floor in every band**, including 8–16 texels
where the codec barely reaches. If this were compression noise it would be
confined to periods 2–3. It is not. The other two tiles give the same shape.

### 3.2 Painted ground reads the same as unpainted ground

**All 64 cells under these three tiles are unpainted** — `ltex=-`,
`base=00000000` x4, `layers=0,0,0,0`, vertex colour 0, read from `layers.txt`
(the `lodgen --dump-layers` output of the MASTER, already on disk). They are
outside the painted box (y 44–51 against a maximum of 32).

So the same test was run on three tiles **inside** the painted box (16 of 16
cells painted on each):

| tile | painted? | paired floor | indep. floor | vanilla | vs paired |
|---|---|---|---|---|---|
| `4.-12.44` | no | 0.0984 | 0.1641 | 0.2728 | 2.77x |
| `4.-16.44` | no | 0.0838 | 0.1495 | 0.2500 | 2.98x |
| `4.-12.48` | no | 0.0899 | 0.1560 | 0.2737 | 3.04x |
| `4.-20.24` (Sanctuary) | yes | 0.0865 | 0.1366 | 0.2529 | 2.92x |
| `4.0.0` | yes | 0.1059 | 0.1560 | 0.3059 | 2.89x |
| `4.-8.-8` | yes | 0.0991 | 0.1476 | 0.2906 | 2.93x |

Painted and unpainted read **the same multiple of their own floor** (2.89–3.04x
across all six). The ranges overlap: the lowest of all six is a painted tile.
Whatever puts the non-height component there is applied to ground that has no
material assignment at all.

### 3.3 A free by-product: green-is-up confirmed a third way

Reading the sheet with UP in BLUE instead of GREEN sends vanilla's NON-INT to
**0.479–0.489** — the value of a random field. With GREEN it is 0.250–0.274.
The correct channel order makes the sheet far more integrable, which is an
independent third confirmation of WW_CHANGES 2026-09-07 (the previous two were
the recoverability of the up component and the VHGT prediction test).

---

## 4. Verdict, and the refuter

**MIXED, and the mixture is measurable.** Vanilla's high-frequency `_msn`
residual is far more integrable than a random field (0.25–0.27 against ~0.50)
and nowhere near the non-integrable ceiling (0.80), but it is clearly and
consistently **above** what an integrable field of the same amplitude and
spectrum reads through the same block codec — 2.8–3.0x the paired floor,
1.7–2.0x the most conservative independent floor.

Under the mixture model `NON-INT = (1-f)·floor + f·0.5` — i.e. the extra
component is isotropic, so half its energy is perpendicular:

| floor used | share of the residual's energy with **no height field behind it** |
|---|---|
| paired (INT-CTRL) | **40–45%** unpainted, 40–51% painted |
| independent (PHASE-CTRL) | **29–34%** unpainted, 32–38% painted |

So roughly **one third of the residual did not come from any height field, and
roughly two thirds did.** It is neither a pure finer heightfield nor pure
composited normals.

### What this means for the feature

1. **Reusing vanilla's sheets where terrain is unchanged is the only route that
   reproduces what he is looking at.** Two thirds of the detail is
   height-consistent, but it is height at a resolution the ESM does not carry,
   and one third is not height at all — neither half is derivable from VHGT.
2. **The "reproduce it from the painted landscape material normals" route
   cannot serve the mountains he actually asked about.** Those cells have no
   painted material — 0 of 64 — and yet they carry the same non-height fraction
   as Sanctuary does. Whatever the source is, it is not per-cell LTEX painting.
   A generator that composites the layers of a cell would still write a blank
   sheet out there.
3. **Integrability discriminates less than the thread assumed, and that is worth
   saying plainly.** A material normal map is itself usually baked from a
   height, so a composited material normal is *mostly* curl-free too — the
   non-integrable part only comes from the blending, the per-layer transforms
   and the renormalisation. The test cannot separate "finer heightfield" from
   "composited height-derived normals" by integrability alone. What it *does*
   settle is that neither pure story is right, and 3.2 settles that the painted
   layers are not the mechanism.

### Refuters — what would overturn each claim

* *That vanilla is above the floor at all*: a real BC3 encoder materially worse
  than the emulated one. Test: encode INT-CTRL with the same compressor
  Bethesda used and re-measure. Bound already in hand — a whole second
  generation of block error on vanilla itself (+1.6–2.1 bytes rms) moves the
  reading by under 1%, so the floor would have to be wrong by ~3x.
* *That the non-height share is ~1/3*: the isotropy assumption in the mixture
  model. An anisotropic extra component changes the 0.5 constant and therefore
  the percentage, though not the sign.
* *That a fine bake filtered down could explain it*: **measured and defeated.**
  A height field baked at 4x and normal-filtered to 512, matched on
  slope-residual rms and riding on vanilla's own low-frequency slope, reads
  NON-INT 0.209–0.217; **the same field with no supersampling reads
  0.206–0.215.** Supersampling adds under 0.005. Renormalising averaged normals
  is not what puts the non-gradient energy there.
* *That it is compression*: refuted by 3.1 — the excess is present at periods of
  8–16 texels, four times the block period.
* *That painted layers are the source*: refuted by 3.2.
* *That the whole thing is a channel-convention error*: refuted by 3.3 — the
  wrong convention reads 0.48, i.e. random.

### Step 5 (the optional LTEX probe)

**Not applicable as briefed, and that is the finding.** The probe was to
correlate the residual against the material normal maps of the layers painted on
the tile. There are none: `--dump-layers` (already on disk as `layers.txt`) has
0 painted cells of 64 for the three tiles. The correlation has no regressors.
The substitute measurement — painted tiles versus unpainted ones, 3.2 — was run
instead and answers the same question negatively.

---

## 5. Mistakes

1. **Matched a control on the wrong quantity.** The supersampling refuter was
   first amplitude-matched on *byte* high-frequency energy. The normal encoding
   saturates, so a much steeper field carries the same byte energy: the control
   came out steeper than vanilla and read NON-INT 0.207 instead of ~0.16, which
   would have understated vanilla's excess by nearly a factor of two. Found by
   printing the slope-residual rms of every field side by side and seeing the
   control at a different steepness. Rule: **a control is matched on the
   quantity the metric actually reads**, and that quantity is printed next to
   every control.
2. **`boxmean` was called with an even window.** `boxmean(a, 4*BLUR)` with
   BLUR=9 gives k=36 and returns an array one row and one column too large
   (the helper assumes odd k, from `msn_features.py`). It raised a broadcast
   error rather than corrupting a number, so nothing was reported from it.
   Fixed to `4*BLUR + 1`. Rule: this `boxmean` is odd-k only; anything deriving
   a window from another constant makes it odd explicitly.
3. **The first version of the test would have reported a false negative.** It
   used our shipped sheet as the positive control, per the brief, and the
   controls separated by 1.0x. Reporting a vanilla verdict against that floor
   would have been wrong; the brief's own gate caught it, which is the gate
   working. Kept in section 2.1 rather than deleted, because the reason our
   sheet cannot be the floor is itself a result about our generator.

4. **The refuter's synthetic grid was hard-coded to 2048**, so it worked at mip
   0 (512 x 4) and raised a broadcast error at mip 1. It crashed rather than
   producing a wrong number, and the mip-1 file was regenerated after the fix
   (`n = base.shape[0] * factor`). Rule: a control's size is derived from the
   subject's, never written as a constant.

All four are visible failures, not silent ones; none reached a number in
sections 3 or 4, and every output file in section 1 was regenerated from the
final script and reproduces the tables exactly. No `src/` file, build,
commit or GUI was touched.

*(Constitution rule 2: these belong in `MISTAKES.md` at the repo root. This lane
may not write outside `scratchpad/mountains_20260907/` and this report, so the
director splices them.)*

---

## 6. Finished-work skill review

**Loaded:** `nifskope-ww-lodgen` — used for the `--dump-layers` semantics and
the corpus paths, and for the standing traps (Windows paths in Python, heredoc
backslashes, the exe/game gate). It saved re-deriving where `layers.txt` came
from and what its columns mean. `nif` was not loaded: no mesh was touched.

**Re-derived from first principles, and it should not have been:** the design of
a *calibrated control pair* for a measurement against vanilla. This lane spent
most of its turns on it, and every step is generic, not `_msn`-specific:

* the floor must carry the SIGNAL's own amplitude and spectrum through the same
  lossy pipeline — a control that skips the codec is not a floor;
* it is matched on the quantity the metric reads, never on a convenient proxy;
* a paired floor built from the subject's own data needs an independent
  phase-randomised twin beside it, or it can be called self-serving;
* the codec is emulated and then **checked on the subject's own bytes** before
  any control uses it;
* the ceiling comes from the same data with the property removed, so
  ceiling/floor measures the test's power and not the data's;
* the metric is first run on inputs whose answer is known (an exact gradient,
  white noise) — a check that cannot fail on its input is not a check.

That is CONSTITUTION rule 4's "an invariant that fails on broken code, and show
it failing" turned into a procedure. It will be needed again for every "is our
output as good as vanilla's" question on this thread.

**Written:** a draft at
`scratchpad/mountains_20260907/skill_draft_control_calibration.md`, ready to
install as `E:\Projects\Claude\.claude\skills\ww-control-calibration\SKILL.md`
and mirrored into `<repo>/.claude/skills` (rule 1a, the two trees drift).
**This lane could not write it into either tree** — the brief limits writes to
`scratchpad/mountains_20260907/` and this report — so it is handed to the
director rather than installed. That is the only reason it is a draft.

**Declined:** a skill for the `_msn` decode path itself (`dds.py` + `bcnp.py` +
the channel convention). It is three lines of import and one sentence of
convention, already stated at the top of `msn_curl.py` and in WW_CHANGES; a
skill would cost more to read than to re-derive.
