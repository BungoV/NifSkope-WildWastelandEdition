# HARNESSWIN2 -- PENDING

## 0. PHASE 2 HAPPENED. This is no longer BUILD PENDING.

Director clock 2026-09-19 17:2x CEDT. The hook-up was applied, the exe built,
and every gate below was run. **Everything from section 1 down is the PLAN as it
stood before the run; where the run refuted it, this section says so and the run
wins.**

    release/NifSkope.exe   23,505,408 B   2026-09-19 16:48:03
                           sha1 072d78f8629aac431fcff51c7d8db349a0672a8b   MZ
    rung (taken once)      release/NifSkope.before_harnesswin2.exe
                           23,504,896 B   15:57:53   sha1 220662f1eb1f344a

`make` exit 0; only `nifskope.o` and `nifskope_ui.o` recompiled, then the link
-- so this exe is the previous one plus these two edits and nothing else. No
qmake (`NifSkope.pro` older than `Makefile.Release`; CELLVIEW1 had already run
it at 15:08:37). Exe newer than all of `nifskope_ui.cpp`, `nifskope.cpp`,
`nifskope.h`, `lodgen.cpp`, `cellview.cpp`, `glview.cpp`, `NifSkope.pro`.
`res/style.qss` and `release/style.qss` identical.

### The gates

    harness_window.sh      15 checks, 0 failures, 0 skips   PASS
    native_open.sh         17 checks, 0 failures, 2 skips   PASS
    render_shot.sh         82 checks, 0 failures            PASS
    cell_open.sh            8 checks, 0 failures            PASS
    impostor_draw.sh       24 steps,  0 failures            PASS
    gltf_export_options.sh  0 rows not as registered        PASS
    gltf_gates.sh           0 gates not as registered       PASS
    body_build.sh           0 rows not as registered        PASS
    native_lighting.sh     14 checks, 3 failures  (8 on the rung -- read below)

`harness_window.sh`'s `FLOOR=` was raised 11 -> 15 from that run, and the
comment now carries the exe stamp it was measured on.

### What the repair did, in numbers

* **native_open.sh row (c): covered 0.8978 -> 0.9331**, at `vp=1024x989`,
  `upp=16.000000` -- the viewport it asked for. Row (d), which had 2 failures on
  HARNESSWIN1's binary, is green too (MAD 20.808 < 48, NCC 0.8411 >= 0.45).
* **The window lands on its request when the request is reachable.**
  `asked=1024x1024 window=1024x1024 viewport=1024x989 ... settings=not-restored`
  with no `FLOORED`.
* **Row (f) fires both ways.** The repaired exe leaves a seeded recent-file list
  exactly as seeded; the rung ADDS the fixture to it. The red control can fail.
* **His real `Recent File List` is untouched by the repaired exe**, proved
  read-only around `native_lighting.sh`, the one spell that had been writing it:
  949 B / sha1 `07f3cec4eec0a86c` before and after.

### Three things in the plan below that the run REFUTED

1. **`render_shot.sh` is NOT a Class U control** (section 3f called it one). Its
   18 shots went 1822x445 -> **857x445**. Its COUNTS are the invariant and they
   held exactly: 82 checks, 0 failures. Nothing to re-base -- it stores no image.
2. **The post-repair size is not the request.** 3b predicted 640x445 for that
   gate. 857 is the layout minimum with the docks already hidden, which row (e)
   prints: `measured layout minimum for this build: 857x480 (asked 640x480)`.
   Above 857 px a request now lands exactly; below it, it still floors and still
   says so.
3. **`grep -c FLOORED` is not a test** (3c rule 2). The log is an append trace
   and one correct run writes its whole convergence: three FLOORED lines and
   then the clean landing. Read the LAST line, as the gate's own `logline` does.

### The four native_lighting baselines: NOTHING WAS RE-BASED

The before/after table the brief asked for turned out to have one row, because
the answer is that the stored baselines were right all along:

    baseline                          stored        rung run      repaired run
    legacy_bto_top.png   1024x989   byte-identical    FAIL        byte-identical
    legacy_bto_obl.png   1024x989   byte-identical    FAIL        byte-identical
    legacy_btr_top.png   1024x989   byte-identical    FAIL        byte-identical
    legacy_btr_obl.png   1024x989   byte-identical    FAIL        byte-identical

