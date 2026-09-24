# Lane GENSMALL1 — small generator items — 2026-09-16

## 0. Exe at launch

Read at 2026-09-16 13:08:27 (local `date`).

```
-rwxr-xr-x 1 bungo 197609 22293504 Sep 16 12:49 release/NifSkope.exe
```

- `release/NifSkope.exe` — 22,293,504 B, mtime 2026-09-16 12:49:19. Built by lane VT1 (tile baker protects the box of the
  chunk it is assembled into); before it, NATIVEVIEW2 added the model-space normal branch to `res/shaders/fo4_default.frag`.
- Rung copy taken before any build of mine:
  `release/NifSkope.before_gensmall1.exe` — 22,293,504 B, same mtime (copied with `cp -p`).
- Existing rungs left untouched: `NifSkope.before_nativeview2.exe` (22,280,192 B, 11:40),
  `NifSkope.before_vt1.exe` (22,288,896 B, 11:54).
- Game check before anything: `tasklist | grep -i -E "Fallout4|NifSkope"` → no match (rc=1). Neither the game nor a
  NifSkope window is up. I am the only lane in the tree; no build mutex taken, none left behind.

_(sections below written incrementally as the work lands)_

---

## 1. The cell rule (plan §5 row 6)

### 1.1 The two readings, with the numbers

The writer and the two readers do not compute a cell from the same number.

| who | file:line | the position it takes the cell from |
|---|---|---|
| **writer** | `src/lodifile.cpp:312` — `lodiCellOf( set.instances[i].pos[0], set.instances[i].pos[1], … )` | the **float** source position, before the u16 quantiser |
| decoder (Python) | `tests/spells/lodgen_native_decode.py`, per-instance loop | the **dequantised** position |
| `--native-verify` (C++) | `src/lodifile.cpp:982` — `lodiCellOf( pa[0], pa[1], … )` after `lodiDecodePosition` | the **dequantised** position |

The instance position is 3 × u16 over the 16,384-unit chunk box (NATIVE §4.1), so
one step is `16384 / 65535 = 0.2500038 u` and `lodoQuantU16` rounds to nearest —
a stored position is at most **half a step, 0.1250019 u**, from the float the
writer sorted on. An instance within that band of a cell line can therefore be
stored on one side of the line and sorted on the other.

**Measured on the Boston pair** (`scratchpad/native1b_20260911/gate/final/occ/Native/Commonwealth.lodi`,
33,123 instances, 12 present chunks), by `scratchpad/gensmall1_20260916/cellprobe.py`,
which prints the table and not the count (`ww-spec-gate-audit`):

| instance | chunk | cell the FILE stores it in | cell the position DERIVES | distance from that cell line |
|---|---|---|---|---|
| 3358 | 6 | 0 | 4 | 0.062501 u (Y) |
| 3361 | 6 | 0 | 4 | 0.062501 u (Y) |
| 3576 | 6 | 1 | 5 | 0.062501 u (Y) |
| 3585 | 6 | 1 | 5 | 0.062501 u (Y) |
| 23559 | 10 | 3 | 7 | 0.062501 u (Y) |
| 26388, 26397, 26399, 26414, 26415, 26470, 26541, 26579, 26586 | 10 | 8 | 9 | 0.062501 u (X) each |

**14 of 33,123 instances, 0.042 per cent.** Every one of them:

- differs by **exactly one cell on exactly one axis**, never diagonally, never by more than one;
- sits **0.062501 u** from the shared cell line — half the worst case the
  quantiser allows, and the closest a representable position can get to a
  4,096-unit line on this grid (the grid points either side of 4,096 are
  4,095.8125 and 4,096.0625).

### 1.2 Whose rule is wrong, and the answer

**Neither the writer nor the file is wrong.** The `.lodi` states each
instance's cell in its own **cell-range table**, which the readers already check
separately partitions the chunk in order (`lodifile.cpp:900-911`, decoder
`walk_lodi`). Re-deriving the cell from a lossy field and then arguing with the
file about it is a reader rule *stricter than the format*. So the writer does not
move and `lodgen_native_baseline --check` must stay at 25 files 0 differ — it does
(§6).

### 1.3 What changed

Both readers now take an instance's cell **from the cell range it falls in**, and
keep the derived cell as a **check on it**: the two may differ only by one on one
axis, and only when the stored position is inside the quantiser's ambiguity band
of that cell line. Anything else is still a refusal, and it names both cells, the
position and the distance.

- `tests/spells/lodgen_native_decode.py` — new constants `POS_QUANT_STEP`,
  `CELL_QUANT_TOL` (half a step + 2 ulp of 16,384 = **0.126955 u**); the chunk walk
  builds `cellOfIndex` from the range table; the per-instance loop sets
  `r['cell']` from the range and `r['cellDerived']` from the position.
- New census lines, so the number is written and can be watched to move:
  `lodi.cellQuantAmbiguous`, `lodi.cellQuantWorstU` (worst distance and the band),
  and one `lodi.cellQuant` line per instance for the first eight.
  Also added `lodi.occluderCount` to the printed census.

### 1.4 The gate, and the refuters

**Gate — the Boston pair decodes.**

```
lodi.instanceCount 33123     lodi.presentChunks 12
lodi.occluderCount 280       lodi.fileBytes 1091072
lodi.cellQuantAmbiguous 14   lodi.cellQuantWorstU 0.062501 (band 0.126955)
6 checks, 0 failures — RESULT PASS   (exit 0)
```

280 is the count the bake's own `native-occluders:` line printed for this pair
(`scratchpad/bakeperf1_20260911/harness/lodgen_native.log:160` — *"boxes 280
written from 599 offered"*), and the decoder reads the box table back whole:
every box's flags, finite centre, positive half extents, and the rule that a box
names an instance **of its own cell**.

**Refuter 0 — the old decoder on the same file.** `REFUSED: instance 3359 out of
(cell, drawKey, ref, part) order`, `RESULT FAIL`, exit 1. Same for
`--native-verify` on the rung exe: *"instance 3359: out of (cell, drawKey, ref,
part) order after instance 3358"*, exit 1.

