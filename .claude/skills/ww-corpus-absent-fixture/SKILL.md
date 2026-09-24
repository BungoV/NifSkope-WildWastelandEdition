---
name: ww-corpus-absent-fixture
description: Gate a feature whose POSITIVE side does not exist in the shipped game data -- a PBR material in a corpus that has none, an emissive map on terrain that carries none, a record field no vanilla plugin sets. Covers proving the absence with a count first, choosing between the resource stack and the data root by testing what the index actually holds, authoring fixture constants that cannot alias, quoting them as the round trip through the codec rather than as the authored floats, and gating both directions in one run. Written from NifSkope WW lane TERRAIN-R (2026-09-11), which lost an hour to four false starts on exactly this. Use whenever a brief says "gate it both ways" and one of the two ways needs data nobody shipped.
---

# Gating a feature the shipped corpus cannot exercise

Repo `E:\Projects\NifskopeWildWastelandEdition`. A brief that says *"present iff
X, both directions, floor each way"* assumes both directions are reachable. Often
one is not: bungo's ruling may be *"if PBRM is used to bake it, it gets
roughness"* while **no Fallout 4 landscape TXST names a `.pbrm` at all**. A gate
that only ever sees the absent side is not a gate (CONSTITUTION 4), and a gate
built on a fixture nobody checked is worse.

## 0. Census the FORMATS before writing a reader (this is step zero, and it bites)

Before writing any independent model, decoder or sampler that must reproduce what
the C++ does, spend two minutes counting what the corpus actually contains:

```bash
python - <<'EOF'
import glob, os, struct, collections
d = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Landscape'
c = collections.Counter()
for f in glob.glob(d + '/**/*.dds', recursive=True):
    with open(f, 'rb') as fh:
        h = fh.read(148)
    fourcc = h[84:88].decode('latin1')
    if fourcc == 'DX10':
        fourcc = 'DX10:%d' % struct.unpack_from('<I', h, 128)[0]
    c[fourcc] += 1
print(sum(c.values()), dict(c))
EOF
```

Lane TERRAIN-R wrote a DXT1/DXT3/DXT5 decoder, ran the gate and got **"0 of 1369
texels modelled"**: every Fallout 4 landscape `_s` map is **BC5U**, two channels,
two BC4 blocks of eight bytes, R then G, with B 0 and A 1. The census would have
said so before a line of decoder was written. It also settled a CONTRACT question
for free -- "the specular map's gloss channel" is its GREEN channel and can be
nothing else, because there is no blue.

## 1. Prove the absence with a number, and ship the number

Do not write "the corpus has no PBRM". Write the count, and put it in the report
and in the contract, because it is the reason the fixture exists:

> 14 of 14 distinct landscape textures on the Sanctuary region resolve
> `legacy-inverted`, 0 `pbrm`, 0 with a metallic map, 0 with an emissive map.

The count comes from the bake's own census line (`fo4cs-census-field`: written
AND moves). If the feature has no census field yet, add one FIRST -- it is the
thing that will later catch the fixture silently missing.

## 2. Decide where the fixture mounts by TESTING the loader, not by reading its header

Two places a fixture can live, and they behave differently:

| | the resource stack (`--resource <dir>`) | the data root (`--data-root <dir>`) |
|---|---|---|
| how it resolves | a `BA2File` index over the folder's data sub-directories | a plain `QFile` open at `<root>/<relPath>` |
| extension filter | **YES** -- `ba2file.cpp` has a hard whitelist | none |
| order | consulted FIRST | after the stack |

`BA2File`'s loose-file whitelist lists `bgsm`, `bgem`, `dds`, `nif`, `bto`,
`btr`, `btd`, `mat`, `cdb`, `tga`, `bmp`, `hdr`, `kf`, `mesh`, `strings` and the
archive types -- and **not `pbrm`, and not `lodm`**. So a `.pbrm` in a
`--resource` folder is invisible, whatever `lodgen.h`'s own paragraph claims.

**Test it, once, before building the fixture around it:**

```bash
release/NifSkope.exe -no-gui lodgen --resource "<fixture dir>" --list-files 20
```

If the file you need is not in that list, the stack cannot serve it. The way out
that costs nothing: mount the fixture as the `--data-root` and the user's real
corpus as the `--resource` stack entry -- the stack is consulted first for the
thousands of textures, the data root supplies the handful of fixture files, and
**nothing writable belongs to the user**.

## 3. Author constants that cannot alias

A fixture whose roughness and metallic are both 0.5 passes a gate that has the
two channels swapped. Pick values that are

* away from 0 and 1 (so a cleared or saturated channel is not the answer),
* away from each other (so a swap is visible),
* and away from the DEFAULT the absent side produces -- roughness 1.0 and
  metallic 0 here, so 0.25 and 0.75 are safe and 1.0 and 0.0 are not.

## 4. Quote the ROUND TRIP, never the authored float

The gate compares against bytes that went through a block codec. `0.25` authored
into a BC1 texture comes back as **65**, not 64, because 5-bit red quantises and
expands; `0.75` in green comes back as **190**. Have the fixture script PRINT the
expected bytes it computed, and have the gate read that same computation --

```python
def q(v, bits):
    n = (1 << bits) - 1
    return round(round(v * 255) >> (8 - bits)) * 255 // n
```

-- so the number in the caption, the number in the gate and the number on disk
are one number. A fixture that prints its own expectations also documents itself
when somebody re-reads the log a week later.

## 5. Gate BOTH directions in one run of one script

The absent side and the present side must be measured by the same code, or the
two are not comparable. The shape that works:

```
bake WITH the fixture   -> census line A
bake WITHOUT it         -> census line B     (same region, same flags)
assert A.feature  > 0   and  B.feature == 0
assert A.sheetCount == B.sheetCount + 1
assert the index says "present" in A and "none" in B
then decode A's new sheet and find the fixture's constants in it
```

Each assertion's floor is the other bake. That is what makes it a gate rather
than a demonstration, and it is why both bakes belong in one script with one
count at the end.

## 6. What the report must say

* the count that proves the absence (step 1);
* that the positive arm is gated **on a fixture and has never run on shipped
  data**, in the same breath as the passing number (CONSTITUTION 9);
* where the fixture lives and that it touched nothing of the user's.

## Worked example

`tests/spells/lodgen_terrain_pbrm_fixture.py` (writes the PBRM and its two flat
BC1 textures, prints the expected bytes) and `tests/spells/lodgen_terrain_pbrm.sh`
(the two bakes, seven census assertions with each other as floors, then the texel
check) -- 14 checks, 0 failures, on a corpus that contains no PBRM at all.
