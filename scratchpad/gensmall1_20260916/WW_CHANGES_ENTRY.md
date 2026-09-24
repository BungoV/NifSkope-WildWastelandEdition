## 2026-09-16 -- lodgen: loose `.pbrm` / `.lodm`, the shadow-caster census, the cell rule, the height row (lane GENSMALL1)

### Loose-file whitelist: `.pbrm` and `.lodm`

`--resource <dir>` now serves `.pbrm` and `.lodm` from a loose folder. The
vendored `BA2File` decides which loose extensions to index by packing the
extension six bits a character into a `quint64` and switching on the value;
anything not in the switch is skipped **in silence**, so the two Wild Wasteland
material sidecars were invisible to a loose run while the same two extensions
inside a `.ba2` were served normally.

Two `case` values were added, in the ascending order the switch was already in:
`0x2CBE4B40000000ULL` (`lodm`, after `"kf"`) and `0x308B2B40000000ULL`
(`pbrm`, after `"nif"`). Before computing them, all **nineteen** values already
in the switch were re-derived from the same packing expression and required to
reproduce the bytes on the line: **19 of 19, 0 mismatches**. Case folds, so one
constant covers `.PBRM` and `.pbrm`.

Nothing else moved: a stock bake of (-20,24) dim 4 under this exe and under the
pre-change exe is byte-identical file for file, and
`lodgen_native_baseline.sh --check` reads 25 files, 25 baked, **0 differ**
against a baseline frozen on the 2026-09-10 exe.

**New gate `tests/spells/resource_ext.sh`** (12 checks): every one of the
nineteen extensions served by name, `.zzz` still refused, the case fold, the
index holding exactly 20 files, the byte-identity bake, and a refuter leg that
goes **red on the exe this one replaces** (12 checks, 5 failures there).

### The census counts shadow casters per source

Under bungo's 2026-09-11 ruling that every placement has exactly one shadow
representation at a time, and that the census counts casters per source, the
native bake now writes a per-source caster count. Each instance goes in exactly
one of four bins by a stated priority -- **tree > card-only > mesh > none** --
so the four sum to the instance count, and the line says so itself:

```
native-casters: per source, of 3526 instances: tree 3446, card 0, mesh 80,
none 0; sum 3526 == instances 3526 == AGREE; mesh-slot casters 5958 over 2982
meshes (each instance once per distinct mesh its base names); terrain march:
runtime, FO4CS measures it
```

`none` is a fault count, not a zero. `mesh-slot casters` counts each instance
once per **distinct** mesh its base names, which is the granularity a per-mesh
column needs.

It is written in three places, because a census word that lives in one of them
is not a contract: the bake line above; the `--native-mesh-report` sidecar as a
new `casterInstances` column (**version 3 / 25 columns -> version 4 / 26**); and
the contract pages. `docs/LODGEN_CENSUS.md` 5.3 gains `shadowCasters[src]`,
`shadowMarchMs` and `shadowMapMs`, 6.1 gains the `native-casters:` line and 6.2
the cross-check that binds the runtime rows to the bake bins;
`docs/LODGEN_NATIVE_LODO_LODI.md` 3.4 is corrected to 4 / twenty-six.

The two **ms** words read `runtime -- FO4CS times it; no bake number exists`,
stated in the row rather than left blank. The plan's figures (march 0.3-0.8 ms,
map 1-3 ms) are carried labelled **ESTIMATE** until a capture round measures
them.

Four new checks in `lodgen_native.sh` hold it, including two that require the
tree and mesh counts to MOVE off zero on a region that has both.

### The far-object reader checks the cell rule

`lodiVerify` now checks that each instance's stored cell agrees with its stored
position. The position is three `u16` over the 16,384-unit chunk box, so the
step is 16384/65535 = 0.2500038 u and `std::lround` bounds the error at
0.1250019 u; an instance within that band of a cell line is reported as
**ambiguous** rather than failed, and anything outside it is a real
disagreement. New in `src/lodifile.h`: `LODI_POS_QUANT_STEP`,
`LODI_CELL_QUANT_TOL` and `lodiCellAgrees()`. **The writer is untouched.**

