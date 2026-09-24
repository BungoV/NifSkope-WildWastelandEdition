# Lane LODIV7 -- `.lodi` v7: one identity a HOUSE, and sky visibility PER VERTEX

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY lane in the tree. Exe at launch:
  `release/NifSkope.exe` 2026-09-18 08:40:07, 22,718,976 B, sha1 62efc25c3871 (CHANVIEW1's; read mtime, size,
  sha1 yourself, first line of the report). Rung ONCE before your first build: `release/NifSkope.before_lodiv7.exe`
  (never delete any `release/NifSkope.before_*.exe`, `release/NifSkope.archlock1_rung.exe`,
  `release/NifSkope.at_0117.exe`, or a `NifSkope_inuse_*.exe`). Markers `scratchpad/lodiv7_20260918/BUILDING`
  (touch FIRST, REMOVE when you write DONE) / `DONE` (first word `lodiv7`). Report
  `scratchpad/lodiv7_20260918/lane_lodiv7_report.md`, INCREMENTAL (a section lands before the next step starts);
  `PENDING.md` past half your context. Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command before every build and every exe
  run; Fallout4 up = stop, PENDING.md. A NifSkope with no `--port` is bungo's window: rename the exe aside as
  `NifSkope_inuse_<pid>.exe` at link time, never kill it. Headless runs: `--port <unused>` + `WW_WINDOW_AT=1960,40`,
  one at a time, second monitor only, every path ABSOLUTE `E:/...`.
