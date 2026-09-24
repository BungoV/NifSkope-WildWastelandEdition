# Lane NATIVEVIEW1 — the viewer opens and renders the NATIVE bake

Status at 2026-09-12 20:1x: **DONE, with one gate RED and no claim that it is
not.** All three routes are written, compiled, linked and exercised end to end;
the harness runs whole; the pictures are taken. `tests/spells/native_open.sh`
reports **17 checks, 1 failures, 0 skipped** — the failure is gate (c), at the
number the brief pre-registered, with its cause measured and its refuter firing.
Nothing was sent to bungo; the director sends.

The lane stopped once in the middle, at 18:5x, because `Fallout4.exe` came up
(pid 55208) and the rule is no builds and no windows while he is playing. It
resumed from `PENDING.md` on the director's word at 19:28 with the game down, and
every build and every launch after that had its own `tasklist` check whose answer
was read first.

Three builds, three relinks; the exe on disk is **19:45:31, 22,275,072 B,
`BUILD-RC=0`**.

---

## 0. Routes added

| route | file | what opening it now does |
|---|---|---|
| `.lodi` | `src/nifskope.cpp` `loadFile()`, beside the `.lodl` branch | builds a Fallout 4 document out of the `.lodi` placements and the `.lodo` geometry beside it (`src/lodinative.cpp`) |
| `.lodl` + sheets | `src/btdterrain.cpp` `nifCreateLodtTerrainScene` | a **Height** view is drawn LIT when a `.lodt` pyramid is beside the file: colour sheet in slot 0, `_msn` in slot 1, per sheet tile |
| `.lodl` + objects | `src/nifskope.cpp`, `WW_LODL_OBJECTS` | appends the `.lodi` objects under the terrain's root so one document carries both halves |

New sources: `src/lodinative.h/.cpp` (the `.lodi`/`.lodo` scene builder, 28,742 B),
`src/lodtsheets.h/.cpp` (the `.lodt` tile unpacker and its session resource root,
10,859 B). Both are listed in `NifSkope.pro`. Neither contains a second parser of
any format: the readers are `src/lodifile.cpp`, `src/lodofile.cpp`,
`src/io/lodvfile.cpp` and `src/lodtfile.cpp`.

**Environment, all unset by default and all inert when unset**

| name | meaning |
|---|---|
| `WW_LODI_REGION="x0,y0,x1,y1"` | cells, inclusive; unset = the file's whole extent |
| `WW_LODI_LEVEL=n` | cluster-ladder level; unset = 0, full detail |
| `WW_LODI_BOXES=1` | draw the occluder boxes, as WIRE boxes |
| `WW_LODI_DUMP=<file>` | the instance census the gates read |
| `WW_LODL_SHEETS=<dir>` | where to look for `<ws>.VT.<dim>.lodt`; unset = beside the `.lodl` |
| `WW_LODL_SHEET_DIM=<n>` | ask for one level by name; unset = the finest (smallest dim) |
| `WW_LODL_SHEET_CACHE=<dir>` | where the unpacked loose DDS go; unset = the system temp dir |
| `WW_LODL_OBJECTS=<file.lodi>` | append the native objects to a `.lodl` document |

**Tiles are decoded to loose DDS, and this says so.** A `.lodt` tile is a packed
payload inside a `LDTX` container, not a file a texture loader can open, so each
tile used is unpacked ONCE to a colour + `_msn` DDS pair under the cache dir, and
that dir is pushed onto the session's Fallout 4 folder list exactly the way
`WW_LODGEN_RESOURCES` does it in `src/main.cpp` (prepend, close archives, never
`save()`). Nothing is written anywhere else and no bake output is touched.

**The one behaviour change that is visible by default**, stated plainly: when a
`.lodt` pyramid IS beside a `.lodl`, a Height open of that file is now lit and
its region snaps outward to whole sheet tiles. That is the change the brief
asked for. With no pyramid beside the file — which is the case for every `.lodl`
in the tree and in bungo's FO4CS Terrain folder (checked: only `.lodl` and
`.lodt.bak-*` files there, no `*.VT.*.lodt`) — nothing changes at all, which is
gate (a).