They are 1024x989; the floored runs were handing the comparer 1822x989, so gate
(a) had been reporting a size mismatch that read as a rendering regression. The
repair makes all four match **byte for byte**, with no new capture. Section 3d
guessed this was one of two possibilities; it is that one.

`native_lighting.sh` therefore goes **8 failures -> 3**. The 3 that remain are
NOT this lane's and are not new:

    gate (b) darkest-fifth IoU own vs flat   1.000  (bar 0.800)
    gate (b) own-minus-flat blockSD          0.00   (floor 3.50)
    gate (d) west > flat > east ordering     0.00%  (bar 99.00%)

Identical values on `NifSkope.before_harnesswin2.exe` (15:57:53) AND on
`NifSkope.before_cellview1.exe` (14:51:48), so the defect is older than
CELLVIEW1, IMPOSTORFIX3 and this lane. It says the "own-normals" and
"flat-normals" renders are coming out as the SAME picture -- the slope signal is
absent, not merely weak. That is a real defect and it needs its own lane; see
CHANGE_NEEDED 6 at the end of this file.

### One mistake made in this phase, already in root MISTAKES.md

I bisected `native_lighting.sh` by re-running it on two RUNG binaries with only
`EXE=` overridden. A rung has no guard, so those two runs **edited his real
Recent File List**: three entries in, three off the end (882 B / `2cb80e7a` ->
949 B / `07f3cec4`; the previous value is saved read-only beside this file as
`recent_file_list_before_1644.txt`). His `Window Geometry` and `Window State`
are byte-identical. I have NOT written his registry to undo it -- restoring it
is his call, and the exact prior list is in that file.

---

