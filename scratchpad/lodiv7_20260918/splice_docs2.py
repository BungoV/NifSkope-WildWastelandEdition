p = 'docs/LODGEN_NATIVE_LODO_LODI.md'
s = open(p, encoding='utf-8', newline='').read()
BS = chr(92)


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:90])
    s = s.replace(o, n)


NEW = """
### 4.9 The group table (v7)

bungo, 2026-09-18 09:4x: *"The houses should be one object each though, for
identity"*. `identity` (§4.1c) is unique per placement, which is what a picker
needs and the opposite of what an eye needs: a kit-built house is 205 separate
wall, roof and floor placements and the identity view paints it 205 colours.
v7 carries a SECOND word beside it.

**`identity` is not touched.** It stays unique, `check_manifest`'s uniqueness
gate passes unchanged, and the group is a parallel table. The viewer's
`identity` channel now DRAWS the group and the new name `placement` draws what
`identity` drew before (§8).

**Layout.** `offGroup` (0x100) points at `u16 group[instanceCount]`, in instance
(sorted table) order — one word a placement, stride 2 (`groupStride`, 0x10C).
Ids are **dense per CHUNK from 0**: a chunk holding *C* groups uses exactly
{0 … C−1}, and a reader that finds a hole, or an id at or past the chunk's own
placement count, refuses by name. `groupCount` (0x108) is the chunks' counts
SUMMED, and the reader adds them up itself rather than trusting the word. Per
chunk and not globally, because a full Commonwealth can hold more than 65,536
groups and the word is a u16; the writer refuses by name if one chunk ever does.

**Who assigns what.** The emitter (`src/nativeemit.cpp`) computes a global u32
`groupKey` per placement, with the sentinel `LODI_GROUP_ALONE` for a placement
that ended up by itself — "alone" is a stated state, not a coincidence of
numbering. The WRITER turns those keys into dense per-chunk ids, because only
the writer knows the sort and the chunk partition. **A component cut by a chunk
border becomes two groups, one a side**, which is the same rule the rest of the
format lives under.

**The rule, which is a PROPOSAL.** Three clauses, in order:

1. a SCOL part's group is its SCOL reference's group;
2. a placement whose BASE model path has an `architecture` component joins a
   connected component over world boxes that overlap or touch within 16 units;
3. everything else is its own group.

The box is the DRAWN mesh's local AABB (`LodoMesh.aabbMin`/`aabbExtent`), its
eight corners placed by the instance's rotation, scale and position and then
re-bounded axis-aligned in world. Not the base's bound SPHERE: `LodoBase`
carries only `boundRadius`, and a sphere of that radius around a long wall's
centre reaches across the street. **Measured on chunk 4.4.-12: the sphere puts
1,526 of 2,449 placements into ONE id; the box's largest group is 205.** The
pair search runs over a 1024-unit spatial hash, which is an accelerator only —
baking the same chunk at 256 and at 4096 units produces the BYTE-IDENTICAL
`.lodi`, and that is a gate (`tests/spells/lodi_v7.sh`).

**THE PATH TEST IS ON THE BASE'S SOURCE MODEL, and that was forced by
measurement.** Asking whether the model path *starts with* `architecture{BS}`
catches **0 of 2,449** placements, and asking whether the DRAWN model's path has
an `architecture` component catches only **238** — because the drawn model of a
far placement is the authored LOD and Bethesda files those by NEIGHBOURHOOD, not
by kind (`LOD{BS}Neighborhoods{BS}Cambridge{BS}Cambridge10_Bld01LOD.nif`). Exactly 1 of
the 314 LOD-rooted paths in this `.lodo` carries an `architecture` component.
The base's SOURCE path does carry the kind
(`Architecture{BS}Buildings{BS}BldgBrick7Story3x5FreeComEntA.nif`) and catches
**1,877 of 2,449** — and it is the string the `.lodo` SHIPS
(`bases[].modelStringOffset`), so a refuter reading the file judges the identical
bytes the rule judged rather than a paraphrase of them.

**The knobs** are in one `GroupKnobs` struct above the emit function, and each
is readable from the environment — `WW_LODI_GROUP_COMPONENT`,
`WW_LODI_GROUP_TOLERANCE`, `WW_LODI_GROUP_SHAPE` (`box`/`sphere`),
`WW_LODI_GROUP_GRID` — as a MEASURING surface, so the table below comes from
bakes of the shipped code rather than from a re-implementation that could be
wrong in its own way. An unset variable changes nothing.

| setting | groups | grouped | largest | singleton |
|---|---|---|---|---|
| **shipped: box, 16 u, grid 1024** | **588** | **1,981** | **205** | **468** |
| tolerance 0 u | 713 | 1,914 | 126 | 535 |
| tolerance 4 u | 632 | 1,960 | 164 | 489 |
| tolerance 64 u | 561 | 1,989 | 208 | 460 |
| tolerance 256 u | 527 | 1,997 | 288 | 452 |
| bound SPHERE, 16 u | 505 | 2,000 | **1,526** | 449 |
| grid 256 u | 588 | 1,981 | 205 | 468 (byte-identical file) |
| grid 4096 u | 588 | 1,981 | 205 | 468 (byte-identical file) |
| component `buildings` | 809 | 1,752 | 168 | 697 |

**What the tolerance cannot do.** At a tolerance of ZERO the largest group is
still 126 placements, so the big components are not an artefact of the 16-unit
slack: Bethesda's row houses physically abut, and a rule over geometry cannot
split a terrace that shares a wall. Splitting one would need the reference or
the base, not the boxes. That is the open question for the ruling, not a defect
in the table.

**Census.** `groups`, `groupedPlacements`, `largestGroup`, `singletonGroups`.
Chunk 4.4.-12: 588 groups over 2,449 placements, 1,981 grouped, largest 205
(a single kit-built house of 205 distinct refs — `DecoMainA1x1Wall01` ×43,
`DecoRoof1x1Str01` ×24, garage floors — spanning 0.7 × 0.4 of a cell), 468
singletons, 1,877 architecture placements.

**Way back.** `--lodi-v6` writes a v6 file with no group table, byte-identical
to what the writer wrote before v7 existed.

### 4.10 The per-vertex sky stream (v7)

bungo, 2026-09-18 09:4x: *"We need per vertex sky visbility too"*. §4.1's `sky`
is ONE byte a placement; a building's base stands in shadow while its roof sees
the whole sky, and one byte cannot say both.

**Layout mirrors §4.8 exactly.** `offVertexSky` (0x110) points at
`u32 first[instanceCount + 1]` in instance order, then one byte a library vertex
of the mesh the placement draws, in that mesh's vertex order;
`vertexSkyBytes` (0x118) is the whole stream, offsets included. `first[0] == 0`,
monotone, `first[n] == vertexSkyBytes − 4 (n + 1)`. **The sky stream and the AO
stream are ONE vertex population**: a placement whose two slice lengths disagree
is refused by name, by both readers.

**The cast** rides the same `place`/`perVertex` loop the v6 scene AO uses, in the
same `LodgenAoScene`: `skyVisibility(p, 300)` — 9 rays, normal-independent,
upper hemisphere, 2-unit Z offset. Same scene, same reach, same parallelism.

**How it compares with the 0x11 byte, and the honest half.** On chunk 4.4.-12,
2,449 placements:

| | median | within 2 | within 4 | within 8 | within 16 | pearson r |
|---|---|---|---|---|---|---|
| **sky** stream vs `sky` byte | 0.38 | 72.4% | 77.9% | 87.8% | 94.1% | **0.9854** |
| **AO** stream vs `ao` byte (same file, same placements) | 0.25 | 91.6% | 93.6% | 96.0% | 97.2% | 0.9878 |

The two streams track their bytes equally well — r = 0.985 against 0.988, where
the same means SHUFFLED score 0.019 — but only 72% of sky's per-placement means
land within 2 where AO manages 92%. **The reason is the two vertex populations,
and it was measured rather than assumed.** The byte is a mean over the stock
`.BTO` chunk mesh's vertices for that placement (`lodgenNativeLighting`,
`src/lodgen.cpp`); the stream is a mean over the authored LOD mesh's vertices.
Sky varies enormously ACROSS one object where AO does not, so the same
population difference moves a sky mean much further: agreement falls monotonically
with how much the slice itself spans — 90.7% within 2 where the slice spans ≤ 8,
74.5% at 16…64, 68.8% at 128…256. Two checks rule the alternatives out: the
disagreement is two-sided (53% high, 47% low, mean signed +1.09), so it is not a
sparser scene; and it is flat in placement size and vertex count. The chunk
border costs both streams on top of that (AO 62%, sky 46% within 2 inside 512
units of an edge, against 99.7% and 74.4% beyond 6,000) for the reason §4.8's
scene note already gives.

**The gate is therefore set on the median, the correlation and the flat-slice
subset** — where the two populations CANNOT disagree — and not on a 95% bar the
mechanism says is unreachable. `tests/spells/lodi_v7.sh` G3.

**Census.** `vertexSkyBytes`, `vertexSkyPlacements`, and the mean. Chunk
4.4.-12: 2,449 placements streamed, 53,396 bytes, mean 119.2, 25,098 vertices at
or above 128.

**Way back.** `--lodi-v6` writes a v6 file with no sky stream.

""".replace('{BS}', BS)

rep("\n## 5. Refusal policy — hard for the generator, soft for the consumer",
    NEW + "## 5. Refusal policy — hard for the generator, soft for the consumer")

rep("| **hard** | magic, **version (1 and 2 are refused by name)**, `vertexStride`, `instanceStride`,",
    "| **hard** | magic, **version (1 and 2 are refused by name)**, `vertexStride`, `instanceStride`, **`groupStride` (v7), a group id that is not dense per chunk, a `groupCount` that disagrees with the chunks' sum, a sky slice whose length disagrees with the same placement's AO slice, a version-3…6 file carrying version-7 header words,**")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('docs s4.9 + s4.10 + refusal table done')
