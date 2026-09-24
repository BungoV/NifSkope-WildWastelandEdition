## Build (BUILD4)

Built and gated by lane BUILD4 on 2026-09-10, after lane CLAMP
(`scratchpad/clamp_20260910/DONE`). Appended, not rewritten. Skills loaded:
`nifskope-ww-resume-pending`, `nifskope-ww-build-verify`,
`nifskope-ww-render-shot`, `ww-texel-picture`.

### B.1 The build

`Fallout4.exe` and `NifSkope.exe` both absent (`rc=1`) before the build and
before every launch below. `qmake NifSkope.pro` rc **0**, `make -j2` rc **0**
(`Nothing to be done for 'first'`). Staleness sweep over all 15 changed files:
**0 stale**; `cmp res/style.qss release/style.qss` in step.

`release/NifSkope.exe` **2026-09-10 01:01:04** against `src/nifskope_ui.cpp`
00:38:50 and `src/lodgen.cpp` 00:40:11 -- lane WATER2's link already carried
this lane's code, and the object check says so rather than inferring it
(`nifskope_ui.o` 00:44:24, `lodgen.o` 00:44:00).

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | 2026-09-10 01:01:04 |
| `src/nifskope_ui.cpp` | 2026-09-10 00:38:50 |
| `src/lodgen.cpp` | 2026-09-10 00:40:11 |
| `tests/spells/lodgen_octahedral.sh` | 2026-09-10 00:54:21 |
| `tests/spells/lodgen_card_arrays.sh` | 2026-09-10 00:53:40 |
| `tools/bake_impostor_cards.sh` | 2026-09-10 00:54:45 |
| `docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_LODM_FORMAT.md` | 2026-09-10 00:57:23 |
| `lodgen_octahedral.log` | 2026-09-10 01:13:15 |
| the three transition pictures | 2026-09-10 01:18 - 01:19 |

### B.2 The gate table, against section 5's pre-registration

| gate | bar | measured | verdict |
|---|---|---|---|
| `lodgen_octahedral.sh` | RESULT PASS | **100 ok, 0 FAIL** | ok |
| `lodgen_card_arrays.sh` | RESULT PASS | **35 ok, 0 FAIL** | ok |
| `lodgen_impostor_cards.sh` | 12 ok, must not move | **12 ok, 0 FAIL** | ok |
| `lodgen_identity.sh` | 8 ok, must not move | **8 ok, 0 FAIL** | ok |
| bake 1 sidecar | `projection ortho` | said orthographic | ok |
| bake 1 `orthofit` | first two within 0.1%, third field 0 | **`orthofit 972.833 972.833 0`** -- the first two agree EXACTLY | ok |
| the perspective control's line | `projection persp` | said perspective | ok |
| the cube's own size | 256 +- 3 units, through `WW_RENDER_ORTHO` -- a different code path | **256.5097** | ok |
| cube spans, run-time table | all 64 frames within 2 texels | **worst 1.78** | ok |
| cube spans, **FROZEN** `prereg_cube.md` | the same bar, against the table written BEFORE the build | **worst 1.87** | ok |
| central asymmetry | <= 0.05 | **0.000** | ok |
| near-edge vs far-edge width | <= 0.05 | **0.000** | ok |
| the `WW_IMPOSTOR_PERSP=1` control | must EXCEED all three | **18.70 texels** (23.19 against the frozen table), **0.495**, **0.850** | ok |
| card array layers | `id1` ortho, `id2` **no key at all** | `{'0004a074': 'ortho', '0004a075': None}` | ok |
| the 19-tree re-bake | 19 baked, 0 failed | **19 baked, 0 failed** | ok |
| its sidecars | 19 of 19 `projection ortho` | **19 of 19** (see B.4) | ok |
| its `orthofit` third field | 0 perspective | **0 of 19** | ok |
| its `.lodm` files | every card `projection ortho` | **18 of 18 ortho, 0 absent** | ok |
| the FO4CS sample set | every card entry ortho, `(absent)` count 0 | **38 card entries, all ortho; no `(absent)` line printed** | ok |
| the transition table | every row within 1 card texel on centre and 2% on both extents, 12 rows | **1 of 12** | **MISS** |
| the ZEROED-OFFSET control | **0 of 12** passing | **0 of 12** | ok |