Director clock at write of everything below: 2026-09-19 ~16:0x CEDT.
Exe in tree at 16:0x, read not assumed: `release/NifSkope.exe` **23,504,896 B,
15:57:53, sha1 `220662f1eb1f344a`** -- NOT the 23,504,384 B / 15:14:27 the brief
gave, because IMPOSTORFIX3 built at 15:57:53 while this lane was writing.
Rung already on disk from that lane: `release/NifSkope.before_impostorfix3.exe`
(23,504,384 B, 15:52:26 = the brief's binary).

**That lane owns the build slot and its exe will move again.** So the figures
above are a reading, not a pin: step 2 below takes this lane's own rung from
whatever `release/NifSkope.exe` is at the moment of the apply, and step 3's shot
census must be taken from that same exe, not from artefacts already on disk.

**Nothing in this lane has been applied, built or run.** No commit, no stash.
`WW_CHANGES.md` and `HANDOFF.md` were not touched. No existing `src/` file was
edited in place: the two repairs are in a refusing hook-up the director applies.

---

## 1. The one sentence

Two repairs, both one-liners in behaviour: the `WW_RENDER_SHOT` grab lambda
must bring the docks down **before** it resizes the window, because Qt will not
make a window narrower than its layout minimum and the docks are part of that
layout until they are hidden; and `setCurrentFile()` / `clearCurrentFile()` must
stop writing bungo's recent-file list on a harness run, which they have been
doing for as long as the harnesses have existed.

## 2. The hook-up, and its `--check`

```
python scratchpad/harnesswin2_20260919/hookup.py            # --check, writes nothing
python scratchpad/harnesswin2_20260919/hookup.py --apply     # all or nothing
```

Run at 15:5x on the tree as CELLVIEW1 left it:

```
  src/nifskope_ui.cpp    before  line 22111  eol=LF   indent=5 tabs  'skope->resize( rw, rh );'
  src/nifskope.cpp       guard   line 8243   eol=CRLF indent=1 tabs  'settings.setValue( "File/Recent File List", files );'
  src/nifskope.cpp       guard   line 8283   eol=CRLF indent=1 tabs  'settings.setValue( "File/Recent File List", files );'

src/nifskope_ui.cpp     1604922 -> 1606934  bytes   CR   +0   LF  +34   (expected CR  +0 LF +34)
src/nifskope.cpp         453785 -> 454779   bytes   CR  +15   LF  +15   (expected CR +15 LF +15)
```

Three locators, each asserted unique (`skope->resize( rw, rh );` 1 of 1;
`::updateRecentFiles( files, currentFile );` 1 of 1;
`files.removeAll( currentFile );` 1 of 1, each picking one of the two identical
`setValue` lines). No tab, indent or line ending is typed anywhere in the
script -- every one is taken out of the file's own bytes, because
`src/nifskope_ui.cpp` is LF-only and `src/nifskope.cpp` is MIXED (CR 10899 /
LF 10904). The already-applied marker is the string `HARNESSWIN2`, which exists
nowhere in the tree today and which only these edits create; it is never a line
the edits repeat.

**Syntax-checked, not built.** The patched files were written to a throwaway
copy and run through `g++ -fsyntax-only` with the flags out of
`Makefile.Release`: **RC=0 for both**, only the pre-existing warnings
(`_USE_MATH_DEFINES` redefined, two `'/*' within comment` at 27229 and 31200,
the `qchar.h -Wsfinae-incomplete` noise). The copies were deleted afterwards so
there is only ever one src tree. To regenerate them:

```bash
python - <<'PY'
import io,sys,os
sys.path.insert(0,"scratchpad/harnesswin2_20260919"); import hookup as H
os.makedirs("scratchpad/harnesswin2_20260919/preview",exist_ok=True)
for p in ("src/nifskope_ui.cpp","src/nifskope.cpp"):
    raw=io.open(p,'rb').read(); es=[H.build(e,raw)[0] for e in H.EDITS if e["path"]==p]
    new=raw
    for s,en,t in sorted(es,key=lambda t:-t[0]): new=new[:s]+t+new[en:]
    io.open("scratchpad/harnesswin2_20260919/preview/"+os.path.basename(p),'wb').write(new)
PY
```

### What the hook-up deliberately leaves alone

* **`src/nifskope_ui.cpp` ~22386, the `WW_IMPOSTOR_BAKE` lambda.** Identical
  resize-before-hide order, `resize( 560, 560 )`, and it is floored today:
  every `*_front.png` this tree has written is **1822x525**, measured on
  `scratchpad/impostorfix3_20260919/fixture/blast_n4/bake/treemapleblasted05_front.png`
  at 15:57. Repairing it would re-size the input to every card sheet while
  lane IMPOSTORFIX3 is measuring those sheets. **CHANGE_NEEDED 5**, below.
* **`src/nifskope.cpp:8273`**, `File/Recent Archive Files` in
  `setCurrentArchiveFile()`. Same defect, different key, one line.
  **CHANGE_NEEDED 4**, below.

## 2b. The gate for the second repair

`tests/spells/harness_window.sh` (this lane owns it, from HARNESSWIN1) gains
row **(f)**, "a harness run does not edit the recent-file list":

* it SEEDS `File/Recent File List` in the **gate scope** with two paths the
  fixture is not among, runs the current exe, and requires the list back
  unchanged. Seeding is what makes it deterministic: the reason row (c) passed
  at 14:47 and failed at 14:58 is that the writer only changes the exported key
  when the list ORDER changes, and re-opening the same fixture leaves it at the
  head already.
* it has a **red control**: the rung is re-run in the same seeded scope and
  must ADD the fixture. A row that cannot watch the defect happen is not a floor.
* the control rung is `release/NifSkope.before_harnesswin2.exe`, falling back to
  `before_impostorfix3.exe`. It must be a **post-HARNESSWIN1** binary: a control
  that cannot see `WW_SETTINGS_SCOPE` would be measured against settings it was
  never given, which is the mistake row (c) already paid for.
* bungo's own key is never written, here or anywhere in this gate.

`FLOOR` is **left at 11 on purpose**. Row (f) should make it 13/15, but that is
a prediction and `ww-test-harness-add` 5c says a floor is measured, never
predicted. **The director raises it**: run the gate, read the count it prints,
write that count and the exe stamp into the comment above `FLOOR=`.

`bash -n` passes; the file is still LF-only (CR 0, 515 LF, 26,481 B).

**Expected before the hook-up is applied:** row (f) check 1 FAILS and its
control passes. That is the defect being shown, not a broken row.

---

## 3. THE RE-BASE PLAN for the render-shot baselines

### 3a. What is actually stored, and it is not twelve

Thirteen spells take a `WW_RENDER_SHOT`. Only **two places in the tree hold a
stored image to compare against**, and one of them is already stale for an
unrelated reason:

| store | files | size on disk | written |
|---|---|---|---|
| `tests/baselines/native_lighting/` | 4 PNG | **1024x989** | 2026-09-16 12:15 |
| `tools/render_regression/baseline/` | 7 PNG | **1280x695** | 2026-07-27; its `current/` set is **1280x741**, so it is ALREADY size-mismatched, from a dock change months before this repair |

Everything else either measures a pair rendered **in the same run at the same
size** (`native_open.sh` coverage, `impostor_draw.sh` IoU, `lodl_open.sh`),
reads the size back **out of the PNG** instead of assuming it
(`render_shot.sh`), or compares against an INDEPENDENT non-image source
(`cell_open.sh` against a Python walk of the plugin). **Those need no re-base at
all -- only a re-measure.** Saying "twelve baselines move" would be wrong, and
it is the single most expensive thing to get wrong here.

### 3b. Which spells move -- MEASURED off the artefacts already on disk

Every "today" figure below is a PNG this tree wrote in the last six hours, read
with a PNG header parser, not predicted. A shot is `window width` x
`window height - 35` (the menu bar and the status bar); that arithmetic is
confirmed at three different sizes.

| spell | asks for | on disk TODAY | floored? | after the repair |
|---|---|---|---|---|
| `render_shot.sh` | 640x480 | **1822x445** (`release/ww_render_shot/cam_*.png`, 15:40) | YES | 640x445 |
| `native_open.sh` | 1024x1024 | vp **1822x989** | YES | 1024x989 |
| `lodl_open.sh` / lodm | 1024x1024 | **1822x989** (`release/lodm_doc.png` 14:55, `release/chunk_0.png` 13:18) | YES | 1024x989 |
| `native_lighting.sh` | 1024x1024 | baselines are **1024x989** | see 3d | 1024x989 |
| `lodi_v7.sh`, `lodl_channels.sh` | 1400x1091 | not run today | unknown | measure |
| `lod_channel_preview.sh` | 480x480 | not run today | unknown | measure |
| `lodgen_octahedral.sh` | 640x480 | not run today | unknown | measure |
| `skeleton_overlay.sh` | 1000x1000 | not run today | unknown | measure |
| `refraction.sh` | 1280x800 | not run today | unknown | measure |
| `cell_open.sh` | **1822x960** | **1822x925** (`scratchpad/cellview1_20260919/images/downtown.png`, 15:39) | **NO** | unchanged |
| `impostor_draw.sh` | 1024x1024 | **1024x1024** exactly | **NO -- own path** | unchanged |
| WW_IMPOSTOR_BAKE cards | 560x560 | **1822x525** | YES | **not touched** (CHANGE_NEEDED 5) |

Two rows in that table are the reason this is a plan and not a table.

* **`impostor_draw.sh` does not go through the grab lambda at all.** Its shots
  are 1024x1024 EXACTLY, not 1024x989, because `forceWindow()` in
  `src/impostorpreviewtest.cpp:271-319` (lane IMPOSTORSHOW) already hides the
  docks and the viewport header BEFORE its `resize()` -- lines 291-295 -- and
  then grows the window back until `grabFramebuffer()` returns the size asked
  for. **The repair this lane proposes is the same repair, already in this
  tree, already working, in the sibling path.** That is the strongest evidence
  available that it is correct, and it means IMPOSTORFIX3's numbers cannot move.
* **`cell_open.sh` is not floored, it ASKED for 1822.** See section 4.

**The "unknown" rows are not a gap, they are the method.** Which scenes floor
depends on which docks the scene raises, not on the number in the script: the
same binary gave 1024x1024 for `refraction_fixture.nif` and 1822x1024 for a
`.lodi` scene on 2026-09-19. It cannot be predicted from the scripts and must
not be. Each spell reports it itself, in one line, in
`release/ww_harness_window.log` -- the word `FLOORED`.

### 3c. The rule for "moved by size only"

Record, for every shot PNG a spell writes, **width, height and sha1, before and
after**, with this one-liner:

```bash
shotcensus() {   # shotcensus <dir> <outfile>
  find "$1" -name '*.png' -newermt '-2 hours' | sort | while read -r f; do
    python -c "import struct,sys,hashlib;b=open(sys.argv[1],'rb').read();w,h=struct.unpack('>II',b[16:24]);print('%s %dx%d %s'%(sys.argv[1],w,h,hashlib.sha1(b).hexdigest()[:12]))" "$f"
  done > "$2"
}
```

Then exactly two classes, and each has a way to fail:

* **Class U -- the size did NOT change.** Then the sha1 **must not change
  either**. A shot the repair did not resize is a shot the repair did not
  reach, so identical bytes are the only honest result. **Size unchanged +
  bytes changed = the repair altered RENDERING, which it has no business doing.
  STOP and report it; do not re-base anything.** `cell_open.sh` and
  `impostor_draw.sh` are the two spells this is predicted for, which makes them
  the cheapest and most valuable runs in the whole list.
* **Class F -- the size DID change.** Three things, all three required before
  a single baseline is re-written:
  1. **The new size is the size that was asked for.** New width == the width in
     that spell's `WW_RENDER_SIZE`, new height == that height minus 35. If it is
     neither the old value nor the request, something else moved and the re-base
     is hiding it.
  2. **The line stopped saying FLOORED.** `grep FLOORED release/ww_harness_window.log`
     after the run must come back empty for that spell, and the same grep before
     the repair must NOT have. A size change with the word still there is not
     this repair.
  3. **The content test, with a control that can fire.** Resample the OLD shot
     to the NEW shot's size (box filter) and take mean |dColour| against the new
     shot; call it `d_honest`. Then resample the OLD shot to a DELIBERATELY
     WRONG size -- the new width +5% -- and take the same metric; call it
     `d_wrong`. **"Moved by size only" means `d_honest` is below the same-size
     noise floor AND clearly below `d_wrong`.** Measure the noise floor first
     by capturing one shot TWICE at the same size without changing anything --
     some of these scenes are time-dependent (`WW_RENDER_TIME`, particles), so
     the floor is not assumed to be zero, it is read. Quote all three numbers
     in the report. Without `d_wrong` the test cannot fail and is not a test.

A re-based baseline is committed only with those numbers written beside it.

### 3d. `native_lighting.sh` -- the one real re-base, and it may be a repair

Its four baselines are **1024x989**: the size a 1024x1024 request produces
**when it is not floored**. So one of two things is true today, and the
director finds out by running it rather than by reasoning:

* the gate is size-mismatching today on all four, in which case the repair
  **fixes** it and nothing is re-based at all; or
* its `.bto` / `.btr` scenes raise no dock above 1024 and it already matches, in
  which case it is Class U and the sha1s must not move.

Either way **do not re-capture these four until the run says which.** They are
the only baselines in the tree whose stored size equals the repaired size, and
re-capturing them first would destroy the evidence.

`tools/render_regression/` is out of scope: `baseline/` 1280x695 vs `current/`
1280x741 was already mismatched on 2026-08-11, long before any of this. Named
so it is not blamed on the repair.

### 3e. The order to run them

1. `tests/spells/harness_window.sh` -- rows (a)-(f). Both repairs, cheapest proof.
2. `tests/spells/native_open.sh` -- the target. Row (c) `covered` must leave 0.8978.
3. `tests/spells/render_shot.sh` -- Class F, 82 checks / 0 failures must HOLD
   while its shots go 1822x445 -> 640x445. It reads the size back, so it is the
   one spell that proves a re-size without any re-base.
4. `tests/spells/cell_open.sh` -- **Class U**, must be byte-identical. If this
   one moves, stop.
5. `tests/spells/impostor_draw.sh` -- **Class U**, floor 0.35 NEVER lowered.
   Coordinate with IMPOSTORFIX3; if it is mid-measurement, run it after.
6. `lodl_open.sh`, `lodi_v7.sh`, `lodl_channels.sh`, `lod_channel_preview.sh`,
   `skeleton_overlay.sh`, `refraction.sh`, `lodgen_octahedral.sh` -- no stored
   images; their check COUNTS must hold.
7. `tests/spells/native_lighting.sh` -- last, and only then the re-base decision.

### 3f. The exact commands

Every block is one command. The process guard is its own command before every
build and every exe run, and `Fallout4.exe` up means park at BUILD PENDING.

```bash
# --- 0. the guard, on its own, every time
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
```

```bash
# --- 1. re-check the anchors on the tree as it stands NOW, then apply
cd /e/Projects/NifskopeWildWastelandEdition && \
python scratchpad/harnesswin2_20260919/hookup.py
```

```bash
# --- 2. the rung row (f) needs, then apply
cd /e/Projects/NifskopeWildWastelandEdition && \
cp -p release/NifSkope.exe release/NifSkope.before_harnesswin2.exe && \
ls -l --time-style=+%H:%M:%S release/NifSkope.before_harnesswin2.exe && \
python scratchpad/harnesswin2_20260919/hookup.py --apply
```

```bash
# --- 3. the BEFORE census, on the shots already on disk, before anything is built
cd /e/Projects/NifskopeWildWastelandEdition && \
find release/ww_render_shot scratchpad/cellview1_20260919/images release \
     -maxdepth 2 -name '*.png' -newermt '-8 hours' 2>/dev/null | sort | while read -r f; do \
  python -c "import struct,sys,hashlib;b=open(sys.argv[1],'rb').read();w,h=struct.unpack('>II',b[16:24]);print('%s %dx%d %s'%(sys.argv[1],w,h,hashlib.sha1(b).hexdigest()[:12]))" "$f"; \
done > scratchpad/harnesswin2_20260919/shots_before.txt; \
wc -l scratchpad/harnesswin2_20260919/shots_before.txt
```

```bash
# --- 4. build.  THIS lane's two edits need no qmake: no file was added, the
#     .pro is untouched, and both sources are already in Makefile.Release's
#     object list.  But another lane's .pro edit would, so ASK the tree rather
#     than trusting that sentence -- at 16:0x the answer was "makefile up to
#     date" because CELLVIEW1 had already run qmake at 15:08:37.
[ NifSkope.pro -nt Makefile.Release ] && echo "RUN QMAKE FIRST" || echo "makefile up to date"
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
```

```bash
cd /e/Projects/NifskopeWildWastelandEdition && \
stat -c "before %y %s" release/NifSkope.exe && \
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; exit $rc' && \
stat -c "after  %y %s" release/NifSkope.exe && \
test release/NifSkope.exe -nt src/nifskope_ui.cpp && \
test release/NifSkope.exe -nt src/nifskope.cpp && \
head -c 2 release/NifSkope.exe && echo " <- must be MZ" && \
cmp res/style.qss release/style.qss && echo "sheet in step" && \
grep -oE "GeneratedFiles/\.obj/[a-z_0-9]+\.o" /tmp/ww_build.log | sort -u
```

The exe's MTIME MUST MOVE (HORIZON2's rule: a header no object lists gives
`rc=0` and a stale exe). The object list is read back because CELLVIEW1 touched
four files in the same tree, so the new exe is not "the old exe plus my diff".