- Build: MSYS2 UCRT64, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f Makefile.Release -j8`,
  make's exit code is the gate (`nifskope-ww-build-verify`). ONE background waiter at a time. `date` for every
  timestamp; never type a time from feel.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the RULED-NOT-A-DEFECT 09:41 line, the CHANVIEW1 and SLAB1
  LANDED blocks); root `MISTAKES.md` top 15; `docs/LODGEN_NATIVE_LODO_LODI.md` s4 (the instance record, the cold
  record `u32 refFormId, i16 scolPart, u16 identity`), s4.1a, s4.7 (v5 placement AO), s4.8 (the v6 vertex-AO stream:
  `offVertexAo` 0xF4, `u32 first[instanceCount+1]` then bytes, `vertexAoBytes` 0xFC -- the layout you MIRROR);
  `src/lodifile.h` (`LodiInstance`, `LodiSrcInstance`, the header struct), `src/lodifile.cpp` (writer ~380-420, the
  v6 stream write, the reader's refusals by name, the self-tests ~1690-1740); `src/nativeemit.cpp` 1660-1720 (the
  placement bytes: `r.sky = skySum / litVerts` -- the MEAN of a per-vertex cast; `r.identity = p.objectIndex` ~1711)
  and 1900-1960 (the `place` lambda with `perVertex`, where the v6 stream is cast per library vertex against the
  scene -- your sky stream is cast in THAT loop); `src/lodgenao.h:141` `skyVisibility(p, maxT)`; `src/lodgen.cpp:4007`
  (the legacy `.BTO` path casts sky per vertex with `maxT` 300 -- same cast, same radius, so the two paths agree);
  `src/lodinative.cpp` 555-800 (the channel switch; 935 writes one colour a placement) and its note lines;
  `tests/spells/lodgen_native_decode.py` (`read_lodi`, `check_manifest` -- "identity unique over this chunk" is a
  gate that MUST KEEP PASSING), `tests/spells/lodgen_native.sh` + its expect files (`lodi.*` keys), `lodl_channels.sh`.
- Skills (repo `.claude/skills`): `nifskope-ww-build-verify`, `ww-test-harness-add`, `ww-channel-view-refuter`,
  `nifskope-ww-render-shot` (native channel table), `ww-texel-picture`, `ww-population-refuter`. Any procedure you
  invent twice becomes a skill under `scratchpad/lodiv7_20260918/skills_proposed/<name>/SKILL.md` with frontmatter.

## bungo's words, verbatim (2026-09-18 09:4x), and what the director measured before them
"The houses should be one object each though, for identity" and "We need per vertex sky visbility too".

Measured (director, `scratchpad` ident_probe*.py, HANDOFF 09:41 line): chunk 4.4.-12 of the v6 pair
`scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi` + `.lodo` has 2,446 placements,
2,446 distinct identities, 2,257 refs, 53 SCOL refs = 242 parts. The close frame (2,500 u around 24900,-41300)
holds 458 placements of 420 refs: 53 `DecoRoof1x1Str01`, 33 `DecoMainC1x1WinA01`, 27 `DecoMainA1x1Wall01`, ... --
a house is forty loose 1x1 kit REFRs, each its own identity, so the identity picture shows a patchwork. The sky
byte 0x11 is the mean of a per-vertex cast (nativeemit.cpp:1694), so the sky picture is the same patchwork. Both
are what the format stores; bungo has now ruled the format must store more.

## The work (in this order; each step lands in the report before the next)
1. **`.lodi` v7, two additions, written by default, with the way back.** Version 7 when the file carries either;
   v6 files still read; a v7 file read by the old reader refuses by name (it already refuses unknown versions --
   prove it). Switch `--lodi-v6` (nifcli, help text) writes today's v6 bytes: G1 is byte identity of a v6 bake
   before and after your change. Header: new offsets/counts in the reserved header space after 0xFC, documented
   with offsets in s4 the way s4.8 is. The reader (`src/lodifile.cpp`) and the Python reader
   (`lodgen_native_decode.py`) both learn v7, refusals by name for every new invariant (monotone offsets, byte
   counts, group ids in range).
2. **Group identity ("one object a house").** A new parallel table, `u16 group` per instance (NOT a widening of the
   8-byte cold record; a new `offGroup` table, instance order), group ids dense per chunk from 0. `identity` (the
   unique per-placement u16 the manifests join on) is UNTOUCHED and stays unique: `check_manifest`'s uniqueness gate
   passes unchanged. The grouping RULE, applied per chunk at emit time, from the placement list nativeemit already
   holds (positions, rotations, scales, `.lodo` base bounds):
   - a SCOL part's group is its SCOL reference's group: all parts of one SCOL = one group;
   - a placement whose base model path starts with `architecture\` (case-insensitive; measure how many of the
     chunk's bases that catches and list the top 20 paths it does NOT catch so bungo can widen it) joins a
     connected component over world-space bounding boxes that overlap or touch within 16 units (state the exact
     box you use: the `.lodo` base bound transformed by the placement, or the bound sphere -- say which and why);
   - everything else (trees, landscape, furniture, vehicles, signs) is its own group;
   - a house cut by a chunk border is two groups, one a side; say so in the census.
   Census words in the doctor/report line: `groups`, `groupedPlacements`, `largestGroup`, `singletonGroups`.
   PRE-REGISTERED refuters (fail on a wrong rule, show each red once): (a) two houses across a street on 4.4.-12
   get different group ids (name the two refs and the gap); (b) every SCOL's parts share one group; (c) a tree next
   to a wall keeps its own group; (d) the union of the components covers every architecture placement exactly once.
3. **Per-vertex sky stream.** Mirror s4.8 exactly: `offVertexSky`, `u32 first[instanceCount+1]` then one byte a
   library vertex in the mesh's vertex order, `vertexSkyBytes`; cast in the same `place`/`perVertex` loop the v6
   scene AO uses, `skyVisibility(p, 300)` against the same scene (own triangles, the other placements, the terrain,
   neighbouring chunks' placements), the same 0xFF-is-unmeasured convention only if v6 has one (say). REFUTER: for
   every placement the stream's mean over its vertices equals the 0x11 byte within 2 (they are the same cast;
   if they are not, find out why before writing a picture -- the byte's `litVerts` population vs the stream's
   population is the first suspect, and the answer goes in the report), and on a building placement the stream has
   > 1 distinct value (not flat). Census: `vertexSkyBytes`, `vertexSkyPlacements`, mean.
4. **The viewer** (`src/lodinative.cpp`, the CHANVIEW1 seam): `WW_LODL_CHANNEL=identity` now draws the GROUP (one
   colour a house, hashed with the stock palette exactly as today); new name `placement` draws today's unique
   identity (the old `identity` picture); `sky` draws the per-vertex stream when the file carries it, the flat
   0x11 byte on a v6 file, and its note line says which ("per-vertex stream, N bytes" vs "placement byte, N
   placements"). Note lines READ BACK from what was uploaded, as CHANVIEW1's do. `lodl_channels.sh` gains the new
   name and the v6-fallback check; every existing check keeps passing.
5. **Pictures**, `scratchpad/lodiv7_20260918/images/`, the CHANVIEW1 framing (`render_slots_e.sh`, close =
   `24900 -41300 2600 8 450`, full = the hotfix 7c arguments; see `scratchpad/chanview1_20260918/lane_chanview1_report.md`
   s4 for the exact env), on a fresh bake of chunk 4.4.-12 with the identical chunkD3 recipe SLAB1 used
   (`scratchpad/slab1_20260918/lane_slab1_report.md` has the command; add nothing but the output folder):
   `chunk_identity_{full,close}.png` (groups), `chunk_placement_{full,close}.png`, `chunk_sky_{full,close}.png`
   (per vertex), and the v6 `chunk_sky_close.png` beside the v7 one in `sky_before_after.png`. Captions carry the
   census numbers. A picture of a channel reads the channel the SHIPPED file carries, never a proxy (MISTAKES 05:0x).
6. **Gate** `tests/spells/lodi_v7.sh` (`ww-test-harness-add`): G1 v6 way back byte-identical; G2 the four grouping
   refuters + uniqueness of `identity` + group ids dense; G3 stream layout invariants + mean-vs-byte within 2 on
   every placement + >1 distinct on buildings; G4 viewer: `identity` render differs from `placement` render, `sky`
   note line names the stream, v6 file falls back by name; G5 neighbours on your final exe before/after with owners:
   `lodgen_native.sh`, `lodl_channels.sh` (48/0), `native_open.sh` (17/0/2), `render_shot.sh` (82/0),
   `lodl_open.sh` (23/0), `lodgen_slab.sh` (16/0); `native_lighting.sh` is 14/3 on the rung already, NOT yours.
   The Python reader's self-check on the vanilla-shaped fixtures (`lodifile.cpp` self-tests) extends to v7.
7. **Docs**: `docs/LODGEN_NATIVE_LODO_LODI.md` s4 header table (new offsets), new s4.9 (group table) and s4.10 (sky
   stream) with provenance; the viewer section's channel table; `docs/LODGEN_CENSUS.md` the new words;
   `nifskope-ww-render-shot` skill text delivered in the report (director applies to both trees).

## Gates (pre-registered)
- G1 `--lodi-v6` output byte-identical to the rung's bake of the same recipe; a v7 file refused by name by the rung exe.
- G2 grouping refuters (a)-(d) each shown red once against a deliberately wrong rule, then green; `identity` unique.
- G3 stream: mean-vs-0x11 within 2 on 100% of placements; > 1 distinct value on every architecture placement with
  > 8 vertices; layout invariants.
- G4 viewer: three channels render, differ from default and from each other, note lines read back; v6 fallback by name.
- G5 neighbours unchanged; find `src tests res docs -newer <launch stamp>` lists only your files.

## Rules
- Writer + reader + viewer + docs + tests. No other bake default moves. `--road-detail 1`. No decimation, authored
  LOD models only. One instance, second monitor, never a desktop capture.
- The grouping rule is a PROPOSAL bungo will rule on: put the knobs (path prefix, 16 u tolerance, box vs sphere) in
  one place with a comment, and table what each knob changes on this chunk (groups count) in the report.
- A number in a caption is a number in the report. No "fixed/final/true".

## Report (`lane_lodiv7_report.md`, incremental)
0 exe at launch + rung; 1 the v7 layout (offsets, counts, refusals by name); 2 the grouping rule, its knobs, its
census on 4.4.-12, the top-20 uncaught paths; 3 the stream, its cast, the mean-vs-byte table (worst 10); 4 viewer
names + note lines; 5 pictures with captions; 6 gate counts and each refuter shown red; 7 neighbours; 8 build
(mtime, size, sha1, the newer-than list); 9 docs + skill text; 10 WW_CHANGES paragraph + HANDOFF LANDED block for
the director to splice; 11 rows for bungo (the rule's knobs, houses cut by chunk borders, anything else he might
rule on); 12 MISTAKES entries (write them at the top of root MISTAKES.md yourself, the moment you see one); 13
finished-work skill review. END with `DONE` (first word `lodiv7`) and five plain sentences for bungo, ending with:
his open window needs a restart.
