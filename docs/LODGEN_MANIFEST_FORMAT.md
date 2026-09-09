# `<chunk>.bto.manifest.txt` v2 — the object chunk manifest

**Contract version: `# lodgen manifest 2`.**
**Status: SHIPPED.** Writer `lodgenBuildObjectChunk()` in `src/lodgen.cpp`,
amended in place by the texture-array pass, the merge pass and the card-array
pass. There is no reader in this tree except those three passes; FO4CS's
*Improved LOD* module is the first outside consumer.

One UTF-8 text file beside every `.BTO`, same stem plus `.manifest.txt`. LF line
endings, one record a line, **space-separated**, the file always ends with a
newline. It carries the per-object constants the `.BTO` has nowhere to put:
which placed reference each drawn object is, what class of thing it is, how big
it is, which texture-array layer a shape is on, and which placements stand on an
impostor card.

---

## 1. Header line

Always line 1, always present when identity is on:

```
# lodgen manifest 2 ws <edid> dim <d> chunk <x> <y> columns index base type x y z scale class height ref part
```

`dim` is the ring (4, 8, 16 or 32 cells a chunk edge) and `<x> <y>` the chunk
coordinate. The `columns` list names the object row's fields **in order**, so a
consumer never guesses.

**Parse by keyword, never by field position, on this line.** The rows below are
positional; the header is not.

---

## 2. Record kinds

Every line after the header is one of five, told apart by its first token.

| first token | record | written by |
|---|---|---|
| a decimal integer | **object row** — one per placement | the chunk builder |
| `C` | **card placement** — the placement stands on an octahedral impostor | the chunk builder; extended by the card-array pass |
| `I` | **instance group** — a base repeated ≥ 8 times in this chunk | the chunk builder |
| `A` | **array layer** — which texture-array layer a shape block is on | the texture-array pass; rewritten by the merge |
| `M` | **source material** — the shape block's source material name | the chunk builder; rewritten by the merge |

Order is not contractual: `A` and `M` lines are appended after the file is first
written and are **regrouped to the end** by the merge pass. A reader keys on the
first token, not on position in the file.

---

## 3. Object row — 11 fields

```
<index> <base> <type> <x> <y> <z> <scale> <class> <height> <ref> <part>
```

| # | field | type | meaning |
|---|---|---|---|
| 0 | `index` | decimal int | the **per-chunk object index**, `0 … n−1` in placement order. This is the same number the mesh carries in vertex colour **R + G·256** |
| 1 | `base` | 8 lowercase hex digits, zero-padded | the base record's form ID |
| 2 | `type` | 4 ASCII chars | the base record type, e.g. `STAT`, `SCOL` |
| 3–5 | `x y z` | float | the placement's **world** position |
| 6 | `scale` | float | the placement's scale |
| 7 | `class` | word | `tree` \| `rock` \| `building` \| `misc` — see §3.1 |
| 8 | `height` | float | **this is a BOUND RADIUS, not a height.** The greatest distance from the placement's local origin to any vertex of any of its shapes, times `scale`, in world units. The column name is wrong and is kept only because the header line is a shipped contract — see `scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md` |
| 9 | `ref` | 8 lowercase hex digits | the **placed reference's** form ID |
| 10 | `part` | decimal int | the SCOL part's ordinal inside that reference, **−1** for a plain reference |

### 3.1 `class` is a heuristic and is labelled as one

FO4 has no class field. The writer derives it from the record type and the LOD
model path: `tree` from the same `isTree` test the sway weight and the repetition
breaker use; otherwise `rock` if the model path contains `rock`, `cliff` or
`boulder`; `building` if it contains `architecture`, `building`, `shack` or
`house`; else `misc`. Case-insensitive. A consumer that needs this to be right
should treat anything but `tree` as advisory.

The class exists because **it decides what the vertex-alpha sway channel
means**. Today only trees write a non-zero sway, so `class` is what makes the
channel interpretable in general rather than by luck.

### 3.2 The stable key is `(ref, part)`, never `ref` alone and never `index`

