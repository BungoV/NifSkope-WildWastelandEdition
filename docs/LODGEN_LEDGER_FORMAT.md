# The LOD bake ledger (`.lodb`) and incremental regeneration

Lane LAND1 / INCR1, 2026-09-12. Companion to `docs/LODGEN_TERRAIN_VT.md` (the
bake itself), `docs/LODGEN_BTD_FORMAT.md` (the `.lodt` contract) and
`docs/LODGEN_IMPOSTOR_SPEC.md` (the LOD material).

A full Commonwealth bake is hours. Almost every edit a person makes touches one
cell. The ledger exists so the second bake can cost what the edit cost, and the
whole design question is not "how do we go faster" but **"what has to be true
for the fast answer to be the same bytes as the slow one"** — because a LOD
tree that is subtly not what a full bake would have produced is worse than no
LOD tree at all: it is wrong in a way nobody can see until they are standing in
it.

---

## 1. The file

`<out-dir>/FO4CSLOD/<WorldspaceEdid>/<WorldspaceEdid>.lodb` on the FO4CS target
(`--native <dir>`), `<out-dir>/<WorldspaceEdid>.lodb` on the stock one, written
by **every** region bake, not only by incremental ones, and written **LAST** so
that the `end` line can count the tree the bake produced.

> **Divergence from the brief, stated rather than done quietly.** INCR1's brief
> suggested `<out>/Terrain/<WS>.lodb`. `--out-dir` has no `Terrain/`
> subdirectory in this generator — that folder belongs to the `--vt` mod tree,
> which this ledger does not describe — so the ledger sits beside the `.BTR` and
> `.BTO` files it is a ledger *of*. One word from bungo moves it.
>
> **2026-09-16, lane LAYOUT1.** That word came: bungo 2026-09-16 19:3x, "The folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?" (lane LAYOUT1). The
> ledger's new home is `FO4CSLOD/<ws>/<ws>.lodb` and it belongs to lane
> BAKEREC1, which owns the ledger. LAYOUT1 moved every OTHER FO4CS-target
> output and deliberately wrote nothing into that spot; the ledger is still
> at `<out-dir>/<WorldspaceEdid>.lodb` as this page says, and the layout gate
> names it as an exemption rather than sweeping it.
>
> **2026-09-17, lane BAKEREC1 moved it.** On the FO4CS target the record now
> sits at `FO4CSLOD/<ws>/<ws>.lodb`, the layout gate's exemption for it is
> GONE (it is an ordinary file under the one root like every other), and the
> record's output paths are relative to **the record's own folder** -- so a
> `--keep-bto` chunk left at the out-dir root spells `../../<name>.BTO`.

> **2026-09-17, lane BAKEREC1: THE CONTAINER BELOW IS VERSION 1 AND IS NO
> LONGER WRITTEN.** Version 2 is PLAIN TEXT -- UTF-8, LF, one record a line,
> `key<TAB>fields` -- and carries five sections this page never had: the five
> staleness hashes, the load order with a per-file FNV-1a 64 over each
> plugin's BYTES, the resource stack, the argument vector token by token, and
> every census line the bake printed. **`docs/LODGEN_BAKE_RECORD.md` is the
> format now**; this page keeps the dependency map (section 2) and the switch
> digest (section 3), which version 2 uses unchanged.
>
> A version 1 file is **refused by name**, by both readers, rather than
> mis-parsed: *"is a version 1 BINARY bake record and this build writes
> version 2 (plain text). Re-bake once without `--incremental`; every bake
> writes it"*. There is still exactly ONE ledger file -- a second one would
> have been the second per-chunk digest INCR1 must not be given.
>
> The v1 container, for reading an old file:

```
offset  size  field
0       4     magic   'LODB'  (0x42444F4C little-endian)
4       4     version 1
8       4     jsonLen
12      4     reserved, zero
16      N     the JSON below, QJsonDocument::Compact, UTF-8
```

