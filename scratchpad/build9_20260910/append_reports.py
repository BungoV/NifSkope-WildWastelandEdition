#!/usr/bin/env python3
"""Lane BUILD9, 2026-09-10 -- append a `## Build (BUILD9)` section to each of the
three lane reports. APPEND only: no line of a lane's own text is rewritten
(CONSTITUTION 8). Default --check, --apply writes.
"""
import sys

MARK = "## Build (BUILD9)"

COMMON_MTIMES = """
### The clocks, in one table

| artefact | time |
|---|---|
| `release/NifSkope.exe` (final) | 2026-09-10 15:52:46 |
| `release/style.qss` | 15:52:46, `cmp` equal to `res/style.qss` |
| FILESTAB hook-up applied | 15:17 |
| FILESTAB first gated exe | 15:20:34, re-gated on 15:25:05 and 15:52:46 |
| HKX3 hook-up applied | 15:29 |
| HKX3 gated exe | 15:32:06, re-gated on 15:35:05 and 15:52:46 |
| SKELOVERLAY hook-up applied | 15:40 |
| SKELOVERLAY gated exe | 15:36:50, re-gated on 15:52:46 |
| alignment before-measurement | 15:43:14 exe |
| alignment after-measurement | 15:52:46 exe |

Exe-newer sweep over every path `git status --porcelain -- src res tools tests`
reports: 0 stale at each gate run.
"""

