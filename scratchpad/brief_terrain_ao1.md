# Lane TERRAIN-AO1 -- the far terrain receives ambient occlusion from the placed objects

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`. Nothing is committed. Exe at launch: from INCR1's DONE line (director fills: ______). Rung ONCE: `release/NifSkope.before_terrain_ao1.exe`. Markers `scratchpad/terrain_ao1_20260911/BUILDING` / `DONE`. Every timestamp from `date +%H:%M`. One NifSkope instance; game check before the link; bungo's window renamed aside. Region bakes only, own out-dir.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block -- CLOCK CORRECTION, the "LANE TERRAIN-AO1 QUEUED" paragraph (bungo 15:4x: objects get AO from terrain and each other; terrain AO is horizon-from-height only; "Okay, so the AO can be acurate from objects" = do it), the TERRAIN-R block (the mask sheet: B = AO), the INCR1 block (the ledger: a new AO term is a new dependency in the map -- the reach of the object AO widens dirty sets; you must update the dependency map INCR1 wrote, with its own gate re-run); `docs/LODGEN_BTD_FORMAT.md` "Ambient occlusion" (the .lodl plane: horizon from height, 8 directions, the formula, `--refresh-ao` byte-identical rule), `docs/LODGEN_TERRAIN_VT.md` (mask B), `docs/LODGEN_LEDGER_FORMAT.md` (INCR1's); `src/lodgen.cpp` the per-placement AO scene (~3836 `LodgenAoScene`: the assembled chunk + heightfield + skirt occluders; the nine-direction escape test ~3300) -- this is the machinery, aimed the other way; `MISTAKES.md` root from 2026-09-11.
- Skills: `nifskope-ww-lodgen`, `ww-control-calibration`, `ww-texel-picture`, `nifskope-ww-render-shot`, `fo4cs-census-field`, `ww-anchored-hookup`, `nifskope-ww-build-verify`, `ww-contract-provenance`, `ww-sheet-diff` (the reach of the change across chunk borders is exactly its subject).

## The work
1. **State the law before code**: for each terrain texel (pyramid level 0 density, and the .lodl plane's 8-per-cell density), occlusion from the placed objects = the same eight-direction horizon march the terrain already does against its own heights, but with the objects' geometry added to the height field -- the simplest honest form is a per-texel "object height above ground" map rasterised top-down from the instance library (cluster spheres or level-0 clusters; state which and why), then the existing horizon formula over (terrain height + object height). Combine with the existing terrain-only term by multiplication (both are visibility fractions). Reach = the march's longest step in world units -> the dependency-map entry for INCR1.
2. **Where it lands**: the pyramid mask sheet's B channel (TERRAIN-R's AO) at every level (coarser by the same filter), AND the .lodl AO plane (the runtime's ring-0 source), both behind one switch `--terrain-object-ao` (off = byte-identical to the rung, the fallback). `--refresh-ao`'s byte-identity rule must survive: state how (the object term is part of the refresh input, or refresh refuses when the switch was on).
3. **Gates**: a region with no placements is byte-identical to the rung with the switch on (floor for the term); a tile under a known building reads darker than the same tile with that building removed (a test plugin that disables the ref, per INCR1's edit machinery), by more than a stated floor; the horizon-only term is unchanged where no object reaches (cmp the far half of a tile); the sky-visibility invariant from the BTD contract (AO = skyVis on a heightfield, r = 0.969) is re-measured and stated if the object term breaks it; census: object-AO texels touched, mean darkening, refusals by name.
4. **INCR1's dependency map** gains the reach; INCR1's identity gate (incremental == full) is re-run with the switch on for the one-cell edit -- it MUST still hold, or the reach is wrong.
5. Build, then the chain at the baselines from INCR1's block; switch off == rung bytes.
6. Pictures: `cmp_terrain_ao.png` (a settlement tile: AO channel before | after | difference x4; the colour sheet lit by it before | after), a render-hook top view of the region with and without the term. Described before cited.
7. Documents: `scratchpad/terrain_ao1_20260911/WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (entries start with `## `); report `scratchpad/lane_terrain_ao1_report.md`; contract amendments (BTD AO section, VT mask B, LEDGER dependency map) with provenance.

## Gates
- A1 law stated before code, with the reach in units.
- A2 no-placement region byte-identical; building-removed floor fires; far half unchanged.
- A3 INCR1's identity gate holds with the term on.
- A4 switch off == rung bytes; chain at baseline.
- A5 exe newer than every changed file; drivers rebuilt; rung == launch bytes; no NifSkope left running.

## Rules
- One build (+ counted relinks). No change to the object-side AO. Never his installed files. Never `git stash`, never commit. Plain language.

## Report
`scratchpad/lane_terrain_ao1_report.md`, incremental (PENDING.md first past half context): `## 0. Pre-registered gates`, `## 1. The law and the reach`, `## 2. The change`, `## 3. Build and gates` (incl. INCR1's re-run), `## 4. Pictures`, `## 5. Owed / red / bungo's calls`, `## 6. Mistakes`, `## 7. Finished-work skill review`.
