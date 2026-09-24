P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth2_20260923/DELIVERABLE_TEXT.md'
s = open(P, encoding='utf-8').read()
E = [
 ("- **NEW RED, owed a ruling:** `impostor_draw.sh` row 5 (silhouette IoU vs the mesh, 16 views, the\n"
  "  N=4 blast card) reads **0.3920 < 0.50** at the default now. Same card, same views: flat snap\n"
  "  0.3920 = `WW_IMPOSTOR_BLEND=0` 0.3920; moved snap 0.5323; smooth end 0.6565. A flat frame on a\n"
  "  4x4 grid is up to half a cell off the view. Row not re-pinned, bar not lowered: whether the\n"
  "  row measures the default or names the smooth end (as rows 17/18 do) is the director's call.\n",
  "- **`impostor_draw.sh` row 5, ruled (director 2026-09-23):** on the N=4 blast card the flat snap\n"
  "  reads **0.3920 < 0.50** (same views: `WW_IMPOSTOR_BLEND=0` 0.3920, moved snap 0.5323, smooth\n"
  "  end 0.6565). Kept at its bar as a NAMED KNOWN RED (\"4x4 flat snap is below the outline floor by\n"
  "  construction; not the shipped grid\"). New twin **row 5t** measures the shipped grid, 8x8 at 2k\n"
  "  (bungo 09:3x; DEFAULTS2 flips the default next), at the crisp end, same 0.50 floor: **0.7029\n"
  "  PASS** (TreeMapleInstitute06Green, the lane's BC7 n8_2k card; `IMPOSTOR_LODM_8` /\n"
  "  `IMPOSTOR_NIF_8` override; a missing fixture FAILS, never skips).\n"),
 ("| `impostor_draw.sh` rows 5, 16 | not re-pinned; measure the flat-snap default | 5: IoU 0.5147 (stipple) -> 0.5323 (moved snap) -> **0.3920 FAIL** (flat); 16b 0.940 |\n",
  "| `impostor_draw.sh` row 5 | bar kept; now a NAMED known red (4x4 flat snap, not the shipped grid) | IoU 0.5147 (stipple) -> 0.5323 (moved snap) -> 0.3920 (flat) |\n"
  "| `impostor_draw.sh` row 5t (new) | row 5's twin on the 8x8 2k card at the crisp end, floor 0.50 | 0.7029 PASS |\n"
  "| `impostor_draw.sh` row 16 | not re-pinned; measures the flat-snap default | 16b 0.939 -> 0.940 |\n"),
 ("| impostor_draw.sh (blast_n4) | 32 steps, 1 failure: row 5 IoU 0.3920 < 0.50 (NEW, above); 15/16/17/18 pass |\n",
  "| impostor_draw.sh (blast_n4; gates/impostor_draw.final.out) | 33 steps, 1 failure = row 5 0.3920, the NAMED known red; row 5t 0.7029 PASS; 15/16/17/18 pass |\n"),
 ("  `lodgen_card_arrays.sh`, `impostor_draw.sh` rows 15, 17, 18.\n",
  "  `lodgen_card_arrays.sh`, `impostor_draw.sh` rows 15, 17, 18; new row 5t (8x8 twin of row 5).\n"),
]
for o, n in E:
    assert s.count(o) == 1, o[:70]
    s = s.replace(o, n)
open(P, 'w', encoding='utf-8', newline='\n').write(s); print('ok')