```json
{ "worldspace": 60, "worldEdid": "Commonwealth", "dim": 4,
  "region": [-24, 16, -13, 27],
  "switches": "<sha1 of the argument vector, section 3>",
  "loadOrder": "<EsmWorld::loadOrderHash, hex>",
  "chunks": [ { "cx": -24, "cy": 16,
                "inputs": "<sha1, section 2>",
                "out": [ "Commonwealth.4.-24.16.BTR <sha1>",
                         "Commonwealth.4.-24.16.BTO <sha1>",
                         "Commonwealth.4.-24.16.BTO.manifest.txt <sha1>",
                         "../tex/Commonwealth.4.-24.16.DDS <sha1>",
                         "../tex/Commonwealth.4.-24.16_msn.DDS <sha1>",
                         "../tex/Commonwealth.4.-24.16_data.DDS <sha1>" ] } ] }
```

Output paths are **relative to the directory holding the ledger**, so a mod
folder can be moved or copied without invalidating it.

### It is deterministic, and that is a gate not a nicety

No thread id; the chunk rows are sorted by `(cy, cx)`, not by the order the
pass retired them. Two full bakes of the same tree therefore write a
**byte-identical** ledger — which is what lets gate B3
compare two output directories *including* the `.lodb` rather than having to
except it, and an exception is exactly where a bug would live.

**2026-09-17, lane BAKEREC1: version 2 states its exceptions instead of having
none.** Version 2 records things version 1 refused to carry -- when the bake
ran, where each plugin lives, the resource stack, the stage times -- because a
record an operator cannot read is a record nobody checks. So the law is now
*byte-identical AFTER NORMALISATION*, and the four volatile things are named on
named lines: the `baked` line's value, the `plugin` line's path field, the whole
`resource` line but its `kind`, and the `census` line beginning `stage times:`.
`lodbNormalise()` (`src/lodbfile.cpp`) and `lodb_read.normalise()`
(`tests/spells/lodb_read.py`) mask exactly those to the literal `<volatile>` --
**masked, never dropped**, so a record that LOST a line still differs. Anything
else moving between two bakes of one tree is a defect, and
`tests/spells/lodgen_bakerec.sh` leg (h) bakes twice into the same folder and
compares. The fourth was found that way: legs (a)-(g) were green while the
record still leaked a wall clock through `stage times: ... meshes 61.4 s`.

---

## 2. The dependency map — what a chunk's bytes are made of

**This section was written before the code was.** That order matters: a
dependency map derived from the implementation can only ever agree with it.

A chunk at `(cx, cy)` of size `dim` reads:

| # | input | where it is read | reach beyond the chunk |
|---|---|---|---|
| 1 | `LAND` heights (`VHGT`) | every cell of the chunk | **1 cell** — `LODGEN_TERRAIN_RING_CELLS = 1`, one 4,096-unit ring, so the chunk's edge normals and its skirt are built from the neighbour's heights |
| 2 | `LAND` vertex colours (`VCLR`) | same | 1 cell |
| 3 | `LAND` quadrant base textures (`BTXT`) and layers (`ATXT`/`VTXT`: LTEX form + opacity) | same | 1 cell |
| 4 | the **bytes** of each `LTEX`'s diffuse, normal and specular, and its material path | the colour/normal sheet composite | 1 cell |
| 5 | every non-deleted, not-initially-disabled `REFR`: form id, base, base type, raw position, raw rotation, scale | the object chunk | **1 cell** — `LodgenObjectOptions::aoSkirtCells = 1` (`lodgen.h:540`); an object one cell out still casts into this chunk's AO |
| 6 | each ref base's four LOD model paths (`MNAM` slots) **and the bytes of those `.nif` files** | the object chunk | with (5) |
| 7 | a `SCOL` base's parts: each part's base, and every placement's raw position/rotation/scale, plus those bases' LOD models and their bytes | the object chunk | with (5) |
| 8 | the switches (section 3) | everywhere | whole region |
| 8a | **the generator itself**: the sha1 of the running executable's bytes, fed FIRST into every chunk's `inputs` digest as `generator <sha1>;` (`lodgenGeneratorIdentity()`, `lodgen.cpp`; lane VTFIX1, 2026-09-24) | everywhere | whole region |
| 9 | with `--terrain-object-ao` only: the same `REFR` rows as (5), (6) and (7), read a SECOND time as an occluder of the far TERRAIN | the object height field, and through it the AO byte (mask sheet B) of both terrain composites | **1 cell** — the march's longest step is **1,458 world units**, which is 0.356 of a cell |

