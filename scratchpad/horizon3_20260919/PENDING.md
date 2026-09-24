# BUILD PENDING -- lane HORIZON3, written 2026-09-19 03:1x

**Fallout4.exe pid 17248 was UP at 03:09:18 (`date` + `tasklist` in the same
turn), so nothing was built, no exe was run, and no picture was taken.** The
implementation is COMPLETE ON DISK and every translation unit it touches parses
under the project's own flags. `g++ -fsyntax-only` is not a build: it writes no
object file, links nothing, and never touches `release/`.

The marker `BUILDING` is in place in this folder. **`DONE` is absent and must
stay absent until the build and the gate below have both run.**

---

## 0. BEFORE ANYTHING -- its own command, every time

```sh
tasklist | grep -i -E "Fallout4|NifSkope"
```

**Fallout4.exe present = STOP.** No build, no exe run, no picture. Do not
wait-loop on the game; end the lane and leave this file in place.

## 1. The rung exe is ALREADY SAVED -- do not re-copy it

`release/NifSkope.before_horizon3.exe` (22,949,376 bytes, 2026-09-18 23:47) is
byte-identical to the current `release/NifSkope.exe`, because nothing has been
built since it was taken. **The build overwrites `release/NifSkope.exe`**, so if
that copy is ever lost the gate's G1 byte-identity check has no rung to compare
against and cannot be re-created without a checkout.

## 2. Build

```sh
export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH
cd /e/Projects/NifskopeWildWastelandEdition
mingw32-make -f Makefile.Release -j8 2>&1 | tee scratchpad/horizon3_20260919/build2.log
```

**qmake is NOT owed.** It was run last session, after `src/lodgsubdiv.h` was
created, and `Makefile.Release` carries the dependency now. No file was added or
removed this session -- only existing files were edited -- so the makefile is
current. (`Makefile.Release.before_qmake` and `Makefile.before_qmake` in this
folder are the pre-qmake copies, kept as the way back.)

Expect warnings from the wider tree; **the only thing that matters is that the
six changed translation units compile and the link succeeds.**

## 3. Gate

```sh
cd /e/Projects/NifskopeWildWastelandEdition
RUNG=release/NifSkope.before_horizon3.exe \
  bash tests/spells/lodgen_horizon3.sh 2>&1 | tee scratchpad/horizon3_20260919/gate.log
```

The pre-registered floors, none of which may be edited to match what the code
turns out to do:

| step | what it proves | floor |
|---|---|---|
| G1a | `.lodo` byte-identical to the rung's bake at the knobs' OFF values | exact |
| G1b | `.lodi` differs only where listed | exact |
| G1c | its OWN bake with `--horizon-scrappable` | `scrappablePlacements 14` |
| G2 | the cut adds and never moves | T-junctions **0**, welded points **12976** |
| G2 census | the two words the gate greps | `horizonSubdivVertices 18754`, `horizonSubdivTriangles 24496` |
| G3 | the third witness re-derives it from the FILE | 20 inserted vertices within 2 deg |
| G3-control | the quarter-turn control **FAILS as it must** | a red that stays red |
| G4 | the horizon picture's ramp is shorter after the cut | pixels differ |

**G3 is the refuter and it does not call the producer.** It is HORIZON2's third
witness, `tests/spells/lodgen_horizon_witness.py`. A check that calls the
producer's own functions is not a check.

Also worth running, because the cut changes the cluster table under it:

```sh
python tests/spells/lodgen_native_fields.py <the gate's .lodo/.lodi>
```

h5 is the partition invariant: `LodoClusterLod::sourceTriangles` must sum, over
any cut, to the mesh's level-0 triangle count. **If h5 goes red the gain
propagation is wrong**, and that is the single most likely place for this code
to be wrong.

## 4. Pictures

G4 takes the horizon picture itself. Two more are owed, and neither is in the
gate:

```sh
# the scrappable channel -- magenta = the player can scrap it, grey = it stays.
# The note line prints the share; expect mean 0.0004 (14 of 33,123), and a v8
# file prints WHY it is all grey instead of just being all grey.
WW_RENDER_SHOT=scratchpad/horizon3_20260919/images/scrappable.png \
WW_RENDER_FLAT=1 WW_LODL_CHANNEL=scrappable \
  release/NifSkope.exe -no-gui <the G1c bake's .lodi>
```

