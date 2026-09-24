# The Improved LOD census — the counters the runtime prints, and the bake numbers they are checked against

> **STATUS: SPEC. NOTHING WRITES THESE ROWS YET.**
> Improved LOD is the FO4CS module that consumes `.lodo`/`.lodi`, the `.lodt`
> pyramid, the card sheets and the mesh texture arrays (bungo 2026-09-09 15:34:
> *"This will be a new module for Fo4cs, called Improved LOD"*). Its runtime does
> not exist yet. This page is the contract FO4CS implements against, so the
> module is **measurable on its first flight instead of argued about** — which is
> the whole of bungo's ruling on gap (4), 2026-09-11 10:0x, verbatim:
> **"We need them"**.
>
> The second half of the page (§6) is not a spec: it is an inventory of the
> census the generator **already prints today**, measured off real bakes, so the
> runtime's numbers have something to be checked against. §7's checker proves
> that half against the files themselves.
>
> This page carries **no `src/` line numbers**. Lane BAKEPERF1 owned `src/` while
> it was written, and a line number taken from a file another lane is editing is
> a rumour by the time it is read (`ww-contract-provenance`). Every claim about
> the bake census is anchored to the census line's own leading word and to a
> frozen log artefact, both named in the provenance footer.

---

## 1. The rows

Seven row kinds, each a `[Tag] key=value` line in the FO4CS log, one set per
frame-window. They are split because one line cannot carry sixty fields without
truncating silently (`fo4cs-census-field`, property 4).

| tag | what it answers |
|---|---|
| `[ImprovedLOD]` | **which arm is serving, and are the files the user's own** — the row every reader hits first |
| `[ImprovedLOD.Ring]` | instances considered, culled three ways, drawn, and triangles — per distance bin |
| `[ImprovedLOD.Cluster]` | the cluster cut: the tolerance in force, what it selected, whether it was a partition |
| `[ImprovedLOD.Cards]` | impostor cards drawn |
| `[ImprovedLOD.Residency]` | tiles and bytes held resident, against their budgets |
| `[ImprovedLOD.Shadow]` | the far-shadow pass — a **second complete consumer** of the same data |
| `[ImprovedLOD.Fade]` | the screen-size fade thresholds, the hysteresis, the cross-fades in flight |

**`[ImprovedLOD]` carries the answer or points at the row that does.** A reader
who quotes a drawn count without reading the arm row has quoted a number from a
fallback arm as though it came from the native one.

### 1.1 A "ring" here is a CENSUS BIN, never a selection rule

`docs/LODGEN_NATIVE_LODO_LODI.md` §4.4 forbids selecting geometry by ring: the
measured 172x spread of bound heights inside one chunk makes a per-chunk distance
wrong by two orders of magnitude for most of its contents. Selection is per
cluster, by screen error.

