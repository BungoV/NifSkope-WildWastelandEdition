# lane OFFSCREEN -- BUILD PENDING

`Fallout4.exe` was up (pid 36716, seen at 17:47 and still running at the end of
the lane), so CONSTITUTION 6 applies: no build, and no launch of
`release/NifSkope.exe` either -- which also blocked the before-picture.

**Code is finished and syntax-checked.** `g++ -fsyntax-only src/nifskope_ui.cpp`
returns RC=0 with no new warnings (`-Wall -Wextra`). Nothing is committed.

## What is on disk

| file | state |
|---|---|
| `src/nifskope_ui.cpp` | +123 lines / +5630 bytes (LF, CR count still 0) |
| `tests/spells/render_shot.sh` | +142 lines / +7384 bytes (LF, untracked file from lane NOPROMPT) |
| `tools/bake_impostor_cards.sh` | header note only |
| `WW_CHANGES.md` | entry spliced, CR count 19020 unchanged |
| `MISTAKES.md` | three entries spliced, CR 0 |
| `release/NifSkope.before.exe` | **byte copy of the 17:22:05 exe**, kept so the before/after picture is still possible after the build overwrites `release/NifSkope.exe`. sha256 `5ca38e289acd2ba84a3496b4783b224920dc9bb1ee5e662d86b5f08f90ee53b2`. Delete it once the gate has run. |

## The resume, paste-able

Every step needs the game down. The gate below is the whole verification.

```bash
cd /e/Projects/NifskopeWildWastelandEdition

# 0. the gate for everything: no game, no leftover harness (bungo's own NifSkope
#    window, if he has one, has no --port and is not to be touched)
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?    # want rc=1

# 1. THE BEFORE PICTURE, from the 17:22:05 exe, BEFORE the build overwrites it.
#    Confounded on purpose-note: this exe predates other lanes' uncommitted
#    changes to the renderer and the shaders, so a difference here is not
#    necessarily this lane's. The clean identity test is step 4, section 6.
export WW_WINDOW_AT=1960,40
WW_RENDER_SHOT="E:/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen_20260909/before.png" \
  WW_RENDER_SIZE=640x480 WW_RENDER_TIME=1 \
  timeout 120 release/NifSkope.before.exe --port 42329 \
  "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleForest2.nif" >/dev/null 2>&1
ls -l scratchpad/offscreen_20260909/before.png
sha256sum scratchpad/offscreen_20260909/before.png | tee scratchpad/offscreen_20260909/before.sha256

# 2. build (one at a time, ~4 min; background it and read the task file)
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?    # want rc=1 again
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc'

# 3. the exe is newer than what changed, and the sheet is in step
test release/NifSkope.exe -nt src/nifskope_ui.cpp && echo "exe newer"
cmp res/style.qss release/style.qss && echo "sheet in step"

# 4. THE GATE. Sections 5 and 6 are this lane's; 1-4 are lane NOPROMPT's and
#    must stay green. Expect 15 + 12 = 27 checks.
timeout 900 bash tests/spells/render_shot.sh 2>&1 | tail -40

# 5. the after picture, same framing as step 1
WW_RENDER_SHOT="E:/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen_20260909/after.png" \
  WW_RENDER_SIZE=640x480 WW_RENDER_TIME=1 \
  timeout 120 release/NifSkope.exe --port 42329 \
  "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleForest2.nif" >/dev/null 2>&1
sha256sum scratchpad/offscreen_20260909/before.png scratchpad/offscreen_20260909/after.png
cat release/ww_headless_windows.log        # every line must read onscreen=0

# 6. ONE TREE BASE, the real thing, with the window check on it
#    (MAX=1 caps it at one base; the octahedral sheet is what strobed)
MAX=1 CANDIDATES=trees OCT=8 TILE=128 \
  bash tools/bake_impostor_cards.sh \
  "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" 16 16 31 31 \
  E:/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen_20260909/card_one
cat release/ww_headless_windows.log        # "shown", "bake" and "sheet", all onscreen=0
ls -l scratchpad/offscreen_20260909/card_one

# 7. tidy
rm -f release/NifSkope.before.exe
```

## What would refute the fix

* Section 6's pixel check failing: an off-screen window on this machine does not
  render the same bytes. The fallback is already in the code -- run everything
  with `WW_WINDOW_VISIBLE=1` and the old behaviour is back exactly -- and the
  real fix would then be to render into an FBO instead of the window
  (`GLView::grabSupersampled` at `src/glview.cpp:20817` is that path already,
  and takes `shift == 0` as a special case that returns `grabFramebuffer()`).
* `before.png` empty or the run hanging at `rc=124`: not this lane; that is the
  Save Confirmation dialog, and lane NOPROMPT's guard is what covers it.
* Section 6's two FLOOR checks failing (the control run's window not seen on a
  screen by one of the instruments): then section 5's zeros mean nothing and the
  gate is not evidence, whatever colour it prints.

## Left for the director

* Two `tools/bake_impostor_cards.sh` runs were killed at the start of this lane
  (pids 23436, 25980; output `scratchpad/images_20260909/gen/cards_trees`). They
  were the live flashing. Their finished cards are cached by form id, so a
  re-run continues; whoever owns that lane should be told.
* `release/NifSkope.before.exe` is a second exe sitting in `release/`. Delete it
  after step 7.