```sh
# tier 2, before and after, same sun, same framing: the pair is the deliverable,
# because a shadow picture alone tells a person nothing.
WW_LODL_CHANNEL=horizon WW_SUN=120,15   # on the rung's bake, then on the cut bake
```

**A whole street coming back magenta means the build-area box test is wrong.**
That is what the scrappable picture is for; `scrappablePlacements 14` can be
right for the wrong reasons.

## 5. Files changed by this lane

Six translation units, all syntax-clean at 03:1x under the project's own flags:

| file | what changed |
|---|---|
| `src/lodgsubdiv.h` | NEW last session: the combinatorial bisection, its constants and its self-test (432 cases, GREEN) |
| `src/lodofile.h` | `LODO_VERSION_SUBDIVIDED = 5`, two header words, the `lodoSubdivideLibrary` declaration |
| `src/lodofile.cpp` | the whole `lodoSubdivideLibrary` body (~21.6 KB) + `lodoSubdivUnit` |
| `src/lodifile.h` | `LODI_VERSION_SCRAPPABLE = 9`, `LODI_INST_SCRAPPABLE = 64`, `LODI_INST_FLAGS_KNOWN` 0x3F -> 0x7F |
| `src/lodifile.cpp` | the v9 writer clause with its `--lodi-v7` drop guard, the widened accept list, the reader refusal for bit 6 below v9 |
| `src/esmdata.h` / `src/esmdata.cpp` | `EsmScrapBox`, `EsmScrapIndex`, `buildScrapIndex()`, `scrappable()` -- the three-clause rule, read out of the plugin |
| `src/nativeemit.h` / `src/nativeemit.cpp` | the option setter, the three option fields, the cut's call site (BEFORE the base loop), the scrappable flag, the two census lines |
| `src/nifcli.cpp` | `--horizon-subdivide`, `--horizon-face-sheet`, `--horizon-scrappable`, the widened `runLodgen` signature, usage and option docs |
| `src/lodinative.h` / `src/lodinative.cpp` | `WW_LODL_CHANNEL=scrappable`: the enum, the name table, `objectChannel`, the colours, the read-back text and the below-v9 note |
| `tests/spells/lodgen_horizon3.sh` | G1c rewritten to run its own `--horizon-scrappable` bake; **the floors were not touched** |
| `docs/LODGEN_NATIVE_LODO_LODI.md` | s3.7 the subdivided library, s4.12 the scrappable bit, both headed NOT FLOWN |
| `.claude/skills/nifskope-ww-lodgen/SKILL.md` | the closing HORIZON3 section: three knobs, their ways back, the floors, the gate's G1/G1c split |
| `.claude/skills/nifskope-ww-render-shot/SKILL.md` | the `scrappable` channel row and what an all-grey v8 render means |
| `MISTAKES.md` | the backslash-in-heredoc entry, newest at the top |

## 6. What is NOT written, and must be tasked separately

**Tier 3, the face sheet.** `--horizon-face-sheet` parses and reaches the option
struct and stops there; **no byte of any file changes**. It needs a `.lodo`
`FACE` table, a parallel `u16 faceId` table, a `.lodi` role-8 `HFACE` stream,
the sampling, and readers, writers, census words and gates for all of it. A
half-written tier 3 is a format a reader must refuse and a knob that lies about
what it does. The knob exists only so the CLI signature and the option struct do
not change twice. What it is FOR: **edge subdivision can never fix an
interior-only shadow** -- the 8 of 60 measured in report section 5. Tier 2 fixes
the other 52.

**`docs/FO4CS_IMPROVED_LOD_PLAN.md` s9, the CONSUMER contract.** Deliberately
unwritten until a bake has emitted the bytes. Report section 9 records why.

## 7. When it is green

Only then: remove `BUILDING`, touch `DONE`, and finish the report at
`scratchpad/horizon3_20260919/lane_horizon3_report.md` (sections 0-14 are
written; section 8 Pictures and the gate results are what a green run fills in).

**Nothing here is "fixed" or "final" or "true" until bungo confirms it live**,
and his open window needs a restart after any deploy.
