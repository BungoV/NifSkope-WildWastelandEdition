## HANDOFF text

**Lane CARDFIX1 (LOD-D), 2026-09-24/25: PARTIAL. Steps 1-6 landed (G4 decided by the director, (a)); step 7 in progress.**
Branch `cardfix1-20260924` in `E:\Projects\NifskopeWWE-cardfix1` (from 71f96c1). Commits: step 1 19c0347,
step 2 91ddd41, step 3 d8302c9, step 4 7896ad1, step 5 1303334 + 6c5f5f8, step 6 24e7835, step 6b 6430dff
(the N8 default), step 6c = the G4 re-pin commit. Exe 309f3aa9a09897da12c94db644ff70f57f33dc7b. Report: `scratchpad/cardfix1_20260924/DONE.md`.

* **Step 5 (IMPOSTORRING1), a horizon ring card set** (`WW_IMPOSTOR_RING=16`, 16 views x 1 row): baked, carried
  in the `.lodm` as `views`/`grid`, drawn by nearest azimuth; an old exe refuses it by name. **Finding for bungo:
  the N8 grid beats the 16-view ring at EVERY elevation measured, the horizon included** (mean IoU at the
  in-between azimuths, el 0: ring 0.4886 vs N8 0.8249). N8 already has 28 frames near the horizon. bungo RULED
  2026-09-25: "Yes, 8x8 is the default choice for a bake". Step 6b made the bake driver default every run,
  trees included, to the N8 grid (RING=0); the 16-view ring stays an option (RING=16), gated by
  impostor_ring.sh (17 / 0, new row R7 reads the driver's own default; red = the step-5 driver).
* **Step 6 (IMPOSTORWIND1 job 3), sway A**: a tree card's normal alpha is now the model's OWN vertex-alpha wind
  weight x linear height (0 on the trunk); a model with no tree-animation shape keeps the synthetic law byte
  for byte. The `.lodm` says `lodm` 2 / `sway` "model" on the card family only; an old reader refuses it by name.
  G1-G3 green. **G4 RED against its pre-registered bar**: BC7 error on the new sway channel (elm) is mean 3.573,
  p95 13 against a bar of 3.0 / 12. The bar was copied from the smooth synthetic sway (1.34 / 4) without
  measuring a real weight. The same sheet's normal R/G channels read 3.266 / 12, the same order.
  **Director decision (a), 2026-09-25 (not bungo's ruling):** G4's bar is now the same sheet's measured codec
  floor x 1.25 (4.082 / 15); the BC7 weights stay `{1,1,32,1}`, because (b) would move every card's normals
  for a sway error of about 1.4 %. Red controls: the next frame (51.1) and the sway cut to 4 bits (6.702)
  both fail. Wind gate 28 / 0 (gates/impostor_wind.run4.out).
* **Step 7 (IMPOSTORPBRM1)** in progress after step 6c; it is a lane's
  worth of work (v6 `.pbrm` fixture, family pbr design, `_s` vs folded F0, TintMask law, per-reference tint).
* A step-5 defect found and fixed in step 6: a ring card ARRAY's file name contained `|` and could not be
  written on Windows. No gate had put a ring set through `--arrays`; the wind gate now does.
* **Owed:** lodgenaggregate learning the ring and `lodm` 2 (refused by name today; not this lane's file); the
  panel's Card frames row and cardsOnDisk ignore ring sets; the FO4CS reader for both ring sets and `lodm` 2.
  Not flown; nothing deployed.

## WW_CHANGES text

**2026-09-24/25 -- lane CARDFIX1 (steps 5-6; branch cardfix1-20260924)**
- **Horizon ring card sets.** The impostor bake takes `WW_IMPOSTOR_RING=V`: V views evenly around the horizon
  at elevation 0 in one V x 1 sheet. lodgen carries it as `views` / `grid` [V,1] (no `oct` key); the preview
  and the in-game drawer pick the nearest azimuth frame. It is an option (`RING=16` on the bake driver); the
  default stays the 8x8 grid (bungo 2026-09-25: "Yes, 8x8 is the default choice for a bake"). An exe from
  before this refuses a ring set by name. (docs/LODGEN_LODM_FORMAT.md 3.2)
- **A tree card sways with the tree's own wind weights (sway A).** A card baked from a model with a
  tree-animation shape writes the model's own vertex-alpha weight x height into the normal sheet's alpha; the
  trunk stays still. Other models keep the old synthetic sway exactly. Such cards and card arrays are `lodm` 2
  and name `sway` "model" plus the base's leaf amplitude and frequency; older readers refuse them by name.
  The bake sidecar says `sway model|synthetic`. (docs/LODGEN_LODM_FORMAT.md 3.3)
- **Fixed:** a horizon ring card array could not be written (its file name contained `|`); it is now
  `<ws>.LodgenCards.legacy.<W>x<H>.ring_d.DDS`.
- The impostor preview harness takes `WW_IMPOSTOR_SWAY_AMP` / `WW_IMPOSTOR_SWAY_PHASE`, and
  `WW_IMPOSTOR_ORBIT_SELECT`.
- New gates: `tests/spells/impostor_ring.sh`, `tests/spells/impostor_wind.sh` (+ `impostor_wind.py`, and
  `impostor_wind_nif.py`, an independent rasteriser of the NIF's own wind weights).

## MISTAKES text

**2026-09-25 00:2x -- CARDFIX1 step 6: G4's bar was copied from the synthetic input onto the real one.**
The BC7 sway-error bar (mean <= 3.0, p95 <= 12) came from the synthetic law's error (1.34 / 4). The model's own
wind weight has sharp edges and compresses worse: measured 3.573 / 13, while the same sheet's untouched normal
R/G channels read 3.266 / 12. Correct code went red on a bar nobody had measured. Rule: a bar for a new input
is measured on that input, and a lossy-codec bar prints the codec's error on an untouched channel of the same
sheet beside it. Left red for a ruling, not re-pinned.

**2026-09-25 00:0x -- CARDFIX1 step 6: G1a was pre-registered on a subject with no population for it.**
"mask-0 texels carry A = 0" was registered on the elm, which is one shape, all tree-animated: 0 mask-0 texels.
Its shape list had been probed and not read. Now a named n/a line; maple and pine carry the row.

**2026-09-25 00:1x -- CARDFIX1 step 5 shipped a ring card array that could not be written.**
The ring group key is `legacy|WxH|ring` and the array file name took the key's tail, `|` included, which
Windows refuses. Step 5's gates never put a ring set through `--arrays`. Found by step 6's wind gate (G3);
fixed by building the name from the group's fields. Rule: a new card-set KIND is gated through every consumer
of card sets (preview, chunk card, card arrays, aggregate), not only the one the step targets.

**2026-09-24 23:1x -- CARDFIX1 step 5: R4's absolute bar (IoU >= 0.60) was above the reference's own ceiling.**
The mesh against itself rotated by half a ring step reaches only 0.5015, so no impostor could pass. Re-pinned
to 0.90 x the measured ceiling. The red filter also dropped colour `EXCLUDED` lines, not only mesh ones; fixed.

## Skill review

- **Written:** `E:\Projects\Claude\.claude\skills\ww-preregister-bar-from-the-subject\SKILL.md`. It covers
  measuring the reference's own ceiling, the subject's population and the real input before writing a bar;
  printing the codec's floor on an untouched channel beside a lossy-codec bar; and what to do when a
  pre-registered bar turns red on correct code. It combines the lane's four MISTAKES entries.
- **Candidates noted, not written** (each one paragraph; a later lane may fold them into existing skills):
  `git apply --cached --unidiff-zero` to stage only one lane's hunks of a shared file (belongs in
  `ww-test-harness-add` or a commit skill); `impostor_defaults.sh` D7 runs only with `RUNG=` (6/0 without, 7/0
  with; belongs in `nifskope-ww-lodgen`); `tests/spells/impostor_wind_nif.py` as an independent reprojection
  of a NIF's per-vertex data for card gates (a HORIZON2-style refuter).
- More candidates: the pbr_shade_ab OLD arm needs the whole runtime (DLLs, qt.conf) -- an arm that cannot
  start reads as NO PICTURE on every case; previewing a loose card set needs a `textures\` tree beside the
  .lodm; staging one step's hunks of a file that already holds the next step's (stage6_docs.py).
- **Loaded skills, gaps found:** `nifskope-ww-worktree-build` section 6 already names the bake driver's fixed
  port 45917; `nifskope-ww-lodgen` does not yet mention `lodm` 2 or ring sets (director splice).
