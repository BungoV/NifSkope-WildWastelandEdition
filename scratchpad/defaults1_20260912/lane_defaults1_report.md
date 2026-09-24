# Lane DEFAULTS1 — bungo's four rulings of 2026-09-12 go into the code

Tree `E:/Projects/NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` **2026-09-12 19:45:31, 22,275,072 B**, sha1
`40cbdec82af64fb19d85d98140f910827a1e038e` (NATIVEVIEW1's).
Rung `release/NifSkope.before_defaults1.exe`, copied 20:2x, **same sha1**, byte-identical
to the launch exe.

This lane is the chartered exception to "lanes never change defaults". It carries
exactly four rulings and no other default moves.

Written incrementally. Every number carries the floor or the refuter it is judged against.

---

## 0. Where each default lived

Every place a default lives: the CLI variable, the option-struct field, the generator
static, the panel row's initial value, the panel row's saved QSettings value, and the
help text that says DEFAULT.

### (a) Object identity — `--identity` becomes the opt-in

| what | old | new | file:line |
|---|---|---|---|
| CLI variable | `lgIdentity = true` | `false` | `src/nifcli.cpp:6383` |
| CLI switch | only `--no-identity` | `--identity` added, `--no-identity` kept | `src/nifcli.cpp:6570-6574` |
| option struct | `LodgenObjectOptions::identity = true` | `false` | `src/lodgen.h:608` |
| panel row | `identityCheck->setChecked( true )` | `false`; label "Identity channels and manifests" → "Identity channels" | `src/lodgenmanager.cpp:831-840` |
| panel target hook | FO4CS target ticks it | FO4CS target leaves it unticked | `src/lodgenmanager.cpp:~1930` |
| help text | "with identity (default) vertices carry…" | "--identity (DEFAULT OFF since 2026-09-12)" | `src/nifcli.cpp:5810, 5820-5827` |
| panel QSettings | not saved (constructed each time) | unchanged — nothing to migrate | `saveSettings()` never writes it |

### (b) Terrain identity — `--terrain-identity` becomes the opt-in

| what | old | new | file:line |
|---|---|---|---|
| CLI variable | `lgTerrainIdentity = true` | `false` | `src/nifcli.cpp:6449` |
| CLI switch | `--terrain-identity` / `--no-terrain-identity` | unchanged, both kept | `src/nifcli.cpp:6998-6999` |
| option struct | `LodgenTerrainOptions::terrainIdentity = true` | `false` | `src/lodgen.h:578` |
| panel row | `terrainIdCheck->setChecked( true )` | `false` | `src/lodgenmanager.cpp:1141` |
| panel target hook | FO4CS target ticks it | FO4CS target leaves it unticked | `src/lodgenmanager.cpp:~1930` |
| help text | none existed | a `--terrain-identity` / `--no-terrain-identity` entry added, saying DEFAULT OFF | `src/nifcli.cpp:5853-5859` |
| panel QSettings | not saved | unchanged | `saveSettings()` never writes it |

### (c) The land default = LAND1's panel (c)

The argv behind panel (c) of `a_land_guide_*.png`, read from
`scratchpad/land1_20260912/a_pics_bake.sh` line 20 (the script that made the picture),
not reconstructed:

```
--road-detail 1 --land-sample stochastic --land-hex 256 --land-mip-bias -0.22 \
                --land-guide flatwarp:1.0 --land-warp 341
```

`--land-sample stochastic` itself only sets hex 256, warp amplitude 0 and mip bias
-0.22; the `--land-warp 341` that follows it overrides the amplitude. So the picture's
effective state, and therefore the new default, is **hex 256, warp amplitude 341, mip
bias -0.22, guide FLATWARP at strength 1.0**, with the warp lattice (1024) and octave
count (1) already at those values and NOT moved.

| what | old | new | file:line |
|---|---|---|---|
| static, warp amplitude | `g_landWarpAmp = 0.0f` | `341.0f` | `src/lodgen.cpp:5952` |
| static, mip bias | `g_landMipBias = 0.0f` | `-0.22f` | `src/lodgen.cpp:5955` |
| static, hex size | `g_landHexSize = 0.0f` | `256.0f` | `src/lodgen.cpp:~6377` |
| static, guide rule | `g_landGuideRule = LODGEN_LANDGUIDE_OFF` | `LODGEN_LANDGUIDE_FLATWARP` | `src/lodgen.cpp:~6147` |
| static, guide strength | `1.0f` | unchanged (already 1.0) | `src/lodgen.cpp:~6148` |
| static, warp lattice / octaves / guide scale / guide slope | 1024 / 1 / 1024 / 0.5 | **unchanged** | — |
| panel rows | Hex tile size 0, Warp amplitude 0, Mip bias 0, Guide rule Off | 256 / 341 / -0.22 / Flat warp | `src/lodgenmanager.cpp:~1375-1400` |
| panel Sample rule row | Footprint (0) | **unchanged** — footprint is still the base rule, and the panel's stochastic entries force warp 0, which is not bungo's pick | `src/lodgenmanager.cpp:~1352` |
| panel QSettings | saved rows held 0/0/0/0 | one-shot migration, below | `src/lodgenmanager.cpp:427-462` |
| help text | "DEFAULT 0 = off = the shipped bake"; "off (the default…)" | states the new defaults and the four-switch way back | `src/nifcli.cpp:5951-5985` |

**The way back, exact and byte-identical:**
`--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off`.

### (d) The verge is not road — `--road-ground-paint` 1.0 → 0

| what | old | new | file:line |
|---|---|---|---|
| option struct | `LodgenCoverOptions::roadGroundPaint = 1.0f` | `0.0f` | `src/lodgen.h:991` |
| CLI switch | `--road-ground-paint 0..1` | unchanged | `src/nifcli.cpp:6912` |
| panel row | Verge painted as road 1.0 | 0.0 | `src/lodgenmanager.cpp:~1520` |
| panel QSettings | saved row held 1 | one-shot migration, below | `src/lodgenmanager.cpp:427-462` |
| help text | "0 leaves the landscape colour there… 1 is ROADS1s bake" | "0 — THE DEFAULT since 2026-09-12… `--road-ground-paint 1` is the way back" | `src/nifcli.cpp:6136-6145` |

**The way back:** `--road-ground-paint 1`.

### The saved-panel problem, and what was done about it

The two identity boxes are not saved to QSettings, so they follow the code. The five
land/road rows ARE saved, and **bungo's saved panel already held the old numbers** —
read out of his registry before any change was made:

```
HKCU\Software\NifTools\NifSkope 2.0\LodGeneration
  landHex 0   landWarp 0   landMipBias 0   landGuide 0   roadGroundPaint 1
  landSample 0   roadDetail 1
```

Without a migration his panel would have gone on baking the old look while the command
line baked the new one, and no byte gate would have caught it (the gates force their own
QSettings state — PANEL1's lesson). So a **one-shot migration** runs when the panel is
built (`src/lodgenmanager.cpp:427-462`): for each of the five moved keys, if the saved
value is still EXACTLY the old default it becomes the new one; a number he set himself
is not the old default and is left alone; absent keys are skipped and read the row's own
new default; a marker key `LodGeneration/defaults1Applied` makes it happen once.

### What did NOT move

`--road-detail` (stays 1), `--road-opacity` (stays 1.0), erosion (stays 0),
`--land-sample` (footprint is still the base rule), `--land-tiling`, `--land-detail`,
the warp lattice and octaves, `--land-guide-scale`, `--land-guide-slope`,
`--land-shade`, `--grade`, `--blend-edges`, the sheet format, the AO switches.
No writer format changed.

---

---

## 1. Gates

The new gate is `tests/spells/lodgen_defaults.sh`. One full `PHASES=abcde` run
of it is saved verbatim at `scratchpad/defaults1_20260912/gate_full.txt`; the
bakes it made are under `gate_full/`.

Its shape is the same in every phase: **a default is only a default if the same
bytes come out of the OLD exe when you spell the switch.** So every phase runs
both exes — `release/NifSkope.exe` (2026-09-12 20:42:12) and the rung
`release/NifSkope.before_defaults1.exe` (19:45:31) — and compares.

Two files are excused from the byte comparison in phase (a), and both exclusions
are themselves checked rather than waved through:

* `<chunk>.bto.manifest.txt` — the sidecar the rung did not write with identity
  off and the new exe does. The gate NAMES every differing file and fails on any
  that is not a manifest or the ledger.
* `Commonwealth.lodb` — the input ledger records the SWITCH LIST of the run, so
  a defaulted run can never byte-equal a spelled-out run. The gate opens it
  instead: same files, same sha1s, input hashes EQUAL, switch hashes NOT equal.

### The numbers, phase by phase

| phase | what it asserts | number | the floor or refuter beside it |
|---|---|---|---|
| (a) | the new exe's DEFAULT bake of chunk (-20,24) dim 4 == the rung's bake with `--no-identity --no-terrain-identity --road-ground-paint 0` and the four land switches | 7 files vs 6, **2 differing: the manifest sidecar and the ledger, nothing else** | the gate lists every differing name and fails on any that is not excused |
| (a) | the ledger agrees under the excuse | 5 files each side, 0 one-sided, 0 different hashes; input hashes **equal**, switch hashes **not equal** | switch hashes EQUAL would be the failure — it would mean the two runs asked for the same thing |
| (a) | the sidecar is written with identity OFF | **678 object rows** | floor 50 rows |
| (a) | refuter | the rung asked for `--no-identity` wrote **NO** manifest | that is exactly what item 2 changed |
| (a2) | the same on a ROAD-BEARING region (-20,20)..(-17,23), where the verge default is visible | 2 differing files, both excused; ledger 5/5, 0 different hashes | — |
| (a2) | refuter | the rung at `--road-ground-paint 1` bakes **DIFFERENT bytes** | without it, phase (a2) could pass with the paint switch doing nothing |
| (b) | the way back: the new exe with the OLD switches spelled == the rung's DEFAULT bake | **7 files each side, 2,080,030 bytes each side, 1 differing file — the ledger, which records the switch list** | nothing else is excused here; the sidecar is not written on this path |
| (b) | the ledger on the way back | 5 files, 0 one-sided, 0 different hashes | — |
| (c) | every `.BTR` of a default bake carries vanilla's land descriptor `52776558133763` | dim 4, 8, 16, 32 — **all four** | — |
| (c) | every `.BTO` carries the plain object descriptor `474989027590661` (no colour, no UV 2) | dim 4, 8, 32 | dim 16 writes no `.BTO` **on either exe** over this one-cell region — proved against the rung, not excused |
| (c) | refuter | the rung's default dim 4 reads BTR `686095322853893`, BTO `5612388490686984` — the WIDE identity descriptors | the check is capable of failing |
| (d) | the far rings are not empty with identity OFF: the same chunk at dim 16 with `--arrays --impostors --slot-fallback`, new exe (identity off) against rung (identity on) | manifest **7,626 object rows**, **7,210 C** (card) lines, **35 I** (instance group), **12 M** (material), **21 A** (array) lines — **identical counts on both exes**; **11** texture-array files each side | floor: every count must be non-zero, so an empty bake cannot pass. Bake times 454 s (new) and 418 s (rung) |
| (e) | the native data does not thin out: the same region with every other switch spelled equal | **2 native files, 2 identical, 0 differ, 0 one-sided**; `--native-verify` clean on the identity-off pair | floor: at least one native file, or "nothing was produced" would read as "nothing moved" |
| (e) | refuter | the RUNG asked for `--no-identity` writes **1 of the native files DIFFERENT** | that is exactly the thinning item 2 removed — without this the phase could pass on code that never had the bug |

**Total: 28 checks, 0 failures, RESULT PASS**, on the 20:42:12 exe against the
19:45:31 rung. The run is `gate_full.txt`; its own header prints both exe
timestamps and sizes so the numbers cannot be read against the wrong pair.

### Two things the gate deliberately does NOT do

* It does not assert the new bake is *better*. It asserts the new bake is the
  OLD bake with the switches spelled — bungo confirms the look by eye.
* It does not compare `Commonwealth.lodb` byte for byte anywhere. The ledger
  records the switch list, so byte equality there would mean the test was
  comparing two identical invocations and proving nothing.

### The GUI half of the gate

`tests/spells/lod_generation.sh` runs the panel's own self-test (121 checks,
floor 121) and every row round-trips through its own settings key. Both exes
were photographed with `WW_LODGEN_SHOT_FULL`, one instance at a time, each on
its own unused port, on the second monitor:
`pics/panel_rung_full.png` and `pics/panel_new_full.png`.

One of its 121 checks had borrowed the identity box's TICKED state from the
Target rows above it, so the defaults move left it measuring an unticked box
(121 checks, 1 failure on the new exe; 121 / 0 on the rung). That is the same
re-base class as the seven spell files, and it was fixed the same way — by
making the harness FORCE the state it measures rather than by lowering the bar:
the check now ticks the box itself, grabs, and puts it back
(`src/nifskope_ui.cpp`, the `identWasTicked` block).

---

## 2. Harness re-base

A default move breaks every harness that silently relied on the old default.
The brief's rule is that the harness gets the switch SPELLED, never a loosened
bar: each of these checks was registered against a particular bake, and that
bake still has to happen — it just has to be asked for now.

Three columns, because the middle one is the evidence that the defaults really
moved: the rung, then the NEW exe with the harnesses untouched, then the new exe
with the switches spelled.

| harness | before (rung) | after build, not re-based | after re-base | what was spelled |
|---|---|---|---|---|
| `lodgen_terrain` | 26 checks, 0 failures PASS | 26 checks, **3 failures** FAIL | 26 checks, 0 failures PASS | `--terrain-identity` on the `GEN` bake, `--identity` on the `OBJ` bake |
| `lodgen_identity` | PASS | PASS | PASS | nothing — it spells `--identity` itself already |
| `lodgen_farring` | PASS | **FAIL** | PASS | `--identity` (the ring cut groups by identity index + UV2.y layer) |
| `lodgen_tree_sway` | 4 checks, 0 failures PASS | 4 checks, **3 failures** FAIL | 4 checks, 0 failures PASS | `--identity` (sway rides in vertex ALPHA) |
| `lodgen_ground_cover` | 29 checks, **6 failures** FAIL | 29 checks, **5 failures** FAIL | not re-based | nothing — red on the RUNG, see §5 |
| `lodgen_roads` | 11 checks, 0 failures PASS | 11 checks, **1 failure** FAIL | 11 checks, 0 failures PASS | `--road-ground-paint 1` inside its `bake()` |
| `lodgen_card_arrays` | PASS | PASS | PASS | nothing |
| `lodgen_texture_arrays` | PASS | **FAIL** | PASS | `--identity` on BOTH bakes |
| `lodgen_terrain_vt` | 41 checks, **2 failures** FAIL | 41 checks, **3 failures** FAIL | 41 checks, **1 failure** FAIL | the four OLD land switches in its `vt()` helper — the remaining failure is the rung's own, see §5 |
| `lodgen_native` | 18 checks, 0 failures PASS | 18 checks, 0 failures PASS | 18 checks, 0 failures PASS | nothing |
| `lodgen_native_baseline` | not run before | — | **25 files, 25 baked, 0 differ, PASS** | `--identity` in `bake()` and in the region bake |

`lodgen_native_baseline --check` is the strongest single number in this lane:
it compares against a baseline FROZEN on 2026-09-10, and with the switch spelled
**0 of 25 files differ**. The defaults move changed no output bytes; it changed
what you get when you ask for nothing.

Byte counts of every re-based spell file are in `CHANGED_FILES.txt`; all seven
are LF-only before and after (CR 0).

---

## 3. Build and chain

Game check, as its own command, read before the build command was typed:

```
tasklist | grep -i "Fallout4.exe"; echo "fo4_exit=$?"
  -> fo4_exit=1                       (nothing)
Get-CimInstance Win32_Process -Filter "Name='NifSkope.exe'"
  -> no rows                          (no window of bungo's, no lane's harness)
```

Nothing was renamed aside, because nothing held the exe.

| step | gate | number |
|---|---|---|
| `make -j2`, its own exit code | `BUILD-RC=0` | 0 |
| exe | `release/NifSkope.exe` | **2026-09-12 20:42:12, 22,279,680 B** (rung 19:45:31, 22,275,072 B) |
| first two bytes | must be `MZ` | `MZ` |
| translation units compiled | read from the build log | **7**: `lodgen.o`, `lodgenchunkpass.o`, `lodgenmanager.o`, `main.o`, `nativeemit.o`, `nifcli.o`, `nifskope_ui.o` |
| relinks | counted in the log | **1** |
| exe newer than EVERY changed file | `git status --porcelain -- src res tools tests`, 42 modified paths | **0 files newer than the exe** |
| stale objects for the touched header `src/lodgen.h` | every `.cpp` that includes it, and every `.cpp` that includes `lodgenchunkpass.h` | 6 + 3, all `ok`, none `STALE` |
| `make -n` | zero compile lines | **0** compile lines, "Nothing to be done for 'first'" |
| stylesheet | `cmp res/style.qss release/style.qss` | in step (relinked at 20:42:12) |

The seven translation units are exactly `src/lodgen.h`'s dependency fan plus the
two files I edited that do not include it through another header. No other
lane's source was pulled in, so this exe IS the rung plus this lane's diff.

### The second build, 21:52:25 — the GUI harness re-base

One of the panel self-test's 121 checks had borrowed the identity box's ticked
state instead of forcing it (section 1, the GUI half). Fixing that is a change
to `src/nifskope_ui.cpp` inside the `WW_LODGEN_TEST` block only, so it needed a
second build.

| step | gate | number |
|---|---|---|
| game check, its own command, read before the build | `tasklist` | nothing running |
| `make -j2`, its own exit code | `BUILD-RC=0` | 0 |
| exe | `release/NifSkope.exe` | **2026-09-12 21:52:25, 22,280,192 B** |
| first two bytes | must be `MZ` | `MZ` |
| translation units compiled | read from the build log | **1**: `nifskope_ui.o` |
| relinks | counted in the log | **1** |
| exe newer than the changed source | `test -nt src/nifskope_ui.cpp` | yes |
| stylesheet | `cmp res/style.qss release/style.qss` | in step |
| the bake gates on the NEW exe | `PHASES=abce` re-run, `gate_recheck.txt` | **22 checks, 0 failures PASS** |
| the panel self-test on the new exe | `lod_generation.sh` | **121 checks, 0 failures PASS** (rung: 121 / 0) |

Phase (d) was not re-run on the 21:52:25 exe: it is 15 minutes of baking and the
diff between the two exes is one block of harness-only code that no bake path
reaches. Its numbers in section 1 are the 20:42:12 exe's, and this paragraph is
here so that is not read as more than it is.

---

---

## 4. Pictures

`scratchpad/defaults1_20260912/images/`. Every pair carries its difference
number, and every crop camera was CHOSEN by the difference, not by eye.

Two routes, because the four rulings do not live in the same place. The land
look and the verge paint are texels in the chunk's terrain diffuse `.DDS`, so
those pictures decode the sheet with PIL. The identity cast lives in VERTEX
data, so those are NifSkope's own top-down renders.

| file | size | what it shows | the number |
|---|---|---|---|
| `i_btr_identity.png` | 1442x641, 995,186 B | chunk (-20,24) dim 4, the `.BTR` top-down: the rung's blue-purple terrain-identity cast beside the new default's neutral, correctly textured land | 1,434,676 px differ |
| `i_bto_identity.png` | 1442x641, 484,380 B | the same chunk's `.BTO`: object identity on vs off | 287,824 px differ |
| `i_land_look_sheet.png` | 1592x608, 818,625 B | the ruled land look in the diffuse sheet — rung, new, difference x8 | — |
| `i_land_look_crop.png` | 810x480, 36,328 B | the same 96x96 texels at 4x where the two sheets differ most | — |
| `ii_verge_sheet.png` | 1592x608, 401,269 B | `--road-ground-paint` 1 vs 0 in the sheet, chunk (-20,20) | 8,837 of 174,888 bytes |
| `ii_verge_crop.png` | 810x480, 31,435 B | the kerb at 4x | — |
| `ii_verge_chunk.png` | 1442x641, 588,929 B | the same chunk rendered, paint 1 vs paint 0 | 108,726 px differ, bbox (0,336,1400,1050) |
| `ii_verge_kerb.png` | 2842x1187, 1,458,397 B | **the headline for ruling (d)**: the Sanctuary cul-de-sac at the camera the difference chose (6085,8490, half-width 1521) — the island and verge painted road-grey at paint 1, reading as ground at paint 0 | 861,849 px differ |
| `iii_blend_edges_sheet.png` | 1592x608, 409,009 B | `--blend-edges off` vs `quadrant --blend-margin 128`, chunk (-20,24) dim 4, FOOTPRINT sampling, everything else at the new default | 19,267 of 262,144 texels (7.35%) |
| `iii_blend_edges_crop.png` | 810x480, 26,148 B | the same 96x96 texels at 4x at (96,160): the hard quadrant seams in `off`, dissolved in `quadrant` | — |
| `iv_panel_rows.png` | 1008x3029, 171,211 B | the LOD Generation settings column, `WW_LODGEN_SHOT_FULL`, rung beside new — the same grab on both exes | 2,743 px differ, bbox (30,518,218,593) |

Picture (iii) was owed conditionally. **The switch does isolate it**:
`--blend-edges off|quadrant` is parsed at `src/nifcli.cpp:6831` and `off` is the
shipped default (`src/lodgen.cpp:5912`), so the pair changes one thing. It is
NOT bundled with `average` here — the base sample rule stayed FOOTPRINT, which
is the default and is not moved by this lane. Only the diffuse sheet changed;
`_data` and `_msn` are byte-identical between the two bakes, which is itself the
refuter that the switch touches the composite and nothing else.

`iv_panel_rows.png` is the picture that says the least and is the most
honest about it. The two columns differ in ONE place — the identity rows,
where the rung reads "Identity channels and manifests" ticked and the new exe
reads "Identity channels" unticked. The five land/road rows read the SAME
numbers on both sides, because those rows come from saved settings and the
one-shot migration has already moved them (see section 5). A picture that
showed the land rows moving would have had to be taken against a wiped
registry, which is bungo's, so it was not taken.

The two renders that came back wrong first (a flat-colour `.BTR`, and a
pixel-identical paint pair) are ledgered as mistake 3; the camera rule and the
resource-root rule are written into `pics_render.sh` as burned-in comments so
the next lane does not repeat them.

---

## 5. Owed / red / bungo's calls

### bungo's calls, named as his

1. **The manifest stays a SIDECAR.** Brief item 2 decoupled
   `<chunk>.bto.manifest.txt` from the identity flag, so a default bake now
   writes it where the rung did not. It is still a file beside the `.BTO`, not
   data inside it. Whether the far-ring bookkeeping ever moves into the chunk
   file is an OPEN call and it is bungo's; this lane only stopped it
   disappearing when identity is off.
2. **`lodgen_terrain_vt` now spells the four OLD land switches in its `vt()`
   helper, and that is a request for a ruling, not a fix.** The measurement:
   V9a-1 asks that the sheet assembled through the virtual-texture pyramid be
   byte-identical to the direct bake. With the RULED land default it is not, on
   2 of 4 chunks of the probe region (`-24 24 -17 31 --dim 4`):

   | configuration | chunks differing of 4 |
   |---|---|
   | the new default | 2 (`Commonwealth.4.-24.28`, `Commonwealth.4.-20.28`) |
   | `--land-hex 0` only | 2 — hex is innocent |
   | `--land-mip-bias 0` only | 2 — mip bias is innocent |
   | `--land-warp 0` only | **0** |
   | `--land-guide off` only | **0** |
   | all four old switches | 0 |

   Magnitude: 4 bytes of 174,888 on one chunk, 27 on the other, 0 on a third.
   So the warp (and the guide that steers it) samples a texel across the
   pyramid's tile boundary differently from the direct path. It is small and it
   is real. All 41 of that harness's checks were pre-registered against the OLD
   look, and a lane does not loosen a pre-registered bar, so the harness asks
   for the old look explicitly and the SHIPPED default is gated instead by
   `lodgen_defaults.sh` phase (a), byte for byte against the rung.
   **What bungo decides**: whether the pyramid path should be made to agree with
   the direct path under the new default (a real fix in the tile assembly), or
   whether a few bytes of divergence at tile boundaries is acceptable and the
   check should be re-registered on the new look. This lane did neither.
3. **His saved panel has ALREADY been migrated, on this machine, today.**
   The one-shot migration runs when the panel is built, and the panel was
   built by this lane's own GUI harness runs. Read back out of the registry
   after them:

   ```
   HKCU\Software\NifTools\NifSkope 2.0\LodGeneration
     landHex 256   landWarp 341   landMipBias -0.22   landGuide 5
     roadGroundPaint 0   landSample 0   roadDetail 1   defaults1Applied true
   ```

   That is the intended behaviour and it is what keeps his panel and the
   command line baking the same thing — but it is a change on his machine,
   made by a lane, so it is said out loud rather than left to be discovered.
   If he wants the old numbers back in the panel he types them in, or deletes
   `defaults1Applied` and the five keys.
4. **The `A <block> -1 <lodm>` hole in the manifest is latent, not live.** The
   impostor spec's `-1` layer means "no texture-array layer", and the layer
   comes from UV2 — which a default bake no longer writes. Measured: 21 `A`
   lines with identity off, the same 21 with it on, every one a POSITIVE layer,
   so nothing is broken today. The durable fix (putting the layer into the merge
   key) is bungo's call; the condition is now written into
   `docs/LODGEN_IMPOSTOR_SPEC.md` beside the `-1` paragraph.

### Red, and not this lane's

* **`lodgen_ground_cover`: 29 checks, 6 failures on the RUNG.** Check C1 is "a
  frozen baseline exists to compare against" and it fails, so five more cascade
  off it. Same failures, same numbers, on the exe before this lane. After the
  defaults move it reads 29 checks, **5** failures — one fewer, not one more.
  Not re-based, because there is nothing to re-base until somebody re-freezes
  that baseline. Owed to whichever lane owns ground cover.
* **`lodgen_terrain_vt` V9c seam continuity: red on the RUNG with identical
  numbers** (interior control 13.243 / 12.182, E/W seam 14.20x, edge step
  14.348). It is the one failure left after the re-base.
  The rung's OTHER failure was that harness's own freshness check, "the exe is
  newer than every source this answer depends on" -- which the rung fails by
  definition, being the older exe. So the honest reading of that harness is
  2 -> 1 where the one that went is an artefact of measuring on the rung, and
  the one that stays is the seam, unchanged.

### Owed

* Nothing in the brief is unstarted. Picture (iii) was conditional and the
  switch existed, so it was taken.
* **Nothing is committed** and nothing has been deployed anywhere near a game
  folder. `release/NifSkope.before_defaults1.exe` is the rung and must not be
  deleted.
* bungo confirms the LOOK by eye. This report claims byte equivalence and
  difference numbers; it does not claim the new default looks right.

---

## 6. Mistakes

Four, all in `MISTAKES_ENTRIES.md` and already spliced into the TOP of root
`MISTAKES.md` (LF only, CR count asserted 0) the moment each was recognised.

1. **A bash heredoc halved the `\n` in a patch anchor — and it happened four
   times in this lane.** Twice in patches against `src/lodgenmanager.cpp`, then
   again in a throwaway probe where `if '\' in s:` arrived as `if '` and died
   with `SyntaxError: EOL while scanning string literal`. NATIVEVIEW1 ledgered
   the same trap hours earlier and the `ww-anchored-hookup` skill says it before
   that. The rule is not about PATCHES, it is about any Python carrying a
   backslash: it goes in a FILE, written with the Write tool, and the script
   builds the character itself (`chr(92)`). Nothing was corrupted — the patch
   scripts refuse before the first replace unless every anchor matches exactly
   once — so the cost was time.
2. **I nearly shipped a latent bug by keeping `idColor` inside the identity
   gate.** `lodgenNativeLighting()` reads `bucket.col` to key the object index
   for the native `.lodo` / `.lodi` writers, so leaving it gated would have
   keyed every vertex to object 65535 with identity off, silently, while every
   legacy byte gate stayed green. Caught by asking WHO ELSE READS IT for each
   block being ungated, not by a test — no test existed that would have failed.
   Phase (e) of the new gate exists so that class of error cannot be silent
   again.
3. **Two renders measured something other than what I said they measured.**
   The `.BTR` frames came back one flat colour because the camera was pinned at
   the `.BTO`'s WORLD centre (a `.BTR`'s Land shape is in the FILE's own space:
   centre 8192,8192,0, ortho half-width 8192). Then the paint-1 and paint-0
   frames came back PIXEL-IDENTICAL while the sheets differ by 8,837 of 174,888
   bytes, because `WW_LODGEN_RESOURCES="$root;$DATA"` lets the game's own
   `Commonwealth.4.-20.20.DDS` win the lookup — every panel was the SHIPPED
   sheet. Found by a swap test: one bake's `.BTR` rendered against the OTHER
   bake's root gave a byte-identical frame, which is impossible if the root is
   read. The shim root now goes in ALONE, and every pair gets a difference
   number before it is described.
