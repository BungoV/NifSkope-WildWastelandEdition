#!/usr/bin/env python3
"""Lane BUILD7: append its `## Build (BUILD7)` section to the HKX2 and HKX5 lane
reports. Append-only, LF-only, asserted both ways. The lanes' own text is never
rewritten (`nifskope-ww-resume-pending` rule 7.3)."""
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

MTIMES = """
| artefact | mtime | bytes |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-10 14:04:34 | 19,197,952 |
| `release/style.qss` | 2026-09-10 14:04:35 | 11,097 (compares equal to `res/style.qss`) |
| `release/hkxanim_dump.exe` | 2026-09-10 14:05:13 | 526,009 |
| `release/hkxwrite_dump.exe` | 2026-09-10 13:41:42 | 795,378 (lane HKX5's, not rebuilt) |
| `fixtures/human_male_vanilla.nif` | 2026-09-10 05:29:52 | 338,563 |
| `scratchpad/build7_20260910/frames_jog.png` | 2026-09-10 14:14:09 | 602,781 |
| `scratchpad/build7_20260910/frames_mixamo.png` | 2026-09-10 14:14:09 | 626,212 |
"""

HKX2 = """

## Build (BUILD7)

Built and gated by lane BUILD7, 2026-09-10, on `main`, **nothing committed**.
`Fallout4.exe` and `NifSkope.exe` were both down at the build and before every
one of the 15 exe launches (`rc=1` each time).

**The build.** `qmake NifSkope.pro` RC=0, then `make -j2` RC=0 (a full rebuild:
qmake regenerating the Makefile invalidated every object). The dependency
read-back the resume asks for, per object rather than by `grep -A`:
`hkxplayback.h` is named by `glnode.o`, `glscene.o`, `nifskope_ui.o`,
`hkxplayback.o` and `hkxplaybacktest.o` -- all five -- and `hkxanim.h` by those
five plus `hkxanim.o`, `gltfimport.o` and `hkxwrite.o`. `release/NifSkope.exe`
is newer than all **70** changed files under `src/ res/ tools/ tests/`.
`res/style.qss` and `release/style.qss` compare equal.

**Lane HKX5's hook-up was applied in this build** (it is a refusing script and
had to be): `scratchpad/build7_20260910/apply_hookup.py` imports HKX5's own
`EDITS` table rather than retyping the anchors, and put
`src/gltfimport.{h,cpp}` and `src/hkxwrite.{h,cpp}` into `NifSkope.pro` --
18,779 -> 18,859 bytes, LF 717 -> 721, CR 0 unchanged. Lane HKX4b's
`src/gltfexport.*` was NOT touched and is NOT in the `.pro`.

### The gates

| gate | expected | measured | verdict |
|---|---|---|---|
| `hkxanim_gates.py` | 134 checks / 3 fixture failures | 134 / 3 | **PASS** |
| `hkxanim_synthetic.py` | 29 / 0 | 29 / 0 | **PASS** |
| `hkxanim_mutate.py` | 20 / 0 | 20 / 0 | **PASS** |
| (g) identity rule, C++ | 8,835 rows, `bone == track` on all | 8,835 rows, 0 mismatches; no refusal | **PASS** |
| (g) `identity_floor.py` | 6 / 6 | 6 / 6 | **PASS** |
| (a)(b)(c)(d)(f) `hkxanim_play.sh`, `skeleton.nif` | PASS | 27 checks, 0 failures | **PASS** |
| (a)(b)(c)(d)(f) `hkxanim_play.sh`, `human_male_vanilla.nif` | 78 / 17 / 4 | 27 checks, 0 failures, 78 / 17 / 4 | **PASS** |
| (e) pictures, `jog` | 4 PNGs, >= 3 distinct | 4 of 4 | **PASS** |
| (e) pictures, Mixamo | 4 PNGs, >= 3 distinct | 3 of 4 | **PASS, with a caveat below** |

The three failures inside `hkxanim_gates.py` are the pre-registered fixture
ones, named: the 17 `Weapon*` bones with no node in `skeleton.nif`; the
`weapon` bone's reference-pose translation at 0.00148 against a 1e-3 bar; and
the furniture `Tpose` clip not being the bind pose on 79 bones.

Numbers worth keeping from `hkxanim_play.sh` (they are the ones the resume's
refuters turn on): (a) worst translation **0**, rotation **7.64e-06 deg**,
scale **0** over 234 comparisons at frames 0, 11 and 22, with `scene t=` equal
to the asked-for time at every frame; (a floor) the same test against the wrong
frame goes red at 5.85e-05 / **0.218 deg**; (b) **0 of 129** nodes differ after
unload with (b floor) **78** differing while posed; (d) the road sign loads,
`setActive` returns false, the sentence contains "does not play", **0 of 2**
nodes change.

### (e)'s caveat, and the frames bungo asked for

Gate (e) passes on the Mixamo clip at 3 distinct of 4, but the two that match
are **5,052 bytes of empty background**. Cause, measured: that clip's COM track
travels **487 units in +Y** (frame 0 y=-0.7, frame 92 y=487.0) while its
root-motion channel is all zero, and gate (e)'s pinned camera is the 35-degree
PERSPECTIVE one at `dist=260` -- so by t=0.767 the figure is 750+ units from
the eye. Not a defect in the playback; a defect in that framing for that clip.

The frames for bungo were therefore rendered on an **orthographic** front
camera, where the travel runs along the view axis and changes nothing:
`scratchpad/build7_20260910/frames.sh`, the pin read back at every grab as
`arm=center/ortho/view view=5 lookat=0,0,62 halfW=80 halfH=70.6854 persp=0
vp=1065x941 upp=0.150235`. The same pin served all 13 pictures, and the two
sheets' bind-pose tiles are byte-identical (md5
`580245fba8bad849de9c5f0a7e9f44bb`) -- that identity is the pin's own proof.
6 of 6 tiles differ within each sheet.

* `scratchpad/build7_20260910/frames_jog.png` -- bind pose + frames 0, 6, 11,
  17, 22 of `jog.hkx`.
* `scratchpad/build7_20260910/frames_mixamo.png` -- bind pose + frames 0, 23,
  46, 69, 92 of the Mixamo clip, plus a **side view at frame 20** (t=0.333334),
  the clip's lowest COM (z=14.89) and so its mid-slide.

### Mistakes found while building (both in the root `MISTAKES.md`)

1. **The resume's own `SRC` and `CLIP` are relative paths, and both produce a
   green-looking falsehood.** `SRC=fixtures/human_male_vanilla.nif` opened a
   scene of one unnamed node -- "0 bones matched (expected 78)", 8 failures of
   27 -- and `CLIP=scratchpad/.../jog.hkx` produced four identical pictures,
   which is gate (e)'s refuter for "the pose never reached the rig". Git-Bash
   gets no MSYS2 path conversion and `winpath()` only rewrites the `/e/...`
   form. With absolute Windows paths: 27/0 and 4 of 4.
2. **The `WW_HKXANIM_CLIP` hook discards the loader's refusal string**
   (`src/nifskope_ui.cpp`: `const QString hkxErr = ...` is only ever tested for
   emptiness). That is why (1) cost a whole render round. Reported, NOT fixed --
   `nifskope-ww-resume-pending` rule 6.

### What was NOT measured

* **Fallout 4 has never loaded any of this.** No flight was run; the game was
  deliberately down for the whole lane.
* The round trip bungo actually asked for -- export an animation, import it
  back, hold the two against each other 1:1 -- is the NEXT step and waits on his
  word about the frames.
* Lane HKX5's own 23 gates were not re-run: they run on
  `release/hkxwrite_dump.exe`, which linking the same two translation units into
  `NifSkope.exe` does not change.
* No harness outside the animation set was run. The build is a full rebuild, so
  every other feature's harness is a candidate; none of them reaches this
  change, and none was run.

### Mtimes, in one table
""" + MTIMES + """
### Skill review (CONSTITUTION 1a)

**Loaded and used:** `nifskope-ww-resume-pending` (the read order, qmake before
make, the per-object dependency walk, the whole-working-set staleness sweep, and
rule 6 -- measure the cause, do not land the fix -- which is why the
`WW_HKXANIM_CLIP` swallow was reported instead of patched);
`nifskope-ww-build-verify` (make's own exit code as the gate, the stylesheet
compare, the "successful build is not a consistent one" object check);
`nifskope-ww-render-shot` (the pin, `upp`, the absolute-path rule, and the
orthographic arm that solved the Mixamo framing); `ww-hkx-animation` (the gate
commands and section 11's playback facts); `ww-anchored-hookup` in spirit,
through HKX5's own script.

**The skill that should have existed and did not:** none new was written. The
two procedures this lane could have re-derived -- composing a labelled contact
sheet from renders, and applying another lane's refusing hook-up -- are already
covered by `ww-texel-picture`'s caption rules and `ww-anchored-hookup`
respectively, and both were followed rather than reinvented.

**Amendments owed and made:** `nifskope-ww-render-shot` and `ww-hkx-animation`
gain the absolute-INPUT-path rule (they carried it only for `WW_*` OUTPUT
paths), with the measured symptom -- a scene of one unnamed node, and four
identical pictures -- so the next lane recognises it in one line instead of a
render round.
"""

