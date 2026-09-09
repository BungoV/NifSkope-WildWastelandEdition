# Lane LODTOPEN — opening a `.lodt` in NifSkope, every plane viewable

Started 2026-09-09. Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`,
73 uncommitted paths on entry (bungo's "Not yet"). `Fallout4.exe` DOWN at start.

## 1. How a `.btd` opens today — the route the `.lodt` must join

**It IS a GUI route.** The brief's step-1 STOP point does not fire: `File > Open`
opens a Fallout 76 `.btd` today, through four touch points.

| what | file:line | what it does |
|---|---|---|
| file-type registration | `src/nifskope.cpp:158-160` | `{ "Bethesda Terrain Database", "btd" }` in `NifSkope::filetypes`, with the comment saying it is not a NIF and is *meshed* here |
| the pre-load picker | `src/nifskope.cpp:9541-9555` (`NifSkope::openFile`) | before `loadFile()`, a `.btd` gets `btdReadWorldInfo()` + `btdQueryRegion()`; the chosen region is parked in `NifSkope::btdPendingRegion` (`src/nifskope.h:820-824`) so **Reload rebuilds the same region** |
| the load branch | `src/nifskope.cpp:9926-9943` (`NifSkope::load`) | suffix `btd` ⇒ **generate** the document instead of parsing one: `nifCreateBtdTerrainScene( nif, fname, spec, &terr )`. No dialog route (command line, harness) falls back to `btdRegionFromEnv()` (`WW_BTD_REGION=x0,y0,x1,y1,lod`) then `btdDefaultRegion()` |
| save guard | `src/nifskope.cpp:9970-9972` | `save()` on a `.btd` document forces Save-As, so a generated NIF is never written over the terrain database |
| the generator | `src/btdterrain.cpp:166-385` (`nifCreateBtdTerrainScene`) | |
| headless CLI | `src/nifcli.cpp:2256-2297` (`cmdBtd`), dispatch `src/nifcli.cpp:5763` | `NifSkope.exe -no-gui btd <f.btd> --info` / `--region X0 Y0 X1 Y1 --lod 0..4 -o OUT.nif` — **the same generator the GUI uses** |
| its gate | `tests/spells/btd_terrain.sh` | 13 checks, heights read back out of the `.btd` by an independent Python decoder |

**The scene it builds** (`src/btdterrain.cpp:251-377`): a fresh Fallout 4 document
(`createNew(0x14020007, 12, 130)`), one `NiNode` root named
`<stem> [x0,y0]..[x1,y1] LOD<n>`, and one `BSTriShape` per tile of `k` cells —
`k` chosen so `(k*n + 1)` stays under 254 vertices a side. Vertex descriptor
`0x0041B00000650407` (28 bytes: full-precision position, half UV, byte normal +
tangent, float bitangent — **no vertex colours**), heights decoded from the
file's own `uint16` grid, normals by central difference, the `+1` rim row/column
sampled out of the NEIGHBOUR cell so tile seams close. Each shape gets a
`BSLightingShaderProperty` + `BSShaderTextureSet` whose slot 0 is the renderer's
inline-colour syntax `#FF808080` — flat neutral grey. **It shows exactly one
plane: heights.** No plane selector of any kind exists on the `.btd` route.

So "the same way, the same kind of scene" means: register the extension, pick a
region before the load, generate in `load()`, remember the spec for Reload, guard
`save()`, and add the matching `-no-gui` subcommand. The plane selector has no
`.btd` precedent to copy, so it is modelled on the picker (a combo beside the
detail combo) plus an environment override for harnesses and renders — the same
two-route shape `WW_BTD_REGION` already gives the region.

## 2. What was built

**The lane ends BUILD PENDING.** `Fallout4.exe` was DOWN at 14:0x when the lane
started and came UP at 14:25:40 (pid 26368), before any build. Nothing was
compiled to an object, nothing linked, no harness run, no picture taken.
`release/NifSkope.exe` is still 13:42:03, older than every file below.

| file | what changed |
|---|---|
| `src/lodtfile.h` / `.cpp` | reader additions only, no second reader: `aoSample()`, `overviewWord()`/`overviewHeight()`, the GCVR table (`gcvrCount()`, `gcvrForm()`), and the one-block inflate cache turned into an LRU with `setBlockCacheSize()` |
| `src/btdterrain.h` / `.cpp` | the tile mesher extracted into one shared `buildTerrainSurface()` both formats call; the whole `.lodt` route added beside the `.btd` one |
| `src/nifskope.h` | `LodtRegionSpec lodtPendingRegion` — Reload rebuilds the region AND the plane |
| `src/nifskope.cpp` | the four `.btd` touch points, each given a `.lodt` twin |
| `src/nifcli.cpp` | `cmdLodt` + `--plane`, dispatch, help |
| `tests/spells/lodt_open.sh` | NEW, 23 checks (G3) |
| `tests/spells/lodt_open_authority.py` | NEW, the independent decoder the harness compares against |
| `docs/LODGEN_BTD_FORMAT.md` | "Opening one in NifSkope" |
| `WW_CHANGES.md`, `MISTAKES.md` | one entry each |

**The entry point is the `.btd`'s, at every one of its touch points.**