**Refuter A — a genuinely misplaced instance** (`scratchpad/gensmall1_20260916/cellrefute.py`
moves instance 3358 one whole cell in X and **re-signs the file whole** — every
chunk crc32, `indexCrc32`, `headerCrc32` — so nothing but the thing under test is
wrong):

> REFUSED: instance 3358 (ref 0011ed8f) is stored in chunk 6 cell 0 but its position
> (7488.1143, 12287.9375 in the chunk box) derives cell 5; the two cells are not
> neighbours on one axis

**Refuter B — a neighbour on one axis, but outside the band.** Instance 26388
moved 8 quantiser steps (2 u) past the line:

> REFUSED: instance 26388 (ref 00057ac3) is stored in chunk 10 cell 8 but its position
> (4098.0625, 7296.1113 in the chunk box) derives cell 9; it is 2.062531 u from the
> cell line, past the 0.126955 u the quantiser could have moved it

**Control C — re-signed with no position change:** `RESULT PASS`, still
`lodi.cellQuantAmbiguous 14`. So the refusals above come from the move, not from
the re-signing.

### 1.5 The same defect in the C++, found here and fixed with it

`--native-verify` refuses the Boston pair for the identical reason, measured on
the rung exe before any change:

```
native REFUSED Commonwealth.lodi: instance 3359: out of (cell, drawKey, ref, part) order after instance 3358
```

`lodiVerify` is a READ path — it writes no bytes — so fixing it cannot move the
writer, and `lodgen_native_baseline --check` is the pin that says so. The rule is
put in **one shared place** (`lodiCellAgrees`, `src/lodifile.{h,cpp}`) rather than
typed twice (CONSTITUTION 10, *what is shared lives in the shared code*).

## 2. Whitelist

`lib/libfo76utils/src/ba2file.cpp` carries a hand-written extension filter that
decides which LOOSE files a `--resource <dir>` is allowed to serve. A file whose
extension is not in the switch is skipped in silence — no warning, no census
line — so `.pbrm` and `.lodm` beside a model were invisible to a loose-file run
while the same two extensions inside a `.ba2` were served normally.

The filter is not a string compare. Each extension is packed six bits per
character into a `quint64`, most significant character first, at bit offsets
48, 42, 36, 30 … with

```
c = (c & 0x1F) | ((c & 0x40) >> 1)
```

which folds case, so one constant covers `.PBRM` and `.pbrm` -- measured, not
assumed: `pack("LODM") == pack("lodm")` and `pack("PBRM") == pack("pbrm")`.

**The constants were not guessed.** Before computing the two new ones, the same
expression was run over **every one of the nineteen values already in the
switch** (`ba2`, `bgem`, `bgsm`, `bmp`, `bsa`, `btd`, `bto`, `btr`, `cdb`,
`dds`, `dlstrings`, `hdr`, `ilstrings`, `kf`, `mat`, `mesh`, `nif`, `strings`,
`tga`) and required to reproduce the bytes already on the line:
**19 of 19, 0 mismatches**. Only then:

| extension | constant | placed after |
|---|---|---|
| `lodm` | `0x2CBE4B40000000ULL` | `"kf"` |
| `pbrm` | `0x308B2B40000000ULL` | `"nif"` |

The switch is kept in ascending constant order, which is the order it was
already in, so the two cases sit where a reader looking for them would look.

`lib/libfo76utils/src/ba2file.cpp`: **33,486 → 33,737 B**, CR 0 → 0, LF 1,115 →
1,119 — four lines, two of comment and two of `case`. This file is TRACKED and was
untouched at `HEAD` (33,486 B, LF 1,115, measured with `git show`, read-only),
so the before column is not a remembered number.

**Every other extension byte-identical** is the claim, and it is the whole
claim: nothing was removed, no constant was edited, and no code path outside the
switch was touched. The gate that holds it is `tests/spells/resource_ext.sh`
(§6), which bakes the same chunk through a loose `--resource` dir twice and
`cmp`s the products.

## 3. Height row

`--vt-height` exists in the code and in §2.2 of the VT contract page, and the
**CLI TABLE in §5 did not list it**. A flag that is implemented, documented in
prose, and missing from the table a user actually reads is a flag that does not
exist for that user.

The row now added states, with every number re-derived from the anchor beside it
(`ww-contract-provenance`):

- **default off**, and that it stays off;
- §2.2 layer 3: a fourth **R16_UNORM** height sheet per tile, same tile grid and
  border as the other three, finest from the LAND records, coarser by the same
  box filter;
- why it is off: it is **uncompressed** where the other three are BC1, so at
  content 256 / border 8 / 2 mips a height sheet is **184,960 B** against a BC1
  sheet's **46,240 B** (§3.6), and a tile goes from **138,720 B** to
  **323,680 B** without cover, **369,920 B** with (§2.2, §3.6);
- **the way back**: not passing the flag is byte-identical to the bake before
  the layer existed;
- the panel row it corresponds to — **Terrain → Carry a height layer**
  (`LodgenVtHeightCheck`), also unticked by default.

`docs/LODGEN_TERRAIN_VT.md`: **189,400 → 190,103 B**, CR 0 → 0, LF 2,905 →
2,906.

**A mistake inside this item, caught by measurement and not by eye.** The row as
first applied carried a curly apostrophe (`U+2019`) in *"a BC1 sheet's"*. A byte
count over the three contract pages read:

```
docs/LODGEN_TERRAIN_VT.md        curly 1   ascii 451
docs/LODGEN_NATIVE_LODO_LODI.md  curly 0   ascii 252
docs/LODGEN_CENSUS.md            curly 0   ascii 56
```

— one curly apostrophe in 759, and it was mine. Normalised to ASCII; all three
pages now read **curly 0**. The same slip then cost a failed anchor on the next
edit (§9).

Still owed in this item: the panel-row-vs-CLI byte-identity leg, and the pin
that the default does not move. Both run in §6.

## 4. Census words

