# CARDLINK1 progress (lane LOD-C), worktree E:\Projects\NifskopeWWE-cardlink1, branch cardlink1-20260924 from 47b2cad

## 2026-09-24 21:16 -- worktree built (rung)
- Skills loaded: nifskope-ww-lodgen, nifskope-ww-build-verify, search-lean.
- qmake needed main's `.qmake.stash` (the compiler probe fails without it: "failed to parse default include paths").
- Copied from main (read only there): `.qmake.stash`, `GeneratedFiles/` (main HEAD 71f96c1 differs from 47b2cad only in
  scratchpad/docs; main `make -n` = 0 compiles, so its objects match 47b2cad), release runtime (*.dll, *.xml, qt.conf,
  style.qss, platforms/, imageformats/, styles/, shaders/). Objects touched newer than the checkout; lodbfile.o, main.o,
  about_dialog.o deleted (NIFSKOPE_REVISION define differs: a worktree .git is a file, qmake finds no revision).
- Build script: scratchpad/cardlink1_20260924/wt_build.sh (gated on make's rc, exe mtime moved, MZ, -nt).
- RUNG exe = release/before_cardlink1.exe, sha1 0af99c9921a331f6db19ea45a43488fb3736c5d4 (45 objects rebuilt).