```bash
# --- 5. the gate, every row
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
```

```bash
cd /e/Projects/NifskopeWildWastelandEdition && \
timeout 1800 bash tests/spells/harness_window.sh 2>&1 | tail -45
```

Then WRITE THE COUNT IT PRINTS into the `FLOOR=` comment in that file. It is
11 now on purpose; it is not raised by guessing.

```bash
# --- 6. the target
cd /e/Projects/NifskopeWildWastelandEdition && \
timeout 1800 bash tests/spells/native_open.sh 2>&1 | tail -30; \
grep -c FLOORED release/ww_harness_window.log
```

```bash
# --- 7. the two Class U controls: these must come back byte-identical
cd /e/Projects/NifskopeWildWastelandEdition && \
timeout 1800 bash tests/spells/render_shot.sh 2>&1 | tail -6 && \
timeout 1800 bash tests/spells/cell_open.sh 2>&1 | tail -10
```

```bash
# --- 8. the AFTER census and the comparison
cd /e/Projects/NifskopeWildWastelandEdition && \
find release/ww_render_shot scratchpad/cellview1_20260919/images release \
     -maxdepth 2 -name '*.png' -newermt '-2 hours' 2>/dev/null | sort | while read -r f; do \
  python -c "import struct,sys,hashlib;b=open(sys.argv[1],'rb').read();w,h=struct.unpack('>II',b[16:24]);print('%s %dx%d %s'%(sys.argv[1],w,h,hashlib.sha1(b).hexdigest()[:12]))" "$f"; \
done > scratchpad/harnesswin2_20260919/shots_after.txt; \
diff scratchpad/harnesswin2_20260919/shots_before.txt \
     scratchpad/harnesswin2_20260919/shots_after.txt | head -40
```