Row 8a is there because rows 1—8 describe what the chunk READS and nothing
describes the program that turns those reads into bytes. The defaults live in
three places (nifcli's `lg*` locals, `lodgen.h`'s option initialisers, the `g_*`
globals in `lodgen.cpp`), so a default flip changes no argument and no input:
before VTFIX1, `--incremental` on the new exe called every chunk clean and kept
the old exe's bytes. The same was true of any code change that moves bytes with
no default touched at all. The exe hash covers both without anyone bumping a
constant. It can only over-rebake: a rebuild that moves no output byte still
dirties every chunk once, which is this ledger's stated direction. An exe that
cannot be read gets a word that never matches a stored ledger. The consequence
for a gate: **two bakes from DIFFERENT exes now always write different `inputs`
digests**, so a byte comparison of `.lodb` files across a rung and a new exe
is expected to differ. Same-exe comparisons (section 1's determinism, the
`--vt`/`--native` pairs below) are unaffected. The census reports such a
chunk as `inputs moved`. It has no separate "generator changed" reason yet;
that belongs to `nifcli.cpp`'s comparison (the dirty list at the ledger read).
Gate: `tests/spells/lodgen_vtfix.sh` G3, which flips a default inside a copy
of the exe and requires the incremental run to equal a full flipped bake. On the
pre-VTFIX1 exe it reported `0 of 2 chunks dirty` and left 4 stale files.

Row 9 does not widen the map beyond what rows 5—7 already ask for: it reads the
same references, and 1,458 units is inside the one cell those rows already
carry. It is a row of its own anyway, because the REASON is different — rows
5—7 widen by one cell for the object chunk's own AO skirt, row 9 for a march
into the terrain sheets — and a later change to either reach must not be
allowed to assume the other one still covers it.

**Where 1,458 comes from, and how it was checked.** The march is
`for ( float dist = 128; dist <= 2048; dist *= 1.5 )`, which stops at 128, 192,
288, 432, 648, 972 and **1458** (1458 x 1.5 = 2187 fails the test), in eight
unit directions — 56 sample points. A texel none of whose 56 points lands on an
occupied square cannot move at all, because the term returns exactly `1.0f` by
early return and `vis * 1.0f` is bitwise `vis`. Lane GROUND1 gated the reach on
that impossibility rather than on a distance estimate: over the Commonwealth
region (—24,24)..(—17,31), 1,048,576 content texels at level 2, **860,624
darkened texels every one of which found its occluder at 1,458 units or nearer,
and none farther**. A further 2,962 texels darkened with no occupied sample of
their own; **all 2,962** share a 4x4 block with a texel that has one, which is
the block codec refitting its endpoints (drops median 6, p95 16, max 30, against
1,007 texels that came out BRIGHTER — which the term, a multiplication by a
number at most 1, cannot do at all).

The switch itself is row 8: `--terrain-object-ao` and
`--terrain-object-ao-strength` are ordinary digested tokens, on neither skip
list, because both can move a chunk's bytes.

Reaches that do **not** widen the map, each checked rather than assumed:

* `cullMargin = 128.0f` (`lodgen.h:572`) — inside one cell.
* the `--vt` border, 256 world units — inside one cell.
* the land-guide macro normal, `--land-guide-scale / 2`, capped at 1,024 units
  by the flag's own refusal — inside one cell.
* the object height field's own **2-cell gather margin**. That is a margin on
  the GATHER — it exists so a placement whose origin sits outside the rectangle
  still contributes the part of its mesh that reaches in — and not a reach into
  a neighbouring chunk's bytes, which is the 1,458 units above. Widening the
  gather cannot change a byte a chunk already owns; narrowing it could.

So **the widening is exactly one cell**, and it is applied twice, deliberately:

* the *input digest itself* loops `cy-1 .. cy+dim` and `cx-1 .. cx+dim`, so a
  height edit one cell outside a chunk already moves that chunk's digest;
