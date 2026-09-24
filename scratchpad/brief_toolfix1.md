# Lane TOOLFIX1 -- the build script's exe rename, and the load-order gate's stale untick (BUILD lane, small)

Director brief 2026-09-24 22:1x. Opus 5.5, account A, in-session agent. Worktree E:\Projects\NifskopeWWE-toolfix1,
branch toolfix1-20260924 from 5c3a51f. Read scratchpad/brief_lodnight_common.md (main tree copy) FIRST.

## 1. tools/ww_build.sh renames the wrong exe
CSM1 (scratchpad/csm1_20260924/DELIVERABLE_TEXT.md, MISTAKES section) saw ww_build.sh twice rename the lane's OWN
previous exe onto release/NifSkope_inuse_<pid>.exe: its lock check matches bungo's running NifSkope by the exe's
path and misreads which exe is in use. release/ now holds 7 NifSkope_inuse_*.exe. Fix: rename aside ONLY when the
exe at THIS tree's release/NifSkope.exe is the image of a running process (compare full image path of each running
NifSkope.exe to this tree's path, case/slash-normalised). Never kill a process. List (do not delete) the stale
inuse files in DONE.md with which are still locked.
Gate: G1 a running NifSkope from ANOTHER tree -> this tree's build does not rename (red: the current script does);
G2 a running NifSkope from THIS tree (a copy launched on the second monitor with WW_WINDOW_AT and an unused --port,
closed after) -> it is renamed aside and the link succeeds.

## 2. tests/spells/lodgen_loadorder.sh G5
ESMFIX1 (merged 13774a3) made TestWorldspace.esp load. G5 still unticks it and says the reader refuses it.
Change G5 to run his live profile as-is (all 47 plugins) and assert TestWorldspace.esp is in the record.
Red: the release/NifSkope.before_esmfix1.exe from E:\Projects\NifskopeWWE-esmfix1 fails the new G5.
Files: tools/ww_build.sh, tests/spells/lodgen_loadorder.sh (+ its _check.py). Nothing else.
Report per the common brief (DONE.md + DELIVERABLE_TEXT.md in scratchpad/toolfix1_20260924/).