BLOCKS = {
"scratchpad/lane_filestab_report.md": """

## Build (BUILD9)

Built and gated 2026-09-10 by lane BUILD9. BUILD8 never wrote a `DONE` marker;
its folder's newest file was 14:56:05 and `tasklist` showed nothing running at
15:16, so it was finished-or-dead and the slot was taken.

`hookup.py --check` at 15:17: 71 edits, every anchor matched as declared.
`--apply`: five files, byte and CR deltas exactly as predicted
(`src/nifskope.cpp` CR 9,379 -> 9,520 = the +141 CRLF lines inserted; the other
four LF-only and unchanged at CR 0). Syntax pass ALL-RC=0 in both
configurations. qmake RC=0, make RC=0. Dependency read-back by object:
`src/filestab.h` is named by `filestab.o`, `filestabtest.o`, `nifskope.o` and
`nifskope_ui.o`; `src/hkxplayback.h` by `nifskope.o` among nine.
`grep -c "lane FILESTAB"`: pro 0, `nifskope.cpp` 6, `nifskope.h` 2,
`nifskope_ui.cpp` 1.

### The gate table (final exe 15:52:46)

| gate | number | verdict |
|---|---|---|
| (1) | 0 strings say NIF; seeded offender found = 1, then 0 again | pass |
| (2) | `.nif 1 .bto 1 .btr 1 .hkx 14939 .gltf 0 .lodl 0 .lodt 0`; first `.hkx` `meshes/actors/_testcharacter/behaviors/_testcharacter.hkx` | pass, floor pass |
| (3) | 78 play / 17 named with no node / 4 by case; clip count +1; became the playing sequence | pass |
| (4) | posed 78 of 139; after unload **1 of 139 differs -- `PipboyBone`** | FAIL, cause measured |
| (5) | 0 group boxes; 6 tool buttons, **2 untipped, both `QLineEditIconButton`**; 2 placeholders; loaded list below browser (516 / 549) | FAIL, cause measured |
| (6) | refuses in words with 0 named nodes, loads nothing | pass |
| (shot) | `scratchpad/filestab_20260910/dock_after.png` written | pass |

**29 checks, 2 failures.** Before picture:
`scratchpad/filestab_20260910/dock_before.png`, taken from the OLD exe at
15:16:33 through `WW_LOADEDNIFS_TEST` before anything was applied -- tab "NIFs",
"Available NIFs", "Search loaded NIFs...", "Loaded NIFs / 2". After:
`dock_after.png` -- "Files", "Available files", "Search loaded files...",
"Loaded file / 1", and the animation archive's folders in the tree beside the
loose `ww` folder. The pair shows the RENAMES; it is not a pixel diff, because
the two harnesses open different fixtures.

### The two reds, measured, not amended

* **(5)** the two untipped buttons are `QLineEditIconButton` -- the clear buttons
  Qt creates inside a `QLineEdit` with `setClearButtonEnabled(true)`, one per
  search field. They are not controls this lane put on the row. Narrowing the
  gate's population after seeing its numbers is what CONSTITUTION 1 forbids, so
  it stays red; excluding Qt's internal buttons is bungo's call.
* **(4)** the node is `PipboyBone`, and `NifSkope -no-gui list` on the fixture
  shows `[72] NiNode 'PipboyBone'` followed by `[73] NiTransformController`: the
  fixture's own animation drives it. The gate snapshots the bind pose at t=0,
  scrubs to t=mid, unloads and compares WITHOUT stepping back, so the scene is
  still at the clip's time. Corroborated twice: `hkxanim_play.sh`'s restore gate
  is 27/0 on `skeleton.nif`, which has no such controller, and lane HKX3's gate
  (d), which DOES step after unloading, reads 0 of 139 on this same fixture.
  **For bungo: unloading a clip leaves the scene at the clip's time.**

### Repairs made to the instruments (not to the assertions)

1. `tests/spells/files_tab.sh` built its loose fixture tree with `mktemp -d`;
   `winpath()` only rewrites drive-style `/x/...` paths, so the binary was handed
   `/tmp/tmp.XXXX/Data`, could not open it, and the first run's census read
   `.nif 0 | .bto 0 | .btr 0 | .hkx 14939`. The tree now lives under
   `scratchpad/filestab_20260910/fixture_tree` and the script refuses if
   `winpath` does not return an `X:/` path.
2. The panel-style floor blanked `tools.first()`, which was already untipped, so
   it could not fire. The victim is now the first button that currently HAS a
   tooltip. The floor passes.
3. `src/filestabtest.cpp` now NAMES what fails: the nodes that did not restore,
   and the class/object name of every untipped button. Both reds only became
   leads once it did.

### The neighbours

`loaded_nifs.sh` went 3 failures -> 9 on the renames, because
`WW_LOADEDNIFS_TEST` asserts the expected TEXT of six labels this lane renamed.
Only the expected literals were updated
(`scratchpad/build9_20260910/fix_loadednifs_expect.py`, 6 anchors x1, CR 0 -> 0);
no check name, assertion or widget was touched, and it went back to **166 checks
/ 3 failures** -- the same three the before-run had.
`hkxanim_play.sh` 27 checks / 0 failures.

### Still owed / not measured

* the ARCHIVE branch of the `.hkx` open (the `QTemporaryDir` staging): gate (3)
  uses a loose path, so it is untested by every gate here;
* `.lodl` / `.lodt` remain accepted by the predicate but unreachable, because the
  tree keeps only paths under `meshes/`;
* the drop route for an `.hkx` dropped on the window is lane HKX3's, and its
  gate (e) is a SIMULATED drop -- a real Explorer drag is bungo's to try.
""",

"scratchpad/lane_hkx3_report.md": """

## Build (BUILD9)

Built and gated 2026-09-10 by lane BUILD9, after FILESTAB and before
SKELOVERLAY.

`hookup.py --check`: nine anchors, each still matching exactly once after
FILESTAB's 71 edits to the same two files. `--apply`: `NifSkope.pro`
19,044 -> 19,350 and `src/nifskope_ui.cpp` 1,476,506 -> 1,478,429, CR 0 -> 0 on
both. `hkxanimui.h` named by four objects (`hkxanimui.o`, `hkxanimuitest.o`,
`nifskope_ui.o`, `timeline.o`), `GeneratedFiles/.moc/moc_hkxanimui.cpp` present,
`WW_HKXANIM_UI` in `Makefile.Release`.

### The gate table (final exe 15:52:46)

| gate | expected | measured | verdict |
|---|---|---|---|
| (a) | one new row, 93 frames, 60 fps, the ROW says both | one row, 93 @ 60, length 1.53333 s | pass |
| (b) | worst translation <= 1e-4, rotation <= 0.01 deg at frame 46 | 78 bound nodes, translation **0**, rotation **1.72665e-05 deg**; frame-0 floor red at 299.83 / 104.964 deg | pass |
| (c) | `frame 46 / 92`; speed 1 -> 2 -> 1; Loop flips; cycle = CycleLoop | all four, readout `frame 46 / 92 - 0.767 s` | pass |
| (d) | 0 nodes differ after unload, > 0 while posed | posed 78 of 139, after unload **0 of 139** | pass |
| (e) | drop accepted, same row, same 93/60; junk floor adds no row | enter 1 / drop 1, rows 2 -> 3; junk 0 / 0, rows 2 -> 2 | pass |
| (f) | `skeleton.hkx` is a row marked refused with a sentence | row 3, text "skeleton - refused", reason in words, reaches the summary line | pass |
| (g) | 0 unstamped number fields of >= 3; 0 group boxes; 0 unstyled selectors of >= 1; 6 of 6 with tooltips; summary line and list outside the splitter; the wheel guard both ways | 3 fields 0 unstamped, 0 boxes, 1 selector 0 unstyled, 6 of 6, both outside; wheel guard's UNFOCUSED half passes, the focused half **cannot fire** | 1 FAIL |
| picture | > 400x80 | `scratchpad/hkx3_20260910/dock_clip_midclip.png`, 1549x284 | pass |

**48 checks, 1 failure, 0 skips.** Regression gate `hkxanim_play.sh`: **27
checks, 0 failures**, 78 / 17 / 4 on `fixtures/human_male_vanilla.nif` -- HKX2's
playback underneath is untouched.

### The one red is an instrument that cannot fire, and it was measured

Gate (g)'s floor asks the wheel to step the Speed field once it has focus. The
harness now prints the focus state beside the result:
`after setFocus: hasFocus no, focusWidget <none>, window active no`. A WW
harness window is deliberately never activated (`WW_WINDOW_AT`, no `raise()`),
and `QWidget::hasFocus()` is false in an inactive window however many times
`setFocus()` is called -- so `wwGuardWheel`, which blocks the wheel exactly
while `!hasFocus()`, is behaving as specified and the floor has no way to reach
the other half. Not a defect in the guard, and not a pass either.

### Two figures in PENDING.md that were wrong

* `grep -c "lane HKX3" NifSkope.pro src/nifskope_ui.cpp` prints **1 and 6**, not
  1 and 7: six of the nine edits reach that file and each carries one marker.
* `grep -c "TimelineSeqBox" release/NifSkope.exe` can never be >= 1.
  `QStringLiteral` compiles to UTF-16; an ASCII grep finds 0 in an exe that
  contains the string twice. `b.count("TimelineSeqBox".encode("utf-16-le"))`
  finds both, and `grep -c WW_HKXANIM_UI Makefile.Release` is the check that
  actually proves the define reached the compiler.

### The build trap this hook-up walked into

Edit 3 adds `DEFINES += WW_HKXANIM_UI`. `make` compares mtimes, not flags, so
`timeline.o` (15:19:10, newer than its source) was kept -- compiled WITHOUT the
define, with `setGLView` compiled out -- while the fresh `nifskope_ui.o` called
it. The link failed on one undefined reference and DELETED
`release/NifSkope.exe` on the way out. Six objects had to be removed by hand
before the rebuild. In MISTAKES.md and in `nifskope-ww-build-verify`, both trees.

### Still owed

A REAL Explorer drag onto the window: gate (e) is a simulated drop through
`QApplication::sendEvent`, which does run the application event filter, but a
native drop arrives by another route. That half is bungo's to try.
""",

"scratchpad/lane_skeloverlay_report.md": """

## Build (BUILD9)

Built and gated 2026-09-10 by lane BUILD9, after FILESTAB and HKX3.

`hookup.py --check`: 5 of 5 anchors match once, no text already present.
`--apply`: `NifSkope.pro` +27 bytes and `src/nifskope_ui.cpp` +1,901, exactly as
predicted, CR 0 -> 0 on both, `grep -c "lane SKELOVERLAY"` = 4 as predicted.
qmake RC=0, make RC=0, `src/skeloverlaytest.cpp` in `Makefile.Release` at three
places. Two-header object sweep over `src/glview.h` and `src/gl/glscene.h`: no
stale object. Exe newer than every changed path; sheet in step.

### The gate table (final exe 15:52:46)

| gate | measured | verdict |
|---|---|---|
| (a) | dock All 130 / Bones 93 / Deforming 93 / Unused 0; overlay census nodes 130, segments 129, stubs 78, missing 0, draws 3 | pass |
| (a') | overlay OFF census all zeros | floor pass |
| (b) | bone-coloured 93 = Bones 93; deforming 93 = Deforming 93; unused 0 = Unused 0; muted 37 = All 130 - Bones 93 | pass |
| (c) | 14,390 pixels changed (2.412%); **0 outside the mask** the overlay reports having drawn | pass |
| (c') | some pixels differ, and the mask covers 12.23% of the frame, not all of it | floor pass x2 |
| (d) | at frame 46, worst joint-to-animated-node distance **0 units** | pass |
| (d') | the same joints against the BIND pose disagree on 122, largest 367.6 units | floor pass |
| (e) | toggling off restores the off-render byte for byte: 0 pixels differ | pass |

**17 checks, 0 failures, PASS.**

### The pictures

Through the render hook, one pinned orthographic camera (`WW_RENDER_VIEW=5`,
centre 0,0,62, ortho 80, 1000x1000, clean), three distinct md5s:
`scratchpad/skeloverlay_20260910/off.png` (bind pose, overlay off),
`on.png` (bind pose, overlay on) and `on_frame46.png` (the clip at frame 46).
The gate's own evidence is under `gates/`, including `gate_mask.png` with the
mask painted dark grey and every changed pixel orange -- every orange pixel is
inside the grey. All three were regenerated on the FINAL exe, after the
alignment change, so what was delivered is what ships.

### The pose-armature factoring: NOT settled, and here is exactly what is known

`WW_SKELETON_TEST` PASSES (129 bones drawn on the vanilla `skeleton.nif`, footer
and tree agree). `WW_POSEDRAW_TEST` FAILS at "clicking a bone did not make it
the active object", and it fails on BOTH fixtures available here --
`fixtures/human_male_vanilla.nif` (130 bones drawn) and the vanilla
`skeleton.nif` (129) -- with `poseBoneAt` at a bone's own drawn screen position
resolving block 0 in each, so the probe the click is compared against is 0 while
the click legitimately selects a real block (152 and 155).

The factoring is arithmetically identical by diff:
`characteristicBoneSize()` is `refreshPoseBoneSize()`'s body with the bone list
parameterised and -1 standing in for the early return (the caller keeps its
previous size, which is what the early return did), and `boneTailIn()` is
`poseBoneTail()`'s body with the list and the cap parameterised. So it is not a
candidate for this failure. WW_CHANGES.md records this harness last green on a
FACIAL rig of 70 bones with an L/R pair -- neither fixture here is that rig, and
both skip the mirror check for want of a pair. **Reported, not cured**
(`nifskope-ww-resume-pending` rule 6: the resuming lane's product is a verdict).
What would settle it: the facial rig the 2026-09 entry used, run once.

### Not measured

Whether the armature is legible on a dense facial rig. No instrument measures
that; the picture is the only one and bungo is the judge.
""",
}


def main():
    apply = "--apply" in sys.argv
    for path, block in BLOCKS.items():
        raw = open(path, "rb").read()
        cr0, lf0, n0 = raw.count(b"\r"), raw.count(b"\n"), len(raw)
        text = raw.decode("utf-8")
        if MARK in text:
            print("SKIP %s: already appended." % path)
            continue
        out = (text.rstrip("\n") + "\n" + block.rstrip("\n") + "\n"
               + COMMON_MTIMES.rstrip("\n") + "\n").encode("utf-8")
        print("%-44s bytes %d -> %d   CR %d -> %d" % (path, n0, len(out), cr0, out.count(b"\r")))
        if out.count(b"\r") != cr0:
            print("REFUSED: CR moved on %s." % path)
            return 1
        if apply:
            open(path, "wb").write(out)
    print("--apply: written." if apply else "--check: clean. Nothing written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
