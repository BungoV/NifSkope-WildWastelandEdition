# Lane BUILD11 -- report (incremental)

Started 2026-09-10 ~16:58. Holds the build slot
(`scratchpad/build11_20260910/BUILDING`). Three BUILD PENDING lanes resumed
into ONE build, in the order the briefs gave: SKELFIX, HKXEDIT1, HKXEDIT2.
Nothing committed.

Process guard before any write: `tasklist | grep -i -E "Fallout4|NifSkope"` ->
**rc=1** (no game, no NifSkope).

Baselines measured with Python byte counts before the first edit:

| file | bytes | CR | LF |
|---|---|---|---|
| src/nifskope_ui.cpp | 1,484,604 | 0 | 31,680 |
| src/nifskope.cpp | 440,714 | 9,520 | 10,669 |
| src/nifskope.h | 48,509 | 0 | 1,171 |
| NifSkope.pro | 19,488 | 0 | 743 |
| WW_CHANGES.md | 1,546,192 | **19,020** | 26,060 |
| src/glview.cpp | 872,220 | 23,423 | 23,494 |
| src/glview.h | 99,195 | 0 | 1,984 |

`release/NifSkope.exe` on entry: **16:45:53**, 20,007,936 bytes (lane BUILD10's).

## 1. The three hook-ups

### 1a. SKELFIX -- applied as written, prediction exact

`python scratchpad/skelfix_20260910/hookup.py --check` printed
`anchor x1 (CR 0, LF 31680) / would grow by 905 bytes; CR 0 -> 0; marker x2`,
and `--apply` produced exactly that: `src/nifskope_ui.cpp` 1,484,604 ->
1,485,509 (+905), CR 0, `grep -c "lane SKELFIX"` = **2**. Every prediction in
`PENDING.md` step 1 held. The rest of SKELFIX was already in the tree
(glview.cpp 5 markers, glview.h 3, skeloverlaytest.cpp 2,
skeleton_overlay.sh 2, skeleton_overlay_mask.py present) -- re-verified, not
re-applied.

### 1b. HKXEDIT1 -- 10/10 anchors, then the script DIED half-applied

`hookup.py` (check) printed all ten anchors `count=1` exactly as the resume
predicted (NifSkope.pro x3, src/nifskope.h x2, src/nifskope.cpp x4 CRLF,
src/nifskope_ui.cpp x1). `--apply` wrote `NifSkope.pro` and `src/nifskope.h`
and then raised

    AssertionError: CR count moved in ...\src\nifskope.cpp

That is the script's own defect, not the tree's: `src/nifskope.cpp` is mixed,
the four regions it edits are CRLF, the script converts the inserted text to
CRLF -- so the CR count MUST grow by the CRs of that text. `ww-anchored-hookup`
section 1 says exactly that ("assert the CR count moves by exactly the CRs of
the inserted text"); the script asserted equality instead. Two files were
written, two were not.

Finished with `scratchpad/build11_20260910/hookup1_rest.py`, which IMPORTS the
`EDITS` table from the lane's own script (nothing retyped), applies only the
two files it never wrote, and asserts the CR delta rather than CR equality:

| file | bytes | CR | markers |
|---|---|---|---|
| NifSkope.pro | 19,488 -> 19,613 | 0 | (inserted text x3, the .pro lines carry no marker string) |
| src/nifskope.h | 48,509 -> 48,681 | 0 | 2 |
| src/nifskope.cpp | 440,714 -> 442,322 (+1,608) | 9,520 -> 9,556 (+36, expected +36) | 6 |
| src/nifskope_ui.cpp | 1,485,509 -> 1,485,628 (+119) | 0 | 1 |

**The owed QUndoGroup does NOT need writing.** HKXEDIT1's resume owes "a
QUndoGroup so Edit > Undo reaches the .hkx document". HKXEDIT2's hook-up
already does it: it replaces the two action lines in `src/nifskope_ui.cpp`
with `wwAnimUndoGroup()->createUndoAction( ... )` /
`createRedoAction( ... )`, and its dock block calls
`animws->installUndoGroup( nif->undoStack, hkx->undoStack )`, which adds both
stacks to that one group (`src/animworkspace.cpp:61`, `:207`). The active
stack follows keyboard focus. No second group was written.

### 1c. HKXEDIT2 -- 15 base anchors, and the tier-2 anchor cannot exist yet

`hookup.py` (check) printed `tier 2 (WW_ANIMWS_HKXMODEL): yes` and 15 anchors
`count=1`, then **refused**:

    NifSkope.pro  after  NO anchor matches once:
      [('DEFINES += WW_HKXCLIP_CANON\n', "'\n'", 0), (..., "'\r\n'", 0)]

The tier-2 edit anchors on the `DEFINES += WW_HKXCLIP_CANON` line that its own
base edit #3 INSERTS, and both the check and the apply resolve every anchor
against the file as it is before anything is written. No single pass can
satisfy it. PENDING.md predicted "16 anchors"; the measured truth is **15 in
pass one, and the 16th only after pass one has landed**.

`scratchpad/build11_20260910/hookup2_apply.py` splits the passes and imports
`EDITS`, `TIER2`, `DOCK_BLOCK` and `vocab_text` from the lane's script -- same
rules kept (LF tried then CRLF, the ending that counts 1 wins, the inserted
text must not already be present, CR moves by exactly the inserted CRs), plus
the same "HKXEDIT1 first" refusal.

| file | edits | bytes | CR |
|---|---|---|---|
| NifSkope.pro | 4 + 1 (tier 2) | 19,613 -> 20,185 | 0 |
| src/nifskope.h | 3 | 48,681 -> 48,915 | 0 |
| src/nifskope_ui.cpp | 7 | 1,485,628 -> 1,488,609 | 0 |
| src/nifskope.cpp | 1 | 442,322 -> 442,452 | 9,556 -> 9,558 (+2, expected +2) |

Markers after: NifSkope.pro 2, nifskope.h 3, nifskope.cpp 1, nifskope_ui.cpp 7.
`grep -n "^DEFINES" NifSkope.pro` now shows `WW_HKXANIM_UI` (132),
`WW_HKXCLIP_CANON` (136), `WW_ANIMWS_HKXMODEL` (138).

## 2. The syntax pass found three missing includes

`sx_BUILD11.sh` (new, self-contained, carrying the three WW defines so
`nifskope_ui.cpp` is checked as it will be compiled) on the two hooked-up
files:

    src/nifskope.cpp:7807  comparison between distinct pointer types 'QObject*' and 'AnimWorkspace*'
    src/nifskope.cpp:7808  invalid use of incomplete type 'class AnimWorkspace'
    src/nifskope_ui.cpp:23699 / :23704  invalid use of incomplete type 'class QUndoGroup'
    src/nifskope_ui.cpp:24298           invalid use of incomplete type 'class HkxModel'

One defect three times: a hook-up inserted a CALL into an existing file and no
INCLUDE beside it, so the type stayed the forward declaration the header
carries. `scratchpad/build11_20260910/fix_includes.py` adds three lines,
marked `(lane BUILD11)`, no behaviour:

* `src/nifskope.cpp` (CRLF): `#include "animworkspace.h"`
* `src/nifskope_ui.cpp` (LF): `#include <QUndoGroup>`, `#include "hkxmodel.h"`

Re-run: **ALL-RC=0**, both files clean.

## 3. The build

`qmake NifSkope.pro` first (three DEFINES lines and seven new SOURCES/HEADERS
across the two hook-ups): **QMAKE-RC=0**. The defines are in the Makefile, which
is how a define is proved to have reached the compiler -- never by grepping the
exe, `QStringLiteral` is UTF-16:

    grep -c WW_HKXANIM_UI Makefile.Release      1
    grep -c WW_HKXCLIP_CANON Makefile.Release   1
    grep -c WW_ANIMWS_HKXMODEL Makefile.Release 1

and every compile line in `build.log` carries all three.

**Dependency read-back**, per object, by the awk walk (`depcheck.py`), not
`grep -A3`: `DEPCHECK missing=0` over eleven objects --
`hkxmodel.o` names hkxmodel.h / hkxfile.h / hkxanim.h; `animworkspace.o` names
animworkspace.h / hkxclipedit.h / animdopesheet.h; `nifskope_ui.o` names
animworkspace.h / hkxmodel.h / nifskope.h / glview.h; `glnode.o` and
`skeloverlaytest.o` name nifskope.h and glview.h.

**The DEFINES trap** (MISTAKES 2026-09-10, lane BUILD9): make compares mtimes,
not flags. `scratchpad/build11_20260910/stale.sh` swept both ways -- the flag
sweep found 7 existing objects belonging to translation units that read one of
the three WW macros (`hkxanimui`, `hkxanimuitest`, `nifskope`, `nifskope_ui`,
`timeline`, `timelineedit`, `timelineviews`) and all 7 were deleted before
make; the header sweep found the 33 objects that include the changed
`src/nifskope.h`, all of which qmake's regenerated lists already name, so make
rebuilt them itself.

**The guard fired, and it was bungo.** The chain's own in-link guard (not the
lane's opening echo) found a NifSkope holding `release/NifSkope.exe`:

    running copy (pid 700) renamed aside      ->  release/NifSkope_inuse_700.exe
    BUILD-RC=0   CHAIN-RC=0

`Get-CimInstance Win32_Process` says pid 700 started **17:05:44** with command
line `"E:\Projects\...\release\NifSkope.exe"` and **no `--port`** -- the
discriminator in `nifskope-ww-build-verify`: no port = bungo's own window, not a
harness. It was renamed aside, never touched. The lane's entry guard had printed
`rc=1` at ~16:58; he opened it seven minutes later.

**Result: `release/NifSkope.exe` 2026-09-10 17:08:39, 20,693,504 bytes**
(BUILD10's was 16:45:53 / 20,007,936 -- +685,568 bytes).

* `cmp res/style.qss release/style.qss` -> **sheet in step** (copied at link
  time, 17:08:39).
* the two link-time copies both landed beside the exe:
  `release/hkclasses_fo4.json` 1,511,662 B and
  `release/hkx_annotation_vocabulary.txt` 55,299 B, both 17:08:39.
* **exe-newer sweep over the whole working set: 1 stale of 114 paths**, and it
  is not mine -- `src/watermark.cpp`, which lane WATER7 is editing in this same
  tree right now (code-only until this lane's DONE). Nothing in this build's
  gates reaches it; the director should know that the 17:08:39 exe does NOT
  carry WATER7's watermark edits.
* gate DRIVERS (build-verify, lane BUILD8's rule): `release/hkxfile_gate.exe`
  16:00:55 and `release/hkxclipedit_gate.exe` 16:28:39 are both newer than
  their own sources (`hkxfile.cpp` 15:48:07, `hkxclipedit.cpp` 16:28:24) -- and
  the clip-edit driver was rebuilt again here anyway.

## 4. The file-based gates, re-derived here

Neither number is accepted from the lane that wrote it; both were run again on
this tree by this lane.

| gate | driver | result |
|---|---|---|
| `python tests/spells/hkxfile_gates.py` | `release/hkxfile_gate.exe` (16:00:55, newer than `src/hkxfile.cpp` 15:48:07) + the Python oracle | **109 checks, 0 failures, 37.3 s** |
| `scratchpad/hkxedit2_20260910/build_gate.sh` | rebuilds and runs `release/hkxclipedit_gate.exe` | **72 checks, 0 failures**, `GATE-RC=0`; with HKXPACK, `FootLeft` seen 2 in the saved file and 1 in the resaved jog, `GATE-RC-WITH-HKXPACK=0` |

The census set `hkxfile_gates.py` needs was already on disk: **15,320 `.hkx`**
under `scratchpad/hkxedit1_20260910/census_hkx/` (it is gitignored and
`extract_census.py` regenerates it, but nothing had to be regenerated).

**The linked binary carries all three lanes' code** (`exestrings.py`, a check
that needs no launch and therefore ran while his window was open). Nine strings,
9 found, and the ASCII/UTF-16 split is the point -- `QStringLiteral` compiles to
UTF-16, so an ASCII grep alone would have called four of these missing:

    AnimWorkspaceDock          ascii=1  utf16=0
    AnimWsRate                 ascii=1  utf16=1
    WW_ANIMWS_TEST             ascii=1  utf16=0
    WW_HKXMODEL_TEST           ascii=2  utf16=0
    hkclasses_fo4.json         ascii=3  utf16=0
    no class database found    ascii=0  utf16=1
    WW_SKELOVERLAY_TEST        ascii=1  utf16=0
    Show Skeleton              ascii=1  utf16=0
    hkx_annotation_vocabulary  ascii=0  utf16=2

## 5. The in-app gate chain

His window closed at **17:16:19** and the chain ran from 17:16:33, one NifSkope
instance at a time, `rc=1` re-read before every launch.
`release/NifSkope_inuse_700.exe` was deleted once `tasklist` was clear;
`release/NifSkope_inuse_20560.exe` (BUILD10's rollback rung) was left alone.

| harness | first run? | result |
|---|---|---|
| `skeleton_overlay.sh` in-app | no | **27 checks, 0-2 failures** over four runs -- see the flake below |
| `skeleton_overlay.sh` picture (j) | no | **5 checks, 1 failure** (53 stray pixels = two joint markers) |
| `hkxmodel_test.sh` | **yes** | **3 checks, 1 failure** |
| `animws.sh` | **yes** | **57 checks, 0 failures, 1 skip, PASS** |
| `files_tab.sh` | no | 28 checks, 2 failures (BUILD9's same two, by name) |
| `hkxanim_ui.sh` | no | 48 checks, 1 failure (BUILD9's wheel floor, cannot fire unfocused) |
| `hkxanim_play.sh` | no | **27 checks, 0 failures, PASS** |
| `hkxfile_gates.py` | no | **109 checks, 0 failures** |
| `hkxclipedit_gate.exe` | no | **72 checks, 0 failures** + HKXPACK 2 / 1 |

Skipped, with the reason: `loaded_nifs.sh`, `top_bar.sh`, `ui_align.sh` -- this
build renames no user-visible string and moves no bar, so BUILD9's
sibling-label class of red cannot have been introduced; the `lodgen_*`,
`water_*`, `.lodl`/`.lodt` and impostor gates -- none of the three lanes
reaches the generator, the water tool or the terrain readers; `WW_POSEDRAW_TEST`
-- already failing before lane SKELFIX on both fixtures here, and SKELOVERLAY's
report says only a run on the 70-bone facial rig settles it.

### The four reds, each with its cause and none of them fixed

1. **`hkxmodel_test.sh` (a): `swapModels()` takes the view back off the .hkx
   document.** The document loads (the harness's `ok` IS
   `HkxModel::loadFromFile`'s return) and `NifSkope::load()` does
   `tree->setModel( hkx )`; the next statement, `emit completeLoading`, reaches
   `onLoadComplete` -> `swapModels()` (`src/nifskope.cpp:7478`), which knows
   only `nif` and `nifEmpty` and so puts `nif` back. The `.kfm` route escapes
   it because a .kfm has its own view. Refuter if this is wrong: the tree would
   read `nifEmpty`, which `onLoadBegin` parks it on; it read `NifModel`.
2. **`skeleton_overlay.sh` (j): 53 stray pixels, all joint markers.** Two round
   grey marks 7 px across, colour (134,139,145) = the muted "not a bone"
   colour, at (486,485) and (463,503) in the 1293x941 run. They are two of the
   19 marker-only nodes the shipped rule keeps deliberately. Rule versus gate;
   bungo's call which gives.
3. **`skeleton_overlay.sh` (c) and (e) are flaky.** Four identical runs:
   (c) 10 / 0 / 4 / 0 pixels outside the mask, (e) 17 / 0 / 37 / 2 differing
   after toggling off -- one run PASSed 27/0. Exact equality on a
   `grabFramebuffer()` that is not bit-stable. Unmeasured: whether some of it
   is a one-frame lag rather than jitter.
4. **`animws.sh` gate (i) has never completed.** SKIPs on the default
   `10mmPistol.nif` (no `NiControllerSequence`); given
   `Meshes/Effects/TeleportInFXLight.nif`, which has one, the harness dies
   inside (i) and writes no summary. The NIF opens and renders fine alone
   (`scratchpad/build11_20260910/seqnif_probe.png`).

## 6. Pictures, and the WW_RENDER_SIZE law

**The picture the director is owed** is
`scratchpad/skeloverlay_20260910/on_frame46.png`, **1500x1000**: the human
fixture at frame 46 of the Mixamo clip, overlay on, in the crouched mid-slide
pose, with the skeleton drawn entirely on the body -- spine, both arms out to
individual finger bones, both legs, the skull -- and no segment running off to
the world origin. The only marks off the body are the two grey joint-marker
dots of item 2 above. The before picture,
`scratchpad/skelfix_20260910/before_on_frame46.png`, is kept: it is gate (j')'s
only floor and (j') confirms it still fails (j).

Also written: `off.png`, `on.png`, `off_frame46.png`,
`gates/gate_frame46_mask.png` in the same folder;
`scratchpad/hkxedit2_20260910/dock_frame46.png` and `viewport_gizmo.png`.

**WW_RENDER_SIZE, measured three ways on this exe** (`rendersize.sh`, each PNG
read back with PIL):

| requested | measured |
|---|---|
| 1000x1000 | **1293x941** |
| 1500x1059 | **1500x1000** |
| 1800x800 | **1800x741** |

Height is exactly `requested - 59`; width is `max( requested, 1293 )`. The hook
does `skope->resize( rw, rh )` and a QMainWindow will not go below its own
minimumSizeHint, so a width under the floor is silently raised. **The floor
moves with the build** -- 1437 on the 15:52:46 exe (which is why SKELFIX's
pictures came out 1437x941), 1293 on this one -- so it is measured per build,
never remembered. `nifskope-ww-render-shot` amended in BOTH skill trees, which
are byte-identical after the edit.

## 7. Documents

* `WW_CHANGES.md`: HKXEDIT1's and SKELFIX's entries spliced under the title
  line, each followed by the measured status block; HKXEDIT2's entry retitled
  from "(BUILD PENDING)" to "(built and gated, lane BUILD11)" and its
  "NOT measured" paragraph replaced with the measured gate result and the
  unproven gate (i). **CR count 19,020 before and after, asserted three times**
  (the file grew 1,546,192 -> 1,574,208 bytes across this session, some of it
  lane WATER7's own entry).
* `MISTAKES.md`: nine entries in two batches -- the CRLF assertion that killed
  a hook-up half-applied, the tier-2 anchor that cannot exist in one pass, the
  three hook-ups with no includes, my own heredoc-backslash repeat, bungo's
  window appearing after the guard, and the four first-run gate findings.
* the three lane reports each gained a `## Build (BUILD11)` section.
* `nifskope-ww-render-shot` amended in both trees.

## 8. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used:** `nifskope-ww-resume-pending` (the read order, qmake before
make, the dependency read-back, the whole-working-set exe sweep, the sequential
harness chain, "measure the cause and STOP", the four documents, and s9's "every
number in a PENDING is a prediction" -- which paid three times here);
`nifskope-ww-build-verify` (the gated chain, `make`'s own `$?`, the rename-aside
that saved this build when bungo's window appeared, the DEFINES-staleness sweep,
the gate-driver `-nt` rule, the syntax pass that found five compile errors before
they cost a build); `ww-anchored-hookup` (its s1 CR rule is exactly what
HKXEDIT1's script got wrong, and its s3a candidate-anchor pattern is what
HKXEDIT2's script implements and then defeated itself with);
`ww-test-harness-add` (s5 floors, s7 "a SKIP is never a pass", s9 "a harness
that has never been executed is a draft" -- three of the four reds are that);
`nifskope-ww-render-shot` (absolute paths, one instance, opacity 0, and "never
quote a requested size -- read it back with PIL", which is the whole
WW_RENDER_SIZE finding); `ww-hkx-animation` (s12's absolute-path trap, s13 and
s14 for what the two HKX layers already settle).

**Amended:** `nifskope-ww-render-shot`, in BOTH trees, replacing "clamped to one
screen / 1400x1400 gives 1507x1067" with the measured
`max(width, floor) x (height - 59)` law, the three-request table, and the fact
that the width floor MOVES WITH THE BUILD. The two trees were byte-identical
before and are byte-identical after.

**Wished had existed, and why each will recur:**

* **"Finish another lane's half-applied hook-up."** Twice in one lane, and the
  fix shape was identical both times: import the failing script's own tables by
  path, re-derive which files it wrote from the INSERTED TEXT (never from the
  anchor, `ww-anchored-hookup` s4), and re-apply only the rest with the correct
  CR assertion. That is a procedure, and it is now written twice in
  `scratchpad/build11_20260910/hookup1_rest.py` and `hookup2_apply.py`. It
  belongs in `ww-anchored-hookup` as a "the apply died half-way" section rather
  than as a new skill -- the director should fold it in; I did not, because
  amending a skill section I only exercised twice on one evening is the kind of
  premature generalisation that has cost this repo before, and the two scripts
  are on disk as the reference the section would point at.
* **"Discharge a SKIP."** `ww-test-harness-add` s7 says a SKIP is never a pass
  and stops there. It does not say what to DO -- find a fixture that exercises
  the branch, re-run, and expect the branch's FIRST execution to be its own
  first-run defect. Doing that here turned one clean-looking `57/0 PASS` into a
  known crash. Recorded as MISTAKES entry 9 rather than as a skill edit, for the
  same reason.

**Declined:** no new skill for the build itself. `nifskope-ww-resume-pending`
plus `nifskope-ww-build-verify` covered every step of it, including the two
traps that fired (the DEFINES staleness and bungo's window), and a third
document restating them would drift from those two.