* the diff then marks a chunk dirty if any chunk **within one cell of it** is
  dirty, which catches the case no input digest can see — a neighbour whose
  *output file* was deleted or corrupted under us.

### The float rule

A `REFR`'s position and rotation are fed as **raw bytes**, never as a formatted
decimal. A 0.001-unit nudge moves a shadow; a decimal with too few places would
hide it and the incremental bake would ship the old shadow.

### What the map deliberately does NOT cover, and what happens instead

The VT pyramid, `--atlas`, `--arrays` and the impostor-card aggregation are
**whole-region** stages: each builds ONE product out of every written `.BTO`, so
a filtered job list would build it from a fraction of the region — a
quarter-sized atlas that still looks like an atlas. Nothing in the ledger could
make that safe, so `--incremental` **refuses** when they are asked for
(section 4) rather than doing three quarters of a job.

**A correction, kept rather than tidied away.** This map, written before the
code, named the merge and `lodgenSimplifyFarRings` in that list too. Reading
them proved otherwise: both are `for ( path : btoPaths )` loops that open one
`.BTO`, rewrite it and save it with **no state carried between files**, so a
filtered list gives each rebaked chunk exactly the treatment a full run would,
and the skipped chunks were merged and cut by the bake that wrote them. They are
not refused. Keeping them on the list would have been the *safe* reading and the
wrong one: the merge is on by default, so `--incremental` would have refused
every command anybody would type — which is how the error was caught, on the
first arm of gate B3's null run.

---

## 3. The switch digest, and the skip list

`switches` is SHA-1 over the **argument vector, in order**, with the tokens
below and their values dropped.

It is deliberately conservative: reordering flags, or spelling a default out
explicitly, changes the digest and forces a full bake. That costs time and can
never cost correctness, which is the right way round.

**Two lists, because a flag can be two things at once.** The test is narrower
than "cannot reach an output byte": it is **cannot make a TRACKED CHUNK OUTPUT
stale**. The ledger tracks the per-chunk `.BTO`/`.BTR`/`.DDS` files it lists and
nothing else, so a flag that writes a separate product into a separate directory
is not its business.

**Token and value both dropped.** The flag names *where* files go, or *how many*
threads carry them, or *which file* an asset is read from — and that last only
because the ledger digests the asset's **bytes** through the same
`lodgenReadAsset()` the bake uses, so moving a mod folder still dirties every
chunk whose assets changed under it.

```
--out-dir  --tex-dir  --data-root  --incremental
--threads  --chunk-threads  --preview-dir
--resource --plugins-txt  --mo2-profile  --mo2-mods
--native   --native-mesh-report
```

**Token kept, value dropped.** The flag changes the picture; its argument is
only a place to put files.

```
--vt
```

`--threads` being on the first list is a **claim**: BAKEPERF1's pass retires
every job on the calling thread in job order, so the worker count cannot reach a
byte. If that ever stops being true, this line is the bug.

### The rule this section got wrong for one afternoon

`--vt`, `--native` and `--vanilla-lod-root` were skipped entirely, then digested
entirely, on the reasoning that **a flag that changes the output belongs in the
digest even when it does not change the inputs**. Two shipped gates said no
within the hour, and both were right:

* `tests/spells/lodgen_roads.sh` **R1** bakes `--no-roads` twice into
  `roadOff/` and `roadOff2/` and compares every byte. The two commands differ
  only in `--vt <dir>` — a destination, exactly like `--out-dir` — so
  digesting the path made two identical bakes write two different ledgers.
  `--vt` keeps its token (the pyramid really is a different picture from the
  stock per-chunk composite) and loses its value.
* `tests/spells/lodgen_native.sh` **check 5** asserts the stock bake is
  byte-identical with and without `--native`. It is, except for a ledger
  recording whether a `.lodo` was written beside it. `--native` goes to its own
  directory and cannot touch a chunk, so it leaves the digest — and the run
  that *would* be wrong is refused outright (section 4).

The surviving rule: **digest what can make a tracked chunk stale; refuse what
builds a region-wide product; ignore the rest.** `--vanilla-lod-root` stays
digested, token and value, because its argument is an INPUT root and no gate
compares two bakes across it.

