# BUILD PENDING -- lane IMPOSTORSHOW, 2026-09-19 10:46

`Fallout4.exe` came up at 10:45 (pid 31220), between the link of the 10:43:51
exe and the run that would have measured it. No build and no exe run since.

## What is on disk right now

* `release/NifSkope.exe` 23,243,264 B 10:43:51, `BUILD-RC=0`.
  It carries the use-after-free repair (`NifModel::addResourceRoot`).
  It does **not** carry the loose-root repair, because that edit landed at
  10:39:31 while the build was still compiling and `impostordraw.o` is stamped
  10:38:08 -- a minute OLDER than its own source. `test exe -nt source` passes
  and is lying; the object is the check.
* `scratchpad/impostorshow_20260919/BUILDING.txt` is still the marker file;
  the phase-A pending note is now `PENDING_phaseA_0930.md` beside this one.

## The resume, in order

Every exe run and every build is preceded by its own command:

```bash
tasklist | grep -i -E "Fallout4|NifSkope"
```

### 1. Rebuild (one object + link, about a minute)

```bash
cd /e/Projects/NifskopeWildWastelandEdition && stat -c %y release/NifSkope.exe \
 && MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build_imp3.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build_imp3.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc'
```

Then the staleness check that the exe timestamp cannot do:

```bash
cd /e/Projects/NifskopeWildWastelandEdition \
 && for p in src/gl/impostordraw.cpp src/model/nifmodel.cpp src/impostorpreviewtest.cpp; do \
      o=GeneratedFiles/.obj/$(basename $p .cpp).o; \
      [ "$o" -nt "$p" ] && echo "ok    $o" || echo "STALE $o"; done \
 && cmp res/style.qss release/style.qss && echo "sheet in step"
```

### 2. The one diagnostic run that decides whether the two repairs worked

```bash
cd /e/Projects/NifskopeWildWastelandEdition \
 && F=scratchpad/impostorshow_20260919/fixture/maple_n4 \
 && rm -f release/ww_impostor_trace.log \
 && WW_IMPOSTOR_TRACE=1 WW_IMPOSTOR_PREVIEW=map \
    WW_IMPOSTOR_LODM="E:/Projects/NifskopeWildWastelandEdition/$F/cards/0004a074_oct.lodm" \
    WW_IMPOSTOR_LOG="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/trace_map.log" \
    WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1024x1024 \
    timeout 180 release/NifSkope.exe --port 27731 \
      > scratchpad/impostorshow_20260919/trace_stdout.txt 2>&1; \
    echo "exe-rc=$?"; sort -u release/ww_impostor_trace.log
```

What each outcome means, decided in advance:

* `exe-rc=0` and the trace reaches `L done` -- both repairs hold. Go to step 3.
* the trace still stops at `E aux bound` -- the colour sheet still does not
  bind. Read `trace_stdout.txt` for the `not found in archives` line: it prints
  the exact name the cache asked for, and the answer is whether
  `<fixture>/textures` is now the registered root.
* `exe-rc=124` (a hang at `qApp->quit()`) or a segfault -- the use-after-free
  is not the only one. The trace file is written with the handle closed at
  every step, so its LAST line is where it died.

### 3. The gate, with the mesh it has been missing

```bash
cd /e/Projects/NifskopeWildWastelandEdition \
 && IMPOSTOR_PORT=27732 \
    IMPOSTOR_LODM="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture/maple_n4/cards/0004a074_oct.lodm" \
    timeout 900 bash tests/spells/impostor_draw.sh \
      "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/landscape/trees/TreeMapleForest2.nif" 2>&1 | tail -25
```

Steps 0..4b are already green on the 10:18 exe. Steps 5..8 have never once
produced an honest number, because no card has ever reached the framebuffer.

**The IoU floor of 0.80 in step 5 is a PRE-REGISTERED GUESS and is owed a
measurement.** The first run that draws an actual card reports the real
distribution; that number sets the floor, with the number written beside it, and
the floor only ever moves DOWN with its measurement attached. Step 6's red
control (honest - shuffled >= 0.15) is what keeps a lowered floor honest, so it
is not adjusted at all.

## Still owed after that, in the brief's order

* steps 7/8 on an ASYMMETRIC subject -- a maple is close to symmetric about its
  own axis and the margin will sit near zero honestly. Candidates that the
  chunk route accepts (`lodgen.cpp:3560..3587` refuses anything that is not a
  tree): `00038599 TreeBlasted01.nif`, `000393cd TreeBlasted02.nif`,
  `0012154f`/`00121550 BlastedForestBurntTreeUpright03/02.nif`,
  `000531b3 TreeMapleblasted05.nif`.
* `images/00_azimuth_180_explained.png` from two `WW_IMPOSTOR_PREVIEW=azimuth`
  runs (one legacy set, one re-baked), then `images/00_READY`.
* the variable-grid rows: N = 4, 5, 8, 12 over the floor; a tall pine keeping
  frameH > frameW; two different-N sets in one scene, with the red control that
  forces one set's N onto the other.
* depth and AO gate rows; the chunk placement path behind `WW_IMPOSTOR_OCT=1`
  with its menu row, master OFF; the `.lodm` open path; the mip cap (C19) and
  aux divisor (C32) in the shader; then steps 3 and 4, the pictures and the
  numbers beside them.

## Delete before the lane closes

`wwImpostorTrace` in `src/gl/impostordraw.cpp` is a diagnostic, not a feature.
It goes once the path it brackets has a gate row that fails without it, and the
`rm -f .../ww_impostor_trace.log` line in `run_harness` goes with it.