---

## 1. Gates

`tests/spells/native_open.sh` with `tests/spells/native_open_authority.py`. The
authority reads the `.lodi`/`.lodo` through `tests/spells/lodgen_native_decode.py`,
the independent decoder written from the format document, so our C++ never
supplies the right-hand side of any comparison.

**Whole run, 19:56: `17 checks, 1 failures, 0 skipped` — FAIL. The one failure is
gate (c), and it failed at the number the brief pre-registered; the bar was not
moved.** Log: `scratchpad/nativeview1_20260912/work/native_open.log`.

| gate | what it measures | measured | floor / refuter, measured | state |
|---|---|---|---|---|
| (a) no sheets, nothing changes | a height document built by this exe vs by `release/NifSkope.before_nativeview1.exe`, `cmp` | byte-identical | an empty `WW_LODL_SHEETS` dir byte-identical; a non-height plane byte-identical with the real sheets present; `lodl_open.sh` 23 checks 0 failures | **PASS** |
| (b) the instance census | manifest rows inside the chunk == `.lodi` chunk table == viewer census | **676 == 676 == 676**; worst world-position difference **0.0046 u** (floor 1 u) | cells (-20,32) dim 4: file says **0**, viewer placed **0** | **PASS** |
| (c) the same scene as the `.BTO` | coverage-mask IoU from one pinned camera, bar 0.95 | **IoU 0.8179** | a DIFFERENT chunk's `.BTO` gives **IoU 0.0001** | **FAIL** |
| (d) the lit terrain is the sheets | NCC and mean absolute colour difference against the `.BTR` of the same cells | **NCC 0.6008** (bar 0.45), **MAD 35.821** (bar 48) | a DIFFERENT chunk's `.BTR` **NCC 0.2023**; the same `.BTR` mirrored in Y **NCC -0.0242** (bar 0.10); an empty-sheets render IS the data view byte for byte; the lit render is NOT the data view (MAD **89.427** > 8) | **PASS** |

### (a) — the module-off identity

Three separate ways of having no sheets all give the same bytes as the exe from
before this lane, and the `.lodl` harness that existed before this lane keeps its
count exactly (23 checks, 0 failures). Separately, the **bake** byte-identity was
measured outside the harness: the same one-chunk bake
(`--terrain-region -20 24 -17 27 --dim 4`, identity ON) run from this exe and from
`release/NifSkope.before_nativeview1.exe` gives byte-identical
`Commonwealth.4.-20.24.BTO` (1,276,441 B), `.BTR` (36,942 B),
`.BTO.manifest.txt` (53,547 B), `native/Commonwealth.lodi` (37,248 B) and
`native/Commonwealth.lodo` (9,657,316 B). This lane changed no writer.

### (b) — closed, including the manifest leg

The manifest leg was owed because the look bake runs with `--no-identity`, so it
writes no placement rows. It was obtained the way the brief allows: **one chunk**
re-baked with identity ON into this lane's own out-dir
(`scratchpad/nativeview1_20260912/bake_identity.sh`), never the whole
Commonwealth and never bungo's Data/Terrain. Result:

```
the manifest has            678 placement rows
of which inside the chunk   676
the .lodi's own chunk table 676 instances   (independent decoder)
the viewer's census         676 rows
keys missed / invented      0 / 0
worst world-position diff   0.0046 u        (floor: 1 u)
```

The 2 outside rows are named and measured, not waved away: `0002c4fe` part 5 lies
117.1 units past the chunk edge and `00145b5d` part 3 lies 46.0 units past it,
both trees whose origin sits in the neighbouring chunk. `lodgen_native_decode.py
--manifest` passes its own 12 checks, 0 failures, on the same pair.

### (c) — FAILS at 0.8179 against the briefed 0.95, and the cause is measured

The refuter fires hard (a different chunk: 0.0001), so the test discriminates;
the scene is simply **larger** than the `.BTO`'s. Measured from the two masks:

```
the .lodi scene covers   219,499 px
the .BTO covers          183,734 px
intersection             181,415 px    = 98.7 % of the .BTO's own pixels
```