Read that diff against section 3c: a line whose SIZE changed is Class F and
needs all three tests; a line whose size is the same but whose sha1 moved is
the STOP case.

```bash
# --- 9. the neighbours, only after 5-8 are read
cd /e/Projects/NifskopeWildWastelandEdition && \
timeout 1800 bash tests/spells/lodl_open.sh 2>&1 | tail -4
```

`impostor_draw.sh` goes LAST and only with IMPOSTORFIX3's agreement; its floor
of 0.35 is never lowered by anybody.

---

## 4. The question answered from files: CELLVIEW1's 1822 px

**It goes through the harnesswindow path, it is not bypassing anything, and the
1822 is its own request.**

* `tests/spells/cell_open.sh:76` -- `SIZE="${SIZE:-1822x960}"`
* `tests/spells/cell_open.sh:131` -- `WW_RENDER_SHOT=... WW_RENDER_SIZE="$SIZE"`

So the spell asks for a 1822-pixel-wide window and gets one. The picture on
disk, `scratchpad/cellview1_20260919/images/downtown.png` written at 15:39, is
**1822x925** = 1822 x (960 - 35), the asked width and the asked height less the
menu bar and status bar. It is **not floored**; the repair in section 2 will
not change it by a pixel, which is why section 3e uses it as a Class U control.

