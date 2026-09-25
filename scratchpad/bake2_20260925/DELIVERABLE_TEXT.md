## HANDOFF text

### Lane BAKE2 (2026-09-25): pre-war Sanctuary, Far Harbor and Nuka-World LOD, baked and installed

- **Branch.** bake2-20260925 (worktree E:\Projects\NifskopeWWE-bake2). Not merged, not pushed.
- **Code commits.** 6da2f793 (.lodi v10 wide-scale bit, ruling (a)); b8d0b957 (VT fill: no-LAND cells filled
  whole, vanilla grid phase from LODSettings); 3ecb8665 (docs §2.6).
- **Installed** into mods\FO4CSLOD\FO4CSLOD, every file sha1-checked, from exe 62412e83:

  | worldspace | files | bytes |
  |---|---|---|
  | SanctuaryHillsWorld | 163 | 517,928,695 |
  | DLC03FarHarbor | 578 | 1,733,544,111 |
  | NukaWorld | 654 | 2,838,424,769 |

  All three have the vanilla fill ON.
- **Fill inputs for the two DLCs.** Vanilla terrain LOD colour and the LODSettings files, extracted read-only into
  E:\Tools\Fallout 4\DataUnpacked\Data. The sha1 manifest is in the lane folder.
- **Owed.**
  - The FO4CS reader must read .lodi v10 (bit 7 = 8 + v/8192). Only Nuka-World writes v10 today (4 cliffs).
  - bungo's in-game look at the three worldspaces.
- **Pre-war** has 15 placement cells (101 placements) with no LAND under them, at x -15..-11, y 16..24.
- **Nuka-World grey cells.** 2303 flat-grey VT cells lie outside the .lodl header, so they are never drawn.
  Vanilla ships no LOD there.
- Detail: scratchpad/bake2_20260925/DONE.md and progress.md.

## WW_CHANGES text

## BAKE2: three more worldspaces, .lodi v10, and the fill halo

2026-09-25 BAKE2: pre-war Sanctuary, Far Harbor and Nuka-World are baked and installed in FO4CSLOD, all rings, with
the vanilla colour fill on.
- **.lodi v10.** A placement scaled above 7.99988 is written with instance flag bit 7 (8 + v/8192, up to 15.99988)
  instead of refusing the whole file. Nuka-World has 4 cliffs up to 9.97. A file without such a placement is
  unchanged byte for byte.
- **The glow round pre-war's playable block is gone.** Cells with no LAND record were blended from the generator's
  placeholder grey. They now take vanilla's colour whole. Measured by picture brightness 1 cell out from the LAND
  edge: 124.5 -> 95.8 on pre-war, against 94.0 further out. The Commonwealth is unchanged (every cell has LAND).
- **The fill finds Far Harbor's vanilla LOD.** Its grid starts at cell -73,-59, so it is not on multiples of 4.
  The phase is now read from LODSettings\<WS>.LOD. Far Harbor's flat-grey cells went 1517 -> 0 of 3584.
- **Tools.** scratchpad/bake2_20260925: bake.sh, verify.sh, halo_gate.py, grey_count.py, grey_split.py,
  halo_pics.py, ba2_terrain.py, widescale_check.py.

## MISTAKES text

## 2026-09-25 -- lane BAKE2 (lane text)

- **Edited a running shell script.**
  - What was done: I edited halo_runs.sh while an instance of it was running.
  - What was true: bash reads a script as it goes, so the running instance executed half-old, half-new lines and
    the run was corrupted.
  - How it was found: the output lines did not match the script.
  - The rule: never edit a .sh while it runs; copy it or wait.
- **First pre-war bake on the raw LAND box.**
  - What was done: I baked pre-war on its raw LAND box (-25 -9 2 25).
  - What was true: the VT ladder needs the region's west and south edges to divide by the dim, so only VT.2 was
    written.
  - How it was found: the census said so.
  - The rule: widen west/south to a multiple of 32 within the CELL bounds before a VT bake (now in the
    ww-whole-map-lod-bake skill).
- **BA2 reader bugs, caught before any file was used.**
  - What was done: I read the cubemap flag as a u16 at byte 22, and packed the DDS header with wrong counts.
  - What was true: the flag is byte 22 only.
  - How it was found: the header length and flag asserts.
  - The rule: assert the rebuilt header length and read BA2 flags byte-wise.
- **Nuka-World top-down: the colourless data view.**
  - What was done: I rendered the first Nuka-World top-down on its full -32..32 box.
  - What was true: the box could not snap to sheet tiles inside the .lodl. The viewer drew the pale data view and
    still returned rc 0.
  - How it was found: the picture looked pale, and the log said "data view".
  - The rule: grep every render log for "data view" (now in the ww-whole-map-picture skill).
- **Halo gate on an all-LAND region.**
  - What was done: I ran halo_gate.py on Nuka-World's all-LAND box.
  - What was true: with no no-LAND cell, the gate crashed on empty means instead of saying there was nothing to test.
  - How it was found: the traceback.
  - The rule: it now prints N/A. A gate must say when it has no subject.