So the native view contains essentially all of the `.BTO` and then some. Three
candidate causes were tested rather than argued: the alpha thresholds are
identical on both sides (0 or 128, read from the blocks); adding the vanilla data
root to the resource stack moved 3 px; and the remaining difference tracks
alpha-tested canopies. The native scene samples **each base's own full-resolution
LOD material**, while the `.BTO` samples the bake's downsampled object atlas, so
leaf texels the atlas lost are still drawn here. That is a real difference between
the two products, not a placement error — (b) already proves every placement is
within 0.0046 u. **The bar stays at 0.95 and the gate stays red** for bungo to
rule on: either the bar becomes a two-sided containment test (">= 97 % of the
.BTO's pixels are inside the native scene, native coverage within 1.25x") or the
native view is asked to sample the atlas.

### (d) — the lit terrain really is the sheets, and it is lit correctly

MAD alone does not discriminate (two different chunks of the same worldspace
differ by about as much), so the harness also runs a normalised cross-correlation
of the two lumas, which is structure and not tone. Own chunk **+0.6008**, a
different chunk **+0.2023**, the same chunk mirrored in Y **-0.0242** — the Y
mirror is the failure this route is most exposed to, because the sheet rows run
NORTH-first while the mesh runs SOUTH-first. A `.BTR`-against-itself noise floor
was measured at 0.000 MAD.

**The lighting had to be fixed to get here, and the fix is one finding worth
keeping.** The first lit render came back about 40 % too dark (mean luma 70.6
against the `.BTR`'s 121.1, MAD 50.457). Cause, read from the bake's own `.BTR`:
the sheet's normal map is an `_msn`, a MODEL-SPACE normal map, and the shader
block did not say so. The `.BTR` carries Shader Flags 1 = 0x80401000
(`Model_Space_Normals` set, `Specular` clear — the two are documented as
incompatible) and Shader Flags 2 = 3 (`ZBuffer_Write` | `LOD_Landscape`). The
sheet branch in `src/btdterrain.cpp` now sets those three bits and nothing else;
the no-sheets arm is untouched, which is why (a) could not move. After the fix:
MAD 35.821, mean luma 92.5.

A second thing was measured rather than eyeballed: the lit render looks mottled
beside the legacy one, and the worry was black speckle. **Zero pixels fall below
luma 20.** It is the sheet resolution — the tiles are BC1 272x272 (136 texels a
cell) against the legacy 2048x2048 per-chunk DDS (512 texels a cell). Not a new
artifact class, and not something this lane can change without touching a writer.

### The pin, so (c) and (d) are arithmetic and not a remembered screen position

Chunk (-20,24) dim 4 is world X -81,920..-65,536 and Y 98,304..114,688, so the
camera is `WW_RENDER_CENTER="-73728,106496,0" WW_RENDER_ORTHO=8192
WW_RENDER_VIEW=1`. The harness computes it from `CX/CY/DIM`, and `upp` is read
BACK from `release/ww_camera_pin.log`. The last grab of the run:

```
lookat=-73728.0000,106496.0000,0.0000  halfW=8192.0000 halfH=7912.0000
vp=1024x989  upp=16.000000
```

**Two spaces, measured, and it corrects what this lane assumed at the start:** the
`.BTR`'s shape sits at translation 0,0,0 over chunk-local vertices, so a legacy
terrain frame needs a CHUNK-LOCAL camera (`8192,8192,0`); the `.BTO`'s
`BSSubIndexTriShape` carries Translation (-81920, 98304, 0), so it lives in WORLD
space and takes the same camera as the `.lodi`. Photographing the `.BTO` at the
chunk-local centre gives an empty 5,179-byte PNG and an IoU of 0.0000 — which is
exactly what happened once, before the file was read.

**`WW_RENDER_SIZE` floors the WINDOW WIDTH near 1024** on this exe: 480x480 came
back 1024x445, 1024x1024 comes back 1024x989. Every frame in this lane is read
back with PIL. This is in the render-skill amendment.

### A fact the gates have to live with

The `.lodi` of this bake reports **`occluderCount` 0**, so `WW_LODI_BOXES=1` draws
nothing on this region. The switch is implemented and says so in words; it cannot
be demonstrated on these inputs.

---

## 2. Build and chain

Three builds this session, each with the game down (`tasklist | grep -i Fallout4`
as its own command, its answer read before the build command was typed) and after
PANEL1's relink lock cleared.

| | |
|---|---|
| rung, copied once before the first build | `release/NifSkope.before_nativeview1.exe` — 22,154,240 B, 18:07:58 — **never delete it** |
| first build | 18:43:27, 22,276,096 B, 4 translation units, 0 errors |
| second build (the material-name fix) | 18:46:50, 22,275,584 B, `BUILD-RC=0` |
| third build (the `_msn` shader flags) | **19:45:31, 22,275,072 B, `BUILD-RC=0`** — this is what is on disk |
| relinks | 3, counted |

The chain's three closing legs, all run after the third build:

  * `git status --porcelain -- src res tools tests` lists only
    `tests/spells/native_open.sh` and `tests/spells/native_open_authority.py` as
    newer than the exe. Those are harness files and are not compiled; **no `src`
    file is newer than the exe.**
  * every object in `GeneratedFiles/.obj/` is newer than its own source (the
    objects live in `GeneratedFiles/.obj/`, not in `release/`).
  * `make -n` prints **0** compile lines.

**Harnesses this change reaches**: `lodl_open.sh` (the `.lodl` open route is
edited), `native_open.sh` (new), `render_shot.sh` (the render hook is the only way
these documents are photographed). **Skipped, and why**: everything under
`lodgen_*` — this lane changed no writer and no bake output, so a bake gate can
only re-measure what it measured yesterday (and the bake byte-identity was checked
directly instead, see (a)); `water_*`, `hkx*`, `gltf_*`, `skeleton_overlay`,
`files_tab`, `animws`, `ui_align` — nothing they touch is on any path this lane
edited.

---

## 3. Pictures

Three labelled panels in `scratchpad/nativeview1_20260912/images/`, ten frames in
all, every one rendered by this lane at 1024x1024 asked (1024x989 delivered, the
width floor), `WW_RENDER_CLEAN=1`, one window at a time on `--port 42931` at
`WW_WINDOW_AT=1960,40`. Sizes read back with PIL. **Nothing was sent; the director
sends.**

| file | size | bytes | what it shows |
|---|---|---|---|
| `i_terrain_and_objects.png` | 2078x2156 | 4,802,724 | NATIVE `Commonwealth.lodl` lit by `Commonwealth.VT.2.lodt` with `Commonwealth.lodi` appended — 4 terrain shapes, 1,156 vertices, plus 676 placements in 50 shapes, 36,866 vertices — top and oblique, LEGACY column beside each |
| `ii_objects_only.png` | 2078x2122 | 2,048,830 | NATIVE `Commonwealth.lodi` alone, cells [-20,24]..[-17,27] at cluster level 0: 676 placements read, 676 drawn, 33 bases, 50 shapes, 36,866 vertices — LEGACY `.BTO` beside each |
| `iii_far_region_levels.png` | 2078x1098 | 399,567 | the whole file, cells [-20,20]..[-9,35], at cluster level 7 (3,526 placements, 52 bases, 54 shapes, 22,358 vertices) beside level 0 (the same 3,526 placements, 79 shapes, 178,009 vertices) |

Every caption is burned into the picture, and every count in a caption comes from
that render's own notes on stdout, not from memory.

**Two honesty notes are burned into the pictures themselves**, because the brief
says to say so rather than fake it:

  * panel (i)'s LEGACY column is a **composite**: the bake keeps terrain and
    objects in two files and two spaces, so the `.BTO` frame's non-background
    pixels are pasted over the `.BTR` frame. Both frames are the same
    orthographic camera and the same window, so nothing moved; the caption says
    it and gives the pasted pixel count.
  * panel (iii) has **no legacy control and no impostor cards**. `bake_look.sh`
    passes no `--impostors`, so this bake carries no aggregate cards at all, and
    the 12 chunks are 12 separate `.BTO` files, so there is no single legacy file
    to put beside it.

The LEGACY control column needs a shim resource root,
`scratchpad/nativeview1_20260912/resroot/` (45 files, 233 MB), because a `.BTO`
asks for the object atlas under `Textures/Terrain/Commonwealth/Objects/` while the
look bake wrote it to `out/look/tex/Objects/`. Without the shim the legacy control
renders MAGENTA — `work/bto_tex.png` is that picture.

---

## 4. Owed / red / bungo's calls

**Owed**
1. **Gate (c) at 0.8179 against the briefed 0.95.** Cause measured (the native
   view samples each base's full-resolution LOD material, the `.BTO` samples the
   downsampled atlas; 98.7 % of the `.BTO`'s pixels are inside the native scene).
   Needs bungo's ruling: change the test to two-sided containment, or make the
   native view sample the atlas. **The bar was not moved by this lane.**
2. `WW_LODI_BOXES=1` cannot be demonstrated on these inputs (`occluderCount` 0).
   The code path is written and says so in words; it has never drawn a box.
3. The sheets are BC1 272x272 — 136 texels a cell against the legacy 2048x2048
   per-chunk DDS's 512. The lit view is correspondingly coarser. Changing that
   means changing a writer, which this lane is forbidden to do.
4. The render-skill amendment (`SKILL_AMENDMENT.md`) is drafted, not spliced —
   the director splices skills.

**Not owed, and why**
  * the AO greyscale picture: identity is off in the look arm, so there is no AO
    to photograph on these inputs.
  * an impostor-card picture: this bake carries none.

**Red**
  * nothing is red any more in the sense of "unexercised". The `.lodl` sheets
    route — the largest risk in the lane when it was written — has now been run
    end to end and measured against the `.BTR` from three directions.
  * the one genuinely open number is (c)'s 0.8179.

**bungo's calls**
  * gate (c): re-shape the test, or re-shape the view. (1) above.
  * nothing else. His ruling ("add a native view for these, yeah") is what this
    lane is.

---

## 5. Mistakes

Ten, written to `MISTAKES.md` at the top the moment each was recognised and
copied in `MISTAKES_ENTRIES.md`. In one line each:

1. the game check and the exe launch were the same shell command, so the check
   could not guard the launch — and bungo was in the game;
2. a bash heredoc halved the backslashes in a hook-up script, twice, after the
   repo skill that forbids exactly that had been read;
3. anchor counting SUBTRACTED CRLF hits from LF hits on a mixed file and
   cancelled a real match to zero;
4. the already-applied probe matched the anchor's own first line, so three
   edits were silently skipped;
5. `materials\` was prepended to material paths that already resolve, which is
   what MADE them miss (`get_full_path` erases everything before the archive
   folder, and only when it is not at offset 0);
6. I assumed the `.BTR` and the `.BTO` of one chunk share a camera space — the
   `.BTR` is chunk-local, the `.BTO` carries the chunk's world origin — and read
   the resulting empty frame as a failure of the thing under test;
7. `grep -c` prints `0` and exits 1, so `|| echo 0` appended a second zero and a
   census capture became `0` twice;
8. the heredoc-backslash trap a THIRD time, this one leaving a patch silently
   unapplied and costing a ten-frame render run;
9. I built a shader block pointing at an `_msn` without declaring it model-space,
   so the lit land came back 40 percent too dark — the flags were in the bake's
   own `.BTR` the whole time.
10. a document append through `python -c` in a double-quoted bash string lost
   every backticked code span to command substitution — an entry this ledger
   already carried from 2026-09-11, which I had not read that far to find.

---

## 6. Skill review

`.claude/skills/nifskope-ww-render-shot/SKILL.md` — **amendment owed**, drafted
in `scratchpad/nativeview1_20260912/SKILL_AMENDMENT.md`: a new recipe section
"Photographing a BUILT document (.lodl, .lodi)" giving the open-by-environment
pattern, the chunk-footprint pin arithmetic, and the rule that a built document's
notes are read from stdout as part of the picture's evidence. Not spliced into
the skill file by this lane — the file is listed here so the director can splice
it, per the usual division.

`ww-anchored-hookup` needs no change; it already says what mistake 2 broke.
`ww-module-off-is-identical` needs no change; gate (a) is its pattern applied
verbatim.
