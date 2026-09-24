
## 3. The identity screenshots (deliverable A)

> *"send me screenshots of the chunks with our identity"* -- bungo, 04:3x.

`ident_pics.py`, log `ident_pics.log`, 13 pictures in `images/ident_*.png`.
Every picture is a PAIR over the same G-buffer, the same geometry and the same
shading, so the only difference between the panels is **which id decides the
colour**:

| | LEFT | RIGHT |
|---|---|---|
| `ident_<camera>.png` | one flat colour per **GROUP** -- our shadow identity | one flat colour per **PLACEMENT** -- every reference its own thing |
| `ident_east_layermerge.png`, `ident_full_layermerge.png` | the **PROPOSAL**: groups merged under a Bld/Building-named CK layer | the groups we ship today |

**How to read them.** Where the two panels look the same, the grouping did
nothing. Where the LEFT panel is **one colour spread over several buildings**,
the file has told the shadow map that all of them are one thing, so none of them
may shadow another. Where the LEFT panel is **many colours over one building**,
the opposite mistake: that building will shadow itself at the seams.

**What the pictures are not.** No shadows are drawn -- this is the identity, not
its consequence. Shading is flat identity colour times N.L at a plain high sun
(azimuth 120, elevation 35) purely so the shapes read. Terrain is flat grey
because it is its own identity by rule. The 149 tree-like placements are
desaturated to a green-grey so they cannot be mistaken for architecture; they
are still one colour each, and each tree is its own group.

**The caption every picture carries:** `588 groups | 2,449 placements |
468 singletons | largest group 205 placements`, plus the in-frame counts for
that camera.

### The files

| picture | what it shows |
|---|---|
| `ident_east.png` | the east camera, 146 groups / 719 placements in frame |
| `ident_street.png` | the street camera -- the one bungo judges by |
| `ident_close.png` | the low oblique from the south-west |
| `ident_full.png` | the whole chunk obliquely |
| `ident_wide.png` | **the wider view**: a million units up on a long lens, north-up, the whole chunk and its margin |
| `ident_east_layermerge.png`, `ident_full_layermerge.png` | the layer-merge PROPOSAL, labelled as a proposal in its own title strip |
| `ident_cu_DN135_GwinnettExt.png` | **the worst case**: 260 placements in ONE CK layer, shattered into **205** shadow identities |
| `ident_cu_AndrewStation.png` | 230 placements in one layer, **7** identities -- the opposite failure |
| `ident_cu_Theater47_Bld01.png` | the clean case: 56 placements, **one** layer, **one** identity |
| `ident_cu_SouthBostonBlock34.png` | 110 placements -> 95 identities |
| `ident_cu_SouthBostonBlock16.png` | 45 -> 33 |
| `ident_cu_SouthBostonBlock02.png` | 73 -> 21 |

Theater47_Bld01 IS on this chunk, so the clean case is the real one and not a
substitute. The last three are the chunk's own worst groups-vs-layer cases,
found by counting v7 groups inside each CK layer (`ident_pics.log`).

### What the close-ups say, in one line each

**DN135_GwinnettExt is the shattering case and AndrewStation is the fusing
case, and they are the same defect seen from two sides.** The v7 rule joins
architecture placements whose world boxes touch within 16 units. Gwinnett is a
long ruined exterior of many small separated pieces, so the rule finds 205
things where the artist meant one building; Andrew Station is a dense
interlocking block, so the rule finds 7 things where a street-level eye sees
many. Neither number comes from anything that knows what a building is -- and
that is the argument for a better identity, not against identity.

**The proposal panel is nearly a no-op here and the picture says so.** Only 3 of
this chunk's 30 layers are Bld-named, so 588 identities become 586 and the two
panels are all but identical. South Boston was layered by CITY BLOCK. That is
the picture's finding: the layer route would work in the Theater district and do
nothing here, which is why it is labelled a proposal in its own title strip and
is not ruled (numbers in s1.13).

## 4. The identity route against the ray cast (deliverable B)

`mpics.py`, log `mpics.log`, table `mpics.json`. The runtime far shadow map
keyed on the `.lodi` group identity, **64 u a texel**, terrain carried in the
SAME map and judged by depth alone. Six cells: the east and street cameras at
120/5, 120/15 and 240/15.

`images/row_M1_east_az120_el05.png`, `row_M1_street_az120_el05.png`,
`row_M1_east_az120_el15.png`, `row_M1_street_az120_el15.png`,
`row_M1_east_az240_el15.png`, `row_M1_street_az240_el15.png`.
LEFT is the ray-cast truth, RIGHT is the map; nothing else differs.

