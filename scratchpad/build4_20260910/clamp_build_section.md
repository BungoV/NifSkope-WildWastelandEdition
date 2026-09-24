## Build (BUILD4)

Built and gated by lane BUILD4 on 2026-09-10. Appended, not rewritten; the
lane's own text above is untouched. Skills loaded: `nifskope-ww-resume-pending`,
`nifskope-ww-build-verify`, `nifskope-ww-lodgen`, `ww-texel-picture`.

### B.1 The build

`Fallout4.exe` and `NifSkope.exe` both absent (`rc=1`) before the build and
before every exe launch below.

`qmake NifSkope.pro` then `make -j2`, per `nifskope-ww-resume-pending` §3 --
qmake first because lane WATER2 added a NEW `#include "esmfile.hpp"` to
`src/lodtfile.cpp`, which a frozen dependency list cannot see.

| step | result |
|---|---|
| `QMAKE-RC` | 0 |
| `BUILD-RC` | 0, `make[1]: Nothing to be done for 'first'` |
| staleness sweep, all 15 changed files under `src/ res/ tools/ tests/` | 0 STALE |
| `cmp res/style.qss release/style.qss` | in step |

**The exe already carried this lane's code.** Lane WATER2's link at 01:01:04
post-dates every changed source, `src/lodgen.cpp` at 00:40:11 included, and the
object-level check confirms it rather than inferring it: `lodgen.o` 00:44:00,
`nifskope_ui.o` 00:44:24, `lodtfile.o` 01:01:01, and every one of the 31 objects
that include `glview.h`, the 5 that include `lodtfile.h` and the 2 that include
`btdterrain.h` is newer than its header. So no relink was owed and none happened.

**One finding, recorded in `MISTAKES.md`:** even AFTER the qmake run,
`Makefile.Release`'s dependency block for `lodtfile.o` names only
`src/lodtfile.cpp src/lodtfile.h src/esmdata.h src/io/lodvfile.h` -- not
`lib/libfo76utils/src/esmfile.hpp`. qmake's scan does not follow into `lib/`.
Harmless today (the object is 01:01:01 and the header is 2026-08-31), latent
tomorrow.

### B.2 Every clock in one table (CONSTITUTION 4)

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | 2026-09-10 01:01:04 |
| `release/style.qss` | 2026-09-10 01:01:04 |
| `src/lodgen.cpp` | 2026-09-10 00:40:11 |
| `tests/spells/lodgen_terrain_vt.sh` | 2026-09-10 00:22:51 |
| `docs/LODGEN_TERRAIN_VT.md` | 2026-09-10 00:26:13 |
| `scratchpad/clamp_20260910/before/.../-24.24.DDS` | 2026-09-10 00:16:46 |
| `scratchpad/clamp_20260910/after/.../-24.24.DDS` | 2026-09-10 01:07:31 |
| `scratchpad/clamp_20260910/edgeband_cover.txt` | 2026-09-10 01:07:50 |
| `scratchpad/clamp_20260910/logs/terrain_vt.log` | 2026-09-10 01:09:16 |
| `scratchpad/build4_20260910/land.bin` | 2026-09-10 01:11:08 |

### B.3 The gate table, against section 3's pre-registration

