# BAKE2 -- DONE (lane BAKE2, worktree E:\Projects\NifskopeWWE-bake2, branch bake2-20260925, not merged, not pushed)

Clock: written 2026-09-25 23:01 (read with `date`). Exe used for every installed file: 62412e83 (release/NifSkope.exe
in the worktree; sources = commit b8d0b957).

## What was asked
Bake pre-war Sanctuary, Far Harbor and Nuka-World with BAKE1 + SEAM1 settings (--dim all, region = the LAND box),
check G1-G5, the no-terrain placement cells and the flat-grey chunks, install each one into
mods\FO4CSLOD\FO4CSLOD\<EDID>, and take two pictures of each (a top-down on the .lodl header bounds and an oblique
over a landmark). Added on the way: ruling (a) (the .lodi wide-scale bit), bungo's halo question, and the
director's fill-ON ruling for the two DLC worldspaces.

## Per worldspace (all three installed; nothing was installed before this lane)

| | pre-war Sanctuary | Far Harbor | Nuka-World |
|---|---|---|---|
| EDID / WRLD | SanctuaryHillsWorld 000A7FF4 | DLC03FarHarbor 03000B0F | NukaWorld 0600290F |
| LAND box / region baked | -25..2 x -9..25 / -28 -12 2 25 | -29..20 x -21..31 / -32 -32 20 31 | -32..32 square / -32 -32 32 32 |
| .lodl header bounds | -28,-13..7,34 | -73,-59..69,78 | -32,-32..32,32 |
| VT levels | 2 (VT.2, VT.4) | 5 (2..32) | 5 (2..32) |
| installed files / bytes | 163 / 517,928,695 | 578 / 1,733,544,111 | 654 / 2,838,424,769 |
| vanilla fill | ON (loose inputs) | ON, grid phase 3,1 | ON, grid phase 0,0 |
| placements drawn | 1350 / 1350 | 27,790 / 27,790 | 18,787 / 18,787 |

Checks (every line a measured verdict; full text in progress.md):
- **G1.** On all three: native-verify rc 0; census check 0 failures, and the --self-floor doctored runs are
  caught (FLOOR ok); the lodb, lodm and lodl readers rc 0; --lodt-check rc 0 on every VT level.
- **G2.** The .lodb lists 47 plugins on all three.
- **G3.** Cards, by plugin index:
  - pre-war: 34 bases, 0 cards. By design: its maples ship their own LOD models for every ring.
  - Far Harbor: 909 bases, 43 with a card (29 from index 00, 14 from 03).
  - Nuka-World: 1158 bases, 26 with a card (all from index 00).
- **G4.** Cover is present and non-uniform on 3 picked cells per world. The red control (no --cover) is BC1 with no
  cover channel on all 3, i.e. RED as it must be.
- **G5.** The mods top level, the MO2 root and the profile files are identical to the before-listings (names,
  sizes, mtimes).
  - FO4CSLOD gained the three folders.
  - 17 Commonwealth files changed at 20:13:47. That was lane TINT1's install (its install_record.tsv); no script
    of this lane writes there.
- **Placement cells with no LAND under them.**
  - pre-war: 15 cells / 101 placements (x -15..-11, y 16..24, east of the NW block; named in progress.md).
  - Far Harbor: 0. Nuka-World: 0.
- **Flat-grey chunks.**
  - LAND chunks: 0 of 18 / 0 of 147 / 0 of 289.
  - VT.4 cells, Far Harbor: 1517 of 3584 before (fill off) -> 0 after.
  - Nuka-World: 2303 flat-grey VT.4 cells, and every one of them lies OUTSIDE the .lodl header, where the terrain
    mesh never samples. They are also no-LAND, and vanilla ships no dim-4 sheet there: its LODSettings grid is 64
    cells from -32, so it stops at 31. Inside the header: 0.
- **Halo gate (halo_gate.py).**
  - pre-war: PASS. Before the fix it was RED: 100 of 180 near cells hot.
  - Far Harbor: PASS (PHASE=3,1).
  - Nuka-World: N/A. Its playable box is all LAND, so no cell exists that could carry a halo.
- **Wide scale on the real Nuka-World bake (widescale_check.py):** W0-W5 PASS, with 4 placements above 7.99988 in
  a v10 .lodi.

Pictures (scratchpad/bake2_20260925/pics/, each checked with frame_check.py: size read back, corners, 20 px inside):
- pre-war:
  - prewar_topdown_after.png, 3200x4264 on the header, INSIDE.
  - prewar_oblique_sanctuary.png.
  - prewar_halo_before_after.png.
- Far Harbor:
  - farharbor_topdown_after.png, 3200x3124 on the header, INSIDE.
  - farharbor_oblique_town.png.
  - farharbor_halo_before_after.png.
