---
name: ww-lodt-offline-decode-compare
description: Prove two .lodt terrain-sheet containers (an old exe's and a new exe's bake of the same box) decode to the same texels, every tile x sheet x mip, and draw full-size before/after sheet pictures -- all in Python, with no NifSkope run. Use when a .lodt storage change must be shown "byte-identical after decode", or when no headless NifSkope can be launched (turn lock taken or refused) and a picture is still owed.
---

# .lodt offline decode compare (lane FLAT2, 2026-09-27)

Written when the harness refused every command outside the lane worktree (FIX1's `turn.sh` included), so no
NifSkope could run and the in-app render of the sea edge was impossible. The sheets themselves could still be
compared and pictured from the bytes.

## Readers (never the writer's arithmetic)
* `tests/spells/lodgen_vt_check.py` `Lodv(path)`: header, table, `payload(i)` = the tile's FULL payload
  (zlib inflated, one-value sheets expanded), `sheetOffset(cover, s, mip)`, `sheetMipBytes`, `unitBytes`,
  `uniformMask(entry)`, `expand(entry, stored)`. Reads the whole file into memory: fine for a region bake,
  not for a whole-map level (use `vtread.Lodt`, mmap, for those).
* `vtread.py` (lane VTBAKE1; a copy lives in `scratchpad/flat2_20260927/work/`): `decode_bc1_blocks(blk, four)`
  and `decode_bc3_alpha(blk)`. BC3 = alpha half bytes 0..7, colour half 8..15 decoded with `four=True`.
* The codec is chosen PER TILE: cover bit (flag 2) picks `dxgiCover` over `dxgi` on the carrier sheet.
  Role 4 = R16 height (no codec), role 7 = RGBA8 horizon.

## The compare
For each present tile: `rb = old.payload(i)`, `ra = new.payload(i)`; count tiles with `ra != rb`; then decode
every sheet x mip of both and count texels where any channel differs. Expected 0 and 0.
**Control first**: flip one byte of one collapsed record in the new stored bytes before expanding; the count
must go non-zero (FLAT2: 1 tile, 348,480 texels). A compare that is never red proves nothing.

## The pictures (bungo's rules)
One sheet a file, full size (finest level: tilesX x content texels a side, border cropped, north up = row 0),
a 60 px black title bar saying what it is and which exe, one fixed ramp shared by before and after (height:
the BEFORE sheet's min..max), a difference picture (black = equal, red = differs, count in the title), and a
per-tile map whose legend is measured back from the saved PNG (pixels per colour = tiles x cell area).
Pictures of decoded game terrain stay out of git (the lane's `pics/` is ignored).

## What it does NOT prove
It shows the FILES agree. It does not show that NifSkope's viewer or FO4CS reads them the same way; say
"not measured" for the in-app render until a headless NifSkope run under the turn lock has shown it.

## Traps met
* `lodgen_vt_check.py header` FAILs "the geometry is the one the document fixes" on any `--vt-density 16`
  bake (content 512, not 256). The RUNG file fails it identically: it is the checker's fixed number, not the change.
* The harness in a night lane may refuse: `$VAR` in a Bash command ("simple_expansion"), `Set-Location` in a
  compound PowerShell command, `>` redirection in PowerShell, and any script path outside the worktree. Pass
  absolute paths as literal arguments and print to stdout.

Script: `scratchpad/flat2_20260927/sea_pics.py <old vt dir> <new vt dir> <out dir>` (branch flat2-20260927).
Env `EDID=<worldspace edid>` (default Commonwealth) picks the files; `COMPARE_ONLY=1` runs the compare and its
control and skips the pictures (used for Nuka-World, 2026-09-28: 83.6 M texels, 0 differ, control red).
