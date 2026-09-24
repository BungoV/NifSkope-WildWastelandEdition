# CELLVIEW3 -- deliverable text for the director

Lane CELLVIEW3, tree `E:/Projects/NifskopeWildWastelandEdition`, branch main,
HEAD 720762a. Nothing is committed; `WW_CHANGES.md` and `HANDOFF.md` are NOT
edited by this lane -- the text below is for the director to splice.

## The exe

| | |
|---|---|
| at launch | 23,619,072 B, 2026-09-19 18:14:53, sha1 `ef4dab1f9c8a819e8692e4bd40e430d96e7329b8` |
| rung taken once | `release/NifSkope.before_cellview3.exe` (that exe, byte for byte) |
| built | 23,625,216 B, 2026-09-19 18:58:55, sha1 `ee87eb9ee6bfe45a199cd5c85820c333c6572256` |

Sources changed: `src/cellview.cpp`, `src/cellground.cpp`, `src/cellground.h`,
`src/lodgen.cpp`, `src/nativeemit.h`, `src/io/material.h`,
`src/io/materialfile.cpp`, `tests/spells/cell_pick.sh`, and a new
`tests/spells/cell_legend_colour.py`. All LF-only (CR count 0, measured with
python byte counts). `MISTAKES.md` spliced CRLF-to-CRLF, CR 10300 -> 10360.

## 1. The identity overlay -- the grey was RIGHT, and could not say so

The question the brief asked first was which of three things the 154 grey
placements were. The answer is (a), measured, not argued:

