## 2026-09-10 - `.lodl` version 3: water BODIES, flow, shore and a stroke store

`src/lodtfile.cpp`, `src/lodtfile.h`, `src/nifcli.cpp` (the `lodl` and `lodgen`
switches), `src/btdterrain.cpp`/`.h` (the plane list only),
`tests/spells/lodl_water.sh` (new), `docs/LODGEN_BTD_FORMAT.md`.

A `.lodl` could say what water is in a cell and could not say WHICH BODY of
water it is. Sixteen `WATR` forms serve the whole Commonwealth: `ExtLakeWater`
alone paints sixteen separate lakes with one colour and one velocity, and
`ExtOceanWater` paints the harbour and four hundred inland pools. Version 3 adds
a per-body table, a per-texel body-ID plane, a flow plane, a shore-distance
plane and a stroke store, so two lakes can have two colours and a river can have
a direction that is not its form's.

**Nothing moves.** `0x00..0x9F` is what version 2 writes, the header grows by 88
bytes at the end, every section and every absolute block-payload offset slides
by exactly 88, and version 3's own sections are appended AFTER the block data.
Gated both ways: with the module off the Commonwealth is byte-identical to the
shipped 35,953,294-byte file, and `WW_LODL_VERSION=2` with the module ON writes
those same bytes.

**The module raises the version and nothing else does.** `--water-bodies` is
off by default, so a run that does not ask for bodies produces the file it
always produced. That is the fallback floor: a consumer with no version-3 reader
loses nothing.

**Two measured corrections to the spec's own rule D**, both argued in
`scratchpad/lane_water2_report.md` §2 and both in `MISTAKES.md`:

1. **The shore test is EXACT.** The read-only census that produced the spec's
   590 bodies compared point clouds decimated to 4,000 points a body -- 4,000
   of the sea's 21,585,117 texels -- and a decimated distance can only be too
   LARGE. It found 218 of the 545 texel pairs that are within two texels.
2. **A merge in which exactly one side inherits the worldspace type is
   accepted only when the inheriting side is the SMALLER of the two** -- in
   rule C's ADJACENT merge and in rule D's bridge, both. Without it in the
   bridge, the exact test lets the 21.5 M-texel sea absorb a painted marsh that
   passes within two texels of it and the whole Commonwealth ocean comes out
   named `ExtMarshScumWater` (21,587,443 texels, 332 components, five forms);
   without it in rule C, the known-answer control's sea takes the form of the
   painted river reach it TOUCHES at its own height, which is how the second
   half of this was found. Rule C already states the direction ("an inheriting
   component is merged INTO the painted one"); the clause is what makes it
   safe.

Commonwealth, measured: 805 components -> 793 after rule C's merge (12 merged,
1 refused because the inheriting side was not the smaller, 1 with more than one
candidate) -> **346 bodies**, 528 bridge merges accepted, 13 refused.
1 sea, 115 rivers, 230 lakes; 89 bodies at 64 texels or more; 115 under 4
texels, flagged TINY. Flow sources: none 132, form NAM0 182, bed 2, drain 30.
**The per-form TEXEL totals are identical to the read-only census's, form for
form** -- no grouping decision can move them, which is what makes them the
strong half of the gate.

**One container, three planes.** Body ID, flow and shore share a tiled zlib
store, one tile a cell, and a tile whose compressed size is 0 is UNIFORM with
its sample in the size field -- the sea's 21.6 M texels cost sixteen bytes a
cell instead of an inflate.

**The stroke store ships EMPTY AND PRESENT.** A count of zero is not the same as
an absent section: a marking tool can write into a store that exists, and a
consumer can tell "nobody has marked anything" from "this file predates
marking".

**The header size is a table now** (`lodtHeaderBytes`), and the version refusal
runs BEFORE any offset is read. The old `ver >= 2 ? V2 : V1` ternary would have
measured a version-3 file against version 2's 160-byte floor.

New refusals, each with its own sentence and each provoked on a hand-corrupted
file by the harness: a section bit over an empty rate or offset, a record
stride SHORTER than the 48 this reader knows (a LONGER one is the
forward-compatible case and strides past the fields it does not know, which is
the rule the `.lodm` sidecars already use), a record whose `id` is not its
index + 1, and a plane naming a body past the table.

**The viewer** gains `--plane bodyid`, `flow` and `shore`; body ID paints as a
categorical hue, sampled NEAREST because an id is a name and the average of two
names is a third body that does not exist.

**What is NOT done, named:** no marking tool (lane WATER3), so nothing writes a
stroke and no body carries flow source 4, a colour override, a name or a
hand-set class. `EsmWorld` exposes no `WATR` accessor, so the `NAM0` velocities
are read by opening the plugin a second time inside `src/lodtfile.cpp` --
`scratchpad/water2_20260909/ESMDATA_CHANGE_NEEDED.md` has the twenty-line
accessor that retires it. Nothing has been flown in a consumer: FO4CS pins
version 1.
