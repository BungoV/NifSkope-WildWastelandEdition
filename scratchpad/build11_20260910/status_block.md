**BUILT AND GATED (lane BUILD11, 2026-09-10).** `release/NifSkope.exe`
**17:08:39**, 20,693,504 bytes; qmake before make; `DEPCHECK missing=0`; the
three WW defines each once in `Makefile.Release` and on every compile line;
`res/style.qss` = `release/style.qss`; `release/hkclasses_fo4.json` (1,511,662 B)
and `release/hkx_annotation_vocabulary.txt` (55,299 B) copied beside the exe;
exe newer than 113 of the 114 changed paths (the exception is
`src/watermark.cpp`, lane WATER7's live edit, not in this exe). Measured gates,
all re-derived rather than accepted:

| gate | result |
|---|---|
| `tests/spells/hkxfile_gates.py` | **109 checks, 0 failures**, 37.3 s (15,320 clips) |
| `release/hkxclipedit_gate.exe` | **72 checks, 0 failures**; HKXPACK sees `FootLeft` 2 / 1 |
| `tests/spells/animws.sh` (first run ever) | **57 checks, 0 failures, 1 skip**, PASS; the skip is gate (i), see below |
| `tests/spells/skeleton_overlay.sh` in-app | **27 checks**, 0-2 failures over four runs (see below); (f) longest drawn 31.94 <= 33.60, (f') old rule 300.51 red, (g) 0 endpoints outside worst 0.93, (g') old rule 19 outside worst 241.30, (h) segments 110 / stubs 62 / skipped 38, (i) armature 111 / marker-only 19, (d) worst 0 units, (d') 122 joints moved, largest 367.6 |
| `tests/spells/skeleton_overlay.sh` picture (j) | **5 checks, 1 failure** -- 53 stray pixels, and they are two joint markers, not segments |
| `tests/spells/hkxmodel_test.sh` (first run ever) | **3 checks, 1 failure** -- the .hkx document loads and the view is taken back off it |
| `tests/spells/files_tab.sh` | 28 checks, 2 failures -- BUILD9's same two by name |
| `tests/spells/hkxanim_ui.sh` | 48 checks, 1 failure -- BUILD9's wheel floor, which cannot fire unfocused |
| `tests/spells/hkxanim_play.sh` | 27 checks, 0 failures, PASS |

**Still red, measured, NOT fixed here** (a build lane's product is a verdict):

1. **`hkxmodel_test.sh` (a): the tree's model is `NifModel` after an .hkx
   load.** The document DOES load (`ok` is true, so `HkxModel::loadFromFile`
   returned true) and `NifSkope::load()` does set `tree->setModel( hkx )` --
   and then `emit completeLoading` reaches `NifSkope::onLoadComplete`, which
   calls `swapModels()` (`src/nifskope.cpp:7478`). That function knows two
   states: `tree->model() == nif` -> park on `nifEmpty`, ELSE -> put `nif`
   back. With the tree holding `hkx` the else branch fires and takes the view
   away. The .kfm route this was copied from does not hit it because a .kfm
   has its OWN view (`kfmtree`), which `swapModels` handles by name; the .hkx
   route reuses `tree`, which `swapModels` owns.
2. **`skeleton_overlay.sh` (j): 53 stray pixels, all of them two joint
   markers.** Every stray pixel is inside two round grey marks about 7 px
   across at (486,485) and (463,503), colour (134,139,145) -- the muted
   "not a bone" colour. They are the joint markers of two of the 19
   marker-only nodes (camera / anim-object class), which the shipped rule
   KEEPS by design: "The other 19 node(s) ... get a joint marker and no bone."
   Gate (j) as pre-registered says every pixel the overlay drew is on the
   character. The rule and the gate disagree; which one gives is bungo's call.
3. **`skeleton_overlay.sh` (c) and (e) are not stable.** Four identical runs
   on this one exe: (c) pixels outside the mask **10, 0, 4, 0**; (e) pixels
   differing after toggling the overlay off **17, 0, 37, 2** -- including one
   run that was 0 and 0 and printed PASS. Both checks demand exact equality on
   a GL framebuffer that is not bit-stable between repaints, so they are
   measuring jitter as often as they measure the code. Untested either way:
   whether some of that is a one-frame lag in the toggle rather than jitter.
4. **`animws.sh` gate (i) has never completed.** With the default
   `10mmPistol.nif` it prints a named SKIP (that NIF has no
   `NiControllerSequence`). Given one that has --
   `Meshes/Effects/TeleportInFXLight.nif` -- the harness dies inside gate (i)
   and writes no summary line at all. The NIF itself is fine: it opens and
   renders on its own in this exe. The lane named this branch as a likely
   first-run defect ("opens a second file through `NifSkope::openFile` and
   waits 2.5 s").