bungo's ruling of 2026-09-11 14:4x is *"the census counts casters per source"*,
under his rule that **every placement has exactly ONE shadow representation at a
time**; his 15:0x ruling made the hybrid far shadow the arm and both of its
halves — the terrain march and the far shadow map — census fields in their own
right.

Two of the three are RUNTIME words: FO4CS times a frame, a baker cannot. What
the generator CAN promise, and now does, is the **bake-side denominator**: how
many instances each source would be asked to cast for.

### 4.1 The number, and why the four bins are a partition

In `src/nativeemit.cpp`, before the mesh report is written, the instance table
is walked once and each instance is put in exactly one of four bins by a stated
priority — **tree > card-only > mesh > none**:

| bin | means |
|---|---|
| `tree` | the base carries `LODO_BASE_TREE` |
| `card` | not a tree, and every representation it has is a card |
| `mesh` | not a tree, and at least one `rep[k]` is a real mesh |
| `none` | no representation of any kind — a **fault** count, not a zero |

Because the priority is total and every instance is tested, the four **sum to
the instance count**, and the line says so itself rather than leaving a reader
to add four numbers up and hope.

A fifth number, `mesh-slot casters`, counts each instance **once per DISTINCT
mesh its base names** (a base has up to four `rep[k]` slots and they may repeat),
which is the granularity a per-mesh column needs.

### 4.2 Where it is WRITTEN, and how it MOVES

Three places, because a census word that lives in one of them is not a contract:

1. **The bake line**, on its own prefix `native-casters:` — see §4.3.
2. **The `--native-mesh-report` sidecar**, as a new column `casterInstances`,
   per mesh. The sidecar went **version 3 / 25 columns → version 4 / 26**.
3. **The contract pages**: `docs/LODGEN_CENSUS.md` §5.3 gains
   `shadowCasters[src]`, `shadowMarchMs` and `shadowMapMs` with their read-from
   columns, refusal words and defaults; §6.1 gains the `native-casters:` line;
   §6.2 gains the cross-check that binds the runtime rows to the bake bins.
   `docs/LODGEN_NATIVE_LODO_LODI.md` §3.4 is corrected to 4 / twenty-six.

The column MOVES (a gate requires more than one distinct value across meshes,
and a non-zero somewhere) and the line MOVES (a gate requires `tree` > 0 and
`mesh` > 0 on a region that has both).

### 4.3 A deviation from the brief's literal wording, named

The brief says to add the per-source caster count *"to the bake's `native:`
census line"*. It is on **`native-casters:`**, its own prefix, instead.

The reason is a house rule already written into `nativeemit.cpp` for the v3
rows: *"The v3 rows go on their OWN lines rather than into the sentence above:
the census line is already at the edge of readable, and a reader greps one
prefix a subject."* `native:` already carries thirty-five clauses. A thirty-sixth
would have made the caster counts harder to read, not easier, and would have
broken the grep-one-prefix-one-subject rule that `native-ladder:` and
`native-occluders:` already follow. The director's call if this is wrong; it is
a one-line change to fold it in.

### 4.4 What the pages say about the two runtime words

Both read from **`runtime` — FO4CS times it; no bake number exists**, stated in
the row rather than left blank, and §6.2 says the same for
`shadowCasters[terrain]` with an em dash in the bake column and the sentence
that the march runs over the resident height source at runtime. The plan's
figures (march 0.3–0.8 ms, map 1–3 ms) are carried **labelled ESTIMATE** and
stay labelled until a capture round measures them — CONSTITUTION's *nothing is
stated as a cause without a measurement* applies to costs too.

**These three were NOT on §6.3's owed list.** That list's eight items are all
container-format holes (a per-slot instance total, a per-base triangle count, a
card count in the header, a watertight bit …). The shadow words were missing
without ever being written down as missing, which is why nothing had chased
them. Nothing had to be retired from §6.3.

### 4.5 Bytes

| file | before | after | CR | LF |
|---|---|---|---|---|
| `src/nativeemit.cpp` | 57,196 | 60,555 | 0 → 0 | 1,352 → 1,419 |
| `docs/LODGEN_CENSUS.md` | 35,284 | 37,851 | 0 → 0 | 448 → 454 |
| `docs/LODGEN_NATIVE_LODO_LODI.md` | 99,478 | 100,130 | 0 → 0 | 1,576 → 1,585 |
| `tests/spells/lodgen_native.sh` | 13,883 | 15,024 | 0 → 0 | 261 → 278 |
| `tests/spells/lodgen_native_fields.py` | not captured | see `CHANGED_FILES.txt` | 0 → 0 | — |

All five are LF-only files and stayed LF-only.

## 5. Asymmetric drop

**The claim holds to the digit, and the native path does not drop.** No code
change was made, which is what the brief says to do in that case.

### 5.1 The audit, before the bake (`ww-spec-gate-audit`)

The skill's first rule is to read the SOURCE of a pre-registered number, not the
number. The number is quoted in `docs/LODGEN_NATIVE_LODO_LODI.md` §9 without a
provenance line; it was traced to
`scratchpad/specs_20260906/spec_fo4cs_native.md:277` (lane SPECS, 2026-09-06),
which names the exact line that drops: `src/lodgen.cpp` — today line **3919** —

```
if ( vBase + quint32( keep.size() ) > 65535 )
        continue;   // bucket full; a second shape would need splitting
```

Three things came out of reading it rather than trusting it:

1. **The drop is counted nowhere.** There is no `droppedForBucketCap` beside
   `culledPlacements`, no census clause, no refusal. The only way to see it from
   outside is to compare the placements the manifest lists against the identity
   indices the `.BTO` carries, which is what lane SPECS did.
2. **That method now depends on a flag that went OFF.** The identity index lives
   in vertex colours, and `--identity` became OPT-IN on 2026-09-12 (lane
   DEFAULTS1). A default bake carries no index, so the measurement re-run as
   written on a default profile reads **nothing at all** and would report zero
   drops — the vacuous-green case the skill's ROADS2 section is about. Both
   bakes below therefore pass `--identity` explicitly, and that is a stated
   deviation from the shipped default, not a silent one.