Why it is not a bypass, by file and line:

* `src/nifskope_ui.cpp:1533` -- `wwApplyHarnessWindow( w )` runs for the window
  whatever the scene is; `WW_CELL_*` and `WW_RENDER_*` both make
  `NifSkope::wwHeadlessRun()` (`src/nifskope.cpp:7594`) true.
* `src/nifskope_ui.cpp:31577` -- `if ( !wwHarnessGeometryRestoreSuppressed() )`
  guards the `restoreGeometry`, so the persisted geometry is not replayed.
* `src/nifskope_ui.cpp:22089` -- the `WW_RENDER_SHOT` grab lambda is the shared
  path and is where `WW_RENDER_SIZE` is read; `cell_open.sh` sets that variable,
  so it is this lambda that sizes the window.
* `src/cellview.cpp` contains **no** `resize`, `showNormal`, `setGeometry`,
  `restoreGeometry`, `QDockWidget` or `viewportHeader` -- grep returns nothing.
  `cellSpecFromEnv()` at `src/cellview.cpp:473` reads `WW_CELL_OPEN` and builds
  a SCENE. It never touches the window.

Worth the director knowing, since it is the likely reason 1822 was written into
that spell: if `cell_open.sh` was calibrated by reading back what a floored run
produced, its 1822x960 is a number inherited from the defect rather than
chosen. It is CELLVIEW1's call whether to keep it; after the repair a narrower
request would be honoured, and 1822x960 does not fit on a 1920x1080 screen with
window chrome at 1960,40 either way.