| cell | ALL | terrain | objects | **objects N.L>0** | of px | self-shadow refused | no data |
|---|---:|---:|---:|---:|---:|---:|---:|
| east 120/5 | 12.48% | 10.21% | 13.20% | **13.06%** | 605,890 | 8.93% | 8.42% |
| street 120/5 | 12.82% | 13.54% | 11.99% | **11.45%** | 452,455 | 1.08% | 0.36% |
| east 120/15 | 10.48% | 8.96% | 10.95% | **11.08%** | 611,075 | 20.74% | 9.05% |
| street 120/15 | 6.82% | 5.89% | 7.88% | **6.53%** | 453,433 | 5.88% | 0.25% |
| east 240/15 | 16.42% | 7.64% | 19.13% | **7.64%** | 206,868 | 28.17% | 8.82% |
| street 240/15 | 27.20% | 18.17% | 37.52% | **3.47%** | 70,507 | 30.82% | 0.22% |
| **mean** | **14.37%** | **10.74%** | **16.78%** | **8.87%** | | **15.94%** | |

Against the same six cells the bake that ships today (row O1/T1) is 39.99% on
the 16-cell mean with a 54.53% objects N.L>0 column (s1.7), so this is not a
close comparison.

**The two numbers bungo asked for by name.**

**(1) The share of truth-dark object pixels lost to self-shadow exclusion.** Of
the object pixels the ray cast calls dark, the identity rule hands this share
back to the light because the caster carried the receiver's own id:

| cell | truth-dark object px | refused | share |
|---|---:|---:|---:|
| east 120/5 | 245,042 | 21,893 | 8.93% |
| street 120/5 | 226,149 | 2,451 | **1.08%** |
| east 120/15 | 104,046 | 21,582 | 20.74% |
| street 120/15 | 66,103 | 3,884 | 5.88% |
| east 240/15 | 283,084 | 79,731 | 28.17% |
| street 240/15 | 435,377 | 134,167 | **30.82%** |

The 30.82% at street 240/15 is the whole cost of the identity rule in one
number, and the row beside it is why the cost is survivable: that same cell is
**3.47% wrong on the N.L > 0 pixels**, because self-shadow at a western sun
falls overwhelmingly on faces the sun is already behind, where N.L blacks the
pixel anyway. **The identity rule's damage is concentrated exactly where the eye
cannot see it.** That is an argument for the route, and it is also the thing to
re-test first if the shipped shader ever stops multiplying N.L in.

**(2) The count of groups that are not one thing.** From s1.4 and s1.5, on this
chunk: **588 groups over 2,449 placements, 468 of them singletons, the largest
holding 205 placements.** Of the 120 groups that hold more than one placement,
**50 are several physically separate solids** at weld tolerance 0. Welding in
world space at 8 units (row M4) turns the 588 groups into **1,508 solids**:
**234 groups hold more than one solid** (the worst holds 68), and **112 solids
span more than one group** (the worst spans 47). So the group is not one thing
in both directions at once, which is what the Gwinnett and Andrew Station
close-ups show with no numbers at all.

**And the ceiling on repairing it.** Row M4 -- the identity replaced by a
perfect one-welded-solid identity -- scores 10.44% mean ALL against M1's 11.41%.
**A perfect identity recovers 1.4 of the 4.1 points the identity rule costs;
the other two thirds is genuine self-shadowing that no identity scheme can hand
back.** Repairing the identity is worth doing and it is not where the picture
is won.

### What would have to be true for section 4 to be wrong

The map is rebuilt when the sun moves, over the LOD meshes of every loaded
chunk, and **this lane has not costed that draw** -- every number above is a
simulator's, taken on one chunk, with the truth cast's standing limits (s0)
attached. The "no data" column is receivers outside the map's (v, s) box and
runs to 9.05% on the east camera, which overshoots the chunk; those pixels are
excluded from every percentage rather than counted as agreement. And the texel
size barely matters (64 u 11.41% -> 16 u 10.92% on the 16-cell mean), so a
future disagreement about resolution is not a disagreement about this table.

## 5. src and tests

**src untouched by this lane.** `git status --porcelain src/ tests/` reports 216
modified files; all of them are pre-existing or lane HORIZON3's uncommitted
work, none is mine. I opened `src/lodghorizon.h` and `src/nativeemit.cpp` to
read the three-way normal rule and the octahedral normal unpack, and wrote
nothing. `tests/spells/lodgen_sunsim.sh` does not exist and was not created --
the gate was dropped by the redirect. No build was attempted: Fallout 4 is up
(pid 17248). Everything in this lane is Python under
`scratchpad/horizon4_20260919/`.