3. **The spec calls its own figure a LOWER BOUND** — *"a placement that lost
   some of its shapes still shows an identity index"*. That is a one-sided
   approximation, so it had to be tested rather than repeated (§5.3).

### 5.2 The stock path, measured twice

`scratchpad/gensmall1_20260916/dropprobe.py`, on the glTF gates' independent NIF
reader (only the vertex-colour walk is new), run on two different bakes of the
same chunk:

| bake | shapes | vertices | manifest rows | with geometry | **with none** |
|---|---|---|---|---|---|
| `--objects -32 0 --dim 32 --identity --slot-fallback --road-detail 1 --no-ao` | 85 | 803,908 | 42,560 | 39,932 | **2,628 (6.17%)** |
| the same chunk in REGION mode (merge 85 → 62 shapes, far-ring cut) | 62 | 704,522 | 42,560 | 39,932 | **2,628 (6.17%)** |

The four saturated shapes are the spec's four, to the vertex:

```
block 243  obj-at   65535 vertices   21845 triangles
block 250  obj-at   65534 vertices   29840 triangles
block 264  obj      65529 vertices   59655 triangles
block 308  obj      65424 vertices   29580 triangles
```

and so is the order-dependence:

```
first index with no geometry      38695
indices at or after it            3865
of those, with no geometry        2628  (68.0%)
of those, WITH geometry           1237  (32.0%)
```

**What the table adds that the count did not.**

- **The merge does not recover a single placement.** 85 shapes become 62 and
  two of the four saturated buckets fall below the cap, and the drop is
  bit-for-bit the same 2,628 with the same first index. The loss happens while
  the buckets are being filled, before anything that could repack them.
- **Every one of the 2,628 is a `STAT`.** Not a tree, not an `SCOL` part
  distinguishable by type — the loss is not a class of object, it is a position
  in a queue.
- **One base loses 851 placements by itself** (`0004a075`), then 326
  (`000393cd`), 263, 203, 181 … Ten bases account for 2,542 of the 2,628. A
  base whose placements happen to be listed late loses almost all of them.

### 5.3 The "lower bound" caveat does NOT apply on this chunk

The spec's caveat needs a placement to own more than one model, so that losing
one shape still leaves an identity index behind. Measured on the manifest's own
instance-group lines: **452 `I` lines name 39,840 placements, and the greatest
number of models named for any one placement is 1.** Not one placement on this
chunk can be partially present, so **6.17% is exact here**, not a floor. (The
caveat stays true as written for a chunk where a placement owns several models;
it is this chunk that does not exercise it.)

### 5.4 The native path on the same chunk

```
lodgen <esm> --worldspace 3C --terrain-region -32 0 -1 31 --dim 32
       --identity --slot-fallback --road-detail 1 --no-ao
       --out-dir <abs> --native <abs>
```

The bake's own census line:

> `Commonwealth.lodi 1410176 bytes (instances 42560 from 42560 arrivals over
> 701131 census refs, **0 dropped** for a base outside the table, 42560 unlit,
> **0 without a stock identity**; chunks 73 present of 100 dense, max 4245 a
> chunk, max scale 4.9700, max baseId 2969, PARTIAL)`

and the independent decoder, which does not read that line:

```
lodi.instanceCount 42560
manifest Commonwealth.32.-32.0.BTO.manifest.txt: 42560 rows, 0 not in the table
  ok   manifest: every row is in the table
  ok   manifest: the stock identity index is in the .lodi cold record
  ok   manifest: identity unique over this chunk (42560 distinct)
12 checks, 0 failures
RESULT PASS
```

| path, same chunk, same profile | placements offered | carried | **lost** |
|---|---|---|---|
| stock `.BTO` | 42,560 | 39,932 | **2,628 (6.17%)** |
| native `.lodo` / `.lodi` | 42,560 | 42,560 | **0 (0.00%)** |

The asymmetry is the whole point of the row: the two paths were handed the same
42,560 placements in the same run, and one of them arrived complete.

**Why, mechanically**, and this is read off the format rather than argued: the
stock path stitches many models into one shape and pays for it with a **shared**
16-bit vertex index, so a bucket is a finite resource several hundred placements
compete for. The native path's only index domains are a cluster's u8 local index
over at most 48 vertices and a u32 `vertexBase` over the library's 419,204
vertices, and an instance is 24 bytes that name a base — there is no shared
resource for a placement to be squeezed out of. The `.lodi` costs 1,410,176 B
for the chunk, **33.1 B a placement**.

### 5.5 No code change, and what is owed instead

The brief: *"No code change unless the native path drops — then STOP and
report."* It does not drop, so nothing was changed.

**Owed, and named rather than done, because it is the stock path and outside
this lane's five items:** `src/lodgen.cpp:3919` still drops in silence. The
honest fix is one counter and one census clause — *"N placements carried no
geometry because their material bucket was full"* — so that a user of the stock
path is told. Today the only witness is a bake with `--identity` on and a
private script. It is a generator lane's item, and it wants bungo's call on
whether the stock path should refuse, warn, or split the bucket.

Also owed: `docs/LODGEN_NATIVE_LODO_LODI.md` §9 now has a re-run to cite. This
lane did not edit §9 because the numbers in it are unchanged and correct; what
it lacks is the provenance line, which the director may want added with the
2026-09-16 re-run named.

## 6. Gates

Every gate below ran on the exe this lane built — **22,300,160 B, 2026-09-16
13:52:56** — and every line is the spell's own summary, copied, not retyped.
The raw logs are beside this report in `scratchpad/gensmall1_20260916/`.