HKX5 = """

## Build (BUILD7)

Built by lane BUILD7, 2026-09-10, on `main`, **nothing committed**.

**Section 5's hook-up is APPLIED.** `scratchpad/build7_20260910/apply_hookup.py`
imports `EDITS` from this lane's own `hookup.py` rather than retyping the four
anchors, asserts each anchor occurs exactly once and each insertion is absent,
and asserts the byte deltas after: `NifSkope.pro` 18,779 -> 18,859 bytes,
LF 717 -> 721, **CR 0 unchanged**. `src/gltfimport.h`, `src/hkxwrite.h`,
`src/gltfimport.cpp` and `src/hkxwrite.cpp` are now in `HEADERS`/`SOURCES`.
Lane HKX4b's `src/gltfexport.*` was left alone and is NOT in the `.pro`.

**`qmake` + `make -j2`, both RC=0.** `GeneratedFiles/.obj/gltfimport.o`
(216,611 B, 14:02:11) and `hkxwrite.o` (106,984 B, 14:02:14) are built and
linked into `release/NifSkope.exe` (14:04:34, 19,197,952 B). Both objects name
`src/hkxanim.h` in the regenerated dependency lists. So the "no `.pro` entry,
`qmake` has not seen these files" line of section 7 is discharged; everything
else in section 7 stands.

**This lane's 23 gates were NOT re-run.** They run on
`release/hkxwrite_dump.exe` (13:41:42), which the application link does not
touch: the same two translation units, the same flags, a different final
binary. Re-running them would have measured `hkxwrite_dump.exe` a second time,
not the exe. What IS newly true is only that the two files compile and link
inside the application.

**Still nothing user-facing.** No menu item, no dialogue, no UI route reaches
`gltfImportAnimation` or `hkxWriteAnimation` -- linking them in makes them
callable, not reachable. And **Fallout 4 has still never loaded one of these
files**; the two flight files in `scratchpad/hkx5_20260910/flight/` are
unchanged and unflown.

Lane HKX1's gates, rebuilt and re-run in the same build because
`src/hkxanim.cpp` is shared: 134 checks / 3 pre-registered fixture failures,
29 / 0, 20 / 0, and the identity rule 8,835 rows with 0 `bone != track` plus
6/6 on the floor. Lane HKX2's in-app gates: 27 checks, 0 failures, twice.

### Mtimes, in one table
""" + MTIMES + """
### Owed, still, from this lane's own list

* The angle-metric correction (`4*asin`, not `2*asin`) is in the
  `ww-hkx-animation` skill already; **`tests/spells/hkxanim_decode.py`,
  `tests/spells/hkxanim_gates.py` and `docs/HKX_ANIMATION_FORMAT.md` section 4.6
  still carry `2*asin`**, so lane HKX1's published angle figures are still
  halves. BUILD7 did not touch them: they are another lane's files and changing
  the metric would have moved the gate numbers this build had to reproduce.
* `docs/GLTF_IMPORT.md` and lane HKX4b's `docs/GLTF_INTERCHANGE.md` have still
  not been read side by side.
"""


def append(path, text):
    p = os.path.join(REPO, path)
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0, path
    assert 'BUILD7' not in b.decode('utf-8'), '%s already has a BUILD7 section' % path
    add = text.encode('utf-8')
    assert add.count(b'\r') == 0
    open(p, 'ab').write(add)
    b2 = open(p, 'rb').read()
    assert b2[:len(b)] == b and b2.count(b'\r') == 0
    print('%s %d -> %d bytes, append-only, CR 0' % (path, len(b), len(b2)))


append('scratchpad/lane_hkx2_report.md', HKX2)
append('scratchpad/lane_hkx5_report.md', HKX5)