**The plugin path is part of the vector**, so baking an edited copy from a
second path fires the "switches differ" refusal. That is correct — a different
plugin is a different input — and it is why a person editing `Fallout4.esm`
edits it in place and keeps the fast path.

### The identity word: a default that moves now moves `switches` (lane INCRGATE1, 2026-09-24)

The argv digest has one blind spot, and INCR1 recorded it as an open finding: it
hashes what was **typed**, so a default that moves inside the exe (lane DEFAULTS1
moved seven on 2026-09-12) leaves `switches` exactly where it was. An
`--incremental` run over a record baked under the old defaults then kept every
chunk it judged clean -- chunks whose bytes the new exe would no longer write.
Measured on the rung exe: a record from `release/NifSkope.before_defaults2.exe`
was accepted with `0 of 1 chunks dirty`.

The record's `switches` line is now
`sha1( argv digest, 0x1F, identity word )`, and the **identity word** is
`gen<N>:<sha1>` over one `key=value` line per EFFECTIVE setting -- the value the
bake used, typed or defaulted -- in a fixed order, first line `generator=<N>`:

* `lodgenIdentityDump( pass, extras )` in `src/lodgenchunkpass.cpp` writes the
  lines: every field of `LodgenChunkPassOptions`, then `LodgenIdentityExtras`
  (atlas, arrays, merge, BC1, keep-BTO, the far-ring cut, the pyramid, the native
  modules) and any `more` lines a front end owns, sorted.
* `kLodgenGeneratorRevision` (`src/lodgenchunkpass.h`, now 1) is the manual half:
  bump it when a change moves output bytes with no setting moving. The dump
  cannot see a constant inside `lodgen.cpp`.
* Paths, thread counts and progress hooks are not in the dump, for the reason
  they are not in the argv digest.
* The command line prints `identity: <word>, N setting(s)`; with
  `WW_LODGEN_IDENTITY_DUMP=<file>` it also writes the lines, for a gate to diff.
  A bare Sanctuary bake reads 112 settings; `--blend-edges off` moves the word.

**One-time cost:** every record written before this lane carries an argv-only
digest, so the first `--incremental` after it refuses "the switches differ" and
asks for one full bake. That is the refusal doing its job.

Gate: `tests/spells/lodgen_incr_identity.py` (G1). Leg (b) bakes with the
`before_defaults2` exe and runs this exe `--incremental` over it: it must refuse
with the switches reason. Leg (c) forges a record whose `switches` is the old
argv-only digest; it must refuse too. This exe: 11 checks, 0 failures. The rung
(no identity word): 11 checks, 6 failures -- the red run.

The ledger code itself moved in the same lane, unchanged in behaviour, from
`src/nifcli.cpp` into `src/lodgenchunkpass.{h,cpp}` (`lodgenIncrementalBegin`,
`...ArmCache`, `...NoteRetired`, `...CacheCensus`, `...CacheRefusal`,
`...OfferReuse`, `...WriteRecord`), so the panel and the command line run one
implementation.

### The panel row "Rebake only what changed" (lane INCRGATE1, 2026-09-24)

The LOD Generation panel's Run section has the row (`LodgenIncrementalCheck`,
settings key `incremental`), **OFF by default**. With it OFF the panel's bake is
byte-identical to the exe before the row (G2 leg (a)). With it ON the panel runs
the functions above with switches `--panel` and the identity word, which hashes
the pass's options, the post passes and every extra row the run reads (as the
run reads it: a hidden row counts as its default). Two differences from the flag,
both because a row is a standing setting and a flag is a request:

* **No record yet is not a refusal.** The whole range bakes and the record is
  written, so the next run can diff. Census:
  `incremental: no bake record at <path> yet; all N chunk(s) baked and the record written`.
* **A verdict the record cannot vouch for bakes whole and says why**, instead of
  refusing: `incremental: every chunk baked because <why>; all N chunk(s) baked
  and the record rewritten`. A refusing row would have no way back short of
  unticking it, and an unticked row writes no record.