| gate | checks | failures | skips | result | log |
|---|---|---|---|---|---|
| `tests/spells/lodgen_native.sh` | **108** | **0** | 2 | PASS | `gate_lodgen_native.txt` |
| `tests/spells/lodgen_native_baseline.sh --check` | 4 | **0** | 0 | PASS (25 files, **0 differ**) | `gate_baseline_check.txt` |
| `lodgen_native_decode.py`, Boston (0,−12)…(11,−1) dim 4 | 6 | **0** | 0 | PASS | `gate_decode_boston.txt` |
| `tests/spells/resource_ext.sh` (NEW) | 12 | **0** | 0 | PASS | `gate_resource_ext.txt` |
| `tests/spells/lod_generation.sh` | 121 | **0** | 0 | PASS | `gate_lod_generation.txt` |
| `tests/spells/lodgen_defaults.sh` | — | — | — | **exit 2, did not run** | `gate_defaults_before.txt` |
| `tests/spells/lodgen_terrain_vt.sh` (both arms) | see §6.6 | | | | `gate_vt_before.txt` |

### 6.1 `lodgen_native.sh` — 108 checks, 0 failures

Its seven sub-blocks, each with its own tally, so a block that silently stopped
running would show as a smaller number and not as a quiet PASS:

| block | checks | failures | skips |
|---|---|---|---|
| the synthetic fixture, decoded | 69 | 0 | — |
| the mutation set | 44 | 0 | — |
| the real Sanctuary pair, decoded | 87 | 0 | — |
| v2 fields | 40 | 0 | 1 |
| geometry on the fixture | 17 | 0 | 0 |
| geometry on the real pair | 15 | 0 | 1 |
| v3 occluders | 22 | 0 | — |

The four checks this lane added are green and the census line they read is:

```
native-casters: per source, of 3526 instances: tree 3446, card 0, mesh 80,
none 0; sum 3526 == instances 3526 == AGREE; mesh-slot casters 5958 over 2982
meshes (each instance once per distinct mesh its base names); terrain march:
runtime, FO4CS measures it
```

`tree casters MOVE off zero (3446)` and `mesh casters MOVE off zero (80)` are
the two that make it a moving field and not a printed constant.

### 6.2 `lodgen_native_baseline.sh --check` — the brief's 25 / 0

```
25 files in the baseline, 25 baked, 0 differ
  ok   every stock file byte-identical to the baseline
  ok   profile matches the baseline (ao=1 identity=1 arrays=1 atlas=1 slot-fallback=dim16 exclude=BTR)
RESULT PASS
```

The baseline was written on the **2026-09-10 03:57:46** exe and this lane's exe
is 2026-09-16 13:52:56 — a different binary, six days and several lanes apart,
and the stock bake did not move a byte. That is the point of the gate and it is
the strongest single statement that the whitelist change reached nothing else.

### 6.3 The decoder on every native fixture, Boston included

The synthetic fixture (69/0) and the real Sanctuary pair (87/0) run inside
`lodgen_native.sh` above. Two more were run by hand because the brief names
them:

| pair | checks | failures | cell-rule reading |
|---|---|---|---|
| `(-32,0)` dim 32 (the bucket-cap chunk) | 12 | 0 | `cellQuantAmbiguous 53`, worst **0.062501 u** (band 0.126955) |
| **Boston** `(0,−12)…(11,−1)` dim 4, 18 chunks | 6 | 0 | `cellQuantAmbiguous 14`, worst **0.062501 u** (band 0.126955) |

Boston is 6 checks and not 12 because it was baked **without** `--identity`, so
the three manifest checks and their three companions have no manifest to read
and are not run. That is stated rather than left as a smaller number a reader
has to explain: the six that ran are the container and cell-rule checks, which
are the ones the new rule lives in.

The worst quantisation distance is the same **0.062501 u** on both, on two
regions that share no geometry — which is what the rule predicts, because the
number is a property of the 0.2500038 u grid step and not of the content.

### 6.4 `resource_ext.sh` — 12 / 0, and RED on the binary it replaces

```
this exe    12 checks, 0 failures, 0 skipped   RESULT PASS
the rung    12 checks, 5 failures              RESULT FAIL
```

A gate that is green on the exe it replaces is measuring something else. This
one goes red on five of its twelve there, by name: `.lodm` not served, `.pbrm`
not served, the index holding 17 instead of 20, and the two legs that compare
the two indexes.

### 6.5 `lodgen_defaults.sh` — exit 2, and why it is not counted

```
no rung at /e/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_defaults1.exe
RC=2
```

It did not run a single check. The spell compares the exe under test against a
**pre-2026-09-12 rung binary that no longer exists on disk**, and lane
DEFAULTS1's change is uncommitted, so the rung cannot be rebuilt from git
either. This is the director's named known red and is **not** this lane's: the
brief's "28/0" cannot be produced by the spell as written, and no baseline was
invented in its place.

*(The director's ROW B, added 13:5x, is the instruction to rewrite this spell so
it needs no rung. Its result is in §11.)*

### 6.6 `lodgen_terrain_vt.sh`, both arms — 45 checks, 0 failures

**Before this lane touched it: 44 checks, 1 failure** (`gate_vt_before.txt`) —
V9c, the director's named known red. **After: 45 checks, 0 failures, RESULT
PASS** (`gate_vt_after.txt`).

The count went 44 → 45 because this lane added **one** check, `V23`, the item-3
pin that `--vt-height` is off by default. The other 44 are the spell as the
director counted it, and all 44 are green — including the rewritten V9c, which
is ROW A and is written up in §11.

```
       E/W seam  28.516 interior  28.804 ratio 0.990 (bar 1.15)
       N/S seam  33.426 interior  25.488 ratio 1.311 (bar 1.45)
       edge step E/W  21.126 N/S  24.979, over the control 0.733 / 0.980 (bar 1.20)
       REFUTER neighbours shifted 16 texels: E/W 1.330 (bar 1.15), N/S 1.602 (bar 1.45) -- 2 of 2 break
  ok   V9c ... under a BC3 decode ...
       pyramid bytes: default 3762048, with --vt-height 7651072
       ratio 203/100, bar 180/100
  ok   V23 --vt-height is OFF by default and the flag is what turns the height layer on
```

### 6.7 Item 3's second half, now closed

The brief owed two things on the height row beyond writing it: that the panel
row and the CLI agree, and that the default is OFF and pinned.

