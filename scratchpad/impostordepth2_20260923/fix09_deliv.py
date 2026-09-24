P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth2_20260923/DELIVERABLE_TEXT.md'
s = open(P, encoding='utf-8').read()
E = [
 ("Exe `release/NifSkope.exe` built 12:08:05, 24,089,088 B, sha1 f7403e44 (BUILD-RC=0).\n",
  "Exe `release/NifSkope.exe` built 13:43:42 after qmake, 24,090,624 B, sha1 6b8ed793 (BUILD-RC=0) --\n"
  "the flat snap. The earlier f7403e44 (12:08:05) had the depth-moved snap at 0.\n"),
 ("- qmake run (it was owed for the new `src/lodgenbc7.h`): Makefile.Release now names it.\n",
  "- qmake run (it was owed for the new `src/lodgenbc7.h`): Makefile.Release now names it.\n"
  "- **NEW RED, owed a ruling:** `impostor_draw.sh` row 5 (silhouette IoU vs the mesh, 16 views, the\n"
  "  N=4 blast card) reads **0.3920 < 0.50** at the default now. Same card, same views: flat snap\n"
  "  0.3920 = `WW_IMPOSTOR_BLEND=0` 0.3920; moved snap 0.5323; smooth end 0.6565. A flat frame on a\n"
  "  4x4 grid is up to half a cell off the view. Row not re-pinned, bar not lowered: whether the\n"
  "  row measures the default or names the smooth end (as rows 17/18 do) is the director's call.\n"),
 ("| `impostor_draw.sh` rows 5, 15, 16 | not re-pinned; now measure the snap default | 5: IoU 0.5147 -> 0.5323; 15: 0.8987/0.8979 -> 0.9108/0.9108; 16b 0.939 -> 0.940 |\n",
  "| `impostor_draw.sh` row 15 | the parallax-ON run names `WW_IMPOSTOR_SLIDER=1` (the flat default would make the row trivially equal) | off 0.9108, on 0.9102, gap bar 0.01 unchanged |\n"
  "| `impostor_draw.sh` rows 5, 16 | not re-pinned; measure the flat-snap default | 5: IoU 0.5147 (stipple) -> 0.5323 (moved snap) -> **0.3920 FAIL** (flat); 16b 0.940 |\n"),
 ("| `impostor_trunk.sh` | rewritten for the rulings; tear references pinned to DEPTH1's measured drawer | -- |\n",
  "| `impostor_trunk.sh` | rewritten for the rulings; tear references pinned to DEPTH1's measured drawer; crisp rows = flat snap, tear row a named KNOWN RED (bungo 13:1x) | bars unchanged |\n"),
 ("## Gates (exe f7403e44, outputs in gates/)\n\n| gate | result |\n|---|---|\n"
  "| impostor_trunk.sh (new) | 38 checks, 5 failures: crisp el0 T1; crisp el20 T1, T2, tear; smooth el20 T2 (2 views) |\n",
  "## Gates (exe 6b8ed793, the flat snap; outputs gates/*.flat.out)\n\n| gate | result |\n|---|---|\n"
  "| impostor_trunk.sh | 38 checks, 3 failures: crisp tear el 0 4.68% and el 20 10.14% (KNOWN RED, bungo 13:1x); smooth el 20 T2 on 2 of 360 views (OPEN). Crisp T1 0 of 360 outside at both el, T2 ok; jumps 15.25 / 64.36 px reported |\n"
  "| impostor_draw.sh (blast_n4) | 32 steps, 1 failure: row 5 IoU 0.3920 < 0.50 (NEW, above); 15/16/17/18 pass |\n"
  "| impostor_aa.sh | 7 checks, 0 failures |\n"
  "| impostor_shrubs.sh | RESULT PASS |\n"
  "| not re-run | lodgen_octahedral / card_arrays (no lodgen object rebuilt), native_lighting (draws no card) -- their f7403e44 results below stand |\n\n"
  "## Gates (exe f7403e44, the depth-moved snap at 0; outputs gates/*.new.out)\n\n| gate | result |\n|---|---|\n"
  "| impostor_trunk.sh (new) | 38 checks, 5 failures: crisp el0 T1; crisp el20 T1, T2, tear; smooth el20 T2 (2 views) |\n"),
 ("Crisp end, worst per-step jump (T3, reported not gated): el 0 3.38 px (3.5x the mesh's), el 20\n20.35 px (19x).\n",
  "Worst per-step jump (T3, reported not gated): flat snap (now the crisp end) el 0 15.25 px, el 20\n"
  "64.36 px; the moved snap was 3.38 / 20.35 px.\n"),
]
for o, n in E:
    assert s.count(o) == 1, o[:70]
    s = s.replace(o, n)
open(P, 'w', encoding='utf-8', newline='\n').write(s); print('ok')