---

## 5. CHANGE_NEEDED 4 -- `File/Recent Archive Files`, still unguarded

`src/nifskope.cpp:8273`, in `setCurrentArchiveFile()`:

```cpp
	settings.setValue( "File/Recent Archive Files", hash );
```

Same defect as section 1, different key: a harness that opens a mesh from a BSA
or BA2 rewrites bungo's per-archive recent list. Not in this lane's hook-up
because the brief named `File/Recent File List`, and a hook-up is all-or-nothing
so an unasked-for edit cannot be dropped later. One line, the same predicate,
whenever a director wants it.

## 6. CHANGE_NEEDED 5 -- the impostor bake has the identical defect

`src/nifskope_ui.cpp` ~22386, the `WW_IMPOSTOR_BAKE` lambda:

```cpp
skope->showNormal();
skope->resize( 560, 560 );                     // <-- docks still up
for ( QDockWidget * dw : skope->findChildren<QDockWidget *>() ) dw->hide();
if ( skope->viewportHeader ) skope->viewportHeader->hide();
```

Measured consequence: every card capture in this tree is **1822x525** instead
of 560x525. Those captures are the input to the octahedral sheets, so repairing
this changes every `*_oct_albedo.png` at once -- and lane IMPOSTORFIX3 is
measuring exactly those sheets right now. **Do not take it while that lane is
alive.** When it is taken it needs the same before/after census as section 3c,
across the whole fixture set, and IMPOSTORFIX3's IoU numbers re-measured.

## 7. One divergence worth naming