`index` is per chunk **and** per ring: the same object has a different index at
dim 4 and dim 8. `base` is shared by every copy of a model. The key that
survives both rings and re-bakes is the pair **`(ref, part)`**.

Measured on Sanctuary (−20,24) dim 4: **471 of the chunk's 678 objects are SCOL
parts**, so a bare `ref` key collapses most of the chunk. The dim-8 chunk that
contains it shares **406 objects by `(ref, part)`, all 406 with the same base and
position**.

A consumer pairing rings builds an index → key table per loaded chunk and matches
on the pair.

### 3.3 What the far-ring simplifier promises this row

`lodgenSimplifyFarRings` decimates rings 2 and 3 **after** the merge, grouping
triangles by (identity index, array layer) so no collapse crosses an object or a
layer; every group is asked for at least two triangles and restored whole on a
null result. Therefore **the set of identity indices in a chunk is invariant
under the pass**: a manifest row can never point at an object that is no longer
in the file. The manifest is not rewritten by that pass, because no row's
meaning changed.

---

## 4. `C` — a placement standing on a card

Written immediately after the object row of the placement it belongs to, when
that placement uses an octahedral card of grid ≥ 2.

```
C <index> <cx> <cy> <cz> <halfW> <halfH> <N> <depthSpan> <lodm>
```

| # | field | meaning |
|---|---|---|
| 1 | `index` | the object index this card replaces |
| 2–4 | `cx cy cz` | the card's centre, **model space** |
| 5–6 | `halfW halfH` | the quad's half extents in model units, gutter included |
| 7 | `N` | frames per side; the sheet holds **N² views** |
| 8 | `depthSpan` | world units the height channel spans |
| 9 | `lodm` | the card set's `.lodm` game path |

**Two more tokens are appended when the set was packed into a card array:**

```
C … <lodm> <arrayLodm> <layer>
```

`arrayLodm` is the `cardArray` `.lodm` game path and `layer` its layer index.
They go on the **end**, so a reader that stops at token 9 is unaffected. The
card-array pass only appends to a line whose token count is exactly 10, so
running it twice cannot double-append.

The crossed `_fs` quads stay in the mesh for the stock engine. A consumer that
reads `C` lines draws the sheets instead and kills those quads by index.

---

## 5. `I` — instance groups

```
I <base> <model> <count> <id0>,<id1>,…
```

`base` is 8 lowercase hex digits; `model` is the LOD model path (**it may not
contain a space** — no shipped path does, but a reader should treat token 2 as
one field and not re-split); `count` is the member count; the last token is a
**comma-separated** list of that many object indices.

Only bases repeated **≥ 8 times** in the chunk get a group. The stitched copies
stay in the mesh for vanilla; a consumer can kill those fragments by the listed
indices and draw the model instanced instead.

---

## 6. `A` — texture-array layer

```
A <shapeBlock> <layer> <lodm>
```

`shapeBlock` is the NIF **block number** of the shape inside the `.BTO`.
`layer` is the layer index in the array named by `lodm` (a `kind: "array"`
`.lodm` game path).

**`layer == −1` means the layer is PER VERTEX, in UV2.y.** That happens when the
merge pass concatenated shapes that sat on different layers. A positive layer
still means the whole shape is on that one, so a consumer can keep its fast path
and only read UV2.y on a −1 line.

`lodm` is the line's remainder, so it may contain spaces; every reader in this
tree joins tokens 3 onward.

The merge pass rewrites the whole `A`/`M` set against the surviving block
numbers, so block numbers in this file always match the `.BTO` as it sits now.

---

## 7. `M` — source material

```
M <shapeBlock> <material>
```

The **source** material name the chunk shape was built from. The chunk shape
itself names no material, exactly as vanilla's do not; this line is how the
texture-array pass finds a source `.lodm`. Like `A`, the material is the line's
remainder. A block may appear on more than one `M` line after a merge, because a
merged shape has more than one source material.

---

## 8. Invariants a reader may assume