Both AFTER bakes ran with ABSOLUTE `--out-dir` and `--tex-dir` (mistake 2 of the
lane's own section 5), rc 0 each, **24 of 24** `.DDS` written.

| gate | bar | measured | verdict |
|---|---|---|---|
| V9a, tint OFF | byte-identical, 4 chunks | byte-identical | ok |
| V9a, tint ON | byte-identical, 4 chunks | byte-identical | ok |
| V9b, `_msn` | byte-identical, 4 chunks | byte-identical | ok |
| the FLOOR under both | >= 2 of 8 pairs differ | **8 of 8** | ok |
| V9c E/W ratio | <= 3.20 (ringed 2.75, clamped 4.07) | **2.75**, interior 1.803 | ok |
| V9c N/S ratio | <= 3.30 (2.87 / 3.78) | **2.87**, interior 1.603 | ok |
| V9c edge step | <= 2.60 (1.961 / 3.310) | **1.961** | ok |
| `lodgen_terrain_vt.sh` | RESULT PASS, 32 -> 35 checks | **35 checks, 0 failures, PASS** | ok |
| `lodgen_terrain.sh` | PASS, pyramid stats unchanged | **26 checks, 0 failures**; assembled and direct both `UP=G D0=76 D1=32 D2=51 D3=32`; vanilla `99/67/67/67` | ok |
| `lodgen_identity.sh` | RESULT PASS, baseline unmoved | **8 ok, RESULT PASS** | ok |
| `edgeband.py` colour, cover | <= 4 texels, 0 beyond, must move | 127 moved, maxd 3, **0 beyond** | ok |
| `edgeband.py` `_msn`, cover | <= 4 texels, 0 beyond | 25,537 moved; maxd 3 and 0 beyond on the y=24 chunks; **maxd 7, 1,014 and 1,405 beyond** on the y=28 chunks | **MISS** |
| `edgeband.py` `_data`, cover | <= 64 texels, 0 beyond | 65,872 moved; maxd 47, 0 beyond on three chunks; **maxd 79, 16 beyond** on `-20.28` | **MISS** |
| `edgeband.py` colour, cover-free | byte-identical | **byte-identical, all four chunks** | ok |
| `edgeband.py` `_msn`/`_data`, cover-free | move inside their bands | identical readings to the cover run | as cover |

V9c's three numbers landed on the pre-registration to three significant figures
(2.75 / 2.87 / 1.961 predicted, 2.75 / 2.87 / 1.961 measured), which is the
strongest single result of the round: the direct sheets now reproduce lane
VTFIX's RINGED readings because they are the same bytes.

The FLOOR line `the colour sheets did not move at all: the ring is inert` prints
on the **cover-free** run and that is by construction -- §4's second control
asks the colour sheets NOT to move there. On the cover run, which is what the
floor guards, it stayed silent (127 texels moved).

### B.4 The new baseline, beside the numbers that justify it

The twelve `sha256[:16]` of the cover AFTER sheets are in
`scratchpad/clamp_20260910/edgeband_cover.txt` and are copied into
`WW_CHANGES.md` next to the band table, never alone (CONSTITUTION 4).

### B.5 The two misses, cause MEASURED, neither fixed nor re-pinned

`nifskope-ww-resume-pending` §6: a resuming lane's product is a verdict.

**Localised** (`scratchpad/build4_20260910/localise.py`, which reuses
`edgeband.py`'s decoder rather than writing a fourth one):

| sheet | beyond-band texels, by nearest border | distance histogram |
|---|---|---|
| `-24.28 _msn` | W=10 E=10 **N=994** S=0 | 4:290 5:298 6:241 7:185 |
| `-20.28 _msn` | W=0 E=0 **N=1,405** S=0 | 4:388 5:394 6:347 7:276 |
| `-20.28 _data` | E=16 | 76:4 77:4 78:4 79:4 -- exactly one 4x4 BC block |
| `-24.24 _msn` (a PASSING chunk, as control) | W=242 E=277 N=262 S=218 | 3:999 -- one distance only |

**The cause, from the MASTER and not from our output**
(`scratchpad/build4_20260910/seamcheck.py` over `--dump-land`, cells
x=-24..-17): Bethesda's landscape disagrees across exactly ONE shared vertex row
in this neighbourhood.

| shared row | max &#124;difference&#124;, VHGT units of 8 |
|---|---|
| y=23 &#124; y=24 | 0 across all eight columns |
| y=27 &#124; y=28 | 0 across all eight columns |
| **y=31 &#124; y=32** | **2, 1, 4, 6, 9, 8, 7, 4** (16..72 world units) |
| y=32 &#124; y=33 | 0 across all eight columns |
| both east seams, x=-21 and x=-17, y=24..31 | 0 |

`lodgenTerrainFillRing`'s documented south-to-north order therefore lets the
y=32 cell overwrite the row it shares with y=31 -- which is the chunk's OWN
boundary grid row. Everywhere else the two cells agree exactly, so the ring can
only change points OUTSIDE the chunk.

The reach then follows arithmetically, and the two histograms are its two cases:
a texel at distance `t` reads heights at `t +- 4` texels through a bilinear tap,
so grid index -1 is read out to **t = 3** (every passing border: one distance,
3) and grid index 0 out to **t = 7** (the north border: 4,5,6,7 and nothing
further). **The pre-registered band of 4 counted the central difference's
128-unit step but not the bilinear tap's own 128-unit footprint, and assumed the
ring could only touch points outside the chunk.** Section 0's own words bind
here: the band is not to be adjusted to fit what came back, so it is reported
missed and left for the lane or the director.

The 16 `_data` texels are one BC block, and the mechanism is already owed item 2
above: `chgt` copies the ring's middle, whose north boundary row moved, and
wetness is a flow accumulation over that whole grid -- a global operator, so it
has no band at all. Nothing new was learned; the outlier is that defect showing
through.

### B.6 What was NOT done

* **Nothing was committed** (CONSTITUTION 8).
* No fix was landed for either miss, and no band was re-pinned.
* `scratchpad/clamp_20260910/before/` is kept -- two gates are red, and it is
  the only picture of the clamped behaviour there will ever be (`§8` of
  `nifskope-ww-resume-pending`). `ns_before/` is kept for the same reason.
* Harnesses NOT run, with the reason: everything in `tests/spells` outside the
  three above. The change is confined to the terrain bake in `src/lodgen.cpp`;
  it reaches no NIF writer, no panel, no collision, no `.lodl`/`.lodt` reader.
* bungo's open NifSkope window predates the exe and needs a restart.