* registration: `{ "Landscape Terrain", "lodt" }` beside the `.btd` row;
* `openFile()`: a picker runs BEFORE the load, so a cancel leaves the current
  document alone (the same reason the `.btd`'s does), and parks the answer in
  `lodtPendingRegion`;
* `load()`: a new `else if` on the suffix that GENERATES the document via
  `nifCreateLodtTerrainScene()`, falling back through `lodtRegionFromEnv()` to
  `lodtDefaultRegion()` on the no-dialog routes;
* `save()`: Save-As forced, so a generated NIF is never written over the
  landscape file;
* `-no-gui lodt <f> --info` / `--region --lod --plane -o OUT.nif`, the same
  generator, so a headless render and a window show the same scene.

**The planes.** Heights are the geometry; every other plane paints that same
surface as vertex colours, ONE AT A TIME, and only that plane is read. Nine
exist on Commonwealth.lodt: `height ao blend colour waterheight watertype
cellflags cellrange overview`. It has no ground cover (section flags `0xd` =
colour + AO + water), and asking for `groundcover` refuses in words naming the
flags rather than drawing black.

* the **Height** view writes no colour channel at all, so its descriptor is the
  `0x0041B00000650407` this route has always written and its scene is the same
  bytes a `.btd` of the same terrain builds — that is what makes G2 a real
  comparison rather than a tautology;
* **blend** composites the five 3-bit alphas over the quadrant's base slot, in
  the documented order (slot 0 is the TOP layer, drawn last), each layer a
  stable hashed colour of its LTEX form ID;
* **colour** decodes the 5-5-5 word at R 11-15, G 6-10, B 0-4;
* **ao**, **overview** and the four per-cell planes read their own flat sections;
* every build PRINTS what the plane measured — range, counts, and for the
  overview its greatest disagreement with the block pyramid on the shared
  lattice. A plane that reads back as one constant and a plane that was never
  read look identical in a picture; the difference has to be a number.

**Two changes in shared code**, both of which the sibling gates cover:

1. the reader's AO/overview accessors — those sections had an offset and no way
   to read a texel;
2. the block cache. This one is load-bearing, and the reason is the format: the
   pyramid is PROGRESSIVE, so a **subsampled** walk alternates between the level
   that stores every 4th sample and the one that stores every 8th. With the
   single slot the reader had, a whole-worldspace open would have missed on
   *every one* of 2.36 M samples. Capacity stays 1 by default so the verifier's
   behaviour is untouched; the scene builder asks for 256 (~1.5 MB at
   Commonwealth's three planes).

**Deliberate divergence from the panel-style house rules, stated.** The new
picker is a clone of `btdQueryRegion` — plain `QLabel` / `QSpinBox` /
`QComboBox`, no `wwHeading`, no `wwMakeScrubField`. The two dialogs appear in
the same place for the same gesture on two files of the same species, and a
`.lodt` picker in a different visual language from the `.btd` picker beside it
would be the worse outcome. Restyling BOTH to the house rules is a candidate,
not this lane's work, and is listed as owed below.

## 3. Gates G1-G5

**None of G1-G5 was run.** They need a build, and the game is up. What follows
is the state of each and the exact command that settles it.

| gate | state | what settles it |
|---|---|---|
| **G0 (not in the brief) syntax** | **PASSED** | `g++ -fsyntax-only` on all four changed translation units — `btdterrain.cpp`, `lodtfile.cpp`, `nifcli.cpp`, `nifskope.cpp` — all RC=0, no warnings of ours. Writes nothing, needs no build slot. It caught one real error (below). It proves compilation, NOT linking and NOT behaviour. |
| **G1 byte identity** | PENDING | `bash tests/spells/lodt_write.sh` — the writer was not touched; only `LodtFile`'s accessors and its cache were, and the cache changes speed, never values. |
| **G2 parity** | PENDING | `EXM1PittWorldspace.btd` (1,581,073 bytes, on disk) → `lodgen --from-btd … --lodt <dir>`, then render both through the hook from one pinned camera. Expect a **non-zero but bounded** difference with a stated cause: a `.btd`'s heights are range-normalised u16 and a `.lodt`'s are quantised on a fixed 32767 bias, so the round trip costs **half a quantum** (0.32 units on the Pitt worldspace, per the format document). Report the max \|Δz\| numerically beside the pixel diff; a lit render is required because a flat one has no shading to differ. |
| **G3 harness** | WRITTEN, PENDING | `bash tests/spells/lodt_open.sh`. Fixture found on disk: `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodt`, 35,953,286 bytes, 2026-09-05 03:14 — **not regenerated**. |
| **G4 suite** | PENDING | `lodt_write.sh`, `lodt_btd.sh` (the reader is shared), `btd_terrain.sh` (**the tile mesher moved under it** — this is the gate that proves the `.btd` scene is unchanged), `lodt_open.sh`. Skipped: everything that touches neither the reader nor the mesher — the collision, block-list, impostor, atlas, array, merge and VT harnesses read no `.lodt` and build no terrain surface. |
| **G5 pictures** | PENDING | nine plane renders + the whole-worldspace and Boston close-up, into `scratchpad/lodt_20260909/`. |

### What WAS measured, without a build

The Python authority (`tests/spells/lodt_open_authority.py`) was run against the
real `Commonwealth.lodt`. It shares no code with `lodtfile.cpp`, and it is the
right-hand side of every numeric check in the harness.

```
cells [-96,-96]..[95,95]      heights -8320.000 .. 44872.000   quantum 8.000
32 samples/cell   block edge 32   4 levels   48,960 blocks
100 LTEX, 15 WATR, 0 GCVR, AO 8/cell, overview 8/cell, sections 0xd
planes: height ao blend colour waterheight watertype cellflags cellrange overview
```

and the values the harness will hold the C++ to, on the 2x2-cell Sanctuary
region `[-20,24]..[-19,25]` (global sample origin 2432,3840) at LOD2:

| quantity | authority |
|---|---|
| tile arithmetic, region LOD2 / LOD0 | 1 shape 289 verts / 1 shape 4225 verts |
| tile arithmetic, whole worldspace LOD2 | **49 shapes, 2,380,849 verts** |
| height at (0,0) / (8,8) / (16,16 rim) | 8208.000 / 8160.000 / 9256.000 |
| shifted control at vertex (7,8), one sample north | 8896.000 against 8264.000 — **632 units apart**, so the comparison can fail |
| height spread over the region | 2640.000 |
| AO over the region's sampled texels | 173..249, mean 216.6 |
| water heights in the region | 450.0 .. 7250.0 |
| overview vs block pyramid on the lattice | equal at the spot check (8208.0 = 8208.0); the harness asserts 0.000 |

The first shifted control tried, one *row* north (gy+4), read **identical** to
the unshifted sample — a vacuous refuter. It was replaced by scanning the region
for the sample with the largest north-neighbour step, which is vertex (7,8).
That is the "a check that cannot fail on its input is not a check" rule doing
its job before the harness ever ran.

## 4. Memory and time on the Commonwealth file

Time: **not measured**, because nothing was built. The two things that will
dominate are stated so the measurement has something to refute: the 2.36 M
sample reads through the pyramid, and `NifModel`'s per-vertex `set<>` writes.

Memory, computed from the file's own header:

| | bytes |
|---|---|
| the file | 35,953,286 |
| the reader's resident prefix (`oData`) — header, LTEX/WATR/GCVR tables, quadrant slots (1.77 MB), per-cell records (590 KB), the 4.7 MB overview, the 2.4 MB AO plane, the 783 KB block directory | **10,221,156 (9.75 MB)** |
| block payloads, seek-read one at a time, **never resident** | 25,732,130 |
| inflated-block cache at 256 blocks, 3 planes | ~1.5 MB |
| default view sample grid, 1537 x 1537 heights | 9.0 MB (18.0 MB with a plane's colours) |
| the resulting BSTriShape vertex payload, 2,380,849 verts | 63.6 MB at 28 B; 72.7 MB at 32 B for a plane view |

So a whole-Commonwealth open costs roughly **20 MB of our own buffers plus the
document's ~64-73 MB**, and never loads a plane it is not showing. That is the
brief's "must not load every plane at full resolution at once": the AO and
overview sections are flat and small enough to ride in the prefix (they always
were), and of the four block planes exactly one — heights — plus at most one
other is ever walked.

## 5. Mistakes

**One, appended to `MISTAKES.md` at the repo root** (2026-09-09, lane LODTOPEN):
a local array named `slots`, which is a Qt keyword macro. `quint16 slots[6]`
parsed as a structured binding and threw eleven cascading errors twenty lines
away from the cause. The `nifskope-ww-lodgen` skill names this exact trap and
was loaded at the start of the lane. Found by the syntax-only pass, so it cost
no build — which is the only reason it was not worse.

Not mistakes, but stated plainly: G1-G5 are all unrun; the harness has never
executed, so its own string parsing is verified only against simulated CLI lines
(done, all five parsers checked) and its exe interactions are not; and the
`.btd` scene is *argued* to be unchanged by construction — the descriptor value
`0x0041B00000650407` is reproduced exactly by `ResetAttributeOffsets(130)` on
those flags, checked by hand nibble by nibble — but `btd_terrain.sh` has not
been re-run to prove it.

## 6. Finished-work skill review

**Loaded and used:** `nifskope-ww-lodgen` (the `.lodt` contract, the byte-identity
gates, the GUI harness rules, the editing traps — including the `slots` trap it
warned about and I still hit), `nifskope-ww-build-verify` (the patch-script rule,
`${PIPESTATUS[0]}`, the exe-under-his-window rule, and the reason the build did
not happen), `nifskope-ww-render-shot` (the hook's switches, which the harness's
plane renders and the pending G5 pictures are written against).

**Not loaded, deliberately:** `nifskope-ww-panel-style`. The new control is a
modal open-time picker cloned from `btdQueryRegion`, not a dock or settings
pane; matching its sibling beat matching the dock rules, and the divergence is
stated in §2 rather than silently taken.

**Amended:** `E:\Projects\Claude\.claude\skills\nifskope-ww-build-verify\SKILL.md`
gained a section **"When you CANNOT build: the game is up"** — the
`g++ -fsyntax-only` chain with the flags read out of `Makefile.Release`, the
`${PIPESTATUS[0]}` gate, the in-repo throwaway script (an MSYS2 login shell
cannot see a `/tmp` file the Git-Bash parent wrote — that cost a turn here), the
moc caveat, the known pre-existing Qt/libstdc++ warning, and the Qt keyword-macro
check. It belonged in that skill rather than a new one: it is the "cannot build"
branch of the build-and-verify procedure. **The director must mirror it into the
repo tree** if a lane running under `<repo>/.claude/skills` needs it —
CONSTITUTION 1a, the two trees do not sync.

**Wished for, not written:** a skill for "teach NifSkope WW to open a new file
type" — the five touch points, the remembered spec, the save guard, the CLI twin
and the harness shape. It was re-derived here by reading `nifskope.cpp`, and it
would recur if a `.lodv` or another generated format ever opens. It is declined
for now because one instance is not a procedure: if a second such format is
opened, that is the session to write it, and this report's §1 is the draft.

## 7. Build and gates (LODTOPEN2)

**The lane ends BUILD PENDING a second time. Nothing was built, no gate ran, no
picture exists.** `Fallout4.exe` was up for the whole window: pid 16884, started
14:34:38, checked at 14:46, 14:49 and 14:52 with its CPU time climbing 982 -> 1323 s
and its working set moving between 11.9 and 14.0 GB. bungo is playing. CONSTITUTION
rule 6 -- game up means the lane ends BUILD PENDING and is resumed, never built
around, never polled for.

### The clocks, in one table (CONSTITUTION rule 4, last bullet)

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | **2026-09-09 13:42:03** |
| `release/style.qss` | 2026-09-09 13:42:03 |
| newest changed source: `src/btdterrain.cpp` | **2026-09-09 14:40:29** |
| `src/nifcli.cpp` | 14:32:02 |
| `src/nifskope.cpp`, `src/nifskope.h` | 14:30:56 |
| `src/btdterrain.h` | 14:22:37 |
| `src/lodtfile.cpp` / `.h` | 14:21:56 / 14:21:17 |
| `tests/spells/lodt_open.sh` | 14:39:27 |
| `tests/spells/lodt_open_authority.py` | 14:37:50 |

The exe is **58 minutes older** than the newest source. `test release/NifSkope.exe
-nt src/btdterrain.cpp` fails, so by the build-verify skill's own gate no harness
and no render may run: they would measure the previous build. That is the whole
reason this section reports no numbers instead of reporting wrong ones.

**No NifSkope process was running** at any check (14:47, 14:52). The exe is not
held, so the rename-aside step will be a no-op unless bungo opens a window before
the build. The wrapper checks anyway.

### Gates

| gate | verdict | number |
|---|---|---|
| G0 syntax (not in the brief) | **PASS**, re-run by me | RC=0 on all four TUs |
| G1 writer byte identity | **NOT RUN** | needs the build |
| G2 `.btd` vs converted `.lodt` parity | **NOT RUN** | needs the build |
| G3 `tests/spells/lodt_open.sh`, 23 checks | **NOT RUN** (written, 16,317 B) | needs the build |
| G4 reached suite | **NOT RUN** | needs the build |
| G5 eleven pictures | **NOT RUN** | folder holds only `PENDING.md` |

G0 is mine, not the previous lane's word for it: I read `CXXFLAGS`, `DEFINES` and
`INCPATH` out of `Makefile.Release` (they move; the skill says read them, not type
them), wrote them to a throwaway `sx_tmp.sh` in the repo, ran
`g++ -fsyntax-only` on `lodtfile.cpp`, `btdterrain.cpp`, `nifcli.cpp` and
`nifskope.cpp` gated on `${PIPESTATUS[0]}`, and deleted the script. **RC=0 on each.**
The only remaining output is the known pre-existing Qt/libstdc++
`-Wsfinae-incomplete` noise from `qchar.h`, which is not ours. It writes nothing and
touches no exe. It proves the four files COMPILE. It proves nothing about linking,
nothing about moc, and nothing about behaviour -- so BUILD PENDING still stands.

### Line endings, measured by Python byte count (never grep)

| file | CR | LF | verdict |
|---|---|---|---|
| `src/nifskope.cpp` | 9234 | 10357 | **MIXED**, as it was |
| `src/lodtfile.*`, `src/btdterrain.*`, `src/nifcli.cpp`, `src/nifskope.h` | 0 | -- | LF-only, unchanged |
| `tests/spells/lodt_open.sh`, `lodt_open_authority.py` | 0 | -- | LF-only |
| `WW_CHANGES.md` | 19020 | 22630 | MIXED, and stays so |
| `MISTAKES.md` | 0 | 211 | LF-only |

`src/nifskope.cpp` CR went **9191 -> 9234, delta exactly +43** = the 44 CRLF lines
added less the 1 CRLF line removed. `git diff --numstat` on it is **49 / 2** --
small, not the whole file, as required.

**One thing that looked like a defect and is not, recorded so it is not re-raised.**
Five of the 49 lines added to `src/nifskope.cpp` are bare LF in a file that is
mostly CRLF. They are inside `validExternalNifPaths`, and `git show
HEAD:src/nifskope.cpp` proves that function was **already an LF-only block before
this change**. The rule is match neighbours, never normalise; they match. The
premise I was given -- that `src/nifskope.cpp` is "CRLF throughout" -- is wrong:
it is CRLF with LF-only blocks in it, and an edit's correct ending depends on
which block it lands in, not on the file's majority.

### Repository state

78 uncommitted paths (was 73 on entry; the lane added `tests/spells/lodt_open.sh`,
`tests/spells/lodt_open_authority.py`, and this folder). HEAD is `2dd8444`,
2026-09-04 13:30. `src/lodtfile.*`, `docs/LODGEN_BTD_FORMAT.md` and `MISTAKES.md`
are UNTRACKED rather than modified -- they are new files under bungo's "Not yet",
which is why they show no `numstat` line. **Nothing was committed.**

### Fixtures confirmed on disk

`E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodt` 35,953,286 B
(2026-09-05 03:14) and
`E:\SteamLibrary\...\Fallout 76 Playtest\Data\Terrain\EXM1PittWorldspace.btd`
1,581,073 B (2026-07-12 01:04). Both G2 and G3 have their inputs.

### Memory and time on the Commonwealth file

**Still not measured, and it cannot be faked from a header.** §4's table is
arithmetic off the file's own header -- correct as arithmetic, and not a
measurement. Time is unknown. `scratchpad/lodt_20260909/PENDING.md` now carries a
timed, peak-working-set harness for the bare whole-worldspace open so the resume
produces a real number to set against §4's 20 MB + 64-73 MB estimate, and says
which figure is measured and which is arithmetic.

### What `PENDING.md` now holds

It covered only the pictures and G2. It now covers the whole resume in order: the
game re-check, the `tools/ww_build.sh` invocation with all four sources, G1, G3 and
G4 with the reason each harness is in or out, the timing/memory measurement, the
eleven pictures, the ground-cover refusal capture, and G2 with its expected
non-zero bounded difference. It also carries the two rules that cost deliverables
before -- absolute paths on every exe argument, and open every PNG and look at it.

### Mistakes

Two, both defects in our own materials rather than in the code, both appended to
`MISTAKES.md` at the repo root: the `nifskope-ww-lodgen` skill directing mistakes
to `docs/MISTAKES.md` against CONSTITUTION rule 2 (fixed in the skill), and the
LODTOPEN report's instruction to mirror a skill into a repo tree that holds no
copy of it (not acted on; listing both trees first is the rule).

None in the code: I changed no source file. The only files I wrote are
`WW_CHANGES.md` (a five-line honesty amendment, CR delta 0), `MISTAKES.md`,
`PENDING.md` and this section.

## 8. Finished-work skill review (LODTOPEN2)

**Loaded and used, all three named in the orders.** `nifskope-ww-build-verify` --
its new "When you CANNOT build" section is what produced G0, and reading the flags
out of `Makefile.Release` rather than from the skill's own sample mattered: the
real `CXXFLAGS` carries `-fno-keep-inline-dllexport`, `-O3` and `-march=haswell`
that the sample omits. `nifskope-ww-lodgen` -- the CLI surface, the harness names
for G4, the byte-count rule for line endings, and the stale line I fixed.
`nifskope-ww-render-shot` -- the switches behind the eleven picture commands, and
the one-instance/second-monitor/unused-port rules written into `PENDING.md`.

**Amended:** `nifskope-ww-lodgen/SKILL.md`, opening paragraph -- mistakes go in the
ROOT `MISTAKES.md` per CONSTITUTION rule 2, with `docs/MISTAKES.md` named as the
older read-only trap ledger, and a note that the line said the opposite until
today. In the LIVE tree only; the repo tree has no copy of this skill.

**The two-tree check, done rather than assumed** (CONSTITUTION 1a). Live tree
`E:\Projects\Claude\.claude\skills` holds all four `nifskope-ww-*` skills; the repo
tree holds only `ww-control-calibration`. Nothing to mirror, and mirroring would
have manufactured the drift the rule warns about. The LODTOPEN report's owed
"mirror it into the repo tree" item is therefore closed as not-applicable, not done.

**Wished for, and NOT written, with the reason.** A "resume a BUILD PENDING lane"
skill was the obvious candidate: re-check the game, prove the exe against the
sources, re-run what does not need a build, refresh the pending block. It is
declined because it is not a procedure of its own -- every step of it is already
in `nifskope-ww-build-verify` (the game gate, `test exe -nt source`, the cannot-build
branch), and a second skill restating them is exactly the drift that produced the
mistake logged above. What was genuinely missing was a HABIT, not a procedure:
turning the pending block into a paste-able resume so the next window spends no
context re-deriving it. That belongs in the block, and it is now in it.

**One correction that IS worth carrying** and is folded into the amended skill's
neighbourhood rather than a new skill: a mixed file's correct line ending is
decided by the BLOCK an edit lands in, not by the file's majority. `src/nifskope.cpp`
is CRLF with LF-only functions inside it, and a brief that says "CRLF throughout"
will send the next lane to normalise five correct lines.

## 9. Build and gates (LODTOPEN3)

**Built, and every gate ran.** `Fallout4.exe` was down at the start and at every
step (a `tasklist | grep` that PRINTED its answer before the build and before
each of the 40-odd exe launches; `rc=1` from grep = no line = no game). No
NifSkope window was ever held, so the rename-aside step was a no-op each time.

### The clocks, in one table

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | **2026-09-09 15:30:27** (first build 15:22:23, rebuilt after the fixes below) |
| `release/style.qss` | 2026-09-09 15:30:27, `cmp` against `res/style.qss` clean |
| newest changed source: `src/btdterrain.cpp` | 2026-09-09 15:29 (edited this lane; was 14:40:29) |
| `src/nifcli.cpp` | 14:32:02 |
| `src/nifskope.cpp`, `src/nifskope.h` | 14:30:56 |
| `src/btdterrain.h` | 14:22:37 |
| `src/lodtfile.cpp` / `.h` | 14:21:56 / 14:21:17 |

`tools/ww_build.sh` gated on all four sources and reported `BUILD-RC=0`,
`exe newer than the sources`, `copies in step`. `find src -newer
release/NifSkope.exe` is EMPTY, so the exe is newer than every file in `src/`,
not only the four named. **No compile or link error occurred at any point** —
the two edits below were made because gates failed, not because the build did.

### Gates

| gate | verdict | the numbers |
|---|---|---|
| **G1** writer byte identity | **PASS** | `lodt_write.sh` 11 checks / 0 failures; heights round-trip EXACTLY vs the ESM (36,864 samples, 0 mismatched, worst 0), alpha and colour words 0 of 36,864 differ. Then the stricter half the brief names: Commonwealth regenerated from `Fallout4.esm` in **4.9 s** and `cmp`'d against the shipped `Commonwealth.lodt` — **byte-identical, all 35,953,286 bytes**. |
| **G2** `.btd` vs converted `.lodt` | **PASS with the predicted, measured difference** | `EXM1PittWorldspace.btd` → `.lodt` in 2.5 s (1,068,711 B). Both meshed cells [-25,-25]..[24,24] at LOD4: **4 shapes, 161,604 vertices each, identical**. Geometry: **max \|dx\| 0.000000, max \|dy\| 0.000000, max \|dz\| 0.305524** world units against a half-quantum bound of 0.305185 — **100.1% of half a quantum**, and the mean \|dz\| is 0.302154, i.e. a near-constant vertical BIAS, not noise. That is the documented cause: `.btd` heights are range-normalised, `.lodt` heights sit on a fixed 32767 bias. Pictures, same framing: **247,363 terrain pixels, 247,265 identical (99.960%), max delta 1, 0 pixels over 8**. |
| **G3** `tests/spells/lodt_open.sh` | **PASS**, 23 checks / **0 failures** | First run was **23 / 2 failures**; both were defects in OUR OWN materials, not in the route (below). Highlights: vertices carry the file's own heights at (0,0), (8,8) and the neighbour-cell rim (0,0)=8208.000; the shifted control is 632 units away so the comparison can fail; whole worldspace = 49 shapes / 2,380,849 vertices; height view 28 B a vertex, every plane view 32 B; overview vs block pyramid **0.000 units** apart; the absent ground-cover plane refuses in words; **9 planes render, 8 distinct pictures**, and the one coincidence is excused only because the plane's own note declares it blank. |
| **G4** the suite the change reaches | **PASS** | `lodt_btd.sh` — 11 ok, RESULT PASS (plus two honest NOTEs: the Pitt source has no land textures and one colour word, so those paths are UNCOVERED by it). `btd_terrain.sh` — **13 checks / 0 failures**, which is the gate that matters most here: the tile mesher was extracted into a shared `buildTerrainSurface()` under the `.btd` route, and the `.btd` scene is unchanged. **Skipped and why**: the collision, block-list, impostor, atlas, texture-array, merge, identity, far-ring, octahedral and VT harnesses read no `.lodt` and build no terrain surface, so the change cannot reach them. |
| **G5** the eleven pictures | **DELIVERED**, all eleven opened and looked at | paths and what each one shows, below. |

### The two failures the first G3 run found, and what they were

Neither was in the `.lodt` route. Both are recorded in `MISTAKES.md`.

1. **The harness's own arithmetic.** Check 16 demanded a `Data Size` of 12324
   for a 32-byte plane vertex. 289 verts x 32 + 512 tris x 6 = 9248 + 3072 =
   **12320**, which is exactly what the code produced. The 12324 was a
   mis-added sum, written into both the check and its comment. Fixed to 12320,
   with both halves of the sum spelled out so the next reader need not add in
   their head. The check still separates 32-byte (12320) from 28-byte (11164).
2. **The colour plane's note said the opposite of what it drew.** The note read
   "280 of 289 samples (96.9%) are not white" for a region that renders
   entirely white — which is why check 22 saw the colour and height views paint
   the same picture. Measured, not guessed: the 289 vertex colours in the built
   NIF are **(255,255,255) at every one of them**, while the AO plane's are
   173..249 over 64 distinct values, so the plumbing works and the plane was
   read. The counter was testing `w != 0xFFFF`, but the writer packs
   `R<<11 | G<<6 | B` and never touches bit 5 (`src/lodtfile.cpp:939`,
   `:1179`), so **pure white is 0xFFDF and 0xFFFF is the no-record word** —
   `w != 0xFFFF` counts RECORDS, not tints. The note now reports both, and its
   headline is the tint. This is the 2026-09-04 21:33 rule: a field that does
   not MOVE with the thing it claims to measure fails.

   **The file's own answer, worldspace-wide**: of 2,362,369 samples at LOD2,
   **4,335 (0.2%) carry a tint** and 151,168 carry a colour record. An
   independent sample of 9,216 positions across the whole worldspace, decoded
   by the Python authority rather than by our C++, agrees: 23 of 9,216 (0.25%)
   are anything but white. Commonwealth has essentially no authored terrain
   colour, so `lodt_open_colour.png` is a white sheet with a few specks and
   that is correct.

   Check 22 was then given the floor it needed: a plane may coincide with
   another only when **its own build note declares it has nothing to draw**,
   the excuse is printed, and the number of DISTINCT pictures must still reach
   8 of 9. A selector that ignored its argument would collapse all nine to one
   picture, and the planes that DO report content would not be excused, so it
   still goes red.

### Memory and time on the Commonwealth file — MEASURED

`scratchpad/lodt_20260909/measure_open.py` (psutil is absent on this machine's
Python 3.9, so the peak comes from Win32 `PeakWorkingSetSize` through ctypes,
which is monotonic in the kernel — the last poll before exit is the true peak).

| what | wall | peak working set |
|---|---|---|
| 2x2 cells at LOD2 (289 verts) — the app's own baseline | 4.4 s | **426.6 MB** |
| whole worldspace at LOD3 (~591 k verts) | 6.7 s | **1296.5 MB** |
| **whole worldspace at LOD2 (2,380,849 verts)** | **14.8 s** | **4431.2 MB** |
| the same, repeated | 14.2 s | 4433.6 MB |

Repeatable to 0.05%. Subtracting the baseline, the open costs **~4.0 GB for
2.38 M vertices, about 1.7 KB a vertex**, and the LOD3 rung gives ~1.5 KB a
vertex — linear in vertex count, not in file size.

**§4's memory table was arithmetic and it was wrong by about fifty times.** It
predicted "~20 MB of our own buffers plus the document's 64-73 MB" by counting
the vertex PAYLOAD. The payload is not what is resident: `NifModel` holds every
vertex as a tree of `NifItem`s. The build's own timing line separates the two
halves and confirms where the cost is: **the `.lodt` read is 216-447 ms** for
any plane, and **meshing and building the document is 9.3-12.0 s**. So the
format and its progressive pyramid are not the expense; the document model is.
That is a real finding, it is owed to bungo as a decision (a whole-worldspace
open at full rate is a 4.4 GB operation), and nothing in this lane changes it.

### G5 — the eleven pictures, all opened and looked at

All under `scratchpad/lodt_20260909/`. Two lit height views, then one per plane
at the same region ([-96,-96]..[95,95] LOD2) and the same camera.

| file | what it shows |
|---|---|
| `lodt_open_commonwealth.png` | the whole worldspace, lit heights, top-down. The Commonwealth is recognisable: the western highlands, the Boston basin, the coastline and the harbour islands. |
| `lodt_open_closeup.png` | cells [-8,-8]..[7,7] at the file's own full rate — the Charles, the street grid and building footprints are legible. |
| `lodt_open_height.png` | the height plane. **Rendered LIT, not flat**, and therefore byte-identical to the picture above (0 differing pixels of 194,565) — because a height view carries no colour channel at all, so its FLAT render is a blank white sheet by construction. That blank sheet is kept as `probe/lodt_open_height_flat_blank.png`; it is the same fact check 15 asserts numerically. |
| `lodt_open_ao.png` | baked sky visibility. Pale by the file's own numbers: **0..255, mean 235.7** over the whole worldspace, so most of the Commonwealth is open sky and the valleys are the faint darker traces. |
| `lodt_open_blend.png` | land-texture blend. The brown field is the fallback base where a sample's quadrant carries no base slot; the coloured patch is the **155,028 of 2,362,369 samples (6.6%)** that carry a layer, deepest stack 5. |
| `lodt_open_colour.png` | terrain colour. Near-white, correctly — 0.2% of samples carry a tint (above). |
| `lodt_open_waterheight.png` | per-cell water plane, **-3000.0..12470.0** units. One ocean level over most of the map, the inland ponds and river reaches picked out in the centre. |
| `lodt_open_watertype.png` | which WATR record a cell uses. Cyan is the worldspace default (0xFFFF); the hashed colours are the 15 named water bodies. |
| `lodt_open_cellflags.png` | uniformly yellow — **all 36,864 cells have both land and water**, which is what the writer reports independently. |
| `lodt_open_cellrange.png` | per-cell relief, deepest **25,760.0** units. Dark because most cells are gentle; the bright traces are the steep ground. |
| `lodt_open_overview.png` | the always-resident coarse grid, 1536x1536 at 8 a cell, **-8320.0..44872.0**, and the most legible elevation picture of the set. It agrees with the block pyramid to **0.000 units**. |

Ground cover is deliberately absent and its refusal is the deliverable —
`lodt_open_groundcover_refusal.txt`, rc 1:

> error: this file carries no "groundcover" plane (section flags 0xd, AO 8 a
> cell, 100 LTEX, 15 WATR, 0 GCVR, overview 8 a cell)

G2's pair and its difference image are `g2_btd.png`, `g2_lodt.png` and
`g2_diff_x8.png` (differences multiplied by 8; the 98 differing pixels trace
the rim of the central pit). Honest caveat on that pair: the Pitt worldspace
has **zero land textures, one colour word and 869 units of relief across 50
cells**, so top-down it is a flat grey square in both halves. It proves the
route and the encoding round trip; it does not exercise alphas or colour.

### Two render-hook switches that did NOT apply, verified rather than assumed

* **`WW_RENDER_SIZE` is clamped.** Every picture came out **1507x1067** whatever
  was asked for (1400x1400, 1200x1200, 360x360 inside the harness). The hook's
  own comment says the size "tops out at one screen"
  (`src/nifskope_ui.cpp:21404`), so this is a clamp, not a dropped variable.
* **`WW_RENDER_CENTER` / `WW_RENDER_DIST` did not take on this route.** The
  skill's own verification was run first: the same `.lodt` rendered at
  distances 90000 and 140000 produced **one identical file**, so the pin
  failed and was not trusted. Candidate cause, NOT proved: `load()` reframes on
  the new contents after the scene is generated (`src/nifskope.cpp:9992`,
  `ogl->setOrientation( ogl->viewState(), true )`), which would overwrite
  whatever the hook set. The discriminator is to render a plain `.nif` with the
  same two distances — a plain load takes the same reframe, so if THAT pins,
  the reframe is not the cause.

  G2's "same camera" is therefore earned differently, and the evidence is
  stated rather than assumed: both scenes build **4 shapes / 161,604 vertices**
  with X and Y identical to the bit, so the auto-framing has identical input;
  and the renderer is deterministic — two independent runs of one scene gave
  **byte-identical pictures** (0 of 194,565 terrain pixels differing).

### Line endings, by Python byte count

| file | CR | LF | verdict |
|---|---|---|---|
| `src/btdterrain.cpp` | **0** | 1350 (was 1339) | LF-only, unchanged in kind |
| `tests/spells/lodt_open.sh` | **0** | 346 (was 321) | LF-only |
| `src/nifskope.cpp` | 9234 | 10357 | MIXED, untouched this lane |
| `WW_CHANGES.md` | mixed, and stays so | | edited in place |
| `MISTAKES.md` | 0 | LF-only | |

`git diff --numstat` on `src/btdterrain.cpp` is 1093/231 — the +11 lines over
the previous 1082/231 are this lane's colour-note fix, nothing else.

### Repository state

**Nothing was committed.** bungo's "Not yet" stands. The working tree carries
the same paths as on entry plus this lane's outputs under
`scratchpad/lodt_20260909/`. HEAD is still `2dd8444`.

### What bungo's open window needs

**A restart.** `release\NifSkope.exe` changed twice today (15:22:23, then
15:30:27); a window opened before that is running the 13:42:03 image and has
neither the `.lodt` route nor the colour-note fix. The next launch has both.

## 10. Finished-work skill review (LODTOPEN3)

**Loaded and used, all three the orders named.** `nifskope-ww-build-verify` —
`tools/ww_build.sh` is its chain, and its "make's own exit code gates" rule is
why `BUILD-RC=0` was read rather than a grep's status; its
exe-newer-than-sources rule is the `find src -newer` line above.
`nifskope-ww-lodgen` — the CLI surface for G1/G2/G4, the harness names, the
byte-count rule for line endings, and the `slots`/`emit` keyword-macro warning
(no new collision this time). `nifskope-ww-render-shot` — the switch list, the
one-instance / second-monitor / unused-port rules, and, decisively, its
instruction to VERIFY a camera pin took by rendering two distances. That check
is the only reason G2 is not resting on a pin that silently did nothing.

**Written this session** (rule 1a: a procedure that cost a build or a
deliverable to learn becomes a skill the same session) —
`E:\Projects\Claude\.claude\skills\nifskope-ww-render-shot\SKILL.md` gained a
section **"Driving many renders from a script"**: the `env` rule for
conditional variables (`${flat:+WW_RENDER_FLAT=1}` as a bare prefix lands in
COMMAND position and dies with rc 127 — it cost nine pictures here), the
clamped `WW_RENDER_SIZE`, the pin-verification that failed on a generated
document, and the "a flat render of a channel-less view is a blank sheet"
trap. That belonged in that skill rather than a new one: every item is a
property of the render hook.

**Declined, with the reason.** A "measure peak working set of an exe run"
skill was the other candidate. Declined: the whole procedure is the
twenty-line `measure_open.py` now sitting in
`scratchpad/lodt_20260909/`, which is re-runnable as it stands, and a skill
restating it would be the documentation drift MISTAKES.md already has an entry
about. The one line worth carrying — psutil is NOT installed for this Python,
use Win32 `PeakWorkingSetSize` via ctypes — is a machine fact and went to
`machine_build_quirks` territory, not a new procedure.

**Two-tree check, done rather than assumed** (CONSTITUTION 1a): the live tree
`E:\Projects\Claude\.claude\skills` holds all four `nifskope-ww-*` skills; the
repo tree `.claude/skills` holds only `ww-control-calibration`. The amendment
above went to the live tree, and there is nothing to mirror.
