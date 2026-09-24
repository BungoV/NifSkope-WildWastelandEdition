DONE -- lane TOOLFIX1, 2026-09-24 22:1x. Branch toolfix1-20260924 (from 5c3a51f). Nothing pushed or merged.

## 1 Skills loaded
nifskope-ww-worktree-build (bootstrap from a sibling worktree, section 5b). nifskope-ww-build-verify read
(for the rename rule it states) and amended. The other common-brief skills were not needed: no source change,
no harness, no render.

## 2 What was built
* tools/ww_build.sh -- the rename step now:
  - lists every running NifSkope.exe with its full image path (Win32_Process.ExecutablePath) and compares it with
    THIS tree's release/NifSkope.exe, lower-cased and with backslashes as slashes;
  - renames only when a path matches AND the exe refuses an exclusive open ([IO.File]::Open ReadWrite/None),
    because the reported image path does not follow a rename (measured, G2b);
  - never kills; processes from other trees are counted and left be.
  Also, so it can gate in a worktree at all: make runs in the script's own tree (was hard-coded to the main tree),
  the log is release/ww_build.log (was the shared MSYS2 /tmp/ww_build.log), a per-tree lock dir
  release/.ww_build.lock replaces the machine-wide "another make is running" refusal (lanes build in parallel by
  design), and WW_BUILD_LOCK_ONLY=1 runs the rename decision alone.
* tests/spells/lodgen_loadorder.sh G5 -- bakes his live Default profile as-is (47 plugins, nothing unticked) and
  checks the .lodb record: 47 plugins, the whole load order path for path, TestWorldspace.esp / BNS Trees.esp /
  TrueGrass.esp by full path, plugin 0 = Data/Fallout4.esm. The red rung runs the same G5 (RUNG5, default
  E:/Projects/NifskopeWWE-esmfix1/release/NifSkope.before_esmfix1.exe; RUNG5=none skips; MUST5 names the plugin).
* tests/spells/lodgen_loadorder_check.py -- record() takes must-have plugin names and checks the whole list;
  untick() retired (no caller left).
No src/ change, no format change, no shipped default changed.

## 3 Gates (transcripts in gate/)
* G1 (a NifSkope from ANOTHER tree -> no rename). RED: 5c3a51f's lock step in a scratch tree whose exe no process
  runs printed "running copy (pid 23560) renamed aside" (bungo's main-tree window). GREEN: new script, same tree:
  "not held ... 4 NifSkope running from elsewhere, left be"; full build in this worktree with 4 other-tree
  NifSkopes up: BUILD-RC=0, no rename, sha1 unchanged a67b55f5. gate/ww_build_g1_g2.txt
* G2 (a NifSkope from THIS tree -> renamed, link succeeds). pid 30248 launched from this tree (WW_WINDOW_AT=1960,40,
  --port 45993, closed after with CloseMainWindow). The exe was LOCKED; the build renamed it to
  NifSkope_inuse_30248.exe, relinked (BUILD-RC=0, exe 22:15:16, sha1 664d465b), new exe free, inuse file LOCKED.
* G2b (not in the brief; the reason for the held test). With 30248 still up its ExecutablePath still read
  ...\release\NifSkope.exe. New script: "report this tree's exe path, but the exe is not held: not renamed".
  RED: the old lock step renamed the free fresh exe, naming pid 23560. Restored by hand.
* G5 (spell). GREEN on this tree's exe: 24 checks, 0 failures, RESULT PASS (G1-G4 unchanged and green; G5 bake
  rc 0 in 24 s, record 47 plugins path for path, TestWorldspace.esp at plugin 28 from "AnotherOne's Test World",
  483 records above 0x0FFFFFFF). In-spell RED: the pre-ESMFIX1 exe fails 2 checks (bake rc 1, "invalid form ID"
  in TestWorldspace.esp, no record). Whole-spell RED: EXE=before_esmfix1 LEGS=5 -> 2 checks, 2 failures,
  RESULT FAIL. Checker sabotage: the green record with the TestWorldspace line removed -> 2 FAIL (whole-list +
  TestWorldspace), the other 4 ok.

## 4 Exe + commits
* release/NifSkope.exe sha1 664d465b315f5c899a10cf2b630412351b7345bb (G2's relink; same objects as the rung)
* rung release/NifSkope.before_toolfix1.exe sha1 a67b55f59f0e2325446e6b971cf481436c46e31e (first build of
  5c3a51f; objects from esmfix1 9dfb9b0 with lodgen.o + the REVISION objects deleted)
* 730977f TOOLFIX1: lodgen_loadorder G5 bakes his live profile as-is ...
* 65369a0 TOOLFIX1: ww_build.sh renames only THIS tree's exe ...
* (this report commit follows)

### Stale NifSkope_inuse_*.exe in the MAIN tree's release/ (listed, not deleted), 22:1x
All 7 open exclusively (none locked). Running NifSkope at the time: 23560 (main, bungo, started 19:25),
plus lane processes from incrgate1 / cardlink1 / cardfix1.
| file | bytes | mtime | locked |
| NifSkope_inuse_2000.exe  | 22341632 | 09-16 15:53 | no |
| NifSkope_inuse_8728.exe  | 23803392 | 09-19 22:13 | no |
| NifSkope_inuse_28576.exe | 23684608 | 09-19 20:58 | no |
| NifSkope_inuse_25584.exe | 23813120 | 09-22 23:32 | no |
| NifSkope_inuse_7644.exe  | 24518144 | 09-24 11:04 | no |
| NifSkope_inuse_2220.exe  | 24536576 | 09-24 12:43 | no |
| NifSkope_inuse_23560.exe | 24716800 | 09-24 20:19 | no  (named after bungo's live pid, yet free) |
Also free: main's release/NifSkope.exe itself. Nothing in main's release/ (71 exe) is held, although pid 23560
reports main's NifSkope.exe as its image. Where 23560's image really lives was not chased (probably moved by
Git Bash mv's in-use fallback); it does not change the fix. All 7 look safe to delete; that is the director's call.
This worktree also holds my own release/NifSkope_inuse_30248.exe (G2's; free now; gitignored).

## 5 What the final bake needs from this lane
Nothing in the bake's flags. The director's own builds in main via tools/ww_build.sh (after merge) will stop
renaming the fresh exe while bungo's window is open. G5 now proves his live 47-plugin profile bakes as-is.

## 6 Skill review
* Loaded: nifskope-ww-worktree-build -- 5b (copy from a sibling at the branch point) worked with one extension:
  the sibling was one merge BEHIND, only src/lodgen.cpp differed, so deleting lodgen.o with the REVISION objects
  made it safe (first make: 62 g++ lines, BUILD-RC=0). Added that as a line to section 5b.
* Amended: nifskope-ww-build-verify (new bullet: rename only this tree's exe and only when really held; the
  reported image path is stale after a rename; its one-command chain still uses a path-only check, prefer
  tools/ww_build.sh), nifskope-ww-worktree-build section 4 (ww_build.sh works in a worktree as-is since TOOLFIX1).
* Wished for / not written: none -- "is this exe held" is now one PowerShell line in the amended skill.
