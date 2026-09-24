#!/usr/bin/env python
"""Lane BUILD11: append a `## Build (BUILD11)` section to each of the three lane
reports. The lanes' own text is never rewritten (CONSTITUTION 8); this appends.
All three are LF-only; asserted.
"""
import os, sys

REPO = r"E:\Projects\NifskopeWildWastelandEdition"

COMMON = """
### The one table of clocks

| artefact | time |
|---|---|
| `release/NifSkope.exe` (this build) | 2026-09-10 **17:08:39**, 20,693,504 B |
| the exe this replaced (lane BUILD10) | 16:45:53, 20,007,936 B |
| `release/style.qss` (copied at link time) | 17:08:39, equal to `res/style.qss` |
| `release/hkclasses_fo4.json` | 17:08:39, 1,511,662 B |
| `release/hkx_annotation_vocabulary.txt` | 17:08:39, 55,299 B |
| bungo's own window, opened mid-lane | started 17:05:44, pid 700, no `--port` |

qmake ran before make; `DEPCHECK missing=0` over eleven objects; the three WW
defines are each once in `Makefile.Release` and on every compile line; the
exe-newer sweep is 1 stale of 114 paths and the exception is
`src/watermark.cpp`, lane WATER7's live edit in this shared tree, which is NOT
in this exe. `WW_HKXANIM_UI`, `WW_HKXCLIP_CANON` and `WW_ANIMWS_HKXMODEL` are
new or newly-read flags, so the objects of every translation unit that reads
one were deleted before make (the BUILD9 DEFINES trap: make compares mtimes,
not flags).

**Skipped harnesses, named with the reason** (`nifskope-ww-resume-pending` s5):
`loaded_nifs.sh`, `top_bar.sh` and `ui_align.sh` -- this build renamed no
user-visible string and moved no bar, so the sibling-label class of red
(BUILD9 s10) cannot have been introduced; `lodgen_*`, `water_*`, `lodl/lodt`
and the impostor gates -- nothing in these three lanes reaches the generator,
the water tool or the terrain readers. `WW_POSEDRAW_TEST` was left alone: it
was already failing at "clicking a bone did not make it the active object" on
both fixtures BEFORE lane SKELFIX, and SKELOVERLAY's report says one run on the
70-bone facial rig is what settles it.
"""