- **The panel side was already gated** and this lane did not need to add to it.
  `lod_generation.sh` carries lane PANEL1's group check over 57 rows, and
  `LodgenVtHeightCheck` is one of them (`src/nifskope_ui.cpp:28673`, key
  `vtHeight`). The run reads: *"new rows checked: 57, missing: 0, outside the
  scroll area: 0, taking the wheel unfocused: 0, not round-tripping through
  QSettings: 0, without a tooltip: 0."* The row is present, unticked
  (`src/lodgenmanager.cpp:1279`, the literal is `false`), round-trips its own
  settings key and carries the tooltip that names `--vt-height`.
- **The two sides meet at ONE field.** The panel writes
  `o.height = xb( "vtHeight" )` (`src/lodgenmanager.cpp:2504`) and the CLI writes
  `lgVt.height = true` (`src/nifcli.cpp:6983`). One key, one meaning — there is
  no second path for the two to disagree on.
- **The default is now pinned by a number**, V23 above: the pyramid the default
  writes is **3,762,048 B** and the pyramid `--vt-height` writes is
  **7,651,072 B**, a ratio of **2.03** against a bar of **1.80**. The bar is
  under the contract's own 2.333 (a tile 138,720 → 323,680 B) and far above
  1.00, so a build that carried the layer by default would read about 1.00 and
  go red, and so would one that ignored the flag.

## 7. Build and chain

The chain is `nifskope-ww-build-verify`, run as written.

**Before the build**, per the standing rule: `tasklist` showed **no
`Fallout4.exe` and no `NifSkope.exe`**. Nothing had to be renamed aside and no
`BUILD PENDING` was written. The same check was made again before every bake in
this lane.