Measured on new data: chunk (-32,0) dim 32 reads 53 ambiguous, worst
0.062501 u against a band of 0.126955; downtown Boston (0,-12)...(11,-1) dim 4
reads 14 ambiguous, worst **the same 0.062501 u** -- which is what the rule
predicts, because the number is a property of the grid step and not of the
content. Zero real disagreements on either.

### `--vt-height` is in the CLI table, and its default is pinned

The flag was implemented, documented in prose in 2.2 of the VT contract page,
and **missing from the CLI table in 5** -- which for a user is the same as not
existing. The row now states: default **off**; a fourth R16_UNORM height sheet
per tile on the same tile grid and border; why it is off (uncompressed where the
others are BC1, so a sheet is 184,960 B against 46,240 B and a tile goes
138,720 -> 323,680 B); the exact way back (do not pass it); and the panel row it
corresponds to, **Terrain -> Carry a height layer**, also unticked by default.

**New check `V23` in `lodgen_terrain_vt.sh`** pins the default by a number: the
pyramid the default writes is 3,762,048 B and the one `--vt-height` writes is
7,651,072 B, a ratio of **2.03** against a bar of **1.80**. A build that carried
the layer by default would read about 1.00 and go red.

### The asymmetric drop, measured, and what it is owed

No code change -- the native path does not drop, which is what the brief says to
do in that case. The stock claim reproduces to the digit on today's exe:
**2,628 of 42,560 placements (6.17%)** carry no geometry on chunk (-32,0) dim 32,
four shapes saturated at 65,535 / 65,534 / 65,529 / 65,424 vertices, first
missing index 38,695, **68.0%** of everything after it missing. Measured twice,
once in single-chunk mode (85 shapes) and once in region mode after the merge
(62 shapes), with identical results -- **the merge recovers nothing**. All 2,628
are `STAT`; one base loses 851 by itself and ten bases account for 2,542.

The spec calls its own figure a lower bound; on this chunk it is **exact**,
because the manifest's 452 `I` lines name 39,840 placements and the greatest
number of models for any one placement is 1.

The native path, same chunk, same run: `instances 42560 from 42560 arrivals ...
0 dropped`, and an independent decoder agrees at 42,560 rows, 0 not in the
table.

**Owed, not done:** `src/lodgen.cpp:3919` still drops in silence. It wants one
counter and one census clause, and it is a stock-path change, so it is bungo's
call whether that path should refuse, warn, or split the bucket.

### Three test-harness repairs

- **`lodgen_terrain_vt.sh` V9c** decoded the `_msn` sheets with a **DXT1**
  reader. They are **DXT5**, and its bars had been pinned on sheets this fork
  generated while all four chunks now copy vanilla's normal file whole
  (byte-identical, measured). Rewritten with a BC3 decoder and re-pinned on
  what is on disk: E/W seam ratio **0.990** against a bar of 1.15, N/S **1.311**
  against 1.45, each bar the geometric midpoint of the true reading and a
  16-texel shift. The refuter runs in the same check: the shifted neighbours
  read 1.330 and 1.602 and break **2 of 2** bars.
- **`lodgen_defaults.sh`** compared against a rung binary that no longer exists
  and could not be rebuilt, so it exited 2 without running a check. It is now
  **rung-free**: it proves "the defaults ARE the switches" on the exe under test
  alone, by spelling the switches out. Two rung-only refuters were dropped by
  name and replaced one for one, so the check count is unchanged.
- **`tests/spells/lodgen_ground_cover.sha256`** now exists -- the frozen
  `--no-cover` reference the gate's C1 check has always wanted. Written from
  release/NifSkope.exe 22,300,160 B 2026-09-16 13:52:56, after
  `lodgen_identity.sh` passed on the same exe.

`lodgen_terrain_vt.sh` now ends **45 checks, 0 failures** (44 before, with V9c
red; the 45th is the new V23).
