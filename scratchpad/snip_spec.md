**Far rings — proxy meshes** (`lodgenSimplifyFarRings`, `--no-simplify` /
`--simplify8|16|32 R` / `--simplify-error UNITS`, the panel's *Far-ring
simplification*, on by default under BOTH targets). Runs LAST, after the merge,
because the merge has already made one shape per material and that shape IS the
cluster a far ring wants one simplified mesh of. Ring 0 (dim 4) is never
touched; ring 1 is 1.00 by default, ring 2 keeps 0.35 of its triangles and ring
3 keeps 0.20.

The cut is per GROUP, keyed by (object identity index, array layer). Both are
INDICES, not quantities — an interpolated identity is a different object and an
interpolated layer is a different texture — and grouping by them means no
collapse can cross either. meshoptimizer creates no vertices, so the survivors
are a SUBSET of the originals and every channel of
`docs/LODGEN_VERTEX_PACKING.md` arrives intact rather than blended; the four
that are quantities (normal, UV, sky visibility in UV2.x, AO in colour B, sway
in colour A, ground contact in Eye Data) ride as weighted attributes so the
metric keeps them meaningful too. Each group is asked for at least two
triangles and its originals are restored if the simplifier returns nothing, so
**the set of identity indices in a chunk is the same before and after**: no
object can leave a ring.

A shape with an **alpha property keeps every triangle** — a cut-out card is
four vertices that spell a silhouette, and a collapse spends the silhouette to
save nothing. That covers the impostor quads and the crossed quads inside
vanilla's own tree LOD models alike, and the impostor cards are excluded a
second time by object index off the manifest's `C` lines so the rule is
checkable. Groups of eight triangles or fewer keep every triangle.

Afterwards the segments are regrouped by the cell of each triangle's CENTROID
(the generator assigns them per placement, which stops being the unit once
triangles move), the vertex array is compacted to the survivors, and the
bounding sphere and the node's multi-bound AABB are recomputed. The manifest is
not rewritten: no row's meaning changed.

The error bound is in WORLD units at ring 0 and is scaled by the ring's dim, so
it is the same on-screen error at every ring — and because a chunk shape's
vertices are miniatures at `Scale = dim`, that scaling cancels and the bound is
a constant in the file's own units. The simplifier stops early rather than
exceed it, so the ratios are targets and the bound is the rail.

**Why a far ring is empty without help.** Measured over `Fallout4.esm`: only
456 of 28,932 LOD-bearing bases fill MNAM slot 2 and only 51 fill slot 3, and
over the chunk that covers Sanctuary at ring 2, (−32,16), **0 of 19,507
references** has a base that fills slot 2 (0 of 148,362 at ring 3). Vanilla
ships 20 dim-16 chunks and 4 dim-32 chunks for the whole Commonwealth, against
344 at dim 4, and no dim-16 chunk over Sanctuary at all. So a far ring holds
something only with `--slot-fallback` (the panel's *Use a nearer LOD slot when
the ring's is empty*, 2,518 refs at ring 2) or with impostor cards — and since
cards are excluded from the cut by design, the fallback is the case this pass
exists for: it is what makes "far chunks grow to many times vanilla's size"
affordable.

**The atlas format.** Vanilla's diffuse sheet is **DXT1**, not BC3 — measured
on the shipped `Commonwealth.Objects.DDS`: 4096×2048, 13 mips, fourCC `DXT1`,
5,592,552 bytes (its `_n` and `_s` are both `BC5U`). `--atlas-bc1`, which the
panel selects for the **stock target**, writes ours the same way: BC1 with
one-bit punch-through alpha for the cut-outs, carried down the whole mip chain,
half the memory of BC3. FO4CS keeps BC3 for its eight-bit alpha. The `_s` sheet
stays BC5 either way.

Gate: `tests/spells/lodgen_farring.sh` — rings 0, 1 and 2 built twice each,
`--no-simplify` against the pass, compared shape by shape through
`lodgen --dump-geometry`: the triangles within 10 points of the ratio, the
vertices down with them, the identity sets equal, the segment count unchanged
with no centroid in another cell or outside the chunk, every vertex inside its
bounding sphere and its node's AABB, the manifest's placement rows unchanged,
ring 0 byte-identical, and the atlas DXT1 under `--atlas-bc1` and DXT5 without
it with punch-through blocks surviving past the top mip. Ring 3 is opt-in
(`FARRING_RING3=1`): 21,498 references before SCOL expansion.
