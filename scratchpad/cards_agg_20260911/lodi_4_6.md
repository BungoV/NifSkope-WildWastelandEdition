### 4.6 The aggregate ring-3 impostors (v4)

bungo, 2026-09-11 08:2x → 08:3x, verbatim: *"At ring 3 the bake takes each
cell's trees, places their cards with the same rotation and mirror the
repetition breaking would give them, photographs the whole cluster from the
horizon views, and writes one aggregate sheet per cell. The ring 3 instance list
then holds one placement per cell instead of one per tree."* — *"1 sounds
good"*.

#### 4.6.1 The version is CONDITIONAL, and that is a deviation stated out loud

A bake with **no** aggregate writes **version 3**, byte for byte what this
writer wrote before the module existed. A bake **with** aggregates writes
**version 4**. The reader accepts 3 and 4 and refuses 1 and 2 by name.

That is not the licence v3 refused v2 under, and the difference is the point: a
v3 file read by a v4 reader is UNAMBIGUOUS — zero aggregates, both new offsets
0, and the header bytes from 0xB0 are the zero pad the v3 writer already wrote.
v2 read as v3 was not: it said "this worldspace occludes nothing" and meant
"this file predates occluders". The condition exists because aggregation is a
MODULE and CONSTITUTION 10 requires its off value to be the exact way back;
making the version unconditional would have made `--aggregate` off a different
file from the bake before it, and there would then be nothing to measure byte
identity against.

**Deviation 6** in §11 records it.

#### 4.6.2 The header words (v4), at the room §12 named

| off | type | field |
|---|---|---|
| **0xB0** | **u64** | **offset: aggregate table (48 B stride)** |
| **0xB8** | **u64** | **offset: covered-instance blob (4 B per entry)** |
| **0xC0** | **u32** | **`aggregateCount`** — 0 is a refusal on a v4 file: a file with no aggregate is a v3 file |
| **0xC4** | **u32** | **`coveredCount`** — the blob's length in u32s |
| **0xC8** | **u16** | **`aggregateStride` = 48**; any other value is refused by name |
| **0xCA** | **u16** | **`aggregateViews`** — azimuths a sheet, a FILE constant; a row that disagrees is refused |
| **0xCC** | **f32** | **`aggSwitchPx`** — the cross-fade threshold, §4.6.5 |
| **0xD0** | **f32** | **`aggBandRatio`** — the band's width as a multiple of the threshold; 1.0 or below is refused |
| 0xD4…0xFF | — | reserved, zero **on a v4 file**. On a v3 file the pad still starts at 0xB0, and the same sweep is what refuses a v3 file carrying an aggregate table |

`indexCrc32` covers, in file order: the chunk table, the cell ranges, the
occluder table, the occluder ranges, **the aggregate table and the covered
blob**. An empty pair folds zero bytes in, which is why a v3 file's CRC is
exactly where it was.

The two payloads are written LAST and only when there is something to write, so
a v3 file's byte layout is untouched.

#### 4.6.3 The aggregate row — 48 B

| off | type | field |
|---|---|---|
| 0x00 | f32[3] | `centre` — the card's centre in WORLD units: the cell's centre in X and Y, the cluster's mid-height in Z |
| 0x0C | f32[2] | `half` — the quad's half extents along the view's own right and up, world units. ONE pair for every view |
| 0x14 | f32 | `depthSpan` — `units = (B − 0.5) × depthSpan`, the card law; **not positive is a refusal** |
| 0x18 | f32 | `boundRadius` — the tree cloud's radius, for the projected-size test; **not positive is a refusal** |
| 0x1C | i16[2] | `cellX`, `cellY` |
| 0x20 | u16 | `views` — must equal the header's `aggregateViews` |
| 0x22 | u16 | `flags` — bit0 `HEIGHT` (**clear is a refusal**), bit1 `MIRRORED` (at least one source tree was composited mirrored); bits 2–15 reserved, and a set reserved bit is a refusal |
| 0x24 | u32 | `identity` — **`0x80000000 \| aggregateIndex`, always**; any other value is refused by name |
| 0x28 | u32 | `coveredFirst` — into the covered blob |
| 0x2C | u32 | `coveredCount` — how many instances this aggregate stands for; **0 is a refusal** |

