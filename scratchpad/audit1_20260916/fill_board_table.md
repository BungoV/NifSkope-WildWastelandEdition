The board is read by the same two instruments section 1.1 used -- each gate's
own `N checks, M failures` summary where it prints one, and the `ok` / `FAIL`
line counts either way -- and the BEFORE column is parsed out of section 1.1's
own table by `scratchpad/audit1_20260916/mk_board_after.py` rather than retyped,
so a row cannot move by transcription.

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

**24 of 27 gates are green and every one of the three reds is a red that was
pre-registered before this lane built anything.**

| red | FAILs | whose |
|---|---|---|
| `lodgen_ground_cover` | 7 | **KNOWN**, the four grass-feature ground-cover failures the brief names, unchanged: 43 ok / 7 FAIL before and after, same lines |
| `lodgen_byte_gate` | 3 | **KNOWN**, named by two LANDED lines. Identical failure set to the before run: the same missing `release/NifSkope.before_panel1.exe` rung, the same two `DIFFERS` rows (`Commonwealth.4.-20.24.DDS`, `Commonwealth.lodi`) at the same byte sizes, the same `byte gate failures: 3` |
| `lodgen_octahedral` | 1 | **KNOWN**, CARDWIDTH: `F1 ... worst 1.74` texels. This is the ONE of its three before-FAILs that was not a stale gate -- the other two were LAYOUT1 moving the card path, fixed in section 1.3, and they are green now |

**Seven rows moved, and every one of them moved in the direction the fix
predicted.** Five went from red to green, one lost two of its three FAILs, and
one moved only in the instrument:

| gate | before | after | why |
|---|---|---|---|
| `btofree` | rc 1, 4 FAIL | **rc 0, 27 checks / 0** | **standing red 2 of the addendum**, adjudicated STALE in 1.3: the sweep predated INCR1's `.lodj` and BAKEREC1's `.lodb` |
| `native` | rc 1, 2 FAIL, 309 checks | **rc 0, 315 checks / 0** | **standing red 1**, adjudicated STALE in 1.3 (`--native-mesh-report` is opt-in). Its section 14 -- this lane's own four new checks against C1 and C2, red by design until the source fix -- is now `ok (lodi-wrap)`, `ok (lodo-wrap)`, `ok (agg-views)`, `ok (agg-record)`, each refused BY NAME |
| `stage_times` | rc 1, 2 FAIL | **rc 0, 18 checks / 0** | **standing red 3**, adjudicated STALE in 1.3: the `.lodo`/`.lodi` pair moved under `FO4CSLOD/` |
| `merge` | rc 1, 1 FAIL | **rc 0, 12 ok / 0** | STALE, unattributed until this lane: object identity is OFF by default since DEFAULTS1 |
| `roads` | rc 1, 1 FAIL | **rc 0, 13 checks / 0** | STALE, unattributed until this lane: the bake record was swept in with the outputs |
| `octahedral` | rc 1, 3 FAIL | rc 1, **1 FAIL** | two of the three were LAYOUT1's moved card path, fixed; the third is CARDWIDTH, above |
| `byte_gate` | rc 1, 3 FAIL | rc 1, "0 FAIL" | **the instrument, not the gate.** This gate spells its failure `FAIL: no exe at ...` with a colon, and the counter's pattern wants whitespace after the word, so it reads 0 where the gate itself says `byte gate failures: 3`. Counted by hand the two runs are identical, file for file and byte for byte. Reported rather than silently corrected, because a counter that can read a red as a green is worth knowing about |

Seconds are not reproducible run to run and are not evidence here: `bakerec`
243 -> 301, `defaults` 757 -> 951, `terrain_pbrm` 34 -> 8. Nothing else was
running on the machine in either pass; the runner does its own `tasklist` check
before every gate and would have aborted the batch otherwise.

`incremental` shows 10 ok before and 11 after with no change in rc, and that is
this lane's own doing rather than drift: its leg **(g)**, the red-before gate for
F6, was added after step 1's board was taken. It reads
`(g) it refuses (rc=2), names the flag, and writes nothing` on this exe.