| reading | value |
|---|---|
| `BUILD-RC` | **0** |
| exe at launch (VT1's) | 22,293,504 B, 2026-09-16 **12:49:19** |
| exe after this lane | **22,300,160 B, 2026-09-16 13:52:56** |
| sha256 | `4ad6ee0c4c844600cf361149c6b00fcdb6aaeeebd8bfec2905f596912a692ee0` |
| objects rebuilt | **6**: `ba2file, lodifile, lodinative, lodofile, nativeemit, nifcli` |
| stale objects for `src/lodifile.h` | **0** |
| exe newer than every file in `git status --porcelain -- src res tools tests` | yes |
| `cmp res/style.qss release/style.qss` | in step (both 11,097 B, 13:52:56) |
| `make -n` compile lines after the build | **0** |
| separate gate-driver exes needing a rebuild | none |

Six objects is the right number and is checked rather than assumed: `ba2file`
is the whitelist, `lodifile` the cell rule, `nativeemit` the caster counts,
`nifcli` the flag table, and `lodinative` / `lodofile` are the two that include
`src/lodifile.h`, which this lane changed. `make -n` printing zero compile lines
afterwards is the statement that nothing was left half-built.

**Git state was not touched.** No commit, no stash, no checkout, no branch. The
only git commands run were `git show HEAD:<path>` and `git status --porcelain`,
both read-only, and they are named where their numbers are used.

## 8. Owed / red / bungo's calls

### 8.1 Red that is NOT this lane's, by name

| what | where | whose |
|---|---|---|
| `lodgen_ground_cover.sh` C2 ×3, C6a, C9, C16 | the ground-cover feature | not this lane's — see §11 ROW C |
| `native_open.sh` one object-coverage failure | pre-existing | the director's named known red |

`lodgen_terrain_vt.sh` V9c **was** on this list at the start of the lane. It is
not any more: ROW A rewrote it and it is green (§11).

`lodgen_defaults.sh` **was** on this list as "exits 2 for want of a rung". It is
not any more: ROW B made it rung-free (§11).

### 8.2 Owed, named rather than done

1. **`src/lodgen.cpp:3919` drops placements in silence.** The stock path's
   16-bit bucket cap loses 2,628 of 42,560 on chunk (-32,0) and counts it
   nowhere. The honest fix is one counter and one census clause. It is a
   generator lane's item and it wants bungo's call on refuse / warn / split.
   Measured in §5; no code was changed, because the brief says to change code
   only if the NATIVE path drops, and it does not.
2. **`--vt-height` has no census word.** The `vt:` line carries 90-odd fields
   and not one of them says whether the height layer was written. V23 now pins
   the behaviour by the pyramid's byte count, which works but is indirect; a
   `vtHeight 0|1` field in the census line would be the direct statement, and
   the census page's own rule 1.2 ("every shipped field is WRITTEN and MOVES")
   argues for it.
3. **`docs/LODGEN_NATIVE_LODO_LODI.md` §9 has no provenance line** for the
   2,628 / 6.17% figure. The number is unchanged and correct and this lane did
   not edit §9; what it lacks is a line naming where the figure came from and
   when it was last re-run. The director may want the 2026-09-16 re-run added.
4. **The two rung-dependent refuters dropped from `lodgen_defaults.sh`** are
   named in §11 ROW B. Each was a statement about the OLD binary's behaviour,
   and nothing on this exe can stand in for them.

### 8.3 A deviation from the brief's literal wording, already flagged

The per-source caster count went on its own prefix `native-casters:` rather than
into the `native:` sentence, for the reason given in §4.3. One line to fold in
if the director wants it the other way.

## 9. Mistakes

Eight, written up in full in `MISTAKES_ENTRIES.md` and spliced at the top of
the root `MISTAKES.md` (437,794 → 445,580 B, CR 0 → 0, LF 7,413 → 7,539).
In short:

1. **Edited three untracked files without measuring them first**, and so cannot
   give a before column for two of fourteen rows in `CHANGED_FILES.txt`. This is
   the worst of the seven because the brief asked for exactly that number.
2. **Used a quoted heredoc with backslashes in it** from Git-Bash, which a repo
   skill (`ww-shell-heredoc`) already warns collapses them. The skill was read
   after the failure instead of before the write.
3. **Wrote a curly apostrophe into a contract page**, then lost an edit to an
   anchor that used the ASCII one.
4. **Wrote a gate leg that would pass on the binary it was refuting** —
   `resource_ext.sh` leg 4 compared a difference instead of two absolutes.
5. **Claimed eight re-derived constants including `hkx`**, which is not in the
   switch at all. Corrected to the 19 that are.
6. **Ran the object bake three times with wrong flags** and read the empty
   output as a result.
7. **Rewrote `lodgen_defaults.sh` phase (b) so it could only run after phase
   (a)**, in a spell whose phases are selectable with `PHASES=`. Green on
   the full run, broken on `PHASES=b`. Found by re-reading the file rather
   than trusting the verdict; fixed after the run finished, and the spell
   re-run to the same 28 / 0.
8. **Put backticks inside a double-quoted `python -c` and let bash eat three
   spans of the text being written** -- while writing entry 7 above. Entry 2
   in a second costume.

Three of the eight (2, 6 and 8) repeat a lesson a skill or a previous lane's entry had
already written down. That is the pattern worth the director's attention, more
than any one of them.

## 10. Skill review

Invoked before choosing inputs or routes, as CONSTITUTION requires. The repo's
skills are at `<repo>/.claude/skills/`, **not** `~/.claude/skills` — the first
attempt to list them appeared to succeed only because `2>/dev/null` hid the
failure on the second path, which is itself worth a line here.

| skill | what it changed about this lane |
|---|---|
| `ww-spec-gate-audit` | Stopped §5 from re-quoting a number. It sent the lane to the SOURCE of the 2,628 figure, which is how the `--identity` opt-in trap was caught: the measurement as written would have read zero on a default profile and reported "no drops". |
| `ww-shell-heredoc` | Correct, and read too late. Every patch after the failure went in as a file written with the Write tool, and none failed that way again. |
| `ww-contract-provenance` | Every number in the `--vt-height` row was re-derived from the anchor beside it rather than copied from prose. |
| `ww-census-field` | The six rules of census page 1.2 are why the two runtime shadow words read `runtime — FO4CS times it; no bake number exists` instead of being left blank, and why the plan's ms figures are carried labelled ESTIMATE. |

**Worth writing, and not written here** (the lane was already over its rows):
a skill for *freezing a baseline file*. Three different gates in this tree want
one (`lodgen_native_baseline.sha256`, `lodgen_ground_cover.sha256`, and the
`stock_baseline` profile header), each with its own format, its own
precondition and its own re-freeze rule, and ROW C had to read a harness to
work out which. The procedure is identical every time: check the precondition
gate passes, record the exe's size / mtime / sha256 in the header, say WHY that
exe, write the hashes, re-run the gate and show the check go green.

## 11. The three added rows (director, 2026-09-16 13:5x)

The brief's five rows were finished first, as instructed. All three added rows
are done as written; none had to be approximated. Same gates, same deliverable
rules, everything in this folder.

### ROW A -- `lodgen_terrain_vt.sh` check V9c

**The diagnosis was right, and there were two faults, not one.**

*Fault 1, the reader.* The check walked the `_msn` sheets in **8-byte blocks**.
The files are **DXT5 (BC3)**: fourCC `DXT5`, 512x512, 10 mips, 349,680 B, and a
BC3 block is **16 bytes** -- 8 bytes of interpolated alpha (a0, a1, six bytes of
3-bit indices) then 8 bytes of DXT1-shaped colour. From block 2 onward the old
reader was decoding alpha bytes as colour endpoints. It also applied the DXT1
`c0 <= c1` punch-through rule, which BC3 does not have: BC3 colour is **always**
the four-colour interpolation.

*Fault 2, the bars.* They had been pinned on sheets this fork generated. All
four chunks now **copy vanilla's normal file whole** -- the census line reads
`msnCopied 5 msnOurs 0`, and a byte compare against vanilla's shipped files is
identical on all four. So the bars were measuring data that is no longer
produced.

**What it does now.** A re-typed BC3 decoder in the harness's own Python, with a
format guard in front of it: it refuses by name if the fourCC is not `DXT5`, and
refuses if mip 0 does not fit at 16 bytes a block. Bars re-pinned on vanilla's
normals, each at the **geometric midpoint** of the true reading and the same
sheets shifted 16 texels, so the bar sits a fixed factor from both sides rather
than being drawn around the number that was wanted:

| bar | value | how it was set |
|---|---|---|
| `RATIO_EW` | **1.15** | sqrt(0.990 x 1.330) = 1.147 |
| `RATIO_NS` | **1.45** | sqrt(1.311 x 1.602) = 1.449 |
| `EDGE_MAX` | **1.20** | a sheet's own last-two-columns step over its control |
| `CTL_LO..CTL_HI` | **20.0 .. 36.0** | the interior control band under a BC3 decode |

**Readings on the current exe:**

- E/W seam 28.516, interior control 28.804, **ratio 0.990** against 1.15.
  VT1 measured 0.99 against a bar of 3.20 -- this lane reproduces VT1's number
  to two decimals, independently.
- N/S seam 33.426, interior control 25.488, **ratio 1.311** against 1.45.
- edge steps 21.126 and 24.979 -- **0.733** and **0.980** of their controls,
  against 1.20.
- The interior control band 20.0..36.0 excludes the OLD DXT1 reader's own
  readings (13.243 and 12.182) by a factor of 1.5, so a build that went back to
  the wrong decoder goes red on the control alone, before the seam is looked at.

**Shown failing on broken input, once, in the same check.** The refuter is
folded into V9c rather than added beside it, so ROW A on its own does not move
the check count: the same two seam measurements are re-run against neighbours
shifted 16 rows / 16 columns, they read **1.330** and **1.602**, and both must
break their bar. `broke < 2` fails the check with `the shifted sheets pass the
same bars, so the bars are accommodating the data instead of measuring it`.

**Count.** `lodgen_terrain_vt.sh` now ends **45 checks, 0 failures**. The
director's floor was "44 checks 0 failures on the current exe": the spell's 44
are all green -- V9c included -- and the 45th is `V23`, the `--vt-height`
default pin this lane added for brief item 3. Nothing is red in this spell.

**Picture:** `v9c_dxt1_vs_bc3.png` (1352x522) -- the two sheets meeting at their
chunk seam, 160 texels either side, decoded as DXT1 on the left (vertical-stripe
garbage) and as BC3 on the right (terrain normals), with both sets of readings
burned in.

### ROW B -- `lodgen_defaults.sh` is rung-free

**28 checks, 0 failures, RESULT PASS, RC=0** -- the brief's 28/0, with no rung.
It previously exited **2** without running a single check, for want of
`release/NifSkope.before_defaults1.exe`, which is not on disk and cannot be
rebuilt from git because DEFAULTS1 was never committed.

The three shapes are stated in the file's own header so a reader can check the
logic rather than the wording:

1. **SAME** -- the DEFAULT bake is byte-identical to a bake with every ruled
   switch spelled out. `(a)` and `(a2)` both green.
2. **DIFFERENT** -- a bake with the OLD values spelled out differs, and the
   files that moved are named: `Commonwealth.4.-20.24.BTO` (1,280,239 vs
   860,743 B), `Commonwealth.4.-20.24.BTR` (46,518 vs 36,942 B),
   `tex/Commonwealth.4.-20.24.DDS` (174,888 vs 174,888 B, same size, different
   bytes). `files that moved: 1 .BTR, 1 .BTO` -- both switches reach their file.
3. **REFUTER** -- `(a) refuter: ONE wrong explicit switch (--land-hex 0) breaks
   the same comparison`, which is shape (1) shown failing once.

**`.lodb` is the one excusable difference**, and the reason is in the file: the
ledger hashes the **switch list**, so a defaulted bake and a spelled-out bake
can never agree there by construction. `lodbcmp` therefore compares the ledger
*contents* instead -- same files, same hashes, input hashes equal, switch hashes
deliberately NOT equal -- which is a stronger statement than excusing the file.

**The two checks dropped with the rung, by name**, each replaced one for one so
the count does not move:

| dropped | why it cannot exist | replaced by |
|---|---|---|
| "the rung with `--no-identity` wrote NO manifest" | it is a statement about the OLD binary's behaviour, and there is no old binary | `(a)` refuter: a deliberately wrong explicit switch (`--land-hex 0`) must break the comparison the right ones pass |
| "on the OLD code the flag DID thin the native data" | same | `(e)` `--native-no-ladder` must move 2 of 2 native files on THIS exe |

**Losing the rung made the gate tighter, not looser.** The `.manifest.txt`
sidecar used to be excused from the `(a)` comparison because the rung did not
write one. Both sides are the same exe now, so the sidecar is held to the byte
like every other file; only `.lodb` is excused.

Per-phase evidence: `(a2)` roads -- default == spelled out, ledger agrees file
for file and hash for hash, and `--road-ground-paint 1` bakes different bytes
from the ruled 0. `(c)` -- with identity spelled ON the descriptors read BTR
686095322853893 and BTO 5612388490686984, against vanilla's 52776558133763 and
the plain 474989027590661, at dim 4, 8, 16 and 32. `(d)` -- rows / C / I / M / A
and all 11 array files identical either way. `(e)` -- 2 native files identical
either way, `--native-verify` clean, `--native-no-ladder` moves 2 of 2.

*One repair after the first green run:* phase `(b)` compared `b_old` against
`$W/a_new`, which phase `(a)` bakes, so `PHASES=b` alone would have had no
left-hand side. It now bakes its own `b_def` default side. The file was not
edited while bash was executing it -- bash reads a script incrementally and an
in-place edit can corrupt a live run -- so the fix went in after the run
finished, and the spell was re-run afterwards to the same 28 / 0.

### ROW C -- the ground-cover baseline is frozen

**`tests/spells/lodgen_ground_cover.sha256`** now exists: NEW file, **2,651 B,
CR 0, LF 43**, LF-only. Its format was read out of the harness rather than
guessed -- `sha256  name` lines over the `--no-cover` bake, with `#` comments
allowed.

**The frozen reference is `release/NifSkope.exe`, 22,300,160 B,
2026-09-16 13:52:56, sha256
`4ad6ee0c4c844600cf361149c6b00fcdb6aaeeebd8bfec2905f596912a692ee0` -- this
lane's own rebuilt exe, not the 22,293,504 B exe on disk at launch**, and the
header says so and says why. This lane's brief rows changed the bake (the
loose-extension whitelist and the native emitter both sit in the bake path), so
the launch exe is not the exe the gate will be run against tomorrow. Two
independent measurements are recorded in the header showing the stock
`--no-cover` bake is not lane-specific.