Also: a run over more than one chunk size keeps the row off for that run and says
so; a cancelled run and a refused native pair write no record. **The panel's
texture arrays are ON by default, and they are built from the whole range**, so
with the default settings every run is a full bake plus a fresh record; the row
saves time only once arrays, the atlas and the cards are unticked.

**A known limit, in the tooltip:** the asset-byte digests are cached per process
(`g_ledgerAssetDigest`, `src/lodgen.cpp`), so a model or texture edited while the
panel stays open is seen after a restart. Fixing it means clearing that cache at
the start of a run, in `lodgen.cpp`; that change is owed.

Gate: `tests/spells/lodgen_panel_incremental.sh` (G2). Three `WW_LODGEN_RUN`
launches: the rung, this exe with the row OFF, this exe with it ON. (a) rung ==
OFF, 10 files, 45,582,390 bytes. (b) ON = OFF plus exactly the record and one
`.lodj`; the record names 1 chunk, `--panel` and the first-run census. RED: the
rung's tree read as the ON tree fails (b2) and (b3). Not covered: a second panel
run over the first run's record. The harness in `src/nifskope_ui.cpp` wipes its
output folder at every launch.

---

## 4. The refusals

`--incremental` never promotes itself to a full bake and never does half a job.
Each refusal names itself, says what to do instead, and exits **1 having baked
nothing** (`LodgenIncrRefusal` in `lodgen.h`).

| enum | fires when | what it says to do |
|---|---|---|
| `LODGEN_INCR_NO_LEDGER` | no readable `.lodb` at `<dir>/<WS>.lodb` | run the same command once without `--incremental`; every bake writes the ledger |
| `LODGEN_INCR_WRONG_SHAPE` | the ledger's worldspace, `dim` or region is not this run's | an incremental run must cover exactly the region its ledger covers |
| `LODGEN_INCR_SWITCHES` | the switch digest differs | bake without `--incremental`; every chunk is dirty anyway |
| `LODGEN_INCR_WHOLE_REGION` | `--atlas`, `--arrays` or `--impostors` is asked for, or `--native` **together with `--no-native-cache`** | do those in a separate full pass over the finished chunks (section 2). The merge and the far-ring simplify are **not** on this list |
| *(no enum, printed inline)* | a `.lodj` chunk cache could not be written, or could not be replayed | bake without `--incremental`: the pair this run would write is missing whole chunks |
| *(no enum, printed inline)* | a replay ran **and** some placement was lit by more than one chunk | bake without `--incremental`: a full bake adds those sums in the one order there is |
| `LODGEN_INCR_OUTPUT_GONE` | *not* a refusal — a file the ledger claims is missing or edited marks that **chunk** dirty | nothing; it rebakes |

An `--incremental` run that promoted itself to a full bake would have lied to
an operator watching a clock. One that silently did half the work would have
lied worse.

`--native` was on the whole-region refusal from 2026-09-12 until 2026-09-17,
and it is the one arm of that refusal whose absence would not have shown in
the output tree.
An atlas built from a fraction of a region **looks** like a quarter of an
atlas; a `.lodo`/`.lodi` pair built from a fraction of one loads, passes
`--native-verify --native-verify-corpus`, matches its own three staleness
hashes, and is simply missing most of the worldspace. The pass collects one
`NativePlacement` per drawn reference and one lighting sample per vertex
**inside the chunk pass** (`src/lodgen.cpp:3784` and `:4069`), so it can only
ever see the chunks that were rebaked.

### 4.1 The `.lodj` chunk cache, and why the refusal is gone

That refusal read, in practice, "refuse the only target anybody bakes".
bungo's ruled FO4CS command is `--native <dir>`, everything under
`Data/FO4CSLOD/`; `--incremental` therefore refused the default pipeline and
was usable only on the stock engine target. The gap was the point of lane
INCR1 (2026-09-17).

A skipped chunk now **speaks from a file** instead of being silently missing
from the pair. Every `--native` bake writes, beside the pair,

    FO4CSLOD/<ws>/<ws>.<dim>.<cx>.<cy>.lodj