1. Object indices are **exact, dense and unique** within a chunk: `0 … n−1`, one
   manifest row each, and no index is split across spatially separate blobs.
   Verified on Sanctuary (−20,24) dim 4: 678 distinct indices, 678 rows,
   range 0–677, none missing either way.
2. `index = R + G·256` decoded from the mesh's vertex colour is an **integer, not
   a hash** — never fuzzy-match colours.
3. Every `C` line's `index` names a row that exists in the same file.
4. Every `A`/`M` line's `shapeBlock` is a live `BSTriShape` /
   `BSSubIndexTriShape` block of the sibling `.BTO`.
5. Two bakes of one chunk are byte-identical, file and manifest
   (`tests/spells/lodgen_identity.sh`).
6. Shape keys and names are matched **case-insensitively** everywhere, because
   the engine's own `BSFixedString` pool is.

## 9. Refusals and degradation

The manifest has no magic, no version field beyond the header line's `2`, and no
checksum. A consumer should:

* refuse the file if line 1 does not start `# lodgen manifest 2` — a v1 manifest
  has **no header line at all** and its rows carry only the first eight fields
  (no `ref`, no `part`), so a positional parse of a v1 file silently reads
  nothing where the key should be;
* ignore an unknown first token rather than fail — the format grows by adding
  record kinds and by appending tokens;
* never index a row past field 10, and never index an `A`, `M` or `C` line's
  trailing path field by position from the right.

## 10. Sample files

The three manifests in `scratch_water/` (`m.bto.manifest.txt`,
`shot_objects.bto.manifest.txt`, `sway.bto.manifest.txt`) are **version 1**:
no header line, eight fields, no `ref`/`part`. They are useful only as a negative
fixture for §9's first bullet. **No v2 manifest exists on disk in this tree or in
bungo's mod folder** — see `scratchpad/handoff_fo4cs/README.md` §5.

---

## Provenance

`src/lodgen.cpp` sha256 `6d7388c53a13343e`,8286 lines, at the time of reading;
it is under active edit by another lane, so anchors are quoted.

| claim | line | anchor |
|---|---|---|
| header line and its column list | `3552-3554` | `"# lodgen manifest 2 ws %1 dim %2 chunk %3 %4 "` |
| object row, 9 fields then `ref part` | `3098-3107` | `manifest.append( QString( "%1 %2 %3 %4 %5 %6 %7 %8 %9" )` |
| field 8 is `localMaxDist * r.scale` | `3106`, computed `3055-3066` | `.arg( double( localMaxDist * r.scale ) )` |
| `class` heuristic and its four words | `3076-3091` | `const char * objClass = "misc";` |
| identity colour is `index & 0xFF`, `index >> 8` | `3096-3097` | `idColor = Color4( float( objectIndex & 0xFF ) / 255.0f, …` |
| `C` line, nine tokens, gated on `oct >= 2` | `3113-3118` | `if ( usedCard.valid && usedCard.oct >= 2 )` |
| `I` line and the ≥ 8 threshold | `3288-3297` | `if ( it.value().second.size() < 8 ) continue;` |
| `M` line | `3526` | `manifest.append( QString( "M %1 %2" )` |
| `A` line written by the array pass | `4344` | `lines.append( QString( "A %1 %2 %3" ).arg( b ).arg( lit.value().second )` |
| `A` line rewritten by the merge | `7575` | `lines.append( QString( "A %1 %2 %3" ).arg( b ).arg( recs[i].layer )` |
| `layer = −1` on a multi-layer merge | `7532` | `target.layer = ( layers.size() == 1 ) ? *layers.constBegin() : ( layers.isEmpty() ? target.layer : -1 )` |
| `A`/`M` remainder joined, not split | `7272-7274` (also `4068`) | `t.mid( 3 ).join( QChar( ' ' ) )` |
| card-array appends exactly two tokens, only at 10 | `8301-8304` | `if ( t.size() == 10 )`, `line += QString( " %1 %2" )` |
| the file always ends with a newline | `3555` | `manifest.join( QChar( '\n' ) ) + QChar( '\n' )` |