4. **`git show HEAD:` was used as the "before" column of `CHANGED_FILES.txt`**
   in a tree that is many lanes ahead of HEAD, so the table claimed
   `src/lodgen.cpp` went 364,264 → 594,075 bytes — everyone's uncommitted work,
   not this lane's diff. Rewritten: "before" is a number this lane measured, or
   the words `not snapshotted`; HEAD appears only in a separately labelled block
   that says it is a landmark.

---

## 7. Skill review

* `.claude/skills/nifskope-ww-lodgen/SKILL.md` (36,753 → 38,639 B, LF 517,
  CR 0) gained a block **"THE DEFAULTS MOVED ON 2026-09-12 (bungo's rulings,
  lane DEFAULTS1)"** before `## The gates`: the four rulings, the exact way
  back as one command line, the decoupling of item 2, `--road-detail` always 1,
  and the new gate. Its `A <block> -1 <lodm>` sentence gained the identity-off
  condition. A lane that reads the skill and not this report now still bakes
  what bungo ruled.
* `nifskope-ww-build-verify` needed no change. Its chain held exactly as
  written: the game check as its own command, `make`'s own exit code as the
  gate, the translation-unit count read out of the log (7, all of them
  `src/lodgen.h`'s dependency fan plus the two files edited directly — which is
  how this exe can be called the rung plus this lane's diff), the stale-object
  sweep for the touched header, `make -n` clean afterwards.
* What is MISSING from the skills, and would have saved this lane an hour:
  **how to render a chunk file and what space it is in.** `.BTR` = the file's
  own space, centre 8192,8192,0 at half-width 8192; `.BTO` = world units,
  chunk (x,y) at dim 4 is `x*4096-57344, y*4096-91136` (chunk (-20,24) →
  -73728,106496); `WW_RENDER_ORTHO` is a HALF-width; and the resource-root rule
  of mistake 3. It is written into `pics_render.sh` as burned-in comments for
  now, which is the wrong home for it. Recommend a
  `nifskope-ww-lod-pictures` skill that carries those numbers, the shim-root
  rule and the "an identical pair is a broken measurement" line. I did not
  write it in this lane because the brief forbids work beyond the four rulings.