one a chunk: that chunk's whole contribution to the region's `.lodo`/`.lodi`
-- every placement it emitted, **in emission order**, and the exact lighting
and placement-AO sums it accumulated per object index. On an incremental run
the chunk pass replays each cached chunk **in that chunk's own queue
position**, so the arrival order -- which the library's mesh ids depend on --
is the order a full bake would have produced. Proved, on a real bake of four
chunks: a null incremental and a mixed one (one chunk rebuilt beside three
cached) each rewrote the `.lodo` and the `.lodi` **byte for byte**
(`scratchpad/incr1_20260917/s2_proof.txt`).

Two rules the file obeys, both for the same reason -- the pair must come out
bit-identical, not nearly identical:

* **every number is hex.** Positions, rotations and scale are `%08x` IEEE-754
  bit patterns and the lighting sums are `%016x` doubles. A decimal round
  trip of a float is not an identity.
* **a placement lit by more than one chunk is refused, not hoped through.** An
  arrival is deduped on `(refForm, scolPart)` **across** chunks, so a
  placement two chunks both light has sums built from both, and
  `(prev + a1) + a2` is not `prev + (a1 + a2)` in floating point. Zero is the
  ordinary answer, because lighting is keyed on the chunk's own identity
  index; anything else means the pair would be NEARLY right.

The exact way back is `--no-native-cache`: no `.lodj` is written, and an
`--incremental --native` run then refuses with exactly the words it used
before this lane. It is there so the cost of the cache can be measured, and
so the old behaviour is reachable rather than merely remembered. Measured on
that four-chunk bake: 658,970 bytes for 2,243 placements = **294 bytes a
placement**, 0.29 % of the 225,399,755-byte `.lodo`.

**Written by default: bungo's call, recorded by lane FIX1 on 2026-09-26.**
bungo asked what the `.lodj` files in the mod folder are. Each one is this cache:
one chunk's placements, which the NEXT bake reads back so that a chunk it skips
still reaches the pair. The game never opens one, and neither does any FO4CS
reader (the FO4CS source has no `lodj` in it). The cost is about 294 bytes a
placement. Deleting a `.lodj` is safe: the next `--incremental` rebakes that one
chunk (`with no native chunk cache 1`), and a full bake writes it again. The
default stays ON. `--no-native-cache` is the way to leave it out.

The cache is an output of the FO4CS target like any other, so it is registered
with `lodgenNoteLayoutFile()` and counted in the census's `layout <root>, N
file(s)` clause. `tests/spells/lodgen_layout.sh` leg (f) holds that count
against the files actually on disk (less the bake record, which is written
after the census line is composed), so a `.lodj` that stopped reporting itself
would go red there rather than silently.

### 4.2 The two dirty lists, and why they are two

A dirty chunk goes on `dirty`. Only some dirty chunks go on `dirtyWide`, and
**the widening is seeded from `dirtyWide` alone**.

The widening exists because the terrain ring and the AO skirt each reach one
cell, so a chunk whose **inputs** moved changes what its neighbours draw. A
`.lodj` is not an input to anything: it is this tree's record of what a chunk
once emitted, and losing it changes exactly one chunk's work, because
rebaking that chunk writes the same bytes again.

This was measured the wrong way round first. Deleting **one** cache file took
the ordinary "an output is missing or edited" path, which seeds the widening,
and the census said `4 of 4 chunks dirty` -- one lost cache rebaked the whole
region, and the harness leg that was meant to prove "one chunk rebuilt beside
three cached ones" had no cached ones in it at all. So the output loop now
walks **every** output rather than breaking on the first lost one, and a
chunk whose only lost output ends in `.lodj` goes on `dirty` and not on
`dirtyWide`. After: `1 of 4 dirty, 3 replayed from cache`.

---

## 5. The census line

```
incremental: 4 of 9 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 3 by neighbour, 0 with no native chunk cache)
  (-20,20) inputs 3f2a1c9d0e11 -> 9b70c4a2f1de
