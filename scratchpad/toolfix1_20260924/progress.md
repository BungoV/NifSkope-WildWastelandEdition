# TOOLFIX1 progress

- 22:2x worktree bootstrap: objects/stash/runtime copied from NifskopeWWE-esmfix1 (9dfb9b0, make -n = 0 there);
  only src/lodgen.cpp differs 9dfb9b0..5c3a51f, so lodgen.o + the REVISION objects (lodbfile, main, about_dialog)
  deleted before the first make. qmake rc 0, Makefile.Release names this tree 123 times, the others 0.
- Finding: main tree's pid 23560 (bungo's window, started 19:25) reports image path
  E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe, but NO exe in main's release/ is held (all 71 open
  ReadWrite/share-none). The reported image path is stale after the 20:36 rename -> a path match alone would rename
  a free exe forever in the main tree. ww_build.sh therefore renames only on path match AND exe really held.
- tools/ww_build.sh rewritten (per-tree lock instead of machine-wide make refusal; make runs in THIS tree, log in
  release/ww_build.log; WW_BUILD_LOCK_ONLY=1 = step 1 only).
- tests/spells/lodgen_loadorder.sh G5: live profile as-is, record = whole load order + TestWorldspace.esp; red rung
  = esmfix1's NifSkope.before_esmfix1.exe. Checker: record() takes must-plugins; untick() removed.
