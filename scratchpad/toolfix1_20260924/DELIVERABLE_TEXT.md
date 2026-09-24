## HANDOFF text
TOOLFIX1 (2026-09-24 22:1x, branch toolfix1-20260924, commits 730977f + 65369a0 + the report commit). Scripts
only, no src change. tools/ww_build.sh now renames release/NifSkope.exe aside only when a running NifSkope's
image path IS this tree's exe AND the exe really refuses an exclusive open; a window from another tree is left
be. It also builds the tree it lives in (was hard-coded to main), so it works in lane worktrees, with a per-tree
lock instead of the machine-wide make refusal; WW_BUILD_LOCK_ONLY=1 runs the rename decision alone. Measured: a
process's reported image path does not follow a rename (bungo's pid 23560 still reports main's NifSkope.exe; no
exe in main's release/ is held). Main's release/ holds 7 stale NifSkope_inuse_*.exe, all free, listed in
scratchpad/toolfix1_20260924/DONE.md, not deleted. tests/spells/lodgen_loadorder.sh G5 bakes his live profile
as-is (47 plugins) and asserts TestWorldspace.esp in the record; red = the pre-ESMFIX1 exe (bake rc 1). Spell
24/24 PASS on exe 664d465b.

## WW_CHANGES text
- Build script: `tools/ww_build.sh` no longer renames the freshly built exe when some other NifSkope is running.
  It renames the exe aside only when that exact file is in use by a running NifSkope, and it builds the copy of
  the project it sits in, so each lane worktree can use it directly. (TOOLFIX1)
- Load-order test: G5 of `tests/spells/lodgen_loadorder.sh` now bakes the live Mod Organizer profile unchanged
  (all 47 plugins, TestWorldspace.esp included) and checks the bake record lists every plugin by full path.
  (TOOLFIX1)

## MISTAKES text
- 2026-09-24 TOOLFIX1: tools/ww_build.sh renamed release/NifSkope.exe whenever ANY NifSkope.exe was running,
  naming that process's pid, so lanes (CSM1 twice) had their own fresh, unused exe renamed after bungo's window,
  and main's release/ gathered 7 NifSkope_inuse_*.exe. Lesson: decide "is this exe in use" on the file itself
  (path match AND an exclusive open fails), never on the process list alone, and never on the reported image
  path alone: it keeps the pre-rename name for the life of the process.

## Skill review
Loaded nifskope-ww-worktree-build (section 5b bootstrap worked; added: a sibling one merge behind is usable once
the objects of the differing src files are deleted). Amended nifskope-ww-build-verify with the rename rule (only
this tree's exe, only when really held; reported image path is stale after a rename; prefer tools/ww_build.sh to
its path-only one-liner) and nifskope-ww-worktree-build section 4 (ww_build.sh works in a worktree as-is since
TOOLFIX1). No new skill: the "is this exe held" test is now one line in the amended skill.
