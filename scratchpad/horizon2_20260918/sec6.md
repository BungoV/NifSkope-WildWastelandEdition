
## 6. The build

Every timestamp here is from `date` or `stat` in the shell that ran the command.
Fallout4 was re-checked as its own command before each of the three builds and
before every exe run in this lane; it was DOWN every time
(`tasklist | grep -i -E "Fallout4|NifSkope"` -> no `Fallout4.exe` row).

**The exe that ships this lane** -- `release/NifSkope.exe`:

| | |
|---|---|
| mtime | **2026-09-18 23:47:33** (+0200) |
| size | **22,949,376 bytes** |
| sha1 | **`b349f807426be700ed2ff9b54ee23e4fab3ba037`** |

**The rung**, taken once before the first build and never deleted --
`release/NifSkope.before_horizon2.exe`, 2026-09-18 22:57, 22,952,448 bytes, sha1
`e578b76f9d7a2d011363e4300a2c94967e14d605`: byte-for-byte the exe this lane was
handed (section 0). No `release/NifSkope.before_*.exe`,
`NifSkope.archlock1_rung.exe`, `NifSkope.at_0117.exe` or `NifSkope_inuse_*.exe`
was deleted or renamed by this lane, and no NifSkope had to be renamed aside at
link time -- there was no window holding the exe at any of the three link steps.

**`find src tests res -newer release/NifSkope.exe`** at 2026-09-19 00:08 returns
four paths and they are all test files, not sources:

```
tests/spells                            (the directory's mtime)
tests/spells/lodgen_horizon.sh          the gate, +G6
tests/spells/lodgen_horizon_witness.json the frozen fixture
tests/spells/lodgen_horizon_witness.py   the checker
```

Nothing under `src/` and nothing under `res/` is newer than the exe. The three
test files are interpreted at run time by `bash` and `python`; none of them is
compiled into anything, so the exe is still the exact build of the sources as
they stand.

**The object-vs-header check** (`nifskope-ww-build-verify`, the LODIV7 lesson):
`src/lodghorizon.h` is the header this lane edited, at **23:46:09**. Every `.cpp`
that includes it has an object NEWER than that:

```
btdterrain.o   23:46:30      lodgen.o      23:47:04
lodinative.o   23:46:30      nativeemit.o  23:46:39
nifcli.o       23:46:48      src/lodghorizon.h  23:46:09
```

Five objects, five includers, none stale. That check is not decoration in this
lane: **the first build of this lane was a stale-object build of the worst kind**
and `find -newer` could not have seen it. `grep -c lodghorizon Makefile.Release`
returns **0** -- qmake froze that Makefile's dependency lists before the header
existed -- so `mingw32-make -f Makefile.Release -j8` printed `Nothing to be done
for 'first'.` and **exited 0** while the exe stayed at 21:59:46
(`scratchpad/horizon2_20260918/build1.log`, 57 bytes, which is how small a build
log gets when nothing is built). The exe's mtime is the second half of the gate,
and a MISTAKES entry is written for it (section 11). A `qmake` run is owed --
this hand repair does not survive one, and the next lane that edits
`src/lodghorizon.h` will hit the same silence.

**The three builds**, all `mingw32-make -f Makefile.Release -j8` under
`PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`:

| # | log | make rc | outcome |
|---|---|---|---|
| 1 | `build1.log` | **0** | `Nothing to be done for 'first'.` -- compiled nothing, exe unmoved. Not a build. |
| 2 | `build2.log` | **2** | 5 error lines: `src/lodghorizon.h:287:33: error: 'LODGEN_HORIZON_MAX_TAPS' was not declared in this scope` -- the constant had been placed below the struct that uses it. |
| 3 | `build3.log` | **0** | 18,870 bytes; compiled exactly `btdterrain`, `lodgen`, `lodinative`, `nativeemit`, `nifcli` and linked. Exe 21:59:46 -> **23:47:33**. |

Build 3's log names five translation units and no others, which is also the
check that the shipped exe is the handed exe plus this lane's diff and nothing
else.

**One source file changed**, `src/lodghorizon.h`, 25,134 bytes, CRLF 0 of 558 LF
(Python byte count, before and after). It is untracked -- HORIZON1 created it and
nothing in this tree is committed -- so there is no `git diff` to quote; the
three functional edits are quoted in full in section 3. Nothing was committed,
nothing was stashed, and `WW_CHANGES.md` and `HANDOFF.md` were not touched
(section 9 hands the director their text instead).