`lodgen_identity.sh` was run **first**, because the harness names it as this
baseline's precondition: **RESULT PASS**. The header also carries the re-freeze
procedure, so the next person does not have to read the harness again.

**Gate result: `lodgen_ground_cover.sh` went from 29 checks / 5 failures to
29 checks / 4 failures**, with `ok C1 the --no-cover bake matches the frozen
baseline` -- the check the missing file had always failed.

The three hashes frozen:

```
05ee9489e68d03bf0c3f41df841c53661faec4324d70f5b4bfaeeddf4658a0b2  Commonwealth.4.-20.24.DDS
5b5e55e3279c0fc78169490c363ef46f1a0651247dde2e3b0977b3b40fff674b  Commonwealth.4.-20.24_msn.DDS
6a91bdfd9bf5b9bc03e43d68ccfd7d7a57fb806fedd1343824b3f87a63bb413a  Commonwealth.4.-20.24_data.DDS
```

**The four failures that remain are pre-existing, are about the grass feature
itself, and are NOT this lane's** -- none of them reads the baseline file:

- **C2, three times.** The control chunk `Commonwealth.4.0.0` is supposed to be
  grass-free, and on this exe it reads `coverMax=69`. So the "nothing moves with
  `--cover`" trio fails, and the control's sheet turns DXT5. Either the control
  chunk needs re-choosing or ground cover is reaching a chunk it should not --
  a grass-feature question, and bungo's or the ground-cover lane's call.
- **C6a**, **C9** and **C16**, inside the C3..C17 rollup.

They were red before the baseline was frozen and they are red for the same
reasons after it. Freezing the baseline neither fixed nor caused them.