The rings survive **only as the bins these rows are reported in**, because that is
the vocabulary the stock bake and bungo both use: the loaded 5x5 grid is full
models, ring 0 is the first band outside it (the stock bake's dim 4), ring 3 is
the horizon (dim 32). `ringEdges` states the four distances in force, so a reader
can tell a redistribution from a regression. A per-ring number printed without
`ringEdges` beside it is unreadable and the field refuses.

### 1.2 The rules every field below obeys

The three rules of 2026-09-04 21:33 (CONSTITUTION 4), and the five properties of
`fo4cs-census-field`:

1. **WRITTEN and MOVES.** No field ships without a test that something assigns it
   and that it changes when the thing it measures changes. The **moves** column
   of every table below IS that test, stated as the scene change FO4CS must
   drive; the lane that ships the field must show the case red with the
   assignment removed.
2. **One key, one meaning, one occurrence per row.** A key is never emitted
   twice, and a retired field's word is deleted rather than left printing.
3. **The composed row fits its buffer**, asserted against the exported capacity
   constant and driven at the SATURATED case, never the quiet one.
4. **A refusal names its reason in words.** Never `none` where a word could say
   why. The refusal column below is the complete list per field; anything not on
   it is a defect.
5. **A default accuses its own plumbing.** `uncounted`, `unread`, `unset`,
   `unchecked`, `unwired` and `unmeasured` are the six defaults used here, and
   every one of them reads as a fault. `unmeasured` is the default of a runtime
   TIMING (the §5.3 `shadowMarchMs` / `shadowMapMs`): FO4CS's timer never wrote
   the field, and no bake number exists to stand in for it. **`0` is never a default** except where zero is a
   measurement (`frames`, `engineFarHidden`), because a quiet zero composes into
   a row that looks like a healthy quiet build.
6. **A metric on no data refuses as `n/a`**, and any counter whose unit assumes
   how often a hook fires prints `frames=` beside it, so the row makes its own
   unit falsifiable.

### 1.3 How to read the "read from" column

`NATIVE 4.1` means section 4.1 of `docs/LODGEN_NATIVE_LODO_LODI.md`. The page
keys are `NATIVE`, `VT` (`docs/LODGEN_TERRAIN_VT.md`), `CARDS`
(`docs/LODGEN_CARD_SHEETS.md`), `ARRAYS` (`docs/LODGEN_TEXTURE_ARRAYS.md`),
`MANIFEST` (`docs/LODGEN_MANIFEST_FORMAT.md`), `LODM`
(`docs/LODGEN_LODM_FORMAT.md`), `BTD` (`docs/LODGEN_BTD_FORMAT.md`, whose sections are
named, not numbered: `BTD Header`). Every one of those citations is checked to
resolve to a section that exists (§7, gate C3).

The word **`runtime`** in that column means the field is not read from any baked
file — it is a state of the FO4CS module or of the engine. It is written out as
`runtime` rather than left blank so that "we could not trace this" and "there is
nothing to trace" cannot be confused.

---

## 2. `[ImprovedLOD]` — the arm, the files, the staleness

Per CONSTITUTION 10, every arm ships with a fallback beneath it and **the output
names the serving arm**, so a fallback is never a silent downgrade. Improved LOD
has four independently switchable consumers and therefore four arm words.

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `arm.objects` | word | which arm draws far OBJECTS: `native` (the `.lodo`/`.lodi` pair) / `stock-fallback` (the engine's own `.bto` tree) / `off` | NATIVE 5, NATIVE 7 | rename the `.lodo` aside and reload: the word must read `stock-fallback` in the next window | `off`, `no_lodo`, `no_lodi`, `refused_hard`, `no_worldspace` | `unwired` |
| `arm.terrain` | word | far TERRAIN: `native` (the `.lodt` pyramid) / `stock-fallback` (the `.btr` chunk meshes) / `off` | VT 4, VT 2.4 | rename the VT `.lodm` index aside | `off`, `no_lodm`, `no_lodt`, `refused_hard` | `unwired` |
| `arm.cards` | word | impostor cards: `native` (card arrays) / `stock-fallback` (the card quads baked into the `.bto`) / `off` | CARDS 1.1, CARDS 1.2 | remove the card array set | `off`, `no_sheet`, `no_layer` | `unwired` |
| `arm.arrays` | word | mesh LOD textures: `native` (texture arrays) / `stock-fallback` (atlas plus loose copies) / `off` | ARRAYS 1, ARRAYS 6 | remove `<ws>.LodgenArrays*` | `off`, `no_array`, `no_uv2` | `unwired` |
| `worldspace` | editor ID | which worldspace every other row describes | NATIVE 3 | walk through a worldspace door; it must change with the cell load | `none` | `unread` |
| `version` | quad | the four far-field container versions actually loaded, `lodo<v>/lodi<v>/lodl<v>/lodt<v>`; a default bake today reads `lodo4/lodi7/lodl2/lodt2`. `.lodo` is 4 only; `.lodi` is 3..9 (7 by default, 9 with `--scrappable`, 8 retired but still read, 3..6 older bakes); `.lodl` is 1..3 (2 by default, 3 with `--water-bodies`); `.lodt` is 2 only. A file the arm does not use prints `-` in its slot | NATIVE 3, NATIVE 4, BTD Header, VT 3.1 | put a v3 `.lodo` in place: the row must print the refusal, not the version | `refused:lodo:v1`, `refused:lodo:v2`, `refused:lodo:v3`, `refused:lodi:v1`, `refused:lodi:v2`, `refused:lodl:v<n>` (outside 1..3), `refused:lodt:v<n>` (not 2), and `refused:<file>:v<n>` for any other unknown version | `unread` |
| `pairing` | word | the HARD pairing rule between the two files | NATIVE 4, NATIVE 5 | re-bake the `.lodo` and leave the old `.lodi`: `lodoIdentity` | `ok`, `lodoIdentity`, `loadOrderHash`, `worldspace` | `unchecked` |
| `stale` | count | files whose SOFT corpus hash disagrees with the user's own data | NATIVE 5, NATIVE 8 | install or remove any plugin and reload: it must become non-zero | `unchecked` when no hash was read | `unchecked` |
| `staleFiles` | list | WHICH file and WHICH hash moved, e.g. `[Commonwealth.lodo:objectCorpusHash]` — a count alone cannot be acted on | NATIVE 8, NATIVE 8.1 | same as `stale` | `none` (only valid when `stale=0`) | `unchecked` |
| `frames` | count | rendered frames in this window — the denominator every per-frame number below is divided by | runtime | stand still for one window: it must equal the frames the engine drew | — | `0` |
| `windowMs` | ms | the wall time this window covers | runtime | change the census interval | — | `0` |

**The soft/hard split is not this page's invention**, it is NATIVE 5: three of the
keys are hashes of the user's data and change the first time any mod is installed
after a bake, so the consumer **loads anyway, logs it and raises `stale`**, while
a version, a stride, a set reserved bit or a CRC failure refuses to load and the
arm word falls to `stock-fallback`.

---

## 3. `[ImprovedLOD.Ring]` — considered, culled three ways, drawn

Sixteen numbers a frame (four bins x four counters) plus the two totals. The
arithmetic is the strongest thing in this page and it is stated as a law:

```
ringInstances[r] == ringCulledFrustum[r] + ringCulledOccluder[r]
                  + ringCulledScreen[r]  + ringDrawn[r]        for every r
```

A row where that does not hold has lost instances between two counters, and the
module says so rather than printing a plausible set.

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `ringEdges` | units, four | the distances that BIN these rows (§1.1). Not a selection rule | runtime | change them in the INI: every per-ring number redistributes and the four totals do not move | — | `unset`, and every field below then refuses `no_bins` |
| `ringInstances` | count per bin | instances CONSIDERED in that bin this frame | NATIVE 4.1, NATIVE 4 | walk toward the far field: mass moves from bin 3 to bin 0 while the total holds | `no_bins` | `uncounted` |
| `ringCulledFrustum` | count per bin | rejected by the frustum test on `base.boundRadius x scale`, the chunk box expanded by `maxBoundRadius` | NATIVE 4.1b, NATIVE 7 | spin 180 degrees on the spot: this count and `ringDrawn` roughly swap | `no_bins` | `uncounted` |
| `ringCulledOccluder` | count per bin | rejected behind one of that cell's occluder boxes | NATIVE 4.5, NATIVE 4.5.1 | stand behind a fitted box in a cell that has one | `no_boxes` — this worldspace's `.lodi` carries `occluderCount` 0 | `uncounted` |
| `ringCulledScreen` | count per bin | rejected by the screen-size threshold of the instance's engine fade class | NATIVE 4.1b, NATIVE 4.4 | raise `fadeObjectsPct`: this rises and `ringDrawn` falls by the same number | `no_threshold` | `uncounted` |
| `ringDrawn` | count per bin | instances that reached a draw | NATIVE 7 | the inverse of every line above | `no_bins` | `uncounted` |
| `ringTriangles` | count per bin | triangles submitted for that bin, summed over the clusters the cut selected | NATIVE 3.2, NATIVE 4.4 | lower `clusterTolerancePx` toward 0: it climbs toward the level-0 total for the drawn set | `no_cut` | `uncounted` |
| `instancesTotal` | count | the sum over the four bins — checked against the `.lodi`'s own `instanceCount` (§4.2) | NATIVE 4 | load a second region's `.lodi`: it rises by that file's instance count | `no_lodi` | `uncounted` |
| `trianglesTotal` | count | the sum over the four bins | NATIVE 4.4.1 | as `ringTriangles` | `no_cut` | `uncounted` |

**`no_boxes` is a real refusal, not a hedge.** The nine-chunk Sanctuary region
writes **zero** occluder boxes and that is correct — it draws 41 distinct LOD
meshes and not one of them is watertight (NATIVE 4.5.3). A census that printed
`ringCulledOccluder=0` there would be indistinguishable from a broken occluder
test; `no_boxes` says which it is, and the number to check it against is in the
`.lodi` header.

---

## 4. `[ImprovedLOD.Cluster]` — the cut

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `clusterTolerancePx` | px | the ONE global pixel tolerance the cut runs at (default 1, bungo 2026-09-11 10:4x) | NATIVE 4.4 | set it in the INI | — | `unset`, and the cut refuses `no_tolerance` |
| `projectionScale` | px | the projection scale the cut actually used, recomputed from the LIVE camera. The reference constant is 1371.0 at 1920 wide and FOV 70 | NATIVE 4.4 | change resolution or FOV: it must move | — | `unread`. **A row printing exactly 1371.0 on a display that is not 1920 wide is the plumbing accusing itself** — the consumer took the page's reference constant instead of its own projection |
| `levelMax` | level | the deepest ladder level the loaded `.lodo` holds | NATIVE 3, NATIVE 3.5.3 | load a `--native-no-ladder` bake: it reads 0 | `no_ladder` | `unread` |
| `clusterSelected` | count per level | clusters the cut selected at each level 0..`levelMax` | NATIVE 3.5.3, NATIVE 4.4 | fly away from a building: mass moves from level 0 upward | `no_cut` | `uncounted` |
| `cutOverlap` | count | chains where MORE than one cluster was selected — the partition broken, geometry drawn twice | NATIVE 4.4 | must be 0; a doctored `parentError` in the file makes it non-zero | — | `unchecked` |
| `cutGap` | count | chains where NO cluster was selected — a hole | NATIVE 4.4 | must be 0; `parentError` is `FLT_MAX` at a root precisely so a chain always terminates | — | `unchecked` |
| `clusterTriangles` | count | triangles the cut produced this frame, all bins | NATIVE 4.4.1 | tolerance 0 must give exactly the level-0 triangle count of the drawn set | `no_cut` | `uncounted` |
| `draws` | count | MAIN-VIEW draw calls, i.e. buckets: (draw size class x family x arrayClass x arraySet x alpha state) | NATIVE 7 | walk into a dense cell: the bucket count moves. Measured expectation today is 8-14 mesh draws plus 1-2 card draws for a whole worldspace | `no_cut` | `uncounted` |
| `overflow` | count | appends refused because the append or indirect-args buffer was full | NATIVE 7 | shrink the buffer below `maxInstancesPerChunk`: it must rise off 0 | — | `uncounted` |

---

## 5. `[ImprovedLOD.Cards]`, `[ImprovedLOD.Residency]`, `[ImprovedLOD.Shadow]`, `[ImprovedLOD.Fade]`

### 5.1 Cards

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `cardsDrawn` | count per bin | card quads drawn in each distance bin | CARDS 1.1, NATIVE 4.4 | walk past a tree line | `no_cards` | `uncounted` |
| `cardsTotal` | count | card quads drawn this frame, all bins | CARDS 1.2 | as above | `no_cards` | `uncounted` |
| `cardBases` | count | distinct bases standing on a card this frame — a base carries a card only when `cardLayer` is not 0xFFFF | NATIVE 3.2 | enter a forested cell | `no_cards` | `uncounted` |
| `cardFrames` | count | how many of the N x N view grid's frames were sampled this window | CARDS 2, CARDS 3.6 | orbit a tree: it climbs toward N squared | `no_cards` | `uncounted` |
| `cardsAggregate` | count | ring-3 AGGREGATE cell impostors drawn, one per forested cell (bungo 2026-09-11 08:3x, *"1 sounds good"*) | CARDS 1.2 | — | `not_baked` — **no bake writes them yet**, lane CARDS-AGG owns it | `not_baked` |

**Today every card field must refuse, and the page says so rather than letting a
reader discover it.** NATIVE 11, Deviation 5: the bake writes what the stock ring
bakes and nothing more, so `cardLayer` is 0xFFFF on every base and
`cardCorpusHash` is 0. Until lanes OBJM / OBJC / OBJP fill those fields,
`arm.cards` reads `no_layer`, `cardBases` reads 0 and `cardsAggregate` reads
`not_baked`. A card census that printed four zeros here would look like a healthy
treeless worldspace.

### 5.2 Residency

Read once, deliberately not streamed for the library (NATIVE 7); the pyramid is
the part that churns. bungo's zoom answer of 2026-09-11 10:5x makes the finest
levels the thing a budget has to bind on, so every resident number here is
printed with its own denominator.

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `tilesResident` | count per level | pyramid tiles held resident at each `.lodt` level | VT 2.1, VT 4 | fly: the finest level churns, the coarsest does not | `no_pyramid` | `uncounted` |
| `tilesPresent` | count per level | the DENOMINATOR — tiles that level actually holds, from the index | VT 4 | fixed per bake; moves only on a re-bake | `no_pyramid` | `unread` |
| `tileBytes` | bytes per level | resident bytes per level | VT 3.3, VT 4 | as `tilesResident` | `no_pyramid` | `uncounted` |
| `libraryBytes` | bytes | resident `.lodo` geometry, finest levels first | NATIVE 1, NATIVE 7 | zoom a scope at a far building: the finest levels page in | `no_lodo` | `uncounted` |
| `libraryBudgetBytes` | bytes | the residency budget in force for the finest levels | NATIVE 7 | set it in the INI | `unbudgeted` — no budget is in force, so `evictions` can only read 0 | `unset` |
| `instanceBytes` | bytes | resident `.lodi` buffers: instances, cold records, chunk directory, and the derived `chunkIndex` the file does not carry | NATIVE 4, NATIVE 7 | load a denser worldspace | `no_lodi` | `uncounted` |
| `cardBytes` | bytes | resident card sheets and card arrays | CARDS 1.2 | enter a forested worldspace | `no_cards` | `uncounted` |
| `arrayBytes` | bytes | resident mesh LOD texture arrays | ARRAYS 1, ARRAYS 3 | load a worldspace with a second array set | `no_arrays` | `uncounted` |
| `residentBytes` | bytes | the sum of the five above — the one number a memory complaint is answered with | NATIVE 7 | any of the five moving | `uncounted` | `uncounted` |
| `evictions` | count per window | resident items dropped because a budget bound | NATIVE 7, VT 4 | lower a budget until it bites. **0 with a budget set means the budget never bound**, which is why `libraryBudgetBytes` prints beside it | — | `uncounted` |

### 5.3 Shadow — a second complete consumer

NATIVE 7 step 8: *"Quote the main-view draw count and the shadow draw count
together."* A shadow row that is a sentence rather than a count is the defect
this group exists to prevent.

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `shadowTolerancePx` | px | the far-shadow view's OWN cluster tolerance, coarser than the camera's | NATIVE 4.4 | set it: `shadowTriangles` falls | — | `unset` |
| `shadowClusters` | count | clusters the shadow cut selected | NATIVE 4.4 | move the sun | `no_cut` | `uncounted` |
| `shadowTriangles` | count | triangles submitted to the far-shadow pass | NATIVE 3.4, NATIVE 4.4 | raise `shadowTolerancePx` | `no_cut` | `uncounted` |
| `shadowDraws` | count | draw calls in the far-shadow pass | NATIVE 7 | as `draws` | `no_cut` | `uncounted` |
| `shadowIdentityUnique` | 0/1 | whether every far caster drawn this frame carried a unique **GROUP** (`.lodi` v7, NATIVE 4.9) — **re-worded by the director 2026-09-18 17:4x, and the change is a change of MEANING, not of wording**. The far-shadow pass keys on the caster identity and excludes self-shadowing by it (bungo 2026-09-11 08:4x), so the unit counted here must be the unit that must not shadow itself: one house, one SCOL, one tree. Two different groups colliding on one id is a wrong shadow; the 205 pieces of one kit house sharing a group is the point, and under the old per-placement reading this word would have called that correct case a collision 205 times over. On a file with no group table (v3–6) it falls back to the per-placement instance index and **says so**, because a silent fallback would make a v6 file read like a v7 one. The instance index and `cold[i].identity` are NOT the shadow key (NATIVE 4.1c) | NATIVE 4.9, 4.1c | doctor two different groups onto one id in the `.lodi`: it must read 0 | `unchecked` | `unchecked` |
| `shadowCasters[src]` | count | far-shadow casters this frame **per SOURCE** — `tree`, `card`, `mesh`, `terrain` (bungo 2026-09-11 14:4x, *"the census counts casters per source"*). The four are a PARTITION, because his ruling is that every placement has exactly ONE shadow representation at a time; the bake-side denominator for the first three is the `native-casters:` line (§6.1, §6.2) | NATIVE 3.4, the `native-casters:` line | walk into a forest: `tree` rises and the others do not. On a region with no trees `tree` reads **0**, and that 0 is a measurement, not a default | `no_cut`, `no_library` | `uncounted` |
| `shadowMarchMs` | ms | the terrain shadow **march** alone, not the whole shadow pass. bungo 15:0x made the hybrid far shadow the arm and both of its halves census fields, so neither half may hide inside the other's number. The plan's 0.3—0.8 ms is an ESTIMATE and stays labelled one until a capture round measures it | runtime — FO4CS times it; no bake number exists | lower the sun: the march is the half whose cost rises | `off` when the far-shadow arm is off, `n/a` on a frame with no terrain in the far field | `unmeasured` |
| `shadowMapMs` | ms | the far shadow **map** alone — the cluster cut plus the geometry rendered into it, the other half of the same hybrid (bungo 15:0x). The plan's 1—3 ms is an ESTIMATE and stays labelled one | runtime — FO4CS times it; no bake number exists | raise `shadowTolerancePx`: fewer triangles, less time | `off` when the far-shadow arm is off, `n/a` on a frame that drew no far caster | `unmeasured` |

### 5.4 Fade

bungo's screen-size fade spec, 2026-09-11 10:4x: Unity's model — projected size
as a FRACTION OF SCREEN HEIGHT, one threshold per engine fade class, default
about 1 percent, roughly 20 percent hysteresis for fade-in, and the fade is the
dither cross-fade.

| field | unit | what it counts | read from | how it MOVES | refusal words | default |
|---|---|---|---|---|---|---|
| `fadeObjectsPct` | percent of screen height | the threshold for the engine's OBJECTS fade class | runtime | set the INI key: `ringCulledScreen` moves | — | `unset`, and `ringCulledScreen` refuses `no_threshold` |
| `fadeActorsPct` | percent of screen height | the threshold for the ACTORS class | runtime | as above, with an actor in view | — | `unset` |
| `fadeItemsPct` | percent of screen height | the threshold for the ITEMS class | runtime | as above | — | `unset` |
| `fadeGrassPct` | percent of screen height | the threshold for the GRASS class | runtime | as above | — | `unset` |
| `fadeHysteresisPct` | percent | the fade-in margin above each threshold | runtime | set it to 0 and objects flicker at the threshold; `crossFadePeak` climbs | — | `unset` |
| `crossFadeInFlight` | count | dithered cross-fades running this frame | runtime | walk across a fade threshold | `no_fade` | `uncounted` |
| `crossFadePeak` | count per window | the window's peak of the above. An instantaneous count alone is unreadable at a 5-second census interval | runtime | as above | `no_fade` | `uncounted` |
| `gridEdgeBand` | units | the width of the dither band at the loaded 5x5 grid's edge, where `.lodi` instances hand over to the engine's full models (bungo's grid-edge seam item 1) | runtime | set it: `suppressedDraws` moves | `no_band` | `unset` |
| `suppressedDraws` | count | engine object draws suppressed because Improved LOD drew that object instead. The join is `cold[i].refFormId` at the instance's own index | NATIVE 4.1a | turn the inward extension off: it falls to 0 | `not_suppressing:no_refmap`, `not_suppressing:no_lodi`, `not_suppressing:off` | `uncounted` |
| `drawnPrimitives` | count | the non-zero primitive read-back NATIVE 7 step 9 requires for one frame BEFORE the engine's far field may be hidden | NATIVE 7 | it is the gate on `engineFarHidden`; if it stays 0 the engine's tree must never be hidden | `unread` | `unread` |
| `engineFarHidden` | 0/1 | whether `spLODObjectRoot` is actually hidden. The census must never claim a replacement it did not perform | NATIVE 7 | it may only become 1 after `drawnPrimitives` was non-zero | — | `0` |

---

## 6. What the generator already promises

This is the other half of the contract: the numbers the bake ALREADY prints, so a
runtime row can be checked against the file instead of against an opinion.

### 6.1 The bake census lines, as they exist today

Measured off the frozen logs named in the provenance footer, not retyped from
memory. Each line is one physical line of prose-with-numbers on stderr; they are
parsed by keyword, never by field position.

| line | what it carries |
|---|---|
| `native:` | `.lodo` bytes; bases written and bases in the ESM census; models loaded and failed; meshes, clusters, triangles, vertices, materials; `.lodi` bytes; instances, arrivals, census refs, drops, unlit, without a stock identity; chunks present of dense, max instances a chunk, max scale, max baseId, `PARTIAL`; bytes a placement; the cache-order before/after; the silhouette counts; the failed model names |
| `native-ladder:` | ON/OFF, the grouping target and the exact-error budget; the level range; meshes with and without a ladder; the five group outcomes by name (formed, too small, no triangle removed, error did not grow, opened a silhouette); which error rule served how many; welded verts and UV conflicts; `maxError`; `coneOpen`; roots and the full-detail triangles they cover; and **one `level L clusters C triangles T meanError E` group per level** |
| `native-ladder-refused:` (v4) | the foliage refusal by name and its way back (`--native-ladder-foliage`); the per-LEVEL silhouette refusal, the levels and the meshes it fired on, the floor it enforced with the view count and the raster size beside it, and the worst kept fraction in the whole file |
| `native-library:` (v4) | where level 0 came from, named as the switch that chose it (`--library near` / `--library mnam`); how many bases had no near `MODL` and kept their `MNAM` slots in place; and placement AO on or off, with measured and not-measured counts and the `0xFF` NOT MEASURED word spelled out |
| `native-library-build:` (lane PERF1, 2026-09-17) | whether the object library was BUILT or KEPT for this bake — `reused (...)` or `rebuilt (<the one test that refused>)`, first refusal wins. Present on every `--native` run; only an `--incremental` one can be offered the choice, so a full bake always reads `rebuilt (not offered: this is not an incremental bake)`. The saving it reports is in the `stage times:` line's library split |
| `native-occluders:` | ON/OFF; boxes written, offered, dropped and the per-cell cap; cells with a box, of cells populated, as a percentage, and cells with none; and the five fit refusals by name (not watertight, too small, no interior voxel, too thin, failed the 100-point probe) |
| `native-casters:` | the per-SOURCE caster counts and the law that makes them a partition: instances total; `tree`, `card`, `mesh`, `none`; their sum against the instance count, printed as **AGREE** or **DISAGREE** so a reader never has to add four numbers up; `mesh-slot casters` over the mesh count (each instance once per DISTINCT mesh its base names); and the note that the terrain march is a runtime cost FO4CS measures, not a bake number |
| `vt:` | levels, tiles, present, cover tiles, bytes, cover on/off and where it went, sheets and whether emissive is present; the mask rule census (`maskPbrm`, `maskLegacyInverted`, `maskNoneDefault`, and the map counts); finest, coarsest, content, border, mips, compression; the three corpus hashes; and the twelve road fields |
| `vt:` ... `terrainObjectAo` clause | **The OBJECT-AO census (lane GROUND1, 2026-09-12; the slab words from lane SLAB1, 2026-09-18).** Written whether or not the pass is on, on the same discipline as the road clause: `terrainObjectAo 0` means the switch was off, and `terrainObjectAo 1 objAoTexels 0` means it was on and nothing in the region stood high enough within 1,458 units of a texel to darken it. The fields, in order: `objAoReach` (1458, the march's furthest step in WORLD units), `objAoStrength`, `objAoPlacements`, `objAoMeshes`, `objAoTriangles`, **`objAoSlab`** (which law marched: 1 = the slab lattice, 0 = `--no-terrain-object-ao-slab`, the pre-2026-09-18 max-Z reading), `objAoSquares` (occupied 128-unit lattice squares), **`objAoSlabSquares`** (of those, the ones whose LOWEST object surface stands more than one cell, 128 units, above the ESM terrain under them â the squares the ceiling term can act on), `objAoTexels`, `objAoMeanDark` (mean darkening of the texels it touched, of 255), and the four refusal fields. **A zero is WRITTEN, never omitted**, so a reader can tell "off" from "an older build". HOW THEY MOVE, measured on chunk 4.4.-12 of the Commonwealth: with the term off, `objAoSquares 0 objAoSlabSquares 0`; with it on, `24729` and `13678`, the second reproduced out of the `--dump-object-ao` lattice and `--dump-land` by a reader that shares no code with the counter; adding `--no-terrain-object-ao-slab` moves `objAoSlab` to 0 and LEAVES `objAoSlabSquares` at 13678, because it is a property of the lattice and not of the march, while `objAoTexels` goes 3,807,692 â 4,162,845 and `objAoMeanDark` 71.6092 â 88.9169. The same fields are on the per-chunk composite's own stderr line and in the `.lodb` bake record, all three written from the one `LodgenObjectAoCensus`. Gate: `tests/spells/lodgen_slab.sh` leg (b) reads all three bakes' lines back by keyword. |
| `native-group:` / the `groups` and `vertex sky` clauses (lane LODIV7, 2026-09-18) | **The v7 clauses, on the same discipline as the road and object-AO ones: a zero is WRITTEN, never omitted, so a reader can tell "off" from "an older build".** `vertex sky ON: N placements streamed (B bytes, mean M, K at or above 128)`, or `OFF (--lodi-v6)`; and `groups G over P placements (R grouped, largest L, S singleton), A placement(s) whose BASE model path has a \`C\` component`, or `OFF (--lodi-v6)`. **The last clause names the COMPONENT the bake actually matched on** (`C`, from `WW_LODI_GROUP_COMPONENT`), not the word `architecture`, because the knob is settable and a census that printed the default while the bake used something else would be telemetry echoing intent instead of truth. New words: `vertexSkyBytes`, `vertexSkyPlacements`, `groups`, `groupedPlacements`, `largestGroup`, `singletonGroups`. HOW THEY MOVE, measured on chunk 4.4.-12 of the Commonwealth: `--lodi-v6` turns both clauses to `OFF` and the pair back to byte-identical v6; raising `WW_LODI_GROUP_TOLERANCE` from 0 to 256 takes `groups` 713 -> 588 (16, the shipped value) -> 527 and `largestGroup` 126 -> 205 -> 288; `WW_LODI_GROUP_SHAPE=sphere` takes `largestGroup` to 1,526; and `WW_LODI_GROUP_GRID` moves NOTHING at all -- 256 and 4096 write the byte-identical `.lodi` -- because the grid is an accelerator and not a rule. Gate: `tests/spells/lodi_v7.sh` G2 and G3, whose four grouping refuters each carry a control that must go red. |
| the retired `vertex horizon` and `horizon` clauses (lanes HORIZON1-2, 2026-09-18; REMOVED by lane HORIZONOUT, 2026-09-19) | **These words are gone from every census line.** They were the baked far shadow's: on the native ladder line `vertexHorizonPlacements`, `vertexHorizonBytesTotal`, `horizonVertices`, `horizonBytesPerVertex`, `horizonMeanElev`, `horizonMinByte`, `horizonMaxByte`, and under `--horizon-refute` the `vhorRefute…` / `horizonRefute…` families; on the `vt:` line `horizonLevel`, `horizonTexel`, `horizonSheets`, `horizonTexels`, `horizonNearSkip`, `horizonZeroBins`. **WHY THEY ARE GONE**: bungo ruled the baked-horizon route out on 2026-09-19 after the first perspective picture of it against a ray-cast sun -- *"As you can see, the end result is terrible"*, *"we revert back to identity data per LOD object from the preauthored LODs"* -- because at a low sun the baked object horizons disagreed with a ray-cast sun on **50-58 %** of object pixels (lane SUNSIM1) while the identity far shadow map simulated at 64 u disagreed on about **9 %** (lane HORIZON4). The switches that wrote them (`--horizon-*`, `--no-terrain-horizon`, `--vt-horizon-texel`, `--horizon-refute`) are gone from the parser and an unknown switch fails by name. **A ZERO IS NOT WRITTEN EITHER**: this is the one place the "write the zero" discipline does not apply, because the pass no longer exists in this exe at all and a `horizonLevel -1` would be a claim about a feature rather than about a bake. A reader that needs the words runs `release/NifSkope.before_horizonout.exe`, which still writes all of them. Gate: `tests/spells/lodgen_horizon.sh` and its refuters are retired with the route; the census words of the surviving v7 clauses are read back by `tests/spells/lodi_v7.sh`. |
| `native-scrappable:` (lane HORIZON3, 2026-09-19, kept by lane HORIZONOUT) | **The `.lodi` v9 bit, on its own prefix.** `OFF` when the bake was not asked for it, and when it was: `scrappablePlacements` (the word the gate greps for) with its share of the instance count, then the three clauses' own counts so a disagreement says WHICH clause moved -- clause 1 `cobjScrapRecipes` over `cobjRecords` with `formListsExpanded` and the resulting base count, clause 2 `buildAreas` from `primitivesSeen` with `primitivesNotBox` / `primitivesUnlinked` / `workshopRefrs` / `tiltedAreas`, clause 3 the `unscrappable` bases and `clause1And3`, their overlap. **A ZERO IS WRITTEN**, never omitted. HOW IT MOVES: the bit is written only under `--scrappable`; a bake at `--lodi-v6` drops the bit rather than claim a version whose layout the file does not have, and the census then reads 0. Gate: `tests/spells/lodgen_scrappable.sh`, which re-derives the count from the FILE's flag bytes and from `Fallout4.esm` without calling any of the bake's code, and carries a red control that flips the bit in a copy. |
| `native-group:` … the `identity-join` clause (lane HORIZONOUT, 2026-09-19) | **WHICH RULE GROUPED THE PLACEMENTS, printed in full rather than named**, because the two rules answer the same question differently and a group count without its rule is not a measurement. `PROXIMITY (the default; --identity-join legacy is the way back)` then the gap in world units, the eligible placement count, the mesh sample points (`level-0 vertices + edge midpoints + centroids, placed`), the sample pairs inside the gap and the milliseconds it took; or `LEGACY` with the component it matched on and the box tolerance. HOW IT MOVES, measured on chunk 4.4.-12 at `--library near`: the default takes `groups` 584 → **158**, `largestGroup` 205 → 208 and `singletonGroups` 462 → 56, and `--identity-join legacy` restores all three exactly. `--identity-join-gap` moves the sample-pair count and the group count together and nothing else. Gate: `tests/spells/lodgen_identjoin.sh`. |
| `cover ...` | the ground-cover per-chunk census: texels, painted points, the dangling-record error class, the informational `ltexNoGnam` / `grasNoTint` **with denominators**, and the read counts (VT 1.6) |
| `roads ...` | one per chunk while the pass is on; the same eleven fields as in the `vt:` line (VT 1a.7) |
| `arrays written:` | textures, arrays, size classes, layers from a `.lodm`, own-emit sources; shapes carrying a layer; shapes without UV2; unreadable textures |
| `merged:` / `far rings:` | the stock-path merge and far-ring simplifier counts |
| `bake census: ... layout` | **the LAYOUT clause (lane LAYOUT1, 2026-09-16).** The absolute root every FO4CS-target file of this bake went under, READ BACK from the paths actually written and never from the setting; the number of files noted under it; and the number written OUTSIDE it, with the first offender named. A SECOND root, if two writers ever disagree, is named in the same line. Its default accuses its own plumbing: a bake that wrote no FO4CS-target file at all prints `layout n/a (no FO4CS-target file written)`, never `layout , 0 file(s)`. HOW IT MOVES: point a bake at a different output folder and the root follows it; write one file outside the root and `outside` rises by one and names it. Gate: `tests/spells/lodgen_layout.sh` leg (f) reads this line back and requires the folder to exist and to hold exactly that many files. |
| `stage times:` | `landscape`, `meshes`, `textures`, `impostors`, in seconds — the four the LOD Generation panel prints |
| `bake-record:` | **The BAKE RECORD clause (lane BAKEREC1, 2026-09-17).** The absolute path of the `.lodb` bake record this bake wrote, then the record READ BACK OFF THE DISK by the same reader `--bake-record` uses: plugin, resource, switch-token, chunk and census-line counts, the `end` line's files and bytes, and the record's own size in bytes. It is printed AFTER the record is closed, so it is the one census line the record does not itself carry, and a bake that dropped a section says so here instead of being believed. A record that will not read back prints `bake-record: <path> REFUSED ON READ-BACK -- <reason>` and the bake still fails loudly rather than quietly. HOW IT MOVES: add a plugin and the plugin count rises; add `--resource` and the resource count rises; bake one more chunk and both the chunk count and the end counts rise. Gates: `tests/spells/lodgen_bakerec.sh` leg (a) requires the line to be present, to be absent from the record itself, and its counts to equal what the independent Python reader finds in the record. |
| `incremental:` | **The INCREMENTAL line (lane LAND1, 2026-09-12; a fifth field from lane INCR1, 2026-09-17).** `N of M chunks dirty`, then the reason each dirty chunk is dirty, as a partition inside one bracket: `inputs moved`, `not in the ledger`, `output lost`, `by neighbour` (the one-cell widening), and `with no native chunk cache`. The last is chunks clean by every other measure whose `.lodj` is simply not on disk -- the first `--incremental` after an older bake -- and it is the field that tells a full-looking run apart from a genuinely dirty one. Up to eight chunks are then named on their own lines with the digest that moved. HOW IT MOVES: edit one cell of the plugin and `inputs moved` becomes 1 and `by neighbour` rises to the ring around it; delete one output and `output lost` becomes 1; delete one `.lodj` and `with no native chunk cache` becomes 1 **and `by neighbour` stays where it was**, which is the whole point of the two dirty lists (`docs/LODGEN_LEDGER_FORMAT.md` §4.2). Gate: `tests/spells/lodgen_incremental.sh` arms (a) and (b) read the numbers out of this line rather than grepping its wording -- grepping the wording is how the first version of that leg passed while nothing at all was cached. |
| `native cache:` | **The CHUNK CACHE line (lane INCR1, 2026-09-17),** printed only on a `--native` bake with the cache on. Chunks written to `.lodj`; chunks replayed from cache and the placements those carried; failures; and arrivals lit by more than one chunk. The last two are not decoration: a non-zero failure count fails the bake, because the pair it would write is missing whole chunks, and a non-zero shared-arrival count on a run that replayed anything REFUSES, because `(prev + a1) + a2` is not `prev + (a1 + a2)` in floating point and the pair would be nearly right. HOW IT MOVES: a null incremental prints `0 written, <all> replayed`; deleting one `.lodj` prints `1 written, <rest> replayed`; `--no-native-cache` removes the line entirely and the incremental run refuses instead. Gate: `tests/spells/lodgen_incremental.sh` arms (a), (b) and (e). |
| the manifest per chunk | one row per placement, with `(ref, part)` as the stable key and the stock identity index (MANIFEST 3, MANIFEST 3.2) |
| the card sidecar `<id>.txt` | per card set: the coverage contract, the projection, the frame class, the gap, the per-frame offsets, `frameclamped`, `framefit`, `orthofit` (CARDS 5) |

### 6.2 The cross-checks a runtime row must satisfy

These are the reason the two halves of this page are one page. Each is an
arithmetic statement between a runtime field and a bake number, and each can be
checked live by a reader with the file open.

| runtime field | bake number | the check |
|---|---|---|
| `instancesTotal` | `.lodi` `instanceCount` (`native:` line: *instances N*) | `instancesTotal <= instanceCount`, and equal when the whole worldspace is loaded |
| `ringInstances[r]` | the chunk table's per-chunk `instanceCount` | the bins partition the loaded chunks' instances; no bin may exceed the chunks it covers |
| `ringDrawn[r]` | `ringInstances[r]` | the four-way sum law of §3 |
| `ringCulledOccluder[r]` | `.lodi` `occluderCount` (`native-occluders:` *boxes N written*) | **0 boxes forces the refusal `no_boxes`**, never a silent 0 |
| `clusterSelected[l]` | the `native-ladder:` per-level cluster counts | no level may select more clusters than the library holds at that level |
| `clusterTriangles` at tolerance 0 | `level 0 triangles` of the drawn set | equal, because tolerance 0 selects exactly level 0 (NATIVE 4.4.1's own floor) |
| `clusterTriangles` at any tolerance | the roots' covered triangles (`roots R covering T full-detail triangles`) | the ladder is a partition of its own surface, so the cut lies between the roots' count and the level-0 count |
| `cutOverlap`, `cutGap` | — | both 0, because the cut is a partition by construction |
| `levelMax` | `native-ladder:` *levels 0..L* | equal, or the loaded file is not the baked one |
| `cardBases` | bases with `cardLayer` != 0xFFFF | `cardBases` may not exceed that count |
| `tilesResident[l]` | the VT index's `tiles` / `present` per level (`vt:` line) | `tilesResident <= tilesPresent` at every level |
| `libraryBytes` | `.lodo` `fileBytes` | `libraryBytes <= fileBytes`; equal is the read-once case NATIVE 7 describes |
| `stale` | the four corpus hashes plus `loadOrderHash` | exactly the soft key list of NATIVE 5; a hard key never raises `stale`, it refuses |
| `shadowCasters[tree]`, `[card]`, `[mesh]` | the `native-casters:` line—s four bins | no runtime source may exceed its own bake bin, and the three together may not exceed `instanceCount` minus the bake line's `none`. `none` is a FAULT count: a placement with no representation of any kind casts nothing |
| `shadowCasters[terrain]` | — | **no bake number exists**, and that is the honest answer rather than a missing row: the march runs over the resident height source at runtime, so the field reads from the frame, never from a file |
| `suppressedDraws` | the cold blob's `refFormId` set | may not exceed the instances actually drawn |

### 6.3 What is MISSING, and is owed to the next generator lane

Named here, **not added** — lane BAKEPERF1 owned `src/` while this page was
written, so no writer was touched. Each item says what the runtime cannot check
without it.

1. **A per-slot instance total in the `.lodi` header.** The brief's example was
   "per-ring instance totals", and the honest version of it is this: a *ring* is
   camera-relative, so no file can state one. What a file CAN state, and does
   not, is **how many instances draw from each of the four MNAM slots**
   (`rep[0..3]`), which is the static property the four ring bins approximate.
   Without it `ringInstances[r]` has no bake-side denominator at all and can only
   be checked against the whole-file total. Header-only: four `u32` in the
   `.lodi`'s reserved 0xB0..0xFF.
   **DONE, `.lodi` v5, lane NATIVE1c 2026-09-16** — `slotInstances[4]` at
   **0xD4..0xE3** (0xB0..0xD3 went to the aggregate in v4). The writer counts the
   slot each placement actually drew from; the reader **refuses a file whose four
   totals do not sum to `instanceCount`**, by name. `lodiDescribe` prints
   `slotInstances0..3`, and `tests/spells/lodgen_native_fields.py` §j4 checks
   both that the sum holds and that the four MOVE — a slot distribution that is
   all in one bin would pass a sum check and mean nothing.
2. **A per-base full-detail triangle count.** `ringTriangles` and
   `clusterTriangles` can only be checked by walking every selected cluster. One
   `u32` per base — "triangles if this base drew at level 0" — makes the check a
   sum over the drawn set instead of a walk. The base row is 32 bytes and full,
   **but `crossPx16[4]` is eight of those bytes and is written as all zeros
   today** (NATIVE 11, Deviation 5), so the room exists without a stride change.
   **DONE, `.lodo` v4, lane NATIVE1c 2026-09-16** — `crossPx16[0..1]` is now one
   `u32 fullTriangles`, the triangles the base draws at level 0 summed over the
   DISTINCT meshes its `rep` slots name. `crossPx16[2..3]` is still free. The
   note about it being a format decision was right, and the decision was taken
   the expensive way: those four bytes are now READ as something they were not,
   so `.lodo` goes to **version 4 unconditionally and refuses version 3 by
   name**, saying what the bytes used to be. The reader RECOUNTS the field from
   the cluster table and refuses a mismatch, so it cannot go stale in a file.
3. **A card count in the `.lodo` header.** `cardBases` has no denominator: the
   number of bases carrying a `cardLayer` is only obtainable by walking the whole
   base table. One `u32` in the `.lodo`'s reserved 0xCE..0xFF.
   **DONE, `.lodo` v4, lane NATIVE1c 2026-09-16** — `cardCount` at **0xD0**. The
   reader refuses `cardCount > baseCount` by name, and the field gate checks the
   header word against a count of the rows that actually carry a layer, so a
   reader sizing its card pass from the header alone cannot be misled.
4. **The aggregate ring-3 impostors do not exist.** `cardsAggregate` refuses
   `not_baked` and will keep refusing until lane CARDS-AGG bakes them. bungo also
   asked (2026-09-11 08:3x) for **the count of forested cells printed before
   anything is built**; nothing prints it today. **Owed to CARDS-AGG.**
5. **A watertight bit in the `.lodo` mesh row.** `ringCulledOccluder`'s refusal
   `no_boxes` can say *that* a worldspace has no occluders but not *why*. The
   generator knows: 2,617 of 2,982 meshes were refused for not being watertight
   (NATIVE 4.5.1). That fact lives only in the `native-occluders:` line and in the
   `--native-mesh-report` sidecar, not in the container. One free bit in the mesh
   row's `flags`.
   **DONE, `.lodo` v4, lane NATIVE1c 2026-09-16** — `LODO_MESH_WATERTIGHT = 4`
   in the mesh row's free flag bits, set from the source soup having **zero
   boundary edges** (`lodoBoundaryEdges`), which is the same test the occluder
   fitter already applied and then threw away. `lodoDescribe` prints
   `watertightMeshes`, and the field gate requires the count to be **strictly
   between 0 and the mesh count** — all-set and all-clear are both the bit not
   working.
6. **`crossPx16` is written as all zeros** (NATIVE 11, Deviation 5). A consumer
   cannot read a per-base screen-size ladder out of the file, so every screen-size
   decision is the runtime's own. Stated as a known hole rather than an owed item:
   the cluster cut replaced it.
   **Half of it is spent as of `.lodo` v4** (item 2 above); the remaining
   `crossPx16[2..3]` is still zeros and the statement still holds.
7. **Nothing names an object's engine FADE CLASS.** `fadeObjectsPct` and its three
   siblings need to know whether a placement is an object, an actor, an item or
   grass. That is the engine's own record, read at runtime, not a bake product —
   **so it is NOT owed to the generator**, and it is written here so a later
   reader does not go looking for it in a file.
8. **Per-instance AO for card-drawn placements.** Not in the original list, and
   it should have been. The instance record's `ao` is the mean over the
   placement's own lit chunk-mesh vertices, so a card-drawn placement — which
   has none — was written **255, fully lit**. Every runtime AO row for a card
   placement was therefore reading a constant. bungo asked the question that
   found it, 2026-09-11 15:3x: *"is vertex AO baked into impostors too on top of
   the texture AO they hold?"*
   **DONE, `.lodi` v5, lane NATIVE1c 2026-09-16** — a parallel `u8` blob, one
   byte per instance, from one ray cast straight up at bake against the assembled
   chunk AND the heightfield, exactly as the chunk's own vertices get theirs.
   `0xFF` means NOT MEASURED and is not AO 255. `--native-no-placement-ao` is
   the way back and leaves the instance payload byte-identical to v4; the two
   derived header words (`headerCrc32`, `lodoIdentity`) move with the companion
   `.lodo`, 12 bytes of 128,256 measured, and both recompute (NATIVE 4.7).

9. **The `.lodo` carries no count of bases that failed to load a model.** The
   `native:` line says *4 without a loadable model*; the file says only 2,970
   bases. A reader of the pair alone cannot tell a 2,970-base worldspace from a
   2,974-base one with four failures. Informational, low value, **named for
   completeness**.

---

## 7. The checker

`tests/spells/lodgen_census_check.py <out-dir> [--census <log>] [--self-floor]`

Read-only, needs no exe, and it never consults the writer: it decodes the pair
with `tests/spells/lodgen_native_decode.py` — the independent decoder written
from this contract, not from the emitter — and compares every number the bake's
census line printed against the number read out of the bytes.

Three verdicts, and only one is a pass:

* **`ok`** — the census line and the files agree.
* **`RED`** — they disagree. The number printed is not the number written.
* **`not-derivable`** — the census word is a BAKE-TIME fact: a refusal reason
  (`refused 2476 too small`), a corpus read (`models 2982 loaded`), a before/after
  measurement (`acmr 1.8600 -> 1.8585`). The container does not carry it. These
  are **named one by one and counted separately, never as passes**, because a
  checker that silently skipped them would report a coverage it does not have.

Measured on the nine-chunk Sanctuary pair (NATIVE1b, 2026-09-11 10:17:44):
**59 checks, 0 failures, 31 census words the files cannot carry.** On LODUI1's
four-cell region pair (2026-09-11 13:25:22): 59 checks, 0 failures, the same 31.

**That figure is HISTORICAL and cannot be repeated as it stands.** Every pair it
was measured on (the table under Provenance) is a **version 3 `.lodo` beside a
version 3 `.lodi`** (their first eight bytes read `LODO 03 00 00 00` and
`LODI 03 00 00 00`), and both today's decoders refuse a version 3 `.lodo` by name
(`src/lodofile.cpp` `if ( h.version == 3 )`, `tests/spells/lodgen_native_decode.py`
`if h['version'] == 3:`), because v4 reinterprets the base row. A current figure
needs a re-bake at `.lodo` 4 / `.lodi` 7 and a fresh run; until then 59/0/31 is a
2026-09-11 measurement, not a gate.

**The floor.** `--self-floor` doctors three claims one at a time —
`lodo.clusterCount`, `lodi.instanceCount`, `lodo.level1.triangles` — and each must
be caught by name. All three were, on both pairs. `--doctor field=value` does one
by hand.

**Two refusals it makes rather than guessing.** If two logs under the out-dir
disagree about a census line it refuses and names both, because choosing the log
that agrees would make the check circular (CONSTITUTION 4). If the decoder will
not read the pair at all it exits 2 with the decoder's own words, so "the files
are unreadable" is never reported as "the census is wrong".

---

## Provenance

This page cites **contract pages and frozen artefacts only** — no `src/` line
numbers, because lane BAKEPERF1 owned `src/` while it was written
(`ww-contract-provenance`: a line number from a file another lane is editing is a
rumour by the time it is read). Every bake-census claim in §6.1 is anchored to the
census line's own leading word, which is stable across edits, and to a log file
that cannot change.

### Contract pages cited

Stamped AFTER the last edit of this page, including this lane's own pointer
paragraph in `NATIVE 7` (`ww-contract-provenance` step 5).

| page | key used above | sha256 (16) | bytes | lines |
|---|---|---|---|---|
| `docs/LODGEN_NATIVE_LODO_LODI.md` | `NATIVE` | `067eeaa70e2924fe` | 79,504 | 1,253 |
| `docs/LODGEN_TERRAIN_VT.md` | `VT` | `13efc51896021bae` | 75,094 | 1,211 |
| `docs/LODGEN_CARD_SHEETS.md` | `CARDS` | `4edb4a9c8703185d` | 42,585 | 723 |
| `docs/LODGEN_TEXTURE_ARRAYS.md` | `ARRAYS` | `64f9154b71f83631` | 13,608 | 256 |
| `docs/LODGEN_MANIFEST_FORMAT.md` | `MANIFEST` | `ba5ca72c83991ad5` | 12,399 | 264 |
| `docs/LODGEN_LODM_FORMAT.md` | `LODM` | `47f2b4ee6b4938ba` | 28,378 | 411 |

### Artefacts the §6.1 inventory was read off

| artefact | sha256 (16) | bytes | written |
|---|---|---|---|
| `scratchpad/native1b_20260911/gate/final/bake_native.log` | `2cd1cdda18fdf5c4` | 202,682 | 2026-09-11 10:17:45 |
| `scratchpad/native1b_20260911/gate/final/bake_occ.log` | `e6073e146b685680` | 809,527 | 2026-09-11 10:18:25 |
| `scratchpad/lodui1_20260911/stage_times/logs/region.log` | — | — | 2026-09-11 13:25 |
| `scratchpad/lodui1_20260911/logs/lodgen_roads.log` | — | — | 2026-09-11 12:2x |
| `scratchpad/lodui1_20260911/logs/lodgen_texture_arrays.log` | — | — | 2026-09-11 13:1x |
| `scratchpad/slab1_20260918/off_new/bake.log` | `e8db0705971ac7ef` | 67,367 | 2026-09-18 07:32:13 |
| `scratchpad/slab1_20260918/after/bake.log` | `77a6372219108e82` | 129,213 | 2026-09-18 07:32:28 |
| `scratchpad/slab1_20260918/oldlaw/bake.log` | `9d969f1f5427fa98` | 129,216 | 2026-09-18 07:32:42 |

### The pairs the checker was gated on

| pair | `.lodo` sha256 (16) / bytes | `.lodi` sha256 (16) / bytes | baked |
|---|---|---|---|
| `scratchpad/native1b_20260911/gate/final/native/Native/` | `1efe74130a6b85f5` / 9,657,316 | `7c42d7265b7ce686` / 128,256 | 2026-09-11 10:17:44 |
| `scratchpad/lodui1_20260911/stage_times/region/` | `1efe74130a6b85f5` / 9,657,316 | `276ec262709c76b7` / 37,248 | 2026-09-11 13:25:22 |
| `scratchpad/native1b_20260911/gate/final/occ/Native/` | `1efe74130a6b85f5` / 9,657,316 | `c12e494b73641b70` / 1,091,072 | 2026-09-11 10:18:21 |

The three `.lodo` files are **byte-identical**, which is NATIVE 6's claim — the
library is built from the full worldspace census in every bake and is not
region-scoped — holding across three bakes three hours apart over three different
cell sets.

**The Boston pair (the third row) cannot be decoded today.** The independent
decoder refuses it at instance 3359 with *"out of (cell, drawKey, ref, part)
order"*; §6.3's owed list does not cover it because it is not a missing field.
Lane CENSUS1's report (`scratchpad/lane_census1_report.md`, §4) carries the
measurement: the instance sits at 2.999985 cells north of its chunk origin after
quantisation, so the decoder re-derives a different cell from the stored position
than the writer sorted on. It is the cell-level twin of the chunk-boundary effect
NATIVE 4.1c and NATIVE 6 already describe, and it is **owed to a generator lane**
as a decoder rule, not as a format change.

Written by lane CENSUS1, 2026-09-11.
