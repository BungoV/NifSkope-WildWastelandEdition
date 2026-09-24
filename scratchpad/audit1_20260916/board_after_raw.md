| gate | rc before / after | s before / after | checks/fails after | ok / FAIL lines before | ok / FAIL lines after | moved |
|---|---|---|---|---|---|---|
| `bakerec` | 0 / 0 | 243 / 301 | 0 block(s): 0/0 | 69 / 0 | 69 / 0 | same |
| `btofree` | 1 / 0 | 123 / 141 | 1 block(s): 27/0 | 18 / 4 | 27 / 0 | **MOVED** |
| `byte_gate` | 1 / 1 | 1969 / 1866 | 1 block(s): 137/0 | -- / 3 | 0 / 0 | **MOVED** |
| `card_arrays` | 0 / 0 | 5 / 5 | 0 block(s): 0/0 | 35 / 0 | 35 / 0 | same |
| `defaults` | 0 / 0 | 757 / 951 | 1 block(s): 31/0 | 28 / 0 | 31 / 0 | same |
| `farring` | 0 / 0 | 34 / 29 | 0 block(s): 0/0 | 21 / 0 | 21 / 0 | same |
| `ground_cover` | 1 / 1 | 25 / 24 | 2 block(s): 50/7 | 43 / 7 | 43 / 7 | same |
| `identity` | 0 / 0 | 2 / 2 | 0 block(s): 0/0 | 8 / 0 | 8 / 0 | same |
| `impostor_cards` | 0 / 0 | 3 / 3 | 0 block(s): 0/0 | 12 / 0 | 12 / 0 | same |
| `incremental` | 0 / 0 | 174 / 162 | 0 block(s): 0/0 | 10 / 0 | 11 / 0 | same |
| `ladder` | 0 / 0 | 125 / 122 | 4 block(s): 32/0 | 40 / 0 | 40 / 0 | same |
| `layout` | 0 / 0 | 865 / 852 | 0 block(s): 0/0 | 23 / 0 | 23 / 0 | same |
| `merge` | 1 / 0 | 20 / 19 | 0 block(s): 0/0 | 9 / 1 | 12 / 0 | **MOVED** |
| `native` | 1 / 0 | 170 / 167 | 7 block(s): 315/0 | 123 / 2 | 133 / 0 | **MOVED** |
| `native_baseline` | 0 / 0 | 11 / 11 | 0 block(s): 0/0 | 3 / 0 | 3 / 0 | same |
| `octahedral` | 1 / 1 | 69 / 67 | 0 block(s): 0/0 | 108 / 3 | 110 / 1 | **MOVED** |
| `panel_run` | 0 / 0 | 45 / 42 | 1 block(s): 137/0 | 137 / 0 | 137 / 0 | same |
| `perf` | 0 / 0 | 549 / 597 | 0 block(s): 0/0 | 11 / 0 | 11 / 0 | same |
| `resources` | 0 / 0 | 3 / 3 | 1 block(s): 4/0 | 4 / 0 | 4 / 0 | same |
| `roads` | 1 / 0 | 21 / 20 | 1 block(s): 13/0 | 10 / 1 | 13 / 0 | **MOVED** |
| `stage_times` | 1 / 0 | 34 / 38 | 1 block(s): 18/0 | 15 / 2 | 18 / 0 | **MOVED** |
| `terrain` | 0 / 0 | 13 / 12 | 1 block(s): 26/0 | 26 / 0 | 26 / 0 | same |
| `terrain_pbrm` | 0 / 0 | 34 / 8 | 1 block(s): 14/0 | 14 / 0 | 14 / 0 | same |
| `terrain_vt` | 0 / 0 | 56 / 53 | 2 block(s): 58/0 | 112 / 0 | 112 / 0 | same |
| `texture_arrays` | 0 / 0 | 7 / 7 | 0 block(s): 0/0 | 40 / 0 | 40 / 0 | same |
| `tree_sway` | 0 / 0 | 8 / 8 | 1 block(s): 4/0 | 4 / 0 | 4 / 0 | same |
| `water_subdiv` | 0 / 0 | 4 / 4 | 1 block(s): 7/0 | 7 / 0 | 7 / 0 | same |

27 gate(s) in the after board, 7 of them moved
  btofree          rc 1 -> 0, FAIL lines 4 -> 0
  byte_gate        rc 1 -> 1, FAIL lines 3 -> 0
  merge            rc 1 -> 0, FAIL lines 1 -> 0
  native           rc 1 -> 0, FAIL lines 2 -> 0
  octahedral       rc 1 -> 1, FAIL lines 3 -> 1
  roads            rc 1 -> 0, FAIL lines 1 -> 0
  stage_times      rc 1 -> 0, FAIL lines 2 -> 0