**There is no `.lodm` path in the row, deliberately.** The sheets live at
`Data\Textures\Lodgen\Aggregate\<EDID>\<cellX>_<cellY>_agg.*`, derived from the
header's worldspace and this row's own cell, so a reader cannot be handed a path
that disagrees with the cell (zero-authoring).

Rows are in **north-up cell order**, the same order as everything else in the
file, and a repeated cell is refused. Two writes of one set are byte-identical.

#### 4.6.4 The covered blob, and why the instances are NOT removed

bungo's words are *"the ring 3 instance list then holds one placement per cell
instead of one per tree"*. **This file has no ring-3 instance list.** It has ONE
instance table and a 4-cell chunk directory, and selection is by projected size
(§4.4) — that is the whole premise of v3. So the aggregate cannot remove
anything; it SUPPRESSES, and the covered blob is what says which.

Per aggregate, `coveredCount` u32s at `coveredFirst`, **ascending** so a
consumer can bisect, each an index into the instance blob. The rules, all
enforced by the reader:

* the aggregates **partition** the blob in order — `coveredFirst` must be the
  running sum;
* no instance is covered **twice** — a doubly covered instance would be
  suppressed twice and the count identity would be measuring a number nothing
  else agrees with;
* **every covered instance stands in the aggregate's OWN cell.** This is the
  rule with teeth: a covered instance somewhere else is a forest being hidden by
  a card that does not draw it.

A runtime reads the blob once at load and marks those instances "aggregated";
past the switch distance it draws the cell's card instead of them, and across
the band it draws both and dithers between them.

**The count identity.** `coveredCount` for a cell equals the `trees` key of that
cell's `.lodm` (§3a of `docs/LODGEN_LODM_FORMAT.md`) equals the number the bake
photographed. Three statements of one number in two files and a log line, so any
one of them can check the others; the bake prints
`count identity photographed N == file covered M == AGREE|DISAGREE` and the gate
reads it back from the bytes.

#### 4.6.5 The cross-fade band, as a projected size and not a distance

No bake can state a ring-3 distance, for the same reason §4.4 gives: a ring is
camera-relative and this file has none. The band is therefore stated the way
bungo's screen-size spec of 10:4x states everything else — as a **projected
size with a hysteresis**:

```
the aggregate is selected when the CELL's projected width falls to
aggSwitchPx, and the per-tree cards cross-fade out over
aggSwitchPx … aggBandRatio × aggSwitchPx.
```

Defaults **96 px** (three quarters of a 128-px frame, the point past which a
sheet can no longer add detail) and **1.2** (his ~20 percent hysteresis).

At the reference projection of §4.4 (`projectionScale` 1371.0) a 4,096-unit cell
is 96 px wide at **58,500 units** and 115 px wide at **48,800 units**, so the
band is about **48,800 … 58,500 units, 2.4 cells wide**, and the aggregate takes
over about **14 cells out**. Those three numbers are the reference READING of
the rule, not the rule: a consumer recomputes them from its live projection.

#### 4.6.6 The identity law (bungo's 08:4x far-shadow ruling)

**One identity per aggregate**, never the dominant tree's, and the space is
disjoint from the instance indices by construction (`0x80000000 | index`).

The identity index is what the far-shadow pass keys on so a caster never shadows
itself. Once a cell's trees are one card they ARE one caster; giving the
aggregate its dominant tree's identity would make it share an identity with that
same tree's own per-tree instances, which are still drawn at the nearer rings,
and the shadow pass would exclude the wrong pixels. The top bit says which space
a consumer is holding, so an aggregate identity and an instance index can never
be confused.

#### 4.6.7 Measured, on the 9-chunk Sanctuary region

| reading | number |
|---|---|
| cells holding at least one tree | 105 |
| cells forested at the default threshold of 8 | 97 |
| aggregates written | **97** |
| trees photographed into them | **3,414** |
| trees refused, their base having no card set in the bake tree | 9 |
| trees refused, their card baked through a perspective camera | 0 |
| aggregate rows in the `.lodi` | 97 |
| covered-instance entries | 3,414 |
| `.lodi` version written | **4** |

The ESM census taken before any of this was built (`scratchpad/cards_agg_20260911`)
reads **97 forested cells holding 3,423 trees** on the same region from the
plugin alone: 3,414 photographed + 9 refused = 3,423 exactly, from two
instruments that share no code.