Independent join (`scratchpad/cellview3_20260919/join_probe.py`: the `.lodi`
read with the offsets `src/lodifile.cpp:26-32` declares, the plugin walked by
`tests/spells/cell_census.py`'s own GRUP reader, nothing asked of the viewer).
Sanctuary -20,7, 142 REFRs:

| base type | LOD model on the base | in the `.lodi` | NOT in it |
|---|---|---|---|
| CONT | no | 0 | 3 |
| FURN | no | 0 | 3 |
| LIGH | no | 0 | 2 |
| MSTT | no | 0 | 8 |
| SCOL | (see below) | 15 | 5 |
| STAT | yes | 11 | 0 |
| STAT | no | 0 | 95 |
| **total** | | **26** | **116** |

**Drawn, the base HAS a LOD model, and yet no group: 0.** There is no join
defect (b) and no coverage defect (c) -- the bake covers cells x -20..-17,
y 4..7, and -20,7 is inside it. The rule holds with no exceptions: a reference
is in the bake if and only if its base -- or, for a SCOL, at least one of its
parts' bases -- carries a LOD model. All 15 SCOLs in the bake have such a part;
all 5 outside it (MetalShelf03Debris02, MetalShelf04Debris01,
HWSingleRampDown01SCVine01, BranchPile03 twice) have none. The arithmetic of the
154: 95 no-LOD STATs + 15 MSTT/FURN/CONT + 44 SCOL parts.

So the repair is the one the brief prescribes for (a). The single grey sentinel
is split in two, by a new `Placement::expectLod`:

* `0xffffffffff` "no LOD model on the base (correct)" -- flat grey 0.35;
* `0xfffffffffe` "has a LOD model but no group (a defect)" -- red 0.95,0.10,0.10,
  so one pixel of it is visible;
* `0xfffffffffd` "unknown" -- the overlay has nothing to say (other overlays).

And the legend's lie is repaired at its cause. The draw site wrote 0.35 grey
while the legend called `overlayColour()` on the same key and printed mauve
0.60,0.21,0.37: two code paths, two answers. There is now ONE --
`overlayKeyColour()` -- called by both, so printed rgb equals drawn rgb by
construction. The legend also prints the sentinel's words.

## 2. The bare ground -- two bugs, not one, and no worldspace default to inherit

What the game does where a quad has no texture: **nothing that we can copy.**
The Commonwealth WRLD record names no default landscape texture at all -- every
subrecord of `0000003C` is dumped in
`scratchpad/mountains_20260907/report_mountains.md` R6, and `DNAM` is two
*floats* (default land height, default water height), not formids. There is no
`LTEX` anywhere on the record. So "use the worldspace default" is not an option
that exists.

What was actually wrong, measured with an independent LAND decoder
(`scratchpad/cellview3_20260919/ground_probe.py`) before any code was written:

* Quadrants 0, 1, 2 of -20,7 carry BTXT `0001F78C`; **quadrant 3 has no BTXT.**
* Quadrants 0, 1, 2 each also carry an ATXT layer whose **LTEX is formid
  `00000000`**. Wherever that null layer reached the 0.5 floor it WON, replaced
  a perfectly good base texture with nothing, and blanked the quad: 7 + 13 + 39
  = **59** of the 133.
* The other **74** are quadrant 3, where no layer reached 0.5 and there was no
  base to fall back on.
* Cell 5,-11 has a BTXT on all four quadrants and no null layer, which is why
  downtown never showed this at all.

Two repairs in `src/cellground.cpp`: a layer naming the null form may not win,
and where the quadrant has no BTXT the floor is any paint at all (the 0.5 floor
only ever meant "dominant enough to replace the base"; with no base there is
nothing to beat). Measured: **133 bare -> 24**, and the 24 are exactly the quads
the independent probe predicted -- quadrant 3 corners where every layer is at
zero opacity, genuinely unpainted in the plugin.

Gate row 7 is repaired, not lowered. It used to read *"most quads resolved a
texture"* with a floor of `>= 1024` -- 32x32, the whole population -- so a row
named "most" could only pass at 100% and had been red at 891 since it was
written. It is replaced by an **identity**, `bare == unpainted +
LTEX-with-no-texture`, which holds in any cell of any worldspace and cannot be
evaluated at all on the old code (neither term existed), plus the measured
ceiling of 24 with the rule stated above it. The floor was never lowered to
pass; it was replaced by something that can fail.

**The VTXT blending question, answered and left alone.** The opacity grids are
already read, so the data cost is zero -- but the change is not small. The
mosaic emits one quad into one bucket per texture and the buckets are unordered;
blending needs the same quad emitted once per contributing layer, in paint
order, with per-vertex alpha. `CellGroundVert` has no alpha channel (its 3-float
`rgb` is VCLR, which the overlay also uses), every blended quad needs an
`NiAlphaProperty` and a draw order the bucket weld deliberately throws away, and
geometry multiplies by the layer count. That is a rewrite of the emitter, not a
tweak. Reported and not done.

## 3. The magenta downtown -- confirmed on a named car, two mechanisms

`Vehicles\Automotive\Sedan02_Postwar.nif` (NIF 20.2.0.7, BSVersion 130, 31
blocks): 5 x BSMeshLODTriShape, 4 x BSLightingShaderProperty, **1 x
BSEffectShaderProperty**, and only **4** BSShaderTextureSet. The five material
strings are four BGSMs plus `Materials\Vehicles\Automotive\Car_Glass01.BGEM`
(271 bytes on disk, magic `BGEM`, version 2). So the glass shape had no texture
from either source, `tex0` stayed empty, and an empty diffuse binds the
missing-texture MAGENTA under `Scene::DoErrorColor` (`src/gl/renderer.cpp`
~951). Two mechanisms, both real:

1. `lodgenLoadModel` read a material only when the name ended `.bgsm`, so the
   BGEM was never opened.
2. Even given the name, `cellview.cpp`'s `isMaterialFile()` accepted `.bgem` and
   the writer put it in the **Name of a BSLightingShaderProperty**, which the
   renderer cannot resolve -- ten empty slots, magenta again.

**Which repair: the first one the brief offers -- read the `.bgem`, so the shape
draws textured.** `EffectMaterial( const QByteArray & )` was the only missing
piece (`Material::openData` already accepted the BGEM magic); the base map lands
in a NEW field `LodSrcShape::effectTex0`, never in `tex0`, because `tex0` feeds
the far-LOD bake whose output is pinned byte for byte -- the bake is unchanged
BY CONSTRUCTION, and `lodgen_native_baseline --check` is the refuter.
`isMaterialFile()` now means `.bgsm` only. Measured on cell 5,-11: **9 shapes
textured from a `.bgem`**.

The second option is kept as the floor under it: a shape that names a material
and resolves NOTHING from any of the three sources is drawn neutral grey
(`#FFB0B0B0`, the same one-texel trick `src/gl/gltex.cpp:172` reads) and
**counted** in a new census line. Magenta now means one thing only: a texture
path the renderer went looking for and did not find.

## Text for WW_CHANGES.md

**Cell view: the identity grey says which grey it is, the ground stops blanking
itself, and effect materials are read (2026-09-19).**
The `.lodi` identity overlay used to paint every reference it could not find a
group for in one grey, and print a different colour for it in the legend. The
grey was measured and it was CORRECT -- of Sanctuary -20,7's 142 references, the
count of "has a LOD model and yet no group" is zero -- so the bucket is split
instead of repaired: "no LOD model on the base (correct)" stays grey, "has a LOD
model but no group (a defect)" is red, and both the picture and the legend now
take their colour from one function, so they cannot disagree. The painted ground
stopped losing 133 of a cell's 1024 quads: an ATXT layer whose texture is the
null form could beat the quadrant's base texture and blank the quad, and a
quadrant with no base texture had nothing to fall back on. Both are repaired and
the census now says, of the bare quads that remain, how many are simply
unpainted in the plugin (24, and the worldspace defines no default landscape
texture to inherit). Shapes under a BSEffectShaderProperty -- car glass, most
visibly -- are read out of their `.bgem` and draw textured instead of magenta;
anything whose material will not read at all is drawn neutral grey and counted,
so magenta is reserved for a genuinely missing file. The far-LOD bake is
untouched: the effect texture has its own field and the byte-identity baseline
still checks.

## Text for HANDOFF.md

CELLVIEW3 landed on the 18:58:55 exe (23,625,216 B, sha1 ee87eb9e). Three
repairs, all measured first: (1) the identity overlay's grey bucket is split
into "no LOD model (correct)" and "has a LOD model but no group (a defect)",
after an independent `.lodi`+plugin join showed the defect half is EMPTY on
Sanctuary -20,7 -- there is no join or coverage bug, the overlay simply could
not say so; legend and picture now share `overlayKeyColour()`. (2) the ground
mosaic went from 133 bare quads to 24 on that cell, by refusing a null-form ATXT
layer the win and dropping the 0.5 floor where the quadrant has no BTXT; the
remaining 24 are unpainted in the plugin and there is no worldspace default to
inherit (mountains R6). (3) `.bgem` effect materials are read into their own
field and the cell viewer no longer puts one in a lighting property's Name --
9 shapes in cell 5,-11 now draw textured that were magenta. `cell_pick.sh` row 7
was replaced by an invariant; four gate rows are new, including the first one in
this tree that compares a legend to the pixels it describes
(`tests/spells/cell_legend_colour.py`). Owed: bungo's eye on the three
before/after pairs; the VTXT per-vertex blend is reported and NOT done (it needs
an alpha channel the ground vertex does not have and a draw order the bucket
weld throws away).

## Gates: counts before -> after

`tests/spells/cell_pick.sh` on Sanctuary -20,7 with an ABSOLUTE `LODI=` path
(a relative one is silently refused by the exe -- see MISTAKES):

| row | before the repair | after |
|---|---|---|
| ground: quads that resolved a texture | 891 of 1024 | **1000 of 1024** |
| ground: bare quads | 133 | **24** (= 24 unpainted + 0 LTEX-with-no-texture) |
| row 7 "most quads resolved a texture" | floor `>= 1024`, i.e. RED at 891 since written | replaced by the identity + a measured ceiling of 24 |
| identity: grey "unknown" | 154 of 240, one bucket, no reason given | **154 "no LOD model (correct)" + 0 "has a LOD model but no group"** |
| legend rgb vs the pixels drawn | never checked; legend said 0.60,0.21,0.37, draw site wrote 0.35 grey | distance **0.014**; distance to the old mauve **0.283** (the row's own control) |
| downtown 5,-11: shapes textured from a `.bgem` | 0 (`git show HEAD:src/lodgen.cpp | grep -c effectTex0` = 0) | **9**, 0 drawn neutral grey |
| whole script | (rows 7 and RED failing) | **PASS, 0 failures, 0 skips** |

Neighbours, and why each was picked:

| gate | why it was picked | result |
|---|---|---|
| `cell_open.sh` | the same viewer's open path -- the overlay writer changed | **PASS**, 2 overlay buckets |
| `render_shot.sh` | the shot hook every picture in this report is taken with | **82 checks, 0 failures, PASS** |
| `harness_window.sh` | GUI window/settings harness; the cell view now writes a different document | **15 checks, 0 failures, 0 skips, PASS** |
| `native_open.sh` | `NativeSrcShape` grew two members, so the native emitter's reader had to be re-proved | **17 checks, 0 failures, 2 named skips, PASS** |

One note on `harness_window.sh`: that gate launches a rung exe
(`NifSkope.before_harnesswin2.exe`) itself, as the floor under its recent-files
row. It is safe -- the harness forces its own settings scope, and the list it
printed is the fixture's `Z:\harnesswin2\...` paths, not bungo's real Recent
Files -- but it is the gate's design, not a choice of this lane, and it is worth
knowing before anyone reaches for the "never run an old rung" rule.

**Every new row was shown failing on the pre-repair state** -- but by reasoning
or by a git-HEAD control, never by launching an older rung with a GUI. The
legend row's control is computed inside the row itself (the distance to the
colour the broken legend printed). The `.bgem` row's control is
`git show HEAD:src/lodgen.cpp | grep -c effectTex0` == 0: the count could only
have been zero before.

## The byte-identity baseline, and a nondeterministic file that is not mine

`tests/spells/lodgen_native_baseline.sh --check` FAILS with 6 CHANGED files --
and reports **the identical 6** when run against
`release/NifSkope.before_cellview3.exe` (headless `-no-gui`, never a GUI). The
drift predates this lane.

Because "the baseline is already red" is not an answer, a direct A/B was run
(`scratchpad/cellview3_20260919/bake_ab.sh`: the same four chunks plus the
region, baked by both exes into separate trees, sha256 lists diffed, with a
"fewer than 8 files = refuse" floor after the first run silently compared two
empty lists). 25 files each, **24 byte-identical**. The one that differs is
`region/Commonwealth.lodb` -- and **two runs of the SAME exe give two different
`.lodb` hashes**, so that file is nondeterministic and the baseline cannot be
green for anyone. Filed as its own task ("Make the .lodb writer deterministic",
task_8e593537); not repaired here, because it is outside this brief and would
change bake output.

## MISTAKES.md

Four entries were spliced into the root ledger, newest first, CRLF-to-CRLF by
byte splice at offset 309 (before the first `## ` header). Final file 646,222 B,
CR 10452 = LF 10452 = CRLF 10452, i.e. pure CRLF, measured with python byte
counts and not with grep:

1. a wait condition that matched its own prose freed the exe slot early (a
   one-NifSkope tree: the wait matched `native_open.sh` inside another gate's
   explanatory line, and `tasklist` is empty in the gap BETWEEN two gates);
2. a join table grouped by a column that lies for SCOLs;
3. a paragraph added after the `*/` that closed the comment (prose became code,
   cost a four-minute build);
4. a byte-scan used as evidence about a format that has a reader in this tree.

## Skills

`.claude/skills/ww-legend-matches-picture/SKILL.md` -- prove a printed legend is
the colour actually drawn by rendering the same camera twice and dividing,
rather than hunting for pixel values (the overlay is a vertex colour, so the
legend's rgb appears nowhere in the PNG). Written to BOTH trees, equal sha1
`7f8a1c2e92391e2b98698affe0586de433a54147`. Not copied to `E:/Tools/AISkills`:
it is a viewer/debug-overlay procedure, not FO4 modding.

## 6. The pictures, and what is still wrong in them

All three pairs are in `scratchpad/cellview3_20260919/images/`, before and after
from the SAME camera (`*_small.png` are half-size copies for reading in chat).
Nothing is under the repo-root `images/`.

**Sanctuary -20,7, ground** (`before_ground.png` | `after_ground.png`, centre
-79872,30720,0, ortho 2457, 1822x925). Before: flat grey blocks scattered right
across the cell -- the 133 quads. After: they are gone, the ground is continuous.
What is STILL wrong: the mosaic is hard-edged. Every quad takes one texture at
full strength, so gravel meets dirt on a straight 128-unit line with no
transition anywhere. That is the VTXT blend reported above and not done, and it
is the most visible thing left in this view.

**Sanctuary -20,7, identity with legend** (`before_identity.png` |
`after_identity.png`, same camera). The two frames are nearly the same picture,
and that is the result: the object tints did not move, because the grey was
right. What changed is the legend -- it now names two reasons instead of one
"unknown", and the rgb beside each key is the rgb on screen (0.014 away,
measured). What is still wrong: at this zoom the hash-wheel colours of the 20
real groups are hard to tell apart, several land in the same yellow-green.

**Downtown 5,-11, the cars** (`before_downtown.png` | `after_downtown.png`,
centre 22528,-43008,0, same ortho and size). Before: the red car top-left has a
solid magenta windscreen, side glass and light bars; the pale car top-right is
blotched magenta; a small magenta shape sits mid-right. After: **magenta-ish
pixels 2826 -> 0**, and the red car now has grey-tinted glass over its seats
that reads as a windscreen. What is STILL wrong: the small shape mid-right is
now solid BLACK, edge to edge. It is not magenta, so by the new rule it is not a
missing file, and it was not drawn neutral grey, so its material read -- it is
drawing a texture (or a lighting result) that is black. I have not diagnosed it
and I am not claiming it is fine; it is the one thing in this view a viewer
would still point at.

## What this lane did NOT do

* Brief item 4 (a pick screenshot composing the GL view with the dock) -- not
  done. It was conditional on items 1-3 closing "with context to spare", and the
  context went on the join measurement and the three repairs.
* The VTXT per-vertex blend -- reported, costed, not done (see section 2).
* `region/Commonwealth.lodb` determinism -- filed, not repaired.
* Nothing is committed. `WW_CHANGES.md` and `HANDOFF.md` are untouched.