The two harness paths do not mean the same thing by `WW_RENDER_SIZE`.
`src/impostorpreviewtest.cpp:271` treats it as the **framebuffer** size and
grows the window until the grab matches. The `WW_RENDER_SHOT` lambda treats it
as the **window** size, which is what `tests/spells/native_open.sh:117-119`
documents, so a 1024x1024 request there is a 1024x989 picture. This lane keeps
the window-size meaning, because changing it would move the HEIGHT of every
shot in the tree as well as the width, and that is a separate decision with a
separate blast radius. It is named here so nobody has to rediscover it.

## 8. Files this lane owns

New:
* `scratchpad/harnesswin2_20260919/hookup.py` -- 3 edits, `--check` green, NOT applied
* `scratchpad/harnesswin2_20260919/PENDING.md` -- this file

Edited (tests only, this lane's own file from HARNESSWIN1):
* `tests/spells/harness_window.sh` -- row (f) added, `FLOOR` comment amended.
  `bash -n` passes, LF-only preserved, 22,494 -> 26,481 B.

No `src/` file, no `NifSkope.pro`, no `WW_CHANGES.md`, no `HANDOFF.md`, no
`impostor*`, `gltf*` or `cellview*` file was modified.

## 9. Nothing here is fixed

*(As written at 16:0x. Section 0 records what happened next: the hook-up was
applied, the exe was built at 16:48:03, and row (c) reads 0.9331. Nothing is
called fixed even so -- the refuters are named in section 0 and below.)*

---

## CHANGE_NEEDED 6 -- native_lighting's own-vs-flat signal is absent, and predates today

Not this lane's, not repaired here, named with numbers because it is now the
only red left in the neighbourhood and someone will otherwise read it as fallout
from the window change.

`tests/spells/native_lighting.sh` gate (b) and gate (d) report that the
"own normals" render and the "flat normals" render are **the same picture**:

    darkest-fifth IoU own vs flat        1.000   bar 0.800
    own-minus-flat blockSD               0.00    floor 3.50
    west > flat > east on covered px     0.00%   bar 99.00%

1.000 and 0.00 are not weak signals, they are no signal. The gate prints its own
algebra beside them -- headlight (0,0,1) in view space, the oblique rotation
putting it along (-0.6516, +0.6142, +0.4453), so N.L should order west 0.7258 >
flat 0.4434 > east 0.0968 -- and the pictures order nothing.

Measured on three binaries, same values every time:

    release/NifSkope.exe                  16:48:03   3 failures (this build)
    release/NifSkope.before_harnesswin2   15:57:53   8 failures (5 are size)
    release/NifSkope.before_cellview1     14:51:48   8 failures (5 are size)

So it is older than CELLVIEW1, IMPOSTORFIX3 and HARNESSWIN2. The five extra
failures on the two rungs are gate (a)'s size mismatch and one blockSD row that
moves with the viewport; the three above are identical throughout.

**The cheapest refuter was run, and it lands.** Hashing the gate's own shots in
`scratchpad/nativeview2_20260912/work/gate/`:

    t_own_obl.png    1024x989   b39e407cc0a7
    t_flat_obl.png   1024x989   b39e407cc0a7
    t_tilt_obl.png   1024x989   b39e407cc0a7
    t_tiltw_obl.png  1024x989   b39e407cc0a7
    t_own_top.png    1024x989   b4f54b564cee   (and flat/tilt/tiltw_top, same)

**All four normal fixtures render BYTE-IDENTICALLY at each view.** This is not a
weak lighting signal, it is the same photograph four times: whatever makes the
own / flat / east-tilt / west-tilt scenes different is not reaching the render
at all. So the subject is the ARMING -- the per-fixture sheet or channel swap,
and whether something is cached between the four runs -- and not the shader. The
census rows in the same gate stay green (Shader Flags 1 bit 12, lit by
`fo4_default.prog`, `.BTO` shapes `msn=0`), which is consistent: the program is
selected correctly and is being handed the same input every time.

And a second, separate defect falls out of the same hashes: gate (d)'s FIRST row
is GREEN -- "at the TOP view an east tilt and a west tilt have the same N.L, so
the two frames are the same picture (max|dY| 0.00)". It is green because every
frame is the same picture. **That row cannot fail while this defect exists**, so
it is a floor that does not fire (ww-test-harness-add 5), and fixing the arming
without fixing the row would leave it passing for the wrong reason.

**A gate that is red for a reason nobody has named gets read as noise within a
day.** That is why this is written down rather than left in a log.