SECTIONS = {
    "scratchpad/lane_skelfix_report.md": """
## Build (BUILD11)

The hook-up applied exactly as this lane predicted: `hookup.py --check` printed
`anchor x1 (CR 0, LF 31680) / would grow by 905 bytes; CR 0 -> 0; marker x2`,
and `--apply` gave `src/nifskope_ui.cpp` 1,484,604 -> 1,485,509 (+905), CR 0,
`grep -c "lane SKELFIX"` = 2. Nothing else needed re-applying.

**The armature rule holds, and every prediction in `PENDING.md` step 3 was
met or beaten:**

| line | predicted | measured |
|---|---|---|
| overlay ON census | segments 110, stubs 62, skipped 38 | **110 / 62 / 38** |
| (f) longest BIND bone-to-bone | 22.40, limit 33.60 | **22.403 over 60 pairs, limit 33.6044** |
| (f) longest drawn at frame 46 | 31.94 | **31.9429** (172 segments) |
| (f') FLOOR, old rule | 300.5 | **300.51 over 129 segments -> red, as it must** |
| (g) endpoints outside the bone box | 0 | **0**, worst overshoot **0.932** (predicted 0.000) |
| (g') FLOOR, old rule | 19 outside, worst 241.3 | **19, worst 241.302** |
| (h) skipped | 38 on, 0 off | **38 on, census all zeros off** |
| (i) armature / marker-only | 111 / 19 | **111 / 19** |
| (d) drawn joint vs animated node | within 1e-3 | **worst 0 units** |
| (d') FLOOR | must disagree with the bind pose | **122 joints moved, largest 367.6** |
| (a) (b) | the dock's 130 / 93 / 93 / 0 | **130 / 93 / 93 / 0, muted 37 = 130 - 93** |

**THE PICTURE THE DIRECTOR IS OWED IS DELIVERED.**
`scratchpad/skeloverlay_20260910/on_frame46.png`, **1500x1000**, the human
fixture at frame 46 (t=0.766667, 60 fps) of
`fixtures/Running_To_Slide_And_Back_To_Running.hkx` with the overlay on. The
long segments are gone: the skeleton is drawn entirely on the body -- spine,
both arms out to individual finger bones, both legs, the skull -- in the
crouched mid-slide pose, and nothing runs off to the world origin. Beside it,
unchanged and NOT deleted because it is gate (j')'s only floor:
`scratchpad/skelfix_20260910/before_on_frame46.png`, the BUILD9 picture in
which the defect was seen.

**Two things the pictures cost, both measured:**

1. **Gate (j) is RED, and every stray pixel is a joint marker.** 53 of them,
   all inside two round grey marks about 7 px across at (486,485) and
   (463,503), colour (134,139,145) -- the muted "not a bone" colour, not the
   bone white. They are the joint markers of two of the 19 marker-only nodes,
   which this lane's shipped rule KEEPS on purpose ("The other 19 node(s) --
   camera, anim-object, weapon, attach and root nodes ... get a joint marker
   and no bone"). Gate (j) as pre-registered asks that every pixel the overlay
   drew be on the character. The rule and the gate disagree and only bungo can
   say which gives: leave the markers and relax (j) to segments, or stop
   drawing a marker for a node that projects off the character. The gate's own
   floor is honest -- (j') confirms the BUILD9 picture still fails (j).
2. **Gates (c) and (e) are not stable, and this is not a regression claim.**
   Four identical runs of the spell on this one exe:

   | run | (c) pixels outside the mask | (e) pixels differing after toggling off | in-app total |
   |---|---|---|---|
   | 1 | 10 | 17 | 27 checks, 2 failures |
   | 2 | **0** | **0** | 27 checks, **0 failures, PASS** |
   | 3 | 4 | 37 | 27 checks, 2 failures |
   | 4 | **0** | 2 | 27 checks, 1 failure |

   Both checks demand EXACT equality on a `grabFramebuffer()` result that is
   not bit-stable between repaints, and the counts are 0-37 pixels out of
   ~800,000. What is NOT settled: whether part of that is a real one-frame lag
   in the toggle rather than GL jitter. The discriminator is a run that
   settles the frame before each grab, or a tolerance stated in the check.

**The WW_RENDER_SIZE law, measured on this exe** (this lane logged its pictures
coming out 1437x941 from a request of 1000x1000):

| requested | measured |
|---|---|
| 1000x1000 | **1293x941** |
| 1500x1059 | **1500x1000** |
| 1800x800 | **1800x741** |

Height is exactly `requested - 59` in all three; width is
`max( requested, 1293 )`. The hook does `skope->resize( rw, rh )`
(`src/nifskope_ui.cpp` ~21636) and a QMainWindow will not go below its own
minimumSizeHint, so a width under that floor is silently raised to it. **The
floor is build-dependent** -- it was 1437 on the 15:52:46 exe this lane
measured and is 1293 on the 17:08:39 one -- so it is measured per build, never
remembered. The pictures above were re-rendered at 1500x1059.
""" + COMMON,

    "scratchpad/lane_hkxedit1_report.md": """
## Build (BUILD11)

**The hook-up needed finishing, twice.** `hookup.py` (check) printed all ten
anchors `count=1` exactly as `PENDING.md` predicted. `--apply` wrote
`NifSkope.pro` and `src/nifskope.h` and then died:

    AssertionError: CR count moved in ...\\src\\nifskope.cpp

The script's own defect, not the tree's: `src/nifskope.cpp` is mixed, the four
regions it edits are CRLF, and the script converts its inserted text to CRLF --
so the CR count MUST grow by exactly the CRs of that text
(`ww-anchored-hookup` s1), not stay equal. Finished with
`scratchpad/build11_20260910/hookup1_rest.py`, which imports this script's own
`EDITS` table (nothing retyped) and applies only the two files it never wrote:
`src/nifskope.cpp` 440,714 -> 442,322 (+1,608), CR 9,520 -> 9,556 (+36,
expected +36), 6 markers; `src/nifskope_ui.cpp` +119, CR 0, 1 marker.

Then the syntax pass found what no lane could have seen alone: the inserted
code needed INCLUDES that no hook-up carried. `src/nifskope_ui.cpp:24298`
`invalid use of incomplete type 'class HkxModel'` -- the
`WW_ANIMWS_HKXMODEL` branch reads `hkx->undoStack` and `nifskope.h` only
forward-declares the class. Fixed with one line, marked `(lane BUILD11)`.

**The owed QUndoGroup was NOT written, because it already exists.** This
report's section 6 owes "a QUndoGroup so Edit > Undo reaches the .hkx
document". Lane HKXEDIT2's hook-up does exactly that and it is in this build:
`src/nifskope_ui.cpp` now creates the window's actions from
`wwAnimUndoGroup()->createUndoAction( ... )` / `createRedoAction( ... )`
instead of `nif->undoStack`, and the dock block calls
`animws->installUndoGroup( nif->undoStack, hkx->undoStack )`, which adds both
stacks to that one group (`src/animworkspace.cpp:61` and `:207`), the active
one following keyboard focus. A second group would have been a duplicate.

**Gates.**

* `python tests/spells/hkxfile_gates.py`: **109 checks, 0 failures, 37.3 s**,
  re-derived here on the 15,320-file census set (already on disk, nothing
  regenerated). The driver `release/hkxfile_gate.exe` (16:00:55) is newer than
  `src/hkxfile.cpp` (15:48:07) -- the gate-driver rule, lane BUILD8.
* `bash tests/spells/hkxmodel_test.sh`, **first run ever: 3 checks, 1 failure.**

**The one red, with its mechanism, and NOT fixed here.** The harness reports

    ok   the document loaded
    ok   the Block Details view `tree` exists
    FAIL (a) the tree's model is an HkxModel after an .hkx load
    the tree's model is NifModel

The document really does load: the harness's `ok` is the `ok` that
`NifSkope::load()` emits, which is `HkxModel::loadFromFile( fname )`'s return,
and `load()` does run `tree->setModel( hkx )`. What takes it back is the very
next statement, `emit completeLoading( ok, fname )` ->
`NifSkope::onLoadComplete` -> `swapModels()` (`src/nifskope.cpp:7478`).
`swapModels` knows exactly two states:

    if ( tree->model() == nif ) { ... park on nifEmpty ... }
    else                       { ... tree->setModel( nif ) ... }

With the tree on `hkx`, the else branch fires and puts `nif` back. The `.kfm`
route this was modelled on does not hit it because a .kfm has its OWN view,
`kfmtree`, which `swapModels` names explicitly; the .hkx route reuses `tree`,
which `swapModels` owns. The shape of the cure is plain (a third state, or the
route re-asserting itself after `completeLoading`), but it is a behaviour
change in another lane's file and a build lane's product is a verdict, not a
cure (`nifskope-ww-resume-pending` s6). **The refuter, if this is wrong:** the
tree would have been left on `nifEmpty`, not `nif` -- `onLoadBegin` parks it
there -- and the harness printed `NifModel`.

Everything after (a) in that harness is therefore unreached: 6 blocks in file
order, numFrames 23 -> 24 with undo/redo, the byte-identical unedited save, and
`animFile()` decoding 23 frames are all still unproven IN THE APPLICATION. They
are proven outside it by `hkxfile_gates.py`'s 109/0.
""" + COMMON,

    "scratchpad/lane_hkxedit2_report.md": """
## Build (BUILD11)

**The hook-up refused, and the refusal was its own.** `hookup.py` printed
`tier 2 (WW_ANIMWS_HKXMODEL): yes` and 15 anchors `count=1`, then:

    NifSkope.pro  after  NO anchor matches once:
      [('DEFINES += WW_HKXCLIP_CANON\\n', "'\\n'", 0), (..., "'\\r\\n'", 0)]

The TIER2 edit anchors on the `DEFINES += WW_HKXCLIP_CANON` line that base edit
#3 INSERTS, and both `--check` and `--apply` resolve every anchor against the
bytes on disk before anything is written -- so it can never count 1 in one pass.
`PENDING.md` predicted "16 anchors"; the measured truth is **15 in pass one and
the 16th only after pass one has landed**.
`scratchpad/build11_20260910/hookup2_apply.py` splits the two passes and
imports `EDITS`, `TIER2`, `DOCK_BLOCK` and `vocab_text` from this lane's own
script, keeping every rule (LF then CRLF per anchor, the inserted text must not
already be present, CR moves by exactly the inserted CRs, HKXEDIT1 first).
Applied: NifSkope.pro 4 + 1 edits (19,613 -> 20,185), nifskope.h 3 (-> 48,915),
nifskope_ui.cpp 7 (-> 1,488,609), nifskope.cpp 1 (CR 9,556 -> 9,558, +2 as
predicted). Then two includes the hook-up did not carry
(`<QUndoGroup>` for the Undo/Redo actions, `"animworkspace.h"` in nifskope.cpp
for the `select()` edit), found by `sx_BUILD11.sh`, marked `(lane BUILD11)`.

**`tests/spells/animws.sh`, first run ever: 57 checks, 0 failures, 1 skip,
PASS.** Not one of the four first-run defects this lane named actually fired
except the one guarding gate (i). Measured green, among others: 78 bone rows
with 93 keys each and 0 rows off, the ruler at 60 fps and the rate row reading
`60 fps`, the readout `frame 46 / 92 - 0.767 s` with the playhead at 46,
`LLeg_Thigh` as track 3 / row 4, row -> viewport selection (block 10 both ways)
and viewport -> row (`COM` row 2), the 30-degree key at frame 46 reading back
**0 deg off** from the document AND **0 deg off** from the viewport node with
every other bone and frame byte-identical, trim 10..50 -> **41 frames** with
frame 0 equal to old frame 10 exactly and the scene range following to
0.666667 s, retime 60->30 -> **47 frames** at ruler 30 with **0** coincident
frames differing, root motion **487.643 -> 0** on bake and byte-identical on
unbake in both the track and the extracted motion, Save -> 6 objects, 95 tracks
x 93 frames, 429,728 bytes on disk, re-read bit for bit, and Undo of the retime
back to 93 frames at 60. Pictures written:
`scratchpad/hkxedit2_20260910/dock_frame46.png` (34,589 B) and
`viewport_gizmo.png` (47,217 B).

**The one skip is not a pass, and chasing it found a real defect.** Gate (i)
prints a named SKIP on the default `SEQNIF`, `10mmPistol.nif`, which carries no
`NiControllerSequence` -- exactly as `PENDING.md` allowed for. Re-run with one
that does, `E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Effects/
TeleportInFXLight.nif`, the harness **dies inside gate (i)**: the log stops
after gate (f) and no `N checks, M failures`, no `PASS`/`FAIL` and no `done`
line is ever written. The NIF is not at fault -- it opens and renders on its own
in this exe (`scratchpad/build11_20260910/seqnif_probe.png`, rc=0). This lane
named the branch itself: "the `(i)` branch opens a second file through
`NifSkope::openFile` and waits 2.5 s -- if the load is slower the checks read
the old model". Gate (i) is unproven in either direction and is not fixed here.

**The standalone document gate, re-derived rather than accepted:**
`scratchpad/hkxedit2_20260910/build_gate.sh` rebuilt
`release/hkxclipedit_gate.exe` and ran it -- **72 checks, 0 failures**,
`GATE-RC=0`, and with HKXPACK `FootLeft` seen 2 in the saved file and 1 in the
resaved jog, `GATE-RC-WITH-HKXPACK=0`.

**Neighbours the change reaches**, since `src/hkxplayback.{h,cpp}` moved:
`tests/spells/hkxanim_play.sh` **27 checks, 0 failures, PASS**;
`tests/spells/hkxanim_ui.sh` **48 checks, 1 failure** -- BUILD9's own known
red, gate (g)'s wheel floor, which cannot fire because a WW harness window is
never activated; `tests/spells/files_tab.sh` **28 checks, 2 failures**, and
both are BUILD9's two pre-registered reds by name (Qt's own
`QLineEditIconButton` clear buttons untipped, 2 of 6; `PipboyBone`, which the
fixture drives with its own `NiTransformController`). The total is one below
BUILD9's 29 and which check moved was not traced -- neither number is a
failure.
""" + COMMON,
}


def main():
    apply = "--apply" in sys.argv
    for rel, text in SECTIONS.items():
        p = os.path.join(REPO, rel)
        b = open(p, "rb").read()
        cr = b.count(b"\r")
        print("%-42s bytes=%d CR=%d  already=%s" %
              (rel, len(b), cr, b"## Build (BUILD11)" in b))
        assert cr == 0, "%s is not LF-only" % rel
        assert b"## Build (BUILD11)" not in b
        if apply:
            add = text.encode("utf-8")
            assert add.count(b"\r") == 0
            open(p, "wb").write(b + add)
            c = open(p, "rb").read()
            print("    wrote: %d -> %d (+%d)  CR %d" % (len(b), len(c), len(c) - len(b), c.count(b"\r")))
    if not apply:
        print("--check only; nothing written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
