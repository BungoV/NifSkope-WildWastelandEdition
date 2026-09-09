## 2026-09-06n — Proxy meshes for the far rings, and the atlas is DXT1 like vanilla's

bungo: "Proxy meshes for the far rings. Every engine since 2017 replaces far
clusters with one simplified mesh per cell ... ring 2 and 3 chunks could ship
at a quarter of their triangles with the same textures. Perf, generator only,
measurable per chunk." And, on whether our LOD is more performant than
vanilla's: "our atlas is BC3 where vanilla's is DXT1, twice the memory per
sheet."

**The proxy pass.** `lodgenSimplifyFarRings` runs LAST, after the merge,
because the merge has already made one shape per material and that shape IS the
cluster a far ring wants one simplified mesh of. Ring 0 (dim 4) is never
touched — it is what the player walks up to and it is what every byte-identity
gate stands on. Ring 1 keeps 1.00 of its triangles by default (off), ring 2
keeps 0.35, ring 3 keeps 0.20; the panel's *Far-ring simplification* row and
its three ratios sit under Object LOD chunks for **both** targets, because a
smaller mesh is a smaller mesh for the stock engine exactly as it is for FO4CS.
CLI: `--no-simplify`, `--simplify8|16|32 R`, `--simplify-error UNITS`.

**What a simplifier must not lose here.** Every vertex of a chunk carries six
channels (`docs/LODGEN_VERTEX_PACKING.md`): the object identity index in
colours R+G, baked AO in B, sway in A, sky visibility in UV2.x, the
texture-array layer in UV2.y, the ground-contact blend in Eye Data. Two of
those are INDICES — an interpolated identity is a different object, an
interpolated layer is a different texture. So the cut is per GROUP, keyed by
(identity index, layer), and each group is simplified alone. meshoptimizer
creates no vertices, so the survivors are a SUBSET of the originals and no
channel is ever blended; the quantities ride along as weighted attributes so
the metric keeps them meaningful as well as intact. Every group is asked for at
least two triangles and restored whole if the simplifier returns nothing, which
makes the identity set of a chunk INVARIANT under the pass: no object can leave
a ring, so a manifest row still resolves at rings 2 and 3.

Shapes with an alpha property keep every triangle — a cut-out card is four
vertices that spell a silhouette and a collapse spends the silhouette to save
nothing — and impostor cards are excluded a second time by object index off the
manifest's `C` lines, so the rule is checkable rather than incidental.
Afterwards the segments are regrouped by the cell of each triangle's CENTROID,
the vertex array is compacted, and the bounding sphere and the node's
multi-bound AABB are recomputed. The manifest is not rewritten.

**Vanilla's own numbers, measured.** All 465 shipped `.BTO` under
`meshes\terrain\commonwealth\objects`, read offline with a parser that
recomputes each shape's `Data Size` from its descriptor and refuses the file on
a mismatch (all 465 passed):

| ring | dim | chunks | triangles | vertices | tris / chunk | tris / cell |
|---|---|---|---|---|---|---|
| 0 | 4  | 344 | 3,708,637 | 5,739,219 | 10,781 | 673.8 |
| 1 | 8  | 97  | 1,101,390 | 2,034,737 | 11,355 | 177.4 |
| 2 | 16 | 20  | 89,696    | 155,630   | 4,485  | 17.5 |
| 3 | 32 | 4   | 10,594    | 17,127    | 2,649  | 2.6 |

Per cell, vanilla's far rings are almost nothing — ring 2 is 9.9% of ring 1 —
and the coverage collapses with them: 20 chunks at ring 2 and 4 at ring 3
against 344 at ring 0. The chunk covering Sanctuary is 24,482 triangles at
ring 0, 7,140 at ring 1, **absent entirely** at ring 2, and 416 at ring 3.

**Why it is absent, and what that costs the gate.** Vanilla fills a ring from
the base's MNAM slot for that ring, and so do we. Measured over
`Fallout4.esm` with POSITIONAL slots (four fixed 260-byte records; every STAT
payload is a whole number of them, the 598 that are not are all FURN, whose
MNAM means something else and which `src/esmdata.cpp` also ignores): of 28,932
LOD-bearing bases, **3,081 fill slot 0, 2,940 slot 1, 456 slot 2, 51 slot 3**.
Over the ring-2 chunk (−32,16), **0 of 19,507 references** has a base that
fills slot 2; over the ring-3 chunk (−32,0), 0 of 148,362 fills slot 3. So a
far ring over Sanctuary is EMPTY unless the generator substitutes the nearest
filled slot (`slotFallback`: 2,518 refs at ring 2, 21,498 at ring 3) or stands
placements on impostor cards. Cards are excluded from the cut by design, so a
card-filled chunk would make the gate vacuous — the harness therefore builds
rings 2 and 3 with the fallback on, which is precisely the case the pass exists
for. The panel has had that toggle since the chunk builder shipped; the CLI had
no way to reach it, so `--slot-fallback` is new here.

