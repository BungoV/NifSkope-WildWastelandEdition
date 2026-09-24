## 2026-09-11 — The FO4CS "Improved LOD" build plan, written against the files the generator writes today

`docs/FO4CS_IMPROVED_LOD_PLAN.md` (new, 72,969 bytes, 1,195 lines, LF-only). Lane
PLAN-FO4CS. Documents only: nothing was built, no exe was launched, and the FO4CS
repository was read and not written.

**What it is.** The plan the FO4CS session builds Improved LOD from — one module
with its own master switch that ships off (bungo 2026-09-09 15:34), built LAST
against frozen contracts, consuming the `.lodo` / `.lodi` / `.lodl` / `.lodt` /
`.lodm` family, the card sheets, the texture arrays and the manifests. It defines
no byte and no field: where it and a contract page disagree, the contract wins.

**Six rungs, each one FO4CS wave with its own flight.** R0 the loaders and
staleness, nothing drawn. R1 vanilla far objects suppressed, the native far field
drawn, and one shadow representation per placement. R2 terrain — the pyramid
beyond, the runtime blend from the `.lodl` weights in the inner band. R3
screen-size selection, GPU culling, the fades, and the hybrid far shadow pass.
R4 cards. R5 the three 5x5-grid rounds, after R1-R4 have flown. Each rung states
nine things: what it reads, what it does, its switch and INI keys, its fallback
and the word the census prints for it, its census fields, its gates with the
numbers pre-registered from the contracts, its flight, what it must not do, and
its RE candidates named as candidates.

**bungo's shadow rulings of 2026-09-11 14:4x -> 15:0x are in it as law**, in
three places: R1 carries "every placement has exactly one shadow representation
at a time, chosen the same way its draw is chosen" with the cell-range drop-out
and the cascades excluded; R3 carries the one-height-source change (the far
terrain shadow march samples the `.lodl` finest level in the inner band and the
pyramid's resident height sheet beyond, with `HeightMap.dds` as the fallback arm)
and the hybrid default (march the terrain, render only the object cluster cut and
the cards into a far shadow map, combine with a max, map-only as the fallback
arm); §3's seams table carries the grid-edge blend band. **His two millisecond
figures are labelled estimates in both places they appear**, with the first FO4CS
capture round named as what measures them.

**Seventeen owed items, with an owning lane and a "blocks" column** — six of them
block a rung. One is new and this lane found it: the `.lodt` height sheet is
opt-in (`--vt-height`) and the CLI table at `docs/LODGEN_TERRAIN_VT.md` §5 does
not list the flag although §2.2 names it, so a bake that does not ask for it
leaves R3's shadow march with no height source beyond the inner band — and the
sheet more than doubles a tile (184,960 bytes of height against 138,720 for the
three colour-class sheets together). Another is owed to the census page here
rather than to a writer: his two rulings ask for a caster count per SOURCE and
for the march's and the map's own costs, and `docs/LODGEN_CENSUS.md` carries none
of the three.

**Fourteen open rulings for bungo in one list**, four new and ten carried from the
generator lanes. The first is the one that changes what gets built: **build the
object library from each base's near `MODL` instead of its `MNAM` slots?** The
ladder is correct and barely selectable — the median level-1 cluster deviates
3.80 percent of its model's diagonal and reaches one pixel only past 52,100
units, because "full detail" in the library is already Bethesda's LOD mesh at a
mean of 47.7 triangles for a whole building. The plan says R5c should not be
chartered before that call is taken, or it ships a mechanism with nothing to
select and the flight reads as a failure of the mechanism.

**Gates, all three green with floors that fired**
(`scratchpad/plan_fo4cs_20260911/plan_gate.py`, read-only, every floor on an
in-memory copy): G1 **73 numbered citations across eight contract pages plus 4
named headings, 0 unresolved** (floor: a `NATIVE 99.99` and a nonexistent `BTD`
heading, 2 caught); G2 **6 rungs, 0 short of the nine parts** (floor: one label
removed from R0, 1 caught); G3 **158 backticked identifiers, 38 keys the page
declares itself, 0 found in no contract** (floor: an invented token, 1 caught).
G2 found R5 genuinely short of its READS and DOES blocks and it was written.
G3 found two defects in ITSELF — a token splitter that took `.` before `[` and so
accused `rep[0..3]`, which `docs/LODGEN_NATIVE_LODO_LODI.md` §4.4 uses verbatim —
and one in the page.

**Provenance.** All eight contract hashes were taken before the pages were read
and re-derived after the page's last edit; **none moved**. The version constants
were re-read last of all, and the second question that step asks produced a real
finding that is in the page twice: **FO4CS's shipped `.lodl` parser pins
`kVersion = 1u` and refuses versions 2 and 3**, with `WW_LODL_VERSION=1` as the
zero-effort way back that needs no rebuild on our side. `HANDOFF.md` moved during
the lane (`81fdc08dfa8b7707` -> `8a7fce2f67c7f898`) because the director spliced
the shadow rulings in, and the footer states both values rather than one.

The page carries **no `src/` line number in either repository** — lane BAKEPERF1
owned `src/` here while it was written and the FO4CS tree is another project's
live worktree — so every claim is anchored to a contract section or to a ruling
paragraph quoted verbatim.