- Nuka-World:
  - nukaworld_topdown.png, 3200x3224 on the header, INSIDE. Column 32 and row 32 are background, because the viewer
    draws sheets only on whole 16-cell tiles inside the .lodl.
  - nukaworld_oblique_galactic.png.
- Every oblique's diamond touches the side edges; this is BAKE1's 08 camera, by design.

## The halo (bungo: "What's that glow around the playable area?")
- **It was ours.** Pre-war cells 1, 2 and 3 off the LAND edge read +38, +30 and +19 luminance over vanilla, against
  +11 further out.
- **Cause.** The vanilla fill blended FROM the generator's placeholder grey over a smoothstep band, and that grey
  also sized the band.
- **Fix (src/lodgen.cpp, commit b8d0b957).** A no-LAND cell is filled whole and is left out of the band's p95.
  The gate is RED on the old exe and GREEN on the new one.
- **No side effects on the Commonwealth.** It has 0 flat-grey cells, so it shows no halo. A Commonwealth region
  baked with the rung exe and with the new one gives identical VT.2/4/8 and VT.lodm.
- **Pictures, luminance by distance before -> after:**
  - pre-war: d1 124.5 -> 95.8, d2 116.4 -> 94.8, d3 106.5 -> 96.9, >=6 94.0 -> 94.0.
  - Far Harbor: 158.6 (unfilled) -> 96 flat.

## Rulings carried out under "no calls after approval"
- **Ruling (a), director's, 2026-09-25: the .lodi v10 wide-scale bit (commit 6da2f793).**
  - Instance flag bit 7 = 8 + v/8192, so scales up to 15.99988 fit (the engine caps XSCL at 10.0).
  - A file with no such ref is v7, byte for byte:
    - fixture, rung vs new: 3 files, 0 differ;
    - pre-war .lodi and .lodo: identical to the install;
    - Commonwealth .lodi against a rebake: IDENTICAL (sha1 eaf1fe5e, 34,371,499 B; a whole-Commonwealth objects rebake with exe 62412e83, 4442 s, vs the installed file, which is lane TINT1's since 20:13 and was unchanged before and after the run); the rebake reports 0 of 184,431 placements above 7.99988, max 4.97, .lodi version 7.
  - The installed Commonwealth .lodi was left alone.
  - The FO4CS in-game reader must learn v10. Logged here; not raised as news; FO4CS not edited.
- **Fill-ON for Far Harbor and Nuka-World (director's, 2026-09-25).**
  - Only the vanilla terrain LOD colour tiles (1296 + 256, 512 BC1) and the two LODSettings .LOD files were
    extracted, read-only, from the DLC BA2s into the input cache E:\Tools\Fallout 4\DataUnpacked\Data.
  - Our own reader was used. The manifest, with source archive and sha1 per file, is
    scratchpad/bake2_20260925/dlc_lod_manifest.tsv (1554 lines, uncommitted).
  - Nothing extracted went into FO4CSLOD or git.
- **Found on the way:** Far Harbor's vanilla grid is phase 3,1 (LODSettings SW cell -73,-59).
  - The fill now reads that phase (commit b8d0b957, docs §2.6 in 3ecb8665). Without it, no Far Harbor sheet is ever
    found.
  - Phase 0,0 keeps the old addressing byte for byte.

## Replaced files (backups + sha1)
- pre-war after the halo fix: VT.2, VT.4, .lodb -> replaced/prewar_halo_2100/.
- Far Harbor fill ON: VT.2/4/8/16/32 + .lodb -> replaced/farharbor_fill_2117/ (BACKUP MATCH, INSTALL MATCH).

## Not done / owed
- The FO4CS reader for .lodi v10 (see above).
- No in-game look at any of the three; bungo owns that.
- Avast blocked three new exes this session (50e5d920, 4638a958, and TINT1 saw the same). Each was renamed aside
  and relinked for a new hash. No exclusion was added.

## Skill review
- **New: fo4-terrain-lod-input-cache** (E:\Projects\Claude\.claude\skills and E:\Tools\AISkills, commit d542644).
  It covers:
  - extracting vanilla DLC terrain LOD from the BA2s into the input cache with a manifest;
  - the DX10/GNRL record traps;
  - the LODSettings grid-phase table;
  - the registration check.
- **Updated: ww-whole-map-lod-bake.** Fill ON everywhere, the grid phase, the no-LAND halo, and the halo_gate /
  grey_count checks on every fill-ON bake.
- **Updated: ww-whole-map-picture.** The terrain region must snap to whole sheet tiles inside the .lodl, or the
  viewer silently draws the pale data view (Nuka-World -32..32 snapped to 47 > 32). Grep render logs for "data view".
- **Not a skill line:** "never edit a .sh while an instance of it runs" is a general bash fact, logged in MISTAKES text.
- **Worked as written:** nifskope-ww-worktree-build, ww-lodo-version-bump (its version-bump checklist carried over
  to the .lodi v10 change).
