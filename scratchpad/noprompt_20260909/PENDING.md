# NOPROMPT — BUILD PENDING (2026-09-09)

Code and harness are written and syntax-clean. **Not built, not run**: the gate
in the brief (`scratchpad/images_20260909/DONE`) did not exist at the check, so
another lane still holds the build slot.

Checked once, at the point the brief puts it:

```
$ ls scratchpad/images_20260909/DONE   -> not found        (DONE=no)
$ tasklist | grep -i -E "Fallout4|NifSkope" ; echo rc=$?   -> rc=1  (nothing running)
```

## What is on disk

| file | state |
|---|---|
| `src/nifskope.cpp` | patched, `g++ -fsyntax-only` RC=0 (+136 lines over the pre-lane copy) |
| `src/nifskope.h` | patched, +3 lines (LF-only, unchanged) |
| `tests/spells/render_shot.sh` | new, `bash -n` clean, 177 lines, LF-only |
| `WW_CHANGES.md` | entry added at the head (LF region, CR count unchanged 19020) |
| `MISTAKES.md` | two entries added (LF-only) |
| `scratchpad/noprompt_20260909/*.orig` | copies of all four files as this lane found them |

Nothing is committed (CONSTITUTION 8: not without bungo's word).

## Resume — paste this

Game down and the build slot free, from `/e/Projects/NifskopeWildWastelandEdition`:

```bash
cd /e/Projects/NifskopeWildWastelandEdition \
 && LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r') \
 && if [ -n "$LOCKED" ]; then mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" && echo "running copy (pid $LOCKED) renamed aside"; else echo "exe not held by a window"; fi \
 && MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc' \
 && test release/NifSkope.exe -nt src/nifskope.cpp \
 && cmp res/style.qss release/style.qss && echo "sheet in step" \
 && timeout 500 bash tests/spells/render_shot.sh 2>&1 | tail -30
```

Run it in the background and read the task file; the build is ~4 minutes and the
harness opens three real windows on the second monitor, one at a time.

## Pre-registered gates (written before the run, CONSTITUTION 1)

`tests/spells/render_shot.sh` must print **14 checks, 0 failures** and `PASS`.
The three that carry the fix:

1. `the bake exits 0 instead of asking` — rc must be 0, not 124. **rc=124 is the
   dialog**, and it is what the old code gives.
2. `the bake really edited the document` — at least one `hidden` line in the
   bake's own `.txt` sidecar. Without it the dirty case was never dirty and the
   check above is vacuous.
3. `and the close path says what it discarded` — `release/ww_headless_close.log`
   must carry `discarded cube_lod`, and must NOT exist for either clean case.

## Watch for, on the first real run

* **The dirty fixture hides its only shape.** `Flags|1` on the cube's one
  BSTriShape means the bake photographs an empty scene. That is fine for what is
  being measured (exit, wall clock, discard log) but the bake's bound may be
  degenerate; if the bake refuses or divides by zero, keep the fixture and read
  section 3's `.out` file rather than assuming the guard failed.
* **`-no-gui new --cube` writes a BSTriShape, not a BSMeshLODTriShape**, so the
  `LOD1/LOD2 Size` half of the bake's edit is not exercised — only the `_L*`
  flag half. A real tree exercises both; a corpus fixture was deliberately not
  used so the gate needs no game files.
* The harness deletes `release/ww_headless_close.log` at the start. If a
  concurrent NifSkope run appends to it mid-harness the counts move; one
  instance at a time.

## Then tell bungo

His open window predates this; the next launch of `release\NifSkope.exe` has it.
