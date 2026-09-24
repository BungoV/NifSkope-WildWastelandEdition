# Lane WATER2 — the `.lodl` version-3 water writer and reader

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main` at `720762a`.
**Nothing committed** (CONSTITUTION 8).

Read first: `CONSTITUTION.md`, `HANDOFF.md`, `scratchpad/specs_20260909/spec_water.md`
in full, `scratchpad/lane_water1_report.md`, `docs/LODGEN_BTD_FORMAT.md`, and the
`ww-lodl-offline-census` skill.

---

## 0. The headline, before the detail

The version-3 writer, reader, CLI and viewer planes are built and gated. **The
census does NOT reproduce lane WATER1's 590 bodies, and that is not a bug in
this lane** — WATER1's rule-D bridge compared DECIMATED point clouds, which can
only overestimate a distance, and it missed 327 of the 545 texel pairs that are
actually within two texels. Making the test exact, as rule D's own words say
("two components at the same height whose SHORES are within 2 texels"), then
exposed a second defect in the rule, which would have handed the Commonwealth's
entire ocean to `ExtMarshScumWater`. Both are measured, both are argued in §2,
and the corrected rule gives **346 bodies**.

Everything else the brief asked for holds: the version-2 bytes are untouched,
both refusals fire by name, the known-answer control passes with its refuter,
an independent decoder agrees with the writer, and the three planes are visible
in the viewer.

---

## 1. What was built

| file | what |
|---|---|
| `src/lodtfile.h` | `LODL_SECT_*` bits (now public), `LodtWaterBody`, `LodtWaterOptions`, the reader's version-3 accessors and its plane-store members |
| `src/lodtfile.cpp` | version 3 and the header-size TABLE; the whole water pass (rule D, flow, the three planes, the stroke store); the version-3 reader with its four named refusals; `lodtWaterCensus`; `lodtWaterSelfTest` |
| `src/nifcli.cpp` | `--water-bodies` and its six switches on `lodgen`; `--water-census` and `--water-selftest` on `lodl` |
| `src/btdterrain.h` / `.cpp` | the three new plane keys `bodyid` / `flow` / `shore`, their labels, their availability from the section BITS, and the code that paints them |
| `tests/spells/lodl_water.sh` | gates G1..G9 |
| `docs/LODGEN_BTD_FORMAT.md` | the version-3 contract |
| `scratchpad/water2_20260909/` | the oracle, the independent decoder, the corruption tool and the measurement scripts |

---

## 2. The census does not reproduce 590, and why that is the right answer

The brief and the spec's gate G5 pin lane WATER1's numbers: **590 bodies, 215
bridge merges accepted, 3 refused, 1 sea / 131 river / 458 lake**. This lane
produces **346 bodies, 528 accepted, 13 refused, 1 sea / 115 river / 230 lake**.
Three separate findings account for the difference. Each was measured with a
script kept in `scratchpad/water2_20260909/`; none is a preference.

### 2.1 WATER1's shore test was DECIMATED, and a decimated distance is one-sided

`scratchpad/water_20260909/final_census.py` states rule D's bridge as *"two
components at the same height whose shores are within 2 texels"* and implements
it by comparing point clouds thinned to at most 4,000 points a body
(`bx[::area // 4000]`). For the sea that is 4,000 points out of 21,585,117
texels. **A thinned point set can only make a minimum distance LARGER**, so
every disagreement is a merge the stated rule requires and the implementation
missed.

`bridge_exact.py` recomputes the same step by scanning the disc of radius 2
around every texel — exact, one pass:

| | pairs within 2 texels | accepted | refused | bodies |
|---|---|---|---|---|
| WATER1, decimated | 218 | 215 | 3 | 590 |
| exact | **545** | 533 | 7 | 340 |

### 2.2 The exact test then exposed a missing DIRECTION in the merge rule

At 340 bodies the count looked like an improvement. `bridge_effect.py` printed
the resulting TABLE instead, and the biggest body was:

```
21587443 texels  ExtMarshScumWater  height 450.0  parts 332  (5 distinct forms)
```

The Commonwealth's ocean, carrying a marsh's name — because the bridge accepts
"their types are equal **or one inherits**" in both directions, so the 21.5 M
texel inheriting sea absorbed a painted marsh that passes within two texels of
it, and the merged body's form is "the most common PAINTED type".

Rule C's adjacent merge already states the direction: *"a component whose type
is the worldspace default is merged **into** the same-height PAINTED component
it touches"*. The bridge is that merge across a gap and needs the same
direction, plus the clause that makes it safe: **the inheriting side may only be
absorbed when it is the SMALLER of the two.** `bridge_variants.py` measured
three readings; the numbers are in §2.4.

### 2.3 The known-answer control found the same defect in rule C itself

The synthetic control (`lodl --water-selftest`, §4) failed on its first run and
named the cause: the sea, 73,728 texels, TOUCHES the tidal step of a painted
512-texel river at exactly its own height, so rule C's *adjacent* merge absorbed
the sea into the river and the ocean came out with the river's form. The guard
belongs in both merges, not just the bridge.

`merge_guard_variants.py` measured three candidate guards, in both merges, on
the Commonwealth. `EDGE` = an inheriting component that reaches the worldspace
edge is never absorbed (principle: the ocean is not an unpainted reach of
anything). `SIZE` = it is never absorbed when it is the larger. `BOTH` = neither.

| guard | rule C | bodies | accepted | refused | bodies where the two readings of the form rule disagree |
|---|---|---|---|---|---|
| none | 792 | 340 | 533 | 7 | **2**, the largest 21,587,443 texels |
| EDGE | 793 | 345 | 529 | 12 | 1 |
| **SIZE** | **793** | **346** | **528** | **13** | **0** |
| BOTH | 793 | 346 | 528 | 13 | 0 |

`SIZE` is what shipped, and it is chosen on a measurement, not a preference:
it is the only single clause under which "the painted type with the most area"
and "the majority type counting inherited area" name the **same** form for every
one of the 346 bodies. `BOTH` is identical to it on this worldspace, so the
edge clause buys nothing here and is not carried.

### 2.4 What the corrected rule gives, and what did NOT move

```
805 components -> 793 rule-C bodies (12 merged, 1 refused, 1 ambiguous)
             -> 346 bodies, 528 bridge merges accepted, 13 refused
class          sea 1, river 115, lake 230
area >= 64     89 bodies (sea 1, river 42, lake 46)
TINY (< 4)     115
flow source    none 132, form NAM0 182, bed 2, drain 30
```

**The per-form TEXEL totals are the invariant, and they are unchanged from lane
WATER1's read-only census, form for form** — no grouping decision can move how
many texels carry a given painted type. `ExtMarshDarkWater` 44,040,
`ExtMarshScumWater` 26,410, `ExtRiverCharlesUpper` 25,214,
`ExtLakeForestWater` 23,662, `ExtCreekSanctuaryWater` 10,666,
`ExtRiverNFoothillsWaterSE` 8,426, `ExtLakeQuannapowittWater` 3,696,
`ExtMurkyWater` 1,427, `ExtLakeIrradiatedWater` 1,400,
`ExtGlowingSeaWater01` 1,203, `ExtRiverNFoothillsWaterSW` 677,
`ExtPuddleWater` 373, `ExtCreekSanctuaryWaterE` 251 — all identical.
Two moved, and the guard is why: `ExtOceanWater` 21,594,553 → 21,583,510 and
`ExtLakeWater` 12,960 → 24,003. The 11,043 texels that changed hands are a
painted lake that the ungoverned rule-C merge had handed to ocean-plane water.

**The Charles is still ONE body**: 25,114 texels over cells (−16..−6, −21..−4),
against WATER1's 25,112 over the same cells — two texels the exact shore test
picked up. That was the property rule D was chosen for, and it survives.

---

## 3. The design as built

### 3.1 Version 3, and what did NOT move

`0x00..0x9F` holds exactly the fields version 2 holds. The header grows by 88
bytes (`0xA0` → `0xF8`), so every section and every absolute block-payload
offset in the directory **slides by exactly 88** and nothing else moves — the
same discipline, and the same gate, as the version 1 → 2 step. Version 3's own
sections are appended AFTER the block data, so the whole version-2 span is
copied through byte for byte.

**The header size is a TABLE now** (`lodtHeaderBytes`), and the version refusal
runs BEFORE any offset is read. The old `ver >= 2 ? V2 : V1` ternary ran before
the version check, so a version-3 file would have been measured against version
2's 160-byte floor by the very reader that was about to refuse it. The spec
called this out as a thing to fix WITH the bump; it is fixed, and the harness
greps for the ternary's absence so it cannot come back.

**Only the water module raises the version.** `--water-bodies` defaults off; a
run that does not ask for bodies writes version 2 and the bytes it always wrote.

### 3.2 The sections

| section | what | Commonwealth |
|---|---|---|
| body table | 48 bytes a record, ID order, `id == index + 1` or refuse | 346 records, 16,608 bytes |
| body-ID plane | uint16 a texel, one tile a cell, 32 a cell | 693,671 bytes, 34,958 of 36,864 tiles uniform |
| flow plane | uint16: dir 8 / speed 4 / confidence 4 | 628,750 bytes, 36,291 uniform |
| shore plane | uint8, 32 world units a step | 1,319,623 bytes, 33,334 uniform |
| stroke store | present and EMPTY: a count of zero | 4 bytes |

All three planes share ONE tiled zlib container, so there is one implementation
and one gate. **A tile whose compressed size is 0 is uniform and its
uncompressed-size field holds the repeated sample** — 99.2% of the wet area is
one body, so most tiles cost sixteen bytes instead of an inflate.

**The stroke store ships present and empty on purpose.** A count of zero is not
the same as an absent section: a consumer can tell "nobody has marked anything"
from "this file predates marking", and lane WATER3's tool can write into a store
that already exists.

### 3.3 Flow

Four sources, first one to answer wins, and the body record's `flow source` byte
NAMES which served (CONSTITUTION rule 10 — a fallback is never silent). The
direction comes from the rule; the magnitude always comes from the form's
`NAM0`, because that is the only speed vanilla carries. With no strokes the
field is constant over a body and equal to its mean, so the automatic case costs
no solve at all and every tile is uniform.

`EsmWorld` exposes no `WATR` accessor and this lane does not own `esmdata.*`,
so the writer opens the plugin a second time through `ESMFile`, only under
`--water-bodies` and only when a path is supplied (the `lodgen` command's own
ESM argument). With no plugin the arm is simply ABSENT and the census says so;
nothing is invented. The twenty-line accessor that retires this is written out
in `scratchpad/water2_20260909/ESMDATA_CHANGE_NEEDED.md`.

### 3.4 Reader and viewer

The reader refuses by name on: a section bit over an empty rate or offset, a
record stride SHORTER than the 48 it knows (a LONGER one is the
forward-compatible case and strides past the fields it does not know), a record
whose `id` is not its index + 1, and a plane naming a body past the table.
Every UNIFORM tile's id is checked at open — free, it is in the directory, and
it covers the whole of the sea; the compressed tiles are checked by
`--water-census`, because inflating 36,864 tiles would make every `File > Open`
pay for a verification.

The viewer gains `--plane bodyid`, `flow` and `shore`, offered from the section
BITS. Body ID is sampled NEAREST: an id is a name, and the average of two names
is a third body that does not exist.

---

## 4. The gates

`tests/spells/lodl_water.sh`: **56 checks, 0 failures, PASS.**

| gate | result |
|---|---|
| G1 module OFF byte-identical | 35,953,294 bytes and `cmp`-identical to the shipped `Commonwealth.lodl` |
| G2 the fallback | `WW_LODL_VERSION=2` with the module ON is byte-identical to G1's file |
| G3 refusal 1 | a hand-made version-4 file: *"unsupported version 4 (this reader knows 1..3)"*; the control (the same file at version 3) still opens; the header size is a table and the ternary is gone |
| G4 refusal 2 | all four provoked on hand-corrupted files, each with its own sentence |
| G5 the census | the C++ agrees with the Python oracle on the body count, the class split, the number of forms, the per-form BODY counts, the per-form TEXEL totals form for form, and the total wet area |
| G6 the control | **failed on its first run and found a real defect** (§2.3); passes now, with the type-blind refuter printed beside it |
| G7 independent decoder | `scratchpad/water2_20260909/lodl_v3_authority.py`, sharing no code with `src/lodtfile.cpp`: every record checked, the WHOLE body-ID plane swept (0 ids past the table), every body's `area` field equal to its own texel count in the plane, the flow word decoding to the body's direction on all 214 flowing bodies, 10,000 random texels, and the shore plane measured to VARY |
| G8 round-trip | two runs of the writer are byte-identical; the stroke store reads back as a count of zero; 48,960 directory entries all moved by exactly 88 and no size moved; identical either side of the directory |
| G9 cost | reported below, not gated |

Neighbouring harnesses, all re-run on the FINAL exe (2026-09-10 01:01:04) and
all PASS: `lodl_write.sh`, `lodl_open.sh` (23 checks, 0 failures),
`lodgen_terrain.sh` (26 checks, 0 failures), `lodgen_identity.sh`. Outputs in
`scratchpad/water2_20260909/gate_*.txt`.

**G9, the cost.** Version 2 35,953,294 bytes → version 3 38,612,038, so the
water sections are **2,658,744 bytes, 7.4% of the file**, for a per-texel body
id, a per-texel flow word and a per-texel shore distance over 37.7 M samples —
which is what the uniform tile buys. The version-3 write took **6 s** wall
against version 2's own reported 4.7 s of phases.

**G4's stride case is not what the spec's §3.8 says, and the spec is wrong
there.** §3.3 states the forward-compatibility rule correctly (a reader whose
record is shorter than the file's strides by the file's value and reads the
prefix it knows); §3.8 then lists the refusal as *"`bodyRecordBytes` larger than
this reader's record"*, which is the opposite and would make the field useless.
The reader refuses on a SHORTER record — one that is missing fields it needs —
and the harness provokes it that way.

---

## 5. The pictures

`scratchpad/water2_20260909/images/charles_four_planes.png` — the Charles,
cells (−16..−6, −21..−4), ONE framing (top view, flat, LOD 0), four planes, and
the only thing that differs between the panels is `WW_LODL_PLANE`. Each caption
carries the number the builder itself printed for that plane.

* **`watertype`** — what version 2 could say. Blocky, per CELL, painted across
  dry land as well; 198 of 198 cells have water and every lake sharing a form
  shares a colour.
* **`bodyid`** — 28,078 of 203,681 samples name a body, 21 distinct in this
  region, highest id 314. **The Charles is one continuous body over the whole
  reach**, and the lake to its north-east is a different colour. That is the
  picture that answers *"or at least an ID for them"* on sight.
* **`flow`** — 27,532 of 28,078 wet samples carry a direction; the Charles and
  the lake read as two different hues, which is two different directions from
  two different rules.
* **`shore`** — steps 4..88 at 32 world units a step, and the gradient from bank
  to mid-channel is visible; it is per BODY, so two bodies that touch each keep
  their own bank.

The four single renders are beside the sheet with their build logs.
`.gitignore` gained one line so this folder's PNGs are not excluded — the same
allow-list pattern the four existing picture folders already use.

---

## 6. What is NOT done, and what is owed

* **No marking tool.** Lane WATER3 owns it. Nothing writes a stroke, so no body
  carries flow source 4, a colour override, a name, or a hand-set class, and
  the propagation solve of spec §4.3 is not implemented — with no constraints
  it is the constant field the writer already emits, which is why the automatic
  case is complete without it.
* **`EsmWorld` has no `WATR` accessor** — `ESMDATA_CHANGE_NEEDED.md`.
* **Nothing has been flown in a consumer.** FO4CS pins `kVersion = 1` and
  refuses version 2 already; version 3 does not make that worse and the
  zero-effort fallback is unchanged.
* **The spec's G5 numbers are superseded** (§2). The spec page itself was not
  edited — it is lane WATER1's deliverable and this lane does not own it. What
  it needs: §2's bridge clause, §3.8's stride refusal reversed, §4.1 step 6's
  ID ordering (which IS implemented as stated: descending area), and §7's G5/G6
  numbers.
* **A grid-resident refusal, unexercised.** The water pass needs the whole
  sample grid in memory and refuses above 268 M samples — the Commonwealth is
  37.7 M, an 804-cell Fallout 76 port would be 662 M. The refusal has never
  been provoked, because no worldspace that large exists here to provoke it
  with, and it is named as unvalidated.
* **The `.btd` path.** `lodtWriteBtd` never sets `LodtWaterOptions::enabled`, so
  a converted Appalachia writes version 2. That is correct today (a `.btd`
  carries no water at all) and it is untested with `waterFrom`.

---

## 7. Mistakes

Four entries, spliced into `MISTAKES.md` (newest first) and kept in
`scratchpad/water2_20260909/MISTAKES_ENTRIES.md`:

1. **a decimated point cloud shipped as a distance RULE** — WATER1's, found
   here; the rule that prevents it is to name a measurement's approximation
   beside the number, or run it once without one, before pinning it as a gate.
2. **a merge rule with no DIRECTION gave the ocean a marsh's name** — the count
   looked like an improvement and the TABLE showed what it had cost. A count is
   not a result.
3. **a heredoc mangled `\n` into a real newline, and the build paid** — the
   third time `nifskope-ww-build-verify`'s "patch with a script file" rule has
   been paid for. Every later patch in this lane went through a `fixNN.py`.
4. **six most-vexing parses in one file** — `std::vector<T> v( size_t( n ) );`
   declares a function, and the errors land on the first USE of the name.

---

## 8. Skill review (CONSTITUTION rule 1a, the finished-work review)

**Loaded and used.** `ww-lodl-offline-census` — its "do not write a third
reader" rule is why the oracle imports lane WATER1's decoder and its bulk path
instead of restating the format, and its "there is no scipy on this machine"
section is why the C++ labeller is run-length + union-find over RUNS rather than
a texel-wise flood fill (6,144² in about a second, which is what made two full
passes over the world affordable). `nifskope-ww-build-verify` — the gated chain,
make's own exit code, the exe renamed aside, and above all its **"a successful
build is not a consistent one"** section: `btdterrain.cpp` began including
`lodtfile.h` before this lane and `Makefile.Release` did not know it, so the
`qmake6` run before the first build was that section's advice taken, not a
guess. `nifskope-ww-render-shot` — the conditional-environment-variable trap
(`env NAME=1 ...`, never a bare `${flag:+NAME=1}` prefix) and "every shot
function prints the size of the file it wrote" are both in the render loop.
`nifskope-ww-lodgen` — the corpus paths and the CLI shape the new switches fit.
`ww-control-calibration` — step 1 is why the known-answer control ran first,
and it is the reason this lane found the rule defect at all.

**Written this session: `ww-cxx-vexing-parse` — DECLINED**, and the reason is
that it is not a procedure, it is one line in a style note. The six failures
here were one mistake repeated in one sitting, and the fix ("give a
single-argument container construction its fill value, or use `resize`") is
already stated where it belongs: in `MISTAKES.md`, beside the class of error it
belongs to.

**Written this session: `ww-spec-gate-audit`.** This IS a procedure, it cost
this lane about a third of its time, and it will recur every time a read-only
measurement lane hands a build lane a pre-registered number. It is at
`E:\Projects\Claude\.claude\skills\ww-spec-gate-audit\SKILL.md` and, per the
two-tree drift rule, at `.claude/skills/ww-spec-gate-audit/SKILL.md` in this
repo.

**Wished for and NOT written.** A skill for "compare a C++ census against a
Python oracle line for line" — declined: the comparison is four regexes against
one file, `compare_census.py` is that comparison, and the general shape ("agree
on the invariant that no grouping decision can move") is one sentence, already
in `ww-control-calibration`'s territory.

---

## 9. Housekeeping

Nothing committed. Files changed: `src/lodtfile.cpp`, `src/lodtfile.h`,
`src/nifcli.cpp`, `src/btdterrain.cpp`, `src/btdterrain.h`,
`tests/spells/lodl_water.sh` (new), `docs/LODGEN_BTD_FORMAT.md`,
`WW_CHANGES.md`, `MISTAKES.md`, `.gitignore` (one allow-list line), and
`scratchpad/water2_20260909/`.

Line endings measured with Python byte counts, not grep: every `src/` file and
`docs/LODGEN_BTD_FORMAT.md` stayed at 0 CR; `MISTAKES.md` 0 → 0; `WW_CHANGES.md`
19,020 → 19,020 CR, unchanged, its mixed pattern intact.

`scratchpad/water2_20260909/` is **773 KB** without `out/`, which holds the
38 MB version-3 `Commonwealth.lodl` the pictures were rendered from — that is
`.gitignore`d (`scratchpad/**/*.lodl`), regenerates in six seconds, and lane
WATER3 will want it. The oracle's 75 MB `body_id_plane.npy` was DELETED for the
same reason WATER1 deleted its own: `.npy` is not in the scratchpad exclusions
and a cached DERIVED array outlives the rule that produced it. It regenerates
with the oracle.

To reproduce everything from the two masters:

```
cd scratchpad/water2_20260909
python census_water2.py          # the oracle: ~4 min, writes census_water2.txt/.json
python bridge_exact.py           # WATER1's decimated bridge against an exact one
python bridge_effect.py          # what the exact bridge does to the body TABLE
python bridge_variants.py        # the three readings of the bridge clause
python merge_guard_variants.py   # the three guards, in BOTH merges, end to end
cd ../..
bash tests/spells/lodl_water.sh  # 56 checks
python scratchpad/water2_20260909/make_sheet.py
```

`bridge_exact.py` and `bridge_effect.py` still import lane WATER1's `labC.npy`
(`../water_20260909/analyse_bodies.py` writes it) because their whole job is to
compare against WATER1's own rule C. The ORACLE does not: it builds rule C
itself, because rule C's merge needed the guard and WATER1's script does not
have it.