native cache: 1 chunk(s) written to .lodj, 3 replayed from cache (1791 placement(s)), 0 failure(s), 0 arrival(s) lit by more than one chunk
```

Read them every run. `9 of 9` means the diff found nothing to skip and the run
is a full bake with extra bookkeeping — true and safe, but not what you asked
for, and the line says so instead of letting a wall-clock reading imply it.

The bracket's **fifth** field, `with no native chunk cache`, counts chunks that
were clean by every other measure and are being rebaked only because their
`.lodj` is not on disk — the first `--incremental` after an older bake, or
after somebody cleaned the folder. It is the field that tells a full-looking
run apart from a genuinely dirty one.

```
native-library-build: reused (previous .lodo kept; base census, load order, plugin corpus and object corpus unmoved, payload checked)
native-library-build: rebuilt (occluders are on and the per-model box is in neither file)
```

`native-library-build:` (lane PERF1, 2026-09-17) appears on every `--native` run and
says whether the object library was BUILT or KEPT, and when it was built, the
one test that decided it — `not offered`, `occluders are on…`, `the load order
moved`, `the plugin corpus moved`, `the object census moved`, `no previous
.lodo…`, `the previous .lodo did not read back: …`, or `…header disagrees with
the record`, first refusal wins. Only an `--incremental` run can be offered the
choice; a full bake always reads `rebuilt (not offered…)`. The saving it reports
is visible in the `stage times:` line's library split, whose `models` cell holds
the payload-checked read when the library was kept.

The `native cache:` line appears only on a `--native` run with the cache on.
`0 replayed from cache` on a run that reported skipped chunks is a defect, not
a performance note: it means the pair was assembled from the rebaked chunks
alone.

---

## 6. The gate

`dirty rebake == full bake`, byte for byte, every file, the `.lodb` included.

The standing gate is **`tests/spells/lodgen_incremental.sh`** (lane INCR1),
six arms, each with its own floor:

| arm | what it measures | floor |
|---|---|---|
| (a) | a null `--incremental --native` rewrites the SAME `.lodo`/`.lodi` | at least one `.lodj` on disk AND at least one chunk replayed |
| (b) | one chunk rebuilt beside cached ones writes the same pair, and a deleted `.lodj` heals itself | at least one chunk rebaked AND at least one replayed |
| (c) | a null `--incremental` on the STOCK target rewrites every file byte for byte | at least one file compared |
| (d) | the record's SUBSTANCE after an incremental equals the full bake's | at least one `chunk` row AND one `out` row; both refuters (a deleted chunk row, a zeroed `out` digest) must fire |
| (e) | `--no-native-cache` writes no `.lodj`, and the incremental then refuses naming the flag | the refusal must name the flag |
| (f) | `lodbNormalise()` and `lodb_read.normalise_file()` agree on the same file, text and line count | the exe must have printed a digest |

The earlier measurement, from lane LAND1, is
`scratchpad/land1_20260912/b3_identity.sh`, with its verdicts in
`scratchpad/lane_land1_report.md` section B3.

Every arm carries its **floor**: `incr == full` is trivially true for an edit
that reached nothing, so each arm also asserts that the edit moved the full
bake's bytes at all. An arm whose floor is empty is reported **VACUOUS**, never
PASS. A check that cannot fail on its input is not a check.

---

## 7. Making a genuinely edited plugin to test with

A loose-file override cannot reach rows 1, 2, 3, 5 or 7 of the dependency map —
those live in the ESM. `scratchpad/land1_20260912/b_esmedit.py` patches a copy
of `Fallout4.esm` in place so nothing downstream can tell it from a plugin a
person edited. Two facts it had to learn:

* **37,019 of 37,020 `LAND` records are zlib-compressed** (flag `0x00040000`:
  u32 decompressed size, then the stream), while **all 1,244,528 `REFR`s are
  not**. Rewriting a compressed record changes its size — which is legal,
  because an ESM has no global offset table: the only length fields are each
  record's `dataSize` and each enclosing GRUP's `groupSize`, and fixing that
  chain up is the whole job.
* **Cell coordinates are not unique across worldspaces.** Cell (-20,20) exists
  in Commonwealth and in a second worldspace of `Fallout4.esm`; a walker that
  ignored the world-children GRUP's label (groupType 1) found two `LAND`
  records for one coordinate and would have edited the wrong one.