**The atlas.** Vanilla's diffuse sheet is **DXT1**, not BC3 — measured on the
shipped file: `Commonwealth.Objects.DDS`, 4096×2048, 13 mips, fourCC `DXT1`,
5,592,552 bytes. The comment in `lodgenBuildAtlas` claimed BC3 "like vanilla's
sheet", and that claim was the argument for ours being BC3. It is now
`--atlas-bc1`, which the panel selects for the **stock target**: BC1 with
one-bit punch-through alpha for the cut-outs, half the memory, vanilla parity.
FO4CS keeps BC3 for its eight-bit alpha. Writing BC1 needed one fix in
`lodgenWriteDds`: with `bc3` false the mip filter forced alpha to 255, so every
mip below the top would have turned a tree's leaves back into a solid square —
at exactly the distance an atlas is looked at. Alpha now rides the chain when
the caller asks for it, which leaves every existing all-opaque BC1 caller (the
terrain bakes, the emissive sheets) byte for byte as it was.

**A question the gates needed.** `lodgen --dump-geometry FILE.BTO` prints one
`G` line per shape — what it weighs (vertices, triangles, segments) and the raw
counts its own invariants stand on: triangles whose centroid is in another
segment's cell, centroids outside the chunk, vertices outside the bounding
sphere or the node AABB — plus an `i` line with its object identity indices.
Every number is a raw count; the comparisons belong to the harness, and each
one can be non-zero on a real file.

**Gates.** `tests/spells/lodgen_farring.sh`: rings 0, 1 and 2 built twice each,
`--no-simplify` against the pass, compared shape by shape — the triangles
within 10 points of the ratio, the vertices down with them, the identity sets
equal, the segment count unchanged with no centroid in another cell or outside
the chunk, every vertex inside its bounds, the manifest's placement rows
byte-identical, **ring 0 byte-identical chunk and manifest**, and the atlas
DXT1 under `--atlas-bc1` against DXT5 without it with punch-through blocks
surviving past the top mip (the half of that check which fails on the old mip
filter). Then `lodgen_merge.sh`, `lodgen_texture_arrays.sh`,
`lodgen_card_arrays.sh`, `lodgen_identity.sh`, `lodgen_impostor_cards.sh` and
`lod_generation.sh`.

**Not done.** The build was refused to this lane (account B allowlists no
NifSkope command), so **no number in this entry that describes OUR output has
been measured** — every measurement above is of vanilla's shipped files or of
`Fallout4.esm`, offline, with no exe. Ours before → after is owed and the
harness is what produces it. Ring 3 is opt-in in the harness
(`FARRING_RING3=1`) because the ring-3 chunk over Sanctuary is 21,498
references before SCOL expansion. The centroid regroup is code that does not
run today: the generator writes ONE segment for every ring but 4
(`segs = (dim == 4) ? 16 : 1`), and that is vanilla parity — measured across
all 465 shipped chunks, every dim-8, dim-16 and dim-32 shape carries exactly
one segment (201, 37 and 5 shapes), while at dim 4 they carry up to sixteen
(519 of 719 shapes have all sixteen). So at rings 1–3 the regroup collapses to
the single run it started with, and the harness's "centroid in its own cell"
check is a floor for the day a far chunk grows a per-cell grid, not a
measurement of one today. The ratio gate can legitimately miss: meshoptimizer stops short
of a target on topology, and a chunk of many small disconnected LOD shells is
exactly where that happens — the harness prints the achieved ratio beside the
asked one so a miss is diagnosable rather than mysterious. `--ao-grey` writes
AO into R and G, which is where the identity index lives, so under that debug
flag the grouping degenerates to grouping by AO level and the pass will barely
cut anything; it is a debug flag and is not defended against. And vanilla's
NORMAL atlas is `BC5U` where ours is BC3 — measured on the same shipped files,
another 2× on a second 11 MB sheet — which this lane did not change.

