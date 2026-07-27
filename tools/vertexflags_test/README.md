# Vertex Flags harness (`WW_VERTEXFLAGS_TEST`)

Checks whether editing a `BSTriShape`'s **Vertex Desc** correctly rewrites the
per-vertex records, for the case where the vertex **size** changes but the
vertex **count** does not.

## Why this exists

`WW_CHANGES.md 2026-07-18b` flagged, after the Create Skin corruption, that
*"any spell doing `set<BSVertexDesc>` + `updateArraySize` on an unchanged vertex
count is suspect — incl. the stock Vertex Flags spell (flags.cpp)"*. The reason
to suspect it is real: `NifModel::updateArraySizeImpl` early-returns when the
row count is unchanged (`nifmodel.cpp`, `if (nNewSize == nOldSize) return true;`),
so the call the spell makes to rebuild the rows appears to do nothing.

That was a **conjecture, never tested**. This harness tests it.

## Verdict (2026-07-21): the stock Vertex Flags spell is CORRECT

All four toggle directions pass in both layers, and both precision/colour
round-trips reproduce the input file **byte for byte**. See
`WW_CHANGES.md 2026-07-21a`.

The early return is harmless here because it is not the rows that need
rebuilding — the row *count* genuinely does not change. Every `BSVertexData`
field variant is already materialised as a `NifItem` in every row; the
`#ARG#`-gated conditions only decide which ones are live. Writing `Vertex Desc`
re-evaluates those conditions (the `Vertex Data` array's
`arg="Vertex Desc #RSH# 44"` names the field, so
`NifModel::invalidateDependentConditions` reaches it), and the serialiser then
writes the new layout. The spell additionally carries positions across a
precision change by discriminating on `valueType()`, not by name — which is the
correct handling of the two `Vertex` variants.

**Do not "fix" this spell.** If you are chasing a desc-related corruption, the
Create Skin case is the one that was real, and its root cause was *creating*
skin arrays that had zero children until the deferred cascade — not flipping a
live/dead bit on already-materialised fields.

## Running it

`=1` toggles Colors (stride ±4 B, uniquely named field).
`=2` toggles Full Precision (stride ±8 B — the sharper case: `Vertex` and
`Bitangent X` each appear **twice** in `BSVertexData`, as `Vector3`/`float` and
`HalfVector3`/`hfloat`, so a by-name lookup cannot tell the variants apart).

```powershell
$W = 'E:\path\to\work'
copy tests\rigging\fixtures\donor.nif $W\g0.nif   # FO4 bs130, half precision, skinned, no colours

$env:WW_VERTEXFLAGS_TEST='2'
$env:WW_TEST_SAVE="$W\gB.nif"
release\NifSkope.exe $W\g0.nif                    # casts the REAL spell, then quits
```

Log: `release/ww_vertexflags_test.log`. The harness casts the actual spell
through `NifSkope::castSpell` (the path the Block Details context menu uses)
with a timer ticking its modal checkbox dialog, so the whole shipping code path
runs — including the `getVertexPositions` / `setVertexPositions` round trip.

## Verifying

```powershell
python tools\vertexflags_test\verify_vertexflags.py $W\g0.nif $W\gB.nif
```

The verifier does not trust the model that wrote the file. The header's
**Block Sizes** table is an independent record of how many bytes each block
actually occupies, so for every shape it asserts

```
saved block size - original block size  ==  Num Vertices * (new stride - old stride)
```

If the rows were left in the old layout while the desc advertised the new one,
the actual delta would be 0 while the expected delta is not. It also re-checks
`Data Size == numVerts*stride + numTris*6`.

## The gauntlet

Chain the four directions and assert the round-trips are byte-identical:

| case | mode | in  | out | stride |
|------|------|-----|-----|--------|
| A    | 1    | g0  | gA  | 32→36  |
| B    | 2    | g0  | gB  | 32→40  |
| C    | 1    | gA  | gC  | 36→32  |
| D    | 2    | gB  | gD  | 40→32  |

`gC` and `gD` must both hash equal to `g0`.

## Harness gotcha

The probe that reads the row layout back is subject to the very hazard being
tested: `"Vertex"` names **both** precision variants, so
`getIndex(row0, "Vertex").isValid()` is true either way and proves nothing. The
first version of this harness reported a false failure for exactly that reason.
For Full Precision the probe must ask the live item for its `valueType()`
(`tVector3` vs the half variant); only the Colors probe can rely on the name
being unique.