**The cube proof is the round's strongest result.** It passes against the
pre-registration as PRE-REGISTERED -- not only against the table the harness
recomputes at run time from the sidecar's own `oct` line. Those two tables
differ by at most **0.10 texels**, so the recomputation did not loosen the bar;
and the perspective control fails every one of the three invariants by a factor
of nine or more. `scratchpad/build4_20260910/frozen_cube.py` is that
re-comparison.

### B.3 The transition table, in full

Three trees x two axis views x two distances, each with its zeroed-offset
control. `card` is the arm under test, `ctl` the reader that ignores
`frameOffset`.

| tree | view | distance | arm | one card texel, px | centre off, px | in texels | dx extent | dy extent |
|---|---|---|---|---|---|---|---|---|
| 0003a28b | front | mid | card | 7.68 | 18.18 | 2.37 | 21.85% | 3.33% |
| 0003a28b | front | mid | ctl | 7.68 | 80.69 | 10.51 | 21.85% | 1.83% |
| 0003a28b | front | ring | card | 1.92 | 8.25 | 4.30 | 6.67% | 0.87% |
| 0003a28b | front | ring | ctl | 1.92 | 12.04 | 6.27 | 6.67% | 0.87% |
| 0003a28b | right | mid | card | 7.68 | 69.16 | 9.01 | 4.71% | 2.07% |
| 0003a28b | right | mid | ctl | 7.68 | 85.09 | 11.08 | 4.71% | 0.87% |
| 0003a28b | right | ring | card | 1.92 | 4.74 | 2.47 | 3.32% | 1.30% |
| 0003a28b | right | ring | ctl | 1.92 | 10.61 | 5.52 | 3.32% | 1.30% |
| 0004a074 | front | mid | card | 7.78 | 27.54 | 3.54 | 6.65% | 5.84% |
| 0004a074 | front | mid | ctl | 7.78 | 42.50 | 5.46 | 6.65% | 1.91% |
| **0004a074** | **front** | **ring** | **card** | 1.94 | 1.58 | **0.81** | **0.92%** | **0.43%** |
| 0004a074 | front | ring | ctl | 1.94 | 13.44 | 6.91 | 0.92% | 0.43% |
| 0004a074 | right | mid | card | 7.78 | 40.39 | 5.19 | 8.26% | 8.29% |
| 0004a074 | right | mid | ctl | 7.78 | 3.20 | 0.41 | 8.26% | 4.04% |
| 0004a074 | right | ring | card | 1.94 | 3.35 | 1.72 | 7.50% | 2.17% |
| 0004a074 | right | ring | ctl | 1.94 | 12.54 | 6.45 | 7.50% | 2.17% |
| 00038599 | front | mid | card | 16.49 | 21.00 | 1.27 | 2.84% | 0.00% |
| 00038599 | front | mid | ctl | 16.49 | 25.00 | 1.52 | 2.84% | 0.00% |
| 00038599 | front | ring | card | 4.12 | 2.12 | 0.51 | 2.63% | 0.40% |
| 00038599 | front | ring | ctl | 4.12 | 4.30 | 1.04 | 2.63% | 0.40% |
| 00038599 | right | mid | card | 16.49 | 2.50 | **0.15** | 18.70% | 0.00% |
| 00038599 | right | mid | ctl | 16.49 | 20.50 | 1.24 | 18.70% | 0.00% |
| 00038599 | right | ring | card | 4.12 | 1.12 | **0.27** | 5.56% | 0.40% |
| 00038599 | right | ring | ctl | 4.12 | 3.35 | 0.81 | 5.56% | 0.40% |

**1 of 12** card rows meets both bars. The one that does is 0004a074 front at
the ring distance. Four more meet the centre bar and are turned away by an
extent (00038599 right mid at 0.15 texels but 18.70% on dx; 00038599 front ring
at 0.51 texels but 2.63%; 00038599 right ring at 0.27 but 5.56%; 00038599 front
mid at 1.27 texels).

**The trunk-width table**, the same run's second half:

| tree | source height px | card height px | height diff | source bottom fifth | card bottom fifth | diff | source top fifth | card top fifth | diff |
|---|---|---|---|---|---|---|---|---|---|
| 0003a28b | 941 | 941 | **0.00%** | 226 | 267 | 18.14% | 311 | 591 | 90.03% |
| 0004a074 | 941 | 941 | **0.00%** | 32 | 53 | 65.62% | 224 | 494 | 120.54% |
| 00038599 | 941 | 941 | **0.00%** | 89 | 129 | 44.94% | 66 | 91 | 37.88% |

The HEIGHT matches to **0.00% on all three trees** -- so the card's vertical
world scale is right, which is the thing the orthographic camera was changed to
fix. The WIDTHS are 18% to 121% wider than the source at both the trunk and the
crown, and the three pictures show why in one look: the card's crown is a filled
blob where the mesh is lacy. That is the shape of coverage-threshold dilation --
a twig covering a fraction of a 128-texel frame becomes a whole texel on the way
in and a solid band on the way out -- and it is NOT the projection. Named as a
candidate with its discriminator: re-run the same measurement at two frame sizes
(the dilation scales with the texel, a projection error does not).

### B.4 Two readings that look like misses and are not

1. **`19 of 20` sidecars saying ortho.** The 20th file is `cards/library.txt`,
   the run-level manifest -- `oct 8 / tile 128 / ref 1183.3 / half_aux 0 /
   candidates trees`, no camera and correctly no `projection` line. It is
   **19 of 19** card sidecars. `run.sh` line 57's denominator is
   `ls "$CARDS"/*.txt`, which counts the manifest.
2. **`run.sh` step 2 printed nothing.** Its `sed -n '/bake 4/,$p'` looks for a
   line containing `bake 4` and the harness never writes one -- the cube block
   is labelled by its content, not by a bake number. The numbers were read
   straight out of `lodgen_octahedral.log` instead and are in B.2. Both are
   reporting defects in `run.sh`, not measurement failures.

### B.5 The transition instrument's own sensitivity -- a finding

CONSTITUTION 4, rule 1 of 2026-09-04 21:33: a field must be WRITTEN and must
MOVE. Comparing each card row with its own control:

| column | rows where card and control read the SAME value |
|---|---|
| centre offset | **0 of 12** -- moves on every row |
| dy extent | 8 of 12 |
| **dx extent** | **12 of 12** |

So the control fails only through the centre column. The `2%` extent bars are in
the gate but nothing in the round demonstrates they can tell the card arm from a
reader that ignores `frameOffset` -- and four of the eleven failing rows are
failed by exactly those bars. This does not make the 1-of-12 result wrong; it
means the extent half of the gate has no floor under it yet. Not fixed and not
re-pinned; reported.

### B.6 The pictures (CONSTITUTION 5)

All three opened before being reported:
`cardortho_transition_0003a28b.png` (48,403 bytes),
`_0004a074.png` (18,186), `_00038599.png` (11,892).
Source | card | overlay, blue mesh and orange card, at the ring distance.

**A defect in the pictures themselves**, against `ww-texel-picture`'s caption
rule: the three panel titles are drawn at one y and overlap into an illegible
line across the top of every one of them, and the bottom caption is CLIPPED at
the right edge -- `0003a28b`'s ends mid-number at `ctl: centre 12.04 px = 6.27
card tex`. The numbers quoted in this report were therefore read from
`transition.log`, not from the picture, which is exactly the coupling that rule
exists to prevent. The pictures still show the thing they were made to show.

### B.7 What was NOT done

* **Nothing was committed** (CONSTITUTION 8).
* No fix was landed for the transition miss and no bar was re-pinned.
* CARDFINAL's gap, mip and per-frame gates are inside `lodgen_octahedral.sh`'s
  100 ok and did not move -- so that expectation is now a measurement.
* Skipped harnesses, with the reason: everything in `tests/spells` this change
  does not reach (block viewer, panels, collision, NIF writers). Beyond the
  lane's own list, BUILD4 also ran `render_shot.sh`, `lodl_water.sh` and
  `lodl_open.sh` because lanes HOOKCAM and WATER2 share this build -- their
  numbers are in `scratchpad/build4_20260910/logs/`.
* The three skill files section 9 owed to the LIVE tree
  (`nifskope-ww-render-shot`, `nifskope-ww-commit`, `ww-analytic-fixture-gate`)
  were checked and are already byte-identical in both trees. Nothing owed.
* bungo's open NifSkope window predates the exe and needs a restart.
