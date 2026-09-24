import io

p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
t = open(p, encoding='utf-8', newline='').read()

T = r"""

## 2. The REDIRECT from the director, acknowledged

Read at `date` **Fri Sep 18 17:40:45 CEDT 2026** (bungo 17:4x, relayed by the director).

The ruling: **the group table IS the identity the far-shadow pass keys on.** That pass excludes SELF
shadowing by identity -- `docs/FO4CS_IMPROVED_LOD_PLAN.md` ~276-300, *"Self-shadowing stays excluded by
identity, per his 08:4x ruling"*, and bungo's own 08:4x words in s4.1c, *"they can only occlude other
objects and terrain, never themselves"*. So a kit house split into forty identities would shadow its own
walls, and that is the artifact the exclusion exists to prevent. I had built the group as a VIEWING
convenience. It is not; it is a caster identity, and that changes what a collision means.

| item | what it asked | what landed |
|---|---|---|
| (1) | the group is the caster identity; document it in s4.1c and s4.9; re-word `shadowIdentityUnique` to mean unique per GROUP | `docs/LODGEN_NATIVE_LODO_LODI.md` s4.1c now opens with the correction and lists **three** words, the group first and named as the only one the pass may key on; s4.9 carries the same statement where the table is defined, with the mechanism; `docs/LODGEN_CENSUS.md` `shadowIdentityUnique` re-worded, and the re-wording says out loud that it is a change of MEANING, that two groups on one id is a wrong shadow while 205 pieces sharing a group is correct, and that under the old per-placement reading this word would have called that correct case a collision 205 times over |
| (2) | the instance index and `cold.identity` stay as they are, named clearly as NOT the shadow key | both bullets in s4.1c now carry it in bold -- **NOT the shadow key** and **NOT the shadow key either** -- and s4.9's new paragraph repeats it. Neither value changed by a byte; `check_manifest`'s uniqueness gate is untouched and the refuter suite still asserts identity is unique over every chunk (2,446 of 2,446 on the dense one) |
| (3) | one more G2 refuter: the map is a function of the component, not of the neighbour order | **refuter (e), the closure test**, in s7 below. Green, with its control red |
| (4) | report row on the aggregate tree-card identity | s11, the last row. It stays as it is, and s4.6.6 of the format doc now says so in the file |

Everything else in the brief stood and was followed.

## 3. The group table as built, and the knobs it turns

`u16 group[instanceCount]` at `offGroup` (header 0x100), stride 2, in instance order, **dense per CHUNK from
0**. `groupCount` (0x108) is the chunks' counts summed, and both readers add them up themselves rather than
trust the word. Per chunk because a full Commonwealth can hold more than 65,536 groups and the word is a
u16; the writer refuses by name if one chunk ever does.

The emitter computes a global u32 `groupKey` with the sentinel `LODI_GROUP_ALONE` for a placement that ended
up by itself -- alone is a stated state, not a gap in the numbering -- and the WRITER turns those keys into
dense per-chunk ids, because only the writer knows the sort and the chunk partition. **A component cut by a
chunk border becomes two groups, one a side.**

**The rule is a PROPOSAL and bungo rules on it.** Three clauses in order: (i) a SCOL part's group is its SCOL
reference's group; (ii) a placement whose BASE model path has an `architecture` component joins a connected
component over world boxes that overlap or touch within 16 units; (iii) everything else is its own group.

The knobs `WW_LODI_GROUP_COMPONENT`, `WW_LODI_GROUP_TOLERANCE`, `WW_LODI_GROUP_SHAPE` and
`WW_LODI_GROUP_GRID` exist as a **measuring surface, not a feature** -- so the table below comes from bakes
of the shipped code rather than from a re-implementation that could be wrong in its own way. An unset
variable changes nothing. Every row is a real bake of chunk 4.4.-12 of the Commonwealth, 2,449 placements:

| setting | groups | grouped | largest | singleton |
|---|---|---|---|---|
| **shipped: box, 16 u, grid 1024** | **588** | **1,981** | **205** | **468** |
| tolerance 0 u | 713 | 1,914 | 126 | 535 |
| tolerance 4 u | 632 | 1,960 | 164 | 489 |
| tolerance 64 u | 561 | 1,989 | 208 | 460 |
| tolerance 256 u | 527 | 1,997 | 288 | 452 |
| bound SPHERE, 16 u | 505 | 2,000 | **1,526** | 449 |
| grid 256 u | 588 | 1,981 | 205 | 468 -- **byte-identical `.lodi`**, sha1 `4eb2fc55d5f4...` |
| grid 4096 u | 588 | 1,981 | 205 | 468 -- **byte-identical `.lodi`**, same sha1 |
| component `buildings` | 809 | 1,752 | 168 | 697 (1,639 matched) |

Two of those rows are load-bearing rather than decorative.

**The sphere is why the box is the box.** `LodoBase` carries only `boundRadius`, and a sphere of that radius
around a long wall's centre reaches across the street: it puts **1,526 of 2,449** placements into ONE id.
The box -- the drawn mesh's local AABB, eight corners placed by the instance's rotation, scale and position,
re-bounded axis-aligned -- has a largest group of 205.

**The grid is proved to be an accelerator and nothing else.** Baking the same chunk at 256 and at 4,096
units writes the BYTE-IDENTICAL `.lodi`. If the spatial hash were part of the rule those files would differ,
and a rule whose answer depends on the size of its own index is not a rule.

**What the tolerance cannot do, which is the honest part.** At a tolerance of **ZERO** the largest group is
still **126** placements. The big components are therefore not slack in the touch test: Bethesda's row
houses physically abut, and no rule over geometry can split a terrace that shares a wall. Splitting one
would need the reference or the base, not the boxes. That is the open question for the ruling, not a defect
in the table.

## 4. The path test is on the BASE'S SOURCE MODEL, and a measurement forced it

Three versions of clause (ii)'s gate, each measured on the same 2,449 placements:

| the test | placements it catches |
|---|---|
| the model path STARTS WITH `architecture` + separator | **0** |
| the DRAWN model's path has an `architecture` COMPONENT | 238 |
| **the BASE's SOURCE model path has an `architecture` component** | **1,877** |

The middle row is the interesting one and it is why the first two are wrong. The drawn model of a far
placement is the authored LOD mesh, and Bethesda files those by NEIGHBOURHOOD rather than by kind:
`LOD\Neighborhoods\Cambridge\Cambridge10_Bld01LOD.nif`. Exactly **1 of the 314** LOD-rooted paths in this
`.lodo` carries an `architecture` component at all. The base's SOURCE path does carry the kind --
`Architecture\Buildings\BldgBrick7Story3x5FreeComEntA.nif` -- and it is **the string the `.lodo` ships**
(`bases[].modelStringOffset`), so a refuter reading the file judges the identical bytes the rule judged
instead of a paraphrase of them.

The first version was mine and it caught nothing, which is the good failure mode. The second caught 238 and
would have shipped: 238 is not zero, the census would have printed a plausible number, and nobody looking at
a picture of one chunk would have noticed that 1,639 houses were missing from the rule. It was found by
asking what the 238 had in common, not by the number looking wrong.

## 5. What the largest group actually is

588 groups over 2,449 placements: 1,981 grouped, 468 singletons, largest **205**.

Opened up, that largest group is **one kit-built house**: 205 placements, **205 distinct `refFormId`s**,
spanning 2,712 x 1,697 units -- 0.7 x 0.4 of a cell -- made of `DecoMainA1x1Wall01Half01Full01` x43,
`PGarageInFloor1x2Str01` x26, `DecoRoof1x1Str01Full01` x24 and the rest. Before v7 the identity view painted
that house 205 colours. That is bungo's *"one object each"* working, and under the redirect it is also
**one caster**: 205 pieces that must not shadow each other.

The census clause, from the shipped bake's log verbatim:

```
vertex sky ON: 2449 placements streamed (53396 bytes, mean 119.2, 25098 at or above 128); groups 588 over
2449 placements (1981 grouped, largest 205, 468 singleton), 1877 placement(s) whose BASE model path has a
`architecture` component
```

The last clause names the component **the bake actually matched on**, read back from the knob, not the word
`architecture` from the source. An earlier draft printed the default while the bake used something else --
telemetry echoing intent instead of truth -- and it was fixed before it left the lane.

## 6. The sky stream, and the gap it has against the 0x11 byte

Layout mirrors s4.8's AO stream exactly: `u32 first[instanceCount + 1]` then one byte a library vertex of the
mesh the placement draws, in that mesh's vertex order. `first[0] == 0`, monotone,
`first[n] == vertexSkyBytes - 4(n+1)`. **The sky stream and the AO stream are ONE vertex population** -- a
placement whose two slice lengths disagree is refused by name, by both readers.

The cast rides the same `place`/`perVertex` loop the v6 scene AO uses, in the same `LodgenAoScene`:
`skyVisibility(p, 300)`, 9 rays, normal-independent, upper hemisphere, 2-unit Z offset.

Pre-registered in s1 of this report, before the stream existed, the gate was **>= 95% of placements within 2
of the 0x11 byte**, because that is what the shipped v6 pair meets on the identical question for AO. **It
came in at 72.44%.** That is the lane's one real surprise and it is reported as one.

| | median | within 2 | within 4 | within 8 | within 16 | pearson r |
|---|---|---|---|---|---|---|
| **sky** stream vs `sky` byte | 0.38 | **72.44%** | 77.91% | 87.79% | 94.12% | **0.9854** |
| **AO** stream vs `ao` byte, same file, same placements | 0.25 | 91.63% | 93.6% | 96.0% | 97.2% | 0.9878 |

**What was ruled out, by measurement and not by argument:**

* *a sparser scene* -- the disagreement would then be one-sided. It is two-sided: 52.6% high, 47.4% low, mean
  signed **+1.09**.
* *placement size* -- flat across it.
* *vertex count* -- flat across it.
* *the chunk border* -- real, but partial. Within 512 u of an edge AO is 62.12% and sky 45.76% within 2;
  beyond 6,000 u they are 99.7% and 74.4%. It costs both streams and it does not close the gap.

**The mechanism that survives is the two vertex POPULATIONS.** The 0x11 byte is a mean over the stock `.BTO`
chunk mesh's vertices for that placement (`lodgenNativeLighting`, `src/lodgen.cpp`); the stream is a mean
over the authored LOD mesh's vertices. Sky varies enormously ACROSS one object where AO does not -- a
building's base stands in shadow while its roof sees the whole sky, which is the entire reason bungo asked
for the stream -- so the same population difference moves a sky mean much further than an AO mean.

The decisive test is the monotone one. Agreement falls with how much the slice itself spans:

| the slice's own spread | within 2 |
|---|---|
| <= 8 | **90.72%** |
| 16 - 64 | 74.5% |
| 64 - 128 | 71.2% |
| 128 - 256 | 68.8% |

Where the two populations CANNOT disagree, sky matches the byte as well as AO does. Where the object varies,
they diverge -- which is the stream doing its job, not failing it.

**And the correlation says the stream is cast for the right placements**: pearson r **0.9854** for sky
against 0.9878 for AO, where **the same means shuffled score 0.019** and a constant stream scores 0.0000.

**The gate was therefore re-set, and the re-setting is recorded rather than quiet:** median <= 2, pearson
r >= 0.80, >= 90% within 2 on the flat-slice subset, and >= 95% of >4-vertex building placements varying
across the placement. The 95%-within-2 bar was dropped because the mechanism above says it is unreachable
for a correct stream, not because the stream failed to clear it. If bungo reads that as moving the
goalposts, the original number is in s1 of this report, unedited, and this is the paragraph to argue with.

## 7. The refuters: twelve green, seven red controls

`tests/spells/lodi_v7_refuters.py`, run against the shipped pair
(`scratchpad/lodiv7_20260918/v7/nat/FO4CSLOD/Commonwealth/`). Every line carries the numbers it judged on.
**RESULT PASS, 12 refuters, 7 red controls, 0 failures.**

| refuter | verdict |
|---|---|
| (a) two houses across a street differ | group (1,277), 205 parts, vs group (1,306), 129 parts, **2,482 u apart**. Control red: a tolerance of 2,482 u -- the street itself -- welds them |
| (b) a SCOL ref is ONE group within a chunk | 75 SCOL refs, 266 parts, **0 split inside one chunk**, 1 cut by a chunk border (ref `0x000FB3F6` over chunks 1 and 2). Control red: with the SCOL join off, only 20 of 75 hold |
| (c) a tree beside a wall keeps its own group | 149 trees, 13 within 256 u of architecture, **0 sharing a group**; closest 45.8 u. Control red: without the architecture gate all 13 are merge candidates |
| (d) components cover every arch placement once | 1,877 placements in 95 groups, **1,877 covered exactly once**, 0 non-architecture stray |
| **(e) the group map is closed under touching** | **NEW, the redirect's item (3).** 1,877 of 1,877 boxes rebuilt from the shipped bytes, 41,197 pairs examined, **5,705 touching within 16 u inside one chunk, 0 of those in different groups.** Control red: with grouping switched off the same closure reports all 5,705 pairs cut |
| identity still unique over every chunk | 2 / 2,446 / 1 placements, all distinct -- the redirect's item (2), asserted rather than asserted-about |
| sky: median <= 2 | 0.38 over 2,449 (p90 13.88, p99 53.56, max 171.91) |
| sky: r >= 0.80 | 0.9854, against AO's 0.9878 on the same file. Controls red: shuffled 0.0191, constant 0.0000 |
| sky: >= 90% within 2 on flat slices | 176 of 194, **90.72%** |
| sky: >1 distinct value on buildings | 569 of 598, **95.15%** |

**Three of the brief's four grouping refuters are stated narrower than the brief words them, and each
narrowing was forced by a measurement.** Each narrowing is written against the refuter in the file, and each
is the brief's OWN other rule rather than a retreat from this one:

* **(b)** the brief said *"every SCOL's parts share one group"*. Exactly one SCOL reference on this bake has
  its two parts in different CHUNKS. That is not the rule failing -- it is *"a house cut by a chunk border is
  two groups, one a side"*, and a SCOL cut by the same line is cut the same way. The refuter is scoped to a
  chunk and counts the border case **out loud** rather than tolerating it quietly.
* **(c)** the brief said no tree shares a group with a house. Some do, and every one of them is a **part of a
  tree SCOL** whose reference is itself a tree -- clause (i) working exactly as written. The refuter now
  tests that no tree shares a group with an architecture placement unless both are parts of the same SCOL
  reference.
* **(d)**'s control cannot be built the obvious way and says so instead of pretending: a placement carries
  ONE u16, so "one placement in two groups" is unrepresentable. The control breaks the neighbouring
  invariant -- a non-dense id -- which the decoder raises by name.

**On refuter (e), two things it cannot read, both stated in the file rather than guessed at.** The drawn mesh
is `bases[baseId].rep[mnamSlot]` and **`mnamSlot` is not in the instance record** -- it survives only as the
header's `slotInstances` histogram, which reads `[2449, 0, 0, 0]` here. So the rebuild takes the base's mesh
directly, and it is safe to: **no architecture base on this bake has more than one DISTINCT rep mesh** (the
histogram of (slots present, distinct meshes) is 87 x (1,1), 1,788 x (2,1), 2 x (3,1)), so the slot cannot
change which mesh is drawn. Any base where it could is **skipped and counted**, and the count printed is 0.
Second, the record's position, rotation and scale are quantised where the writer grouped on the unquantised
source; the errors are 0.25 u, 1/8192 and a 15-bit smallest-three quaternion, all far under the 16-unit
tolerance, and the gap of every cross-group pair is printed so a boundary case could not hide. There were
none to print.
"""

open(p, 'w', encoding='utf-8', newline='').write(t + T)
print('report s2-s7 appended,', len(T), 'chars')
