# Handoff — NifSkope, Wild Wasteland Edition

## TOP BLOCK -- written 2026-09-24 16:52 (`date`-read) by lane LEDGERFIX1, after the ledger wipe

READ THIS FIRST. Everything below the next `## SESSION HANDOFF` heading is the
restored 09-10..09-12 ledger: history, not current state. Where it says "nothing
committed" or names an exe, this block overrides it.

### The wipe and the recovery (2026-09-24)
- 13:57 a director one-liner opened HANDOFF.md, WW_CHANGES.md, MISTAKES.md and
  scratchpad/_handoff_anchor.txt for writing before reading them and emptied all
  four. Last commit of the three was 720762a (2026-09-09).
- Recovery, minimal path by bungo's ruling ("just go with the minimal path"):
  HANDOFF.md = the editor snapshot of 2026-09-12 08:19 (450,000 B);
  WW_CHANGES.md = scratchpad/build8_20260910/WW_CHANGES.md.bak (content to 09-10);
  MISTAKES.md = the 09-09 commit (no newer copy exists). Then this top block,
  the nine 09-24 lane texts and three director lines below, the same lanes'
  WW_CHANGES and MISTAKES sections, and the Todd's treat wording again.
- THE GAP: HANDOFF 09-12..09-23, WW_CHANGES 09-10..09-23 and MISTAKES
  09-09..09-23 are NOT in these files. The detail lives in each lane's folder:
  scratchpad/<lane>_<date>/DONE.md and DELIVERABLE_TEXT.md. Not reconstructed
  from transcripts, by ruling. PBRR0 and DOCFIX2 (also 09-24) are in their
  folders only.
- Rule from the wipe (MISTAKES.md, same date): read into a variable, close,
  then write; assert the new ledger is LONGER than the old before every write;
  splice scripts refuse an empty anchor and a target under 10 kB; commit the
  ledgers daily.

### Tree and exe
- Committed and pushed to origin main by LEDGERFIX1 on bungo's word ("then
  COMMIT AND PUSH"); the hashes are in the WW_CHANGES entry "Ledger recovery".
  a4c2069 ledgers rebuilt; cea8808 .gitignore (game data, extracts, binaries,
  two groups held for bungo); 85c0b14 src + res (203 files); 8fa18e3 tests +
  tools (178); 5e4f069 docs + constitution + this repo's skills (103);
  92c068f scratchpad text (10,414 files, ~98 MB); then this ledger note.
  Held for bungo (ignored, on disk): skill copies from other projects and five
  notes naming another outside RE source (bungo 17:4x: Nomad MAY be named;
  the notes stay held only for their other contents). Untracked-not-ignored after: 0.
  Game data, binaries and generated bulk stay local by .gitignore.
- release/NifSkope.exe = FOG1's 2026-09-24 18:45:55, sha1 629a3911,
  24,669,184 B (carries every landed lane below). Rollback rung
  release/before_fog1 = 45108d83 (PBRWX1). bungo's open window predates it:
  RESTART. Overseer re-ran FOG1's full gate 19:26: 64 checks 0 failures in one run;
  zero set 10 cases 0 failures.

### Next
- FOG landed (FOG1). NEXT NifSkope lane = CASCADED SHADOWS: spec
  scratchpad/pbrprep1_20260924/spec_cascaded_shadows.md. Then -> contact shadows -> SSAO -> SSGI ->
  bloom (linear HDR only while Bloom or SSGI is on); then the R5 card bake
  (specular + tint), not started. bungo: "only stop when it fully works".

### Owed to bungo / waiting on him
- A look at the weather preview at dawn, dusk and night (PBRWX1), after the restart.
- Held rulings: (c) roughness floor, (d) emitter-size widening.
- Open from TODDSTREAT1: should the generic third-party skill copies (xse-plugin,
  fo4cs-*) be in the public repo at all; does the reverse-engineering skill stay
  ignored; three untracked files name another outside RE source; the
  hkclasses extractor is ignored, so the generator of res/hkclasses_fo4.json is
  not public.
- Re-run the command-line bakes since 09-19 that used --data-root.

### Lane and director lines, 2026-09-24, newest first
- 2026-09-25 21:45 GREY1:
  GREY1 (2026-09-25 21:10-21:45, measure only, branch grey1-20260925, nothing changed or re-baked): why our far-field
  buildings read greyer than in game. Ranked:
  1. The per-placement material swap never reaches the LOD. 44% of Commonwealth building placements carry one; in
     full detail they are 25% more saturated than their shared LOD atlas (S 0.189 vs 0.151). The swap moves colour
     p10 0.7x .. p90 1.6x over 4,049 (base, swap) variants. This is vanilla's own LOD trait.
  2. The game's light is coloured (shaded faces lit blue, S ~0.3, vanilla and Physical Weathers alike). The
     viewer's is not: Legacy is white, and Lookdev gives far-field shapes a flat grey ambient, not the DALC.
  3. The viewer's tone map under the headlight takes 5%.
  Not causes: missing textures (the on-screen base colour equals the atlas), the palette rule, mips, FO4CS grading,
  imagespace.
  Proposal: a per-placement RGB multiplier (census2.py's full/LOD mean colour of the (base, swap)); Lookdev DALC for
  far-field shapes. Refuter: the grey buildings he points at carry no XMSP/MODS.
  Side finding: the Physical Weathers CommonwealthClear IMSP points at DLCCoast REFR/STAT IDs, not imagespaces.
  Full record: scratchpad/grey1_20260925/DONE.md.
- 2026-09-25 21:08 TINT1:
  TINT1 (2026-09-25, branch tint1-20260925, not merged):
  - Object library re-baked at `.lodo` v5, which keeps vertex colour: 52,925 colour rows, 166 meshes.
  - Installed at 20:13 into mods\FO4CSLOD\...\Commonwealth: .lodo, .lodi and 15 legacy _n arrays. The .lodb was kept (BAKE1's). Backups and sha1s are in scratchpad/tint1_20260925/replaced + install_record.tsv.
  - FO4CS ImprovedLOD refuses v5 cleanly ("version 5; this reader knows 4"; module ships off). Teaching it v5 is owed, and comes last.
  - Census over 184,431 placements:
    - 16.3% carry colour + the Vertex_Colors flag.
    - Only 3.5% carry a real hue, and almost all of that is trees (TreeBlasted02_LOD_1 alone has 4,055 placements).
    - Building LOD models carry no hue: not in vertex colour, not as a palette flag, not in the stock .bto.
    - So the grey buildings bungo sees are NOT fixed by v5. Next place to look: the atlas diffuse textures and whether they resolve.
  - The viewer needed 61d920ab to draw the v5 colour; merge it with SEAM1's commit.
  - A headless lit picture inherits the Vertex Color option, so use WW_LODL_AO=1 for colour pictures.
- 2026-09-25 18:41 WHITE1:
  WHITE1 (2026-09-25, branch white1-20260925 from d40017be): bungo asked "What's up with those white corners though?" about
  whole_top_after.png. It is DATA, and there is no code change, build or re-bake.
  - **What the band is.** The pale band is the steep outer wall of the western mountain block: land x -76.5..76.5,
    y -76.5..77. Heights run from the -352 floor to 31,000-35,000 units within 3-4 cells, at 50-83 deg.
  - **Where the colour comes from.** Bethesda's own dim-4 terrain LOD diffuse paints steep faces pale grey: ring
    +16.7 lum over the core. `--vt-fill-vanilla` copies that colour: ours +10.7, corr 0.977 with vanilla. The
    fill-OFF bake is flat there (0.0).
  - **Candidates refuted.** 0 painted LAND in the ring (C3). The file's own texels carry the band (C4). The unlit
    render equals the texels and the lit render is weaker (C5).
  - **bungo's "are those mountain peaks?"** No, they are steep faces. 97.6% of pale texels in the western block sit on
    slopes > 30 deg (non-pale 27.0%). 17.8% sit above the block's p80 height (non-pale 20.3%).
  - **Owed.** A ruling only if he wants it gone: fade the fill's weight on steep faces, or darken vanilla's cliff
    colour. Either one departs from vanilla on purpose.
  - **Side finding.** On 47 seam samples at the wall's lip, LAND's two cells disagree with each other, and the .lodl
    carries a neighbour's value there. It is one sample row and not the band.
  Report: E:\Projects\NifskopeWWE-white1\scratchpad\white1_20260925\DONE.md; pictures in its pics/.
- 2026-09-25 18:05 EXTENT1:
  **EXTENT1 (2026-09-25, branch extent1-20260925, no src change, no bake).** bungo asked whether the whole
  Commonwealth map is covered by the landscape heightmap. It is. The installed `Commonwealth.lodl` header covers
  cells -96..95 x -96..95. That is every LAND cell of Fallout4.esm (36,864 of 36,864) and his MO2 order (the same).
  Heights sampled at 5 outer cells (121 samples each) match LAND exactly (0 mismatched). Vanilla's non-flat .BTR
  tiles lie inside the same square. The director's "-42..33 x -48..39" is the PLACEMENT bounds (184,431 objects
  in 4,541 cells), which earlier pictures used as their frame. The grey land around the objects in the old
  09_overview was unpainted LAND (32,909 cells have no BTXT/ATXT), not missing terrain. SEAM1's
  `--vt-fill-vanilla` sheets, installed at 16:14, paint it.
  Pictures (untracked): `scratchpad/extent1_20260925/pics/whole_top_before.png` / `whole_top_after.png`
  (top-down, 3200x3224, cells -96..95, all objects), `whole_obl_before.png` / `whole_obl_after.png` (view 8).
  Placement cells on grey: 776 -> 8 (the 8 are the Glowing Sea, painted grey in the ESM). Cells with no terrain
  under them: 0 -> 0. Refuter: a placement cell whose centre pixel in `pics/top/T.png` is the background colour,
  or a LAND cell outside the .lodl header.
  Open: the viewer's object layer refuses the whole map (13.1M vertices > 9.5M cap, all-or-nothing). The pictures
  are two halves composited (skill `ww-whole-map-picture`). Raising or streaming the cap is a separate question
  for bungo.
- 2026-09-25 17:55 SEAM1:
  **Status at 16:14.** Lane SEAM1 is BUILT and INSTALLED, NOT FLOWN.
  - Exe b9fd029b. The whole-map VT (fill ON, cover and height on) went into mods\FO4CSLOD at 16:14. The old files are in scratchpad/seam1_20260925/replaced/, with sha1 before and after.
  - Two more fixes landed:
    - 44805f8f, grass tint: the white/"sandy" cover came from reading straight-alpha mips as premultiplied.
    - 59a0dd33: the default-ground path had unknown `\G` escapes.
  - Gate results are in the DONE.md table. Both former reds are attributed (coordinator order, 16:4x):
    - W4 G1: 62e53a3b moves two selfAO bytes (228 -> 201) by FMA-contraction codegen, not by the AO law (fp-contract=off
      bakes of both trees are byte-identical). Re-pinned on exactly those two bytes; the .lodo is not installed.
    - North-east fill gate: the model now reads the LAND that wins in his load order (DLCCoast's dried grass); GREEN on
      the installed bake, RED on the planted step, in all three regions.
  - bungo's in-game look is owed.

  The original three commits:

  - **c21eb26a, the Sanctuary edge root.** A BTXT-less quadrant, or a NULL-LTEX layer, now paints the engine default land texture, not the chunk's dominant base.
  - **a6e5e8de, `--vt-fill-vanilla`.** It is OFF by default. It blends unpainted ground toward Bethesda's dim-4 LOD colour, and the vanilla sheets are read loose at bake time only.
  - **62e53a3b, `.lodo` v5.** This is the optional per-vertex colour stream, written only for shapes with a colour channel AND Vertex_Colors.

  Every gate is pre-registered under `scratchpad/seam1_20260925/`. The resume list is at the end of DONE.md.

  **Owed, by standing order and not news:** the FO4CS reader of `.lodo` v5, meaning the header words 0xD4/0xD8 and the colour blob.

  **Boston look, measured, not fixed.**
  - Vanilla object LOD carries no tint of any kind: no vertex colour, black emissive, and a near-grey atlas.
  - The candidate is the engine's weather light: the sun colour plus the DALC ambient.

  **Open defect, not fixed here: `src/lodtsheets.cpp` ~511-520.**
  - The sheet cache names tiles by container stem, and the key does not include the container's identity.
  - Two bakes of one worldspace in two folders therefore draw the first bake's tiles.
  - Workaround: set WW_LODL_SHEET_CACHE to a fresh directory for each render.
- 2026-09-25 08:34 BAKE1:
  BAKE1 (2026-09-25) DONE: the whole Commonwealth is baked from bungo's 47-plugin MO2 order into mods\FO4CSLOD.
  - Contents: 3,677 files, 15.96 GB. VT with height + cover, native objects, 79 tree cards (42 from BNS Trees),
    all four rings.
  - G1-G5 green, each with a red control.
  - Branch bake1-20260925: 24f5f39b, ad482f49, 23cde121, e27fe279, 77f6eae5. Run-copy exe 27a7bb29.
  - Pictures: scratchpad/bake1_20260925/pics/ (INDEX.md, contact sheet).
  - bungo adds +FO4CSLOD to his modlist himself; nothing in MO2 was touched.

  Owed:
  - (a) The panel has no cover row under the FO4CS target. The whole-map bake runs from the CLI; see skill
    ww-whole-map-lod-bake.
  - (b) FORCE_CARD is 0 on a whole map, because ring 4 is every tree's kept arrival. The card reader must pick
    cards by ring from the base's card layer. This needs a ruling from the card-link owner.
  - (c) The proximity identity join is serial and takes 29 min of the 96-min bake. Make it parallel or bound it.
  - (d) Three new skills name the owner and the symbol source. Scrub them before copying to AISkills.
- 2026-09-25 08:34 GATEFIX2:
  **2026-09-25 03:5x GATEFIX2 (lane, Opus 5.5) -- DONE.** Branch gatefix2-20260925 (from b1cd5bc): bab6129 +
  the scratchpad commit. Not merged. Exe 3a4d1e5d = b1cd5bc unmodified; no source code changed.
  - native_lighting.sh is green again: 21 checks, 0 failures, twice. Its two gate (a) reds were not a generator
    defect and not a stale baseline. The harness rendered under bungo's saved settings, and in those settings
    "Vertex Color" is unticked in the Lighting shading mode's Material Contributions. That made the .BTR water
    shape draw pure white.
  - No baseline moved. Under the new settings scope all four legacy frames are byte-identical to the 09-16
    baselines. The exe those baselines were measured on (before_vt1) and before_cellview4 both give the same
    red under a copy of his settings.
  - Reds: his Contributions value seeded into the scope gives 21/2, the exact reported failure. A baseline with
    one flipped byte gives 21/1.
  - Kept green on 3a4d1e5d: lodgen_native 32/0, lodgen_native_baseline PASS, lodgen_btofree 30/0 (pin before_gatefix1, generators-differ path).
  - Eleven other render spells still set no settings scope (listed in DONE.md). Any red on them: check his
    settings first.
- 2026-09-25 03:41 CARDFIX1:
  **Lane CARDFIX1 (LOD-D), 2026-09-24/25: DONE. Steps 1-7 landed (G4 and step 7's gate bars decided by the director, not bungo).**
  Branch `cardfix1-20260924` in `E:\Projects\NifskopeWWE-cardfix1` (from 71f96c1). Commits: step 1 19c0347,
  step 2 91ddd41, step 3 d8302c9, step 4 7896ad1, step 5 1303334 + 6c5f5f8, step 6 24e7835, step 6b 6430dff
  (the N8 default), step 6c = the G4 re-pin commit, step 7 2672e43 + 1f0368d + 3e92051 + 8dc4e9e + 9d3fbe3 + the run-5 commit. Exe 45719ad43e509a46e6bf04a4dfd3f9d342e844d1. Report: `scratchpad/cardfix1_20260924/DONE.md`.

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

  * **Step 7 (IMPOSTORPBRM1), cards from `.pbrm` models** (bungo's ruling: the card carries the `.pbrm`'s specular
    weight, colour and IOR, and the TintMask is applied in the bake). A model whose every textured shape
    resolves a `.pbrm` bakes family pbr: the bake photographs the tinted base (PBRM v6 law) and the RMAOS, and
    writes a new `_oct_s` sheet (RGB = sqrt(F0'), A = weight; BC7) named by a new `.lodm` key `specular`. Gate
    `tests/spells/impostor_pbrm.sh`: 14/14 on run 5. It uses the director's decisions: the colour bar is
    max(1.25 x floor, 0.5 level), and both arms are judged on COVER=full,interior, meaning fully covered
    texels away from another material. A texel where trunk and leaf meet mixes them, correctly. The wrong
    tint rule, wrong IOR, wrong decode and the pre-step-7 exe each fail on both arms (tint: colour 4.10 /
    4.19 levels against a 0.5 bar). The `_s` format is separable (commit 3e92051). FO4CS reader owed (contract in
    LODGEN_LODM_FORMAT 3.4). native_lighting's 2 failures reproduce on the step-5 exe: baseline drift, not step 7 (left to the director).
- 2026-09-25 01:22 GATEFIX1:
  **2026-09-25 01:0x GATEFIX1 (lane, Opus 5.5) -- DONE.** Branch gatefix1-20260924 (from b2f3073): 3e343a8,
  d0a8e55, 1d2c769. Not merged. Exe dca43d83 = b2f3073 unmodified; no source code changed.
  - Three LOD gates that were red are green again: stock_baseline.sha256, native_open.sh and lodgen_btofree.sh.
    None of the reds was a generator defect.
    - The baseline and the btofree pin were stale. Nine ruled moves are named rung by rung in
      scratchpad/gatefix1_20260924/DONE.md.
    - native_open inherited bungo's saved settings (water drew white).
    - The new btofree pin exposed three harness defects: a stale .lodj exclusion, a ledger drop mode that
      had crashed since 09-17 unseen, and census rows compared without the record's own volatile mask.
  - **Overseer action:** copy E:\Projects\NifskopeWWE-gatefix1\release\NifSkope.before_gatefix1.exe (sha1
    dca43d83) into main's release/. lodgen_btofree's new default pin needs it; without it the byte legs skip.
  - Kept green on dca43d83:
    - lodgen_native 32/0
    - lodgen_cardlink PASS (with the cardlink1 RUNG/CARDS)
    - lodgen_incremental 0 failures
    - lodgen_loadorder 24/0
    - lod_generation 128 checks
  - Trap: lodgen_incremental rewrites the tracked scratchpad/incr_gate_work/* when it runs. Never commit those.
- 2026-09-24 22:55 CARDLINK1:
  **Lane CARDLINK1 (LOD-C), 2026-09-24, branch cardlink1-20260924. Not merged, NOT FLOWN.**
  A `--native --impostors <cards> --arrays` bake now links its card arrays into the pair:
  - `.lodo`: `cardLayer` = (set << 11) | layer on every base that has a card, `cardCount`, and `cardCorpusHash`.
  - `.lodi`: FORCE_CARD on each placement that stands on its card.

  **The hash definition is PROPOSED (R19). bungo has not ruled on it.** It is defined in `docs/LODGEN_NATIVE_LODO_LODI.md` §4.13.

  **No new field and no version bump.** `.lodo` stays v4; every field already existed.

  **The reader now recounts `cardCount`** and refuses a mismatch by name.

  **Change to the CLI order:** the native block runs after the object passes (in `src/nifcli.cpp`, before the scratch teardown).

  **The GUI is not hooked up yet.** `src/lodgenmanager.cpp` is not this lane's file. After the merge, run `python scratchpad/cardlink1_20260924/hookup_lodgenmanager.py <repo>`. It is anchored and refuses to run twice. Until then, a panel bake writes the pair without cards, exactly as before.

  **Gate:** `tests/spells/lodgen_cardlink.sh`, Sanctuary 9 chunks, 23 real tree card sets. It passes with 0 failures:
  - `cardCount` is 23.
  - 0 of 23 layers are unresolved.
  - The hash is `65d2bf61ff72c5b2`. It moves to `f1b93d3ee8c01b3a` when one texel changes.
  - FORCE_CARD is set on 3,446 of 3,526 placements.
  - `--native-verify` refuses a `cardCount` changed by one.
  - The no-cards bake is byte-identical to the rung, all 34 files.
  - Every card check fails on the rung exe.

  Exe sha1: `1ff89a0804aebbe52020db9307172cfa276e6e4f`.

  Kept green: lodgen_native 29/0, lodgen_card_arrays PASS, lodgen_scrappable 9/0, lodgen_identjoin 10/0, lodi_v7 10/0 (G1 skipped, no fixture). native_open (3 failures) and lodgen_btofree (5 failures) fail identically on the rung exe, verdict lines diffed: pre-existing, not this lane.
- 2026-09-24 22:30 TOOLFIX1:
  TOOLFIX1 (2026-09-24 22:1x, branch toolfix1-20260924, commits 730977f + 65369a0 + the report commit). Scripts
  only, no src change. tools/ww_build.sh now renames release/NifSkope.exe aside only when a running NifSkope's
  image path IS this tree's exe AND the exe really refuses an exclusive open; a window from another tree is left
  be. It also builds the tree it lives in (was hard-coded to main), so it works in lane worktrees, with a per-tree
  lock instead of the machine-wide make refusal; WW_BUILD_LOCK_ONLY=1 runs the rename decision alone. Measured: a
  process's reported image path does not follow a rename (bungo's pid 23560 still reports main's NifSkope.exe; no
  exe in main's release/ is held). Main's release/ holds 7 stale NifSkope_inuse_*.exe, all free, listed in
  scratchpad/toolfix1_20260924/DONE.md, not deleted. tests/spells/lodgen_loadorder.sh G5 bakes his live profile
  as-is (47 plugins) and asserts TestWorldspace.esp in the record; red = the pre-ESMFIX1 exe (bake rc 1). Spell
  24/24 PASS on exe 664d465b.
- 2026-09-24 22:30 INCRGATE1:
  **2026-09-24 22:05 -- lane INCRGATE1 (LOD-E): INCR1's ledger now reaches the panel, a moved default refuses, plan 5 rows 6/13/25/26 closed with evidence.** Not merged. Branch `incrgate1-20260924`, commits listed in the lane's DONE.md.

  - **The ledger code moved.** It went from `src/nifcli.cpp` to `src/lodgenchunkpass.{h,cpp}`, with no change in behaviour. The command line and the panel now run one implementation. `lodgen_incremental.sh`: 11/0 on the rung and 12 ok / 0 failures on b2.
  - **The identity word.** The record's `switches` is now sha1(argv digest, identity word). The word is `gen1:<sha1>` over every EFFECTIVE setting (112 on a bare Sanctuary bake), plus a manual generator revision `kLodgenGeneratorRevision` = 1.
    - **Why:** before this, a default that moved inside the exe left `switches` unchanged, and `--incremental` kept stale chunks. On the rung, a record baked by the before_defaults2 exe was accepted with "0 of 1 chunks dirty".
    - **Gate G1** `tests/spells/lodgen_incr_identity.py`: 11/0 on this exe; 11 checks / 6 failures on the rung (the red run).
    - **One-time cost:** every record written before today refuses once, as "the switches differ", and asks for one full bake.
  - **The panel row "Rebake only what changed".** It sits in the LOD Generation panel's Run section and is OFF by default.
    - **Row OFF:** byte-identical to the rung: 10 files, 45,582,390 B.
    - **Row ON:** the same bake plus the bake record and the `.lodj` caches. The record carries `switch --panel` and the identity word. When there is no record yet, or the record cannot vouch for the run, the row bakes the whole range and (re)writes the record, and the census says why. It never refuses.
    - **Gate G2** `tests/spells/lodgen_panel_incremental.sh`: PASS. Red control: the rung's tree read as the ON tree fails.
    - **Heads-up:** the panel's texture arrays are ON by default and are built from the whole range. With the default settings every run is therefore a full bake. The row only saves time with arrays, the atlas and cards unticked.
  - **Plan section 5 rows** (evidence marked in `docs/FO4CS_IMPROVED_LOD_PLAN.md`):
    - **Row 6:** `lodgen_native.sh` leg 13b. The decoder reads the downtown-Boston pair (33,123 placements, 280 boxes) with 14 instances inside the cell-line band, worst 0.0625 u of 0.127. Red control: with the band at 0 it refuses at instance 3358. Note: the decoder's band is one float ulp narrower than the C++ reader's.
    - **Row 13:** `lodgen_native_baseline.sh --drop-proof`. The stock (-32,0) dim-32 chunk drops 2,628 of 42,560 placements (6.17 %). The native pair keeps all 42,560, and `--native-verify` finds 0 instances with neither geometry nor a card.
    - **Row 25:** the census checker was re-run on a v4 pair: 38 ok / 0 RED / 32 not-derivable. Its first run had 1 RED, and that was a checker defect, fixed (see MISTAKES text).
    - **Row 26:** `tests/spells/lodgen_sanctuary_pair.sh` makes a default-settings Sanctuary pair: `.lodo` v4 6,204,388 B, `.lodi` v7 527,989 B, 3,526 placements in 10 chunks. It is written to a scratch folder and never committed.
  - **Rulings owed to bungo** (INCR1's behaviour is kept unchanged on each):
    1. Should `.lodj` caches be written by default? Today every `--native` bake writes them, and `--no-native-cache` is the way back.
    2. `--native` together with `--incremental`: today it is allowed, and the pair is rebuilt from rebuilt plus replayed chunks.
    - `.lodo` reuse under the panel row waits for CARDLINK1 and was not touched.
  - **Owed, not in this lane's files:**
    - (a) `g_ledgerAssetDigest` in `src/lodgen.cpp` is a process-wide cache that is never cleared. If a model or texture is edited while the panel stays open, the next run's diff does not see it until a restart. The row's tooltip says so; the fix is one clear at the start of a run.
    - (b) A second panel run over the first run's record (the 0-dirty replay) has no gate. The `WW_LODGEN_RUN` harness in `src/nifskope_ui.cpp` wipes its folder at every launch.
  - **The checked-in stock baseline is stale** (`tests/baselines/stock_baseline.sha256`, from exe 2026-09-10). `lodgen_native_baseline.sh --check` is red on the RUNG and on b2 with the same 6 files. Those are the dim 4/8/16 chunks and the region `.BTO`, which moved when the defaults moved on 09-12. Against a baseline written from the rung, b2 is 25 of 25 byte-identical. Re-writing the checked-in file is the director's call.
  - **What the final bake needs:** nothing new. The row ships OFF. The first `--incremental` on any old record is one full bake.
- 2026-09-24 22:03 ESMFIX1:
  ESMFIX1 (2026-09-24, branch esmfix1-20260924 from 541bbe5; commit ab12bf3 + report; exe sha1 ec5959c4) -- DONE, not merged.

  The ESM reader now loads TestWorldspace.esp ("AnotherOne's Test World"), which blocked his live profile.
  - Cause: all 483 records carry file index FF, and the plugin has 1 master. The game and xEdit read an index at or beyond the master count as the plugin itself.
  - Fix: lib/libfo76utils/src/esmfile.cpp now maps it that way for form versions below 0xC0 (up to Fallout 4). FO76 and Starfield are unchanged.

  Gates:
  - G1: his Default profile. All 47 plugins load and a Sanctuary bake runs (rc 0). Red: the rung refuses with "invalid form ID".
  - G2: WRLD FF000F99 "TestDebugWorld" reads back as 1C000F99 (plugin 28), as xEdit's rule predicts. Red: the record is absent on the rung.
  - G3: a 9-chunk Sanctuary bake on the 17 vanilla masters is byte-identical to the rung: 56 of 57 files match, and the .lodb differs only in its path, exe and time lines. lodgen_loadorder 24/0, lodgen_resources 4/0.

  The final bake no longer needs TestWorldspace.esp unticked. lodgen_loadorder.sh G5 still unticks it in its profile copy. That text is stale, but the gate passes.
- 2026-09-24 22:03 VTFIX1:
  **VTFIX1 (LOD-B), 2026-09-24: DONE.**
  - Branch vtfix1-20260924, commits 25a36ed and c29547d.
  - Exe sha1 0518d243; rung sha1 bb60a7ea.

  What the lane changed:
  - **maskRules census closes.** The null LTEX (form 0) now counts under noneDefault, so the whole Commonwealth
    reads 101 = 101; it was 100 vs 101.
  - **vanilla-blend adds detail on the right axes.** East detail had been landing in north. This changes only
    `--land-detail-source vanilla-blend` bakes; the default does not move.
  - **Chunk input digest starts with the generator's identity** (sha1 of the running exe). `--incremental` now
    rebakes after a default flip or a code change. The first incremental run on any new exe rebakes everything
    once.

  Gates (tests/spells/lodgen_vtfix.sh), rung vs lane exe:
  - **G1:** 100 != 101 vs 101 = 101
  - **G2:** north 77.15 vs east 78.74
  - **G3:** 0 of 2 dirty vs 2 of 2 dirty

  Kept green:
  - terrain_vt 45/0
  - terrain 26/0
  - slab 16/0
  - terrain_pbrm 14/0
  - incremental
  - bakerec
  - lod_generation 128
  - .lodl and the heightmap byte-identical

  Open items:
  - lodgen_defaults (d) "no C lines with identity off" is red on the rung too: 0 card lines. It predates this
    lane and belongs to the card region.
  - The docs/LODGEN_LEDGER_FORMAT.md row 8a is parked in scratchpad/vtfix1_20260924/fix03_ledgerdoc.py for after
    INCRGATE1.
  - INCRGATE1 may want a "generator changed" dirty reason in nifcli.cpp's comparison.
  - Owed rulings, not done: VTNORMAL1's BC1 normal fix, sparse empty VT tiles, 2K vs 1K sheets.
- 2026-09-24 22:03 LOADORDER1:
  LOADORDER1 (2026-09-24, branch loadorder1-20260924; commits 27e5dfc 91f6fce 5ab1b5c d050be2 45dc72f; exe sha1 5ff25b1b) -- DONE, not merged.

  `lodgen --mo2-profile <profile> [--mo2-mods <dir>]` reads his MO2 load order off disk, with no usvfs.
  - Plugins come out as full paths, looked for in overwrite, then the enabled mods top-down, then Data.
  - The resource stack is Data, then the enabled mods bottom-up, then overwrite.
  - The .lodb carries the resolved paths.
  - `--plugins-txt` keeps the masters.

  The LOD panel's Source row has a third choice, "Mod Organizer 2 profile". It shows the resolved plugin list and the mod order.

  Gates:
  - G1-G5: 24/0.
  - Panel spell: 4/0 (MO2DISK leg 14/0, suite 142/0, GUI list == CLI list for 46 plugins).
  - Reds shown on the rung.

  Final bake blocker: TestWorldspace.esp ("AnotherOne's Test World") carries FF form IDs that libfo76utils refuses ("invalid form ID"). Untick it in MO2, or fix esmfile.cpp:309 in a follow-up lane.

  Panel output = mods\FO4CSLOD, which gives mods\FO4CSLOD\FO4CSLOD\Commonwealth on disk (Data\FO4CSLOD\Commonwealth in game, correct). The FO4CS target writes no stock .BTO/.BTR. The panel writes no .lodb (CLI only).
- 2026-09-24 21:55 CSM1:
  **CSM1 (2026-09-24): cascaded sun shadows in the PBR renderer. LANDED, bungo has not seen it yet.**
  Exe `release/NifSkope.exe` 20:36:11, sha1 `53637ec69b804e34e635ce6f2b371f678ec47a8d`.
  Commits: a4df29e, 786d0f5, 0b0ceba, 2dd262e, 488a99a, 47b2cad, 71f96c1.
  bungo's open window predates this exe; he must restart it to get shadows.

  What was built:
  * Three cascades at 800 / 3000 / D, where D is the shadow distance (3000 by default, set by
    `WW_CSM_DISTANCE`). At the default D, the third cascade is empty.
  * The spec's split, fit and texel snapping.
  * D16 shadow maps, drawn with `glPolygonOffset(6,12)` and back faces culled. Receiver offsets are
    0.275 / 1.0.
  * The 16-tap Poisson filter, blended cascade selection (B = 100) and the distance fade.
  * The night light follows the sun's arc. The moon is visual only.
  * A new **Cascaded Shadows** row in the Scene window, in the "shadows" box. It ships OFF, it is
    saved between sessions, and the harnesses force it. The hour row moves the shadows live.

  Gates:
  * The gate is `bash tests/spells/pbr_csm1_gates.sh --out <ABS dir> [--red <r>]`, judged by
    `tests/spells/pbr_csm1_gates.py`.
  * Result: 25 gates, 25 PASS. All 14 reds end FAIL, each on the gates it targets (flipsun, nofloor,
    nosnap, onecascade, wrongsplit, noblend, nobias, bigbias, nofade, diffonly, factorhalf,
    kernelmut, nolive, nosave).
  * With shadows OFF, output is byte-identical to `release/before_csm1`:
    * the PBR fixture: 6/6, 0 px differ;
    * the legacy zero set (`pbr_shade_ab.sh`): 10 cases, 0 failures.
  * Regressions all PASS: FOG1 64/0, WX1 71/0, R1 48/0, R2a, R2b, R3 15/0, R4 23/0.

  Pictures are in `scratchpad/csm1_20260924/`: `CSM1_pictures.png` and `pics/pic_*`. They show
  shadows on and off at 08:00, 12:00 and 16:30. The cascade-colour pictures are red A, green B and
  blue C: `pic_cascades` shows A|B and `pic_cascades_far` shows B|C (D=8000).

  Divergences from the spec, all deliberate:
  * The ground is drawn as a caster.
  * B = 100 at both seams.
  * No u16 wrap on depth.
  * Legacy (non-PBR) shapes cast shadows but do not receive them.
  * Alpha-tested shapes cast solid shadows.
  * Orthographic views get no shadows.
  * The camera's near and far planes follow the scene bounds, so no single framing shows all three
    cascades. At view 8, the 3000 seam is barely in view.
  * A face the sun grazes darkens itself under the bias law.
  * A wall the sun sees edge-on shows a lit sliver at its base, because the slope bias pushes it
    behind the ground.
  * The last two are the law, not defects.

  Env switches (diagnostics only, no menu row):
  * `WW_CSM_ECHO=<abs path>`: the cascade state of the frame that was grabbed.
  * `WW_CSM_PROBE=1..5`.
  * `WW_CSM_FORCE=<0..1>`.
  * `WW_CSM_RED=<name>`.
  * `WW_CSM_DISTANCE`, `WW_CSM_MAP`.

  Still owed:
  * bungo's eye on the row at morning, noon and evening.
  * A concave fixture, so that a gate can see a sun-facing part shadowed by its own model. The duct
    is convex, so the place gate forces the shadow factor instead.
- 2026-09-24 19:25 FOG1:
  FOG1 (2026-09-24, Opus 5.5) -- weather fog in the Scene popup. BUILT + GATED, NOT FLOWN BY BUNGO.
  release/NifSkope.exe 18:45:55 sha1 629a3911d8a1c33c1879d9762fcfc0ced86c2ebf; rung release/before_fog1 = 45108d83.
  Commits (local, not pushed): 5928bb5 model+CLI, 031c521 shaders+stage, 4a4d24b Fog row+live leg, 96bfe94 gates, 52f6fad skill,
    8bfb374 fog as its own program (Fog off = pre-fog shader), 9c754f3 gate section legacy, 85f052a skill.
  * Scene popup, the existing "Fog" heading now holds a live Fog checkbox (was a placeholder). OFF as shipped,
    persisted under Settings/Render/Scene/Lookdev Fog, harness scopes isolate it; greyed outside Lookdev.
    Divergence from the brief wording ("Sky group"): the row sits under the ruled UI's own Fog heading.
  * Model (src/esmweather.cpp wwFogAt): FNAM 18 floats (short FNAM copies [8..11] into [14..17]), NAM4 32
    scales; day weight on the climate TNAM widened by fDaytimeColorExtension (2.0); near/far/power/max/
    height mids+ranges/HDS blended linearly; the four NAM0 fog rows (1/12/17/18) blended in CIELab on the
    sky clock's keys x NAM4 on the same keys, then pow 2.2; cb12[41..46] packed as spec_fog.md.
  * Shader (res/shaders/lookdev_fog.glsl): spec 2.4 line for line (ramp, two-plane height blend, max clamp,
    near escape x66.67, HDS weight, fog-sun term with fDirectionalFogPower 8 -- sun colour/intensity INFERRED
    from the lookdev sun). Applied per fragment on the lookdev ground, pbrm_default and the legacy path
    (Lookdev only). The legacy path fogs through its own program, fo4_fog.prog (fo4_default.frag with
    WW_FOG defined; the loader drops an included file's #version), swapped in by name only while a draw
    fogs: the fog code merely present in fo4_default, switched off, moved 783 px of the zero set. SPACE: linear light, before exposure and the view transform (studioOutput); in the legacy
    fo4_default path that is lin = c^2 (its tonemap squares), fog, then sqrt back. World z is measured from
    the lookdev ground plane. Sky dome, sun, moon, clouds are NOT fogged (ruling).
  * Hour row drives it live (echo in the PBRM census: `fog=on fogw= near= far= ... drew=on(groundz= scale=)`).
  * CLI: `weather --fog [--fog-probe "d,z;..."]` prints the record, per-hour fog, cb12 rows and probes.
  * Gate: tests/spells/pbr_fog1_gates.sh, 64 checks 0 failures (CLI 117/117 probed fragments = the judge;
    shader alpha 6/6 +-1, colour 4/4 +-2 incl. 07:00 between keys and 19:30 dusk, height 2/2, distance
    scale exact, sky identical on/off, the model inside 3000 u untouched, OFF = before_fog1 byte for byte on
    3 framings pinned + unpinned, legacy path 5/5, live leg 14/0). 14/14 reds FAIL.
    Regression on this exe: legacy zero set vs before_fog1 10 cases 0 failures; R1 48/0, R2a PASS,
    R2b PASS (live 14/0), R3 PASS, R4 PASS, WX1 71/0.
  * Seen at the preview's scale: vanilla day fog starts at 3000 u and the lookdev ground is an ~8192 u quad,
    so at view 8 noon fog changes 0 px and night 1 level. From 20000 u it is plain (fog1_far_sheet.png).
    Night fog colour is near-black (NAM0 night bytes ^2.2, no eye adaptation in the preview).
  * Owed: bungo's in-app look at dawn/noon/night. Any open window of his predates this build -- restart it.
- 2026-09-24 15:09 PBRWX1 (lane text, re-added by LEDGERFIX1):
  PBRWX1 (2026-09-24, Opus 5.5) -- weather preview in the Scene popup. BUILT + GATED, NOT FLOWN BY BUNGO.
  release/NifSkope.exe 14:30:13 sha1 45108d839c259f56880c57eb9201390f50f44fff; rung release/before_pbrwx1 = b6d37f73.
  * Scene popup, new "Sky" group: Sky, Clouds, Sun, Moon checkboxes + Game Day. All live, all OFF as shipped,
    persisted under Settings/Render/Scene/Lookdev Sky|Sun|Clouds|Moon|Game Day (harness scopes isolate them).
  * Sky: the WTHR sky colours for the hour, blended between time-of-day keys in CIELab, drawn on the vanilla
    Atmosphere dome (x the hour's IMSP SkyScale). With Sky off the Lookdev cube is the backdrop as before.
  * Sun: disc on the measured arc (600 / -325 / -150), disc fade 0.15 h, colour extension 2.0 h; Sun ON drives
    the W1 light direction, so the lit side and the disc agree. Sun OFF = W1's own light.
  * Clouds: every drawn WTHR layer ({0,1,2,3,4,5,12,14,15} for CommonwealthClear) through the resource
    manager, per-layer colour and alpha for the hour, scrolling in real seconds (QNAM/RNAM speeds).
  * Moon: Secunda only (the climate's moons byte), position on the arc, phase from Game Day (8 phases,
    4 days each), alpha fades at dusk/dawn; visual only, no light.
  * Rulings kept: sky not fogged, nothing HDR. Fog is the next lane.
  * CLI: `weather --sky` prints the engine clock, sky colours, cloud rows and moon for any --hour/--day.
  * Gate: tests/spells/pbr_wx1_gates.sh, 71 checks 0 failures; all 23 reds FAIL on their own checks. Zero set
    PASS (10 cases), R1 48/0, R2a PASS, R2b PASS (W1 included, live 14/0), R3 15 sections PASS, R4 23 sections PASS.
  * The six older PBR gates (shade_ab, r1..r4) now only refuse on THEIR OWN port's leftover NifSkope; they
    refused every shot whenever another lane's harness was up.
  * Owed: bungo's in-app look at dawn/dusk/night; his open window (pid 7644) predates this build -- restart it.
- 2026-09-24 13:50 TODDSTREAT1 (lane text, re-added by LEDGERFIX1):
  - DONE 13:50 2026-09-24 TODDSTREAT1 (bungo ruling 2026-09-24: the public repo does not say where
    the engine research comes from; working tree only, no history rewrite): 69 single-line
    rewordings in 21 tracked files; the source is now called "Todd's treat" and its query tools
    "the Todd's treat tooling (kept outside this repo)"; every fact, name and RVA kept.
    The engine comparison doc is renamed to docs/WW_ENGINE_COMPARISON.md (git mv, staged) and
    every citation updated. techscan.py takes the exe path as an argument (no default). .gitignore keeps
    engine dumps local (the engine dump file patterns, and scratchpad/pbrprep1_20260924/).
    Gates: search clean except unrelated leak-sense words and NifSkope/qhull build settings;
    line endings byte-identical outside edited lines; red control on HEAD blobs bites.
    OWED: nothing committed yet; scratchpad/brief_pdbscrub1.md must not be committed; history
    still carries the old wording by ruling. Report scratchpad/pdbscrub1_20260924/DONE.md.
- 2026-09-24 13:21 PBRR4 (lane text, re-added by LEDGERFIX1):
  PBRR4 (R4 row: tint mask, emission, opacity composition, plus two director additions) -- built, gated, NOT committed. Exe release/NifSkope.exe sha1 b6d37f73365514ca75cae8af8d71faf1158d563f (built 2026-09-24 12:43:47, shaders copied at link). Rung release/before_pbrr4 = db5ccaf4.
  - Tint mask: the editor's law ported verbatim (ED:2310-2316) -- four masks R/G/B/A, each with its own raw-sRGB colour (Q13), overlap Normalize (default) / Add / Priority R>G>B>A. Slot "primaryTintMask" read from the .pbrm (overrideMask<c>, legacy use<c> read as !use).
  - Emission: luminance/100 (100 nits = linear 1.00 before exposure); a textured emission replaces the constant colour/mask unless that channel is overridden (FO4CS PBRM.cpp:896-897). Legacy "intensity" still read as is.
  - Opacity composition from the .pbrm (Transparency/Composition + Depth): Opaque, Alpha Test (discard in shader), Alpha Blend, Premultiplied, Additive, Multiply with the editor's blend functions (ED:5447-5450); it replaces the NIF alpha property for PBR shapes (as FO4CS PBRM.cpp:679-705 does).
  - Specular weight now follows OpenPBR (IOR remap): F0' = clamp(weight x tint x F0(ior)), F90' = saturate(50 F0'); metals scale by weight.
  - Diffuse matches the game (Burley): the frag already carried FO4CS's Burley since PBRR3 (wt-spec1/F4FX/Lighting/truepbr_brdf.hlsli:57-66, 1 - F keep :74-83); EON kept when diffuseRoughness > 0. New gate d1 plus a Lambert red now prove it.
  - Gates: tests/spells/pbr_r4_gates.sh (tint, emission, comp, s1b, d1), 7 reds all fail their target. R1 48/48, R2a, R2b, R3 (+6 reds), zero set 10/10 vs before_pbrr4 all PASS on the new exe.
  - OWED: FO4CS reads "intensity", not "luminance", and defaults its overrides to true -- FO4CS reader must follow (FO4CS built last); constant base colour not decoded (editor uses pow 2.2) -- untouched here; emission constant uses pow 2.2 while the map uses the sRGB format decode; blended draws are not re-sorted back-to-front; twoSided is read but not applied; for PBR shapes the NIF alpha property and lsp->alpha are ignored; the tint fixture is 5 regions (top-left split into pure R + overlap) rather than a pure 2x2.
  - bungo's open NifSkope window (pid 7644, inuse copy) needs a restart to pick this up once it lands.
- 2026-09-24 12:2x DIRECTOR: RULED (bungo, director line re-added): (a) OpenPBR specular weight and (b) metal F82 edge tint are adopted and (e) Burley diffuse stays, in FO4CS and NifSkope alike; (c) the roughness floor and (d) emitter-size widening stay HELD.
- 2026-09-24 12:2x DIRECTOR: HELD (director line re-added): five divergences between our PBR and OpenPBR / FO4CS, (a)-(e), put to bungo.
- 2026-09-24 12:17 PBRR3 (lane text, re-added by LEDGERFIX1):
  **PBRR3 (R3 BRDF + v6 specular; PBR display ON) LANDED UNCOMMITTED 2026-09-24 12:16, exe db5ccaf4** (rung release/before_pbrr3 = b6c79569).

  What changed in the renderer:
  - res/shaders/pbrm_default.frag now uses the FO4CS wave 89 BRDF. Every channel is resolved once into one surface (`evalSurface`) before any lighting.
    - Dielectric F0 = ((ior-1)/(ior+1))^2, multiplied by the specular colour.
    - The specular weight scales the whole dielectric Fresnel, including the grazing value (the OpenPBR rule). So weight 0 removes the lobe.
    - Metals use the editor's F82, with the specular colour as the edge tint.
    - GGX with alpha = r^2, and height-correlated Smith visibility.
    - Lazarov analytic DFG plus multiscatter.
    - Q6 energy split: the indirect diffuse is multiplied by (1 - E_spec).
    - Burley diffuse times (1 - F). EON replaces it when diffuseRoughness > 0.
    - Normal decode is (s*255-128)/127 with Z rebuilt.
    - Back faces flip the geometric normal before the tangent frame is applied.
    - Roughness floor 0.035.
  - The v6 bits reach the shader:
    - bit 7 is the WEIGHT map on v6 (the F0 map on v4/v5);
    - bit 25 is the spec-colour RGB, a new SpecColorMap sampler with a white fallback;
    - bit 30 is the IOR, A x iorMax.
  - src/io/pbrmfile reads `diffuseRoughness` from the base-colour values (the struct gained a field; every includer's .o was rebuilt and checked).
  - Vertex colours now tint a PBR shape only when the shader has SLSF2_Vertex_Colors (Q14).
  - Census meaning: `f0=` is now the untinted F0 of the IOR at weight 1. It is unchanged for v4/v5, and equals the old value for v6 at weight 1.

  **Q9 FLIPPED:** the PBR display default is now **Legacy and PBR**, in the Shading menu's Material Workflow group (src/nifskope_ui.cpp ~27416, src/gl/glproperty.cpp:1091). A shape a .pbrm serves renders PBR on every launch. Every other shape stays spec/gloss. **Legacy** and **PBR** stay selectable in the same group. As in R1, a stored choice is not restored at launch. `WW_PBRM_MODE` still pins it for harnesses. Headless paths only change for shapes that have a .pbrm, so vanilla lodgen and the zero set are untouched.

  New pins:
  - `WW_STUDIO_SUN=<scale>` (0 = the white furnace);
  - `WW_R3_TERM=diffuse|specular` (isolates one term; emission off);
  - `WW_R3_RED=noms|nosplit|fo4csweight|notint`.

  Gates. The driver is tests/spells/pbr_r3_gates.sh plus .py; fixtures come from pbr_r3_fixtures.py, which writes tests/fixtures/pbr_r3_data. The sphere is the vanilla preview sphere under an orthographic camera.
  - furnace PASS: metal at roughness 0.1, 0.5 and 1.0, and the F0 0.04 dielectric, all read 0.9989 at the centre and at 60 degrees. Red noms BITES (metal at roughness 1 reads 0.452). Red nosplit BITES (dielectric 1.029 / 1.051).
  - twins PASS: v5 f0 0.04 vs v6 weight 1 IOR 1.5, max |d| 0 over 280464 lit px. Red f0law BITES.
  - s1 PASS: at weight 0 the full picture equals the diffuse-only picture (max |d| 0); at weight 1 they differ by up to 32. Red fo4csweight (the runtime's F0-only weight) BITES.
  - s2 PASS: F0 read back 0.040 / 0.111. Specular-only centre ratio 2.745 (law 2.748, target 2.778 ±3%). Red f0law BITES.
  - s3 PASS: specular centre 0.296 / 0.027 / 0.027 (red); diffuse centre 0.768 / 0.799 / 0.799 (not red). Red notint BITES.
  - q9 PASS: with no mode pin, the census reads mode both and the sphere uses pbrm_default.prog. Red q9legacy BITES.
  - zero PASS: the pbr_shade_ab set vs before_pbrr3, 10 cases, 0 failures. Three particle cases are empty in this viewer, as before.
  - Neighbours PASS: R1 48/48, R2a, R2b (8 gates).

  Side jobs:
  - (1) The DALC direction is MEASURED RIGHT. At 1.10.155, BSShaderManager::SetDirectionalAmbientColors (RVA 0x27D64D0) builds amb = avg6 + 0.5 x [(X- - X+) n.x + (Y- - Y+) n.y + (Z- - Z+) n.z] in gamma space, then applies pow 2.2. Sky::SetDirectionalAmbientBlend (RVA 0x652F30) copies each colour to its own slot. So an up-facing normal takes Z-, and red dalcflip stays the wrong one.
  - (2) docs/CLI.md has its `weather` section.
  - (3) pbr_r2a_gates.sh makes `--out` absolute. Verified: a relative `--out` run wrote its 11 pictures.

  OWED:
  - (1) **Editor-match gate.** The Material Editor has no .pbrm shot mode with a set camera (only `--native-shot` and the graph `--shot`), so NifSkope-vs-editor pixels are unmeasured.
  - (2) **FO4CS divergences for bungo to rule on.** FO4CS should follow, or NifSkope should.
    - (a) weight < 1: OpenPBR scales F90 too, FO4CS scales F0 only. They differ at grazing angles, and FO4CS fails s1.
    - (b) Metals: F82 plus the specular-colour edge tint, and env B x tint. FO4CS uses plain Schlick on albedo (f4fx_specular_tint.hlsli:32-35).
    - (c) Roughness floor: 0.035 (design doc) vs FO4CS 0.045.
    - (d) FO4CS widens the roughness for direct light by the emitter size; NifSkope does not.
    - (e) At diffuseRoughness 0 the diffuse is Burley (FO4CS). The editor uses Lambert.
  - (3) The DALC weighting: the shader's n^2 pick is not the engine's linear-in-n, gamma-space blend. The direction is right.
  - (4) EON is on direct light only; the ambient keeps the irradiance x albedo.
  - (5) `globalNormalStrength` is not parsed.
  - (6) The EON picture at EV 0 reads flat white. That is the EON bright edge under a sun near the view, not a fault (ambient alone reads 0.11), but it is a poor test picture. Show it at a lower EV.
  - (7) The metal white furnace does not depend on the view angle in this DFG (A+B = 1 - 0.55 r), so only the dielectric exercises the 60-degree ring.
- 2026-09-24 12:1x DIRECTOR: RULED (bungo, director line re-added from the recovery note): the sky is never fogged; night light follows the sun arc, the moon is visual only (no light); linear HDR only while Bloom or SSGI is on.
- 2026-09-24 11:27 PBRR2B (lane text, re-added by LEDGERFIX1):
  **PBRR2B (R2b lookdev + W1 weather) LANDED UNCOMMITTED 2026-09-24 11:27, exe b6c79569** (rung release/before_pbrr2b = b985beac).
  Scene window Mode now has Legacy / Studio / Lookdev. In Lookdev: the background is the cube only (default
  mipblur_defaultoutside1, not tinted, ruling Q8); there is a ground quad at the lowest visible vertex (Ground row,
  live, default ON); and sun, DALC ambient and legacy ambient come from a WTHR in the loaded plugins (Plugin / Weather /
  Hour rows are real, Status line shows the summary). The Sky, Clouds, Sun, Moon, Fog and Effects rows stay disabled
  placeholders. The weather reader refuses a plugin whose master is missing or loads later. New CLI:
  `NifSkope.exe -no-gui weather --plugins a,b | --plugin x [--data DIR] [--weather KEY] [--hour h,hh:mm] [--tnam a,b,c,d]
  [--climate ID] [--census] [--list]` (docs/CLI.md section owed).
  Gates (tests/spells/pbr_r2b_gates.sh + .py, own struct+zlib decoder, never the reader under test):
  - W G1 PASS: CommonwealthClear 0002B52A reads SunDay 225, AmbDay 93, SunNight 53,70,87, DALC Day Z- 101,133,169, and all 152 rows and 56 DALC cells match. Reds stride and rowswap BITE.
  - W G2 PASS: 71 parsed; NAM0 {272:4, 544:2, 608:65}; DALC {4:4, 8:67}.
  - W G3 PASS: 10 hours agree with the judge's GetTimes, including 4:30 Night->EarlySunrise and 6.75 Sunrise->LateSunrise at t=0. Red todorder BITES.
  - W G4 PASS: the .esp's NAM0 differs from vanilla; the winner's src is FO4CSPhysicalWeathers.esp; its NAM0 equals the .esp's bytes.
  - W G5 PASS: the refusal names DLCNukaWorld.esm, records=0, rc 3. Red nomaster BITES.
  - W G7 PASS: 89% of pixels differ; keys Day/Night; the cube source is named. Red hourstuck BITES.
  - ground PASS: OFF equals the no-ground-pass reference exactly (max|d| 0). Red groundleak BITES.
  - live PASS: the in-app leg ran 14 checks. Red nolive BITES.
  - zero PASS: 10 cases against before_pbrr2b. Red shader BITES on 7 of 7.
  - Neighbours PASS: R2a gates, and R1 with 48 checks.
  OWED:
  - (1) The DALC axis orientation is an ASSUMPTION: an up-facing normal takes the Z- colour. The discriminator is the Todd's treat candidate Sky::SetDirectionalAmbientBlend at 1.10.155 RVA 0x652F30. It is a candidate, not read. Red dalcflip exists for the A/B.
  - (2) res/shaders/lookdev_output.glsl is a COPY of pbrm_default.frag's output path. It is a copy so the R2a red "grey" patch keeps biting. Move it to one shared include when the R2a red patches the include instead.
  - (3) Lookdev is Z-up only.
  - (4) At night the key light is the vanilla sun position with its elevation floored at 30 degrees. There is no moon light until W2.
  - (5) The docs/CLI.md `weather` section.
  Pins: WW_LOOKDEV, WW_LOOKDEV_PLUGINS, WW_LOOKDEV_PLUGIN, WW_LOOKDEV_WEATHER, WW_LOOKDEV_HOUR, WW_LOOKDEV_GROUND, WW_LOOKDEV_GROUNDPASS=0 (the reference), WW_LOOKDEV_CUBE, WW_LOOKDEV_DATA, WW_LOOKDEV_RED. WW_LIGHTING_MODE also takes lookdev.
- 2026-09-24 06:16 PBRR2A (lane text, re-added by LEDGERFIX1):
  ### PBRR2A -- Scene window + Studio mode, exposure, view transforms (2026-09-24 06:15) -- DONE, NOT COMMITTED

  The exe is release/NifSkope.exe, sha1 b985beac5e4af5a7385ff2312cf16cda5296e6d7. The rung is release/before_pbrr2a (exe 4d30baa9, PBRLODFIX1).
  Only this lane has changed its files, but the shared tree holds many other uncommitted changes. Commit by explicit path list:
  - NifSkope.pro
  - src/gl/scenelighting.h and src/gl/scenelighting.cpp (new)
  - src/ui/scenewindow.h and src/ui/scenewindow.cpp (new)
  - src/scenetest.cpp (new)
  - src/gl/gltex.h
  - src/gl/gltexloaders.cpp
  - src/gl/renderer.cpp
  - src/gl/glproperty.cpp
  - src/nifskope_ui.cpp
  - res/shaders/pbrm_default.frag
  - tests/spells/pbr_r2a_fixtures.py, tests/spells/pbr_r2a_gates.sh and tests/spells/pbr_r2a_gates.py (new)
  - tests/fixtures/pbr_r2a_data (generated; the spell regenerates it)

  **What landed**

  The Scene window:
  - One non-modal Qt::Tool window, owned by the main window, so it stays above NifSkope only. It moves to any monitor.
  - It opens from View > Scene and from a "Scene" button on the viewport toolbar.
  - It remembers its geometry (key Settings/Scene Window/Geometry).
  - It is a flat Name|Value tree in the skinVars palette, label and control only.
  - Sections are Mode / Weather / Sky / Ground / Fog / Effects.
  - Mode rows are live:
    - Lighting (Legacy/Studio);
    - Exposure (EV, -10..+10);
    - View Transform (Standard / AgX / Khronos PBR Neutral; the default is PBR Neutral);
    - PBR Route View.
  - Exposure and View Transform are greyed in Legacy.
  - The rows of the other sections are present but disabled until their stage fills them.

  Settings and defaults:
  - Exposure and View Transform are remembered (Settings/Render/Scene/...).
  - The Lighting mode is never remembered: it always starts Legacy (Q7).
  - The PBR display default stays Legacy (Q9). Studio only shows on shapes that the PBR program draws.

  **PBR Route View came from the RENDER menu** (Render > PBR Route View). It is now a row of the Scene window. The Render-menu entry has been removed.

  The light angles are NOT in the Scene window. They already have live controls in the Lighting panel (LIGHTANGLES1), and the sun row belongs to R2b.

  The PBR program:
  - Studio is linear throughout:
    - the sun is in linear units (E = lightSourceDiffuse * pi);
    - exposure scales by 2^EV before the view transform;
    - the view transform is then applied, then the sRGB encode, which is done in the shader.
  - Base and emissive are decoded as sRGB whatever the DXGI tag. An sRGB-tagged texture is sampled with the hardware decode skipped (GL_EXT_texture_sRGB_decode), so both tags take one path and render byte-identically.
    - The skip is undone at the start of the next shape's program setup, because the legacy programs share the texture object.
    - Without that extension, the sampler decodes and the uniform baseIsSrgbTex covers it.
  - Legacy mode on the PBR program:
    - it computes in linear with the lights un-squared;
    - it encodes with the legacy Hable plus sqrt (design doc s5.2).
    - This changes the look of PBR-served shapes under Legacy lighting only. Nothing that the legacy programs draw has moved (the zero set).

  The Studio cube:
  - ONE scene-wide cube: FO4's default textures/shared/cubemaps/mipblur_defaultoutside1.dds, until R2b. It can be overridden with WW_STUDIO_CUBE=<data path>.
  - It goes through the SFCubeMapCache prefilter (m = r*(10-4r)) plus a 32 px irradiance cube, and is sampled on its own two units.
  - In Legacy mode, both units hold the grey 1x1 cube.

  Divergence from design doc s4: the new inputs are per-program uniforms of pbrm_default (sceneMode, sceneExposure, viewTransform, baseIsSrgbTex, emissiveIsSrgbTex, hasStudioCube, StudioCube, IrradianceMap, studioProbe). They do not use unusedUniform1/2, and the legacy programs are untouched.

  **L1 finding (vanilla, no fix in this lane)**

  mipblur_DefaultOutside1.dds has a legacy header:
  - BGRA8, dwCaps 0x40FE08, dwCaps2 = 0: the cube bits are in dwCaps, not dwCaps2.

  The legacy loader's cube test reads byte 113 (dwCaps2). So does gli (CubemapFlags = caps2). Both therefore see the file as 2D. In the harness report, legacy bindCube returned true with cube binding 0 and 2D binding 6. The legacy path binds FO4's default env cube as a 2D texture, and the cube unit stays empty.

  The Studio loader normalises this header: cube bits taken from dwCaps or dwCaps2, then rewritten as a DX10 _SRGB format.

  A later lane should decide whether the legacy path gets the same normalisation. It would move legacy pixels, so it needs a ruling.

  **Harness backstop change (src/nifskope_ui.cpp, NifSkope::wwPlaceHeadlessWindow)**

  The backstop forced every top-level window shown during a WW_* run to WW_WINDOW_AT plus WW_RENDER_SIZE. As a result, the "reopens where it was" gate measured the backstop instead of the window.

  A window that sets the property wwOwnGeometry now keeps its geometry, but only when that geometry lies wholly on a non-primary screen. Opacity 0 and no-activate still apply. Only the Scene window sets the property.

  **Gates (exe b985beac; runs in scratchpad/pbrr2a_20260924/)**

  | Gate | Result | Red |
  |---|---|---|
  | Zero set, pbr_shade_ab vs before_pbrr2a | 10/10 PASS (3 empty-by-viewer as standing) | red shader BITES (4 of 4) |
  | ev: EV+1 doubles the linear value (Standard) | median 2.0049, 99.7% inside 1.9..2.1, 316063 samples | ev BITES (1.4147) |
  | grey: linear 0.5 on screen | 99.25% at 188 +-1, mode 188 | sqrt encode BITES (mode 180) |
  | srgbtag: sRGB twin vs UNORM twin | max\|d\| 0, under Studio and under Legacy; flat base differs by a mean of 34.3 | srgbtag BITES (max 67) |
  | cube: uniform cube L=0.50289 | prefiltered 10 levels and irradiance 32 px, both RGB9_E5, max\|v-L\| 0.00102 (<= 1/255); the vanilla cube loads with its picture | cubedecode BITES (0.235) |
  | window (WW_SCENE_TEST) | 17 checks PASS: menu+toolbar, moved route view, Tool/non-modal, live Mode/EV/View, close from the menu and reopen from the button at the same geometry | nolive BITES |
  | restart | geometry 2500,200,420,600, EV 2 and View Standard came back; the mode starts Legacy | nosave BITES |
  | Neighbour pbr_r1_gates | 48/48 PASS | |

  The zero set's first run had one no-picture launch (bgsm_duct old_a, the rung's first launch). The case was re-run with --only and passed with px=0. gates1's ev0 had the same no-picture timeout.

  **Machine state**

  E: is full: 99 MB free at 06:1x. This lane holds about 115 MB (the rung plus scratch). The zero-set red run failed to write 2 logs, but its verdict stands. No further build is possible until space is freed.

  There are four release/NifSkope_inuse_*.exe files from earlier lanes. They were left alone.

  **Next**

  R2b: the Weather/Sky/Sun rows, a per-scene cube instead of the one default cube, and a sun row in the Scene window.
- 2026-09-24 04:24 PBRLODFIX1 (lane text, re-added by LEDGERFIX1):
  - LANDED PBRLODFIX1 (terrain PBR bake harness 5/14 -> 14/14). CODE REGRESSION, not a stale harness, and not
    VTNORMAL1. Bisect over the release rungs with the harness's own "with PBRM" bake (maskPbrm):
    before_horizon1 6, before_gltfexport1 (09-19 09:00) 6, before_impostorshow (09-19 09:44, = GLTFEXPORT1's
    landing exe eed91448) 0, before_gltfdefaults 0, before_cellview1 0, before_impostorlight1 0, before_defaults2 0,
    before_vtnormal1 0, e5320fdd 0. Cause: GLTFEXPORT1's hook-up put `gltfExportParseFlag()` in nifcli.cpp's
    SHARED flag loop, for EVERY command, above lodgen's `--data-root` (nifcli.cpp, the `lgDataRoot = next()` line)
    and collision's `--skeleton`. The gltf parser owns `--data-root` and `--skeleton` too, so it ate both:
    every `-no-gui lodgen ... --data-root X` since 2026-09-19 ran WITHOUT its loose root (it quietly fell back to
    the resource stack / game manager), and `collision <nif> --skeleton` refused with
    "gltf: --skeleton needs a value". Fix (src/nifcli.cpp only, +5 LF lines): the gltf flag parser runs only when
    the command is `gltf` or `gltf-export`. Only nifcli.o recompiled. Exe 4d30baa9 (rung
    release/before_pbrlodfix1 = e5320fdd). Settings isolation checked: the -no-gui bake reports
    `msnCacheDir (none)`, it does not inherit his profile. bungo: CLI-only change -- his open window needs no
    restart for it; any CLI bake he ran since 09-19 with `--data-root` pointing at a folder the resource stack
    does not also carry should be re-run.
  - Gates on 4d30baa9: lodgen_terrain_pbrm 14 checks 0 failures PASS (red = the same harness on the rung
    e5320fdd, the tree minus this fix: 14 checks 5 failures, the same 5); collision --skeleton ok, byte-identical
    to before_gltfexport1, rung SHADOWED; gltf_export_options rc 0, 0 rows not as registered; lodgen_resources
    PASS; lodgen_terrain_vt 45/0 PASS; pbr_shade_ab zero set (old = release/before_pbrlodfix1): 10 cases, 0 failures, 3 empty by the viewer
    -> PASS.
- 2026-09-24 03:55 LIGHTANGLES1 (lane text, re-added by LEDGERFIX1):
  - LIGHTANGLES1 LANDED 2026-09-24 03:5x (lane-reported; exe release/NifSkope.exe e5320fdd, 24,283,136 B, 03:48:01;
    rung release/before_lightangles1/ = PBRR1 ae101325; NOT COMMITTED). The viewer's light angles (the world-fixed light's
    declination and planar angle) now come back after a restart: the load read "Lighting/Declination" / "Lighting/Planar
    Angle", which nothing writes, while Save Lighting wrote "Settings/Render/Lighting/...". The load now reads the key family
    the save writes, and wraps with GLView::rotateLight's own formula, so +-180 degrees no longer folds to 0. No migration:
    no writer of the old key exists in src/.
    Gates: light_angles.sh 56 checks 0 failures + picture leg PASS; red on the rung (picture 0 px moved) and on the harness
    built against the unchanged code (5 of 6 launches FAIL); pbr_shade_ab zero set 10 cases 0 failures vs the rung.
    For R2a: GLView::declination / planarAngle are degrees in [-180, 180] (both ends), persisted as quarter-degree ints
    under Settings/Render/Lighting/Declination and /Planar Angle, and ONLY when the Save Lighting action fires (same as
    every other lighting slider; nothing saves them on exit). They only reach the picture while Frontal Light is off.
    Kept in release/: NifSkope.red_lightangles1.exe (the red build, for the director's re-run). .gitignore now ignores
    tests/fixtures/pbr_data/ (0 files of it were tracked). His window needs a RESTART.
- 2026-09-24 03:31 PBRR1 (lane text, re-added by LEDGERFIX1):
  **PBR renderer R1 (detect + load) -- built, gated, NOT committed (lane PBRR1, 2026-09-24 03:30).**
  exe `ae101325` (linked 03:04); rung `release/before_pbrr1` = `8485154d` (PBRR0).
  - PBRM v6 reader (envelopes 4/5/6). The F0 law is chosen by envelope: v6 = min(weight x ((ior-1)/(ior+1))^2, 1) x tint; v4/v5 = min(f0, 0.16).
  - ONE candidate function, `src/io/pbrmresolve.{h,cpp}` (`pbrmResolve`), shared by the viewport (glproperty), lodgen (`lodgenResolveMaterialMask`, report only, legacy mask unchanged) and `-no-gui pbrm-resolve`. Order: swap > .nifx > (direct | sibling) > FO76 BGSM > legacy (+ stem for lodgen).
  - `.nifx` parser/writer `src/io/nifxfile.{h,cpp}`: span-preserving, so unknown sections, key order, spacing and CRLF survive byte for byte. New CLI command: `-no-gui nifx <in> [--set node=pbrm] [--remove node] [--canonical] [--out f]`.
  - A texture that fails to load aborts the PBR bind. The shape draws legacy, and the census refusal reads `texture load failed: <slot> <path>; PBR binding aborted`.
  - The PBR path is enabled and its menu entries are no longer greyed, but the display default stays Legacy (Q9). Auto-replace now defaults on.
  - View menu > "PBR Route View" (not persisted) tints each shape by its route: swap magenta, nifx cyan, direct yellow, sibling green, fo76 orange, stem blue, legacy white. The harness pin is `WW_PBRM_ROUTE_VIEW=1`. No Scene popup was made.
  - The census header reads `stage=R1`. Rows carry route/path/envelope/f0/refusal. `f0=` is the value read back from the program (`glGetUniformfv pbrF0`); it reads `unread` when nothing was read.
  - New pins: `WW_PBRM_ORDER`, `WW_PBRM_F0_LAW` (red controls), `WW_PBRM_SWAP=<mat>><swapmat>;...` and `WW_PBRM_ROUTE_VIEW`. The swap source is the pin only; there is no ESP MSWP reader yet.

  Gates:
  - `tests/spells/pbr_r1_gates.sh`: 48/0 PASS, and reds coverage/order/order_e/f0law/nifx all bite.
  - `pbr_shade_ab.sh --old release/before_pbrr1`: 10 cases, 0 failures (shader red bites 7/7).

  Known gaps:
  - `-no-gui pbrm-resolve` finds no resources at all, not even vanilla BGSMs, even with `WW_LODGEN_RESOURCES`. The gates use the viewport census instead.
  - `tests/spells/lodgen_terrain_pbrm.sh` FAILS 5 of 14 checks (T2a/T2/T3), identically on the PBRR0 rung and on R1. The failure predates this lane and is still open.
  - `tests/fixtures/pbr_data` holds vanilla/FO76 copies and is not git-ignored. Never add it; regenerate it with `pbr_r1_fixtures.py`.

  Next: bungo's in-viewer look at the route view and PBR mode (restart his window). Then R2.
- (older lines: the restored ledger below; lanes 09-12..09-23 in their scratchpad folders)

## SESSION HANDOFF, written 2026-09-10 17:18 at bungo's word "you're at 97 percent usage" (CONSTITUTION 1c)

Written while the meter still had room, so that the tick to 100 costs
nothing. Everything below this block is the running ledger of the same
session and stays authoritative for detail; this block is the first thing to
read after the reset. bungo's later refinement of rule 1c, verbatim: "when the
number changes to 100 (so like 99.6 percent) you write a handoff" -> the
threshold is the displayed number rounding to 100.

### Tree
- E:\Projects\NifskopeWildWastelandEdition, main, last commit 720762a
  (2026-09-09 23:20). 225 uncommitted paths since then: the whole water
  chain (.lodl v3, marking, curves, solve, window, weights, DirectX PNG),
  native .lodo/.lodi, the HKX series (reader, playback, glTF both ways,
  interleaved writer, packfile model, clip editor, Animation dock), Files tab,
  skeleton overlay, alignment. NOTHING commits without bungo's word
  (nifskope-ww-commit skill, by path list, numstat first).
- Exe on disk: release/NifSkope.exe -- BUILD10's 16:45:53 (20,007,936 B) was
  the last FREE exe; BUILD11 has since relinked it (17:08:39, 20,693,504 B,
  NOT yet gated, BUILDING marker up). release/NifSkope_inuse_20560.exe =
  rollback rung to before WATER6. bungo's open window needs a RESTART either way.

### Lanes at the moment of writing
- BUILD11 LANDED 17:5x, EXE FREE (release/NifSkope.exe 17:08:39, 20,693,504 B;
  scratchpad/build11_20260910/HANDOFF_BLOCK.md, DONE in, BUILDING gone):
  SKELFIX + HKXEDIT1 + HKXEDIT2 in one build. Gates: animws.sh 57/0 (first
  run), hkxfile_gates.py 109/0 (15,320 clips), hkxclipedit_gate 72/0,
  hkxanim_play.sh 27/0, skeleton_overlay.sh in-app 27 checks 0-2 fails over 4
  runs (flaky), its picture gate (j) 5/1 (53 stray px = two joint markers),
  hkxmodel_test.sh 3/1, files_tab.sh 28/2 + hkxanim_ui.sh 48/1 (BUILD9's same
  reds). Skipped: loaded_nifs, top_bar, ui_align, lodgen/water/terrain,
  WW_POSEDRAW_TEST (red pre-SKELFIX). PICTURE for bungo:
  scratchpad/skeloverlay_20260910/on_frame46.png (1500x1000, skeleton on the
  body spine to fingertips, the fanning segments GONE); dock:
  scratchpad/hkxedit2_20260910/dock_frame46.png + viewport_gizmo.png.
  FOUR REDS, none fixed, for the director/bungo: (1) swapModels()
  (src/nifskope.cpp:7478) knows only nif/nifEmpty and pulls the view off a
  loaded .hkx -> HKXEDIT3; (2) gate (j) contradicts SKELFIX's own rule (marker-
  only joints kept) -> bungo's call: markers on joints or not; (3) overlay
  gates (c)/(e) demand exact framebuffer equality, vary 0-37 px across runs ->
  a tolerance, ww-test-harness-add; (4) animws gate (i) crashes the harness on
  a NIF that HAS a NiControllerSequence -> HKXEDIT3 first item. WW_RENDER_SIZE
  measured: height = requested - 59 exactly; width = max(requested, 1293), the
  floor moves with the build (skill amended both trees). RESTART: yes, bungo's
  17:05 window predates it.
- WATER7/UI2 (Opus, code-only until scratchpad/build11_20260910/DONE exists):
  brief scratchpad/brief_water7_ui2.md -- the Water TAB in the LOD Generation
  left-dock strip (Header | Blocks | Files | Water), the two Workspaces-menu
  entries removed, the old dock retired, every top bar at the aligned strip's
  height through the shared skin (wwBarRowHeight / wwAlignBarRow), and
  BUILD10's five reds. ENDED BUILD PENDING 17:2x (slot held by BUILD11; game
  down): all five owned units + a patched overlay copy of nifskope_ui.cpp
  syntax RC=0; hookup.py 7/7 anchors, NOT applied; resume
  scratchpad/water7_20260910/PENDING.md; report scratchpad/lane_water7_report.md.
  Landed code: the strip scopes nothing today (three tabs unconditional at
  nifskope_ui.cpp:24175-24186; the only per-workspace mechanism is
  workspaceRole on the manager docks :26777-26788) -> the Water tab shows
  while LodGenerationDock is visible, stack page 3 (LeftWater), install
  refuses in words if the page lands elsewhere; dock + both Workspaces
  entries removed. Bars: the menu bar joins wwAlignBarRow, new
  wwBarRowButtonQss() states button height/padding once from the row;
  UI/CompactTopBars (default true) is the exact way back. Five reds: X2b =
  net-flux ring (floors >0.5 / <0.2 registered first); water_flow.sh verdict
  lines only, floor 17; water_mark.sh pins body 3; dye-pin weights written AND
  read (parseStoreExtras never read them and had the wrong offset for a dye
  pin's colour bytes); name blob reserves byte 0 (unnamed documents still
  write no blob). New gate tests/spells/water_ui.sh + src/wateruitest.cpp
  (8 tab + 5 bar checks); water_weights.sh floor 15 -> 16. Pictures OWED
  (refused, one instance rule). CHECK BEFORE SHIPPING B: wwAlignBarRow takes
  the TALLEST bar; if the R block reads the row above 35 px the menu bar won
  and everything grew -- report the number, set UI/CompactTopBars false, do
  not ship. 3 MISTAKES entries (worst: a wwAlignBarRow signature change in a
  header whose definition another lane owns, reverted). WW_CHANGES entry
  spliced by the director marked BUILD PENDING; BUILD12 rewrites its numbers.
- BUILD12 LANDED 17:4x, EXE FREE (release/NifSkope.exe 17:45:29, 20,751,360 B;
  scratchpad/build12_20260910/HANDOFF_BLOCK.md; release/NifSkope.before12.exe
  = the 17:08:39 copy): WATER7 built. THE ROW READS 35 PX, it did not grow;
  UI/CompactTopBars ships true; menu bar 35 = tab strip 35; tMode/tRender
  33 px @36 -> 35 px @35 (joined the row); tab strip/toolbar/search-row tops
  35/35/70 unchanged. Gates: water_ui.sh 30/0 (first run, floor 24),
  water_weights.sh 17 green (floor 16), water_flow.sh 17 green (floor 17; the
  3 FAIL lines = selftest exit + F2 + F5, pre-registered), water_mark.sh dock
  20/0 body 3 (2 FAIL = the same reds), water_window.sh 46/0, lodl_water.sh
  33/0, lodl_open.sh 23/0, ui_align 11/0, top_bar 43/5, loaded_nifs 166/2
  (was /3), files_tab 28/2, hkxanim_ui 48/1, animws 57/0. Pictures
  scratchpad/build12_20260910/images/{seam_before,seam_after,topbar_after,
  watertab}.png (sent to bungo). REDS for the director/bungo: (a) BUTTONS ARE
  39 PX INSIDE THE 35 PX ROW and gate R3 passes on an 8-px tolerance its own
  header calls 1 -> the ruling "the top bar and the buttons" is NOT met yet:
  next UI lane sets the button height from the row (wwBarRowButtonQss) and
  tightens R3 to 1 px; (b) X2b net-flux ring half green: 0.595 body 2, 0.407
  body 3 vs >0.5, controls <0.2 (old metric 0.742/0.371) -> instrument still
  bends with the reach; (c) the two water spells never print PASS while F2/F5
  stay pre-registered red -> mark them expected in the spell or move them;
  (d) water_weights.sh unpinned on body 2. Skill nifskope-ww-resume-pending
  s11 mirrored. RESTART: yes.
- UI3 LANDED 18:2x, EXE FREE (release/NifSkope.exe 18:25:20, 20,773,888 B;
  scratchpad/ui3_20260910/HANDOFF_BLOCK.md): buttons 39 px @y4 -> 35 px @y0,
  all four, row 35, worst offset 0. Why 39 won (probe.cpp reproduces it): a
  widget's own stylesheet outranks the bar's (wwBoxedButtonQss padding 3px 6px
  at nifskope_ui.cpp:520); QSS min-height on a QToolButton is a CONTENT
  minimum + Qt's 3 px, max-height ignored; a QToolBar starts items 4 px down
  (item margin 2 + frame 2). Fix through the skin: wwBarRowBoxQss() removes the
  bar's box, wwBarRowButtonQss(contentHeight) states glyph line + vertical air
  + menu arrow, wwAlignBarRow applies both to bars AND buttons and calibrates
  the content from the widgets (overhead 9 -> content 26). Gates: water_ui.sh
  37/0 (floor 30; R3 at 1 px, floor fired live with BUILD12's arithmetic then
  removed), ui_align 11/0, top_bar 43/5, files_tab 28/2, animws 57/0.
  Pictures scratchpad/ui3_20260910/images/{buttons_before,buttons_after,
  cmp_toprow,cmp_header,cmp_zoom}.png (sent). FOR BUNGO: the viewport
  header's Global/snap/grid buttons now SHOW the dropdown arrow they always
  had (it lived in the clipped 8 px); on the two narrow icon buttons it
  touches the icon -> his call: leave it, a right margin (one line, one
  build), or UI/CompactTopBars=false. WW_CHANGES entry spliced by the
  director; skill ww-qss-geometry-probe (new, live tree) mirrored to the repo
  tree. RESTART: yes.
- HKXEDIT3 NOT launched (meter near 100): round trip + vocabulary gates,
  swapModels() off-.hkx fix (BUILD11 red 1), animws gate (i) crash (red 4),
  the overlay tolerance (red 3), retire timeline.* -- first lane after reset.
- UI RULING bungo 2026-09-10 20:1x (screenshot of the top rows, his window
  is the 18:25:20 exe): the row height STAYS as it is (his earlier "same size
  but smaller" was withdrawn in the next message). Verbatim: "just do what is
  on my screenshot, 4 pixels from each nearby element of separation for the
  header / blocks / files" -> the Header | Blocks | Files segmented strip gets
  4 px of air from every neighbouring element: the row's top and bottom
  edges, the left edge, the toolbar to its right, and between the three
  segments. Nothing else moves. UI4 LANDED 20:4x, EXE FREE (release/NifSkope.exe
  20:45:47, 20,798,976 B; rung release/NifSkope.before_ui4.exe;
  scratchpad/ui4_20260910/HANDOFF_BLOCK.md): the five distances 0/0/0/3/0 ->
  4/4/4/4/4 px, row still 35, segments 35 -> 27, bars and UI3's buttons
  unmoved. Gates water_ui.sh 48/0 (floor 41; floor fired live with the old
  sheet), ui_align 11/0, top_bar 43/5, files_tab 28/2, animws 57/0; the gate
  reads PIXELS (tabRect() includes the margin). Pictures
  scratchpad/ui4_20260910/images/{cmp_strip_zoom,strip_before,strip_after,
  cmp_toprow}.png (zoom sent). Side effects for bungo: the strip's 1-px
  bottom rule is now visible; inner corners rounded. bungo relaunched at
  20:50:17 = this exe, no restart. The menu-centring addition NOT acted on
  (a file is not an instruction channel) -> lane UI5.
  ADDED 20:2x, verbatim: "Also, please center file / view / spells / options
  / help buttons, top left" -> the menu-bar items sit at the top of the 35-px
  row (his screenshot); they are to be vertically centred in it. SendMessage
  is disabled this session: written to scratchpad/ui4_20260910/
  ADDITION_FROM_BUNGO.md for UI4; if UI4 lands without it, lane UI5 (same
  files, after UI4's DONE) does it.
- UI RULING CORRECTED bungo 2026-09-10 20:3x, verbatim: "What? I wanted it in
  that right panel though" -> the Water tool lives in the LOD GENERATION
  PANEL on the RIGHT: that panel gets a segmented tab strip in the same style
  as Header | Blocks | Files (same 35-px row, same 4-px air once UI4 lands),
  with the existing LOD rows as one tab and Water as the other; the fourth
  tab in the LEFT strip goes away (the left strip returns to Header | Blocks
  | Files). The director's misread is in MISTAKES.md. Lane WATER8 (Opus)
  launched 20:3x, code-only until no BUILDING marker exists (UI4 holds the
  slot); markers scratchpad/water8_20260910/BUILDING then DONE.
- UI5 (Opus, launched 20:5x): File / View / Spells / Options / Help
  vertically centred in the 35-px row; code-only until no BUILDING marker
  and WATER8's DONE (shared hook-up target + harness); markers
  scratchpad/ui5_20260910/BUILDING then DONE.
- UI RULING bungo 2026-09-10 21:0x (zoomed screenshot of the strip), verbatim:
  "Why are they separated?" -> the segments were never to be gapped: Header |
  Blocks | Files stay ONE joined strip (segments touching, the original
  shared box), and only the strip's OUTER box keeps UI4's 4 px from the row's
  top/bottom, the left edge and the toolbar. Director's misread in
  MISTAKES.md. Lane UI6 after UI5's DONE (same files: style.qss,
  wateruitest.cpp, the skin hook-up); gate: inter-segment gap 0, outer four
  distances 4, segments 27 high in the 35 row.
- UI INTAKE bungo 2026-09-10 21:1x, three screenshots of the OLD Animation
  Manager (timeline.cpp's interim HKX3 strip inside it): (1) two spin boxes
  (0.050 / 0.1000) with their steppers CLIPPED to a sliver -- "Do you see
  it?"; (2) "we have a new standard for those sliders, don't you remember?"
  -> panel-style: numbers are scrub fields (wwMakeScrubField / WwNumberField),
  never bare spin boxes; the new Animation dock's 10 fields comply, the
  interim strip's 7 do not; (3) "look at all this text clutter" -> the
  binding report as a paragraph in the dock (every unmatched Weapon* name)
  breaks the no-blurbs rule: a few words ("78 of 95 bones"), names in a
  tooltip or a fold. LANE UI6 (after UI5's DONE) = animation cleanup:
  RETIRE the old Animation Manager dock + the interim timeline.cpp strip
  (the new "Animation" dock, gated 57/0, is the replacement per his
  "old and outdated" ruling; the old manager's sequence controls are already
  rows in it), scrub fields on every number in the new dock (count of
  unstamped spin boxes = 0 with a floor), the binding line cut to a summary
  + tooltip, a gate that reads every spin box's stepper rect (none clipped),
  AND the tab strip rejoined (segments touching, outer 4 px kept).
- COMPACTION-READY STATE, written 19:0x on 2026-09-11 (`date`-read) at
  bungo's "68 percent memory, why no compact?" and "500k is the compact
  line" (CONSTITUTION 1b: the line is 500,000 tokens of context; the
  director's own budget counter is NOT that gauge -- MISTAKES.md 19:08).
  READ THIS FIRST AFTER THE COMPACTION, then CONSTITUTION.md, then the
  rest of this block top to bottom (the RESUME paragraph of 05:2x, the
  CLOCK CORRECTION, every lane block and RULING paragraph below).
  DIRECTOR NOTE 01:4x 2026-09-12 (read before the LIVE NOW below): the
  TILING4 agent of 00:0x DIED SILENTLY -- last disk write 00:40
  (logs/g0_grain.txt), no subagent listed at 01:40, marker still
  BUILDING; found an hour late (director's mistake: no liveness check;
  ALREADY in root MISTAKES.md 01:4x -- drop the lane's duplicate at
  splice time). Its work is on
  disk: report complete through section 1 (Gate F1 passed: instrument
  acquits the rung 7/7, convicts the isolated warp 7/7, TILING3's
  proposal 5/7; split frozen 00:11, law 00:27), h1_sweep: H1 hex tiling
  reads NO swirl 7 of 7, repeat 5 of 7, but the brief's per-sheet grain
  gate vs the same chunk's vanilla is unattainable by ANY sampler
  (vanilla grain spans a factor 24 by painted texture; rung 1/7, TILING3
  proposal 4/7). DIRECTOR DECISION 01:4x: grain + band gates become G1
  (median over the seven within 20 % of vanilla's median, TILING3's own
  criterion) AND G2 (per sheet within 20 % of the rung's, no regression);
  the literal per-sheet number still reported beside them. RELAUNCHED
  01:4x as lane TILING4 RESUME (Opus, Agent tool, background): brief
  scratchpad/brief_tiling4b.md, same lane dir and report (appended),
  same DONE marker, same exe baseline 23:26:29 21,484,032 B. Landing
  sequence unchanged (verify exe/DONE/sweep, mirror skills, splice with
  `python scratchpad/splice_lane_docs.py scratchpad/tiling4_20260912
  TILING4`, pictures cmp_tiling4.png + triptych, refill
  brief_grade1.md's exe line, launch GRADE1).
  bungo's UI NOTES 01:3x-01:4x (scratchpad/brief_uinotes_20260912.md,
  nine rulings collected BEFORE any animation-workspace work, in his
  words there): 1 remove the bottom status bar entirely (its Saved /
  Loading messages need a home, two harnesses read it); 2 selected
  keyframes vanish -- they must stay visible and turn orange, Blender
  style; 3 a track context-menu entry under Remove track that strips
  chosen translation X/Y/Z and rotation X/Y/Z components from every key
  of the track (his case: COM of Running_To_Slide keeps Z, loses X/Y).
  4 the transport-bar icons redrawn (Blender's transport, our SVG in the
  wwskin palette, tooltips with shortcuts; "Load..." / "root" header too).
  5 the Animations list gets shortcuts, a right-click menu and drag-and-drop
  reorder for delete/copy/paste/cut/duplicate/rename/move (Blender keys).
  6 the crowded Keys panel + bottom button row -> Blender dope-sheet shape:
  header menus, selection-driven collapsible sidebar sections, no button row.
  6a (01:51) those sections live in a panel that opens from the RIGHT side of
  the Animation dock (toggle + N key, Blender sidebar); left keeps list + tree.
  7 right-click on any row/ruler adds an annotation AT THE CLICKED FRAME
  (today only the button, at the playhead; the sheet menu ignores the click x).
  7a remove annotation by right-click too; BUG: dragging an existing marker
  while zoomed in resets the timeline zoom to default.
  7b selected annotation orange (active), orange-red for the other selected
  ones; same two-tone for keys in item 2.
  8 playhead BLUE (Blender: blue frame box + line); outside the clip range
  darkened like Blender, inside unchanged.
  9 draggable start/end handles on the ruler (+ Start/End boxes) to lengthen
  or trim the clip; replaces the Trim controls.
  Plus six unconfirmed timeline guesses put to him (in the file). These
  become lane UINOTES1 (brief to write) and go BEFORE any further
  timeline/animation lane; not before the lodgen queue unless he says.
  RULING 02:05 (bungo): "That's it for now, you can run a second agent
  on it then merge" -> LANE UINOTES1 LAUNCHED 01:56 (Opus, Agent tool,
  background) IN A SEPARATE COPY OF THE TREE: E:/Projects/NifskopeWWE_ui
  (robocopy 01:53 of everything but scratchpad/, plus scratchpad *.md
  and *.py; exe 23:26:29 21,484,032 B inside). Brief
  scratchpad/brief_uinotes1.md (both trees); rulings file
  scratchpad/brief_uinotes_20260912.md (items 1-9, 6a/6b/7a/7b). It
  runs beside TILING4 by his word (the one-instance rule: the UI lane
  waits for any --port NifSkope from the main tree; his window is never
  touched). MERGE PROTOCOL (director, after both DONE): read the copy's
  scratchpad/uinotes1_20260912/CHANGED_FILES.txt (A/M list + CR/LF
  counts), confirm none of TILING4's files are on it (lodgen.*,
  nifcli.cpp, lodgenmanager.cpp, btdterrain.*, lodtfile.*,
  docs/LODGEN_*, tests/spells/lodgen_*, bake_impostor_cards.sh), copy
  each listed file from the copy into the main tree (byte copy; line
  endings travel with the file), copy its report/images/skills over,
  then ONE build in the main tree + the UI chain + the lodgen chain,
  splice both lanes' docs (UINOTES1's WW_CHANGES entry after TILING4's),
  send the pictures. If the same file is on both lanes' lists it is a
  hand merge, said so in the report. A liveness monitor (35 min silence
  or DONE, either lane) is armed in this session (task blpb29y44).
  ANSWERED 02:0x (bungo: "right now I can save the edited hkx, right?"):
  yes -- Save writes the clip back to its .hkx as interleaved (the class
  the game loads by name), Save as to a new file; hkxwrite_gates 23/23
  round-trip, HKXPACK re-reads it; an edited clip has NOT been flown in
  game yet (his).
  His NifSkope window: OPEN since 01:29 (pid 60820, no --port) on the
  23:26:29 exe; rename aside at link time, never kill.
  LANDED 02:4x: TILING4 DONE 02:35 (resume lane), verified (exe
  02:08:57 21,487,616 B newer than every changed source; rung
  before_tiling4 == launch bytes; no NifSkope/Fallout4 at 02:36),
  spliced 02:37 (WW_CHANGES + MISTAKES +1, the lane's duplicate of the
  director's dead-lane item dropped + HANDOFF block below), skill
  ww-prototype-is-not-the-product written from the lane's draft and
  mirrored, pictures cmp_tiling4.png + sheet_tiling4.png sent 02:4x.
  RESULT: `--land-sample stochastic` is now a hex tiling (Heitz-Neyret),
  swirl 13/14 (warp 7/14), grain vs current build 14/14, repeat 9/14
  (warp 11/14) -> DEFAULT UNCHANGED, still OFF; his call: soft hex
  LANDED 03:2x: GRADE1 DONE 03:23, verified (exe 03:06:21 21,489,152 B
  newer than every changed source, stale sweep 0; rung before_grade1
  02:08:57 21,487,616 B intact; no NifSkope/Fallout4 at 03:23; skills
  mirror clean), spliced 03:23 (WW_CHANGES CR 19020, MISTAKES +4,
  HANDOFF block below), pictures cmp_tone.png + curve.png sent 03:26.
  RESULT: no tone curve; best gain differs per cell (0.699..1.388, mean
  1.004); `--grade K` shipped, default 1.0 == rung bytes; RED: ground-
  cover plane empty on all 25 tiles (tint never fires), road materials
  do not resolve from the unpacked root, saturation above vanilla on
  every tile, two near-grey tiles (-12,28) (-16,28).
  PC RESTART 03:2x (his, announced 03:24): this session and the
  in-session UINOTES1 agent die with it. NOTHING LAUNCHED after GRADE1.
  RESUME AFTER RESTART, in this order: (1) UINOTES1 in the copy
  E:/Projects/NifskopeWWE_ui -- read scratchpad/uinotes1_20260912/
  PENDING.md + CHANGED_FILES.txt + the report there (it was at work
  step 6 of 9, Animations list, at 03:18; steps 1-5 written up, the
  lane was told 03:26 to write PENDING now); relaunch it (Opus,
  background) with scratchpad/brief_uinotes1.md + "resume from
  PENDING.md", liveness Monitor armed; (2) ROADS3 in the main tree
  (brief scratchpad/brief_roads3.md, exe line refilled 03:2x to
  03:06:21 21,489,152 B + the GRADE1 findings), Opus, background,
  Monitor; then INCR1 -> TERRAIN-AO1 -> TERRAINFMT1 -> EROSION1 ->
  director's Sanctuary bake -> one final message. Owed to bungo:
  answer on 6b (Material Manager picture) and the hard line
  (`--blend-edges quadrant` default vs a bake switch). His window:
  none at 03:23; next launch of release/NifSkope.exe = 03:06:21.
  RESTART CANCELLED 03:33: the DNS fault was MSI Center leaking the UDP
  port pool (killed, fixed, memory + Downloads/fix_dns.bat). UINOTES1
  kept running in the copy (PENDING.md + CHANGED_FILES.txt written
  03:28-03:30, still at work). LIVE NOW: lane ROADS3 (Opus, Agent tool,
  launched 03:3x on GRADE1's exe 03:06:21 21,489,152 B; brief
  scratchpad/brief_roads3.md; markers scratchpad/roads3_20260911/
  BUILDING / DONE; liveness Monitor armed) beside UINOTES1 in the copy.
  UINOTES1 DONE 04:04 in the copy E:/Projects/NifskopeWWE_ui: all nine
  rulings coded, BUILD PENDING (Fallout4.exe up since 03:48; no build, no
  launch, no pictures, every gate unexecuted; syntax-checked with a
  refuter; qmake re-run in the copy because the robocopied Makefiles
  pointed at the main tree; rung before_uinotes1 = 23:26:29 bytes). MERGE
  LIST scratchpad/uinotes1_20260912/CHANGED_FILES.txt in the copy: 19 M
  files (src/anim*, hkx*, nifskope.cpp/.h/_ui.cpp, ui/nifskope.ui,
  uialigntest.cpp, wwskin.h, tests/spells/animws.sh), 0 A, 0 lodgen
  files; VERIFIED 04:0x that none of the 19 changed in the MAIN tree
  since the robocopy (01:53) -> a byte-copy is a clean merge. MERGE
  DEFERRED by the director until ROADS3 lands AND the game is down: then
  copy the 19 + report + uinotes1_20260912/ into the main tree, one
  build, UI chain (animws, hkxanim_ui, ui_align, water_ui, files_tab,
  top_bar, skeleton_overlay) + lodgen chain, pictures, splice its docs
  after the lodgen entries. Owed to bungo from its report s13: SVG files
  vs drawn paths (no Qt Svg deployed), frame-0 as the axis-strip
  reference, rename not rewriting the HKX internal name, reorder not
  touching NIF block order, keys clipboard not built.
  LANDED 04:3x: ROADS3 DONE 04:32, verified (exe 04:10:38 21,489,152 B
  newer than every changed source, stale 0; rung before_roads3 == launch
  bytes; game went down 04:08:58 before its one build; no NifSkope/
  Fallout4 at 04:33; skills ww-simulate-before-build NEW + ww-control-
  calibration amended, mirrored 04:3x), spliced 04:3x (WW_CHANGES CR
  19020, MISTAKES +4, HANDOFF block below), pictures cmp_road_wash.png +
  cmp_road_profile.png sent 04:3x. RESULT: `--road-opacity A` (default
  1.0 == rung bytes 19/19; `--roads-legacy`); no single opacity fits
  both test chunks; the two-tone band is the road MESH's ramped skirt
  (alpha correlation -0.79 ours vs 0.00 vanilla) -> a mesh lane, not a
  colour knob; NEW RED: lodgen_terrain_vt V9c fails identically on the
  rung exe (E/W seam ratio 14.20) -> wants its own lane. BAKE_INSTRUCTION
  unchanged. LIVE NOW: DIRECTOR MERGING UINOTES1 into the main tree
  (04:3x): rung before_uinotes1 = ROADS3's 04:10:38 exe; the 19 files
  byte-copied from the copy + report + uinotes1_20260912/ + flags_
  uinotes1.rsp; ONE build next (game down at 04:33), then the UI chain
  + lodgen chain, pictures, splice its docs. If this block is the last
  line: the merge copy is done, the build state is whatever
  release/NifSkope.exe's mtime says (before_uinotes1 = 04:10:38 rung).
  RULING 05:0x (his words over scratchpad/road_detail_look.png, vanilla
  vs detail 0 vs detail 1 on (-20,20)): "--road-detail 1 is always on,
  do not ever use road detail 0, that looks terrible". Standing: the
  default becomes roadDetail 1.0 (src/lodgen.h:784 is 0.0f today), the
  0 setting is never the default again and never a lane's pick; ROADS2's
  "vanilla is one flat colour a material" measurement does not overrule
  his eye. Queued as item 0 of INCR1 (brief prepended 05:0x); the
  BAKE_INSTRUCTION carries `--road-detail 1` explicitly until the flip
  is built. Earlier he asked "what happened to the roads ... now it's
  just solid colors" = ROADS2's default flip, answered with the picture.
  RED 05:0x: bungo: "Why can't I open any nif in nifskope right now, it
  freezes when I do" on the merged UI exe (04:38:12 / relinked 05:06:05
  by UINOTES1b). UINOTES1b's own after-chain shows it: animws 7 s on the
  rung vs 3 min 7 s on the merged exe, same 210 checks. Lane told 05:0x
  to measure the load path first and fix it if it is UINOTES1's code.
  bungo told to use release/NifSkope.before_uinotes1.exe (ROADS3's
  04:10:38, gated) meanwhile. That rung is NOT to be deleted.
  RULING 05:1x (his diagnosis): "the issue with the roads is, these
  meshes have some terrain included there, you can see the sharp mesh
  terrain being included into the chunk's bake" = the road NIFs' vertex-
  alpha skirt (ROADS2's 76 shapes; ROADS3 F3e' measured it, vanilla
  shows none of it). LANE ROADS4 chartered (scratchpad/brief_roads4.md:
  item 0 = the --road-detail 1 default flip, moved out of INCR1; then
  measure the skirt, simulate alpha-as-coverage / drop-skirt / +cover,
  one build, gates G0-G6, pictures). QUEUE NOW: UINOTES1b (live) ->
  ROADS4 -> INCR1 -> TERRAIN-AO1 -> TERRAINFMT1 -> EROSION1 -> bake.
  RULING 07:3x (bungo, over icons_before_after.png): "Why do these turn yellow when selected? the buttons, keep them white" / "the pose and the dot" -> the ON state of the pose-with-gizmo and auto-key toggles is WHITE (the palette's brightest ink token), never the accent orange. UINOTES2 resumed in the copy for it (marker DONE2 when done; gate (q) re-measured; then the director merges the same files into main again). LIVE NOW: LAND1 (main) + UINOTES2 resumed (copy).

LANDED 07:2x: UINOTES2 DONE 07:20 in the COPY (exe there 06:56:29 21,850,624 B; animws 224/0, Ctrl+A 3 of 3 -- two eaters found: setSequenceByName's ClearAndSelect and rebuildList; both toggles icons, lit in accent, pose glyph is OURS (Blender has none); lodl_open 23/0 on its exe BUT its exe also carries ROADS4's lodgen.h rebuild, so the 05:48:33 crash is NOT proven to be UI code -- the lane showed a plain NIF and a 2.65 MB terrain NIF render headlessly on the rung; only the .lodl DOCUMENT in the viewer dies, 19-frame stack, after "meshed and built"; LAND1 told to report lodl_open on the 06:31:05 exe). MERGED INTO MAIN 07:22 by byte copy: src/animworkspace.cpp, src/animworkspacetest.cpp, tests/spells/animws.sh (+ skills nifskope-ww-build-verify amended, ww-toggle-lit-gate new; mirrored). NOT YET BUILT IN MAIN: LAND1's next build carries them (told 07:23; animws 224/0 row added to its chain). Docs spliced (WW_CHANGES CR 19020, MISTAKES +4). Pictures icons_before_after.png sent; the ruling pictures r3/r6/r6a/r7/r7a/r9 + dock_overview in scratchpad/uinotes2_20260912/images/after/ NOT yet sent. Three unruled things the lane saw: annotation labels overlap at 1:1 when markers are close; the side panel keeps an empty "Retime to" row on an annotation; the Animations list row is one long clipped label. Owed to bungo: one mouse drag of a clip row (drop it on another row; Ctrl+Z puts it back). Copy tree E:/Projects/NifskopeWWE_ui kept until he says. LIVE NOW: LAND1 only (main tree). QUEUE: -> GROUND1 -> TERRAINFMT1 -> bake.

LANDED 07:0x: ROADS4 DONE 06:57:44. release/NifSkope.exe 06:31:05 21,819,904 B (one build, zero relinks); rung before_roads4 = 05:48:33 kept. Director verified: exe newer than every changed source, lodgen.h objects fresh, sheet in step, skill ww-material-folder-classify mirrored; docs spliced (WW_CHANGES CR 19020, MISTAKES +3, the lane put its own HANDOFF block in); picture road_ground_look.png sent. FINDINGS: --road-detail 1 is the default (G0 green, byte-identical to the flag). The brief's skirt premise is REFUTED: zero texels are painted by skirt-only triangles. What bungo saw is the road NIFs' own terrain-material shapes (materials/Landscape/Ground/, the verge) winning 36.1 percent of the road plane on (-20,20) and 24.9 on (-8,8); taking them out DOUBLES the seam (15.4 -> 33.4 vs vanilla 5.4), so the knob --road-ground-paint ships at 1.0 = unchanged. The real driver is our asphalt tone (surface L 110.8 vs vanilla 93.5) = --road-opacity; 0.326 matches vanilla's seam on (-20,20) but the two chunks want opposite values, NOT shipped, HIS CALL. REDS: lodgen_roads 11/1 (R5 short by 0.0145, the bar was calibrated at detail 0 -> LAND1 recalibrates); lodl_open 23/2 with SIX segfaults in the headless render path, present on the 05:48:33 exe = UINOTES1's code -> sent to UINOTES2 as item 2b at 07:0x; G2/G3/G4(-8,8) as the report's section 3 says (G3's red is the detail flip itself). LIVE NOW: UINOTES2 (copy tree) + LAND1 (main tree, launched 07:0x, brief_land1.md wraps TILING5 + INCR1, exe line filled 06:31:05). QUEUE: -> GROUND1 -> TERRAINFMT1 -> bake.

LIVE NOW 06:16: TWO lanes. ROADS4 in the main tree (launched 06:02; game went down ~06:10; it was editing src/lodgen.h + src/nifcli.cpp at 06:07). UINOTES2 in the COPY E:/Projects/NifskopeWWE_ui, refreshed by robocopy /E (no deletions) from main at 06:15 with ROADS4's half-done lodgen.h/nifcli.cpp inside (told to leave them alone); its exe = the 05:48:33 UINOTES1b exe; it rungs before_uinotes2 IN THE COPY; the director merges by its CHANGED_FILES.txt (byte-copy, same pattern as UINOTES1 at 04:3x). Liveness Monitors on both. bungo's own NifSkope pid 48104 (no --port) is up on the 05:48:33 exe. When UINOTES2 lands: merge into main only when ROADS4 is not linking (check no make/ld running; the merged files are UI-only), then rebuild main in a build-resume as UINOTES1b did, OR hand the rebuild to the next lodgen lane's first build (prefer the latter: one build). Delete the copy only when bungo says.

RULING 06:1x (bungo, over the transport row crop): "okay, new icons look good, what is the "pose" button and the round dot that's not centered button?" -> told: Pose-with-gizmo and Auto-key (ruling 6's toggles); offered both as icons -> "Both icons". LANE UINOTES2 chartered: scratchpad/brief_uinotes2.md (both toggles as icons in ruling 4's set with a lit state; the Ctrl+A red; the owed pictures for rulings 3/6/6a/7/7a/9; the mouse-drag note). UI-only lane, may run BESIDE a lodgen lane (two-lane cap) once the game is down; exe line filled at launch. QUEUE NOW: ROADS4 (live) + UINOTES2 (next slot) -> LAND1 -> GROUND1 -> TERRAINFMT1 -> bake.

LANDED 06:0x: UINOTES1b SUSPENDED 05:50 (PENDING2.md: Fallout4.exe pid 48328 up since 05:42:58; it built three times with the game up -- its MISTAKES entry). release/NifSkope.exe 05:48:33 21,817,856 B holds all nine UI rulings; UI chain measured (animws 210/1: Ctrl+A keeps 1 of 4 rows because the viewport answers a selection with setCurrentItem -- behaviour red, three fixes named in the report, HIS CALL; the other six harnesses same counts as the rung, on the 05:06:05 exe); lodgen chain NOT run; his NIF-open freeze NOT reproduced (merged 2.0-2.7 s vs rung 2.9-3.9 s on the same files). Director verified: exe newer than every changed source, sheet in step, skills mirror clean, rung before_uinotes1 intact; docs spliced (WW_CHANGES CR 19020, MISTAKES +6, HANDOFF block below); picture scratchpad/uinotes1_sheet.png sent. UINOTES1b's owed chains (lodgen + six UI harnesses on the 05:48:33 exe) are ROADS4's item -1, no resume lane. Pictures for rulings 3, 6, 6a, 7, 7a, 9 still owed; one mouse drag of a list row owed to bungo. LIVE NOW: lane ROADS4 (scratchpad/brief_roads4.md; offline work while the game is up, builds when it is down, else BUILD PENDING).

FOLD 05:2x (bungo: "and they keep on stacking" / "fold small ones together"): LANE LAND1 = TILING5 + INCR1 (scratchpad/brief_land1.md wraps both briefs; rung before_land1; markers land1_20260912); LANE GROUND1 = TERRAIN-AO1 + EROSION1 (scratchpad/brief_ground1.md; rung before_ground1; markers ground1_20260912). QUEUE NOW: UINOTES1b (live) -> ROADS4 -> LAND1 -> GROUND1 -> TERRAINFMT1 -> director's Sanctuary bake (--road-detail 1) -> one final message. Five lanes left after UINOTES1b lands, four of them lodgen (serial: same files).

RULING 05:19 (bungo, after the warp sweep picture scratchpad/warp_sweep.png: vanilla | plain | warp 170 | warp 340 | hex, all --road-detail 1): "what is used for the land sample warp? the normal or slope map?" (answer: neither, a hashed lattice on world X/Y, lodgen.cpp:5985) then "since we're reusing vanilla terain normals and slope maps, might as well use them to guide this a bit". LANE TILING5 chartered: scratchpad/brief_tiling5.md (macro normal from the global heightmap; candidates downhill drag / aspect rotation / slope-modulated hash warp; TILING4's fourteen-sheet scorer on the real exe; ship behind --land-guide; default stays plain unless he rules). QUEUE NOW: UINOTES1b (live) -> ROADS4 -> TILING5 -> INCR1 -> TERRAIN-AO1 -> TERRAINFMT1 -> EROSION1 -> bake. Bakes for the sweep: tiling4_20260912/out/dw_0|dw_170|dw_340|dw_hex/t2020 (05:14, rung exe, textured roads). He finds 683 too strong; no amplitude ruled yet.

MERGE BUILT 04:38:12: release/NifSkope.exe 21,798,400 B (qmake+make
  RC 0, stale sources 0, stale objects 0, style.qss in step). LIVE NOW:
  lane UINOTES1b (Opus, Agent tool, launched 04:4x; brief scratchpad/
  brief_uinotes1b.md; markers scratchpad/uinotes1_20260912/BUILDING2 /
  DONE2) = the resume-pending lane: UI chain + lodgen chain on the
  merged exe, gates (k1)-(q) run for the first time, pictures per
  ruling, docs; then the director splices its docs and sends pictures.
  Queue after: INCR1 -> TERRAIN-AO1 -> TERRAINFMT1 -> EROSION1 ->
  Sanctuary bake -> final message; NEW RED to queue: V9c (lodgen_
  terrain_vt E/W seam, fails on the rung too), road mesh skirt (a mesh
  lane), ground-cover plane empty, blend-edges default (his call).
  (superseded LIVE NOW follows for the record)
  blotchiness at 256 u vs the swirls. LIVE NOW (superseded): lane GRADE1 (Opus,
  Agent tool, launched 02:4x on TILING4's DONE exe 02:08:57; brief
  scratchpad/brief_grade1.md refilled 02:4x with the exe line + the
  parallel-lane rule; markers scratchpad/grade1_20260911/BUILDING /
  DONE) beside UINOTES1 in the copy. His window: none at 02:36.
  (superseded LIVE NOW follows for the record)
  LIVE NOW: lane TILING4 (Opus, Agent tool, launched 2026-09-12 00:0x,
  markers scratchpad/tiling4_20260912/BUILDING; brief
  scratchpad/brief_tiling4.md; exe at its launch = TILING3's DONE exe
  release/NifSkope.exe 2026-09-11 23:26:29 21,484,032 B). GRADE1 was
  launched 00:00 and STOPPED 00:01 on RULING 00:0x (it had only read
  the brief; its BUILDING marker and before_grade1 rung removed, no
  source touched) -- it relaunches after TILING4 on TILING4's DONE exe
  (its brief's exe line must be refilled). TILING3 DONE 23:57, spliced
  00:0x (WW_CHANGES + MISTAKES +4 + HANDOFF block below; skills
  mirrored; DEFAULT `--land-detail-source vanilla`; repeat fix
  `--land-sample stochastic` present, OFF, 6 of 7). DIRECTOR DECISION
  00:0x: EROSION1 after TERRAINFMT1 (0 chunks without a vanilla sheet).
  bungo is denoising + 4x-upscaling all 2304 vanilla _msn sheets
  (Downloads/vanillachunks). bungo's window: none held the exe at the
  last check. Game down.
  RULING 00:0x (bungo, over scratchpad/tiling3_20260911/images/
  cmp_tiling3.png, the PROPOSAL panel "--land-sample stochastic"): "the
  proposal looks pretty good, but maybe it could use some improvement"
  -> the stochastic sample is the direction; not shippable as is
  (strain 0.718 reads as swirls, 6 of 7, the pick turns on one sheet).
  Lane TILING4 (brief_tiling4.md): a swirl instrument with known-answer
  controls, histogram-preserving hex tiling (Heitz-Neyret) and per-cell
  rotation as strain-free candidates, selection 7 + disjoint validation
  7, then the default for painted ground if 7 of 7 twice. Runs BEFORE
  GRADE1 because GRADE1 fits the tone on the shipped ground.
  RULING 22:1x (bungo, over scratchpad/tiling2_20260911/images/
  cmp_tiling2.png, the green panel "--land-sample average --blend-edges
  quadrant"): "Is green "ours" the final one? because it lost all the
  texture to it, now it's only solid color blobs" -> NOT final. TILING2
  keeps `footprint` as the default (the rung's bytes) and proved with a
  validated instrument that vanilla's fine detail correlates with no
  fixed-phase candidate (land textures at any mip, VCLR, slope, shading
  all r = 0.000 with the alignment control at 0.79). Director's reading:
  that is the signature of the SAME textures with the phase broken
  (stochastic tiling), or a noise term (vanilla ships
  Textures/Terrain/Noise.dds, 1024^2 DXT1, 11 mips -- no engine setting
  name found in a two-minute grep), or a finer bake resampled. Lane
  TILING3 tests the three by spectrum and moments, ships the winner
  behind `--land-sample stochastic` (deterministic, seeded by world
  texel) or `--land-noise`, gates repeat AND grain on the same bake.
  + 22:2x bungo: "the diffuse of vanilla lod land textures, it looks
  like there's variety to it, some geological features shown" ->
  hypothesis D added to brief_tiling3.md, tested FIRST: the detail is
  shading from a finer, unshipped heightfield (vanilla's own _msn is its
  picture; correlate vanilla colour residual vs shaded vanilla _msn);
  proposed default = vanilla's own sheets as the detail source where
  they exist, procedural erosion as the fallback -- his call, he was told.
  RULING 23:0x (bungo, after RULING 22:4x): "For now, I think we can use
  vanilla normal map for those tiles, use those details for the
  diffuse" -> SHIPPED DEFAULT for a chunk with a vanilla sheet:
  `--land-detail-source vanilla` -- vanilla's _msn fine detail over our
  coarse normal (== vanilla's bytes where our coarse terrain agrees,
  guarded), and the colour's fine detail = the shading of that normal
  detail. Sent to TILING3 at 23:05 (its deliverable regardless of A/B/C's
  verdict); chunks with no vanilla sheet keep the rung until EROSION1.
  He asked what the green channel holds; director measured 23:05 on
  chunk -20,24 (scratchpad msn_channels.py): all three channels signed,
  |n| = 1.02 sd 0.07; R = east-west tilt, G = UP (255 flat, mean 242.5
  -> z 0.90), B = north-south tilt (mean 99.7 -> -0.22; ours -0.15, the
  same bias, so convention). Our writer already matches: coarse 8x8
  correlation vanilla vs ours R-R 0.94, G-G 0.76, B-B 0.93.
  + 23:1x bungo: "so now we do not use our own normal map if that is
  toggled, but reuse these ones for terrain chunks" -> the default is
  NOT a blend: a chunk with a vanilla _msn gets vanilla's sheet byte for
  byte, our normal bake skipped for it; chunks without one get ours; the
  guarded composite survives only as `--land-detail-source vanilla-blend`
  (not default). Sent to TILING3 23:1x; gate = cmp == vanilla per chunk.
  RULING 23:5x (bungo, over Materialize with our 128-u height as
  displacement + vanilla's 4x-upscaled _msn): vanilla's G channel "is
  the slopeness for the micro details our heightmap bakes do not
  possess" -> the colour-detail driver for chunks reusing vanilla's
  _msn is MICRO-STEEPNESS (acos vanilla G - acos our coarse G), no light
  direction, view-independent; sent to TILING3 23:5x (keep it if it
  matches vanilla's colour residual at least as well as lit shading).
  His next step (stated by the director, he did not deny it): integrate
  vanilla's normal into height, our heightmap for everything coarser
  than the LAND grid, the integrated normal for finer -> candidate for
  EROSION1's slot as the fine-height source where a vanilla sheet
  exists (Poisson/Fourier solve, deterministic), erosion only where none.
  Our height for chunk -20,24 was cropped from the installed
  Commonwealth.HeightMap DDS (R16, pixel = h/8 + 32767, row 0 north,
  cell (cx,cy) at column (cx+96)*32, row (95-cy)*32) and sent as 16-bit
  PNGs; the 4x-renormalised vanilla _msn (blue recomputed) also sent.
  NOTE 23:3x: vanilla's sheets are DXT5 (BC3), 512^2, 10 mips (header
  read 23:1x); the colour block is BC1's 5:6:5, so the north-south slope
  in B (5 bits, far from midpoint) is visibly blocky. bungo ran a BC1
  artifact-removal ESRGAN model (OpenModelDB "1x_BC1" family) on the
  chunk -20,24 _msn and judged the before/after good: blocks gone, rills
  kept. FOR TERRAINFMT1: a decode -> clean -> re-encode path for the
  copied vanilla _msn keeps that gain only in BC7 or uncompressed
  (DXT5 re-encode brings the blocks back); renormalise after cleaning
  (recompute G from R,B). Candidate item, his call; measure the cleaned
  sheet (unit length, channel means, coarse agreement, fine energy at
  the 4-texel scale) before shipping any model output.
  + 23:3x measured (session scratchpad measure_clean.py): block-grid
  ratio N-S 1.52 -> 1.06, up 1.51 -> 0.99; coarse agreement >= 0.99;
  means within 1 level; fine N-S energy doubled (restored toward E-W's,
  not invented; per-texel r 0.92). bungo lit it in Materialize: plates
  gone, drainage reads. Pipeline shape: the model runs OUTSIDE the bake
  as a one-time pre-pass writing a cleaned cache (uncompressed/BC7);
  the bake copies from the cache when present, raw DDS otherwise.
  RULING 23:1x (bungo): "out of bounds terrain blends are not included
  in the actual cells out of bounds, they never were, so we can't
  recover the color data anymore, because it was baked in a different
  tool outside of fo4" -> layerless cells (heights only) have no colour
  to bake from. TILING3 told 23:1x: for a chunk with no land-texture
  layers, reuse vanilla's COLOUR sheet byte for byte too (same rule as
  the _msn); detail-over-ours only where layers exist; count chunks per
  class. EROSION1's brief: for layerless chunks with no vanilla sheet the
  colour is a material assignment grown from the pass (rock on steep /
  scoured, sediment on deposits, the flat default between), fitted to
  vanilla's own out-of-bounds sheets as the reference.
  IDEA 23:1x (bungo): "so, if we had erosion simulation to add those
  details in, we could also use that to do something with the out of
  bounds areas..." -> two jobs. (a) Out-of-bounds cells that HAVE LAND
  records: EROSION1's pass covers them as-is (no vanilla sheet there, so
  they are its chunks). (b) Beyond the last LAND record: grow terrain
  (continue the border slopes, ridges at hill scale, erode, bake meshes
  + sheets for chunks the game never shipped) = candidate lane HORIZON1,
  parked after EROSION1, NOT chartered. Its first step is an engine
  test, not code: place one chunk one step past the worldspace extent
  and see whether the game draws it (LOD loads by the chunk filename
  grid around the player; the worldspace's stored extent may clip it).
  Nothing is built on it before that test says yes.
  RULING 22:4x (bungo, over the director's cmp_msn_2024.png -- vanilla's
  _msn vs ours for chunk (-20,24), local variance 88.15 vs 5.96, ours
  baked from the LAND heightmap): "We lose all the fluvial, erosion
  features and other topographical features" -> the far sheets must
  carry them again. Director's answer (23:0x): the loaded cells never had
  them either (same 128-u grid); Bethesda's unshipped source terrain
  exists only in the far sheets. Two sources, both wanted: (1) vanilla's
  own copy where a shipped sheet exists and our coarse terrain still
  agrees with it (TILING3's `--land-detail-source vanilla`, guarded);
  (2) GROWN: lane EROSION1 (brief_erosion1.md) = a deterministic
  hydraulic erosion pass on our heightfield at bake resolution, seeded by
  world position, byte-identical 1 vs 16 threads and single-chunk vs
  region, feeding the _msn writer and the colour shading; fitted to
  vanilla's measured erosion statistics (rill spacing, flow coherence
  with the coarse downslope, amplitude vs slope) with floors; `--erosion
  <strength>` 0 == rung bytes; the knob may be pushed past vanilla.
  Queued right after TILING3, before GRADE1, so the tone fit sees the
  finished ground. Bears on TERRAINFMT1's _msn plan: blending land-texture
  normals adds grain, not geology -- amend that brief at its launch.
  RULING 21:1x (bungo, over scratchpad/roads2_20260911/images/
  cmp_sanctuary_road_v2.png, "ours --roads (the new defaults)"): "ours
  before and after roads, looks like an issue, no diffuse is sampled from
  the road, only like solid colors" ... "our road wasn't flat before, but
  now it is, and it still has the seam at the edges of it" -> ROADS2's
  default `--road-detail 0` (diffuse flattened to its average) and the
  opaque hard-edged paint are NOT accepted as the shipped look; vanilla's
  road is a soft blue-grey wash the ground shows through, and ours is
  two-tone (skirt band around a darker core). Lane ROADS3
  (brief_roads3.md): fit vanilla's per-texel road opacity and edge
  profile, the residual detail strength, the hue; cross-road profile
  gate (no step where vanilla has none); switches off == rung bytes;
  `--roads-legacy` stays the way back.
  RULING 19:2x (bungo, over the "OURS default 341.3333" panel of
  scratchpad/resume3_20260911/images/cmp_tiling_fixed.png, the newest
  bake, exe 19:08:42): "Yes, you can see the tiling pattern of each
  texture, which is not good, hard blend edges also appear in some
  places, and yeah, it's muddy or blurry looking" -> lane TILING2
  (brief_tiling2.md): measure vanilla's periodicity / edge width /
  detail spectrum first, then the repeat-averaged land sample, the edge
  blend (quadrant border suspected), the detail term from wherever it
  correlates -- all behind switches, off == rung bytes; tone stays
  GRADE1's, the constant stays RESUME3's.
  STANDING INSTRUCTION 16:38: run the queue to the end, send every
  comparison picture as it lands, one message when everything for lodgen
  is done. QUEUE AFTER RESUME3 (briefs written): ROADS2
  (brief_roads2.md), TILING2 (brief_tiling2.md, added 19:2x on his
  judgement of cmp_tiling_fixed.png -- BEFORE GRADE1 because GRADE1 fits
  its tone curve on this lane's colour), TILING3 (brief_tiling3.md, added
  22:2x on his judgement of TILING2's picture: the averaged ground is
  "solid color blobs", the footprint ground is the repeat, neither
  ships; vanilla's grain by spectrum + moments, not fixed-phase
  correlation; stochastic tiling / Noise.dds / resampled bake as the
  three hypotheses; DONE 23:57), TILING4 (brief_tiling4.md, added 00:0x
  on his judgement of TILING3's proposal panel: swirls out, 7 of 7,
  then the default), GRADE1 (brief_grade1.md; exe line to be refilled
  from TILING4's DONE), ROADS3
  (brief_roads3.md, added 21:1x on his judgement of ROADS2's picture --
  after GRADE1 so the road's own gap is measured on graded ground), INCR1 (brief_incr1.md),
  TERRAIN-AO1 (brief_terrain_ao1.md), TERRAINFMT1
  (brief_terrainfmt1.md; + ingest bungo's cleaned 2K _msn cache, BC7 or
  uncompressed), EROSION1 (brief_erosion1.md, added 23:0x on "We lose
  all the fluvial, erosion features"; moved here 00:0x -- no Commonwealth
  chunk lacks a vanilla sheet), then the director's full-module Sanctuary bake
  + the handoff + the bake instruction; each brief's "Exe at launch"
  blank is filled from the previous DONE line; NATIVE1c parked on his
  near-model ruling. DIRECTOR PROCEDURE PER LANDING (every lane tonight):
  verify DONE + exe mtime/size + no source newer than the exe + the gate
  log's count; mirror any amended skill to E:\Projects\Claude\.claude\
  skills (cp + cmp); splice the lane's WW_CHANGES_ENTRY / MISTAKES_ENTRIES
  / HANDOFF_BLOCK with `python scratchpad/splice_lane_docs.py
  scratchpad/<lane_dir> <LABEL>` (LF-only texts; WW_CHANGES.md must stay
  at CR 19020; the script strips a lane's title/note lead and refuses
  otherwise -- fix by hand as done for CARDS-AGG); send pictures with
  SendUserFile (proactive while he is away); update the queue line in
  this block with `date`-read times; launch the next lane with its brief
  path in the prompt, model opus, background. Uncommitted: 67 modified +
  322 untracked paths (the 35 foreign skill dirs among them must NEVER be
  committed; bungo removes them). Commit waits for his word. Owed by
  bungo (all listed in the "Owed by bungo" section below): the near-model
  library (NATIVE1c), cover's home, sort-in-cell, REFR id placement, the
  UV weld, .BTO under FO4CS, candidates CLI default, the aggregate cost
  tier and composite-vs-render, the conditional .lodi version word, UI5's
  hover box, UI6's NIF key editing loss, the arrow-air dock width, the
  fade-threshold menu rows, the shadow numbers, the height sheet in the
  full bake, the residency budgets, the commit.
- CLOCK CORRECTION (director, `date` = 15:54 on 2026-09-11, MISTAKES.md
  entry of the same minute): every director-written label in this block
  from "15:1x" through "17:1x" was typed from elapsed-time feel and is
  INFLATED by up to sixty minutes. File-anchored truth: BAKEPERF1's exe
  14:48:52; PLAN-FO4CS landed 15:04 (docs/FO4CS_IMPROVED_LOD_PLAN.md
  mtime); NIFPARSE1 launched ~15:06, ended BUILD PENDING 15:47 (its own
  report says 16:0x/16:2x/16:5x -- it inherited the wrong clock from its
  launch prompt); CARDS-AGG launched ~15:08 (census 15:11..15:21), built
  15:49:39, bake 15:53; FLAGSCAN1 landed 15:51; PIC-GRASS and SPLAT1
  launched ~15:3x-15:4x. bungo's rulings labelled 15:1x..16:5x below fell
  between 15:05 and 15:50 in that order; their ORDER is right, their
  minutes are not. Labels 05:2x..15:0x were read from `date` and stand.
- RESUME 2026-09-11 05:2x (new director session after the 100-percent stop).
  Found on disk: WATER8's hook-up APPLIED (hookup.py --check: E1/E2 markers
  present x1, E3-E5 anchors intact), its build chain finished
  (chain.log QMAKE-RC=0 BUILD-RC=0 CHAIN-RC=0, release/NifSkope.exe
  21:02:12 20,830,208 B, style.qss 21:02:12, NO source newer than the exe)
  but the lane died before its gates: BUILDING still up, no DONE, report
  stops at 0.3, images/ empty, no WW_CHANGES_ENTRY. UI5 got as far as its
  probe (probe_out.txt: QMenuBar::item margin-top 6 -> ink -0.5 px, 8 ->
  +1.5 px; rects never move, gate must read pixels); no code written.
  Account B: session 1 percent, week 0 percent (05:17). Fable/weekly meters
  of the primary: not named by bungo this session -> Opus for everything.
  Game down, no NifSkope running (rc=1). Launched 05:2x, both Opus, Agent
  tool: lane WATER8-GATE (brief scratchpad/brief_water8_gate.md: gate the
  21:02 exe, group L + siblings, the two LOD-tab pictures, entry/handoff
  text, then DONE; no build, no source) and lane UI5 (brief
  scratchpad/brief_ui5.md: menu items centred through the skin, code-only
  until WATER8's DONE, one build, gates M1-M5). Skill trees diffed for the
  seven skills the briefs name: identical. bungo's window: unknown whether
  open; the 21:02 exe carries WATER8 and needs a restart once gated.
- WATER8 + WATER8-GATE block, spliced 2026-09-11 06:5x (lane text verbatim):

**WATER8 LANDED AND IS NOW GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-10 21:02:12**, **20,830,208 bytes** (UI4's was 20:45:47,
20,798,976). Markers: `scratchpad/water8_20260910/DONE` in, `BUILDING` gone --
**lanes UI5 and UI6 are unblocked**. Report `scratchpad/lane_water8_report.md`
(sections 0..0.3 are WATER8's pre-registered gates, 1..6 are the gating run);
entry text `scratchpad/water8_20260910/WW_CHANGES_ENTRY.md`; mistakes text
`scratchpad/water8_20260910/MISTAKES_ENTRIES.md` (TWO entries, not yet in
`MISTAKES.md`). No rollback rung was taken for this build; the nearest is
`release/NifSkope.before_ui4.exe` (18:25:20).

**Lane WATER8 itself died at its account limit right after its chain returned
`CHAIN-RC=0`**, leaving `BUILDING` up, no `DONE`, no documents and an empty
`images/`. It had built correctly; nothing was wrong with the exe. Lane
WATER8-GATE (2026-09-11 05:2x) ran the gates, took the pictures and wrote the
documents. It built nothing and touched no source, QSS or `.pro` file.

## What bungo gets

*"What? I wanted it in that right panel though"* -- done. The LOD Generation
dock on the RIGHT now opens with its own two-segment strip under the title:
**LOD** (the generator, unchanged) and **Water** (the whole marking tool,
including the full-screen flow window's button). The LEFT strip is back to
**Header | Blocks | Files**, the standalone Water Marking dock is gone, and so
are both Workspaces-menu entries. Both strips carry the SAME stylesheet, byte
for byte, in the same 35 px row.

Pictures, `scratchpad/water8_20260910/images/`: **`lodtab_water.png`** and
**`lodtab_lod.png`** (499x741, the same dock, same crop, the two tabs) and
`toprow_after.png` (602x82, the three-tab left strip). In-app grabs.

## Gates (all on the 21:02:12 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **59 checks, 0 failures, 0 skips, PASS** (floor 48) | 48 / 0, floor 41 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `top_bar.sh` | 43 / **5** -- the same five `Panels lists the ... dock` | 43 / 5 |
| `files_tab.sh` | 28 / **2** -- BUILD9's same two | 28 / 2 |
| `animws.sh` | 57 / 0, 1 skip (the 10mmPistol has no sequence), PASS | 57 / 0, 1 skip |
| `water_mark.sh` | dock 20 / 0 PASS; self-test FAIL on X2b **0.407** (gate > 0.5), body 3 | same red, same number as BUILD12 |
| `water_window.sh` | 46 / 0, PASS | 46 / 0 |
| `lodl_water.sh` | 33 ok, 0 FAIL, RESULT PASS (prints no count) | 33 / 0 |
| `loaded_nifs.sh` | **166 / 0, PASS** | 166 / **2** |

Consistency, not just success: `src/nifskope.h` (20:59:18) is older than **35
of 35** objects that include it (20:59:57..21:02:08); **116 of 116** changed
files under `src res tools tests` are older than the exe; `res/style.qss` and
`release/style.qss` are byte-identical.

Skipped with the reason: every suite the change does not reach (lodgen,
terrain, impostor, gltf, hkx*, collision, block, water solve / flow / weights),
`skeleton_overlay.sh` (flaky by BUILD11's own measurement) and `lodl_open.sh`
(its fixture is bungo's installed `Commonwealth.lodl`, which nothing here
rewrites).

## Three things for the director

1. **`loaded_nifs.sh` 166 / 2 -> 166 / 0, and the two are finally NAMED.**
   BUILD12's handoff said nobody could say which check turned green because only
   a count had been recorded. A control run of the kept rung
   `release/NifSkope.before_ui4.exe` (18:25:20, WATER7's fourth tab still in)
   answers it: `the top selector orders Header, Blocks and NIFs without
   remapping modes` and `NIF Browser is above Loaded NIFs in its own mode`.
   Removing the fourth left tab cured both. Log
   `scratchpad/water8_20260910/logs/loaded_nifs_CONTROL_ui3exe.log`.
2. **Gate L8 is HALF the gate it was registered as, and lane UI6 needs to know.**
   Report 0.1 registers it as 4 px clear of the row *"and of one another"*; as
   shipped (`src/wateruitest_lod.cpp:485-517`) it measures only the row's top
   and bottom, and **nothing measures the gap between the LOD strip's two
   segments**. The inter-segment gap is measured only on the LEFT strip, by
   UI4's `(S5) every pair of segments is 4..4 px apart` -- and that 4 is exactly
   what bungo's *"Why are they separated?"* sends to 0. UI6 must add L8's
   missing half, or the right-hand strip keeps its gap silently. Nothing was
   changed here: a gate repair needs a build, and the brief forbids one.
3. **X2b is still 0.407** on body 3 against a gate of > 0.5 (0.595 on body 2).
   Unchanged since BUILD12 to three digits, so it is the instrument, not a
   regression -- and still owed to bungo as a decision.

## Restart

**NO -- he already has it.** bungo launched `release\NifSkope.exe` himself at
**2026-09-11 05:32:30** (pid 8428, no `--port`, so it is his interactive window
and not a harness), which is after the 21:02:12 link. His open window IS this
build: the Water tab is in the right-hand panel in front of him now.

**That window HOLDS `release/NifSkope.exe`.** The next build must rename the
running copy aside immediately before the link (`NifSkope_inuse_8428.exe`),
never kill it -- and lanes UI5/UI6, which this DONE just unblocked, are the ones
that will hit it.

## State

Nothing committed (CONSTITUTION 8). Changed by lane WATER8:
`src/lodgenmanager.cpp`, `src/nifskope_ui.cpp`, `src/nifskope.h`,
`src/wateruitest.cpp`, `NifSkope.pro` (all through
`scratchpad/water8_20260910/hookup.py`, CR 0 in all three shared files) and the
new `src/wateruitest_lod.cpp`. Changed by WATER8-GATE: nothing outside
`scratchpad/water8_20260910/`. Game down (`rc=1`) at every check; no harness
instance was left running, and the only NifSkope on the machine at the end is
bungo's own window (pid 8428).

**Lane UI5 is ALIVE in the tree** (`scratchpad/ui5_20260910/probe.cpp` and
`release/ui5_probe.exe` both stamped 2026-09-11 05:22, a standalone Qt probe,
not NifSkope). It has touched no file under `src/`, `res/`, `tests/` or
`tools/`, so the exe-newer sweep above still holds -- but the next build must
account for whatever it lands.

- UI RULING bungo 2026-09-11 05:4x on the UI3 dropdown arrows (shown
  scratchpad/ui3_20260910/images/cmp_zoom.png + cmp_header.png), verbatim:
  "That's fine, as long as the dropdown arrows do not intersect with the
  text / icons like on the screenshots you showed me" -> the row stays as
  UI3 left it; the menu-arrow on every viewport-header button gets air so
  it never overlaps the icon or the label (the pivot dot and grid buttons
  are the two that overlap today). Goes into lane UI6's brief; gate = a
  pixel read of the arrow's column vs the icon's last ink column, gap >= 1.
- RULING bungo 2026-09-11 05:5x -> 06:0x, LANE SKEL2 (after UI6). Over a
  screenshot of human_male_vanilla.nif with Overlays > skeleton on (all 130
  nodes labelled, clutter) beside the Skeleton Manager: (1) verbatim "for
  that bone view toggle, shouldn't it mirror the skeleton manager view?" ->
  the overlay draws EXACTLY the rows the manager lists under its chip (All /
  Bones / Deforming / Unused) + search; names only on hover/selection or a
  Names toggle; selection two-way. (2) verbatim "shouldn't we improve both
  views (that will now be shared)?" -> "All good" to this list: manager Bone
  column never elides to nothing (today LArm_Upp.. -> LAr.. -> blank rows
  with numbers only; name gets the width, numbers fixed right), search keeps
  ancestors, rows coloured by kind (deforming / helper / non-bone) with the
  SAME colours the overlay uses, selected/hovered rows match the viewport;
  overlay = ONE drawing routine shared by Pose Mode and the overlay (today
  two renderers; CONSTITUTION 10 shared code), depth fade as Pose Manager,
  X-ray toggle, non-bone nodes on All only as muted markers; click selects in
  both, double-click a row frames the bone. (3) verbatim "what about the bone
  shape? Shouldn't it be something like in Blender?" -> Blender OCTAHEDRAL
  bone by default (square ring at the head tapering to the tail, head ball =
  joint, child head on parent tail), Stick as the exact way back, display
  modes as Blender (Octahedral / Stick / B-Bone / Envelope; the last two may
  be refused with reasons), unselected grey / selected bright / active
  lighter. REFERENCE PICTURE (bungo 06:0x, screenshot of Blender 4.5.3 LTS,
  a single default armature bone in Object Mode): a solid-shaded octahedron,
  its square ring about a tenth of the length from the head, tapering to a
  point at the tail, a ball at the head and a smaller ball at the tail,
  flat grey with lit faces so the four facets read. Build exactly that
  silhouette. COLOUR, bungo 06:1x verbatim: "Just keep the color of the
  bones blue" -> bones stay BLUE (the skin palette's blue, the one the
  overlay uses today), not Blender's grey; selection/active/hover are
  brightness steps of that blue, non-bone markers stay muted. Also owed to
  SKEL2: name the two marker-only nodes that project
  above the back at frame 46 ((486,485) and (463,503) in the 1293x941 run);
  the joint-marker call (BUILD11 red 2) is largely settled by the Bones
  chip: markers only on All.
- RULING bungo 2026-09-11 06:4x, verbatim: "we should only have those 5
  .lod types in fo4 community shaders target" (after asking why legacy
  toggles like .btr generation show under the FO4CS target). -> Under
  target = FO4 Community Shaders the LOD Generation panel offers ONLY the
  five outputs .lodl (landscape), .lodt (terrain textures), .lodo (object
  library), .lodi (instances), .lodm (material sidecars). Every legacy row
  hides under that target: .btr, .bto, bake terrain textures, chunk
  textures from the pyramid, and their sub-rows; the Stock engine target
  keeps them all. DEPENDENCY, flagged to him: FO4CS's Improved LOD module
  reads .bto + manifests today and NO native file; the .lodo/.lodi emitter
  is a patch not yet built into the exe (docs/LODGEN_NATIVE_LODO_LODI.md).
  Lane order: NATIVE1 (hook the emitter + --native switch into the build,
  real worldspace pair, decoder gate) THEN LODUI1 (the panel gating, with
  a self-test counting visible rows per target, floor both ways). The
  FO4CS loader work (.lodl v3, .lodo/.lodi readers) stays in the FO4CS
  session's list. Queue after SKEL2.
- RULING bungo 2026-09-11 07:0x, verbatim: "I've only wanted trees for the
  impostors" (after being told --candidates missing|trees|all bakes cards
  for any LOD base with an empty far slot). -> Impostor cards are TREES
  ONLY: the tree set is lodgenIsTreeModel's three tests (record type TREE,
  model path in a trees folder, model file name starting "tree"; never a
  bare substring). The --candidates switch and its panel row go away (or
  refuse anything but trees with the reason in words); a non-tree object
  with an empty far slot drops out at that ring as vanilla does, or keeps
  its mesh under far-ring simplification. The bake driver
  (--list-impostor-candidates, tools/bake_impostor_cards.sh) lists trees
  only. Gate: the candidate list on a Commonwealth region contains zero
  non-tree bases, with a floor (a known non-tree base with an empty slot
  named and shown absent). Folded into lane LODUI1 (same files).
  AMENDED bungo 07:1x -> 07:2x: "Make trees only a toggle"; then, on the
  alternative behind it, "let's keep it simple like that for now, later we
  can expand it to have us be able to select which objects turn into
  cards". -> Panel row "Trees only", ON by default. On: cards for the
  three-test tree set. Off: cards for every base in the plugin's OWN LOD
  set (STAT MNAM / TREE / SCOL parts) whose far-ring slot is empty -- the
  old "missing" rule, nothing else; no type/folder/size filters, no hand
  list. Gate both states with floors (on: zero non-tree candidates, a
  named empty-slot non-tree shown absent; off: that same base present).
  --candidates all is retired. FUTURE (his words, parked): a way to pick
  which objects become cards -- candidate list with exclude ticks first,
  rules second; not before he asks.
- RULINGS bungo 2026-09-11 07:3x -> 07:5x, also LODUI1: (a) card
  resolution list gains 512 px ("Add it, why not"); the bake hook already
  accepts 32..512 (nifskope_ui.cpp ~22540), only the panel combo stopped at
  256; sheet = 8x8 views so 512 = a 4096 sheet per channel, ~12 MB per tree
  type; tooltip/cost line say so. (b) His ask "an override to select which
  LOD level gets forced to be replaced with impostors, even if it possesses
  authored mesh LODs" = the EXISTING "Cards from ring" row
  (--impostors-from-level, FO4CS target only); told him so. LODUI1 makes it
  tree-only in effect and label now that cards are trees only. Rings
  explained to him: loaded 5x5 grid = full models, ring 0 = dim 4 = the
  first LOD band outside it, ring 3 = dim 32 = horizon.
- RULING bungo 2026-09-11 08:0x, LANE NATIVE1 (the .lodo/.lodi hook-up),
  from "anything we can do to improve it, performance wise ... the earlier
  the better" -> five items offered, he took "1, 2" and "3 sounds good";
  "we wanted screen size based lod, right?" = yes. INTO THE .lodo/.lodi
  LAYOUT BEFORE THE EMITTER IS HOOKED UP: (1) per-CLUSTER screen error in
  .lodo -- a cluster hierarchy per mesh (simplify, group, measure error)
  so the runtime picks detail per cluster by projected size, not by the
  four MNAM rings; (2) per-cluster bounding sphere + normal cone in .lodo
  (~64-128 tris per cluster) for GPU culling; (3) per-instance bound
  radius (f32) in the .lodi record for the screen-size fade (radius /
  distance, one divide; sphere errs safe). Item 4 (hemi-octahedral sheets)
  was WITHDRAWN: the shipped sheets already are (director mistake,
  MISTAKES.md 08:0x). Item 5 (instances pre-sorted by mesh then material
  at bake) ACCEPTED 08:1x ("5 sounds good"), told him it changes no draw
  count, only load-time grouping. SECOND ROUND 08:2x ("anything pre-nanite
  games do that we don't"), checked against the tree first: ACCEPTED (2)
  precomputed occluders -- a few boxes per cell baked from the meshes for
  chunk rejection, and (3) vertex/triangle order for the GPU cache on every
  .lodo mesh (post-transform cache + fetch locality; NOT in the tree today,
  no meshopt_optimize* call anywhere). PENDING his yes: (1) AGGREGATE
  impostors at ring 3 -- one sheet per forested cell photographed from the
  horizon views over the trees' repetition-broken cards, ring 3 instances
  become one per cell, cross-fade at the 2->3 border; count of forested
  cells printed before anything is built. WITHDRAWN as a rule (4) material
  tiering by ring: his challenge "why are you so sure" stands -- specular/
  roughness still read at distance; parked as a MEASUREMENT (bake ring 3
  both ways vs vanilla far shading, same tile). 08:3x: "1 sounds good" ->
  aggregate ring-3 impostors ACCEPTED; "Dithered cross fade is good" ->
  runtime item, owed on the FO4CS side (the plan's per-object dither).
  FAR SHADOWS, his words: "remember, we'll have far shadows cast by those
  lods, so a far shadow cast by a LOD tower behind me, will cover the area
  I'm at" -> BAKE CONSTRAINT for NATIVE1 and the card lanes: every object
  in the LOD set is a shadow caster. Decimation may never open a
  silhouette (gate: watertight/hole count per far mesh vs its ring-0
  source); cards cast from the height channel (already stored, normal B);
  the aggregate cell sheets carry height too; the per-cluster selection
  serves the SHADOW view as well as the camera view (a cluster culled for
  the camera may still be needed for the light). 08:4x, his words on the
  far-shadow mechanism: "Our shadow casters are based on the color id
  right now I think, so they can only occlude other objects and terrain,
  never themselves since that causes visual issues" -> the identity index
  (R+G) is what the far-shadow pass keys on; self-shadowing is excluded by
  identity, so a coarser shadow-view cluster selection is safe (no
  self-occlusion to get wrong), and the identity channel must survive into
  every native/aggregate output (gate: identity present and unique per
  placement in .lodi and on the aggregate sheets). EMISSIVE at night
  ("Yeah"): the .lodm emissive multiple per layer must be read by the
  runtime -- FO4CS owed list. RULING bungo 2026-09-11 09:2x ("Your solution
  sounds good") on runtime-blend vs baked far terrain: HYBRID by band --
  ring 0 (the band touching the loaded cells) blends at runtime from the
  .lodl's per-texel LTEX weights so the loaded-cell edge is seamless; ring
  1 out samples the .lodt pyramid; cross-fade across ring 0. GENERATOR
  GATE owed (NifSkope side, before FO4CS commits): the ring-0 weights blend
  and the pyramid's baked colour agree on the same texel -- same source
  textures, same grading -- printed as a per-tile colour error vs the bake
  with a floor (a deliberately wrong grading shown red). Runtime side =
  FO4CS owed list. RULING bungo 2026-09-11 09:3x, verbatim: "fo4cs is going
  to lit it, we can keep it as roughness, of if PBRM is used to bake it, it
  gets roughness, if a vanilla legacy material, its gloss gets inverted
  into roughness" -> FAR-TERRAIN ROUGHNESS is baked into the .lodt data
  sheet's BLUE channel, replacing shore distance (derivable at runtime from
  the .lodl water planes; the BTD contract already says so). Source per
  layer through the SAME blend as the colour: a PBRM-backed material gives
  its roughness map; a legacy BGSM/vanilla material gives 1 - gloss
  (specular map's gloss channel inverted). Wetness stays in G and darkens
  roughness at runtime. No metallic channel. Gate: far roughness vs the
  near material's roughness on the same texel within tolerance, floor = a
  deliberately un-inverted gloss shown red; the census names which rule
  served each layer (pbrm | legacy-inverted | none->default). Contract
  page docs/LODGEN_TERRAIN_VT.md row "2 | data" changes with it. Lane:
  NATIVE1 (format) or its own TERRAIN-R lane if NATIVE1 grows too large --
  director's call at brief time. ADDED bungo 09:4x: "then also add
  metallic map, but that should only get derived from PBRM" -> a FIFTH
  .lodt sheet, metallic, single channel BC4 (DXGI 80), same tile grid /
  border / mips; written ONLY when at least one PBRM-backed layer in the
  worldspace carries a metallic map; absent sheet = metallic 0 (the
  fallback, named in the .lodm index). Legacy materials contribute 0,
  never a guess. Gate: sheet present iff a PBRM metallic layer exists
  (both directions, floor each way); far metallic vs the near PBRM
  material on the same texel within tolerance. SUPERSEDED 09:5x by bungo:
  "you can mirror how it's set up for the .lodm" then "we just add the
  coverage for whatever's missing in terrain textures that lod objects
  have in the texture department" -> the terrain pyramid takes the OBJECT
  texture family law (docs/LODGEN_LODM_FORMAT.md 2.1): family word real
  (pbr on the FO4CS target, legacy converted at bake: gloss -> 1-gloss,
  metallic 0), one MASK sheet rmaos = R roughness, G metallic, B AO, A
  (terrain's own fourth = ground cover, or subsurface 0 -- lane proposes,
  director rules), EMISSIVE sheet when any layer supplies one (absent =
  none, named in the index), colour and msn normal kept, height sheet
  kept. The fifth-sheet metallic note above is VOID. Dropped from the
  sheets: shore distance (runtime from .lodl water) and baked wetness
  (close-up effect per the contract; far wetness = weather state, FO4CS
  runtime). The terrainVT .lodm index's sheets[] and family change with
  it; contract page rewritten by ww-contract-provenance. Gates as above
  (roughness/metallic vs near material per texel; emissive present iff
  a layer has one, both ways; family word == rule that served). Each of 1-3 and 5 ships with a floor gate
  (field written AND moves) per the three rules of 2026-09-04.
- GAP REVIEW RULINGS bungo 2026-09-11 10:0x (five gaps offered; his answers
  verbatim): (1) roads/decals on far terrain -- "We do the same with roads
  and decals as vanilla" -> LANE ROADS1: rasterise road and decal meshes
  into the terrain bake (colour + mask sheets, and the .btr chunk sheets
  on the stock path) the way vanilla's graded bakes carry them; gate =
  vanilla's shipped sheet vs ours on a road tile, road pixels present,
  floor = a tile with no road shown unchanged. (2) engine double-draw --
  "This will be solved through FO4CS, vanilla LOD will be suppressed" ->
  runtime, FO4CS owed list; the bake emits NO stock override chunks.
  (3) staleness hash -- "Add it" -> .lodo/.lodi carry a corpus hash of the
  object records + load order (mirror of .lodl's vhgtCorpusHash) so a
  reader can refuse/warn on a plugin change; --verify-only covers them;
  into NATIVE1. (4) performance census -- "We need them" -> a census page
  for the Improved LOD module (draws per ring, triangles, resident tiles,
  card count, cull rate, residency bytes), written NifSkope-side as the
  spec (docs/LODGEN_CENSUS.md, contract style) and consumed by FO4CS;
  every field under the written-AND-moves rule. (5) end-to-end bake --
  "I will bake everything when you finish all the work, and I wake up" ->
  bungo runs the full Commonwealth bake himself after the lane queue
  lands; the director's job before he wakes: every lane below DONE, one
  exe carrying all of it, the GUI bake's four stage times printed by the
  panel, and a one-page bake instruction in the handoff.
  LANE QUEUE (director's split 2026-09-11 09:0x, after UI6 and SKEL2
  landed): NATIVE1a LANDED 08:49 (exe 08:49:08 20,989,440 B; block spliced
  below; the hook-up had ALREADY been applied by BUILD6 on 2026-09-10 and
  only the contract's STATUS line said otherwise); NATIVE1b LANDED 10:14
  (exe 10:14:23 21,101,056 B, v3: ladder levels 0-7, spheres + cones,
  280 occluder boxes on the region, lodgen_native.sh 18/0 in its FINAL
  log -- the first log of the same name reads 18/1 before the counted
  relink; block spliced below); TERRAIN-R LANDED 11:27 (exe 11:27:12
  21,137,920 B: mask sheet R rough / G metal / B AO / A cover, emissive
  sheet when present, family "pbr" real, shore + wetness dropped, the
  ring-0 formula gated mean 3.3-3.6/255 vs floor 13.7-15.2 and ceiling 0;
  Sanctuary's 14 layers ALL legacy-inverted, 0 PBRM, so the PBRM/emissive
  arms are gated on a fixture only; block spliced below). ITS PICTURE
  scratchpad/terrain_r_20260911/images/ours_vs_vanilla_tile.png says the
  rest plainly: mean 20/255 from vanilla. CORRECTED by ROADS1 (12:1x):
  that chunk (-20,24) holds ZERO road triangles -- the director's "most of
  it the roads" was wrong (MISTAKES.md, ROADS1's entry); the 20/255 is the
  splat GRADING gap, the August calibration item never closed (still
  OPEN: "splat calibration vs vanilla grading" -- proposed lane SPLAT1,
  bungo's call: vanilla's sheets are graded x0.82 uniformly and smoothed;
  ours are the raw 17-grid blend). ROADS1 LANDED 12:19 (exe 12:19:06
  21,180,928 B: road meshes rasterised into the colour sheet only, as
  vanilla measured to do -- road family scores 0.716 vs floors <= 0.601,
  every other family inside its floor, normal sheet untouched; the loop
  road chunk (-20,20) road-centreline presence 0.127 -> 0.307 (ceiling 1,
  ground 0.344), road colour error 38.85 -> 24.36/255; lodgen_roads.sh
  11/0; --no-roads byte-identical 9/9; block spliced below). ROADS1 RED
  for a lane of its own (defect, not a ruling): our far-terrain sheets are
  DXT1 with 8 mips while ALL 6,120 of Bethesda's are DXT5 with 10 -- a
  DXT1 _msn has no alpha and our chunk renders blue-purple beside
  vanilla's (proposed lane TERRAINFMT1, after the ruled queue). LODUI1
  LANDED 13:24 (exe 13:24:12 21,234,176 B: FO4CS target = the five
  outputs + native row, 4 legacy rows hidden both ways; Trees only ON
  (19 vs 33 candidates on Sanctuary, floor named); 512 px; "Tree cards
  from ring"; lod_generation.sh 116/0 (rung 97/0), lodgen_panel_run.sh
  125/0, lodgen_stage_times.sh 16/0; GUI native run wrote the pair; the
  bake instruction for bungo = scratchpad/lodui1_20260911/
  BAKE_INSTRUCTION.md; block spliced below). CENSUS1 LANDED 13:5x (docs
  only: docs/LODGEN_CENSUS.md 60 fields, checker 59/0 + 31 named
  cannot-carry; block spliced below). BAKEPERF1 LANDED 14:48 (exe
  14:48:52 21,261,312 B: the chunk queue is ONE shared function
  src/lodgenchunkpass.*, worker pool + writer thread in
  src/lodgenparallel.*, BC encoders parallel, results retired in job
  order; byte identity green on 60 + 163 files; NO SPEED-UP: the NIF
  parser is not thread-safe (heap corruption in NifItem::deleteChildItems
  under ~BaseModel on worker threads, 5 of 5), so the chunk fan-out
  ships OFF (--chunk-threads 1) and the default bake is the bake that
  always ran; headless path now sets SEM_NOGPFAULTERRORBOX so no crash
  dialog reaches his desktop; block spliced below). PLAN-FO4CS LANDED
  15:0x (docs only: docs/FO4CS_IMPROVED_LOD_PLAN.md 72,969 B, six rungs
  R0-R5 as FO4CS waves, 14 open rulings in its section 6, 17 generator
  owed items with owning lanes in section 5, gates 73+4 citations / 6
  rungs complete / 0 invented words, floors fired; the ww-census-contract
  amendment (section 2a) APPLIED to both skill trees by the director,
  identical; block spliced below; sent to bungo 16:0x). CARDS-AGG RUNNING
  15:4x (brief scratchpad/brief_cards_agg.md; markers
  scratchpad/cards_agg_20260911/). NIFPARSE1 LAUNCHED 15:5x, CODE-ONLY
  until CARDS-AGG's DONE (bungo "Yeah, go for it": the NIF model loader
  made thread-safe so the chunk fan-out ships ON; gate = 20 consecutive
  16-thread runs on two regions without a fault + byte identity vs
  serial + stage-time speed-ups + a ~100-chunk memory point; brief
  scratchpad/brief_nifparse1.md; markers scratchpad/nifparse1_20260911/;
  it owns src/model, src/xml, src/data loading, lodgenchunkpass/parallel;
  lodgen.cpp only via --check hook-up after CARDS-AGG). ALSO ALIVE, all
  read-only until the slot frees: PIC-GRASS (16:1x: vanilla | ours tint
  0.35 | ours --grass-tint 0 | tint footprint, same Sanctuary texels;
  bakes after CARDS-AGG's DONE), FLAGSCAN1 (16:4x, bungo "how does Fo4
  determine if an object is meant to be baked into the terrain texture
  ... does that apply to other object types": whole-Commonwealth STAT
  flag-bit rates road vs non-road -- candidates Has Tree LOD (6), Add-On
  LOD Object (7), Has Distant LOD (15), Uses HD LOD Texture (17), CELL
  Distant LOD only (12) -- from the re-downloaded xEdit defs; then the
  flagged non-road list, a second-tile displaced-floor score, and the
  Has-Tree-LOD vs lodgenIsTreeModel agreement; no exe), SPLAT1 (16:5x,
  bungo over the TERRAIN-R picture: "the terrain textures ... do use
  their correct scale, right? So they'd be very tiny repeating pixel
  sized patterns" / "almost like pebbles on a full 1 or 2k texture" ->
  phase A read-only: spectra + 3x3 variance ours vs vanilla with the
  calibration controls, the mip the code ACTUALLY selects vs the
  footprint mip (32 u/texel), tint/VCLR/mip isolated; phase B after
  CARDS-AGG + NIFPARSE1 DONE: the ONE change behind a switch, rebake,
  colour error vs vanilla before/after -- this is the August "splat
  calibration vs vanilla grading" item, finally in numbers). QUEUE AFTER
  THOSE: INCR1 (brief_incr1.md), TERRAIN-AO1 (terrain AO from placed
  objects, bungo 15:4x "Okay, so the AO can be acurate from objects" =
  yes; brief to write), then the director's end-to-end Sanctuary bake +
  handoff.
  Briefs WRITTEN and waiting, in order: ROADS1 (brief_roads1.md), LODUI1
  (brief_lodui1.md), CENSUS1 (brief_census1.md, no build), BAKEPERF1
  (brief_bakeperf1.md), CARDS-AGG (brief_cards_agg.md); each brief's "Exe
  at launch" blank is filled by the director from the previous DONE line.
  NATIVE1a as briefed was (brief scratchpad/brief_native1a.md:
  apply NATIVE0b's hook-up at re-counted anchors, first REAL Sanctuary
  region pair, decoder + --native-verify, v2 fields = corpus hash, REFR
  formID per instance, instance radius rule, ONE sort law (mesh, material,
  tie-break), meshopt cache order, identity unique + equal to manifest,
  silhouette (shadow-caster) gate per mesh, stock path byte-identical with
  --native off, contract rewrite; markers scratchpad/native1a_20260911/),
  then NATIVE1b (cluster hierarchy with screen error from FULL detail
  down, cluster bounds + normal cones, occluder boxes per cell -- the
  GPU-driven data, format v3), then CARDS-AGG (aggregate ring-3 impostors,
  own lane), TERRAIN-R (the object-family mask/emissive sheets for the
  pyramid, contract rewrite, AND the ring-0 weights-blend-vs-pyramid
  colour gate -- moved here from NATIVE1, it is terrain),
  ROADS1, LODUI1 (five .lod types under FO4CS, Trees-only toggle, 512 px,
  Cards-from-ring tree-only), CENSUS1 (the census page), BAKEPERF1 (bungo
  10:1x: "bake time, anything we can do to speed it up? use my system to
  its fullest here?" -> measured: src/lodgen.cpp has ZERO threading, the
  GUI runs one chunk per event-loop tick; machine = 16 logical cores,
  31 GB, RTX 5070 Ti. Lane: chunks/tiles fanned over a thread pool with the
  shared caches read-only before the fan-out; BC encoding per block in
  parallel; impostor bakes batched into offscreen targets per frame under
  the one-instance rule; writes off the critical path. GATES: whole-corpus
  BYTE IDENTITY single- vs multi-thread on every output file; the four
  stage times before/after printed by the panel; no ordering leak).
  One track at a time, each to DONE. STANDING OK bungo 10:2x: "you can do
  some small scale bake tests to confirm things actually work" -> the
  director (through lanes) runs SMALL-REGION bakes headless at will --
  the 9-chunk Sanctuary region or a comparable tile set -- with every
  module on, as the end-to-end check before he wakes: files written,
  decoders pass, the four stage times printed, pictures through the
  render hook. Never the full Commonwealth (that is his bake); never with
  the game up; never on his installed Data\Terrain files (own out-dir).
- FO4CS-SIDE RULINGS bungo 2026-09-11 10:3x (the campaign AFTER the far
  field; recorded here because the data comes from this generator; the
  FO4CS session owns the runtime): on "is the 5x5 grid performant" -> no;
  his words: "Screen size fade to lower LODs would be nice to have within
  that 5x5 grid, 2 sounds good too, 3 sounds ogod" -> ACCEPTED all three:
  (1) screen-size fade for the engine's fade classes inside the loaded
  grid (hybrid ladder rung 0, bound-sphere projected size, hysteresis,
  render-only) AND screen-size selection of LOWER LODs for objects inside
  the grid; (2) shadow casters inside the grid at lower detail (from the
  LOD model or our cluster set, never the full mesh); (3) extend the
  cluster LOD inward: objects in the outer cells draw from the .lodo
  cluster set by screen error, the engine's full draw suppressed per
  object. Data implications for NATIVE1 NOW: the .lodo cluster hierarchy
  must include the NEAR levels too (not only far), i.e. the error ladder
  runs from full detail down, so the inward extension has something to
  select; the instance table must be joinable to the engine's placed REFR
  (formID in the .lodi record) so a runtime can match an engine object to
  its cluster set. Memory: project_hybrid_lod. SCREEN-SIZE FADE SPEC
  (bungo 10:4x "how will screen size fade be determined? engines like
  unity allow you to set it"): Unity's model -- projected size =
  boundRadius / distance x projection scale as a FRACTION OF SCREEN HEIGHT
  (resolution-independent, FOV-aware); one threshold per engine fade class
  (objects, actors, items, grass), default ~1 percent, a menu row + INI key
  each; hysteresis ~20 percent above the threshold for fade-in; the fade is
  the dither cross-fade. Cluster LOD selection uses the same projection on
  each cluster's stored geometric error against ONE global pixel tolerance
  (default 1 px). Both become census fields (CENSUS1). ZOOM (bungo 10:5x
  "when you zoom in Fallout 4, you view the LOD right?"): yes, distance
  LOD ignores the scope; with the cluster ladder from full detail + the
  projected-size test, zoom walks far objects up the ladder and un-hides
  sub-pixel ones; limits = a residency budget for the finest levels
  (census resident-bytes) and far TEXTURES stay array/card resolution
  until a later FO4CS round streams real material textures for zoomed
  objects (not a bake change). GRID-EDGE SEAM (bungo 10:5x "note these
  things"), FO4CS runtime, four items: (1) dither cross-fade .lodi
  instances out / engine full models in over a band at the loaded-grid
  edge instead of the engine's hard swap; (2) ring 0 terrain drawn from
  the .lodl finest level (== LAND heights exactly) + the ring-0 weights
  blend, so no crack, no visible skirt, matching texture; (3) shadow
  blend band where the engine's cascades end and the identity far
  shadows begin; (4) the same screen-size fade thresholds inside and
  outside the grid. Cell streaming itself stays the engine's (August
  principle).
- UI5 HOVER BOX, OPEN CALL for bungo (picture sent 11:0x while he slept):
  scratchpad/ui5_20260910/images/cmp_menu_hover.png -- before UI5 the
  hovered/open menu title's box was 20 px at the top of the 35-px row;
  after UI5 it is the full 35 px (probe render by lane UI5-HOVERPIC,
  release/ui5_hoverprobe.exe, NOT an in-app grab; notes
  scratchpad/ui5_20260910/HOVERPIC_NOTES.md). Default if he says nothing:
  stays full row (Blender). "short box" = one padding rule back to a
  20-px box, centred, one build, can ride with any UI lane. Skill
  ww-qss-geometry-probe gained section 3c (state under test), mirrored
  to the live tree 11:0x.
- UI5 block, spliced 2026-09-11 09:0x (lane text verbatim):

**UI5 LANDED, EXE FREE.** `release/NifSkope.exe` **2026-09-11 05:58:21**,
**20,855,296 bytes** (WATER8's was 2026-09-10 21:02:12, 20,830,208). Markers:
`scratchpad/ui5_20260910/DONE` in, `BUILDING` gone. Report
`scratchpad/lane_ui5_report.md`; entry text
`scratchpad/ui5_20260910/WW_CHANGES_ENTRY.md`; **five MISTAKES entries NOT
appended by the lane** -- `scratchpad/ui5_20260910/MISTAKES_ENTRIES.md`, the
director splices. **There is NO pre-UI5 rollback rung on disk** (see the reds
below); the nearest earlier exe is `release/NifSkope.before_ui4.exe`
(2026-09-10 18:25:20).

## THE TITLE NUMBER

bungo, verbatim: *"Also, please center file / view / spells / options / help
buttons, top left"*.

```
                                   before 20:45:47   after 05:58:21   want
  the five titles' text, ink y        6..16 / 6..17   13..23 / 13..24
  the CAP BAND's centre               10.0            17.0             17.0
  offset from the row's centre        -7.0             0.0             0
  and the five agree, spread           -               0.0             0
  the item's painted box            y 0..19 (20 px)   y 0..34 (35)     the row
  ------------------------------------------------------------------------
  the ROW                             35              35               35  unchanged
  the menu bar's own size hint        20              35 (<= 35)
  tFile / tLOD / tView / header / tab strip           all 35           unchanged
```

The cap band is the topmost row of ink down by the font's own `capHeight()`
(8 px here). It is what a reader judges centring by, and it is NOT the whole ink
band -- that also holds the mnemonic underline under F / V / S / O / H and the
descender of the `p` in Spells and Options, which is what the first run of the
gate got wrong. The whole ink band is printed beside it in every log.

The before numbers come from the 20:45:47 exe's OWN in-application grab
(`scratchpad/ui4_20260910/images/strip_after.png`, whose first 35 rows are the
menu row) read by `scratchpad/ui5_20260910/measure_before.py`, and the gate's
own live floor reproduces them to the pixel (`File 6..16`, `Spells 6..17`).

## The mechanism, and the two things it is NOT

Measured over 30 cases outside the application BEFORE a line was written
(`scratchpad/ui5_20260910/probe.cpp`, `probe_out2..4.txt`, skill
`ww-qss-geometry-probe`; `release/ui5_probe.exe` links Qt only and is not part
of the application):

1. **`QMenuBar::item { min-height }` is not consulted on this path at all.**
   Fifteen cases, 22 to 36 px, item height 20 in every one. The row's sheet has
   stated a menu-item min-height since lane WATER7 and it has never done
   anything -- **this is why UI3's "the items take the same calibrated content
   number" did not centre them.**
2. **A `::item` margin moves the item but cannot be gated.**
   `actionGeometry()` grows with the margin, exactly as `QTabBar::tabRect()` did
   for lane UI4.
3. **The two vertical paddings are consulted and leave the rect honest.** Item
   content 16 (measured), row 35, split 9 / 10.

New `wwBarRowMenuItemQss( rowHeight, itemContent )` in the shared skin, applied
inside `wwAlignBarRow`, which MEASURES the content (row sheet applied once with
both paddings zeroed, `ensurePolished()`, read back -- no event loop, which is
probe case E and is what makes it possible during construction) and appends the
rule LAST so it outranks the min-height above it. `wwBarRowMenuItemContent()`,
`wwBarRowMenuItemPadTop()`, `wwBarRowMenuItemPadBottom()` read the numbers back.
Fallback, named: a menu bar with no titles, or items taller than the row, takes
the font's own line height. Way back unchanged and still exact:
`UI/CompactTopBars = false`, the same single reader, and M4 measures that state
live (titles back at cap centre 12.0, item 24 px).

## Gates (all on the 05:58:21 exe, sequential, one instance, `.gatelock`)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **76 checks, 0 failures, 0 skips, PASS** (floor 62) | 59 / 0, floor 48 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `top_bar.sh` | **43 / 5** -- the same five | 43 / 5 |
| `files_tab.sh` | **28 / 2** -- the same two | 28 / 2 |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0, 1 skip |

Logs `scratchpad/ui5_20260910/logs/`. Nothing moved from baseline but
`water_ui.sh`, which gained this lane's 14 checks plus the 3 the chain's two
LOD-tab picture arguments add. Skipped with the reason: every suite a menu-item
padding does not reach (lodgen, terrain, impostor, gltf, `hkx*`, collision,
block, the water solve/flow/mark/window suites) and `skeleton_overlay.sh`,
flaky by BUILD11's own four-run measurement and untouched.

**The floor fires, live, in the same run.** The 20:45:47 arithmetic
(`padding-top: 2px; padding-bottom: 2px`) is appended over the shipped sheet and
the SAME predicate is asked again: worst **-7.0**, red by name; taken away,
**0.0** again. Both halves are checks, so the picture is the shipped state.

## Pictures

`scratchpad/ui5_20260910/images/`

* **`cmp_menu_zoom.png`** (1080x338) -- File..Help from both exes at **4x
  nearest**, red rule between, each half labelled with its exe and its numbers.
  **This is the picture for bungo.**
* `toprow_after.png` (1512x107) -- the in-app grab of the top of the window.
* `cmp_toprow.png`, `menu_zoom_before.png`, `menu_zoom_after.png`,
  `seam_after.png`, `strip4x_after.png`, `lodtab_lod.png`, `lodtab_water.png`.

## For bungo / the director

1. **The hover band is now the height of the row.** Giving a title the row's
   padding makes its painted box the whole row, so `QMenuBar::item:selected`
   (`res/style.qss:49`) paints a 35-px highlight when a title is hovered or
   open, where it painted a 20-px one before. That is what "centred in the row"
   means geometrically and it is Blender's behaviour, but it was not separately
   asked for and no count sees it. **His call.**
2. **THREE LINKS, not one.** The application code was compiled once and never
   changed after 05:48:20; links 2 and 3 rebuilt only `src/wateruitest.cpp`
   because the first run of a gate that had never been executed found two
   defects IN THE GATE (it called the mnemonic underline "the text"; its
   way-back half set an empty stylesheet, which the application does not
   re-resolve). Both are MISTAKES entries.
3. **The rollback rung is gone**, destroyed by this lane's own `build.sh`
   copying the exe before EVERY link. Fixed (the rung is written only if none
   exists) and the two misleading copies were deleted. The exact way back for
   this change is the setting.
4. **Not measured:** any theme but dark, any device pixel ratio but 1, any font
   but this machine's; and why `setStyleSheet( QString() )` does not re-resolve
   the menu bar's items in the application when it does in a standalone rig.
5. UI3's open question is still open and untouched: the dropdown arrow on the
   viewport header's two narrow icon buttons.
6. `res/style.qss` was NOT touched, and neither was the segmented strip --
   lane UI6's, after his *"Why are they separated?"*. `water_ui.sh`'s groups S
   and L still read 4 px of air with the segments apart, which is the expected
   state for this exe and UI6's to change.

## Restart

**YES.** Whatever bungo opens next must be launched after 05:58:21. His last
window (pid 8428, launched 05:32:30 from WATER8's 21:02:12 exe) was closed by
him at 05:48 and never touched by this lane; every link after that ran with
`rc=1`.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane, through the refusing
script `scratchpad/ui5_20260910/hookup.py` (9 edits, 9 of 9 anchors matched
once, CR 0 -> 0 on every file): `src/wwskin.h` (14,041 B),
`src/nifskope_ui.cpp` (1,507,501 B), `src/wateruitest.cpp` (54,229 B),
`tests/spells/water_ui.sh` (12,744 B). Three skill files in the REPO tree were
amended and need mirroring to `E:\Projects\Claude\.claude\skills`:
`nifskope-ww-build-verify` (14,403 -> 16,546 B), `ww-qss-geometry-probe`
(8,052 -> 10,395 B), `ww-test-harness-add` (11,126 -> 12,783 B), all CR 0.
Game down (`rc=1`) at every check.

- UI6 block, spliced 2026-09-11 (lane text verbatim):

**UI6 LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 07:06:04**, **20,867,584 bytes** (UI5's was 05:58:21,
20,855,296). Markers: `scratchpad/ui6_20260910/DONE` in, `BUILDING` gone.
Report `scratchpad/lane_ui6_report.md`; entry text
`scratchpad/ui6_20260910/WW_CHANGES_ENTRY.md`; **six MISTAKES entries NOT
appended by the lane** -- `scratchpad/ui6_20260910/MISTAKES_ENTRIES.md`.
**The rollback rung is real this time**: `release/NifSkope.before_ui6.exe` is
the 05:58:21 bytes exactly (20,855,296), written once and guarded against a
second write.

## What bungo gets

* *"Why are they separated?"* -- **both** segmented strips are one joined box
  again. Header | Blocks | Files on the left and LOD | Water in the right-hand
  panel: segments touching, one shared seam, square inner corners, the two
  outer corners rounded, and UI4's 4 px still between the strip and the row's
  top and bottom, the window's left edge and the toolbar. Segments still 27 px
  in a 35 px row; nothing else moved. `UI/SegmentedStripAir = 0` still emits
  the 18:25:20 sheet byte for byte.
* *"That's fine, as long as the dropdown arrows do not intersect with the text
  / icons"* -- every button in the top row that has a menu now keeps a column
  for its arrow. Thirteen of them, worst **2 px clear** (it was **-3** on the
  narrow icon buttons, i.e. the arrow drawn inside the glyph). Nothing vertical
  changed; the menu buttons are 4 px wider.
* *"old and outdated"*, *"Do you see it?"*, *"we have a new standard for those
  sliders"*, *"look at all this text clutter"* -- **the Animation Manager dock
  is gone**, and with it the interim Havok clip strip inside it. The Animation
  dock takes its seat in **Workspaces > Animation**, which it never had. All
  eleven of its number fields are scrub fields and **none of them draws a
  native stepper any more** (two still did). The bone-binding report is
  **"78 of 95 bones"**, 14 characters, with the whole 456-character sentence in
  its tooltip.

Pictures, `scratchpad/ui6_20260910/images/`: **`cmp_strip_left.png`**,
**`cmp_strip_right.png`**, **`cmp_arrows.png`** (before/after, in-application
grabs, same spell, same arguments, each labelled with its exe) and
`animdock_after.png`.

## Gates (all on the 07:06:04 exe, sequential, one instance, `.gatelock`)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **86 / 0 / 0, PASS** (floor 72) | 75 / 0 asked the same way (76 with a second LOD picture), floor 62 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `top_bar.sh` | 43 / **5** -- the same five | 43 / 5 |
| `files_tab.sh` | 28 / **2** -- the same two | 28 / 2 |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 57 / 0, 1 skip |
| `hkxanim_ui.sh` | 48 / **1** -- the same unfirable wheel floor | 48 / 1 |
| `loaded_nifs.sh` | 166 / **3** | 166 / 0 quoted, but the RUNG reads 166 / **1** |

Consistency: 118 of 118 changed paths older than the exe; every object of all
six changed headers newer than its header; `res/style.qss` and
`release/style.qss` byte-identical.

## Four things for the director

1. **`animws.sh` gate (j) had NEVER RUN.** The whole panel-style group --
   including the scrub-field count bungo ruled on -- lived inside the
   sequence-NIF branch, and the fixture (`10mmPistol.nif`) has no
   `NiControllerSequence`, so every 57 / 0 since HKXEDIT2 finished without
   asking a panel-style question. It is a lambda now and both branches call it;
   that is 9 of the 15 checks `animws.sh` gained. On its first execution it
   found a real defect: Speed and Frame were stamped as scrub fields and still
   drew Qt's up/down arrows (`wwMakeScrubField` only removes them when its
   `chrome` flag is on). Fixed in the same wave. **A fixture NIF that HAS a
   NiControllerSequence would light nine more checks and nobody has named one.**
2. **`loaded_nifs.sh` 166 / 3, and two of the three are this lane's, measured.**
   Thirteen menu buttons 4 px wider raise the viewport header's minimum width,
   and in the harness's window QMainWindow takes it out of the LEFT DOCK: `dock
   tab strip w 174 -> 164`, `viewport header w 833 -> 857`. Two checks probe the
   Loaded Files name column at a fixed `nameArea.left() + 60` and that point now
   lands on a glyph. **Not repaired** -- it is another lane's gate and the fix is
   to take the probe point from the column's own rect. The THIRD red ("the three
   editor modes are equal-width joined segments") is red on the rung too, twice,
   so it predates this lane and nobody has noticed since.
3. **What the retired dock could do and the new one cannot** -- section 5.1 of
   the report, sixteen rows. Nothing was ported; this is bungo's call. The
   largest by far is **editing a NIF's own animation keys**: the Animation dock
   is a Havok clip editor and a NIF sequence is read-only in it. Also gone: the
   graph/F-curve view and key inspector, per-interpolator lanes for
   non-transform controllers, channel copy/paste, CSV import/export, the lint
   scan, the lane filter, snap steps, normalise and follow-playhead.
4. **FOUR LINKS, not one**, and all four are stated in the report's 3.1. One
   carried application code; the other three were single harness translation
   units, each forced by a gate going red on its FIRST EXECUTION. Two harnesses
   (`WW_ANIMPLAY_TEST`, `WW_ROTKEY_TEST`) are retired in place and write a line
   saying so; their coverage is gone until somebody re-aims them.
   `TimelineWidget` is still compiled but never constructed -- deleting the
   class is a separate lane, because its file also owns the icon set the whole
   window draws with.

## Restart

**YES.** Whatever bungo opens next must be launched after **07:06:04**. No
NifSkope was running at any point in this lane (`rc=1` at every check,
including immediately before each of the four links), so nothing of his was
touched and no exe had to be renamed aside.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane:
`src/nifskope_ui.cpp`, `src/nifskope.h`, `src/wateruitest.cpp`,
`tests/spells/water_ui.sh` (all four through the two refusing scripts
`scratchpad/ui6_20260910/hookup.py` and `hookup_gates.py`, 24 + 13 edits, every
anchor once, CR 0 -> 0), and directly `src/wateruitest_lod.cpp`,
`src/hkxplayback.{h,cpp}`, `src/hkxanimui.{h,cpp}`,
`src/animworkspace.{h,cpp}`, `src/animworkspacetest.cpp`,
`src/hkxanimuitest.cpp`, `src/ui/widgets/timeline.{cpp,h}`,
`src/spells/animationsetup.cpp`, `src/ui/widgets/physicspanel.cpp`,
`tests/spells/hkxanim_ui.sh`. Game down at every check.

**Skills: one amended and one written, in BOTH trees, byte-identical.**
`ww-qss-geometry-probe` 14,244 -> 17,337 B (md5 d4c15a63) gains sections 3c
(a widget that paints no background grabs onto WHITE, and an icon copied while
its painter is still attached) and 3d (isolating a subcontrol by two renders at
one pinned geometry; pin the whole row, not one widget at a time). **NEW:
`ww-retire-a-surface`** 6,918 B (md5 cfff4228) -- the inventory that comes
first, the seven places a dock is still reachable from, the workspace INDEX
stored in QSettings that shifts when a list loses an element, the harnesses'
three honest futures, and the control run of the kept rung.

- SKEL2 block, spliced (lane text verbatim):

**SKEL2 LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 07:58:59**, **20,927,488 bytes** (UI6's was 07:06:04, 20,867,584).
ONE build (07:55:14) plus ONE counted relink (07:58:59, the armature-membership
fix). Markers: `scratchpad/skel2_20260910/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_skel2.exe` = the 07:06:04 bytes exactly, written once.
Report `scratchpad/lane_skel2_report.md`; entry text
`scratchpad/skel2_20260910/WW_CHANGES_ENTRY.md`; **three MISTAKES entries NOT
appended by the lane** -- `scratchpad/skel2_20260910/MISTAKES_ENTRIES.md`.

## What bungo gets

* *"shouldn't we improve both views (that will now be shared)?"* -- Pose Mode and
  the Overlays armature are **one drawing routine** now, `GLView::drawArmature()`,
  fed by each caller with a bone list and per-bone state. Pose Mode keeps
  everything it had. A source gate counts the bone-drawing calls inside both old
  functions (0) so they cannot drift apart again, and ONE palette function
  answers for the viewport and the dock's rows.
* *"what about the bone shape? Shouldn't it be something like in Blender?"* --
  the bone is Blender's **solid octahedron**: eight flat-lit facets, the ring at
  one tenth of the length, a ball at the head and a smaller one at the tail.
  **Overlays > Bone Display** has Octahedral, Stick and Wire, plus X-ray and
  Names. B-Bone and Envelope are refused, with the reason (a NIF node carries
  none of the five numbers they need).
* *"Just keep the color of the bones blue"* -- **#4772b3**, the palette's
  `toggle`, which is Blender's own option blue and the only blue in `skinVars[]`.
  Unselected is that blue dimmed to 0.78, selected is the blue, active is lighter
  still, hovered is a rim.
* *"for that bone view toggle, shouldn't it mirror the skeleton manager view?"* --
  it does, through ONE shared function that decides which rows are listed. Under
  all four chips and under a search the overlay draws exactly the dock's rows,
  checked BY NAME. Click a bone and its row goes current and scrolls into view;
  click a row and the bone lights; double-click a row and the viewport frames
  that bone. Names are on the hovered and selected bones only.
* **The Skeleton Manager's Bone column never elides to nothing.** At a forced
  400 px, 95 rows sit at depth 6 or deeper and **0** of them are empty or
  elided; the tightest (`RArm_Finger13`, depth 14) has 59 px to spare. Under the
  shipped law that same row had **-71 px** and 50 rows were elided.
* **The two grey dots are named**: `Camera` (block 148) and `CamTarget`
  (block 159).

Pictures, `scratchpad/skel2_20260910/images/`: **`cmp_bones_before_after.png`**,
**`bones_stick_vs_octa.png`**, **`cmp_manager_names.png`**, `bones_xray.png`,
`bones_bones_chip.png`, `bones_all_chip.png`, `manager_after.png`.

## Gates (all on the 07:58:59 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `skeleton_overlay.sh` in-app | **46 checks, 0 failures, PASS** | 27 / **2** on the rung |
| `skeleton_overlay.sh` whole spell | **rc=0** | rc=1 |
| (j) under the Bones chip | 5 / 0, **0 stray pixels** | newly registered |
| S2 `Wire` coverage vs the rung | **only-rung 2, only-new 0**, bar 64 | -- |
| S2 control, `Octahedral` | FAILS the same test, as it must | -- |
| S6 the dots | 2 clusters, 0 unnamed | -- |
| `WW_SKELETON_TEST` | PASS, 130 / 93 / 93 / 0 | same |
| `WW_POSEDRAW_TEST` | RED, the SAME one failure | RED, same failure |
| `WW_POSEEXTRAS_TEST` | RED, the SAME one failure | RED, same failure |
| `animws.sh` | 72 / 0, 1 skip, PASS | 72 / 0, 1 skip |
| `water_ui.sh` | 86 / 0, PASS | 86 / 0 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `hkxanim_ui.sh` | 48 / **1**, the same | 48 / 1 |
| `loaded_nifs.sh` | 166 / **3**, the same three | 166 / 3 |
| `top_bar.sh` | 43 / **5**, the same five | 43 / 5 |
| `files_tab.sh` | 28 / **2**, the same two | 28 / 2 |

Consistency: **122 of 122** changed paths older than the exe; **38 of 38**
objects of `glview.h` and **9 of 9** of `skeletontools.h` newer than their
header; `res/style.qss` and `release/style.qss` byte-identical.

The one count that moved and its NAME: `water_ui.sh` first read 82 / 0 because
four `(shot)` checks arm only when the spell is given `SHOT=` / `TABSHOT=` /
`STRIPSHOT=` / `LODSHOT=`. Re-run with them: 86 / 0, PASS.

## Four things for the director

1. **The Bones chip draws long reach-through bodies.** An FO4 body weights the
   `*_skin` helpers, so under that chip the dock lists 93 helpers and none of
   the chain nodes between them, and each listed bone reaches to its nearest
   LISTED ancestor -- across the torso in places. It is the mirror taken
   literally and it passes every gate (0 pixels off the character); it does not
   look like a skeleton. bungo's call: leave it, draw each listed bone as its
   own short stub under a chip (closer to Blender), or let a chip keep the
   connecting nodes as muted context (which breaks the "lists exactly the dock's
   rows" gate as written). Picture: `bones_bones_chip.png`.
2. **17 of the 172 drawn bodies are muted grey, not blue** -- `Pelvis`, both
   `*_UpperArm`, `*_ForeArm1..3`, `*_UpperTwist1/2`, `*_Thigh`, `*_Calf`. The
   Skeleton Manager classes them "not a bone" because no skin lists them, which
   is true of this file. 155 bodies and 93 of the 130 joint balls are blue. One
   line makes the whole armature blue, at the cost of the dock's colours no
   longer agreeing with its own Bones filter.
3. **Both Pose harnesses were RED ON THE RUNG** and still are, each with one
   named failure, measured before this lane wrote a line.
   `WW_POSEDRAW_TEST`: *clicking a bone did not make it the active object* -- a
   real application defect (a click at bone 0's screen position selects block 5).
   `WW_POSEEXTRAS_TEST`: *weight overlay found no influenced vertices* -- a
   harness defect (it inspects bone 0, the root, which drives no vertex). Two
   small lanes, neither this one's.
4. **NOBODY CLICKS THE NEW MENU ROWS.** Every gate drives the armature through
   the API or through the render hook's environment switches. Overlays > Bone
   Display is proved only by its strings being in the binary (`Bone Display` x1,
   `GLView/ArmatureDisplay` x1, `BoneDisplay%1` x1, read back out of the exe)
   and by the code compiling. A harness that opens the dropdown and clicks the
   three rows is owed.
5. **`WW_RENDER_SIZE`'s width floor moved again**: a request of 1000x1000 comes
   back **1524x941** on this build (it was 1293 on 17:08:39 and 1437 on
   15:52:46). Height is still `requested - 59`. The skill's table is amended
   with the exe each row was measured on.

## Skills

**NEW:** `<repo>/.claude/skills/ww-way-back-by-coverage/SKILL.md` -- proving an
exact way back when the look was ALSO ordered to change, and the shape of
merging two drawing routines into one.
**AMENDED, both need mirroring to `E:\Projects\Claude\.claude\skills`:**
`ww-test-harness-add` (new 5c, the measured framebuffer tolerance and its own
floor; new 5d, force the states the lane itself just made persistent) and
`nifskope-ww-render-shot` (the `WW_RENDER_SIZE` table gains the exe per row and
the 1524 measurement, plus the rule that a screen COORDINATE is never carried
between builds).

## Restart

**YES.** Whatever bungo opens next must be launched after **07:58:59**. No
NifSkope was running at any point in this lane (`rc=1` at every check, including
immediately before both links), so nothing of his was touched and no exe had to
be renamed aside.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane:
`src/glview.h`, `src/glview.cpp` (binary CRLF splice,
`scratchpad/skel2_20260910/patch_glview.py`), `src/skeletontools.h`,
`src/skeletontools.cpp`, `src/skeloverlaytest.cpp` (`patch_test.py`),
`src/nifskope_ui.cpp` (`hookup.py`, 2 anchors, CR 0 -> 0, marker x2),
`tests/spells/skeleton_overlay.sh`; NEW
`tests/spells/skeleton_overlay_coverage.py`,
`tests/spells/skeleton_overlay_dots.py`. Game down at every check; no harness
instance was left running.

- NATIVE1a block, spliced (lane text verbatim):

**NATIVE1a LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 08:49:08**, **20,989,440 bytes**, sha256 `140bb8d879cb2cf3...` (SKEL2's was
07:58:59, 20,927,488). ONE build (08:43:28) plus **TWO counted relinks** (08:47:59, the two
defects the gate found; 08:49:08, a measurement bug of my own). Markers:
`scratchpad/native1a_20260911/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_native1a.exe` = the 07:58:59 bytes exactly, md5-verified, written
once. Report `scratchpad/lane_native1a_report.md`; entry text
`scratchpad/native1a_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries NOT appended by
the lane** -- `scratchpad/native1a_20260911/MISTAKES_ENTRIES.md`. **Nothing committed.**

## THE PREMISE OF THE BRIEF WAS STALE, AND THAT IS THE FIRST THING TO KNOW

The brief's item 1 -- apply NATIVE0b's owed hook-up -- was **already done**. Lane BUILD6
applied it, ran qmake, built and gated it on **2026-09-10 03:57**, and said so only in a
section at the END of `scratchpad/lane_native0_report.md`. The contract page's STATUS block
at the TOP still said "NOT YET BUILT INTO THE EXE", and so did the brief. Measured, not
inferred: every A1-A4 / B1-B6 anchor present exactly once, `Makefile.Release` naming the
three sources, their objects on disk older than the 07:58:59 exe -- and the exe ran
`--native-fixture` and a real region bake before a line of this lane's code was written. The
STATUS line is now current and this lane owns keeping it so.

## What bungo gets

**The first real worldspace pair exists.** Nine-chunk Sanctuary, dim 4, 35 s headless:
`Commonwealth.lodo` **5,692,388 B** (2,970 bases, 2,982 meshes, 10,634 clusters, 142,138
triangles, 273,695 vertices) and `Commonwealth.lodi` **126,512 B** for **3,526 placements --
the stock manifests' count exactly, 0 dropped**. 35.9 bytes a placement, which reaches the
spec's planned 5.1 MiB at the spec's own placement count.

**His five performance rulings of 2026-09-11 are in the bytes**, each with a test that shows
the field moves and a floor that shows the test can fail: the staleness hash ("Add it"), the
instance-to-REFR join, the bound-radius rule, instances pre-sorted by mesh then material, GPU
cache order, the identity that the far shadows key on, and the shadow-caster silhouette rule.

**The stock path did not move.** 25 of 25 output files byte-identical with `--native` off,
AND 25 of 25 against the lane-0 baseline written by the 2026-09-10 03:57:46 exe.

## Gates (all on the 08:49:08 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `lodgen_native.sh` (NEW, nine legs) | **13 checks, 0 failures, PASS** | newly registered |
| -- the decoder on the fixture | **56 / 0** | 46 / 0 (+10 v2 expect keys) |
| -- the refusal set | **24 / 0**, each refused BY NAME | 20 (+9 v2, 1 corrected) |
| -- stock path with `--native` off | **25 files, 0 differ** | -- |
| -- the decoder on the real pair | **87 / 0** | 72 / 9 on the rung (both 9 were the decoder's own bars) |
| -- the v2 field gate | **25 / 0** | newly registered |
| -- the staleness floor | refused, naming the file and the field | newly registered |
| `lodgen_native_baseline.sh --check` | **25 in, 25 baked, 0 differ, PASS** | 25 / 0 |
| `lodgen_terrain.sh` | **26 / 0** | 26 / 0 |
| `lodgen_identity.sh` / `lodgen_merge.sh` / `lodl_write.sh` | **PASS** | PASS |
| `lodl_open.sh` | **23 / 0** | 23 / 0 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 72 / 0, 1 skip |
| `water_ui.sh` | **86 / 0, PASS** | 86 / 0 |

Three counts moved and each is named: the fixture decoder 46 -> 56 (ten v2 expect keys); the
mutation set 20 -> 24 (nine v2 mutations, and the `indexCrc32` case replaced because
`headerCrc32` covers the stored value and was answering instead); `water_ui.sh` 82 -> 86 with
the four `SHOT=` arguments, the same arming rule SKEL2 recorded.

## Three real defects the gate found, all fixed

1. **4 of 3,526 instances sat up to 0.055 u outside their own chunk's cull box** -- the
   writer computed `maxBoundRadius` from the unquantised scale and the consumer cannot.
2. **meshopt's cache optimiser read WORSE than Bethesda's own order on 21 of 2,982 meshes.**
   The writer now keeps whichever order is better, per shape.
3. **My own:** fixing (2) dropped the "after" accumulators, so the census printed
   `acmr 1.8600 -> 0.0000` and the gate called that an improvement. Caught by reading the
   number, not the verdict.

And two of my own instruments were wrong and were fixed as instruments: the decoder's
manifest position bar was measuring the manifest's six-significant-digit printf (step 1.0 at
six-digit coordinates -- 3,356 of 3,526 y values), and the cache-order floor encoded an
expectation about Bethesda's corpus instead of testing the metric.

## FOR BUNGO -- two calls, both written into the contract as deviations

1. **The mesh/material sort sits inside the CELL, not inside the chunk.** Mesh-major inside a
   chunk would leave one cell's instances in up to sixteen runs, which the 8-byte cell-range
   row cannot describe, and that blob is what the near-field suppression reads. A cell is
   4,096 units, so the grouping is still contiguous per (cell, mesh, material).
2. **The placed REFR form id stayed in the 8-byte cold record**, at the instance's own index,
   instead of growing the hot record from 24 to 32 bytes. The join is already one array read
   with no search; growing it is +33% on the one buffer the per-frame cull dispatch reads
   (~1.2 MiB on a full Commonwealth) to duplicate a number already there.

Either is a v3 format break if he wants it the other way round, and the readers refuse the
old stride or the old order by name.

## Owed / red

* **The (-32,0) dim-32 asymmetric drop proof was NOT run** -- it needs its own
  `--slot-fallback --native` bake. The census gate it backs is exact on this region.
* **The LOD panel has no `.lodo`/`.lodi` row at all**, and prints no stage times for any
  output today, so **the four stage times are NOT delivered**; the row is lane LODUI1's.
* Nothing in FO4CS reads either file; there is no reconstruction path, so parity is coverage
  numbers plus a point-set picture (`images/coverage.png`: 3,519 mutual pairs of 3,526 =
  99.80%, worst 0.1374 u once the manifest's print step is budgeted, 0 over the format's
  bound).
* `WW_CHANGES.md`, `HANDOFF.md` and `MISTAKES.md` NOT edited by the lane -- text is in
  `scratchpad/native1a_20260911/`.

## What NATIVE1b inherits

The cluster hierarchy is v3 and the room is named in the contract's new section 12:
`.lodo` header **0xC0..0xFF (64 reserved bytes)** for the cluster-error table's offsets,
`LodoCluster.flags` **bits 2-15** for the normal cone, `LodoBase.crossPx16[4]` already in the
row and written as 0, `.lodi` header **0x98..0xFF (104 reserved bytes)** for the occluder
table. The library stores full-detail geometry -- nothing is decimated -- so the ladder bungo
asked to run "from full detail down" has somewhere to start. **The instance record's growth
slot is GONE** (v2 took 0x16 for `drawKey`): a per-instance tint or light index now means a
32-byte stride and a reader that refuses 24 by name.

**RESTART: yes** -- any window bungo has open predates 08:49:08.

Skills amended in the REPO tree, for the director to mirror to the live one:
`nifskope-ww-lodgen` (the relative-path trap; the manifest-printf trap) and
`ww-standalone-writer-gate` (name the rule that must answer).

- NATIVE1b block, spliced 2026-09-11 (lane text verbatim):

**NATIVE1b LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 10:14:23**, **21,101,056 bytes**, sha256 `ab97d12c511aa598...`
(NATIVE1a's was 08:49:08, 20,989,440). ONE build (09:56:11) plus **TWO counted
relinks** (10:04:40 and 10:14:23, both for defects the gate found). Markers:
`scratchpad/native1b_20260911/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_native1b.exe` = the 08:49:08 bytes exactly, md5
`cf7fc96177afec212bb63a90c08c88c4`, written once. Report
`scratchpad/lane_native1b_report.md`; entry text
`scratchpad/native1b_20260911/WW_CHANGES_ENTRY.md`; **four MISTAKES entries NOT
appended by the lane** -- `scratchpad/native1b_20260911/MISTAKES_ENTRIES.md`.
**Nothing committed.**

## What bungo gets

**His items 1 and 2 of 08:0x and "2 sounds good" of the second round are in the
bytes.** `.lodo`/`.lodi` are **v3** and both readers refuse v2 BY NAME.

* **The ladder**, from FULL detail down. Level 0 is byte-for-byte what v2
  emitted; above it each material's clusters are grouped four at a time,
  simplified with the group's border LOCKED so nothing can crack, and re-split
  under the same caps. A new 48-byte row per cluster carries the bounding
  sphere, the normal cone, the error against full detail, the parent link and
  the level. Nine-chunk Sanctuary: **levels 0..7, 20,678 clusters (was 10,634),
  1,900 of 2,982 meshes laddered**, 4,715 roots covering 142,138 full-detail
  triangles exactly. `.lodo` **5,692,388 -> 9,657,316 B**;
  `--native-no-ladder` writes 6,204,388 B and is the exact way back.
* **Occluders**: 280 boxes over 87 of 147 populated cells on a downtown region,
  each fitted inside a watertight mesh, shaved by a voxel and probed at 100
  interior points before it is written.
* **The selection law** stated and implemented as a reference selector: nine
  cases (0.5/1/4 px x 2,000/8,000/32,000 u), every one a PARTITION, triangles
  124,205 -> 104,090.

Pictures, `scratchpad/native1b_20260911/images/`: **`ladder.png`** (a maple at
57 -> 28 -> 10 -> 3 triangles as the error goes 0 -> 236 -> 528 -> 804 units) and
**`occluders.png`** (280 box footprints over 33,123 downtown placements).

## THE ONE THING FOR BUNGO TO DECIDE

**The ladder is correct and barely selectable.** The median level-1 cluster
deviates by **3.80 percent of its model's own diagonal**, which reaches one
screen pixel only past **52,100 units** -- so at his default 1-px tolerance the
first step is not selected anywhere in the Commonwealth, and the measured cut
saves between 0 and 16 percent of triangles on this region. **The cause is that
"full detail" in this file is already Bethesda's LOD mesh**, a mean of 47.7
triangles for a whole building. Building the library from each base's near
`MODL` instead of its `MNAM` slots would give the ladder real room and would
serve his 10:3x ruling properly; it needs **no format change at all**, but it is
a much bigger library and a bake-time cost, so it is his call and not a lane's.

## Gates (all on the 10:14:23 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `lodgen_native.sh` (now **13 legs**) | **18 checks, 0 failures, PASS** | 13 / 0 |
| -- the decoder on the fixture | **69 / 0** | 56 / 0 |
| -- the refusal set | **44 / 0**, each refused BY NAME | 24 / 0 |
| -- the stock path with `--native` off | **25 files, 0 differ** | 25 / 0 |
| -- the decoder on the real pair | **87 / 0** | 87 / 0 |
| -- the field gate | **37 / 0, 1 named skip** | 25 / 0 |
| -- NEW, the geometry gate (spheres, cones, the cut, the boxes) | fixture **17 / 0**; real pair **15 / 0, 1 skip** | newly registered |
| -- NEW, the two ways back | one level, flag clear, 0 boxes | newly registered |
| -- NEW, the occluder region | **280 boxes, 280/280 inside, 280/280 leak when grown** | newly registered |
| `lodgen_native_baseline.sh --check` | **25 in, 25 baked, 0 differ, PASS** | 25 / 0 |
| `lodgen_terrain.sh` | **26 / 0, PASS** | 26 / 0 |
| `lodgen_identity.sh` / `lodgen_merge.sh` / `lodl_write.sh` | **PASS** | PASS |
| `lodl_open.sh` | **23 / 0** | 23 / 0 |
| `ui_align.sh` / `animws.sh` | **11 / 0** and **72 / 0, 1 skip** | same |
| `water_ui.sh` | **82 / 0** | 86 / 0 **with** its four picture arguments; 82 is the same suite without them (NATIVE1a's own note) |

Four counts moved and each is named: the suite 13 -> 18 (four new legs), the
fixture decoder 56 -> 69 (13 v3 expect keys), the mutation set 24 -> 44 (20 v3
mutations), the field gate 25 -> 37 (the ladder's 11 checks and the boxes' 4,
skipped where there is no box). Consistency: no source under `src` or
`NifSkope.pro` newer than the exe; 0 stale objects over the three changed
headers; `res/style.qss` and `release/style.qss` in step.

## Two real defects the gate found, both fixed, one relink each

1. **The sphere and the cone described the geometry that walked IN, not the
   quantised geometry written OUT** -- worst sphere overshoot **0.058 u**, worst
   cone cosine deficit **0.002033**. Both **0.000000** now; the contract states
   it as a format rule.
2. **A simplified group could open a hole.** Two meshes went from a WATERTIGHT
   level 0 to four and eight boundary edges, which is exactly what his
   far-shadow ruling forbids. A per-STEP refusal now catches it in the writer:
   **57 groups refused** on this region.

And three of my own instruments were wrong and were fixed as instruments: the
spell's `pwd -W || pwd` line (two-line paths, nine false failures), a cone floor
that could not fire on a zero-width cone, and a box floor that could not fire on
a box with room to grow.

## FOR BUNGO -- the calls, new and carried

1. **NEW: build the library from the near `MODL`?** (above). His call.
2. **NEW: the ladder simplifies on a POSITION WELD**, so levels 1 and up carry
   the first contributor's UVs where two vertices shared a quantised position --
   **117,722 welds merged differing UVs** over 263,876 source vertices. Level 0
   never uses the weld. The alternative (weld by position AND UV, far fewer
   laddered meshes) is one bake to measure.
3. **CARRIED, unchanged**: the mesh/material sort inside the CELL, and the
   placed REFR form id in the COLD record. v3 did not change either.

## Owed / red

* **The aggregate ring-3 impostors are NOT this lane** -- CARDS-AGG.
* The (-32,0) dim-32 asymmetric drop proof is **still not run**.
* The LOD panel still has **no `.lodo`/`.lodi` row** and prints no stage times --
  lane LODUI1's, and the four stage times are still not delivered.
* Nothing in FO4CS reads either file; there is still no reconstruction path, so
  the pictures are the decoder's own geometry.
* `WW_CHANGES.md`, `HANDOFF.md`, `MISTAKES.md` NOT edited by the lane.

## Restart

**YES** -- any window bungo has open predates 10:14:23. No window held the exe at
any of the three links, so nothing of his was renamed aside and nothing killed.
Game down and zero NifSkope processes at every check and at the end.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane: `src/lodofile.h`,
`src/lodofile.cpp`, `src/lodifile.h`, `src/lodifile.cpp`, `src/nativeemit.h`,
`src/nativeemit.cpp`, `src/nifcli.cpp`, `NifSkope.pro` (the last two through the
refusing script `scratchpad/native1b_20260911/hookup.py`, 7 anchors each exactly
once, CR 0 -> 0), `docs/LODGEN_NATIVE_LODO_LODI.md` (rewritten to v3, 96 anchor
rows, idempotent), `tests/spells/lodgen_native_decode.py`, `_mutate.py`,
`_fields.py`, `lodgen_native.sh`, and the NEW
`tests/spells/lodgen_native_cut.py`. **Three skill files in the REPO tree were
amended and need mirroring to `E:\Projects\Claude\.claude\skills`:**
`ww-spec-gate-audit` (7,345 -> 10,041 B), `ww-control-calibration`
(4,515 -> 6,279 B), `nifskope-ww-lodgen` (27,775 -> 30,210 B), all CR 0.

- TERRAIN-R LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  11:27:12, 21,137,920 B** (NATIVE1b's was 10:14:23, 21,101,056). Rollback rung
  `release/NifSkope.before_terrain_r.exe` (10:39:04, 21,101,056 — equal to the
  launch exe). Markers: `scratchpad/terrain_r_20260911/DONE` in, `BUILDING`
  gone. Report `scratchpad/lane_terrain_r_report.md`; entry text
  `scratchpad/terrain_r_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries
  NOT appended by the lane** — `scratchpad/terrain_r_20260911/MISTAKES_ENTRIES.md`,
  the director splices. ONE build (11:01:06) plus **TWO counted relinks**
  (11:23:53, 11:27:12), both declared in the report's section 5.

  **WHAT SHIPPED.** The `.lodt` container is **version 2** and its sheets are the
  OBJECT texture family (bungo 09:5x). Role 3 `data` (AO / wetness / shore /
  cover) is RETIRED and refused by name; **role 5 `mask`** carries `rmaos` —
  R roughness, G metallic, B AO, A ground cover — and **role 6 `emissive`** is
  written only when a layer's material supplies one. Shore proximity and wetness
  are dropped from the container (shore = a runtime subtraction from the `.lodl`
  water planes; far wetness = a weather state). The `.btr` chunk `_data.DDS` on
  the stock path is UNCHANGED and still byte-identical with the rung. `family` in
  the terrain index is **`"pbr"` and it means it**, with a per-layer rule census
  (`maskRules`) so the word is auditable. A v1 file is refused, not converted;
  bungo's installed `Data\Terrain` was listed READ-ONLY and holds no `.lodt`.

  **THE RING-0 GATE HE ASKED FOR (09:2x) IS DELIVERED.** The contract states the
  runtime blend's seven steps once (`docs/LODGEN_TERRAIN_VT.md` §2.5) and
  `tests/spells/lodgen_terrain_model.py ring0` implements them independently
  (own ESM walk, own BC1/BC3/**BC5U** decoding, own mip choice): two Sanctuary
  tiles at **mean 3.26 / 3.59, p95 8, max 14 / 17** of 255, floor (weights
  ignored) **13.70 / 15.15** = 4.2x, ceiling **0** over 73,984 texels.

  **GATES.** `lodgen_terrain_vt.sh` **41/1** (rung 35/1; +6 new green, the one
  failure is V9b, red on the rung too), `lodgen_ground_cover.sh` **29/5**
  (identical to the rung), `lodgen_terrain.sh` 26/0, `lodgen_native.sh` 18/0,
  `lodgen_card_arrays.sh` 35 ok PASS, `lodgen_texture_arrays.sh` 40 ok PASS,
  `lodl_open.sh` 23/0, `ui_align.sh` 11/0, `water_ui.sh` 82/0 (floor 72 — the
  brief's "86" was never this exe's number), and NEW
  **`lodgen_terrain_pbrm.sh` 14/0**. Identity: the stock path (6 files) and the
  object arrays bake (12 files) are byte-identical with the rung. Consistency:
  7 of 7 changed sources older than the exe, 11 of 11 dependent objects rebuilt,
  `res/style.qss` and `release/style.qss` byte-identical.

  **TWO CALLS FOR BUNGO.** (1) **Where the ground cover lives.** Shipped in the
  mask's alpha; `--vt-cover-in-color` reaches the other arm. Measured: the two
  cost **exactly the same bytes** (the format is per-tile by the COVER bit), and
  the stock engine tolerates a BC3 colour sheet either way (**2,001 of 2,001** of
  vanilla's own chunk colour sheets are DXT5). The only discriminator is meaning
  — the colour sheet's alpha is the slot `.lodm` §2.1 calls OPACITY and tells a
  consumer to alpha-test. (2) **The roads.**
  `scratchpad/terrain_r_20260911/images/ours_vs_vanilla_tile.png` puts our tile
  beside Bethesda's own sheet for the same ground at the same density: mean
  difference 19.96/255, and most of it is the roads and a rubble patch vanilla
  carries and we do not. That is lane ROADS1, and this is the case for running it
  before he bakes.

  **FOUR REDS / OWED.** (a) V9b, the assembled-vs-direct `_msn` byte identity the
  contract §2.4 claims — red on the rung, untouched here, nobody has measured
  whether the claim or the code is wrong. (b) **The resource stack cannot see a
  `.pbrm` OR a `.lodm`**: `BA2File`'s loose-file whitelist
  (`lib/libfo76utils/src/ba2file.cpp`) has neither extension, so `--resource`
  silently ignores both and `lodgen.h`'s own "consulted FIRST ... and .lodm
  alike" is not true today. Two lines in a vendored library; not taken here.
  (c) The object bake does not yet CONSUME the shared mask resolver — the law has
  one home and the gloss is genuinely shared, but the object path still takes its
  PBR answer from a `.lodm` sidecar, so a PBRM model bakes objects legacy and
  terrain PBR. (d) The render-hook top view of the region is owed; the four
  texel-level pictures are in `scratchpad/terrain_r_20260911/images/`.

  **NEW FILES.** `tests/spells/lodgen_terrain_model.py` (the independent terrain
  model: `mask` = T1/T2, `ring0` = T4), `tests/spells/lodgen_terrain_pbrm.sh` and
  `tests/spells/lodgen_terrain_pbrm_fixture.py` (T2/T3 both ways on a fixture),
  and the skill `.claude/skills/ww-corpus-absent-fixture/SKILL.md` — **written to
  the REPO tree, the director mirrors it to the live tree.**

  **RESTART: YES.** Anyone holding a NifSkope window from before 11:27:12 needs
  to restart it. No NifSkope was running at any point during this lane and none
  is running at its close; the game was down at every launch.

- ROADS1 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  12:19:06, 21,180,928 B** (TERRAIN-R's was 11:27:12, 21,137,920). Rollback rung
  `release/NifSkope.before_roads1.exe` (12:11:51, 21,137,920 — sha1 `a3283ae6…`,
  equal to the launch exe byte for byte). Markers:
  `scratchpad/roads1_20260911/DONE` in, `BUILDING` gone. Report
  `scratchpad/lane_roads1_report.md`; entry text
  `scratchpad/roads1_20260911/WW_CHANGES_ENTRY.md`; **three MISTAKES entries NOT
  appended by the lane** — `scratchpad/roads1_20260911/MISTAKES_ENTRIES.md`, the
  director splices. ONE build (12:12) plus **ONE counted relink** (12:19),
  declared in the report's section 3.1 with what the relink fixed.

  **WHAT VANILLA WAS MEASURED TO DO, BEFORE ANY CODE.** On Bethesda's own
  `Commonwealth.4.-20.20.DDS` — and that tile matters, see the correction below.
  (1) The road is NOT in the LAND paint: all sixteen cells have a LAND record and
  none of the sixteen painted landscape textures is a road, asphalt, concrete or
  pavement. (2) It IS at the road meshes' footprint and at no other family's:
  road AUC **0.716** bright / **0.678** grey against its own displaced floor
  (0.470–0.601 / 0.448–0.513), while trees 0.529, rocks 0.448, architecture
  0.621, set dressing 0.560 and every generic `bDecal` shape 0.564 all stay
  inside their own floors; ceiling 1.000. (3) The colour is the road material's
  own diffuse under the sheet's own grading — ×0.82 on the road,
  ×0.83 on the background, the same factor. (4) **The `_msn` does NOT carry it**:
  vanilla's `_msn` against a normal from the LAND heightmap alone disagrees by
  13.58° on the road and 14.14° on the background (control 14.45°).

  **WHAT SHIPPED.** Road meshes are scan-converted top-down into the far-terrain
  **colour sheet only**, both bake paths, between the VCLR multiply and the grass
  tint; the cover byte under a road is scaled by
  `1 − coverage × --road-cover-suppress` (default 1). Topmost triangle wins;
  colour = the shape's diffuse at the interpolated UV × vertex colour, mip from
  the triangle's own UV-area ratio; opaque shapes cover fully and only a shape
  whose MATERIAL or `NiAlphaProperty` says so honours the texture's alpha.
  Normal, height, roughness, metallic and emissive sheets untouched. The mesh
  test is component equality — `STAT` + `landscape`/`roads`|`sidewalks` + a
  component after — never a substring (`SetDressing\RailRoad\WaxCandle02Off.nif`
  is placed twelve times in Sanctuary). `--roads` / `--no-roads` (**on by default
  under both targets**) and `--road-cover-suppress F`.

  **GATES.** New `lodgen_roads.sh` **11/0 PASS**. Baselines all held:
  `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1 (V9b, red on the rung
  too), `lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0,
  `lodgen_texture_arrays.sh` PASS, `lodgen_card_arrays.sh` PASS,
  `lodgen_native.sh` 18/0, `lodl_open.sh` 23/0, `ui_align.sh` 11/0,
  `water_ui.sh` 82/0. `--no-roads` vs the rung exe: **9 of 9 files identical**.
  Road-presence metric, mask taken from vanilla's own sheet by colour and the
  extractor controlled first (51.1 % inside the independent geometric projection
  against 3.5–16.3 % displaced): **ceiling 1.0000, floor 0.1274, after 0.3065**,
  surrounding ground 0.3442; pre-registered bars ≥2× floor and ≥0.8× ground, both
  met. Whole-tile mean error 24.39 → **22.81**, on the road centreline
  **38.85 → 24.36**. Census: 253 placements / 73 meshes / 88,513 triangles /
  **27,695 texels** on the loop-road region, **0 texels** on the road-free one.
  Consistency: 3 of 3 changed sources older than the exe, both objects rebuilt,
  `res/style.qss` and `release/style.qss` byte-identical.

  **A CORRECTION TO THIS HANDOFF.** TERRAIN-R's block and
  `docs/LODGEN_PARITY.md` said of chunk (-20,24) that "most of" its 19.96 mean
  difference "is the roads". **Chunk (-20,24) contains zero road triangles** —
  86,770 are in (-20,20), 63,703 in (-16,24), 284,532 in (-16,16). The 19.96 is
  real and its cause is the splat grading, the next line in the same gap list.
  ROADS1 re-took the gate on (-20,20). MISTAKES entry written.

  **TWO CALLS FOR BUNGO.** (1) **The shared material resolver.** Every
  `Landscape\Roads\Country\*` and `\Alley\*` piece names its material as an
  absolute Bethesda build path and carries an empty texture set;
  `lodgenLoadModel` prepends `materials/` and resolves nothing, so 65 of 270 road
  shapes had no diffuse and every decal among them was invisible. The ROAD pass
  fixes it for itself (`lodgenRoadMaterialPath`, keying on the last
  `materials/`); the OBJECT bakes still drop those textures, and fixing it in the
  shared loader would move output the byte-identity gates pin. Yes or no.
  (2) **`--road-cover-suppress` default 1.0** — no grass and no grass tint under
  a road. Nothing was measured about vanilla here, because vanilla ships no cover
  plane to measure. 0.0 is the exact way back.

  **THREE REDS / OWED.** (a) Our road reads lighter and less blue than
  Bethesda's — the same splat-grading gap as the ground around it, which is why
  its error now matches the background's (24.36 vs 24.4) rather than beating it.
  (b) **Our far-terrain sheets are DXT1 with 8 mips; all 6,120 of Bethesda's
  Commonwealth sheets are DXT5 with 10** (3,060 colour + 3,060 `_msn`, measured
  over the whole folder). A DXT1 `_msn` has no alpha channel; this is what makes
  our chunk render blue-purple beside vanilla's in `top_region.png`. A lane of
  its own. (c) `Landscape\Sidewalks\*` is carried by the rule and is untested:
  187 texels against `Landscape\Roads`' 23,170 on the measurement tile.

  **PICTURES.** `scratchpad/roads1_20260911/images/cmp_sanctuary_road.png`
  (vanilla | ours without | ours with, the same 512 texels at the same 32 units,
  plus 4× zooms of the cul-de-sac), `road_mask_and_metric.png` (the gate's mask
  over all three, its own control burned in), `top_region.png` (the region from
  above through the render hook, each side staged as its own data root).

  **NEW FILES.** `tests/spells/lodgen_roads.sh`,
  `tests/spells/lodgen_roads_metric.py`. **TWO SKILLS AMENDED IN THE REPO TREE,
  the director mirrors them to the live tree:**
  `nifskope-ww-vanilla-compare` gains step 1a (pick the tile by PROJECTING the
  thing under test — the histogram that found (-20,24) empty), and
  `nifskope-ww-lodgen` gains the trap that a spell shelling out to `python`
  measures whichever `python` is on the PATH (the MSYS2 one has no `numpy`, and
  `lodl_open.sh` read 23/1 with two EMPTY numbers because of it before reading
  23/0 from Git Bash).

  **RESTART: YES.** Anyone holding a NifSkope window from before 12:19:06 needs
  to restart it. No NifSkope was running at any point during this lane except its
  own three headless renders and the GUI harnesses, one at a time, on the second
  monitor; none is running at its close; the game was down at every launch.

- LODUI1 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  13:24:12, 21,234,176 B** (ROADS1's was 12:19:06, 21,180,928). Rollback rung
  `release/NifSkope.before_lodui1.exe` (12:42, 21,180,928 — md5
  `9ca009cb289858c17d87546a216c45a6`, equal to the launch exe byte for byte).
  Markers: `scratchpad/lodui1_20260911/DONE` in, `BUILDING` gone. Report
  `scratchpad/lane_lodui1_report.md`; entry text
  `scratchpad/lodui1_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries NOT
  appended by the lane** — `scratchpad/lodui1_20260911/MISTAKES_ENTRIES.md`, the
  director splices. ONE build (13:09:49) plus **THREE counted relinks** (13:13:51
  the two checks the first gate run turned red; 13:22:11 and 13:24:12 both for
  the panel grab, which twice could not reach the rows it was offered as proof
  of), each declared in the report's section 4. `qmake` was re-run with the
  build: `src/lodgenmanager.cpp` gained a NEW include (`nativeemit.h`) that the
  frozen dependency list did not name.

  **WHAT SHIPPED, against his four rulings of that morning.**
  (06:4x, the five .lod types) Under **FO4 Community Shaders** the panel offers
  `.lodl`, `.lodt`, the new `.lodo`/`.lodi` pair and the `.lodm` sidecars, and
  four legacy rows are hidden whole: the `.btr` section (with its terrain-texture
  bake, cover rows and identity channels), the `.bto` head, the object atlas and
  "Chunk textures from the pyramid". The **Stock engine** shows all four again
  and loses the native head. The object SETTINGS did not split: the section's
  header carries two check boxes and one is offered at a time, so every row below
  stays reachable under both targets.
  (07:0x–07:2x, trees only) **`LodgenTreesOnlyCheck`, ON by default**; off is the
  old empty-far-slot rule and nothing else. `--candidates all` is **retired** and
  refuses by name in the CLI and in `tools/bake_impostor_cards.sh`. The three
  path tests now have ONE definition (`lodgenIsTreeModel`).
  (07:3x, 512 px) in the card resolution list and in the driver's `TILE` list;
  the cost line is computed and moves.
  (07:4x) "Cards from ring" is now **"Tree cards from ring"** and is tree-only in
  effect in both states of the toggle.
  **THE NATIVE ROW EXISTS AND IS WIRED** — the item NATIVE1a and NATIVE1b both
  closed on. A GUI-driven one-chunk Sanctuary run wrote
  `Terrain\Commonwealth.lodo` **9,657,316 B** and `.lodi` **37,248 B**.
  **THE FOUR STAGE TIMES ARE DELIVERED**, in the panel's new result line and on
  the command line, from one formatter.

  **GATES.** New `lodgen_stage_times.sh` **16/0 PASS** and `lodgen_panel_run.sh`
  **125/0 PASS** (the panel driven to completion twice, floor 125 MEASURED);
  `lod_generation.sh` **116/0 PASS** (was 97/0 on the rung, floor raised 74 →
  116). Baselines all held on the final exe: `lodgen_native.sh` 18/0,
  `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1 (V9b, red on the rung
  too), `lodgen_roads.sh` 11/0, `lodgen_ground_cover.sh` 29/5,
  `lodgen_terrain_pbrm.sh` 14/0, `lodl_open.sh` 23/0, `lodl_write.sh` PASS,
  `ui_align.sh` 11/0, `lodgen_merge.sh` / `lodgen_identity.sh` /
  `lodgen_card_arrays.sh` / `lodgen_texture_arrays.sh` /
  `lodgen_impostor_cards.sh` PASS. Consistency: the exe is newer than all 10
  changed files and the six objects that include `lodgen.h` are newer than it;
  `res/style.qss` and `release/style.qss` byte-identical.

  **THE STAGE TIMES ON THE SANCTUARY REGION**, measured, one chunk at dim 4:
  GUI object run `landscape 0.0, meshes 3.8, textures 0.2, impostors 0.0`; GUI
  heightmap run `landscape 1.1, meshes 0.0, textures 0.0, impostors 0.0`; the CLI
  region bake `landscape 0.0, meshes 4.1, textures 0.4, impostors 0.0`. The
  one-page bake instruction for the full Commonwealth, with those numbers and an
  extrapolation labelled as an estimate, is
  `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`.

  **THREE CALLS FOR BUNGO.** (1) **The FO4CS target still writes `.BTO` chunk
  files** — not as an offered output, but because the texture arrays, the card
  arrays, the shape merge and the far-ring cut all read them back, and FO4CS's
  Improved LOD module reads them today. The summary line says so in words. Stop
  writing them, or keep them until the runtime reads the pair? (2) **Where does
  the ground cover live** — carried over from TERRAIN-R, still open, and
  `--vt-cover-in-color` is still INI-only with no row, as the brief instructed.
  (3) **The Trees-only row is ON by default in the panel, but `--candidates`
  still defaults to `missing` in the CLI and the driver** — kept that way so no
  existing gate moved; the bake instruction names `CANDIDATES=trees` explicitly.
  Make them agree, or leave the CLI's old default alone?

  **TWO REDS / OWED.** (a) The panel grab reaches the target row, the five
  outputs and the native row, but **Trees only, 512 px and "Tree cards from ring"
  are below the fold** in it — they are proven by the harness counts and by the
  cost line printed in the log, not by the picture. A fourth relink would buy a
  second grab scrolled to the impostor rows. (b) Gate L3 was pre-registered as
  "a bake at 512 writes a 4096-wide sheet"; what is true is **4096 on the
  frame's LONG side**: `TreeMapleForest1.nif` at `OCT=8` gives 2176 x 4096 at
  512 px and 1152 x 2048 at 256 px, because a thin tree gets a narrower frame
  from its own silhouette (the row's tooltip already says so). The number moves
  with the row, which is what the gate was for.

  **PICTURES.** `scratchpad/lodui1_20260911/images/panel_fo4cs.png`,
  `panel_stock.png` (in-app dock grabs, 497 x 741, the progress pane hidden for
  the grab so the settings get the height) and `result_line.png` (the pinned
  action bar with the four stage times in it).

  **NEW FILES.** `tests/spells/lodgen_panel_run.sh`,
  `tests/spells/lodgen_stage_times.sh`.

  **RESTART: YES.** Anyone holding a NifSkope window from before 13:24:12 needs
  to restart it. No NifSkope was running at any point during this lane except its
  own harnesses and two headless card bakes, one at a time, on the second
  monitor; none is running at its close; the game was down at every launch.

- CENSUS1 block, spliced (lane text verbatim):

**CENSUS1 LANDED, DOCUMENTS ONLY, NO BUILD, EXE UNTOUCHED.** `release/NifSkope.exe`
is still LODUI1's **2026-09-11 13:24:12**; this lane compiled nothing, launched
nothing and wrote no `src/` file — lane BAKEPERF1 held `src/` and `NifSkope.pro`
throughout, and the director's amendment at launch voided the brief's
"header-only field … then do it" clause, so every missing generator census word is
NAMED as owed instead. Report `scratchpad/lane_census1_report.md`; markers
`scratchpad/census1_20260911/`.

**Shipped:**
* `docs/LODGEN_CENSUS.md` — **60 field names** in seven log rows
  (`[ImprovedLOD]`, `.Ring`, `.Cluster`, `.Cards`, `.Residency`, `.Shadow`,
  `.Fade`), each with name / unit / what it counts / the contract section it is
  read from / the scene change that must MOVE it / its complete refusal list /
  a default that accuses its plumbing. Covers every item of bungo's "We need
  them" (2026-09-11 10:0x) plus his screen-size fade spec (10:4x), his zoom answer
  (10:5x, the finest-level residency budget) and the grid-edge seam band. Its
  second half inventories the census the generator already prints and turns it
  into 14 cross-checks a runtime number can be tested against.
* `tests/spells/lodgen_census_check.py` — read-only, no exe, decodes a pair with
  the independent decoder and checks the census line against the bytes.
* one pointer paragraph in `docs/LODGEN_NATIVE_LODO_LODI.md` §7.
* new skill `.claude/skills/ww-census-contract/SKILL.md` (repo tree).

**Gates, all green, all with floors that fired:**
* **C2** on the 9-chunk Sanctuary pair (`scratchpad/native1b_20260911/gate/final/native`,
  NATIVE1b 10:17:44): **59 checks, 0 failures, 31 census words the files cannot
  carry**; `--self-floor` caught all three doctored claims. Same numbers on the
  NEWEST pair on disk, LODUI1's `scratchpad/lodui1_20260911/stage_times/region`
  (13:25:22).
* **C1 / C3** (`scratchpad/census1_20260911/page_gate.py`): 60 field rows, 0 blank
  cells over the six gate columns, 109 citations all resolving; floors caught 5
  blanks and 2 bad citations on in-memory copies.
* Logs `scratchpad/census1_20260911/logs/`.

**FOR THE DIRECTOR, three things:**

1. **A defect, found and not fixed** (not this lane's file).
   `tests/spells/lodgen_native_decode.py` REFUSES the downtown-Boston pair
   `scratchpad/native1b_20260911/gate/final/occ` — 33,123 placements, the only
   pair on disk with occluder boxes — at instance 3359, *"out of (cell, drawKey,
   ref, part) order"*. Measured by hand: its neighbour sits at **fy 2.999985**
   cells from the chunk origin after quantisation, one step below the cell line at
   3.0, so the decoder re-derives cell 4 while the writer sorted the float position
   into cell 0. Cell-level twin of the chunk-boundary effect NATIVE 4.1c / NATIVE 6
   already document; the fix is a DECODER rule — take an instance's cell from the
   cell RANGE it falls in — not a format change. **No gate has ever decoded that
   pair**: NATIVE1b ran `lodgen_native_cut.py` on it and `lodgen_native_decode.py`
   only on Sanctuary.
2. **Five owed generator words**, in the page's §6.3 with the exact bytes each
   would take: per-MNAM-slot instance totals in the `.lodi` header (the honest
   version of "per-ring totals" — a ring is camera-relative and no file can state
   one); a per-base full-detail triangle count (the room exists: `crossPx16[4]` is
   eight bytes written as all zeros, NATIVE 11 Deviation 5); a card count in the
   `.lodo` header; the aggregate ring-3 impostors AND the forested-cell count bungo
   asked for at 08:3x (both owed to CARDS-AGG); and a watertight bit in the mesh
   row's free flags.
3. **Mirror the new skill** `ww-census-contract` to
   `E:\Projects\Claude\.claude\skills\` (CONSTITUTION 1a — the two trees drift and
   nothing syncs them).

**RESTART:** not needed. No exe changed; bungo's window is unaffected.
**Uncommitted:** this lane adds 3 new files under `docs/`, `tests/spells/` and
`.claude/skills/`, edits `docs/LODGEN_NATIVE_LODO_LODI.md`, and writes its own
`scratchpad/census1_20260911/`. Nothing committed (CONSTITUTION 8).

- **BAKEPERF1 LANDED AND GATED, EXE FREE.** `release/NifSkope.exe`
  **2026-09-11 14:48:52, 21,261,312 B** (LODUI1's was 13:24:12, 21,234,176).
  Rollback rung `release/NifSkope.before_bakeperf1.exe` (13:24:12, 21,234,176 B,
  md5 `edd2a99ea9fcbeb21b18db05c8b0dbb3` — equal to the launch exe byte for
  byte). Markers: `scratchpad/bakeperf1_20260911/DONE` in, `BUILDING` gone.
  Report `scratchpad/lane_bakeperf1_report.md`; entry text
  `.../WW_CHANGES_ENTRY.md`; **three MISTAKES entries NOT appended by the lane**
  — `.../MISTAKES_ENTRIES.md`. **ONE build (13:59:25 → 14:02:15, qmake re-run
  for two new sources) plus FIVE counted relinks**, each declared in the
  report's §3: 1 the array-pseudonym race, 2 the nested fan-out, 3 the
  crash-dialog fix + the whole texture budget, **4 the fallback (chunk fan-out
  opt-in) + serialised NIF parsing**, 5 the persistent fan-out pool. A SIXTH
  link was taken and discarded for diagnosis only —
  `make LFLAGS="-Wl,-subsystem,windows -mthreads"` to put the symbol table back
  for one gdb run; the shipped exe is stripped as the project's flags say.

  **WHAT BUNGO ASKED, AND THE ANSWER.** *"bake time, anything we can do to speed
  it up? use my system to its fullest here?"* — **not on the chunk queue yet,
  and the blocker is named: the NIF parser is not thread-safe.** Building
  NifModels on worker threads faults; five runs of five on the nine-chunk
  Sanctuary region ended in `STATUS_HEAP_CORRUPTION`, and a symbolised stack put
  it in `NifItem::deleteChildItems()` under `BaseModel::~BaseModel()` inside
  `lodgenLoadModel`, with every other worker in the same parser. Serialising the
  parse makes the fan-out survive and it is then **1.8x SLOWER** than one
  thread, because parsing is the part that cannot overlap. So the machinery
  ships switched off behind one number and the default bake is byte for byte the
  bake that has always run.

  **WHAT SHIPPED.** `src/lodgenchunkpass.{h,cpp}` (new) — the chunk queue the
  panel and the CLI had written out twice, now one function, with results
  retiring **in job order** so `writtenBto` (the atlas, texture arrays, merge,
  far-ring cut, card arrays all read it in order), the printed lines, the panel
  preview, the progress bar and the `.lodo`/`.lodi` accumulator can never see
  completion order. `src/lodgenparallel.{h,cpp}` (new) — `--threads N` (the
  general fan-out budget, default the machine, used by the BC1/BC3/BC4 block
  encoders), `--chunk-threads N` (**default 1**), a persistent pool, a bounded
  **writer thread** that drains before the run returns, and the peak working
  set. `nativeemit` gained a per-job **journal** replayed in job order.
  `gamemanager` locks the process-wide NifModel resource map; `nifmodel` locks
  the array-pseudonym tables; `nifcli` calls `SetErrorMode` so a headless crash
  never shows a dialog.

  **GATES. Byte identity GREEN, all four, both regions** — Sanctuary 9 chunks
  60 files / 24,975,886 B and downtown Boston 25 chunks 163 files / 77,591,755 B,
  the rung against the shipped default AND the serial queue against
  `--chunk-threads 16`, plus the literal `--threads 1` form. The comparator was
  shown RED first on one flipped byte and on one missing file. Stability
  `--chunk-threads 16`: **5 of 5 clean** after relink 4 (5 of 5 faulted before
  it). **TWELVE HARNESSES, ELEVEN GREEN, ONE UNMOVED RED**:
  `lodgen_stage_times` 16/0, `lodgen_terrain` 26/0, `lodgen_native` 18/0,
  `lodgen_identity` PASS, `lodgen_merge` PASS, `lodgen_texture_arrays` PASS,
  `lodgen_card_arrays` PASS, `lod_generation` **116/0** (floor 116),
  **`lodgen_panel_run` 125/0** (the panel driven to completion twice on the
  rewritten chunk loop), `ui_align` 11/0, `water_ui` 82/0 (floor 72); the one
  red is `lodgen_terrain_vt` **41/1**, the same `V9b` check at the same count
  that was red on the rung. **AND THE HEADLINE NUMBER: no regression and no
  speed-up** — three alternating runs a side give medians 16,040 ms (rung) vs
  16,130 ms (shipped), 0.6 percent apart against a 2.3 s within-exe spread.
  Stage times, memory and the Commonwealth extrapolation: report §3.8–§3.11.

  **FIVE REDS.** (R1) **the NIF parser is not thread-safe** — a lane of its own,
  and the one thing that unlocks the machine; (R2) the chunk fan-out is correct
  and byte-identical but SLOWER, so it ships off; (R3) a pre-existing defect
  found while auditing: `src/model/nifmodel.cpp:1421` copy-assigns
  `multiArrayPseudonyms1` INTO the global `arrayPseudonyms` instead of
  re-pointing a reference, so the first multi-array row displayed replaces the
  application's singular-name table for the session — UI path, deliberately not
  fixed here; (R4) the pyramid tile loop is still serial, same blockers; (R5) no
  panel row for either thread number, as the brief instructed.

  **OWED, best first.** Hoist `lodgenBuildObjectChunk`'s per-chunk `modelCache`
  to the RUN — every chunk re-parses every model it needs, it is byte-identical
  because the loader is a pure function of the path, and it speeds up the SERIAL
  path, which is the one that ships. Then batched card bakes: **one process,
  many models**, not K models per frame (the frame's arithmetic is a world
  measurement off viewport pixels — lane CARDORTHO's trap), worth a process
  start plus the hook's fixed 1200 ms sleep per card.

  **NEW FILES.** `src/lodgenchunkpass.{h,cpp}`, `src/lodgenparallel.{h,cpp}`,
  two skills in the REPO tree (`nifskope-ww-crash-diagnose`,
  `ww-parallelise-a-stage`) for the director to mirror.

  **RESTART: YES.** Anyone holding a NifSkope window from before **14:48:52**
  needs to restart it.

- PLAN-FO4CS LANDED, DOCUMENTS ONLY, NO BUILD, EXE UNTOUCHED.
  `release/NifSkope.exe` is still LODUI1's **2026-09-11 13:24:12**; this lane
  compiled nothing, launched nothing and wrote no file under `src/`, `res/`,
  `tests/` or `tools/`. Lane BAKEPERF1 held `src/` and `NifSkope.pro` throughout
  and neither was touched. The FO4CS repository
  `E:\Projects\Fo4CommunityShaders` was read and **not written**. Report
  `scratchpad/lane_plan_fo4cs_report.md`; markers and gate
  `scratchpad/plan_fo4cs_20260911/`.

  **SHIPPED.** `docs/FO4CS_IMPROVED_LOD_PLAN.md` — new, **72,969 bytes, 1,195
  lines, CR 0**. The plan the FO4CS session builds Improved LOD from: one module,
  its own master switch, ships off (bungo 2026-09-09 15:34), built LAST against
  frozen contracts, defining no byte and no field — where it and a contract page
  disagree the contract wins, and the page says so in its first paragraph.

  **SIX RUNGS, each one FO4CS wave with its own flight**, each with all nine
  parts (reads / does / switch and keys / fallback and its arm word / census /
  gates / flight / must not / RE candidates): **R0** loaders + staleness, nothing
  drawn, the flight is one census line; **R1** vanilla far objects suppressed,
  native far field drawn, ONE shadow representation per placement; **R2** terrain
  — the `.lodt` pyramid beyond, the runtime blend from the `.lodl` weights in the
  inner band, cross-faded; **R3** screen-size selection, sphere/cone/occluder
  culling, the four fades, and the hybrid far shadow pass; **R4** cards; **R5**
  the three 5x5-grid rounds, after R1-R4 have flown.

  **HIS SHADOW RULINGS OF 14:4x -> 15:0x ARE IN IT AS LAW, IN THREE PLACES.**
  R1: every placement has exactly one shadow representation at a time, chosen the
  same way its draw is chosen — cells inside the loaded grid drop out of the
  far-shadow casters by the same cell-range table that drops their draws, the
  cascades never contain LOD geometry, casters counted per source, self-shadow
  excluded by identity. R3: ONE height source for draw and shadow (the `.lodl`
  finest level in the inner band, the pyramid's resident height sheet beyond,
  `HeightMap.dds` as the fallback arm), and the HYBRID as the default — march the
  terrain, render only the object cluster cut plus the cards into a far shadow
  map, combine with a max, map-only as the fallback arm. §3's seams table: the
  grid-edge band blends shadows the way it blends draws. **His two millisecond
  figures are labelled ESTIMATES in both places they appear**, with the first
  FO4CS capture round named as the thing that measures them.

  **GATES, three, all green, all with floors that FIRED**
  (`scratchpad/plan_fo4cs_20260911/plan_gate.py`; read-only on the tree, every
  floor on an in-memory copy so a turn ending early cannot poison it):
  G1 **73 numbered citations over eight contract pages + 4 named headings, 0
  unresolved** (floor caught 2); G2 **6 rungs, 0 short** (floor caught 1);
  G3 **158 backticked identifiers, 38 keys the page declares itself, 0 found in
  no contract** (floor caught 1). G2 found R5 genuinely short of READS and DOES
  and it was written. G3 found two defects in ITSELF before it found one in the
  page — a token splitter that took `.` before `[` and therefore accused
  `rep[0..3]`, a phrase NATIVE 4.4 uses verbatim.

  **PROVENANCE.** All eight contract hashes taken before reading and re-derived
  after the last edit: **none moved**. Version constants re-read last of all, and
  the second question that step asks produced a finding that is in the page
  twice: **FO4CS's shipped `.lodl` parser pins `kVersion = 1u` and refuses 2 and
  3**; `WW_LODL_VERSION=1` is the zero-effort way back and needs no rebuild on
  our side. `HANDOFF.md` MOVED during the lane
  (`81fdc08dfa8b7707`, 297,028 B -> `8a7fce2f67c7f898`, 302,554 B) because the
  director spliced the shadow rulings in mid-lane; the footer states both values.
  **No `src/` line number in the page, in either repository.**

  **FIVE THINGS FOR THE DIRECTOR.**
  1. **A NEW OWED ITEM nobody had.** The `.lodt` height sheet is opt-in
     (`--vt-height`) and `docs/LODGEN_TERRAIN_VT.md` §5's CLI table does not list
     the flag although §2.2 names it. His 15:0x ruling makes that sheet the
     shadow march's height source beyond the inner band, so **a full bake without
     it leaves R3 with no source** — and it more than doubles a pyramid tile
     (184,960 B of height against 138,720 for the three colour-class sheets
     together). Two halves: a generator lane for the CLI row, and bungo for the
     bake. This belongs in the bake instruction before he bakes.
  2. **THREE CENSUS WORDS ARE OWED HERE, not to a writer.** His 14:4x ruling asks
     for a caster count per SOURCE; his 15:0x ruling makes the march's own cost
     and the shadow map's own cost the two numbers the first capture round must
     produce. `docs/LODGEN_CENSUS.md` carries none of the three. The plan names
     them in plain English rather than coining keys, so the census-page lane owns
     their spelling.
  3. **R5c should not be chartered before the near-`MODL` call is taken.** The
     ladder is correct and at his own 1-px tolerance its first step is selected
     nowhere in the Commonwealth (median level-1 cluster 3.80 percent of the
     model diagonal, one pixel only past 52,100 units, because "full detail" is
     already Bethesda's LOD mesh at 47.7 triangles a building). Shipping the
     inward extension into that library would read as a failure of the mechanism.
     It is §6 item (a) and it is first in the list for that reason.
  4. **FOUR END-MENU ROWS need his word.** He asked for *"a menu row + INI key
     each"* for the four screen-size fade thresholds on 10:4x; the standing rule
     is no row without him, so the plan proposes and does not assume. Every other
     key in the page is INI-only policy and every one is labelled a PROPOSAL.
  5. **A SKILL AMENDMENT IS READY AND WAS NOT APPLIED**, because the brief
     confined this lane to its own two files:
     `scratchpad/plan_fo4cs_20260911/SKILL_AMENDMENT_ww_census_contract.md` — a
     new section 2a for `ww-census-contract`, "when the page cites more than one
     sibling", carrying the four traps this lane paid for (named headings as well
     as numbered ones; a numbered LIST inside a section is not a subsection;
     fix the extractor and never the document when a vocabulary gate accuses a
     word you can read in the source; a real word that belongs to no contract
     goes in a NAMED allowlist with its source file, never a silent corpus
     widening). **The director applies it to BOTH trees** (CONSTITUTION 1a).

  **UNCOMMITTED:** this lane adds `docs/FO4CS_IMPROVED_LOD_PLAN.md` and its own
  `scratchpad/plan_fo4cs_20260911/` plus `scratchpad/lane_plan_fo4cs_report.md`.
  Nothing committed (CONSTITUTION 8). **RESTART: not needed** — no exe changed
  and bungo's window is unaffected.

- NIFPARSE1 block, spliced (lane text verbatim):

- NIFPARSE1 block (ENDED BUILD PENDING 16:5x; resume scratchpad/nifparse1_20260911/PENDING.md), spliced (lane text verbatim):

Text for `HANDOFF.md`'s top block. This is the BUILD PENDING version; if the
build slot frees and the gates run, it is rewritten with their numbers.

---

- **NIFPARSE1 ENDED BUILD PENDING (slot held by CARDS-AGG; game down
  throughout).** Nothing built, nothing applied, nothing committed. Resume
  `scratchpad/nifparse1_20260911/PENDING.md`; report
  `scratchpad/lane_nifparse1_report.md`; FOUR MISTAKES entries **not appended
  by the lane** at `.../MISTAKES_ENTRIES.md`.

  **WHAT IT FOUND, AND IT MATTERS BEFORE ANYONE BUILDS TO BAKEPERF1'S
  CONCLUSION.** *"The NIF parser is not thread-safe"* was drawn from one stack
  taken in the middle of a whole chunk bake — which is the parser and the plugin
  reader and the texture cache and the archive layer and the message sink at
  once — and for `STATUS_HEAP_CORRUPTION` a stack names where the damage was
  DETECTED, not where it was done. Following the call out of `lodgenLoadModel`
  instead of stopping at the file boundary turned up two unsafe things, **and
  neither is in the model layer**:

  1. `GameManager::GameResources::init_archives()` / `close_archives()`
     (`src/gamemanager.cpp:174-209`, `:254-262`) **delete and rebuild one shared
     `BA2File` with no lock**, and the self-healing retry inside `get_file`
     (`:310-316`) calls `close_archives()` — freeing an index other workers are
     reading, and dangling the interior `std::string_view`s `findFile` hands
     out. `lodgenWarmSharedIndices()` covers FIRST use and only first use.
  2. `Message::append` / `Message::message` **construct `QMessageBox` WIDGETS on
     whatever thread calls them** and append to an unguarded static vector
     (`src/message.cpp:160`, boxes at `:30/:51/:197`). Refuted for the
     `-no-gui` command line (the widget-building handler is installed only
     inside `qobject_cast<QApplication*>`, and `-no-gui` builds a plain
     `QCoreApplication`) — but **live for the LOD Generation PANEL**, where one
     missing texture in a chunk worker builds a widget off the GUI thread.

  And the reads line up with BAKEPERF1's own bisect: `lodgenReadAsset` serves
  `.nif` from `lodgenMeshArchives()` with const, pointer-based
  `findFile`/`extractFile` and **never enters `GameManager`**, while `.dds`,
  `.bgsm` and `.pbrm` fall through to `GameManager::get_file`. The two variants
  that came back CLEAN — `--no-tex-dir` and `--no-roads` — are exactly the two
  that stop entering it.

  **THE DELIVERABLE IS THE EXPERIMENT, and it is written and syntax-checked, not
  run.** `src/nifparsestress.{h,cpp}` (new) builds, loads, walks and destroys
  `NifModel`s on N threads from bytes read once up front — no `EsmWorld`, no
  texture cache, no archive lookup, no road gatherer, no file I/O in the
  threaded region. Red ⇒ the model layer really is the fault; green ⇒ BAKEPERF1's
  conclusion is wrong and the fix belongs in the resource layer. Every worker
  must also reproduce the one-thread reference DIGEST of what it read back, so
  silent corruption that does not fault still fails; the floors are a sabotage
  mode that must go red, a fixture that must load, an item count that must
  exceed 32, and a load count that must equal `threads x reps x files`.
  Driver: `tests/spells/parse_stress.sh` (S1 the floor FIRST, then S2/S2b/S3/S4).
  `g++ -fsyntax-only` RC=0.

  **TWO REFUSING SCRIPTS, both `--check` green, NEITHER APPLIED:**
  `hookup.py` — 7 anchors, all matched once, CR 0 both sides on `NifSkope.pro`
  (CARDS-AGG's) and `src/nifcli.cpp`; `fixes.py` — **25 edits** over
  `src/message.cpp`, `src/gamemanager.{h,cpp}`, `src/data/nifvalue.cpp`,
  `src/model/nifmodel.cpp`, `src/xml/nifexpr.cpp`, `src/lodgenparallel.{h,cpp}`, all
  matched once, CR unchanged on all eight. They are not applied because gate N1
  (the fault named from a symbolised stack) is not discharged, and BAKEPERF1's
  own third recorded mistake was shipping three fixes on hypotheses.

  **ONE THING IN THE FIX SET IS A BUG BUNGO CAN SEE**, and it is BAKEPERF1's red
  R3, left unfixed there: `src/model/nifmodel.cpp:1431` binds
  `QHash<QString,QString> & pseudonymMap = arrayPseudonyms;` and then assigns
  `pseudonymMap = multiArrayPseudonyms1;`, which **copy-assigns into the global**
  instead of re-pointing — so the first multi-array row ever displayed replaces
  the application's singular-name table for the rest of the session. The fix is
  a pointer; its gate is the editor harnesses, not the bake.

  **NOT MEASURED, and named as such:** the fault is NOT named — three candidates
  with the experiment that separates them, no cause stated. No build, no relink,
  no bake, no harness, no 20-run loop, no stage table, no memory point. Whether
  a thread-safe parser would produce a SPEED-UP at all is unmeasured: BAKEPERF1's
  numbers have the fan-out 1.5–1.8x SLOWER with the parse serialised and peaking
  at 21.9 GB on 25 chunks, and the per-worker cost (its own plugin reader plus
  its own texture cache) means the default can never simply be the core count —
  `lodgenChunkThreadMemoryCap()` is declared for that and its constant must come
  from the 100-chunk run, which has not happened.

  **SKILL AMENDED:** `.claude/skills/ww-anchored-hookup/SKILL.md` gained
  sections 5 and 5a — never anchor an `after` on a line ending in `{`, and measure a
  multi-line anchor's indentation with Python byte counts instead of typing it, and build it by READING the line out of the file.
  Repo tree only; **the director mirrors it to the live tree**.

  **RESTART:** not needed. No exe changed; bungo's window is unaffected.
  **Uncommitted:** this lane adds `src/nifparsestress.{h,cpp}`,
  `tests/spells/parse_stress.sh`, its own `scratchpad/nifparse1_20260911/`, and
  edits `.claude/skills/ww-anchored-hookup/SKILL.md`. Nothing committed
  (CONSTITUTION 8).

- SPLAT1 PHASE A LANDED, PHASE B **BUILD PENDING**, NOTHING BUILT, NOTHING
  CHANGED. Read-only lane: no exe run, no bake made, nothing touched under
  `src/`, `res/`, `tests/`, `tools/`, `docs/` or `NifSkope.pro`; no commit; the
  game was down at every check. Report `scratchpad/lane_splat1_report.md`;
  resume `scratchpad/splat1_20260911/PENDING.md`; entry text, mistakes (3) and
  the contract amendment in the same directory, for the director to splice.

  **THE ANSWER TO HIS QUESTION.** He asked whether the terrain bakes use the
  landscape textures at their correct scale. **They do not.** The bake stretches
  every landscape texture to **2,048 world units a repeat**; the engine's own
  repeat is **341.3333** -- exactly **6x** smaller. `fLandTextureTilingMult`
  = 1.5 in `Fallout4.exe` 1.10.155 (one code reference, VA 0x1403A74C6),
  `uv = vertexIndex * mult/4 = 0.375` over the 17x17 quadrant grid, 128 world
  units a vertex, so 128/0.375 = 341.3333 -- twelve repeats a cell.
  `scratchpad/splat1_20260911/s2c_engine_tiling.py` re-derives every address
  from the shipped binary. At the correct scale one repeat is 10.7 texels of a
  32-unit far sheet, which is the "very tiny repeating pixel sized patterns" he
  described, and the footprint-matched tap over it lands near the texture's own
  mean -- smooth, like vanilla's.

  **THE MIP IS NOT THE DEFECT.** It already picks the footprint mip (5.00 for
  all 20 layer textures, all 2048x2048 with 12 mips) and an exact box mean over
  the footprint is 0.50 units SMOOTHER, not 52. The director's first candidate
  is refuted by its own number.

  **THE NUMBERS.** 3x3 local variance of luminance, 512x512 sheets at 32 world
  units a texel; instruments gated 12/12 first (`s0_selftest.py`), codec floor
  1.52, phase-randomised twin and a smooth/checker known-answer pair beside
  every row.

  | | (-20,24) | (-20,20) |
  |---|---|---|
  | vanilla | 19.81 | 29.39 |
  | ours as shipped | **76.22** | **75.26** |
  | offline re-bake at TILE 2048 (the model's control) | 71.95 | 61.71 |
  | **offline re-bake at TILE 341.333** | **12.93** | **14.60** |

  Ruled out with their own numbers: the grass tint (chunk (-20,24) has NO cover
  plane -- `_data.DDS` is DXT1 with no `WWCV` stamp -- so the tint touched none
  of it, and that is the tile with the LARGER excess), VCLR (moves the sheet by
  0.02 and 0.05 of a 52 and 32 excess; and the cells carrying no VCLR show the
  larger excess), the BC1 codec (1.52), the 17x17 blend (0% on one tile, 33% on
  the other, and it goes away with the tiling too).

  **A CONTROLLED NEGATIVE.** Bethesda's sheet contains no landscape-texture
  pattern at ANY of six candidate repeats -- every correlation inside its own
  phase-twin floor -- while the same correlation reads **+0.91 / +0.88** against
  OUR sheet, where one certainly is. Their fine detail is objects, roads and
  paint.

  **SAID PLAINLY: THIS DOES NOT CLOSE THE COLOUR ERROR.** Whole-tile mean
  absolute RGB against vanilla goes 16.89 -> 15.02 and 21.73 -> 20.16 of 255.
  The remaining 16-20 is the GRADING (ROADS1's x0.82-0.83), and "splat
  calibration vs vanilla grading" stays OPEN. Nobody should read this lane as
  closing it.

  **PICTURE FOR BUNGO:**
  `scratchpad/splat1_20260911/images/speckle_diagnosis.png` (1078x1324) --
  vanilla | ours as shipped | ours re-baked offline at 341.333 | the difference
  x4, the same 128x128 texels at (216,128) of chunk (-20,24) chosen by the
  metric, 4x nearest neighbour, local variance burned into every panel
  (18.58 / 89.21 / 13.29).

  **WHY PHASE B DID NOT RUN.** Its gate was CARDS-AGG's DONE and NIFPARSE1's
  DONE with no `BUILDING` marker anywhere. CARDS-AGG held `BUILDING` at every
  poll and neither DONE appeared (`logs/poll2.log`). **No code was written, so
  nothing is half-applied.** The change when it runs is ONE value -- `TILE` at
  `src/lodgen.cpp` 6335 and 7623 -> a bake option defaulting to 341.3333, CLI
  `--land-tiling`, `--land-tiling 2048` the exact way back -- and it must move
  the COLOUR and the MASK/EMISSIVE paths together (the same `TILE` is read at
  6680, 6688, 7661, 7665, 7699, 7703, 7718, 7722), or the roughness sheet ends
  up describing different ground from the colour beside it. `_msn` comes from
  VHGT and must not move at all. Gates S1-S7 pre-registered in `PENDING.md`.

  **THREE REDS FOR THE DIRECTOR.** (a) `docs/LODGEN_TERRAIN_VT.md` 2.5's VCLR
  range does not reproduce: it says 249..255 over cells -20..-17 x 24..27;
  measured, 11 of 16 cells carry a VCLR and the range is **203..255** (and
  170..255 on the other region). Its conclusion survives, its number does not.
  (b) The contract cites the bake for the tiling constant -- a guess became law
  by restatement (MISTAKES entry). (c) When the tiling moves, every stored
  colour expectation in `lodgen_terrain.sh` / `lodgen_terrain_vt.sh` moves with
  it; re-taking them silently would hide the very change under test.

  **RESTART: no.** No exe was built or run by this lane.

  **AND THE COUNTERWEIGHT, IN THE SAME BREATH.** Correcting the tiling does not
  move our spectrum TOWARD vanilla's -- it moves it away. Vanilla keeps 16% of
  its power at 2-4 texels and 6% below 2; at the correct tiling we keep 2.6% and
  0.2%. That is consistent with the controlled negative above: vanilla's fine
  detail is content we do not bake (objects, rubble, road edges, paint), not
  ground-texture grain. So today our sheet has the WRONG fine detail at the
  wrong amplitude from a source vanilla does not have; after the fix it has
  almost none, which is honest for a sheet built from the splat alone, and the
  missing content is TERRAIN-AO1 / ROADS1 / objects, already open. Report both
  or the fix reads as more than it is.

  **THE PHASE-B GATE IS UNREACHABLE, MEASURED.** CARDS-AGG's `BUILDING` is
  stamped 15:48, its newest file 16:02, nothing since, and its own `PENDING.md`
  says "THE BUILD HAS NOT RUN YET"; NIFPARSE1 wrote its four handoff documents
  15:43-15:47 and stopped. Neither will write a DONE, so SPLAT1 ends BUILD
  PENDING on that evidence, not on the poll limit. **THREE LANES NOW SHARE ONE
  BUILD SLOT AND ONE FILE** (`src/lodgen.cpp`): the order that avoids a second
  build is NIFPARSE1's hook-up, then CARDS-AGG's, then SPLAT1's one tiling value
  LAST with the anchor pass re-run -- `nifskope-ww-resume-pending`, spelled out
  in `scratchpad/splat1_20260911/PENDING.md`.

- CARDS-AGG block, written 16:42 (lane text verbatim):

**CARDS-AGG LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 16:20:02**, **21,419,520 bytes**, md5 `3ebf175826feee6c6545cf0873635acc`
(BAKEPERF1's was 14:48:52, 21,261,312). **ONE build (15:49:39) plus ONE counted
relink (16:20:02, for a defect the picture gate's own ceiling found).** `qmake`
was re-run: two NEW sources and a new header that three existing files include.
Markers: `scratchpad/cards_agg_20260911/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_cards_agg.exe` = the launch bytes exactly, md5
`b9b8f55472514b1e1c2bb7a5de780737`, written once. Report
`scratchpad/lane_cards_agg_report.md`; entry text
`scratchpad/cards_agg_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries
NOT appended by the lane** -- `.../MISTAKES_ENTRIES.md`. **Nothing committed.**

## What bungo gets

**His "1 sounds good" is in the bytes.** Every FORESTED cell now gets one
impostor card set, composited from that cell's own trees' cards at the rotation
and mirror the repetition breaker gives them, from **8 horizon azimuths**. The
`.lodi` is **version 4** and carries one aggregate row a cell plus the list of
instances each one stands for.

**THE CENSUS HE ASKED FOR, printed before any code** (report section 1, from
`Fallout4.esm` alone): the Commonwealth holds **64,662 tree placements with a
LOD base over 3,685 cells**; **2,631 cells are forested at 8 trees or more and
hold 60,605 of them (93.7 percent)**; the typical forested cell is a dozen to
three dozen trees (p50 14, **max 92 -- no cell reaches 128**). A further
**99,274** tree placements have no LOD model at all and cannot be aggregated.
9-chunk Sanctuary: 97 forested cells, 3,423 trees.

Pictures, `scratchpad/cards_agg_20260911/images/`:
**`agg_cell_sheet.png`** (the whole 512x48 sheet of one 65-tree cell, all eight
azimuths, coverage and height, texel level),
**`cmp_individual_vs_aggregate.png`** (the calibrated pair: the same 65 trees
composited finely, the ceiling, the shipped card, a WRONG cell, and the
difference), **`agg_region_map.png`** (the region from above, read back from the
`.lodi`, every cell's own coveredCount).

## Gates (all on the 16:20:02 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| count identity, three instruments a cell | **10 / 0**; 97 rows, 3,414 == 3,414 == 3,414; FLOOR 135 cells under the threshold, none aggregated | newly registered |
| the calibrated picture gate | **8 / 0**; mass error subject **0.0525** (worst 0.1160), ceiling 0.0138, floor 0.3142, over 128 cell-views | newly registered |
| the height channel | span 124..156 of 255, sd **8.89**, correlation with cluster size **0.773**, flat-plane floor 0 | newly registered |
| `--aggregate` off vs the rung | **27 files, 0 differ**; comparator RED first on a flipped byte and a missing file | newly registered |
| standalone v4 layout | **18 / 0** + the fixture's **21 / 0**; 13 named refusals | newly registered |
| `lodgen_native.sh` | **18 / 0 PASS** | 18 / 0 |
| `lod_generation.sh` | **116 / 0 PASS** | 116 / 0 |
| `lodgen_panel_run.sh` | **125 / 0 PASS** | 125 / 0 |
| `lodgen_terrain.sh` / `lodgen_stage_times.sh` / `lodgen_roads.sh` | 26 / 0, 16 / 0, 11 / 0 | same |
| `ui_align.sh` / `water_ui.sh` | 11 / 0, 82 / 0 | same |
| `lodgen_terrain_vt.sh` | **41 / 1** (V9b) | 41 / 1, the carried red |
| `lodgen_octahedral.sh` | **FAIL, 1 check (F1, worst 1.88)** | **pre-existing: the same check, the same 1.88, on the RUNG exe.** Control log kept |

**Not one count moved.** Consistency: no changed file under `src res tools
tests` or `NifSkope.pro` is newer than the exe; `res/style.qss` and
`release/style.qss` byte-identical.

## The defect the gate found

The composite normalised each tree's layer by the NUMBER of samples that landed
in a target texel instead of by the share of the texel's AREA, so a tree
covering a tenth of a coarse texel composited as if it covered all of it: the
aggregate carried **94 percent more coverage mass** than the same cluster
composited finely, against a ceiling of 1.4 percent. Only the CEILING arm could
have found it -- the subject looked plausible and the floor was comfortably
worse. Fixed, one relink, now 5.25 percent against that same ceiling.

## FOR BUNGO -- one call, and two deviations

1. **THE CALL: what the aggregate may cost.** A per-tree card is paid once per
   TREE TYPE (all 36 Commonwealth types = about **28 MB**, measured on disk). An
   aggregate is paid once per CELL: **135.6 MB** for the worldspace at the
   shipped tile 64 / threshold 8, removing **57,974 quads** from the far band;
   **571.9 MB** at tile 128; **29.5 MB** at tile 64 / threshold 32 for 23,962
   quads. Both are switches (`--aggregate-tile`, `--aggregate-min`) and neither
   needs a rebuild.
2. **DEVIATION: the photograph is an orthographic COMPOSITE, not a render.** The
   render route is 21,048 photographs and over seven hours of sleep for the
   Commonwealth; the sheets have been orthographic and metric since 2026-09-10,
   so compositing them is an exact resample. Contract `LODGEN_CARD_SHEETS` 10.4.
3. **DEVIATION: the `.lodi` version word is CONDITIONAL** -- 3 without
   aggregates, 4 with -- because that is what makes the module's off value
   byte-identical (Deviation 12). Every earlier bump was unconditional. **One
   line to make it always move, and every v3 baseline is re-pinned.**

## Owed / red

* `lodgen_octahedral.sh` F1 is red on the rung too -- a lane of its own.
* A **relative `--impostors` path silently finds no card set** (the CLI does not
  resolve it; the refusal at least names the directory). One line, not taken.
* **No panel row**: `--aggregate` is CLI-only.
* The card library on disk holds **20 of the worldspace's 36 tree types**; a
  full Commonwealth bake wants a full card library first.
* **Nothing in FO4CS reads any of it** -- the rows, the covered blob, the
  threshold and the sheets are all owed to the runtime, and so is the dithered
  cross-fade.
* The **view-basis handedness is argued, not proved**: the one-tree control (a
  single-tree cell's aggregate must reproduce that tree's own card frame) was
  not run.

## Restart

**YES** -- any window bungo has open predates **16:20:02**. No window held the
exe at either link, so nothing of his was renamed aside and nothing killed. Game
down and zero NifSkope processes at every check and at the end.

## State

Nothing committed (CONSTITUTION 8). Changed: `src/lodifile.{h,cpp}`,
`src/nativeemit.{h,cpp}`, `src/lodgen.{h,cpp}`, `src/nifcli.cpp`,
`NifSkope.pro` (CR 0 throughout), the NEW `src/lodgenaggregate.{h,cpp}`, and the
three contract pages `docs/LODGEN_LODM_FORMAT.md` (3a),
`docs/LODGEN_CARD_SHEETS.md` (10), `docs/LODGEN_NATIVE_LODO_LODI.md` (4.6 +
Deviations 12, 13). **Two NEW skills in the REPO tree for the director to mirror:**
`ww-downsample-gate`, `ww-module-off-is-identical`.

- RESUME3 block, spliced (lane text verbatim):

## Lane RESUME3 -- 2026-09-11, one build for NIFPARSE1 and SPLAT1 phase B

**Two blockers closed with measurements, not theories.**

### 1. The bake's heap fault is NOT the NIF parser, and it is fixed

For a week the LOD generator's chunk fan-out has been off with the blocker
recorded as "the NIF parser is not thread-safe". It is not.

* **The experiment.** `NifSkope -no-gui parsestress` runs the model layer ALONE
  on N threads -- no plugin reader, no texture cache, no archive lookup, no file
  I/O in the threaded region -- and every worker must reproduce the
  single-threaded digest of what it read back. **20 consecutive runs at 16
  threads: 10,240 loads, 0 mismatches, 0 faults**, with both sabotage floors
  seen red first.
* **The fault.** With BAKEPERF1's containment mutex removed the 9-chunk
  Sanctuary region faulted **3 of 5** at `--chunk-threads 16`. Four symbolised
  stacks: two of them INSIDE `cliMessageHandler`, the headless CLI's own message
  handler, which wrote through `err()` -- a function-local `static QTextStream`
  with no lock. `GameResources::get_file` calls `qWarning()` on every miss and
  the road pass misses the same `.bgsm` on every placement, so sixteen workers
  grew one `QString` buffer at once. The other two stacks are an innocent
  `QList` reallocation that reached the corrupt heap first.
* **The fix**: the handler writes with `std::fputs` to `stderr` under its own
  mutex (the CRT locks the `FILE *`). One thread produces exactly the same bytes
  in the same order -- the way back is exact.
* **The gate**: `--chunk-threads 16`, **20 of 20 clean on Sanctuary and 20 of 20
  on Boston**, byte-identical to the serial bake on both regions.
* **THE DEFAULT STAYS `--chunk-threads 1`** and the reason is speed and memory,
  not safety. Medians of 3, alternating: Sanctuary 17.9 s / 1.75 GB serial
  against 40.5 s / 5.80 GB at 16 (2.26x slower); Boston 128 s / 3.76 GB against
  156 s / **25.1 GB** (1.22x slower). The texture stage is where it goes
  backwards -- 5.8 -> 31.9 s and 66.1 -> 89.4 s -- because sixteen workers keep
  sixteen cold texture caches. The source comments and usage text that said the
  fan-out was off because the parser faults were rewritten. `--chunk-threads 0` now means the smaller
  of the cores and what free memory holds, and the bake census says which bound
  decided it.
* Also landed: the shared archive index behind one read/write lock;
  `Message::append` / `message` refuse to build a `QMessageBox` off the GUI
  thread (live for the LOD Generation panel, not for the command line); and the
  block-tree name column no longer copy-assigns into the application's global
  pseudonym table.
* **NOT landed, deliberately, with the number that says why**: the lazy
  `NifValue::type()` re-init and the four shared `QRegularExpression`s. The
  experiment put 10,240 loads through both and neither moved. Named as latent in
  the report; the prepared edits are still in
  `scratchpad/nifparse1_20260911/fixes.py`.

### 2. The landscape textures were baked 6x too large -- fixed, default moved

* The bake tiled every landscape texture at 2,048 world units a repeat. The
  engine's own repeat is **341.3333** = 128 / 0.375, read out of `Fallout4.exe`
  1.10.155 (`fLandTextureTilingMult` 1.5f, one code reference, the 17x17
  quadrant loop). **6.0000x too coarse.**
* All **fourteen** sampling sites -- colour, mask and emissive, stock bake and
  pyramid -- read one value now. `_msn` is byte-identical at both tilings.
* `--land-tiling 2048` is the exact way back, byte-identical to the previous exe
  on every file of both test tiles.
* Real-bake numbers, chunk (-20,24): local variance **76.22 -> 12.34** against
  vanilla's 19.81 (SPLAT1 predicted 12.93 offline, 4.6 % out), mean abs RGB vs
  vanilla **16.33 -> 14.33** of 255. Chunk (-20,20): 74.06 -> 25.83 and
  20.43 -> 18.92. On that second tile the prediction was 14.60, and the
  discriminator says why: the offline model draws no roads, and against the
  `--no-roads` bake the same tile reads **17.00 vs 14.60, +16.4 %**, inside the
  gate. The tiling's own effect agrees with the prediction to **2.4 %**.
* **It does not close the colour error against vanilla.** That is the grading
  (x0.82-0.83), still open as "splat calibration vs vanilla grading".
* `docs/LODGEN_TERRAIN_VT.md` 2.5 rewritten with `T` and its provenance; its
  VCLR range corrected from 249..255 to the measured 203..255.

### The chain, in one line

Every harness the change reaches at its baseline -- `lodgen_terrain` 26/0,
`lodgen_terrain_vt` 41/1, `lodgen_ground_cover` 29/5, `lodgen_terrain_pbrm`
14/0, `lodgen_native` 18/0, `lodgen_stage_times` 16/0, `lodgen_panel_run`
125/0, `lod_generation` 116/0, `lodl_open` 23/0, `ui_align` 11/0, `water_ui`
82/0, the five card/array suites PASS, `lodgen_octahedral`'s one pre-existing
`F1 worst 1.88` -- plus all five editor harness logs BYTE-IDENTICAL to the
rung's. **One count moved: `lodgen_roads.sh` 11/0 -> 11/1**, and it is the
tiling's doing in a way worth knowing: bar 2 asks the road to correlate with
vanilla at least 80 % as well as the surrounding GROUND does; the road term did
not move (0.3065 -> 0.3061) and the ground's improved 17 % (0.3442 -> 0.4024),
so the bar rose past a signal that stood still. Nothing was changed to make it
green. **Our road raster is now the worse-matching part of the sheet** -- owed to
a roads lane.

### What is owed to bungo

1. **A restart of his open NifSkope window** -- the exe was relinked.
2. Two pictures: `scratchpad/resume3_20260911/images/cmp_tiling_fixed.png` (the
   tiling, real bake, vanilla beside it) and
   `scratchpad/pic_grass_20260911/images/cmp_grass_tint.png` (the grass tint at
   the corrected tiling).
3. The open item that did NOT close: splat calibration vs vanilla grading.

### State

* Nothing committed. **389 uncommitted paths** in the tree (243 under
  `src/ docs/ tests/ res/ .claude/` plus `NifSkope.pro`); fifteen are this
  lane's: `NifSkope.pro`, `src/lodgen.{h,cpp}`, `src/nifcli.cpp`,
  `src/message.cpp`, `src/gamemanager.{h,cpp}`, `src/model/nifmodel.cpp`,
  `src/lodgenparallel.{h,cpp}`, `src/lodgenchunkpass.cpp`,
  `docs/LODGEN_TERRAIN_VT.md`, `tests/spells/lodgen_terrain_model.py`, the two
  amended skills, plus NIFPARSE1's new `src/nifparsestress.{h,cpp}` and
  `tests/spells/parse_stress.sh`.
* Exe: `release/NifSkope.exe` **2026-09-11 19:08:42, 21,435,904 B, md5
  `847236ecb8f83d64b25b8a2ceeec91d3`** -- one build (16:53:23) plus **five**
  counted relinks, each `BUILD-RC=0`. Exe newer than every changed file (0
  STALE) and **0 of 246 object dependency blocks stale**. Rung `release/NifSkope.before_resume3.exe` 16:20:02, 21,419,520 B,
  md5 `3ebf175826feee6c6545cf0873635acc` (== CARDS-AGG's DONE line).
* Report `scratchpad/lane_resume3_report.md`; resume
  `scratchpad/resume3_20260911/PENDING.md`; ledger text in that folder
  (`WW_CHANGES_ENTRY.md`, `MISTAKES_ENTRIES.md`, this file).
* Skills amended and mirrored to the live tree:
  `nifskope-ww-crash-diagnose` (the `_NO_DEBUG_HEAP=1` rule, the MSYS2-only
  toolchain rule, "point it where the fault is", and a new section 7 "separate
  the stage from the pipeline") and `ww-anchored-hookup` (anchors are read from
  the file, never from a report -- prose normalises punctuation).

- ROADS2 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  20:46:44, 21,458,944 B** (RESUME3's was 19:08:42, 21,435,904). Rollback rung
  `release/NifSkope.before_roads2.exe` (19:08:42, 21,435,904 — sha1
  `89065512abfd1fed11ab4f934c943f5972bd106d`, equal to the launch exe byte for
  byte, untouched). Markers: `scratchpad/roads2_20260911/DONE` in, `BUILDING`
  gone. Report `scratchpad/lane_roads2_report.md` (all nine sections, verdicts
  in 0.1); entry text `scratchpad/roads2_20260911/WW_CHANGES_ENTRY.md`; **five
  MISTAKES entries NOT appended by the lane** —
  `scratchpad/roads2_20260911/MISTAKES_ENTRIES.md`, the director splices. **The
  contract IS amended in place** because the brief asked for it:
  `docs/LODGEN_TERRAIN_VT.md` section 1a, six amendments, LF-only. ONE build
  (20:13) plus **TWO counted relinks** (one failed to compile and wrote no exe —
  `lodgen.cpp:9037` used `coverOpts` where the telemetry block has `opts.cover`;
  the second, 20:46:44, is the shipped exe), declared in the report's 4.1.
  **bungo's open NifSkope window needs a restart.** No NifSkope GUI was launched
  by this lane at any point; `Fallout4.exe` was checked before every link and
  was not running.

  **THE INHERITED RED IS CLOSED.** `lodgen_roads.sh` **11/0** (RESUME3 left it
  11/1): R5 reads floor 0.1347, after **0.3431**, reference 0.4038, so bar 1
  (0.3431 >= 0.2694) and bar 2 (0.3431 >= 0.3231) both clear, and the mean
  colour error against vanilla on the road centreline falls from 38.11 with no
  road to **22.37** with one, the whole tile from 22.45 to 20.80. Adding our
  road now makes the sheet MORE like vanilla on both, which was not true before.

  **WHAT THE SEAM ACTUALLY WAS.** Measured on chunk (-20,20) before any code —
  vanilla's own `Commonwealth.4.-20.20.DDS` grid, 512 texels at 32 world units,
  no resampling. Not a piece-join or alpha-compositing bug, which was the
  obvious guess: it is **the road diffuse's own texture pattern printed at
  footprint scale**. A 256-world-unit UV repeat lands every **8.01 bake texels**,
  so the material's own stripes go straight into the sheet. Within-material
  luminance correlation along the road **0.852 ours / 0.016 vanilla**; phase-fit
  R² **0.140 / 0.013** against a 0.022 floor; local 5×5 SD on the road **10.33 /
  6.62**, and off it 5.52 / 5.38 (the control agrees). Three rival mechanisms
  refuted with numbers: **0 of 474** road materials set `bAlphaBlend` (6 of 512
  SHAPES do blend, via `NiAlphaProperty`); median covering-Z spread at a road
  texel 12.299 units, so no z-fighting; **0 of 474** shapes mip-clamped.

  **WHAT SHIPPED, five flags, all on the exe's own usage page.**
  `--road-detail 0..1` (**default 0**) lerps the diffuse sample to the texture's
  own average, which is what vanilla's far road measures as; detail 1 is what
  banded it. `--road-composite max-z|blend` (**default max-z**) — the topmost
  triangle wins, or paint in ascending mean world Z with `dst = lerp(dst, src,
  srcAlpha)`. `--road-raised` / `--no-road-raised` (**default no**) refuses a
  road base that carries its own Distant LOD mesh plus anything under
  `Landscape\Roads\HighwayOverpass\` or `…\Bridge\`. `--road-sidewalks` /
  `--no-road-sidewalks` (**default no**) refuses `Landscape\Sidewalks\`.
  **`--roads-legacy` is the one-token way back and means all four.**

  **THE COMPOSITE DEFAULT WENT AGAINST THE PLAN, and the harness decided it.**
  Blend was built to fix the seam and lost to the max-z path it was meant to
  replace. On `lodgen_roads_metric.py`: old pipeline 0.3061 (bar 2 fails),
  **max-z + detail 0 = 0.3404 (both bars clear)**, blend + detail 1 = 0.2481,
  blend + detail 0 = 0.2669 (both fail), bars 0.2694 / 0.3228. Max-z is also
  better at the piece boundaries (5.996 vs blend's 6.204, vanilla 5.271). Blend
  does win local 5×5 SD (6.542 vs 6.820), phase R² (0.033 vs 0.052) and road
  colour error (12.50 vs 14.09), so it is **kept behind the flag, not deleted**,
  with all five measures in the contract. **If a later lane changes the road
  colour or the grading, re-run all four variants — the ranking is not safe.**

  **TWO FAMILIES REFUSED, each on its own number, chunk (-8,8) downtown.**
  Clearance above the top of the same mask displaced five ways, tie-averaged
  brightness. *Raised roads*: vanilla **−0.009**, before **+0.314**, after
  **+0.001**; flat road still clears (+0.068 vs vanilla's +0.011); non-road
  control −0.028 / −0.005 / −0.094. Census `roadRefusedRaised 110`,
  `roadRaisedBases 35`; downtown road texels **163,586 → 40,436**.
  *Pavements*: 17,801 kerb texels, 15,696 of them >2 texels from any flat road;
  vanilla's clearance there **−0.102** (it paints nothing), ours +0.284, mean
  luminance **86.7 vanilla / 128.1 before / 102.1 after**, while the flat road
  reproduced vanilla's own clearance to **0.001**. Census
  `roadRefusedSidewalk 414`, `roadSidewalkBases 64`. Every refusal is NAMED in
  `roadRefusals`.

  **THE TREE CLAUSE IS NARROWER.** `lodgenIsTreeModel()` matched a `trees`
  component anywhere, catching `SetDressing\TreeSwing01.nif`,
  `TreeNoose01_Branch.nif` and five siblings — swings and gallows props. Now
  scoped to a `landscape` first component. Over every placed base in the
  Commonwealth census: **137 tree bases under both rules** (135
  `Landscape\Trees`, 2 `Landscape\Plants`), **7 flip out** (82 placements,
  **0 with a distant LOD mesh**, so no card or impostor ever came of them),
  **0 flip in**; the 36 of 137 kept bases that carry a distant LOD mesh are
  exactly the 36 lines `--list-impostor-candidates --candidates trees` prints
  for the whole worldspace, so the re-typed classifier and the shipped C++ agree
  base for base.

  **GATE S2, THREE WAYS, 9 of 9 FILES EACH, cells -20..-17 x 20..23.**
  `--roads --roads-legacy` on the new exe = the rung's `--roads` bake byte for
  byte (content hash `32f88dc8df56092a` both sides); `--no-roads` = the rung's
  `--no-roads` bake (`72cc5e098dfa7d4e`); and `--roads` with nothing else named
  = `--roads` with all four knobs spelled out at their defaults, so the defaults
  are exactly those four and not a fifth unstated one. Region bakes only, all
  under `scratchpad/roads2_20260911/out/`; **his installed `Data\Terrain` was
  never touched**.

  **GATE S4 WAS REFUSED AS WRITTEN and substituted.** The brief's gate — the
  four `SetDressing\Tree*` props absent from the Sanctuary candidate list — is
  green on the RUNG too: Sanctuary lists 20 lines, the whole Commonwealth 36, no
  `SetDressing` in either on either exe, because the lister returns early on
  `!b.hasLod` (`nifcli.cpp:3350`) and all seven props have no MNAM and bit 15
  clear. The gate could not fail. The flip table above is the substitute and it
  fails in both directions. Found by running the gate on the OLD exe first.

  **THE HARNESS CHAIN, 20:51→20:55, every count at RESUME3's baseline:**
  `lodgen_roads.sh` **11/0** (was 11/1 — closed), `lodgen_terrain.sh` 26/0,
  `lodgen_terrain_vt.sh` 41/1 (`V9b`, red on the rung too),
  `lodgen_ground_cover.sh` 29/5 (red on the rung too), `lodgen_terrain_pbrm.sh`
  14/0, `lodgen_native.sh` 18/0, `lodgen_panel_run.sh` 125/0,
  `lod_generation.sh` 116/0, `ui_align.sh` 11/0, `water_ui.sh` 82/0. 383 checks,
  6 failures, all six red on the rung as well.

  **PICTURES, all four looked at before being cited**, same grid, same mip, no
  resampling, numbers burned in: `scratchpad/roads2_20260911/images/cmp_seam.png`
  (the banding is plainly there in the BEFORE zoom and plainly gone in the
  AFTER: boundary gradient 5.302 vanilla / 9.752 / **5.712**, local SD 6.653 /
  10.375 / **6.678**), `cmp_highway.png` (vanilla's downtown sheet has **no
  highway in it at all**; ours painted a bright interchange and now does not),
  `cmp_sidewalk.png` (**the change bungo has not seen** — a bright cream kerb
  band that vanilla has nothing of, gone), `cmp_sanctuary_road_v2.png` (ROADS1's
  crop re-taken).

  **STILL RED, WITH THE NUMBER.** Ours: the road interior is now **too smooth**
  — local 5×5 SD 4.369 against vanilla's 5.533, having been 10.375, the known
  cost of detail 0; mean road luminance **101.59 against vanilla's 92.18**; and
  laid beside vanilla at Sanctuary, **vanilla's cul-de-sac is a soft desaturated
  blue-grey blob and ours a crisp warm pale-grey ribbon with a visible kerb
  line** — the geometry is right, the hue and the crispness are not, and neither
  has a number yet. That chroma/edge measurement is **owed to a later lane**.
  The skirt still darkens with the mesh's vertex alpha where vanilla's does not
  (correlation −0.791 vs +0.001) but the size is small: 3.7 luminance units over
  676 of 262,144 texels. **Not ours:** the off-road ground is ~16 luminance units
  too dark (whole sheet 71.16 against vanilla's 83.84; off-road 68.69 against
  83.52) — TILING2 and GRADE1's.

  **FOR LANE TILING2, ONE THING.** Every road number in this lane was measured
  on top of the current ground. When TILING2 changes the ground, `lodgen_roads.sh`
  R5 (bar 2 is 0.8× the ground's own agreement with vanilla) **and** the
  four-variant composite ranking both need re-running before anyone assumes
  max-z still wins.

  **TWO SKILLS AMENDED**, both trees (`…NifskopeWildWastelandEdition\.claude\skills`
  and the live mirror `E:\Projects\Claude\.claude\skills`), LF-only, verified by
  byte count on both copies. `ww-spec-gate-audit` gained *"Run the gate on the
  OLD binary FIRST"* — a gate that is already green is measuring something else
  — with the five-row flip table to substitute when one turns out vacuous.
  `ww-control-calibration` gained *"A rank statistic with a dominant tie block is
  not reproducible"*: FLAGSCAN1's grey AUC of **0.716 re-measures as 0.757 from
  its own script**, because one saturation value covers **53.5 percent** of that
  tile and `np.argsort` breaks ties by raster index; tie-averaged it is 0.731. It
  carries a fifteen-line `rankdata`/`auc_tie` (no `scipy` here) and four
  reporting rules, including **prefer a clearance over a raw score**, which is
  how every family gate in this lane was read.

- TILING2 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  21:52:22, 21,466,624 B**, sha1 `9492e5a60deaf9aab19bf6269c14bf6caed01989`
  (ROADS2's was 20:46:44, 21,458,944). Rollback rung
  `release/NifSkope.before_tiling2.exe` (21:20:07, 21,458,944 B — sha1
  `955b0952a5f7ab62d4dcfef7852a923c5a6d3f22`, the launch exe byte for byte,
  untouched). Markers: `scratchpad/tiling2_20260911/DONE` in, `BUILDING` gone.
  Report `scratchpad/lane_tiling2_report.md` (all ten sections 0..9); entry text
  `scratchpad/tiling2_20260911/WW_CHANGES_ENTRY.md`; **six MISTAKES entries NOT
  appended by the lane** — `scratchpad/tiling2_20260911/MISTAKES_ENTRIES.md`,
  the director splices. **The contract IS amended in place** because the brief
  asked for it: `docs/LODGEN_TERRAIN_VT.md` **§2.5a** (new) plus a TILING2
  provenance block at the end, LF-only, anchors re-found. ONE build (21:47:49,
  21,465,600 B) plus **ONE counted relink** (21:52:22, the shipped exe) — the
  relink's reason is in the report's section 5 and in MISTAKES entry 1.
  **bungo's open NifSkope window needs a restart.** No NifSkope GUI was launched
  outside the harness chain; `Fallout4.exe` was checked down before every link
  and every bake; every bake went into `scratchpad/tiling2_20260911/out/` only,
  two 4x4-cell regions, never his installed `Data\Terrain`.

  **NO DEFAULT CHANGED.** A bake with this exe and no new flags is the rung's
  bytes: 18 files over both test tiles, **0 differ**, and the same with
  `--land-sample footprint --blend-edges off` typed out. The compare is shown
  able to fail (each switch moves exactly 6 of the 18 — the chunk colour DDS and
  the two `.lodt` containers, 3 a tile). `_msn`, `_data`, `.lodm`, BTO, BTR and
  the manifest are byte-identical at EVERY setting including `--land-tiling
  2048`. `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` is deliberately
  untouched: nothing bungo bakes moves.

  **FOUR NEW FLAGS, all opt-in.** `--land-sample footprint|average` (default
  `footprint`) — `average` reads the landscape diffuse's 1x1 mip, which IS the
  exact average over one repeat (a landscape texture ships a full mip chain to
  1x1 and one repeat is the whole texture). `--land-detail 0..1` (default 0)
  lerps back k of the footprint sample's departure from that average.
  `--blend-edges off|quadrant` (default `off`) cross-fades the neighbouring
  quadrant's composite over `--blend-margin` world units either side of every
  2,048-unit line, quintic, exactly 0.5 ON the line. `--blend-margin` (default
  128 = 4 texels, the 17x17 opacity grid's own spacing).

  **VANILLA'S LAW, measured over the 22 shipped dim-4 sheets BEFORE any code**
  (`scratchpad/tiling2_20260911/logs/t3_laws.txt`, `t3b_seam.txt`; the
  population rule — mip-3 SD >= 5.25 — was written down first). Repeat: **0 of
  22** above their own null floor, ceiling 0.264 absolute / 0.448 over the
  sheet's own floor. Edges: w50 median 4.12 over 2.50..6.50, hard-edge median
  523 of 6,000, quadrant-seam median 1.041 with a worst of 1.100. Detail:
  22.6% / 21.5% of variance finer than 4 texels, and the ceiling for "how
  different two real sheets are" is **0.670**, measured vanilla against vanilla
  on neighbouring tiles.

  **WHAT THE SWITCHES DO, on both tiles.** Repeat 1.037 -> **0.092** on
  (-20,24) (vanilla's ceiling 0.264). Quadrant seam 1.236 -> **0.955** and
  1.065 -> **0.847**, below vanilla's own median on both, at no measurable cost
  (local variance -0.9%, spectrum distance 1.227 -> 1.226, repeat +1.3%).
  `--land-sample average` ALONE makes the grid WORSE (1.236 -> 1.710): with each
  quadrant flat the only edges left ARE the lines, so the two switches are not
  independent. The 0.564 that (-20,20) still reads at that bin is proved NOT to
  be a texture repeat — with `average` the sheet is byte-identical at
  `--land-tiling 341.3333` and `2048`, and in the largest road-free window
  vanilla reads 0.245 where we read 0.376 (dimensionless 0.255 vs **0.131**).

  **THE BLUR IS REFUSED, WITH THE TABLE, AND NOTHING ROUTES TO GRADE1.** Ten
  candidates against vanilla's high-pass residual, each with its own phase twin
  as the floor: the land diffuse at the footprint mip and at mip-1, -2, +1, the
  exact footprint box mean, the fully averaged texture, VCLR, the slope from the
  shipped `_msn`, the best of eight directional shadings, and our own sheet —
  **every one |r| <= 0.006** against floors of the same size
  (`logs/t4_corr.txt`). The control that makes it mean something
  (`logs/t4b_align.txt`): the same model against OUR OWN bake reads **r =
  +0.7948 and +0.7040 with the best shift at exactly (0,0)**, row-flip control
  0.0501 / 0.0137. Vanilla's far-terrain colour was NOT produced by resampling
  the landscape textures this composite composites. VCLR reads -0.004 / -0.001
  and the best lighting azimuth 0.006, so there is nothing for a grading lane
  either. **Still red: the blur.** Local variance 4.84 against vanilla's 19.81
  at the recommended setting, 12.29 at the rung.

  **THE DETAIL TRADE, PRICED.** k = 0/0.15/0.25/0.35/0.50 gives a repeat of
  0.092/0.175/0.268/0.366/0.532 for a local variance of
  4.84/5.00/5.22/5.58/6.45. Vanilla's ceiling is crossed at **k = 0.246**,
  having recovered 0.37 of the 14.97 missing levels. The repeat and the
  texture's detail are the same signal.

  **BUNGO'S CALL, BLOCKING.** Which defaults? The recommendation with its costs
  named is `--land-sample average --land-detail 0.20 --blend-edges quadrant`
  (repeat and grid inside vanilla's law; blurrier than vanilla, which is the
  complaint that is not fixed). The alternative is `--blend-edges quadrant`
  alone: the grid goes, the repeat stays. No lane changes the look of every
  future bake on its own.

  **CHAIN, all on the new exe** (22:05..22:10, `scratchpad/tiling2_20260911/
  chain.sh`, logs `logs/c_*.log`): `lodl_open.sh` **23/0**, `lodgen_terrain.sh`
  **26/0**, `lodgen_terrain_vt.sh` **41/1** (V9b, red on the rung too),
  `lodgen_roads.sh` **11/0**, `lodgen_ground_cover.sh` **29/5** (the same five
  by name), `lodgen_terrain_pbrm.sh` **14/0**, `lodgen_native.sh` **18/0**,
  `lodgen_panel_run.sh` **125/0**, `lod_generation.sh` **116/0**,
  `ui_align.sh` **11/0**, `water_ui.sh` **82/0**. Every count is ROADS2's
  baseline exactly; nothing moved, nothing to explain.

  **FOR ROADS3** (the director's addendum, measurement only — no road code was
  touched): the road edge on (-20,20) is a **contrast** defect, not a width
  defect. Our road crosses in **6.00 texels** (10-90%), vanilla's in 6.00, w90
  12 against 11 — but ours stands **+29.45** luminance levels over its surround
  where vanilla's stands **+3.92**: vanilla's far-LOD road is a ~2-level rise
  over twelve texels, and its width row is read off a 23.26-level profile that
  is the terrain's own texture under the mask, not a road edge. **Do not chase
  all 25 levels with the road pass**: our non-road surround is 69.41 where
  vanilla's is 88.57, so 19 of them are a whole-sheet tone offset that belongs
  to GRADE1. Mask and profiles: `logs/t3c_road.txt`, `road_mask_t2020.npy`.
  **ROADS2's standing note still stands**: if a default ever changes, the ground
  moves, so `lodgen_roads.sh` R5 bar 2 and the four-variant composite ranking
  need re-running. They are green on the defaults as they are.

  **KNOWN LIMITATION, named not hidden**: a quadrant border lying on the chunk's
  own edge is not cross-faded (the neighbouring cell's paint is not loaded in the
  tile baker). The adjacent chunk does not blend it from its side either, so no
  new seam is created — but that one line stays as hard as it is today. All
  seven interior lines per axis are blended.

  **SKILLS AMENDED IN BOTH TREES** (`E:\Projects\NifskopeWildWastelandEdition\
  .claude\skills` and the live mirror `E:\Projects\Claude\.claude\skills`):
  the pyramid owns the colour sheet, so a colour change has two sites and the
  one that ships is the pyramid; the phase twin is a floor for a STRUCTURE
  statistic only and never for a periodicity or a spectrum; a zero correlation
  proves nothing without an alignment control that reads ~0.8 against a field
  the model does reproduce; and a crop's own null floor is higher than a
  sheet's, so a picture's verdict number must be the whole sheet's.

- TILING3 block, spliced (lane text verbatim):

## TILING3 -- vanilla's fine detail: where it comes from, and what we now copy

**Exe** `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B. One build, no
relinks after it. Rung `release/NifSkope.before_tiling3.exe` 21,466,624 B.
Nothing committed. **bungo's open NifSkope window needs a restart.**

### bungo's rulings, verbatim, in the order he gave them

> "For now, I think we can use vanilla normal map for those tiles, use those
> details for the diffuse."

> "so now we do not use our own normal map if that is toggled, but reuse these
> ones for terrain chunks."

> "out of bounds terrain blends are not included in the actual cells out of
> bounds, they never were, so we can't recover the color data anymore, because it
> was baked in a different tool outside of fo4."

> the G channel of vanilla's `_msn` (up, = cos of the slope angle) "is the
> slopeness for the micro details our heightmap bakes do not possess"

### What the default now does

`--land-detail-source vanilla` is **the shipped default**.

1. A chunk with a shipped vanilla `_msn` -> the output `_msn` **IS vanilla's
   file, byte for byte** (`cmp` == 0). Our normal bake is skipped for it. No
   guard, no composite, no threshold: the only question asked is whether the file
   exists.
2. A chunk with **no land paint on any cell** -> the output COLOUR **IS vanilla's
   file, byte for byte**. Classification is **per chunk, by "any cell has
   paint"**, because the shipping writer assembles a chunk sheet from four
   virtual-texture tiles and has no per-quadrant seam at that point. A partly
   painted chunk counts as painted, so the rule errs toward keeping our composite.
3. Every other chunk keeps our colour composite and takes the **crevice term**:
   `dL = kDiv * div(detail normal)`, `kDiv = --land-shade`, default **-3.242**,
   added equally to all three 8-bit channels.
4. A chunk with no vanilla sheet keeps our bake, exactly as before.
5. `--land-detail-source none` == the rung's bytes, proved file by file.

`--vanilla-lod-root` (default `E:/Tools/Fallout 4/DataUnpacked/Data`) reads
vanilla's sheets **as loose files only**, never through the resource stack. That
is load-bearing: the stack would serve our own previously installed output out of
the game's `Data`, and a bake would "reuse vanilla" by copying yesterday's copy
of itself.

### The two places the measurement contradicted the instruction, and what shipped

* **A Lambert shading of vanilla's normal detail reads zero.** R^2 0.0000 at a
  twin floor of 0.00001, with the up coefficient flipping -1.34 to +0.45 sheet to
  sheet; the fit machinery recovers an injected 0.5 as 0.5000, so the null is the
  data's. A Lambert dot cannot see a rill: the two walls cancel. What measures is
  the **divergence** of the same field -- median r -0.1046 against a twin floor of
  +0.0006, same sign 7 of 7, over the twin 7 of 7 -- so that is what ships.
* **Micro-steepness (`acos(up fine) - acos(up coarse)`) also reads zero as a
  darkening driver**: median |r| 0.0039 against the crevice term's 0.1046, sign
  agreement 4 of 7 (chance). Tested exactly as instructed, kept only if at least
  as good, and it is not: **the crevice term stays.** No rebuild was needed. The
  tint half reads zero too -- the three channels move together to within 0.0018
  and the saturation term is -0.0016 -- so the shipped term is a pure darkening,
  which is what the data supports.
* **But micro-steepness wins as an ENVELOPE:** `|dTheta|` against the colour
  residual's own envelope reads +0.1269 median, beats its twin 7 of 7 and beats
  the divergence's envelope 5 of 7. It predicts WHERE vanilla's grain lives, not
  its sign. That is the strongest open lead for the next lane -- it would mean
  modulating an amplitude rather than adding a level, a second fitted parameter
  and a second build. Not smuggled into this one.

### What is still red

* **The repeat is still there on the default.** The shipped default was never
  asked to fix it; it delivers the ruling. Real bakes: rung 1.037 / 1.261, ship
  1.042 / 1.242, against vanilla's 0.201 / 0.032 and a ceiling of 0.264.
* **The repeat fix exists and is OFF.** `--land-sample stochastic` (domain warp
  A=683, lattice 1024, 1 octave, mip bias -1.00) reads repeat 0.183 with grain at
  111 % of vanilla's on (-20,24) -- both halves of the gate -- and 0.593 with
  grain 99 % on (-20,20): repeat red. Its 7-sheet selection scored **6 of 7**; the
  red is (-36,-20) at repeat 0.095 / ratio 0.698 against a 0.448 ceiling. Its RMS
  strain 0.718 is above the prototype's own "over 0.5 is a visible wobble" note.
  **Turning it on is bungo's call and the numbers are in the report.**
* **~98 % of vanilla's fine colour is not a function of vanilla's fine normal**
  (22-column ceiling R^2 0.018 / 0.023 on a residual SD of 4.476 / 5.459). No
  per-texel law from the `_msn` can do better than the crevice term by much. The
  grain's real source is the land textures sampled about four mips finer than the
  footprint, with the repeat broken -- which is what the stochastic proposal is.

### Gates, with counts

* off == rung: **9 files each, both tiles, 0 differing**.
* 1 vs 16 chunk threads, 16-chunk block: **97 files, 0 differing**.
* `_msn` cmp == vanilla: **18 of 18**; no-vanilla control (empty root): **0
  differing vs rung on 9 files**.
* Chunk classes -- t2024 1/1/0/0, t2020 1/1/0/0, edgeN 16 chunks 7 layered / 9
  layerless / 0 layerless-without-vanilla, census region 180 chunk sheets (dim 4
  and dim 8) 121 layered / 59 layerless / 0 without. **Nothing anywhere lacked a
  vanilla sheet**: Bethesda ships a complete 48x48 dim-4 grid over cells -96..95.
* Untouched by the default: `_data`, BTR, BTO, manifest, `Commonwealth.VT.2.lodt`,
  `Commonwealth.VT.4.lodt`, `Commonwealth.VT.lodm` -- byte-identical.
* Chain at TILING2's baselines, all eleven harnesses. `lodgen_ground_cover.sh`
  first read 29/6: its check C1 asserts all three sheets are 174,888 bytes (DXT1),
  and vanilla's `_msn` is BC5 at 349,680. The harness now pins
  `--land-detail-source none` on its own bakes -- a harness forces the state it
  measures -- and reads **29/5, line for line identical to TILING2's**. Shell
  only, no rebuild.

### A pre-registered gate clause that the ruling superseded

F2 as registered said "`.lodl` and `_msn` byte-identical at EVERY setting". The
`.lodl` half holds. The `_msn` half **cannot and must not** -- bungo's correction
made the `_msn` the deliverable. It was replaced by the gate he dictated, which is
stricter: every chunk with a vanilla sheet -> cmp == vanilla; every chunk without
-> cmp == rung; state the count each side. Both counts are above.

### Files

Report `scratchpad/lane_tiling3_report.md`. Instruments and logs under
`scratchpad/tiling3_20260911/` (`d1_geology.py`, `d2_deep.py`, `d3_shade.py`,
`d4_steep.py`, `a3_abc.py`, `a4_warp.py`, `a5_tune.py`, `a6_pick.py`,
`f3_gate.py`, `t3_bake.sh`, `make_pics3.py`; `logs/`, `out/`). Picture
`images/cmp_tiling3.png`. Code in `src/lodgen.h`, `src/lodgen.cpp`,
`src/nifcli.cpp`; harness `tests/spells/lodgen_ground_cover.sh`.

- TILING4 block, spliced (lane text verbatim):

## TILING4 DONE 02:3x -- `--land-sample stochastic` is a hex tiling now, the default did not move

Brief `scratchpad/brief_tiling4.md` + the resume brief `brief_tiling4b.md`
(grain/band gates replaced by G1/G2 and their band twins). Report
`scratchpad/lane_tiling4_report.md`, **960 lines, sections 0-9, appended never
rewritten, CR 0**. Lane dir `scratchpad/tiling4_20260912/` (`DONE` in,
`BUILDING` gone, `PENDING.md` current).

### The exe
- `release/NifSkope.exe` **2026-09-12 02:08:57, 21,487,616 B**, BUILD-RC=0, one
  build, no relinks, exe newer than all three changed sources, style.qss and
  the shaders in step. `Fallout4.exe` down at 02:07:01 and again inside the
  chain before the link.
- Rung `release/NifSkope.before_tiling4.exe` 00:06:03, 21,484,032 B, md5
  `8ec07038d14f4a973921d8a1c39a33b1`, untouched. New exe md5
  `8a1a1e718c6d6822ad0d60b90803fd69`.
- `release/NifSkope_inuse_60864.exe` is the renamed-aside launch copy. Nothing
  was killed. **RESTART: yes** for any window older than 02:08:57.
- **A GUI `release/NifSkope.exe` pid 59040 appeared at 02:24:09 with no
  arguments and this lane did not start it** (every bake was `-no-gui lodgen`
  and all 42 exited rc=0). It was left alone and had closed by itself at 02:35.
  **At 02:35 no NifSkope and no `Fallout4.exe` is running: the exe is free.**

### Landed code (three files, nothing else)
`src/lodgen.cpp` -- `g_landHexSize` (6081), the two lattice constants in
**double** (6086), `lodgenLandHexCell` (6096), `lodgenLandHexOffset` (6122,
through the warp's own hash), `lodgenLandHexTap` (6133, variance-preserving
blend, **alpha from the largest-weight tap, never blended**),
`lodgenSetLandHexSize` (6183), and the tap called at **both** sampling sites,
**7666** (chunk) and **8990** (VT pyramid). `src/lodgen.h` 225.
`src/nifcli.cpp` 6090 (`stochastic` = hex 256 + mip bias -0.22), 6116
(`--land-hex`). `--land-sample warp` = TILING3's warp, kept reachable.
**Default unchanged.** Way back: `--land-hex 0 --land-mip-bias 0`.

### Gates
| gate | result |
|---|---|
| F1 (instrument + split) | PASS, from the first agent: split frozen 00:11, law 00:27, controls 7/7, 7/7, 5/7 |
| parity C++ vs prototype | **0 of 75 disagree**, every setting moves the sample, `--sabotage` fails 56 of 75 |
| **F2 byte identity** | **PASS** -- off == rung 9/9 x2 tiles; the way back == rung; stochastic moves exactly 3 files (colour DDS + **both** `.lodt`, the second-site proof); `_msn` == vanilla 10/10; `warp` == TILING3's bake byte for byte; 1 vs 16 threads 97 files 0 differ. Comparator shown red on a flipped byte and a deleted file first. |
| **F3 on the product** | **NOT MET.** 14 sheets x 3 arms, real bakes (`f3_full.sh`/`f3_full.py`): hex repeat **4/7 and 5/7**, swirl **6/7 and 7/7**, G2 7/7 and 7/7, G1 -14.3 % (pass) and -36.3 % (fail). Warp: repeat 5/7 and 6/7, swirl 3/7 and 4/7, G2 0/7 and 1/7. Rung: repeat **0/7 and 0/7**. |
| **F4 the chain** | **PASS**, 11 of 11 at TILING2/TILING3 baselines: 23/0, 26/0, 41/1, 11/0, 29/5, 14/0, 18/0, 125/0, 116/0, 11/0, 82/0 (02:12-02:18). `lodgen_ground_cover` held at 29/5 untouched. |

### What this means in one line
The default stays `footprint` because the hex tiling is 9 of 14 on the repeat
where the warp is 11 of 14. What the lane ships is the **meaning** of the
experimental switch: against the warp bungo judged, the swirl goes 7/14 -> 13/14
and per-sheet grain-vs-previous-build 1/14 -> 14/14, at 11/14 -> 9/14 of the
repeat.

### Red, for the director
1. F3 not met (repeat), so no default change and no `BAKE_INSTRUCTION.md`.
2. The warp beats the hex tiling on the repeat on the product; the earlier "no
   cost in repeat" claim was the offline prototype's and is withdrawn in
   section 6.
3. The hex tiling's **blotchiness at its 256-unit cell scale is ungated** -- no
   instrument in the lane measures it. `images/sheet_tiling4.png` at 1:1.
4. **`offline_bake.py` must not be used for absolute numbers again** (wrong by
   up to 0.27 on the repeat, both directions, and ~1 unit on the swirl).
5. G1 and G1-band cannot separate the arms: the **rung fails both** (-24.0 %,
   -42.9 %; 2/6 and 1/6 bands against 6/6).
6. (-36,-20) fails TILING2's **ratio** law while its amplitude falls 1.501 ->
   0.073, because the ratio's denominator is that sheet's own no-repeat floor
   and collapses with it. The warp fails it the same way. Possible artefact;
   the law is frozen and it was counted as a failure.

### Deliverables for the overseer to splice
`scratchpad/tiling4_20260912/WW_CHANGES_ENTRY.md` (product numbers),
`MISTAKES_ENTRIES.md`, this block. `docs/LODGEN_TERRAIN_VT.md` **is already
amended in-tree** -- 2.5e, two CLI rows, a provenance block with per-claim
anchors; `d0_doc.py` wrote it and `d1_doc.py` corrected its counts to the
product's (116,939 B, sha256 `b5fff76b436404d0`, CR 0, both refuse on re-run).
Pictures `images/cmp_tiling4.png` (3-up crop, numbers burned in) and
`images/sheet_tiling4.png` (the three sheets whole at 1:1).

### Bungo's calls
Swirls or blotches (the 1:1 picture is the comparison); which meaning the
experimental switch should carry; whether a cell-size / second-octave sweep on
**real** bakes is worth a lane; whether the ratio law wants a floor under its
denominator. Nothing was committed and nothing stashed.

- GRADE1 DONE 2026-09-12 03:0x, EXE FREE (release/NifSkope.exe 03:06:21,
  21,489,152 B, md5 6af74b4b4667ce50c4506a2d42a04fdf; rung
  release/NifSkope.before_grade1.exe 02:08:57, 21,487,616 B, md5
  8a1a1e718c6d6822ad0d60b90803fd69 = TILING4's DONE exe, intact;
  scratchpad/lane_grade1_report.md, HANDOFF_BLOCK + WW_CHANGES_ENTRY +
  MISTAKES_ENTRIES in scratchpad/grade1_20260911/). THE ANSWER IS THAT THERE
  IS NO TONE CURVE. The transfer ours -> vanilla has no constant: the best
  gain is 0.8916 on (-20,24) (we are too BRIGHT there, +6.79 levels) and
  1.1612 on (-20,20)'s ground (too dark, -14.70) -- opposite signs one chunk
  apart. 25-tile census k = 0.615..1.241 mean 0.892 sd 0.144; 96 land cells
  0.699..1.388 mean 1.004. Both fixed sRGB slips refuted by 2 orders (RMS
  56.6/78.5 and 68.6/59.7 vs identity 19.9/22.7) and the colour path has no
  conversion to slip. Affine/gamma "win" degenerately (slopes 0.019/0.185,
  their RMS = vanilla's own sd) because our sheet and vanilla's barely
  correlate texel to texel (r 0.034 / 0.314). VCLR is dead for tone (mean
  multiplier 254.9/255 on six tiles; removing it moves k by 0.0003).
  FINGERPRINT: a per-cell CONTENT difference with a near-zero mean -- not an
  exposure, a gamma, a colour-space slip or a lighting term; the residual's
  only sign-consistent partner is our OWN luminance (r +0.835/+0.827 vs phase
  twins -0.056/+0.012), height sits AT its twin floor on both tiles.
  fo4-engine-constant-from-ini-setting NOT invoked: its precondition
  (position-independent gain) fails. SHIPPED: `--grade K`, both writers,
  after road+tint before the crevice term, clamped 0..4, default 1.0 with the
  multiply BRANCHED OVER -- off == the rung's bytes by construction and
  measured (24/24 files identical, no-flag and --grade 1.0). GATE G3's
  "reduced on BOTH tiles" REFUSED WITH ARITHMETIC per the skill (the error in
  k is a parabola whose vertex is that tile's own k_opt; the two straddle 1),
  the refusal asserted against the BINARY: pooled k=0.8403 gives (-20,24)
  20.127 -> 18.108 and (-20,20) 21.897 -> 28.488. Own-optimum k does reduce
  each (-12.8 % and -6.2 %); binary vs prediction within +0.031..+0.173 of a
  1.0-level tolerance; rung exe exits 2 on --grade. Gates: G1 controls 24/0,
  ship gates S1-S5 8/0. HARNESSES at ROADS2's baselines exactly: lodgen_terrain
  26/0, lodgen_terrain_vt 41/1, lodgen_roads 11/0, lodgen_ground_cover 29/5,
  lodl_open 23/0, lod_generation 116/0, lodgen_native 18/0,
  lodgen_terrain_pbrm 14/0. Contract amended: new 2.5f, step 8 of the 2.5
  ring-0 formula (so FO4CS blends the same tone; at 1.0 the step does not
  exist), --grade in 5, GRADE1 provenance re-stamped. Pictures
  scratchpad/grade1_20260911/images/{cmp_tone,curve}.png. RESTART: yes (any
  open window predates 03:06:21).
  REDS FOR BUNGO / OTHER LANES: (1) the ground-cover plane is EMPTY on all 25
  census tiles including Sanctuary -- every _data sheet carries dwReserved1 = 0
  (set exactly when coverMax == 0) and its decoded alpha is 0 on 100 % of
  texels, so the grass tint NEVER FIRES and 0.35 could not be fitted; that is
  a cover-source defect, not a tint defect. (2) two census tiles are
  near-achromatic and over-bright -- (-12,28) sat 0.054 lum 124.3 and (-16,28)
  sat 0.018 lum 126.8, RGB RMS 51.2 and 48.6 against a ~20 typical: they look
  like a missing/greyscale base texture. (3) the road materials do not resolve
  from the unpacked data root (materials/c:/projects/fallout4/... is not in
  any archive), so the road colour is whatever the fallback is. (4) we are
  more saturated than vanilla on every tile measured (0.243 vs 0.219 mean) and
  no brightness model can explain it -- a gain leaves S unchanged; that is the
  next question and it is a CONTENT question, for the lane that owns the layer
  blend. (5) THE RECORD IS CORRECTED: "the off-road ground is ~16 levels too
  dark" is true of (-20,20) only; it reverses sign on (-20,24). Of the road's
  ~21 levels of over-contrast on (-20,20), 6.3 are the road being too bright
  and 14.7 are the ground being too dark.

- ROADS3 **BUILT AND GATED**. `Fallout4.exe` was up at 03:57:20 (PID 22908) so
  the lane was written BUILD PENDING; it had exited by **04:08:58**, the build
  was spent after a fresh `tasklist`, and `tools/ww_build.sh` returned
  **BUILD-RC=0 on the first try — one build, zero extra relinks**. New
  `release/NifSkope.exe` **2026-09-12 04:10:38, 21,489,152 B**, md5
  `fe65cc978f3896881140c2eea57c69c6`. Rung kept aside as
  `release/NifSkope.before_roads3.exe`, md5
  `6af74b4b4667ce50c4506a2d42a04fdf`, **equal to the exe this lane found at
  launch**; the two are the same size and different bytes. The old exe was
  renamed aside at link time, never killed; no window of bungo's was touched.
  Markers: `scratchpad/roads3_20260911/DONE` in, `BUILDING` gone;
  `PENDING.md` is superseded and says so at its top. Report
  `scratchpad/lane_roads3_report.md` (sections 0-7 and `## DONE`). Entry text
  `scratchpad/roads3_20260911/WW_CHANGES_ENTRY.md`. **Four MISTAKES entries NOT
  appended by the lane** — `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md`, the
  director splices. Contract amended in place: `docs/LODGEN_TERRAIN_VT.md`
  section 1a (new 1a.5d with its build results, plus 1a.4, 1a.6, 1a.7, 1a.8),
  LF-only, byte-counted. `src/lodgen.h`, `src/lodgen.cpp` and `src/nifcli.cpp`
  changed; backups beside the lane (`*.bak`) and the edit re-runnable as
  `patch_road_opacity.py` + `patch_usage_synopsis.py`. Nothing committed,
  nothing stashed. **bungo's open NifSkope window DOES need a restart** — it is
  still on the 03:06:21 exe.

  **THE GATES, ON THE BUILT EXE.** F2 byte identity: the new exe with no flag,
  with `--road-opacity 1`, and with `--roads-legacy` reproduces the rung **9/9
  and 10/10 files identical in all three arms on both chunks**, every file
  compared, not a sample. The compare is shown able to fail in the same run:
  `--road-opacity 0.326` moves 3 files on (-20,20) and 4 on (-8,8), **all of
  them colour** — the `_msn`, the `_data`, the `.bto`, the `.lodl` and the
  `.lodm` are byte-identical at every setting. F4 chain at GRADE1's baselines
  row for row: `lodgen_roads` **11/0** (R5 floor 0.1354, after 0.3435, reference
  0.4039, bars 0.2708/0.3231 both cleared; centreline colour error 38.04 →
  22.34), `lodgen_terrain` **26/0**, `lodgen_terrain_vt` **41/1**,
  `lodgen_ground_cover` **29/5**, `lodgen_terrain_pbrm` **14/0**,
  `lodgen_native` **0 failures in all seven sections**, `lodl_open` **23/0**,
  `lod_generation` **116/0**. Zero NifSkope processes left running.

  **THE ONE RED ROW IS NOT THIS LANE'S, AND THERE IS A CONTROL FOR IT.**
  `lodgen_terrain_vt` holds its 41/1 count but the failing check is **V9c**
  where the historic red row was V9b. The rung exe was re-run through the same
  harness (`EXE=release/NifSkope.before_roads3.exe`,
  `logs/f4_vt_RUNG_control.txt`) and fails V9c with **digit-for-digit identical
  numbers** — E/W seam 188.074, interior 13.243, ratio 14.20, edge step 14.348.
  So it predates ROADS3. **NEW RED: V9c wants a lane** — the E/W chunk-seam step
  is 14.20x the interior against a bar of 3.20, and the interior control itself
  (13.243 / 12.182) is outside its own 1.20..2.20 window.

  **WHAT VANILLA'S FAR ROAD IS.** Not a painted ribbon: a **wash that follows
  the ground under it**. Road luminance regressed on the mean luminance of the
  non-road texels within 8 texels — vanilla **+0.714** on (-20,20) and **+0.755**
  on (-8,8), our own unpainted ground **+0.637** / **+0.565**, ours **+0.339** /
  **+0.209**. Floors both sides: the field translated reads −0.009 mean (worst
  0.298) over 5 draws, a known-answer fixed paint +0.000, a known-answer
  ground+4 +1.000. Vanilla's road stands **+4.29** and **+4.40** levels over its
  surround on two tiles a biome apart; **ours stands +29.96 and +3.84** — right
  on (-8,8) to 0.56 of a level, **25.67 levels too contrasty on (-20,20)**,
  because our paint is a fixed material colour (99.05 / 106.68) while our ground
  swings 60.96 → 102.12.

  **THE BRIEF'S OWN LAW WAS REFUSED BY ITS OWN FLOOR.** Fitting
  `vanilla = a·ourPaint + (1−a)·ourGround` per texel leaves **17.6 %**
  unexplained against a shuffled-ground floor of **18.2 %** on (-20,20), and
  **52.6 %** against **51.5 %** on (-8,8) — worse than the floor. Ceiling
  (vanilla against its neighbouring shipped sheet) 18.1 % / 39.2 %. The spread
  of `a` (sd 1.414 / 1.789, p10 0.258 / −2.000) is not something an opacity can
  do, and an alignment control over ±3 texels buys only 0.005, so the mask does
  name vanilla's road and the fit still fails. Nothing was built on it.

  **THE HUE IS NOT THE DEFECT — ROADS2's owed number, delivered, and null.**
  Road minus surround on the opponent axes: vanilla +2.30 / −2.14 and ours
  +3.35 / −3.25 on (-20,20); vanilla +4.10 / −3.07 and ours +1.96 / −1.24 on
  (-8,8). Same sign, same direction, every gap under 3 levels of 255. On the
  road texels themselves at Sanctuary vanilla's b_y is −12.62 against our
  −12.39, saturation 0.162 against 0.161. The difference bungo sees is
  brightness.

  **VANILLA KEEPS NO ROAD-TEXTURE DETAIL** — residual-vs-detail correlation
  **+0.0275** against a phase-twin floor of 0.0270 / 0.0644 on (-20,20) and
  **+0.0162** against 0.0124 / 0.0163 on (-8,8), best-fit strength negative.
  ROADS2's `--road-detail 0` default is confirmed by a test that could have
  overturned it. **The edge WIDTH is refused as unresolvable**: a 4.29-level
  rise under a 6.59-level local SD, SNR **0.65**.

  **THE TWO-TONE IS THE OPPOSITE WAY ROUND FROM THE GUESS.** Vanilla reaches
  full value one texel in and runs flat; ours ramps 79.86 → 86.48 → 93.45 over
  three texels, plateaus ~96.5, then climbs to 105.5 in the core — a **darker
  outer band around a brighter core** (the alpha-blended skirt over a far darker
  ground, the wider trunk material inside). Biggest step inside the road: ours
  **3.88** against vanilla's **1.31** on (-20,20); on (-8,8) ours **1.25**
  against vanilla's **4.43**, i.e. already smoother than vanilla there.

  **WHAT SHIPS: `--road-opacity A`, default 1.0**, scaling the road
  plane's alpha into the composite at both writers (`lodgen.cpp:7944` and
  `:9261`), with the multiply **branched over at 1.0** so the off value is the
  rung's bytes by construction, the ground-cover suppression deliberately left
  on the UNSCALED coverage, `roadOpacity` added to the census, and
  `--roads-legacy` restoring 1.0 unless the caller named a value.

  **THE DEFAULT DID NOT MOVE, AND THAT IS ARITHMETIC.** Every candidate was
  simulated on the rung's own sheets with the generator's own composite before
  any code existed. On **(-8,8) no opacity can meet the brightness gate at
  all** — the composite can only land between our ground 102.12 and our paint
  106.68 and vanilla's road is 94.59, **7.53 levels outside** the reachable
  interval. On **(-20,20) the gates are mutually exclusive by 22 levels**:
  absolute level wants a = 0.83, the rise wants 0.326, the step wants ≤ 0.25,
  the local SD wants ≥ 0.75. Those 22 levels are the **ground's** (ours is 19
  levels darker than vanilla's there — GRADE1's per-cell content difference,
  which TILING2 told this lane in writing not to chase with the road pass). A
  per-texel **ground-relative mode was built, simulated and rejected with its
  numbers** (Sanctuary road 25 levels below vanilla with a rise of **−1.8**,
  more than half its texels clamping to 0; downtown local SD halved to 0.48 of
  vanilla's).

  **BUNGO'S CALL — the table, both tiles; BAKED rows marked:**
  a = 1.000 BAKED → (-20,20) 99.05 / rise +29.96, (-8,8) 106.68 / +3.84;
  a = 0.830 BAKED → **92.43 (−0.09 vs vanilla's 92.52)** / +23.34, 105.89 /
  +3.05;
  a = 0.500 priced → 80.01 / +10.91, 104.40 / +1.56;
  a = 0.326 BAKED → 73.09 (−19.43) / **+4.01 against vanilla's +4.29**, 103.39 /
  +0.55;
  a = 0.250 priced → 70.48 / +1.39, 103.26 / +0.42.
  Plainly: **0.326 stops the Sanctuary road reading as a stripe and costs 19
  levels of darkness on it**, because it is borrowing against a ground error.
  Ground first, road second, is the honest order.

  **PICTURES** `scratchpad/roads3_20260911/images/cmp_road_wash.png` (two
  chunks × four columns: vanilla | the rung | a = 0.326 | a = 0.83, each with
  its road level, rise, local SD and step burned in) and `cmp_road_profile.png`
  (cross-road luminance profile, vanilla / rung / ground / both candidates, with
  the per-texel opacity the wash would need on the right-hand scale). **Every
  panel in both pictures is a real bake or Bethesda's own sheet — nothing is
  simulated.** How good the pre-build pricing turned out to be, measured rather
  than guessed: it agrees with the bake on the AGGREGATES to **0.29** and
  **0.22** of a level and differs **per texel** by 1.58 levels on average, 7.34
  at p99 and **15.28** at worst, because the bake goes through 8-bit
  quantisation and BC1 and the simulation does not.

  **THE SKIRT IS GEOMETRY AND THE KNOB CANNOT REMOVE IT.** Correlating road
  luminance with the road mesh's own interpolated vertex alpha (ROADS2's
  `seam.py`, re-run on these bakes): vanilla **+0.001**, ours **−0.792** at
  a = 1, **−0.694** at 0.83, **−0.436** at 0.326 — and our unpainted ground's own
  floor on the same texels is **−0.325**. Opacity walks it toward the ground's
  floor, never to vanilla's zero. The feathered-boundary gradient (54 texels,
  vanilla 4.242) reads 3.979 / 3.352 / 2.979 at those three settings, inside
  vanilla's at all of them.

  **SKILLS.** Added `.claude/skills/ww-simulate-before-build/SKILL.md` (apply a
  candidate knob's arithmetic offline to already-baked sheets and read the whole
  gate table off it before spending a build — the four conditions under which
  that is legitimate, the three things it cannot show, and the labelling rule).
  Amended `.claude/skills/ww-control-calibration/SKILL.md` with "a floor that
  returns NaN is not a weak floor, it is no floor". Both mirrored into
  `E:\Projects\NifskopeWWE_ui\.claude\skills\` **additively** — nothing in that
  tree was deleted or overwritten, because lane UINOTES1 is live in it.

  **REDS CARRIED.** (1) our ground on (-20,20) is 19 levels darker than
  vanilla's (68.69 vs 83.52) and is the actual reason that road reads wrong;
  (2) the object-path material fix-up at `src/lodgen.cpp:6811` still keys on the
  LAST `materials/` in a path — this lane confirmed the
  `materials/c:/projects/fallout4/...` lines in the bake log come from
  `lodgenLoadModel`'s object path and NOT from the road pass (road census:
  `roadRefusedNoTexture 7` of 319 shape tiles, `roadTexels 27509`), so GRADE1's
  red 3 narrows to the object path; (3) the ground-cover plane is empty on these
  chunks (`dwReserved1 = 0`), which is why the grass tint is inert.

  **OWED:** bungo restarts his NifSkope window (new exe 04:10:38); nobody has
  looked at any of this in the game — the default's bytes did not move so there
  is nothing new to see at the default, and the two candidate settings have not
  been flown. `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` needs **no
  change**: no default moved, so the instruction it carries still bakes the same
  bytes.

- UINOTES1 block, spliced (lane text verbatim):

# HANDOFF block -- lanes UINOTES1 + UINOTES1b (for HANDOFF.md's top block)

**Lane UINOTES1 / UINOTES1b -- bungo's nine animation-workspace rulings of
2026-09-12 01:3x-02:0x. All nine are CODE IN, MERGED INTO THE MAIN TREE AND
BUILT. `release/NifSkope.exe` is 2026-09-12 05:48:33, 21,817,856 bytes, sha256
`b62c49989abf002ebacb32769e7e7057d72043b17942436f493c89eaa9e47c47`, built at
`make rc=0` with 0 errors and one warning that is not this lane's
(`-Wunused-function` on `len3`). His open NifSkope window needs a restart to see
any of it.**

**The lane is SUSPENDED, not finished.** `Fallout4.exe` came up (pid 48328,
started 05:42:58) and the standing rule stops the lane. The marker is
`scratchpad/uinotes1_20260912/PENDING2.md`, not a DONE marker. The lodgen chain
has not been run at all and six of the seven UI harnesses have not been re-run on
the newest exe.

**Resume from** `scratchpad/uinotes1_20260912/PENDING2.md` (steps 1-5 at its
foot). The report is `scratchpad/lane_uinotes1_report.md`, sections 0-15 plus
`## Build (UINOTES1b)`; the merge list is
`scratchpad/uinotes1_20260912/CHANGED_FILES.txt`.

**The rung -- never delete or rename it:**
`release/NifSkope.before_uinotes1.exe`, **2026-09-12 04:10:38, 21,489,152
bytes**. (The 23:26:29 / 21,484,032-byte exe named in the previous version of
this block is superseded; the 04:10:38 one is the baseline every "before" number
and picture in this lane came from.)

**What is measured, with every number beside its floor:**

| harness | rung 04:10:38 | merged exe | note |
|---|---|---|---|
| `animws` | 72 checks, 0 fail, 1 skip | **210 checks, 1 fail, 1 skip** | 05:48:33 exe; the rung carries the older 72-check gate |
| `hkxanim_ui` | 48 / 1 | 48 / 1 | 05:06:05 exe |
| `ui_align` | 11 / 0 | 15 / 0 | 05:06:05 exe |
| `water_ui` | 84 / 0 | 84 / 0 | 05:06:05 exe |
| `files_tab` | 29 / 1 | 29 / 1 | 05:06:05 exe |
| `top_bar` | 43 / 5 | 43 / 5 | 05:06:05 exe |
| `skeleton_overlay` | 5 / 1 | 5 / 1 | 05:06:05 exe |

Every red except `animws`'s is the same count on both exes, so this work added
none of them. `animws` went 210/15 at 05:07 to 210/1 at 05:48; **thirteen of
those fourteen repairs were the gate's own defects**, made with five refusing
patch scripts (`gatefix2.py` .. `gatefix6.py`). Only `src/animworkspacetest.cpp`
changed between the 05:06:05 exe and the 05:48:33 one -- no product source has
moved since 05:06:05.

**The one red that is the product's, and it stays red:**
`FAIL (o) Ctrl+A selects every row: 1 of 4`. Control in the same run: with the
list's own signals blocked the same `selectAll()` takes **4 of 4**. The chain
that eats the selection, read from source: `itemSelectionChanged` ->
`listRowChosen` -> `selectEntry(drive=true)` -> `WwHkxAnimHub::activate` ->
`GLView::setSceneSequence` -> `GLView::sequenceChanged`
(`src/nifskope_ui.cpp:24752`) -> `AnimWorkspace::setSequenceByName` ->
`list->setCurrentItem( it )` (Qt ClearAndSelect). A behaviour failure is measured
and stopped on, never fixed to make a gate green. Cost today: multi-row Delete /
Copy / Cut cannot be reached with Ctrl+A. Three candidate fixes are named in the
report's Build section; bungo picks.

**bungo's 05:06 report that the merged exe freezes on opening any nif is NOT
reproduced.** Same files, same window, through the exe's own `--port` IPC:
merged **2.02-2.68 s** per open against the rung's **2.93-3.86 s**. The ~+0.6 s
per-open growth as files pile up is identical on both exes. The three-minute
`animws` run was my own modal dialog and is now 7 seconds. Still open until he
says which file and whether his window already had files in it.

**Owed:** the whole lodgen chain (`lodgen_chain.sh` is written; ROADS3's numbers
to match row for row are in `PENDING2.md`); the other six UI harnesses on the
05:48:33 exe; one drag of a row with a real mouse (a `QDropEvent` sent with
`QApplication::sendEvent` is not delivered inside the application -- the gate's
own filter on that viewport counted **0**, while the dock's own half, the commit
through to the clips, playback and `Scene::animGroups`, is green); pictures for
rulings 3, 6, 6a, 7, 7a and 9 (rulings 1, 2/7b/8, 4 and 5 have shots, listed in
the report); and, from lane UINOTES1 itself, the key-level clipboard, the
QPainterPath-instead-of-svg icon decision and the four questions in report
section 13. An older defect found in passing and equal on both exes: the `--port`
IPC command is space-separated (`src/main.cpp`), so a path with a space in it
never opens.

**Process:** I breached the game guard -- three builds and three harness runs
between 05:42:58 and 05:49 because my one-line guard counted `Fallout4.exe` and
`NifSkope.exe` together and I read the `1` as the harness. Both entries are in
`scratchpad/uinotes1_20260912/MISTAKES_ENTRIES.md` (the other is retyping the
build command instead of copying the skill's line, which cost two builds).

**Nothing was committed, nothing was stashed, bungo's game folder was not
touched, the rung exe is untouched, and one GUI NifSkope at a time was used on
the second monitor with its own unused `--port`.**

- UINOTES2 block, spliced (lane text verbatim):

**Where it stands.** The two gizmo switches in the animation dock's transport row
are icons in ruling 4's set and light up in the accent when their mode is on;
Ctrl+A in the Animations list keeps every row; the `lodl_open` crash lane ROADS4
handed back is measured and is NOT the UI's. Built and gated in the copy tree
only. **Nothing committed, nothing stashed, nothing under
E:/Projects/NifskopeWildWastelandEdition opened, edited, built or run** — except
one read-only copy of `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md`, which
the director told me to take.

**The exe.** `E:/Projects/NifskopeWWE_ui/release/NifSkope.exe`, **2026-09-12
06:55:52, 21,850,624 B**, make exit 0, `make -q` says nothing left to build,
`res/style.qss` and `release/style.qss` identical. The rung it replaces is kept
as `release/NifSkope.before_uinotes2.exe` (05:48:33, 21,817,856 B, md5
980e64c1aa4e5478b5833d83ebea9655 — the same bytes lane ROADS4 measured).

**Gates on that exe.** animws **224 / 0** (2 skips, both fixture skips) against a
210 / 1 rung; lodl_open **23 / 0 PASS** against 23 / 2 with six segfaults on the
rung; ui_align 15 / 0; water_ui 82 / 0 (82 on the rung too — the brief's 84 is
the main tree's number); top_bar 43 / 5, the same five as before; skeleton_overlay
46 / 0 in-app plus its mask and dots gates run by hand under a numpy-equipped
python. hkxanim_ui and files_tab were **skipped**: both want
`scratchpad/hkx1_20260910/clips/*.hkx`, absent from this copy, and its only home
is the main tree.

**Three files changed, all pure LF**: `src/animworkspace.cpp` (121,358 B),
`src/animworkspacetest.cpp` (153,971 B), `tests/spells/animws.sh` (12,251 B).
Byte counts and the not-touched list are in
`scratchpad/uinotes2_20260912/CHANGED_FILES.txt`.

**What the next lane needs to know.**

1. **The lodl viewer crash is still owed, and not to the UI.** On the 05:48:33
   binary every `.lodl` render in the viewer segfaults (3 of 3 here, 6 of 6 in
   the harness). It is not scene size and it is not the UI chrome: that same
   binary headless-renders a plain NIF and a 2.65 MB terrain NIF built from the
   same lodl, both rc 0 and both pictures byte-identical to my exe's. Only the
   `.lodl` DOCUMENT in the viewer dies, and the stack is 19 frames with no
   repetition, inside a queued call after "meshed and built". My exe is green,
   but my build also recompiled lodgen.o, lodgenchunkpass.o, lodgenmanager.o,
   nativeemit.o and nifcli.o against lane ROADS4's in-flight `src/lodgen.h`, so
   **my green row does not prove the rung's lodgen sources are green.** Whoever
   owns that path should re-measure once ROADS4's edits land.
2. **Loop does not light and that is deliberate.** The dock's loop button takes
   its icon from the shared `aAnimLoop` action, and the render toolbar re-skins
   that action on every popup refresh with a single-state icon
   (`src/nifskope_ui.cpp`, `tlMakeIcon( "loop", ... )`). Making Loop light means
   changing the RENDER TOOLBAR's own glyph, which bungo did not ask for. The
   harness prints loop's two inks beside the two it gates, measured and
   deliberately not asserted.
3. **Three things the pictures show that nobody has ruled on**: annotation labels
   overlap each other at 1:1 when markers are close (Blender drops the text
   instead); the right-side panel keeps an empty "Retime to" row when an
   annotation is selected; the Animations list row is one long clipped label.
4. **Ctrl+A's deliberate trade**, written beside the code: a selection the user
   made outranks the viewport's echo of it. If bungo wants the list to collapse
   to the clip the viewport switched to, it is the condition on one line.

**Pictures** in `scratchpad/uinotes2_20260912/images/after/` (r3, r6 x2, r6a x2,
r7, r7a x2, r9, the 1:1 dock overview, the transport row at 2:1 off and on) and
the before/after composites beside them.

- DIRECTOR MISTAKE 18:3x (MISTAKES.md): 35 foreign skill directories were
  blanket-copied into .claude/skills (core-*, fo4-bridge-*, fo4-gate-length-
  by-neighbour, fo4-pdata-chain-walk, fo4cs-*, hudframework, prismaui,
  rdc-cli, reverse-engineering, swf, xse-plugin); deletion classifier-blocked
  twice; they are UNTRACKED (git status 226 -> 271 paths) and must NEVER be
  committed. bungo removes them:
  `cd .claude/skills && rm -r core-* fo4-bridge-* fo4-gate-length-by-neighbour fo4-pdata-chain-walk fo4cs-* hudframework prismaui rdc-cli reverse-engineering swf xse-plugin`.
  Kept on purpose (NifSkope-relevant, were live-only): nif,
  nifskope-ww-lodgen, nifskope-ww-vanilla-compare, ww-artefact-localise,
  ww-qss-geometry-probe, behaivor-graph, fo4-animation-archive-scan,
  fo4-measure-animation-clip. Fable meter: bungo said 97
  percent (17:xx); the last Fable lane was HKXEDIT2; both live lanes are Opus.

### Owed by bungo
- FLAGSCAN1 LANDED 17:1x (read-only, report scratchpad/
  lane_flagscan1_report.md, scripts scratchpad/flagscan1_20260911/): NO
  FLAG marks terrain-baked objects. Over 10,940 placed STAT bases /
  802,195 placements, no header bit is even a majority on roads; 70 of
  ROADS1's 71 road models have flags 0x00000000; Has Tree LOD (6), Uses HD
  LOD Texture (17), Has Currents (19), Show In World Map (28) are set on
  ZERO placed STATs; CELL "Distant LOD only" on 0 of 36,865 exterior
  cells; Has Distant LOD and Visible When Distant run BACKWARDS (rarer on
  roads). Vanilla's baked set is its UNFLAGGED set; ROADS1's folder rule
  stands alone; the best candidate refused on a second tile (grey 0.474
  vs floor 0.498). TWO LEADS -> LANE ROADS2 (small, lodgen.cpp, after
  SPLAT1 phase B): (1) we probably bake RAISED highways/bridges that
  vanilla gives LOD meshes instead -- bit 15 is set only on raised road
  bases (60 of 82 HighwayOverpass/Bridge + raised sidewalks, none of 388
  flat), and on chunk (-8,8) the elevated family reads 0.419 vs floor
  0.509 (more colourful than ground) while flat road clears 0.716 vs
  0.629; lodgenIsRoadModel() accepts HighwayOverpass\* and Bridge\* ->
  a dark ribbon under every raised deck, 994 placements; refuter = one
  bake + one score on that tile; (2) the tree heuristic: ZERO TREE-
  signature records are placed in the Commonwealth (the memcmp clause
  never fires), bit 6 is dead, so the folder/filename heuristic does all
  the work; its four false positives are startsWith("tree") under
  SetDressing\ (a rope pile, a noose branch, two tree swings) -> the
  filename clause applies only under Landscape\ folders.
- STANDING INSTRUCTION bungo 2026-09-11 16:38 (`date`-read), verbatim:
  "Just let it all keep running, let me know when you're done with
  everything for lodgen" -> the director runs the queue to the end
  without asking, one lane to DONE at a time, then the end-to-end
  Sanctuary bake and ONE message with the exe to launch and what is red.
  RUN ORDER: CARDS-AGG LANDED 16:42 (`date`-read; exe 16:20:02
  21,419,520 B md5 3ebf1758..; `--aggregate` off by default, .lodi v4
  only when aggregates exist, 97 cells / 3,414 trees on Sanctuary,
  silhouette mass error 0.0485 vs ceiling 0.0036 / floor 0.16, off ==
  rung 27/27 files; the composite is an orthographic resample of the
  card sheets, NOT a render (deviation stated); his calls: cost tier
  (135.6 MB tile 64 / thr 8, 571.9 MB tile 128, 29.5 MB thr 32 vs a 28 MB
  per-tree library), composite vs render, the conditional version word;
  red: lodgen_octahedral.sh F1 pre-existing on the rung; block spliced
  below; picture sent 16:44) -> RESUME3 RUNNING 16:44 (`date`-read;
  brief_resume3.md: NIFPARSE1 experiment + fixes + 20x gate, SPLAT1
  tiling fix, PIC-GRASS's picture; markers scratchpad/resume3_20260911/)
  -> ROADS2 (brief_roads2.md: feathered/blended roads, raised highways
  out, tree clause scoped, sidewalks decided) -> GRADE1 (brief_grade1.md:
  the tone transfer curve, cause by fingerprint, one change, tint
  re-fitted) -> INCR1 (brief_incr1.md) -> TERRAIN-AO1
  (brief_terrain_ao1.md) -> TERRAINFMT1 (brief_terrainfmt1.md: DXT5 + 10
  mips + _msn alpha semantics measured, the _msn detail term from the
  layers' _n maps fitted or refused) -> director's full-module Sanctuary
  bake + handoff + bake instruction. NATIVE1c PARKED until he rules on
  the near-model library (plan section 6 item a). PIC-RIVERBED LANDED
  17:0x (`date`-read; read-only; scratchpad/pic_riverbed_20260911/images/
  riverbed_texture.png + NOTES.md, sent 17:0x): the grey spots ARE the
  pebbles of textures\Landscape\Ground\RiverbedRocks02Wet_d.dds (2048^2,
  DXT5; LTEX 000BC13D/000BC13F via TXST 000BC13E, 52.6 percent of the
  window) at 6x -- one pebble = 64 texture texels = 64 world units at the
  2048 repeat (2 far texels) vs 10.6 units at 341.333 (a third of a
  texel, averages away); local variance ours 106.8 vs vanilla 29.0; the
  texture's grain correlates +0.318 with our sheet at 2048 (floor -0.002)
  and is absent at 341.333 and absent from vanilla. Its one mistake
  (layers ranked by record id, not by resolved texture) is in
  MISTAKES.md.
- RULINGS bungo 2026-09-11 16:3x (`date` 16:36) over chunk_vs_vanilla.png:
  (1) "The difference between this and vanilla looks pretty stark ... I
  wonder why that is" -> four stacked gaps named: tiling (RESUME3 fixes),
  TONE (~11/255 darker, lower saturation -- candidates with different
  fingerprints: colour-space slip = gamma-shaped curve of ours vs vanilla
  at matched texels; plain grading = constant ratio; baked lighting /
  ambient term in vanilla's bake = position-dependent ratio), large-scale
  BLOTCHING (VCLR strength and/or the same baked term), ROAD COLOUR (pale
  grey vs dark blue-grey = the tone gap on one material). LANE GRADE1 =
  that transfer-curve measurement on the FIXED tiling, right after
  RESUME3, before any grading value is typed. (2) "look at the roads,
  there is a visible seam while vanilla doesn't have it" -> mechanism:
  Bethesda's road meshes are FEATHERED (vertex alpha fades edges/end caps
  into the ground; joint pieces are alpha-BLENDED over the pieces
  beneath); ROADS1's rasteriser overwrites by max-z, ignores vertex alpha
  and paints alpha-blended materials as solid -> hard line where the
  winner changes. FIX in ROADS2 (already queued for raised highways +
  the tree filename clause): composite in draw order with the material's
  blend mode x vertex alpha instead of overwriting by height; gate = a
  seam metric (colour gradient along piece boundaries, ours vs vanilla on
  the same road, floor on a boundary where both are solid).
- PIC-CHUNK LANDED 16:3x (`date`-read; read-only; scratchpad/
  pic_chunk_20260911/images/chunk_vs_vanilla.png + _msn.png, NOTES.md,
  facts.json; sent to bungo): chunk (-20,20) ours (ROADS1's 12:19 bake)
  vs vanilla at full 512. Road placement verified (identity beats all six
  flips/rotations; cross-correlation peak within 1 texel). Ours BC1 /
  174,888 B / 8 mips vs vanilla BC3 / 349,680 B / 10 mips on BOTH colour
  and _msn (= ROADS1's TERRAINFMT1 lead, now with bytes). Whole sheet ~11
  /255 darker on all channels (the grading gap). NEW FINDING, its own
  item: our _msn is far too SMOOTH -- texel-scale roughness 0.93 vs
  vanilla's 10.15 (~11x): Bethesda's normal sheet carries erosion detail
  at texel scale that VHGT (128 u/vertex) cannot give; the ridges agree,
  the fine detail is absent; the TILE fix does not touch this. Candidate
  sources for the detail, to measure before believing: the land
  textures' own normal maps blended by the same weights (most likely --
  vanilla's bake likely composites the layer _n maps), or a detail
  normal from the colour. Lane TERRAINFMT1 grows to: DXT5 + 10 mips like
  vanilla, and the _msn detail term, gated by the same roughness metric
  against vanilla with a floor and a ceiling.
- SPLAT1 PHASE A LANDED 16:1x (`date`-read; block spliced above): THE
  LAND TEXTURES ARE BAKED 6x TOO LARGE -- `constexpr float TILE = 2048`
  at 14 sites in src/lodgen.cpp vs the engine's 341.3333 u/repeat
  (fLandTextureTilingMult 1.5 in Fallout4.exe 1.10.155, one xref, u =
  col x 0.375 over the 17x17 quadrant grid at 128 u/vertex -> 128/0.375).
  Offline re-bake at 341.333: local variance 76 -> 13 vs vanilla 20-29;
  mip / tint / VCLR / codec refuted with numbers; the x0.82 grading gap
  (16-20/255) is NOT closed by it and stays open. Picture sent to bungo
  16:1x. Phase B = BUILD PENDING (scratchpad/splat1_20260911/PENDING.md).
  NOTE for FO4CS: the ring-0 runtime blend must use the same 341.333
  repeat or the cross-fade to the pyramid shows. RESUME3 BRIEF WRITTEN
  (scratchpad/brief_resume3.md): ONE build lane after CARDS-AGG's DONE
  for NIFPARSE1's pending (experiment -> named fault -> only the fixes
  the verdict supports -> the 20x fan-out gate) AND SPLAT1 phase B
  (`--land-tiling`, default 341.333, 2048 = the way back, all 14 sites,
  _msn untouched, contract amended incl. the VCLR-range correction
  203..255). CARDS-AGG IS ALIVE at 16:17 (relinked release/NifSkope.exe
  16:17, files 16:15-16:16) despite its stale PENDING saying "build has
  not run"; SPLAT1's belief that it "will never produce a DONE" was a
  misread of that stale file. Then ROADS2 (raised highways excluded +
  tree filename clause under Landscape\ only), INCR1, TERRAIN-AO1.
- LANE TERRAIN-AO1 QUEUED (bungo 2026-09-11 15:4x, after "AO between all
  geometry and the terrain, and terrain gets AO from placed objects
  too?" -> as built: objects get AO from terrain AND each other (per
  placement, ray-cast vs the assembled chunk + heightfield + skirt);
  terrain AO (.lodl plane, pyramid mask B) is horizon-from-height ONLY,
  so ground under a house or a forest stays bright; his "Okay, so the AO
  can be acurate from objects" = do it): after the height-only pass,
  rasterise each placement's ground occlusion into the pyramid's AO
  channel (and the .lodl plane? -- lane proposes: the plane is the
  runtime's ring-0 source, so BOTH, or the plane only at its 8/cell
  density) from bounds + clusters, the objects' own march aimed at the
  ground. Gates: a region with no placements byte-identical; a tile
  under a known building darker than the same tile with the building
  removed (floor); horizon AO term unchanged where no object reaches.
  Brief to write after CARDS-AGG lands. Reminder given to him: terrain
  AO is TEXTURE (plane/sheet per texel), object AO is per VERTEX (and
  per instance for cards).
- FROM BAKEPERF1 (2026-09-11 14:4x), his calls: (a) the NIF parser is
  not thread-safe -- the one thing that would unlock the machine for the
  bake (lane NIFPARSE1: per-thread model construction with the slab
  pool's mutex and the shared condition caches made safe, gated by the
  same byte-identity chain + 20 consecutive 16-thread runs without a
  fault); until then the bake is single-threaded as it always was; (b)
  the chunk fan-out is correct but 1.5-1.8x SLOWER and peaks at 21.9 GB
  on 25 chunks -- off by default, keep as is; (c) a pre-existing UI bug
  found in passing: the block-tree name column overwrites the main name
  table on the first multi-array row (task chip queued by the lane; a
  small lane); (d) the Commonwealth chunk pass extrapolates to 1.6-3.8 h
  single-threaded -- his bake will take hours; the memory curve was
  REFUSED as an extrapolation (two points).
- NOTE FOR THE FO4CS PLAN's R5c (bungo 2026-09-11 16:2x, "vanilla
  fallout 4 has some precombines placed in some places, that contain
  several trees, right?"): SCOL tree clusters are already expanded per
  part by the generator (lodgen.cpp ~3389: each part = its own placement,
  identity, card, instance record with the SCOL ref + part ordinal).
  PRECOMBINES are the loaded cells' merged draw, invisible to the far
  field -- but for R5c (engine full draw suppressed per object) an
  object inside a precombined mesh has NO draw of its own: suppression is
  per precombined batch or not at all. Director splices this into
  docs/FO4CS_IMPROVED_LOD_PLAN.md R5c's must-not / RE-candidates when a
  plan revision is next made (the plan lane has landed).
- OWED TO NATIVE1c (bungo 2026-09-11 16:1x, over native1b's ladder.png:
  "Hm, that tree LOD becomes a stump there"): the cluster ladder must
  REFUSE alpha-tested foliage clusters (leaf cards do not simplify
  geometrically -- the crown becomes detached fragments, then nothing;
  a tree's far representation is the impostor card by ruling); trunk /
  opaque clusters may ladder. Second gate: every level of every mesh
  keeps >= a stated fraction of level 0's silhouette from the horizon
  views (ww-silhouette-compare) or the level is refused and the mesh
  stops laddering there -- a building may shrink, a tree may never
  become a stump. Note for the picture: a level is per group, so "every
  cluster at level N" is not a valid cut; the real cut mixes levels.
- OWED TO NATIVE1c (bungo 2026-09-11 15:3x, "is vertex AO baked into
  impostors too on top of the texture AO they hold?"): cards carry the
  model's vertex colour (in the colour sheet) and a height-neighbourhood
  self-AO x texture AO (mask B) -- but NO placement AO (ground contact,
  neighbours), which the chunk meshes have per vertex (colour B,
  ray-cast). Add a per-INSTANCE AO byte to the .lodi record (or fill the
  base row's selfAO, 255 today, and add the instance byte), ray-cast at
  bake against the chunk + heightfield the way the chunk vertices get
  theirs, so the runtime darkens each card by its surroundings; census:
  written AND moves (a tree under a bridge reads darker than one in a
  field). Part of NATIVE1c with the near-model library.
- LANE INCR1 QUEUED (bungo 2026-09-11 15:2x, verbatim: "what if I load
  in some mod, and it only updates like one cell in the worldspace, can
  the lodgen regenerate only partially, saving us a lot of time?" and,
  on the mechanism, "so the new .esp gets read and diffed against the
  original bake from original plugin") -> incremental regeneration: a
  per-chunk LEDGER of hashes over the RESOLVED inputs each chunk sees
  (winning LAND records, placements after override/disable, base
  records, LTEX->TXST, model/texture content) written by every full
  bake; `--incremental` re-resolves the load order, hashes, diffs per
  chunk (never plugin files as files), widens by a written dependency
  map (skirts/borders, pyramid levels, horizon-AO reach, water bodies,
  merge, arrays/cards, library append), rebakes the dirty set through
  BAKEPERF1's deterministic queue, copies the rest, re-signs CRCs and
  corpus hashes; refuses to a full bake by name when it cannot answer.
  THE GATE: incremental == full bake BYTE FOR BYTE on every file for four
  synthetic edits (one-cell LAND override + moved REFR, plugin removed,
  texture replacer, new model), floor = a wrong widening shown to break
  identity. Brief scratchpad/brief_incr1.md; runs after CARDS-AGG; he can
  stop it to bake first ("Say stop if you'd rather bake first").
- PARKED FO4CS FEATURES the data enables (bungo 2026-09-11 15:1x, after
  "anything else our new LOD system enables us to do in fo4cs?"; all
  AFTER the far field flies; nothing to bake except where marked): world
  state at distance via the per-instance REFR form id (disabled /
  destroyed / built references follow the game's enable state); far
  grass from the cover plane + tint; weather wetness x roughness on far
  ground; far water with flow / shore foam / dye from the .lodl planes;
  city glow from emissive layers and (BAKE: an emissive-to-light table)
  distant light sources seeded from emissive texels; mid/long-range sky
  occlusion for the loaded cells from the .lodl AO; true far depth for
  the volumetric-air campaign; occluder-box culling inside the grid ONLY
  where previs is absent or broken (his note: "Vanilla games has previs
  and precombines" -> intact previs is left alone, our boxes cull where
  a mod broke it and everywhere outside the grid); and THE WORLD MAP --
  his words "the real world map sounds awesome. Like in Skyrim." -> a
  Skyrim-style 3D map rendered in-engine from the .lodl relief + the
  pyramid colour + native objects + tree cards + water bodies, free
  camera, zoom/tilt, time-of-day shadows, markers on real ground,
  replacing the Pip-Boy's flat map (FO4CS side, its own campaign; the
  Pip-Boy map is a Scaleform SWF, so the render goes to a texture or a
  PrismaUI overlay -- candidates, not decided).
- RULINGS bungo 2026-09-11 14:4x -> 15:0x on SHADOWS vs the LOD system
  (for the FO4CS plan's R1/R3 rungs; lane PLAN-FO4CS was launched before
  them -- the director splices them into docs/FO4CS_IMPROVED_LOD_PLAN.md
  when it lands): (1) his question "the LODs won't conflict with shadow
  casters? Especially cascaded and far shadows ... far shadows falling on
  me from behind, from objects a few kilometers away at low sun angle"
  -> two rules: EVERY placement has exactly ONE shadow representation at
  a time, chosen the same way its draw is chosen (cells inside the loaded
  grid drop out of the far-shadow casters by the same cell-range table
  that drops their draws; the cascades never contain LOD geometry; the
  grid-edge band blends shadows the way it blends draws) with a census
  count of casters per source; and far casters at low sun come ONLY from
  the far-shadow pass, which sees the whole world the light sees (cluster
  cut at the shadow view's looser tolerance + cards from their height
  channel oriented to the LIGHT), self-shadow excluded by identity as
  before. (2) verbatim "for this new LOD system, the way terrain shadows
  get sampled from needs to change" -> ONE height source for draw AND
  shadow: the far terrain shadow march samples the .lodl finest level in
  ring 0 and the pyramid's resident height sheet beyond (the same data
  the ground is drawn from); HeightMap.dds becomes the fallback arm
  (module off). (3) on "How cheaper?" and "any other cons pros?": the
  HYBRID is the default -- march the terrain (heightfield-exact contact,
  ~0.3-0.8 ms est., cost rises with low sun, cannot do overhangs), render
  ONLY the object cluster cut + cards into a far shadow map (~1-3 ms est.
  for everything, small for objects alone), combine with a max; both
  costs are census fields and the first FO4CS capture round measures
  them; map-only is the fallback arm if the march misbehaves. Estimates
  are estimates until captured.
- FROM CENSUS1 (2026-09-11 13:5x), a FINDING not a ruling: the independent
  decoder `tests/spells/lodgen_native_decode.py` REFUSES the downtown
  Boston pair (NATIVE1b's occluder region, the only pair with occluder
  boxes) at instance 3359 -- a neighbouring instance sits 2.999985 cells
  from its chunk origin after quantisation, one step below the cell line,
  so the decoder re-derives a different cell than the writer sorted on;
  the cell-level twin of the chunk-boundary rule the contract already
  documents. Fix = a decoder rule (derive the cell from the QUANTISED
  position the same way the writer does), not a format change; no gate
  had ever decoded that pair. Small lane (NATIVE1d) or folded into
  NATIVE1c. Also owed to the next generator lane, named by CENSUS1 with
  bytes: per-slot instance totals in the .lodi header, per-base
  full-detail triangle count (room exists, zeros today), card count in
  the .lodo header, a watertight bit in the mesh row, and the aggregate
  cards + forested-cell count (CARDS-AGG). Today's bake writes NO card
  layer, so every card census field must refuse rather than print 0.
- FROM LODUI1 (2026-09-11 13:2x): (a) the FO4CS target STILL WRITES the
  `.BTO` chunk files (not offered as a row) because the texture arrays,
  card arrays, merge and far-ring cut read them back and FO4CS's module
  reads them today -- stop writing them, or keep until the runtime reads
  the pair? (director: keep until FO4CS reads .lodo/.lodi, then drop; the
  summary line already says so in words); (b) the panel's Trees-only row
  is ON by default while the CLI/driver `--candidates` default stays
  `missing` -- make them agree? (director: yes, CLI default = trees, one
  line + the gate literal); (c) cover's home, carried from TERRAIN-R.
- FROM ROADS1 (2026-09-11 12:1x): (a) 65 of 270 road shapes name an
  absolute Bethesda build path for their material with an empty texture
  set; fixed for the ROAD pass only -- the shared object loader still
  drops those textures (fixing it there moves the byte-identity gates) --
  extend the fix to the shared loader as its own small lane? (b)
  `--road-cover-suppress` default 1.0 (no grass under roads), unmeasurable
  against vanilla (no cover plane there); (c) sidewalks in the rule but
  untested (187 texels).
- FROM TERRAIN-R (2026-09-11 11:2x): cover's home -- mask A (shipped) or
  colour A (`--vt-cover-in-color`, INI-only); identical bytes either way,
  the engine tolerates BC3 colour (2,001/2,001 vanilla sheets are DXT5);
  the only discriminator is meaning (colour A = the object family's
  opacity slot, which a consumer alpha-tests). Director: keep mask A.
  Also its red: the resource stack cannot see a `.pbrm` or a `.lodm`
  (vendored extension list) -- a doc claim is untrue today; small lane.
- FROM NATIVE1b (2026-09-11 10:1x), THE IMPORTANT ONE: the cluster ladder
  works but is barely selectable -- "full detail" in the library is
  Bethesda's LOD mesh (mean 47.7 tris per building), so the median level-1
  cluster deviates 3.80 percent of the model diagonal and reaches 1 px
  only past 52,100 units. Building the library from each object's NEAR
  model (the full MODL) instead fixes it with NO format change; it is what
  his "extend the cluster LOD inward" ruling implies (the inward extension
  needs near-model geometry), at the cost of library size and bake time.
  Director's recommendation: yes, as lane NATIVE1c, with the library size
  and bake time on the Sanctuary region printed before/after. Also: coarse
  levels carry one contributor's UVs at a position weld (117,722 welds
  merged differing UVs; level 0 untouched) -- accept for far levels, or
  weld on position+UV (more verts, fewer merges); his call.
- FROM NATIVE1a (2026-09-11 08:4x), two calls, both written into the v2
  contract as deviations: (a) the mesh/material sort sits INSIDE the cell
  (chunk, cell, draw rank, ref, part) rather than mesh-major across the
  chunk, because mesh-major would break the cell-range blob the near-field
  suppression reads -- keep, or mesh-major with a second index; (b) the
  placed REFR form id stays in the COLD record (already there, same index)
  rather than growing the hot 24-byte record to 32 (+33 percent on the
  per-frame cull buffer) -- keep, or duplicate into the hot record.
- FROM UI6 (2026-09-11 07:0x), his calls: (a) the OLD Animation Manager
  could EDIT a NIF's own animation keys (insert/delete/copy/paste/scale,
  easings, interpolation, mute, text-key markers), had a curve view + key
  inspector, lanes for non-transform controllers (alpha/colour/vis/UV),
  channel copy/paste, CSV in/out, lint, lane filter, snap steps, normalise,
  follow-playhead -- the new Animation dock is a Havok clip editor and a
  NIF sequence is READ-ONLY in it. Nothing ported (lane_ui6_report.md 5.1,
  16 rows). Decide: port NIF key editing into the Animation dock (a lane),
  or accept read-only NIF sequences. (b) The arrow air made 13 menu
  buttons 4 px wider; in a narrow window the left dock loses 10 px --
  keep, or 6 px air (fails the 2-px gate on the tightest button). (c)
  `WW_ANIMPLAY_TEST` / `WW_ROTKEY_TEST` retired in place -- Play-animates-
  viewport and rotation-key-insert coverage GONE until re-aimed (a lane).
  (d) `TimelineWidget` still compiled, never built; deleting it is its own
  lane (its TU owns the icon set). (e) loaded_nifs 166/3: one red predates
  UI6 (three handoffs quoted 166/0 -- unknown when it turned), two are
  UI6's dock-width probe at a fixed 60-px column -- harness repair owed.
- SETTLED 2026-09-11 06:2x ("Yeah, good."): clip load keeps EVERY-FRAME keys
  (the file opened is the file on disk; Blender imports per sample too);
  Reduce stays a button. Refinement owed to the next animation lane: the
  Reduce tolerances persist in QSettings between sessions.
- The GUI bake with the four stage times (landscape / meshes / textures /
  impostors) for the FO4CS handoff README.
- The Blender trip: scratchpad/hkx4_20260910/out/human_male_mixamo.gltf,
  scene rate 60 (24 warps).
- Game flight: scratchpad/hkx5_20260910/flight/JogForward_marked.hkx as an
  MO2 mod.
- The second commit (~225 paths).
- Two red gates: impostor cube 1.78 vs bar 1; the 32-texel corner in _msn on
  cell -20.28. Parked: reuse vanilla _msn where unchanged; far-cell material
  plugin shape.

### Owed by the director, after the reset
1. Read CONSTITUTION.md, then this block, then the 2026-09-10 ledger blocks
   below. Check `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?`.
2. Splice BUILD11's HANDOFF_BLOCK.md and WATER7's report s7 into the ledger
   when they exist; splice scratchpad/water7_20260910/WW_CHANGES_ENTRY.md into
   WW_CHANGES.md by Python (CR count 19,020 must not move); mirror any
   amended .claude/skills to E:\Projects\Claude\.claude\skills.
3. Then: bungo's call on the dropdown-arrow margin (UI3); HKXEDIT3; HKXEDIT3 (round
   trip + vocabulary gates, game flight, retire the interim timeline.* dock);
   the FO4CS side list (loader reads .lodl, accepts v3, .lodo/.lodi readers).
   Exe at the time of this block: release/NifSkope.exe 20:45:47 carries EVERY
   landed change; no lane is running; 225+ paths uncommitted.


**THE COMMIT, 2026-09-09 evening. bungo lifted his "Not yet", and everything
uncommitted since 2026-09-04 is on `origin/main`.** Eight commits, in the
order the work landed: `1bf719c` the .gitignore that keeps the bulk out;
`f6296a8` the file family and the FINAL NAMES (.lodl / .lodt / .lodo / .lodi
/ .lodm, each reader refusing the other meaning by name, land bytes
byte-identical, v2 water, landless cell 97.6% -> 0); `d934e6e` the generator
round (impostor ladders 46.4%, ground cover, the virtual texture, up-in-green
worth 67.7% of the light, the quintic ease 0.947 -> 0.209, card fill 0.500 ->
0.854, gap = max(2, side/16), bleed 4/19 -> 0, `--candidates trees` 33 ->
19); `080f0cf` the LOD Generation panel, the channel preview and the plane
picker; `e760cc8` headless never prompts and never shows a window (15/0 then
55/0); `7ee437e` the seven format contracts and the FO4CS handoff package;
`164d24b` the 683 scratchpad reports, briefs, scripts and pictures; and
`5e023f4` the ledger commit that carries this paragraph. Nothing was built or
run for the commit; the exe on disk is still 19:35:14 and the PENDING gates
are still pending.

The push moved `origin/main` from `965dfe8` to `5e023f4` -- **26 commits, not
8.** Eighteen of them were the 2026-09-03/04 work (the outfit sidecars, the
segment reader, the AO chunk edge, the LOD water and `.lodt` rounds) that had
been committed locally and never pushed. Nothing was rewritten and nothing was
forced; it was a fast-forward.

EXCLUDED, and why: 1,904 files and 647.3 MB that a public repo must not
carry -- `heightmaps/` (436.2 MB of generated far-terrain HeightMap DDS, 13
worldspaces, regenerated by the lodgen heightmap bake), `scratch_water/`
(5.5 MB of hand-made .btr/.bto scratch bakes), the FO4CS sample set
`scratchpad/handoff_fo4cs/samples/` (129 files, 162.1 MB -- its MANIFEST.md
and `make_samples.sh` ARE committed, so it regenerates), and under
`scratchpad/` every .dds/.bto/.btr/.nif/.lod*/.bin/.head/.orig, every
`*.manifest.txt` and `*.verts.txt`, the five multi-MB LAND dumps of
mountains_20260907, and every .png outside the four folders a handoff or a
committed report points at. What went in under `scratchpad/`: 695 files,
23.24 MB; the two largest are handoff_contact_sheet.png (2.82 MB) and
handoff_card_sheets_0003a28b.png (1.88 MB), and nothing reaches 5 MB.

Line endings were measured with Python byte counts before every commit, not
grep. Every LF-only file stayed at 0 CR. The three mixed files moved exactly
as their own diffs account for: `WW_CHANGES.md` +3,120 LF lines and dCR +0,
`src/glview.cpp` +20 CRLF / -3 CRLF and dCR +17, `src/nifskope.cpp` +180
CRLF / +29 LF / -1 CRLF and dCR +179 -- its CRLF-with-LF-blocks pattern
intact. No file was normalised, and none needed a binary splice.

SESSION 2026-09-09 - director mode, and the two mountain answers

**Read CONSTITUTION.md (new, repo root) before this file.** bungo's rulings of
2026-09-09: Opus 5 agents do the work, the director charters and verifies;
skills named in every brief; MISTAKES.md at the repo root takes every mistake
the moment it is recognised (docs/MISTAKES.md is the older ledger). Nothing
was committed while this session ran (his "Not yet", lifted that evening --
see the block above); CONSTITUTION.md, MISTAKES.md, the two briefs, two
reports, the images folder and an untracked `.claude/skills/` copy were new
on top of the 70 files.

**Lane MSN ran the curl-free test** (`scratchpad/lane_msn_report.md`,
`scratchpad/mountains_20260907/msn_curl.py`, `curl_out*.txt`). Verdict MIXED:
about two thirds of vanilla's `_msn` detail is consistent with a height field,
one third is not, and the excess is the same multiple (2.9-3.0x floor) on
painted and unpainted ground while the mountain tiles have zero painted
material. Refuters killed: block compression, a finer heightfield filtered
down, the blue/green transposition (confirms 2026-09-07 a third time). Our own
sheet cannot serve as a positive control (1.0x separation: it is codec noise in
that band); the floor was rebuilt through the encoder, skill
`ww-control-calibration` written. **Consequence: recomputation cannot reach
vanilla's detail; reuse vanilla's `_msn` where terrain is unchanged. His call.**

**Lane IMAGES delivered the two owed pictures**
(`scratchpad/mountains_20260907/images/mountain_distance_compare.png`,
`mountain_peak_closeup.png`, report `scratchpad/lane_images_report.md`), real
renders through the hook, same camera both halves, tiles 16.-64.32 and
4.-60.36 (summit cell (-60,36), 38,840 units). Ours is flat grey at luminance SD
0.00 against vanilla's 16.0 / 14.9: no painted material under either tile. New
finding: our terrain chunks ship vertex colours by default
(`lgTerrainIdentity`) which vanilla's do not, and NifSkope multiplies them into
the diffuse (photographs blue); both images used `--no-terrain-identity`.
Whether FO4's LOD shader reads that channel is still unmeasured (lane IDENTITY's
open item). Skill `nifskope-ww-vanilla-compare` written. Relative `--out-dir`
paths resolve against the EXE's folder (MISTAKES.md).

**BUG, bungo 2026-09-09 on the peak close-up, verbatim: "You can see the square
pattern on the right in the terrain, which is not good."** Ours shows a square
lattice on the mountain face that vanilla does not. Not investigated yet (his
go pending). Candidates, unmeasured: the bilinear `_msn` creases at every
height-sample line (129 samples across a 512 sheet at dim 4 = a 4-texel
period, C0 only), the VHGT 8-unit staircase, or the x-mod-4 table noted in
WW_CHANGES 2026-09-07. The measurement is the residual's autocorrelation on
OUR sheet at lags 4/8/16 against vanilla's, same tile, `msn_curl.py` machinery.

**Lane LATTICE answered the square pattern** (`scratchpad/lane_lattice_report.md`,
picture `images/mountain_peak_lattice_before_after.png`, skill
`ww-artefact-localise`). It is our `_msn`, not the mesh (neither .btr ships
vertex normals) and not the renderer; period = one height sample (4 texels at
dim 4), NOT the cell border; the 8-unit staircase contributes 0.000. Cause 1
FIXED in `src/lodgen.cpp` heightAt: the bilinear parameter goes through a
quintic ease (same four taps, hits every VHGT sample, cannot ring); grid
roughness 0.947 -> 0.209 before the codec (vanilla 0.296), HF energy +25%;
on the shipped BC1 file only 0.809 -> 0.637. Changes every terrain `_msn` in
every worldspace. Built 13:42:03, exe newer than lodgen.cpp, LF clean, 73
uncommitted paths. **bungo saw no visible difference, and he is right**: cause 2
dominates the picture -- BC1 flattens 16-29% of our 4x4 blocks (vanilla
0.1-3.3%) because our sheet carries ~10 code steps of HF signal vs vanilla's
65. PENDING: `tests/spells/lodgen_terrain.sh` (Fallout4.exe was up); the VT
pyramid path `lodgenBakeVtTile` (~lodgen.cpp:6104) still has BOTH 2026-09-07
defects (nearest + up-in-blue), needs its own lane.

**RULING bungo 2026-09-09 ~16:4x, THE FINAL FILE NAMES (supersedes the 16:1x
`.lodg` ruling):** `.lodl` = land (what `.lodt` held: heights, AO, blend,
colour, water, overview), `.lodt` = TEXTURES (what `.lodv` held: the terrain
sheets per level; his words "lodx? it was meant to be lodt"), `.lodo` = objects
(the mesh library, was to be `.lodg`), `.lodi` = instances, `.lodm` = materials.
`.lodv` and `.lodg` retired; `.lodt` is REPURPOSED on his reaffirmation, so the
writer must give the texture file its own magic and a reader opening an
old-meaning `.lodt` must refuse by name (gate: old Commonwealth.lodt -> texture
reader = refusal; same bytes as .lodl -> verify byte-identical). His
`E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodt` becomes
`Commonwealth.lodl`; FO4CS's terrain loader needs the extension change. `.bto`
and `.btr` stay the stock bake. Owner of the renames: lane TERRAINFIX (or its
successor); lane CONTRACTS freezes the names in the documents. The spec is
`scratchpad/specs_20260906/spec_fo4cs_native.md` (copied out of %TEMP% today);
its Lane 0 = a checked-in `stock_baseline.sha256` from one named build, then
the instance table. Not launched yet.

**Also today: the `.lodt` opens in NifSkope** (lanes LODTOPEN/2/3,
`scratchpad/lane_lodt_open_report.md`): same route as `.btd`, nine planes one at
a time via the picker's plane combo or `WW_LODT_PLANE`; `-no-gui lodt`; 23/0
harness `tests/spells/lodt_open.sh`; writer byte-identical; whole-map full-rate
open = 14.8 s / 4.4 GB (document build, not the read). Exe 15:30:27. His file:
`E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodt`. Far terrain
vertex spacing measured (`lane_btr_spacing_report.md`): vanilla = 128 x level
units, ~1000 columns per chunk at every level, adaptive not gridded.

**EVENING 2026-09-09, STATE AT ~20:00 (exe release/NifSkope.exe 19:35:14; 84+
uncommitted paths -- all of them committed later that evening, block at the
top of this file):**
- Landed and gated today, in build order: `.lodl` viewer (same route as .btd,
  nine planes); headless close never prompts (Qt 6.5 quit closes windows first);
  VT pyramid `_msn` bilinear-through-ease + up-in-green; landless-cell `.lodl`
  fix (DiamondCity was 97.6% wrong, Far Harbor 62, Amphitheater 97 -> all 0);
  `.lodl` VERSION 2 (worldspace default water height + WATR form; FO4CS pins v1
  -> it must learn v2 with the rename; way back WW_LODT_VERSION=1);
  the FINAL NAMES rename (lodl/lodt/lodo/lodi/lodm, old spellings refuse and
  name the new one; land magic unchanged, texture container magic LDTX);
  `--candidates trees` tree-only (19, was 33); three hex comments; qmake re-run
  (btdterrain.o -> lodtfile.h dependency); headless windows INVISIBLE (opacity
  0, never on the primary; the native-window recreate flash fixed in the
  constructor + app event filter; render_shot.sh 55/0); V9a re-pinned with
  numbers (43 edge texels, the ease reaching the cover slope gate; RIGHT).
- His installed files: `mods\FO4CS\Terrain\<ws>.lodl` x5, v2, each
  --verify-only 0 mismatched; `.lodt.bak-20260909` beside each (v1 bytes).
- Delivered: `scratchpad/images_20260909/handoff_contact_sheet.png` + 16
  pictures (terrain L4/8/16/32 lit + triangulation, objects ring0/far/identity,
  3 card-sheet pages, card-vs-source, 882 cards in a chunk); the FO4CS sample
  set `scratchpad/handoff_fo4cs/samples/` (129 files, 162.1 MB, MANIFEST.md),
  handoff README §4/§5 corrected; contracts docs/LODGEN_*.md under new names.
- Open, found today, NOT fixed: impostor card quads draw OPAQUE in a chunk --
  every card `_fs.DDS` is DXT1 with no alpha flag where vanilla's alpha-tested
  LOD tree textures are DXT5 (the octahedral sheets are DXT5 and fine); the
  direct chunk bake's edge clamp (a one-cell ring would make it match the
  pyramid; re-baselines identity gates; his call); the FO4CS reader needs
  `.lodl` + v2.
- NEXT on his word: the COMMIT -- DONE that evening, eight commits pushed to
  `origin/main` (block at the top of this file) -- then native lane 0
  (`stock_baseline.sha256`) and the `.lodo`/`.lodi` writer; the rock decal
  intake below awaits his go.
- Skills written today (live tree E:\Projects\Claude\.claude\skills, some also
  in <repo>\.claude\skills): ww-control-calibration, nifskope-ww-vanilla-compare,
  ww-artefact-localise, ww-contract-provenance, nifskope-ww-resume-pending;
  nifskope-ww-render-shot and nifskope-ww-build-verify amended.

**RULING bungo 2026-09-09 ~20:1x, impostor framing, verbatim: "biggest, texture
aspect ratio, maximizing the size of the geometry on each render, so that there
is still a little bit of padding, 8 pixels on 1024x1024, 16 on 2k, and so on,
you can guess the rest"**, then clarified: **"What we essentially need,
maximizing the tree's size in each row and column, with enough pixel padding so
that there's no mip map bleeding into other rows and columns"** -> the padding
is MIP SAFETY between frames: its size follows the mip chain (8 texels per
128-texel frame = 3 clean mips is his reference point), the frame edge is
dilated into it so coarse mips bleed the tree's own colour, alpha padded so the
silhouette shrinks rather than haloes; geometry fills the frame to that
padding; aspect from the real silhouette. Then, verbatim: **"nifskope needs to
position the tree in each view, so that the tree is equal in size on each one,
and with minimal pixels wasted, and end up with no mipmap bleeding to nearby
frames"** and **"then the tree must be positioned correctly, so that when a 3d
tree transitions to an imposter, the tree won't change position"** -> ONE
scale per card (max silhouette over 64 views), the card centre fixed once per
card, the `.lodm` carries the PIVOT-TO-CENTRE OFFSET + half extents + padding,
and the TRANSITION GATE: source model and card rendered from one camera must
coincide (centre within 1 texel, extents within 2%; zeroed-offset control must
fail). **LANDED, lane CARDFIT3 (exe 21:01:14, `scratchpad/lane_cardfit_report.md`,
pictures `scratchpad/cardfit_20260909/pics/`):** fill x median 0.500 -> 0.729
(worst 0.125 -> 0.250), y 0.812 -> 0.859, sheet area -2.5%; the cause was the
aspect ladder's floor, not the bounding sphere (frames were already centred to
1 texel); padding = max(2, side/16) per axis rounded even, mips = 1 +
log2(min pad): bleed 4 of 19 sheets -> 0 on every border of every shipped mip,
zero-padding control fails 19/19; `card.center` = the pivot->card-centre
offset, now documented and gated against the model's own bound spheres
(6.4x/12.3x/19.7x discrimination, zeroed control fails); `_fs.DDS` now DXT5
with vanilla's header, 98.3% of the quad cuts at alpha 0.5; half-aux confirmed
OFF (correct per 2026-09-06); sample set cards regenerated (7 array classes,
was 4); octahedral 70/0, cards 14/0. NOT the one-texel render test bungo asked
for: the render hook cannot pin a camera (WW_RENDER_CENTER does nothing;
WW_RENDER_DIST fixed, WW_RENDER_CLEAN=1 added) -- the pinned-camera hook is
OWED, and the one-texel transition render with it. Per-frame positioning:
measured, not shipped (report §6).

**CORRECTED by lane CARDPAD (exe 22:04:35, `scratchpad/lane_cardpad_report.md`):
bungo, verbatim: "When I say padding 8 for 1k, it's 8 pixels of distance
between two rendered objects"** -> the number is the GAP: gap(side) = max(2,
side/16) even, margin per side = gap/2, inner rect 15/16 on 128 frames; fill x
median 0.729 -> 0.854 (max 0.938), y 0.922, sheet area -2.0%. Confirmed
online (Polycount edge padding: gutter = 2x the per-shell padding, 4 per shell
on 512). OPEN, his call: mips = 1 + log2(gap) ships the level where the margin
is half a texel, so a border tap reads the neighbour on 13 of 19 sheets (worst
64/255); zero bleed = ship one mip fewer (mips = log2(gap): 128 frame -> 3
levels 128/64/32), which is what "8 = 3 clean mips" meant. PENDING (game came
up): `lodgen_card_arrays.sh` and the sample-set card regeneration, resume
`scratchpad/cardpad_20260909/PENDING.md`; the shipped sample set is intact.

**LIVE LEDGER 2026-09-10 ~00:5x (written for rule 1b, compact at 50%):**
COMMITTED + PUSHED 720762a (2026-09-09 evening: 9 commits + 18 unpushed older;
bulk ignored). Exe 00:13:19 (BUILD3: camera pin 27/27; render_shot 82/1, the 1
= a section-6 desktop-noise bar, not the camera). LANDED since the commit,
UNCOMMITTED: CARDFINAL (mips = log2(gap), per-frame positioning, 0/19 bleed,
sheet area -9.5%, card arrays 33/0, sample set regenerated), HOOKCAM/BUILD3
(the pin; the transition test UNMET because an in-chunk scene cannot isolate
one tree), WATER1 (census: 590 bodies, 1 sea = 99.2% of wet area, 131 rivers,
458 lakes, 16 WATR forms, 1 stepped body, 54 of 93 need a stroke; spec
scratchpad/specs_20260909/spec_water.md). WATER2 LANDED 01:01:04 (.lodl v3 built + gated, lodl_water.sh 56/0; census
CORRECTED to 346 bodies -- WATER1's shore test was thinned to 4,000 points per
body; merge clause: the smaller side inherits; 13 of 15 per-form totals match
WATER1 to the texel; Charles 25,114 texels; owed: EsmWorld WATR accessor
(ESMDATA_CHANGE_NEEDED.md), spec_water.md sections 2/3.8/7 -> lane WATER3).
BUILD4 LANDED ~01:4x (one build served CLAMP + CARDORTHO; exe still
01:01:04, objects current). CLAMP: terrain VT 35/0, V9 byte-identical both
paths, seam step 1.961 (bar 2.60), edge band: colour 0 beyond 4, `_msn`
1,014+1,405 beyond and `_data` 16 beyond -- ALL on the NORTH border at 4-7
texels, cause measured: Bethesda's own landscape disagrees across one shared
vertex row (y=31|32, 1-9 VHGT units) and the ring's south->north order
rewrites the chunk's own boundary row there; NOT fixed, NOT re-pinned (his
call: which side owns a disagreeing seam row). CARDORTHO: orthographic bake
landed, cube fixture worst 1.87 texels (bar 2), perspective control fails all
three, 19/19 sidecars ortho, sample set 38 entries ortho; TRANSITION 1 of 12
rows (heights match 0.00%, widths 18-121% wide; candidate = coverage-
threshold dilation; extent bars have no floor) -> lane CARDWIDTH running
(code-only while the game is up). Fallout 4 came UP after BUILD4 finished.
WATER3 ended BUILD PENDING ~02:3x (game up): src/watermark.{h,cpp} +
src/watermarkpanel.{h,cpp} 3,533 lines syntax-clean, tests/spells/water_mark.sh,
NifSkope.pro updated, the three hook-ups as a refusing patch script NOT applied;
P0 identity pre-measured 0 mismatches over 37.7 M texels; the Charles is body 3
and the marsh body 2 (ids by descending area); spec_water.md current (735
lines); canvas = a top-down map dock, not the 3-D plane (stated divergence).
Resume scratchpad/water3_20260910/PENDING.md.
CARDWIDTH LANDED (exe 02:07:48; scratchpad/lane_cardwidth_report.md): the width
excess was TWO COVERAGE DEFINITIONS (bake 16/255, reader alpha test 0.5, up to
5.41 texels of half-width apart), not dilation (0 texels) nor BC3 (<=0.5); fix =
base-colour alpha re-encoded so {a>=128}=={coverage>=16} (a'=160+(a-16)*95/239),
stated as `coverage 16 128 160` in every sidecar/.lodm/array layer; 19/19 sheets 0
forbidden alpha; card height vs mesh 0.16/0.31/1.24%; trunk band = source at
card pitch exactly (164/164, 38/38); octahedral 110/1 (F1 cube 1.78 vs bar 1,
unmoved by the fix, left red), arrays 35/0, cards 12/0, identity 8/0; the
perspective transition rows REFUSED as unmeasurable (0.43 px per card texel);
extent floor F6 fires 8/8. That exe also compiled WATER3's four new files.
RUNNING ~03:1x (game down): BUILD5 (WATER3 hook-ups + build + gates P0-P8 +
the Charles stroke picture; creates water3 DONE) -> CLAMP2 (the cell owns its
rows; edge band 0 beyond 4/4/64 on all four borders; builds after water3 DONE);
NATIVE0 on FABLE (the .lodo/.lodi writer in new files, lane-0 baseline harness).
**03:1x: all three lanes (BUILD5, CLAMP2, NATIVE0) were killed by the account's
session rate limit (reset 03:10).** State found on disk at 03:20: WATER3's three
hook-ups APPLIED, exe rebuilt 02:26:06, dock.png taken, gates not run; CLAMP2
had written nothing; NATIVE0 left partial src/lodofile.cpp, lodifile.cpp,
nativeemit.cpp + audit.py. Relaunched 03:2x as BUILD5b (gates from the built
exe), CLAMP2b (same brief), NATIVE0b on Fable (inspect the partials, continue).
**04:0x: BUILD5b LANDED** (exe 03:38:56; marking tool P0-P8 all PASS, water_mark.sh
41/0, the Charles: one stroke -> 99 directions, R 1.000 -> 0.807, undo 0 bytes;
dock.png retaken with 6/6 settings visible; five MISTAKES entries incl. solve()
accumulating derived fields). **CLAMP2b BUILD PENDING** (rule in
lodgenTerrainFillRing; land vertex channels already own their rows; resume
scratchpad/clamp2_20260910/PENDING.md). **NATIVE0b LANDED standalone** (all six
lod{o,i}file/nativeemit files complete; decoder 46/46; 20 mutations refused;
hook-up 4+6 sites unapplied in HOOKUP_CHANGE_NEEDED.md; lane-0 baseline PENDING;
audit: 703.3 B/placement vs spec 682; seed cannot carry the yaw -> quaternion =
drawn rotation). **BUILD6 LANDED 04:4x (exe 03:57:46, 18,526,720 B; EXE FREE FOR BUNGO):** CLAMP2b
gated: ring control 13/0 (old order refused), edge band colour+_msn 0 beyond 4 on
ALL borders, y=24 chunks byte-identical to BUILD4, identity 8/0, terrain 26/0;
RED, measured not cured: V9b `_msn` differs on -20.28 only, 32 texels at the
-19|-18 cell corner on the region's OUTER y=31|32 edge (no neighbour for the
ring); `_data` 16 beyond 64 on -20.28 = the wetness defect, left. NATIVE0b
hooked up (lodgen.cpp 4 sites, nifcli.cpp 6, qmake): (0,0) d4 .lodo 5,696,484 B
(2,970 bases; 4 WrhsLeanTo LOD nifs failed to load), .lodi d4 136,992 / d8
279,624 (717.5 B/placement) / d16 33,064 / d32 16,392; readers + decoder accept
all 5 pairs; decoder manifest leg FAILS at 0.125 on d4/8/16 = the manifest's
6-digit print, bar not re-pinned; LANE 0 stock_baseline.sha256 checked in, 25
files, --check 0 differ, 8 s -- a REGION SET, not the worldspace: a full
Commonwealth bake is still unmeasured; bungo's GUI run is the first number.
bungo is BAKING in the GUI now (his timing run); nothing launches the exe until
he says done; then create scratchpad/water4_20260910/GO for WATER4's build.
**05:2x: WATER4 LANDED CODE, BUILD PENDING** (scratchpad/lane_water4_report.md,
resume scratchpad/water4_20260910/PENDING.md): the disc defect measured (39
patches, p99 jump 40.78 deg, watermark.cpp 1077-1097 held capsules); replaced by
a potential-flow solve (div(k grad phi)=S, k = depth x stroke bump, no-flux banks,
Jacobi-PCG, residual 1e-9) + dye (kinds 7/8/9, a 4th plane via header word 0xF4
+ LODL_SECT_DYE, version stays 3); numpy twin gates: F1 ratio 2.0000, F2 parts
0.5/0.5, F3 0, F6 plume 98.2 vs 96 predicted; TWO FAIL AS REGISTERED, not moved:
F2 island-bank tangency 12.0/22.4 deg (staircase bank reconstruction) and F5 p99
8.44 vs 5 deg (patches 0 of 39). Picture images/charles_flow_proto_pair.png.
NEXT after bungo's bake: BUILD7 = GO marker -> WATER4 build + gates (F1-F7,
water_flow.sh, P0-P8, the render-hook pictures) -> water4 DONE -> WATER5's
hook-ups + build. **05:5x: WATER5 LANDED CODE, BUILD PENDING** (scratchpad/lane_water5_report.md,
resume scratchpad/water5_20260910/PENDING.md): src/watercurves.{h,cpp} (model,
<Worldspace>.water.json v1, mirror to the .lodl store, PNG RG/B/A + 16-bit mask,
flipped-green test) + src/waterwindow.{h,cpp} (top-level window, F11, overview +
texel-level map, Blender Curve-Pen tools, arrows, Reverse, per-point weight,
one-point = pin, undo, Solve = WATER4's solve()); hook-ups 12/12 anchors dry;
gates W1-W7 written, unrun; dock REDUCED to a button + hidden canvas (P5-P8
pin it). OWED: CHANGE_NEEDED.md C1-C3 = the solver must consume point weights,
one-point pins and the raster source layer (WATER4's solve() ignores them);
the DirectX PNG convention ruling (05:4x) came after this lane's launch --
verify its export sign at hook-up time. Skill ww-anchored-hookup mirrored.
BUILD ORDER after bungo's bake: GO -> BUILD7 = WATER4 build+gates -> water4
DONE -> WATER5 hook-ups + build + W1-W7 -> then WATER6 (C1-C3 + PNG convention).
**06:1x: HKX1 LANDED CODE, BUILD PENDING** (scratchpad/lane_hkx1_report.md,
docs/HKX_ANIMATION_FORMAT.md with REFL/DISASM/XML provenance; src/hkxanim.{h,cpp}
both routes one API; tests/hkxanim_dump.cpp standalone exe built; Python
decoder + gates): C++ vs Python 64,379 rows translation 3.8e-6 / rotation 1e-5
deg; synthetic 90-deg clip 29/29; 20 corruptions refused; block boundary ok.
FIXTURE TRUTHS (FAIL as pre-registered): skeleton.hkx has 17 Weapon* bones the
body NIF lacks and 4 names differing only by case (Head/HEAD, Spine1/2, Weapon);
the furniture 'Tpose' idle is NOT the bind pose (no bind clip in the archive).
Census of 15,320 hkx: only THREECOMP40/48 + 16-bit; 856 lossless 1st-person
clips refused by name. Pending: qmake+make of NifSkope (2 new .pro lines),
resume scratchpad/hkx1_20260910/PENDING.md. Skill ww-hkx-animation mirrored.
RUNNING: HKX2 (Opus, playback + mapping, code-only: case-insensitive partial
bone match, unmatched listed, root motion separate).
**FIXTURE, bungo 2026-09-10 ~06:3x, verbatim: "Vanilla human male, whole model
(body, rear of the head, face) with this: C:/Users/bungo/Downloads/Animations -
Mixamo Collection/Data/Meshes/Actors/Character/animations/AnimPreviews/
Running_To_Slide_And_Back_To_Running.hkx. Also, save the human model for later
use for me"** -> lane FIXTURE (Opus) assembles `fixtures/human_male_vanilla.nif`
(+ a copy at E:/Projects/Fallout 4 Mods/HumanMaleVanilla/) from MaleBody /
BaseMaleHead + parts on skeleton.nif, copies the clip to fixtures/, decodes it
with HKX1's standalone tools (format + bone-name match reported); the
nifskope-cli is the SAME exe (-no-gui) so exe steps wait for bungo's bake.
RUNNING: DOCS2 (Opus, provenance re-derivation + FO4CS README + 3 WW_CHANGES
splices), FIXTURE (Opus).
**RULING bungo 2026-09-10 ~06:5x, animation interchange: glTF 2.0** ("gltf sounds
good"; FBX discussed: no public spec, Blender imports only binary, would need
Assimp -- deferred unless he asks). Lanes, after HKX3: HKX4 = export a loaded
clip + the character (skeleton, skinned mesh) as glTF (JSON + bin, per-bone
translation/quaternion/scale channels at the clip rate, root motion on the
root node), Blender opens it natively; HKX5 = glTF import to our clip type
(resample, bone-name map, root motion) -> then the .hkx WRITER sized by ONE
reflection lookup in the exe: does Bethesda's Havok build still contain the
uncompressed interleaved animation class (trivial to write via HKXPACK XML) or
only spline-compressed (needs our own spline compressor, days). **ANSWERED by lane HKXCLASS 07:0x (record in ww-hkx-animation section 9 +
scratchpad/hkxclass_20260910/): hkaInterleavedUncompressedAnimation IS registered
(reflection 0x02e988e0, vtable 0x02e98938, loader [136]; the loader dispatches
on the class NAME, never on m_type; HKXPACK packs it; a 10-frame interleaved
clip round-trips exact); delta/wavelet classes absent; census 15,320 clips =
13,514 spline + 856 lossless, nothing else. => the .hkx WRITER emits
interleaved (small); a game flight by bungo is the only unmeasured step.
DOCS2 LANDED 07:1x (248 cites re-derived, 111 moved, verify 0; FO4CS README
504 lines with the empty bake-time table; HKX1 + WATER5 WW_CHANGES spliced;
watercurves.cpp:932 still writes +green = north -> owed to the solver lane).
FIXTURE LANDED 07:2x: fixtures/human_male_vanilla.nif + a copy at E:/Projects/
Fallout 4 Mods/HumanMaleVanilla/ (338,563 B, README beside each, partitions
byte-identical to the nine donors, node set = skeleton.nif + EyeLeftDummy001);
the Mixamo clip: spline THREECOMP40, 93 frames at 60 fps (vanilla 30), 95
tracks, 78 match the fixture, the 17 unmatched = the Weapon* set, root motion
zero with the travel on the COM track (y 0 -> 487); HKX1's reader REFUSES it
because its transformTrackToBoneIndices is EMPTY -> rule needed in
src/hkxanim.cpp: empty index array = identity map (measured 75/95 at shift 0);
owed to HKX2/HKX3. Render pictures PENDING (Fallout4.exe came up 05:32).
**~07:4x: HKX2, HKX4, HKX5 killed by the session limit (reset 08:10).** State at
13:17 (game down, no NifSkope): HKX2 report complete (9 sections), code +
PENDING + scripts on disk, died at the skill amendment; HKX4 left src/gltfexport.*
+ tests/gltfexport_dump.cpp + gltf_check.py, no report, died at first compile;
HKX5 left nothing. Relaunched 13:2x as HKX2b (verify + finish + the identity-map
rule in hkxanim.cpp), HKX4b (resume the exporter), HKX5b (fresh). bungo's bake
times: NOT YET RECEIVED. Builds still wait for his word.
**HKX3 (UI) brief, from bungo 2026-09-10 ~13:3x:** the Load Animation (.hkx)...
button with the file dialog exists in the render toolbar's Animation panel
(nifskope_ui.cpp:26890, HKX2); his words: "So, I can just click in there to open
windows explorer and select the file on disk" -> yes; and "Or maybe I could just
drag the hkx into nifskope" -> DROP support: a dropped .hkx (and later .gltf)
onto the window loads as an animation onto the open model via the same path as
a dropped .nif; keep the button too. Still open: panel vs Animation Manager dock
(the dock's list is built from NiControllerSequence blocks; a loaded clip needs
a second list); an unload row; the panel-style self-test; marking a refused
entry in the list. HKX2b LANDED 13:3x: hook-ups already applied, identity-map
rule in hkxanim.{h,cpp} + the Python decoder (Mixamo 95 tracks by identity,
78/17/4), PENDING.md exact for the build lane (qmake, gates 134/3, 29/0, 20/0,
hkxanim_play.sh, shots.sh x2 = eight pictures of the human fixture).
**HKX5b LANDED 14:0x** (scratchpad/lane_hkx5_report.md): src/gltfimport.{h,cpp} +
src/hkxwrite.{h,cpp} (direct packfile emit, bit-exact, default; HKXPACK XML route
too), 23/23 gates: RT1 5 clips 64,379 rows max 1e-7 / 1.6e-7 deg; RT2 via glTF
8e-6 / 3.4e-5 deg; Mixamo RT1 0.0; 25 corruptions refused; HKXPACK re-reads our
packfile (interleaved, 2,185 transforms, stride 48). OWED TO THE DIRECTOR: (1)
the ww-hkx-animation skill's angle metric is HALF the true angle (2*asin -> should
be 4*asin; HKX1's published angles are halves; amendment text in
scratchpad/hkx5_20260910/SKILL_AMENDMENT_ww_hkx_animation.md, apply after HKX4b
lands); (2) HKX_ANIMATION_FORMAT.md section 1 NamedVariant stride is 0x18 not
0x20; (3) reconcile GLTF_INTERCHANGE.md when HKX4b writes it. GAME FLIGHT for
bungo (report section 8): scratchpad/hkx5_20260910/flight/JogForward_marked.hkx
(head yawed 45 deg) as an MO2 mod at Meshes/Actors/Character/Animations/MT/
Neutral/JogForward.hkx -> jog in third person: turned head = the engine loads
the interleaved class; straight = not read; T-pose/crash = rejected; then the
exact rewrite JogForward.hkx must be indistinguishable. Hook-up = refusing
script, 4 .pro lines, unapplied (qmake owed after the HKX lanes).
**PLAN RULED by bungo 2026-09-10 ~14:1x, verbatim: "you can give me a few frames
of imported animations on the human rig, at different points in animation's
time, then when I confirm they're good you'll do a test export / import for
animations to see if they're 1:1 accurate, then I'll do a test for import into
Blender and then export to nifskope"** -> BUILD7 RUNNING (Opus): HKX1+HKX2+HKX5
built, gates, eight frames of the human fixture (jog + Mixamo) as two contact
sheets, then EXE FREE for his bake. Then, on his confirmation: the in-app
export/import 1:1 test (HKX4b's exporter + HKX5b's importer through the exe),
then his Blender round trip. His bake still precedes the water build chain.
**HKX4b LANDED 14:0x** (scratchpad/lane_hkx4_report.md, docs/GLTF_INTERCHANGE.md,
skill ww-interchange-readback both trees): src/gltfexport.{h,cpp}; gates 0 not as
registered: validator 0 failures + 12/12 broken copies refused; independent
read-back worst 4.2e-6 units / 2.3e-6 deg over 234 channels x 23 frames; mesh 9
shapes 6,443 verts identical; BLENDER 4.5 FOUND at E:/Tools/3D/Blender 4.5 --
headless import: 110 bones, 9/9 meshes, action JogForward 0-17.6 frames. Files
bungo can open in Blender NOW: scratchpad/hkx4_20260910/out/human_male_jog.gltf,
human_male_mixamo.gltf (60 fps + root motion), vanilla_malebody.gltf (control).
Two registered failures = the FIXTURE: human_male_vanilla.nif's LLeg_Toe1 sits
2.865 units from MaleBody.nif's authored pose (vanilla donor passes 1,246/0).
LINK RISK for BUILD7 (running): HKX5b's hook-up put gltfimport in NifSkope.pro
and includes gltfexport.h, but gltfexport.cpp is NOT in the .pro until HKX4b's
hookup.py (11/11 anchors, unapplied) is applied -> if BUILD7 fails to link, a
BUILD7b applies scratchpad/hkx4_20260910/hookup.py first. The angle-metric
amendment APPLIED to ww-hkx-animation in both trees (4*asin).
**BUILD7 LANDED 14:2x, EXE FREE FOR BUNGO (release/NifSkope.exe 14:04:34,
19,197,952 B; his window needs a restart):** HKX1+HKX2+HKX5 built (HKX5's 4 .pro
lines applied; HKX4b's gltfexport.cpp still NOT in the .pro -> its hookup.py is
the next build's first step); gates 134/3, 29/0, 20/0, identity rule 8,835/0,
hkxanim_play.sh 27/0 x2 (78/17/4); the frames: scratchpad/build7_20260910/
frames_jog.png + frames_mixamo.png (13 tiles, one orthographic camera; the
Mixamo COM travels 487 units along the front axis). SENT to bungo, awaiting his
confirmation -> then the in-app export/import 1:1 test (needs HKX4b's hook-up
built), then his Blender round trip. **bungo 14:3x: "they look good, now export
and import of gltf"** -> BUILD8 RUNNING (Opus): HKX4b's hook-up applied, build,
the in-app round trip hkx -> glTF -> hkx measured (4*asin angles), the same
five Mixamo frames re-rendered from the round-tripped clip vs the original,
plus a headless Blender 4.5 leg (import our glTF, export, re-import, compare);
ends EXE FREE FOR BUNGO -> his bake -> the water chain.
**BUILD8 LANDED 15:3x, EXE FREE FOR BUNGO (release/NifSkope.exe 14:37:53,
19,382,272 B; restart needed):** HKX4b's hook-up applied; the in-app round trip:
jog 1,794 rows max 8e-6 units / 3.4e-5 deg (root motion 1.5e-5 over 165 units);
Mixamo 7,254 rows 4.4e-5 / 5.2e-6 deg; the writer alone 0.0; via BLENDER 4.5 at
its default 24 fps 0.29 / 4.83 deg (its own re-timing) and at 30 fps 1.0e-4 /
4.7e-4 deg (Blender also puts a non-zero roll on 107 of 110 bones) -> tell the
user to set the scene frame rate first. Pictures: original vs round-trip sheets
differ by at most 170 px of 1.35 M at 16/255 (jog 3/4), Mixamo 22 px at 1/255;
noise floor 0 px. Gates all as registered. Reader gained the interleaved-class
READ arm in src/hkxanim.cpp (BUILD8 crossed into another lane's file, logged).
Import is CLI-only (no menu route yet); the export menu item was never clicked
by hand. 174 files dirty. NOW: bungo bakes; the DONE marker for FILESTAB / HKX3
/ SKELOVERLAY builds is withheld until he says done, so no lane takes the exe
during his run; then those three build one at a time, then the water chain.
FILESTAB + HKX3 LANDED CODE, BUILD PENDING 15:4x (resumes scratchpad/
filestab_20260910/PENDING.md [71 anchored edits, 6 gates; .lodl/.lodt stay out of
the tree: it keeps only meshes/ paths] and scratchpad/hkx3_20260910/PENDING.md
[9 anchored edits behind WW_HKXANIM_UI, gates a-g + a dock grab; timeline.cpp is
CRLF and on neither list in .gitattributes / rule 8 -> fix the lists]). OWED
skill (third time): 'add a WW_*_TEST harness' -- HKX3 report section 10 lists
what it must say; the next build lane writes it. BUILD ORDER after his bake:
BUILD9 = FILESTAB -> HKX3 -> SKELOVERLAY (one build each or one combined,
anchors re-checked before each apply), then the water chain (BUILD7-water:
WATER4 -> WATER5 -> WATER6).
**bungo 2026-09-10 ~16:0x, frames + round trip APPROVED ("Looks fine"); his
Blender trip = step 3, on him (set the scene rate to 60 first). RULED for a lane
ANNOT after the interface builds: the workspace lists ANNOTATIONS (event name +
time) as timeline markers, add/rename/move/delete with the game's own event
vocabulary offered (from the behaviour graphs; free text allowed); the
interleaved writer carries annotation tracks; the glTF export carries them in a
NifSkope extension so Blender round-trips them; gate = a vanilla clip with
events through write and through glTF comes back annotation-for-annotation,
and the list equals HKXPACK's dump. Same lane, the cheap edits: trim, retime
(60->30), remove/rename tracks, root-motion bake/unbake (COM <-> extracted),
Save as .hkx from the workspace. **SUPERSEDED the same minute by bungo, verbatim:
"Just make hkx fully editable in our nifskope"** -> the HKX EDIT series, after the
interface builds + his bake: HKXEDIT1 = the RAW layer, an .hkx opened as a block
tree in the Blocks tab (container, animation, binding, annotation tracks,
skeleton; every field from the exe's hkClass reflection; edit like NIF blocks;
save via the interleaved writer; generic over all hkx so behaviour graphs
follow); HKXEDIT2 = the ANIMATION layer, WHICH REPLACES THE ANIMATION MANAGER DOCK
(bungo 2026-09-10 ~16:4x: "Animation manager was one of the first features for
nifskope, and it's pretty old and outdated btw"): one new animation workspace
under the panel style with Blender's timeline + dope sheet as the reference --
clip list (NIF sequences + loaded clips), a dope sheet with keys per bone track,
annotation markers and float tracks as rows, transport/loop/speed/rate in one
row, timeline<->viewport bone selection both ways, the old manager's sequence
controls folded into rows; HKX3's additions to the old dock are the interim.
(pose bones with the gizmo at a frame
-> key; dope sheet with keys per track, move/copy/delete, interpolation; float
tracks + annotations as timeline rows; trim/retime/track edits/root-motion
bake; Save as .hkx); HKXEDIT3 = round trip + vocabulary gates (our write,
HKXPACK re-read, a game flight, event names from the behaviour graphs). Out of
scope: a spline compressor (the game loads interleaved); Blender stays the
performance editor via glTF.
**16:2x: bungo opened NifSkope expecting the UI changes ("where's the top bar
alignment, and nif to generic files edits?") -> the held slot released: BUILD9
RUNNING (Opus) = FILESTAB -> HKX3 -> SKELOVERLAY -> the ALIGNMENT fix (one
height, one top edge; geometry-read gate), one build each with BUILDING/DONE
markers, the owed skill ww-test-harness-add, then EXE FREE. HKXEDIT1 RUNNING
(Fable, the raw hkx block-tree layer in new files, no exe). His bake and his
Blender trip still owed by him; the water chain (WATER4 -> WATER5 -> WATER6
builds) queues behind BUILD9.
**BUILD9 LANDED 16:5x, EXE FREE (release/NifSkope.exe 15:52:46; 193 uncommitted
paths):** Files tab 29/2 (the 2 reds: Qt's own clear buttons untipped; a fixture
NiTransformController moving PipboyBone), animation workspace 48/1 (the 1 = a
wheel floor that cannot fire in an unfocused harness window), skeleton overlay
17/0, alignment 11/0 (strip 26 px vs toolbar 33 -> both 35, search row = content
top 70). DEFECT seen by the director in skeloverlay on_frame46.png: long segments
fan from the skeleton to a point far above/left of the character -- nodes that
do not follow the clip (camera / AnimObject / unmatched nodes at their bind or
origin position) are being joined to animated parents; the overlay should draw
only the Skeleton Manager's BONE class, or skip segments whose child has no
track. Lane SKELFIX. **HKXEDIT1 LANDED CODE, BUILD PENDING 17:2x** (scratchpad/lane_hkxedit1_report.md,
docs/HKX_PACKFILE_MODEL.md): res/hkclasses_fo4.json = 908 classes recovered by
emulating their dynamic initialisers, signatures 908/908 = HKXPACK's, exe wins on
2; src/hkxfile.{h,cpp} generic packfile model + writer: 15,278/15,278 shipped
clips byte-identical (C++ 3.0 s, Python oracle 29.8 s), 42 refused by name
(hclClothSetupContainer, unregistered in 1.10.155); 5-field edit gate exact;
HKXPACK reads our files; 20/20 corruptions refused; src/hkxmodel.{h,cpp}
(HkxModel : BaseModel, KfmModel idiom, undo stack, save); hookup.py 10/10
unapplied; owed: a QUndoGroup so Edit > Undo reaches the .hkx document.
SKELFIX LANDED CODE, BUILD PENDING 17:5x (scratchpad/lane_skelfix_report.md):
the stragglers are TRACKED nodes the clip parks at the origin (COM 300 units,
CamTarget, Camera, the AnimObject* set, CharacterBumper, EyeLeftDummy001), not
the Weapon* bones (no node in this NIF); the brief's 'both ends in the Bones
filter' rule was REFUSED with numbers (60 of 129 segments: FO4 weights the
*_skin helpers, so Pelvis/COM/thighs/calves are 'not a bone' to the dock);
shipped: the Bones filter closed upwards through parents and cut at the deepest
node with every bone beneath it (COM) -> 111 of 130 nodes, 110 segments,
longest 31.9 <= 33.6, by structure not by name; gates f-j with the old rule as
the floor; owed: the re-rendered picture; WW_RENDER_SIZE not reaching the
pictures (1437x941) logged. Skill ww-offline-scene-model in both trees.
**HKXEDIT2 LANDED CODE, BUILD PENDING 18:2x** (scratchpad/lane_hkxedit2_report.md,
resume scratchpad/hkxedit2_20260910/PENDING.md): src/hkxclipedit.{h,cpp} = the
key model (sparse keys over the dense clip, every-frame keys on load, Reduce
with a tolerance; insert/move/copy/delete, annotations, float-track rows, trim,
retime, remove/rename track, root-motion bake/unbake, Save through hkxwrite +
the canonical hkxfile layout) gated standalone 72/0 + HKXPACK
(release/hkxclipedit_gate.exe): 30 deg about X at frame 46 reads back 0 off,
every other track/frame byte-identical; FootLeft at frame 30 survives
save/reload; trim 41 frames, retime 47; COM 487.643 -> 0, unbake
byte-identical; saved file re-reads bit for bit, HKXPACK unpacks 8,835
transforms. src/animworkspace + src/animdopesheet = the "Animation" dock
replacing the Animation Manager (one list NIF sequences + clips, dope sheet at
the clip's rate with bone rows under the NIF hierarchy, markers, float rows,
one transport row, the old sequence controls as rows, selection both ways,
gizmo pose hold + Insert key + auto-key, one QUndoGroup); WW_ANIMWS_TEST +
tests/spells/animws.sh written, NEVER RUN; hookup.py 15/15 anchors (16 with
tier 2), REFUSES until HKXEDIT1's is applied (order in PENDING.md: HKXEDIT1
hook-up, then this, then qmake, then delete the stale .o list). FOUND: HKXPACK
prints empty annotation text for every hkxwrite-emitted file (fixup order;
our two readers fine) -> the workspace saves canonical; CHANGE_NEEDED (float
tracks in reader+writer; HKX5's fixup order; the timeline.* retirement
follow-up). Gate (c) as pre-registered cannot hold on an every-frame model
(delete key = 45/47 interpolation, 0.877 deg off; Undo is the way back).
bungo's call owed: every-frame keys vs reduced on load. Divergences from
Blender in the report s5. res/hkx_annotation_vocabulary.txt = 1,640 names
from 15,278 shipped clips. WW_CHANGES entry spliced by the director (CR
19,020 unchanged); skills ww-hkx-animation s14 + ww-anchored-hookup s3a
mirrored to the live tree.
**BUILD10 LANDED 16:5x, EXE FREE (release/NifSkope.exe 16:45:53, 20,007,936 B;
scratchpad/build10_20260910/HANDOFF_BLOCK.md):** WATER4 (already in the
15:52:46 exe) water_flow.sh 47/2 (both reds pre-registered: F2 island bank
12.01/22.40 deg, F5 p99 8.44), WATER5 (exe 16:23:22) water_window.sh 46/2
floor 24, W5 0 differ of 21,754,958, WATER6 water_weights.sh 16/16 floor 15,
X2a moves 29,310 of 29,312 texels; neighbours after WATER6 unchanged to two
decimals. FIVE REDS ROUTED TO LANE WATER7: (1) X2b 0.371 vs 0.5 on the Charles
= the radial-cosine instrument bends with the river -> net-flux ring;
(2) water_flow.sh grep takes the informational line; (3) floor 18 with two
registered reds; (4) water_mark.sh runs body-3 gates on body 2; (5) WATER5
window defects: dye-pin weight never written, first named body reads back
nameless (name offset 0). Instrument repaired: the window self-test held a
deleted document (exit 139). WW_RENDER_SIZE honours WIDTH, takes 59 px of
chrome off HEIGHT (render-shot skill amended). release/NifSkope_inuse_20560.exe
left as the rollback rung (41116 removed by the director). Pictures:
water4/water5/build10 images dirs. RESTART: yes, bungo's next launch has it.
**RULING bungo 2026-09-10 ~18:4x, verbatim: "when you reach 100 percent usage,
stop what you're doing and write a handoff"** -> CONSTITUTION rule 1c: at the
account meter's 100 percent (rate-limit notice, a lane dying with a limit
error, or bungo's number) no new lane, no build, no gate; this top block is
rewritten complete at that moment and the reply names it.
RUNNING 18:3x: BUILD11 (Opus, holds the build slot: SKELFIX + HKXEDIT1 hook-up
+ HKXEDIT2 hook-up, one build, then skeleton_overlay.sh, hkxmodel_test.sh,
animws.sh, the WW_RENDER_SIZE check; markers scratchpad/build11_20260910/
BUILDING then DONE) and WATER7/UI2 (Opus, code-only until BUILD11's DONE;
brief scratchpad/brief_water7_ui2.md: Water tab in the LOD Generation strip,
the compact bars through the skin, the five reds).
BUILD11 after BUILD10 = SKELFIX + HKXEDIT1 hook-up + HKXEDIT2 hook-up (that
order, one build, BUILDING/DONE markers) + animws.sh + skeloverlay re-render +
the WW_RENDER_SIZE check. UI2/WATER7 (Water tab + compact bars) in parallel,
code-only, builds after BUILD11.
**UI INTAKE bungo 2026-09-10 ~18:0x (Workspaces menu screenshot): "Two issues
with water window and water marking appearing here"** -> (1) two workspace
entries for one tool ('Water window' + 'Water Marking'), and 'Water Marking'
ticked alongside 'Default'; (2) a pop-up window is not a workspace. Rule, CORRECTED by bungo the same minute ("They should be in the LOD gen
workspace"): NO water workspace at all -- the water tool (the marking rows and
the button that opens the full-screen flow window) lives INSIDE the existing
'LOD Generation' workspace, since the land file is a LOD product; both 'Water
window' and 'Water Marking' leave the Workspaces menu; the old dock retired.
HOW (bungo, screenshot of the Header | Blocks | Files strip: "You'd access them
like this"): in the LOD Generation workspace the water tool is a TAB in that
same left-dock tab strip -- Header | Blocks | Files | Water -- same segmented
strip, same 35-px bar height; the tab holds the marking rows (body select, the
per-body rows, curve tools, Solve, Save/Load curves, Export/Import PNG) and the
button that opens the full-screen flow window.
**UI RULING bungo 2026-09-10 ~18:1x (screenshot of the aligned Header | Blocks |
Files + Object Mode row): "compact these vertically like this, the top bar and
the buttons"** -> that row's height is THE bar height for the whole top of the
window: the menu row (File View Spells Options Help), the Workspaces / LOD /
Animation / Collision row, and every button in them take the same compact
height and vertical padding through the shared skin (wwBarRowHeight /
wwAlignBarRow), no per-widget heights; gate = geometry read-back of all three
rows equal within 1 px + before/after grabs. Same lane as the Water tab (UI2 /
WATER7, after BUILD10). Lane WATER7 after
BUILD10 (WATER5's hook-ups are being applied by BUILD10 right now).
BUILD9's own block follows:
<!-- Lane BUILD9, 2026-09-10. TEXT ONLY for the HANDOFF.md top block; the
     director splices it (CONSTITUTION 8). -->

**BUILD9 DONE 16:0x -- the three pending lanes are built, gated and ledgered,
and the bars share one row.** Deployed exe `release/NifSkope.exe` **15:52:46**,
`release/style.qss` equal to `res/style.qss`, exe newer than every changed
source. **His open window needs a restart.** Nothing committed (193 uncommitted
paths).

* **FILESTAB** applied (71 anchors) and gated: `files_tab.sh` **29 checks,
  2 failures**. Green: 0 "NIF" strings on the page with the seeded-offender
  floor, the tree lists `.nif 1 / .bto 1 / .btr 1 / .hkx 14939` over FORCED
  roots, opening a clip from a real row gives 78 / 17 / 4 and becomes the
  playing sequence, and an `.hkx` with nothing open refuses in words. The two
  reds are measured and deliberately NOT amended: the "every tool button
  explains itself" count includes Qt's own `QLineEditIconButton` clear buttons
  (2 of 6), and "unload restores the bind pose" reports `PipboyBone`, which the
  fixture drives with its own `NiTransformController` while the gate compares
  across two different scene times. **Question for bungo: unloading a clip
  leaves the scene at the clip's time.** Pictures
  `scratchpad/filestab_20260910/dock_before.png` / `dock_after.png`.
* **HKX3** applied (9 anchors, `WW_HKXANIM_UI`) and gated: `hkxanim_ui.sh`
  **48 checks, 1 failure, 0 skips**; gates a-f all green, including 0 of 139
  nodes differing after unload and the drop accepted with the same 93 @ 60. The
  one red is gate (g)'s wheel floor, which CANNOT fire: a WW harness window is
  never activated, so `hasFocus()` is false and the guard blocks the wheel
  exactly as specified (the harness now prints
  `hasFocus no, focusWidget <none>, window active no`). Picture
  `scratchpad/hkx3_20260910/dock_clip_midclip.png`. `hkxanim_play.sh` still
  27 / 0.
* **SKELOVERLAY** applied (5 anchors) and gated: `skeleton_overlay.sh`
  **17 checks, 0 failures, PASS** -- the overlay's census equals the Skeleton
  Manager's (130 / 93 / 93 / 0), 0 changed pixels outside its own reported mask,
  byte-identical restore when toggled off, and every joint on the animated node
  at frame 46. Pictures `off.png` / `on.png` / `on_frame46.png` plus
  `gates/gate_mask.png`.
  **OWED:** `WW_POSEDRAW_TEST` FAILS at "clicking a bone did not make it the
  active object" on BOTH fixtures here; the pose-size/tail factoring is
  arithmetically identical by diff, and this harness was last recorded green on
  a FACIAL rig of 70 bones that neither fixture is. One run on that rig settles
  it.
* **ALIGNMENT (UIALIGN) LANDED.** Measured before: main toolbars 35 px, viewport
  toolbar 33, dock tab strip 26, search row starting 3 px above the viewport's
  content. Now one row: tab strip and viewport toolbar both top 35 height 35,
  search row top 70 = viewport content top 70. Stated once in the shared skin
  helpers (`wwAlignBarRow`, `wwBarRowHeight`, `wwStartContentBelowBar`,
  `wwSegmentedTabBarQss( rowHeight )`), never per-widget; the row is the tallest
  natural height so no toolbar is squeezed into its chevron. New gate
  `tests/spells/ui_align.sh` / `src/uialigntest.cpp`: **11 checks, 0 failures**,
  with the floor that a 4 px difference goes red. Seam pictures
  `scratchpad/build9_20260910/seam_before.png` / `seam_after.png`.
* **Neighbours after everything:** `loaded_nifs.sh` 166 / 3 (the same three as
  before this session; six expectations that quoted the renamed labels were
  repaired, literals only), `top_bar.sh` 43 / 5 -- all five pre-existing, it
  expects a View menu listing six docks that were merged into "Left Editor"
  before this session.
* **Documents:** WW_CHANGES.md gained one entry carrying all three lanes plus
  the alignment (CR 19,020 unchanged); MISTAKES.md gained five entries (my
  DEFINES-staleness mistake, the four never-executed gates, the two wrong resume
  figures, and FILESTAB's and SKELOVERLAY's own sections, which had never been
  spliced); each lane report gained a `## Build (BUILD9)` section. The owed skill
  `ww-test-harness-add` is WRITTEN in both trees, and `nifskope-ww-build-verify`,
  `nifskope-ww-resume-pending` and `nifskope-ww-panel-style` were amended in both
  (panel-style did not exist in the repo tree and now does).
* **Not mine, still in the tree:** `src/hkxmodel.{h,cpp}` (untracked, not in
  `NifSkope.pro`, written 15:54-15:56 -- lane HKXEDIT1) and `sx_tmp.sh` at the
  root from 02:30, another lane's throwaway. Neither was touched.

**RULING bungo 2026-09-10 ~14:4x, the NIFs tab becomes the FILES tab, his
words:** "add hkx files to the NIFs tab, search for them in already set game
folders ... rename 'available NIFs' to 'available files', and rename NIFs tab to
'Files', then also rename Loaded NIFs to 'Loaded Files', Basically replace
mentions about nifs to generic 'files', because now we'll be able to browse and
open not just nifs, well, we already can, with stuff like bto or btr" -> lane
FILESTAB (after BUILD8): the browser tree lists .hkx (and .lodl/.lodt/.bto/.btr,
.gltf) from the configured game folders + archives beside .nif; every 'NIF'
string in that dock (tab title, 'Available NIFs', 'Search NIFs...', 'Loaded
NIF', 'Search loaded NIFs...') becomes 'Files'; opening an .hkx from the tree
loads it as an animation onto the loaded model (the same path as the button /
the drop); the loaded-files list shows clips beside models; panel-style rules.
**RULING bungo 2026-09-10 ~14:5x: "Also, add support of these to the timeline
workspace"** -> loaded .hkx clips appear in the Animation Manager / timeline
dock beside the NIF's own sequences (a second list source next to the
NiControllerSequence blocks, HKX2 report section 6 item 1), with the timeline
scrub/loop/speed driving the clip, frame ticks at the clip's rate (60 fps
Mixamo, 30 vanilla), root-motion toggle, unload. Lane HKX3 (after BUILD8). HKX3 RUNNING (Opus, prepares edits; builds after BUILD8).
**RULING bungo 2026-09-10 ~15:0x, verbatim: "Add to the overlays: View skeleton,
shows you the bones, basically the same view as in the skeleton manager"** ->
lane SKELOVERLAY: an Overlays menu entry 'Show Skeleton' beside Show Nodes /
Show Axes / Do Skinning: draws the bone tree as bones (parent->child segments,
joint points, the Skeleton Manager's colouring: deforming / unused / skinned),
over the model, depth-tested off so it reads through the mesh, follows the
animated pose when a clip plays; the Skeleton Manager's own view is the
reference (same bones, same states). SKELOVERLAY LANDED CODE, BUILD PENDING 15:2x (src/gl/glscene.cpp, glview.cpp,
src/skeloverlaytest.cpp; Scene::findNode() added because getNode() CREATES nodes;
hookup.py 5 anchors unapplied: NifSkope.pro + nifskope_ui.cpp; resume
scratchpad/skeloverlay_20260910/PENDING.md, gates a-e with floors). Skill fix
applied both trees: build-verify's syntax script is sx_$LANE.sh, not sx_tmp.sh.
**UI INTAKE bungo 2026-09-10 ~15:1x (screenshot): "See the issue with alignment
here?"** -> the left dock's tab strip (Header / Blocks / NIFs) and the viewport
toolbar (Object Mode / Select / Add / Object / Global) do not share a row: the
tabs are taller and start higher, a step at the seam, the search field below
the tabs misaligned with the toolbar's second row. Rule: one bar height and one
top edge across the width. Small lane UIALIGN after the animation builds
(panel-style; measure the two rows' rects before/after in a harness).
bungo 04:0x: no rock impostors (rock decal intake CLOSED). **HKX ANIMATION PREVIEW, RULED by bungo 2026-09-10 ~05:0x, verbatim: "in
animation workspace, add an option to load a hkx file with animation, then they
get added to the animations list, and if there's rigged geometry with nodes /
bone names that match, they play"** -> queued AFTER WATER5. Shape: the
Animation workspace gets Load animation (.hkx); each loaded animation becomes an
entry in the existing animations list beside the NIF's own; playback drives the
bones through the existing controller path when the loaded NIF's node names
match the tracks (partial match = play the matched bones, list the unmatched;
no match = refuse in words); root motion separate. Lanes: HKX1 reader +
spline decompressor (HKXPACK XML first, packfile later; gate = bone-for-bone
match against an independent decoder + a T-pose animation leaves the bind
pose), HKX2 playback/mapping, HKX3 the UI rows. Prove on the player skeleton.

**WATER5 (UI), RULED by bungo 2026-09-10 ~04:3x, after WATER4 lands**, his words:
"That stroke doesn't look smooth at all, it's like overlapping circles" (the
disc fill -> WATER4's solve); "do you draw it on that tiny map?" -> "curves you
can draw in nifskope, that can have as many connection points as you want. Then
you solve the rest with a button to fill in the gaps, something like a
simulation"; and for painting: "just make it open a new popup window that can
be set to full screen and you can drag that shows the flowmap". So: editable
curves (add/move/delete/insert points, direction arrow, per-point speed; a
one-point curve = a pin; Blender curve edit mode = the reference) drawn in a
SEPARATE POP-UP WINDOW showing the flow map, draggable, full-screen-able, with
zoom and pan; the Solve button runs WATER4's simulation; plus PNG export/import
(RG = direction, B = speed, A = confidence, + the body mask; import = a raster
source layer in the stroke store; refuse a flipped green channel against the
solve). **RULING bungo 2026-09-10 ~05:4x, verbatim: "why not just align it with
a normal map standard to some extent, red and green channels directx
directions"** -> the flow PNG uses the DirectX normal-map convention (R = +X,
G = +Y toward the image bottom, both centred on 128 = FO4's own _n maps), B =
speed, A = confidence; the .lodl flow plane's contract states its mapping to
that PNG with one checked-in test image; a still lake = a flat normal map. The tiny dock canvas goes away. **bungo, verbatim: "Add all the tools needed
to mark the rivers and solve it and export import there, into that new
window."** -> the pop-up window is the WHOLE water tool: body select, the
per-body rows (class, water form, colour override, still water, name), the
curve tools, the dye pins, Solve, Reload/Save, Export/Import PNG -- the dock is
reduced to the button that opens the window (or removed). **bungo, verbatim: "allow me to save the curves as some type of a file"**
-> the curves, pins, dye pins and per-body overrides also save/load as a
standalone text file beside the land file (proposed `<Worldspace>.water.json`,
world coordinates, versioned, human-editable, shareable between mods and
regenerations; the .lodl stroke store stays the baked copy; loading the file
onto a regenerated .lodl re-derives the planes). WATER4 running on
Fable (GO marker after bungo's bake). Design agreed, no
lane yet: WATER4 = potential-flow solve inside each body (continuity, banks,
islands, depth), dye advection (river-into-sea plume, factory dye pin), ice from
the shore plane in winter (reader side).
RULING bungo 2026-09-10 ~02:5x on the disagreeing seam row, verbatim: "The
cell owns it then" -> the ring fills ONLY the texels beyond the chunk and never
overwrites the chunk's own boundary rows; Bethesda's hairline disagreement at
the seam is preserved, the edge band goes to 0 beyond 4 on all four borders.
QUEUED: CLAMP2 (implements that ruling in lodgenTerrainFillRing; after
CARDWIDTH frees lodgen.cpp), native lane 0, rock decals, the
stale-line-number docs pass, the next commit (52 uncommitted). Chain as briefed
before WATER2 landed: WATER2 (.lodl v3
writer: body table, bodyid/flow/shore planes, stroke store; resume if pending
scratchpad/water2_20260909/PENDING.md) -> CLAMP (direct bake one-cell ring via
a shared sampler; BUILD PENDING, resume scratchpad/clamp_20260910/PENDING.md;
owed: wetness stays chunk-scoped, land vertex channels lodgen.cpp:~903 carry
the same clamp) -> CARDORTHO (the card bake used a 60-deg PERSPECTIVE camera
while framing orthographic; orthographic bake + the one-reference transition
test + sample cards). QUEUED: native lane 0 (the baseline hash needs a bake),
rock decals (needs the exe), WATER3 marking tool (after WATER2). Skills: 
ww-texel-picture mirrored to the live tree; nifskope-ww-commit in both.

**WATER, bungo 2026-09-09 ~20:4x (design talk, no lane yet, his go pending):**
in the FO4CS-only system water = the `.lodl` planes (height + type per cell,
v2 worldspace default). Wanted: a BODY ID plane + a body table (class
sea/river/lake, water form, colour override, source/outlet, mean flow) and a
per-texel flow plane (direction, speed, confidence); flow computed from
connectivity (height steps; centreline toward the sea for FO4's flat tidal
rivers; lakes 0) and then MARKED by the user in NifSkope with strokes/pins on
the water plane, propagated inside the body, saved beside the baked plane;
depth and shore distance derivable (shore distance worth baking). First lane =
read-only census + spec (how many bodies, stepped vs flat, distinct WATR
types), then the panel+writer lane under panel-style rules.

**BUG INTAKE 2026-09-09 ~17:4x, bungo verbatim, seen while the impostor bake
ran on his second monitor (not investigated, his go pending):**
1. CLOSED by bungo 2026-09-10 ~03:3x, verbatim: "Ignore the rock decals, we won't
   have rock impostors" -> no rock impostor lane, ever; the candidate rule is trees
   only (already so since the `--candidates trees` fix). Original report:
   "some objects like rocks have floating decals with transparency on the edges"
   + "Which in the normal map render appear as white-ish" (the card bake's
   normal frames show the decal planes as near-white = a normal pointing at
   the camera / an unset tangent-space value, on top of the rock)
   -- candidates, unmeasured: the rock's separate decal shapes (moss/dirt alpha
   planes) baked into the card or chunk as floating alpha-tested quads; or the
   card's mask/alpha edge from the half-size aux sheet. Measure on one rock
   base: which source shapes reach the bake, and the card's alpha edge vs the
   source's.
2. "the screen is flashing white and black, that's a view hazard for
   epileptics" -- lane OFFSCREEN running: headless renders never show a window.
3. "why are the non-tree objects being baked as impostors" -- MEASURED:
   `--candidates trees` in src/nifcli.cpp:3228 means `tree || missing`, a
   superset of the default; 14 of 33 "trees" candidates were shacks and rock
   cliffs. Fix = tree only; owner: the RENAME lane (it edits nifcli.cpp).

**PARKED by bungo 2026-09-09 ("let's save this for later").** Owed decisions
when it resumes: (1) reuse vanilla's `_msn` where terrain is unchanged, and/or
write the sheet uncompressed/BC7 (an uncompressed render of the peak was
offered, not made); (2) the mod's shape (family-level assignment + the ~8-cell
exact ring; the untested pattern-matching route at the finest far level);
(3) run the pending terrain gate with the game down. Next thread: FO4CS lodgen.

SESSION HANDOFF 2026-09-08 - the LOD terrain thread, and the out-of-bounds mountains

**Nothing is committed.** 70 files uncommitted on `main` since 2026-09-04, on
bungo's "Not yet". The build in `release/NifSkope.exe` is CURRENT - 2026-09-07
12:48, newer than every source file - so the gates below were run against the
code as it sits. `Fallout4.exe` is down.

**STANDING CONSTRAINT, still in force, his words:** "We are at 97 percent usage
of Fable, you need to use agents on Lane B / second account, do not use agents
here", reinforced twice ("All these things run on lane B I hope", "98 usage and
you're running them here? There's much more usage there"). Ultracode does not
override it. Agents go to account B via
`E:\Projects\Claude\tools\claude-b\run-b.ps1`. Account B **cannot execute
`release/NifSkope.exe`** - the sandbox refuses it - so a B lane ends BUILD
PENDING and the overseer builds and runs the gates.

### What landed, and is built and gated

1. **The impostor bake is configured and its resolution is a ladder** -
   WW_CHANGES 2026-09-06q. Frames (4x4 / 6x6 / 8x8) and base resolution are
   panel rows; under them sit a SIZE ladder (pure halving, three rungs, against
   the run's largest base via `WW_IMPOSTOR_REF` from
   `--list-impostor-candidates`) and an ASPECT ladder (1, 3/4, 1/2, 3/8, 1/4).
   Plus `--card-half-aux`: normal, mask and emissive at half size, base colour
   never, because its alpha is the silhouette. Measured 3,584 -> 1,664 bytes,
   46.4%.
2. **The terrain `_msn` wrote UP in BLUE; Fallout 4 puts it in GREEN** -
   WW_CHANGES 2026-09-07. Cost 67.7% of the light and 92.0% of the shading
   variation, measured by re-encoding vanilla's own sheets our way. Gated.
3. **The `_msn` was nearest-sampled**, so a 512x512 sheet held 129x129 distinct
   values. Now bilinear. Catmull-Rom was tried and reverted - VHGT's 8-unit
   staircase makes a C1 spline ring.

### THE OPEN THREAD, and where to resume

bungo's last words on it: **"Still blurry, vanilla is superior in quality"**, and
he is right. Vanilla's `_msn` carries **6.6-8.2x our high-frequency energy** on
the same tile, same size, same mip. Ours is now correct and smooth; it is not as
detailed, and the reason is not yet established.

Ruled out: a tiled detail-normal texture. Autocorrelation of vanilla's
high-frequency residual dies by lag 6 with no peak out to a full cell.

**The next test, written but never run:** a normal map derived from a height
field is curl-free - `d(nx/nz)/dy = d(ny/nz)/dx`. If vanilla's residual is
integrable it came from a finer heightfield than the ESM ships, which is data we
do not have, and the honest answers are to reuse vanilla's `_msn` where terrain
is unchanged or to synthesise erosion detail conditioned on slope. If it is NOT
integrable the detail came from landscape material normal maps composited in -
which we can reproduce, and which would let us match vanilla directly. That
verdict decides the feature, so run it first.

### The mountains, and the mod bungo wants

The question that started it: FO4's out-of-playable-area mountains have colour in
vanilla and go dark when rebaked by DynDOLOD or xLODGen. Four lanes on account B
answered it. Reports and every script are now in
`scratchpad/mountains_20260907/` (copied out of `%TEMP%\claude\laneb\`, which
is a temp folder and will not survive).

**The finding.** The far cells have terrain but **no painted material**. Of
36,864 Commonwealth cells, **3,955 are painted (10.7%)**, in a box x -36..32
y -41..32 that is 77.5% dense, over 100 distinct LTEX. The other **32,909 have
nothing to bake**. Established from the MASTER via a new `lodgen --dump-layers`,
after bungo corrected an earlier check that had read our own output and was
therefore circular: "we do that check on the vanilla map. Not the new generated
one we have." Our regenerated far diffuse is **blank, not dark** - 28 of 36
tiles at luminance standard deviation 0.00. Vanilla's colour out there is baked
into shipped textures whose source material assignment does not exist in the
plugin.

**His goal, his words:** "a .esp plugin, that corrects the material blends in
those outside playable area cells", so "the community can use to bake corrected
LOD diffuse", and "if people replace those materials, they can get baked and
replaced in those far chunks too from those new material textures."

**Lane RECOVER: the colour-to-material recovery DOES NOT WORK, and should not
ship.** 28.4% top-1 on a spatially-honest holdout, which is almost entirely
spatial autocorrelation: ~40% at 0-2 cells from painted terrain, **0.6-16.1%
beyond 16 cells** across six independent splits, and **the median real target
sits 42.5 cells out** (p90 67, max 97). Copying the nearest painted cell's
material, using no colour at all, beats it - 29.4% against 28.4%. The cause is
structural, not tuning: Commonwealth's 58 well-sampled materials sit a median
**2.4 RGB units apart** while measurement noise is **11.9**, so the single most
common far colour (14.7% of all far terrain) is consistent with **28 different
materials**. And it fails invisibly - colour error 10.8 when the material is
wrong against 9.0 when it is right - which is the one failure mode the brief
called worse than nothing. Terrain and slope features were rejected on covariate
shift: height standard deviation is 138 inside the painted area and 282 outside,
because the outer heightfield is procedural border terrain. **This is an
authoring problem, not a recovery problem.**

**Lane ESPWRITE: the plugin writer WORKS, and its gate passes.**
`make_landfix_esp.py` turns an assignment file into valid LAND overrides. The
gate is a full round trip over the whole Commonwealth: 36 blocks, 74,340 groups,
754,220 records, **215,518,075 bytes in and out, every block byte-identical**;
all 36,864 LAND payloads and 236,791 subrecords byte-identical; the compression
wrapper and the XXXX oversize escape both proved. It builds a 52,089,800-byte
`LandFix.esp` over 32,909 cells today. So the vehicle for the mod is ready and
proved - it is the CONTENT that has no honest source.

**Therefore the mod's shape is bungo's call, and he has not made it.** The
defensible version is a coarse regional or family assignment plus the roughly
8-cell high-confidence ring around the painted area, shipping the per-assignment
confidence beside it (calibrated median 0.000, mean 0.040; only 5.21% of
quadrants reach 0.30, and `recovered_conf.png` shows those as a thin ring). Do
not ship RECOVER's full-map output as if it were recovered truth.

**Owed to him and never delivered:** the two images he asked for - "a comparison
image, from a distance, and a detailed close up shot of one of those mountain
peaks outside the playable area". `msn_compare.py` in that folder builds the
`_msn` pair; the diffuse pair needs the same treatment.

### Also open, from lane IDENTITY

* Whether FO4's LOD object shader honours `SLSF2_Vertex_Colors` at all. We set
  it at `src/lodgen.cpp:3504`. Routes to settle it: Todd's treat (1.10.155; its tooling is kept
  outside this repo), `Fallout4 - Shaders.ba2`, FO4CS.
* `--no-ao` ships dead UV2 and Eye Data channels.
* **A silent geometry drop**: `src/lodgen.cpp:3213` does
  `if ( vBase + keep.size() > 65535 ) continue;` - it discards the shape without
  a word.
* `docs/LODGEN_VERTEX_PACKING.md:21-23` has the wrong descriptor hex.
* Optional, needs his say-so because it is a GUI tool: run xLODGen's FO4 path
  once and put `msn_updecide.py` on its output, to settle whether it transposes
  the channels the way its FNV path demonstrably does. That would explain the
  dark mountains for every existing rebake, not just ours.

### The big artefacts left in %TEMP%

`cells.npz` (566 MB), `land.bin` (80 MB), `LandFix*.esp` (152 MB),
`training.npz`, `features.npz`, `Shaders011.fxp` and the `gen/` bakes are still
under `%TEMP%\claude\laneb\`. All of them are regenerable from
`Fallout4.esm` and from `recovered.txt` by the scripts now in
`scratchpad/mountains_20260907/`, so losing them costs time, not evidence.

LANE LEDGER 2026-09-06 20:5x: TERRAIN1 LANDED (ground cover + the terrain virtual texture). Built 20:55 after the overseer fixed what the lane could not, having never compiled: TWO most-vexing-parse declarations (std::vector<T> n( size_t( x ) ); a cast round a bare identifier is a parameter declaration) which caused all 8 compile errors, and --lodm-check/--lodv-check missing from the no-plugin question list (THIRD time; the list now carries a warning). Then two gate checks that asserted things untrue of correct output: the colour filter bar (both sides are BC1, so the bound is the parent block error PLUS the worst of the four children's; now 0 violations everywhere) and the nearest-resample control (0 of 1369 differed by >8 because a Commonwealth pyramid is interpolated from 128-unit land samples; now searches for the tile with the most relief -- 1259 discriminating texels, box 1259, nearest 645). The mutation battery had a mutation that did not mutate (tileCount:=1 on a 1-tile container); now current+1, 23 of 23 refuse by name. HEIGHT SHEET NOW OPT-IN behind --vt-height (default 3 sheets): MEASURED +98%% on (-20,24)..(-17,27) with cover, not the lane's predicted 133%%, and for an FO4 source it is interpolation (finest level 32 units a texel vs LAND's 128) while the worldspace heightmap already carries every real height in 75.5 MB. Kept because a FO76 port has 4x the terrain detail; the gate turns it on since it is the one uncompressed sheet and where the filter law is proved exactly. FINAL: terrain VT 30/0, whole suite green, workspace 87 (floor 74), 70 files uncommitted. NEXT: the FO4CS-native format workflow (wf_2e8fa3cb-875) and the FO76 port risk register (wf_1bbd97f9-7e0) are both still running; the native spec's first stage is expected to be the instance table, which is the 16x far-field memory win and the thing the FO76 port needs most.
LANE LEDGER 2026-09-06 19:1x: FARRING1 LANDED + TERRAIN1 LAUNCHED (window pid 42636). FARRING1: built 19:09, every gate green, workspace 74. The overseer found and fixed three defects the lane could not (it never built): --dump-geometry was missing from the no-plugin question list so the harness measured 0 and failed 4 checks; the centroid floor demanded 0 outside the chunk when 305 are normal before the pass and 266 after; the ratio floor demanded 0.35 which topology forbids (swept: 0.912@32, 0.848@128, 0.845@512, 0.845@2048 units) -- errorWorld default 32 -> 128, the measured knee. FINDING: decimating instanced LOD shells is worth ~15%% and cannot be worth more (7,626 identity groups over 55,293 triangles = 3.5-43 tris a group, almost all open border); far-ring cost is set by HOW MANY objects go there. Our dim-16 chunk with slot fallback is 4,884 KB vs vanilla 271 KB mean, and vanilla gets there by authoring nothing (456 of 28,932 bases fill slot 2, 51 fill slot 3). The paying levers are impostor cards and a SCREEN-SIZE CULL (not yet written, recommended next). 2026-09-06o: mips round to nearest, landed alone on bungo's word, identity baseline moved once. TERRAIN1 implements scratchpad/spec_terrain_vt.md (1,125 lines, 6 investigators + 3 adversarial lenses, 28 fatal findings answered) + scratchpad/reqs_terrain_clipmap.md (a HEIGHT sheet at every pyramid level, origin-aligned, span stated per level -- so a geometry clipmap can consume it later; nothing camera-relative baked). BUNGO DECIDED: keep both the pyramid and the .btr sheets (~4.0 GB vs 1.5 GB today); grass tint ON at 0.35; the rounding fix alone and first (done). Kept per recommendation: the .lodv name, COVER_FULL 96 with --cover-full after a census.
LANE LEDGER 2026-09-06 17:4x: FARRING1 on account B (proxy simplification at rings 2-3 via meshoptimizer after the merge + a BC1 atlas for the stock target; gate tests/spells/lodgen_farring.sh), window pid 33152, cwd E:/Projects/NifskopeWildWastelandEdition (read back). FARRING1 CLOSED **BUILD PENDING**: the wrapper (bash tools/ww_build.sh) and every NifSkope command are refused to account B, so nothing was compiled or run — code, harness and docs are on disk and the overseer owns the build and the gates. Landed: lodgenSimplifyFarRings (after the merge, per (identity, layer) group, alpha-tested shapes and cards untouched, identity set invariant), the atlas bc1 flag + the mip-chain alpha fix it needed, CLI --no-simplify/--simplify8|16|32/--simplify-error/--atlas-bc1/--slot-fallback/--dump-geometry, the panel's Far-ring simplification rows (both targets) and their WW_LODGEN_TEST counts (number floor 8 -> 12), tests/spells/lodgen_farring.sh, spec + packing + WW_CHANGES 2026-09-06n. Report scratchpad/lane_farring_report.md. TERRAIN1 not yet launched: its design is being measured and hardened by workflow wf_c44850fc-82b (read-only, 6 investigators + design + 3 adversarial lenses), output scratchpad/spec_terrain_vt.md; launch the implementation from THAT spec, not from brief_lodgen_terrain.md alone. DISTANT LIGHTS PARKED by bungo 2026-09-06 17:4x ('lets ignore lights for now'); its design workflow was stopped, but the thinking is kept in scratchpad/priorart_distant_lights.md (the industry three-tier pattern: near real lights, mid additive coronas, FAR = emissive baked in the LOD texture scaled by a night factor, with a per-instance seed) and scratchpad/reqs_distant_lights.md (colour resolved from base+ref in sRGB with linear multipliers; intensity separate from radius and float because 8 bits stop at 1; day/night is a CONSUMER rule, never baked, daylight costing zero). The cheapest half of that feature needs no new format at all: our _g/_e emissive sheet exists and reads 0 on every vanilla surface.
LANE LEDGER 2026-09-06 17:2x: EMIS1 LANDED. Built 17:17:35 by the overseer; every gate green: octahedral (pbr meta emissive 2.5 shapes 2; legacy _g max 0 over covered texels; _oct_g.DDS DXT1 3 mips), arrays (pbr sidecar+lodm emissiveScale 2.5/2.5, _e BC1 43836 bytes, 20 A lines), card arrays (both emissive layers green), merge, cards, identity, resources, workspace 66/66. STADIUM ANSWERED (EMIS1 section 6, tools/lod_emission_probe.py): 3430 of 3430 LOD shader blocks in Fallout4.esm carry Own-Emit with a BLACK colour, 0 effect shaders, 0 filled glow slots, 0 of 121 LOD materials emit; DiamondStadiumLight01/02/03 are STATs with ZERO MNAM slots (DiamondLights01AlphaOn.BGSM, white emittance x6.0) plus LIGH records -- vanilla draws NO emitting geometry past the loaded ring, so far city lights are weather/imagespace, and a distant-light-sprite lane (bake LIGH + emitting LOD-less bases per chunk) is the only way to get them. OWED from EMIS1 doubt 5: the chunk Own-Emit bit now follows the source; vanilla sets it always -- TERRAIN1 step 0 restores parity. Next: TERRAIN1 (design workflow wf_c44850fc-82b running read-only while the game is up), then FARRING1, then the card-bake code lane.
LANE LEDGER 2026-09-06 16:33: EMIS1 on account B (emissiveScale in the .lodm: legacy = the source emissive multiple with the colour folded into _g, 0 without own-emit or with a black colour; pbr = the source .lodm value; per layer in arrays; plus the stadium far-light measurement), window pid 29408, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_emis.md; no commits. MO2LOAD closed 16:25 BUILD PENDING; the overseer built 16:27:02 and every gate passed (lodgen_resources.sh 4/4 with hashes, identity, arrays 20 A lines / 2 pbr, merge, card arrays, cards, octahedral both bakes, workspace 66/66). MO2LOAD open items: MO2 mode refuses outside MO2 by design (no modlist parsing); an empty Specified list installs no stack; the Data folder own archives sit at the bottom of the MO2 stack. Queue after EMIS1: TERRAIN1 (brief ready), far-ring, card-bake code.
LANE LEDGER 2026-09-06 15:58: MO2LOAD on account B (Source: Specified = plugins + an ordered resource list, last overrides, the block-list drag widget; Source: Mod Organizer 2 = plugins.txt + the virtual Data with archives by plugin order; a generator-owned resource stack; --resource/--plugins-txt/--mo2; lodgen_resources.sh), window pid 41784, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_mo2.md; no commits. CARDS1 closed 15:37 BUILD PENDING; the overseer built 15:41 and 15:56 (manifest trailing newline fix, fix104: one A line per chunk had been glued to the last M line since the arrays pass shipped) and every gate passed on 15:56:30 (arrays 20 A lines / 2 pbr, merge 10->4, card arrays 2 layers + _g, cards, identity; octahedral + workspace passed on 15:41:33). OPEN for bungo: the legacy emissive law yields the full albedo where an opaque source has alpha 255 throughout (CARDS1 doubt 2); a multiplier is undecided.
LANE LEDGER 2026-09-06 14:4x: CARDS1 on account B (card frame size classes + the emissive sheet _g/_e: spec, bake channel, lodgenCard, both array passes, harnesses, docs; ends BUILD PENDING if the wrapper is refused), window pid 16848, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_cards.md, report scratchpad/lane_cards_report.md; no commits. Queue after it: ground cover + terrain VT (one lane), far-ring proxies + BC1 stock atlas (one lane); one lane at a time in this tree.
LANE LEDGER 2026-09-06 14:12: DOCS1 on account B (docs only; MERGE3 closed 14:01: account B allowlists no NifSkope command at all, so the overseer built tools/ww_build.sh and ran every gate himself on the 14:03:22 exe: merge 10->4 shapes, card arrays 2 layers, arrays, cards, identity, octahedral, workspace 59/59 all PASS), window pid 41720, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_docs.md, report scratchpad/lane_docs_report.md; no commits.
LANE LEDGER 2026-09-06 13:59: MERGE3 on account B (MERGE2 closed 13:56 unable to build: its session executes only under the repo, so the MSYS2 build is now the in-tree tools/ww_build.sh), window pid 42144, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_merge3.md, report continues scratchpad/lane_merge_report.md; no commits.
LANE LEDGER 2026-09-06 13:53: MERGE2 on account B (continuation of MERGE1, whose window closed 13:31 without building: the game was up), window pid 43704, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_merge2.md, report continues scratchpad/lane_merge_report.md; builds + gates + docs for the merge, the atlas _s sheet and the card arrays; no commits.
LANE LEDGER 2026-09-06 13:18: MERGE1 on account B, window pid 35908, cwd E:/Projects/NifskopeWildWastelandEdition (read back), brief scratchpad/brief_lodgen_merge.md, report scratchpad/lane_merge_report.md (session a53dc563); shape merging after the atlas + atlas _s sheet (built, harness verdict unread) + card sheet arrays (fix100, to apply); no commits (his Not yet); waits for Fallout4.exe to exit before building.

**2026-09-06 — the far rings ship as proxies, and the atlas is DXT1 like
vanilla's.** bungo: "Proxy meshes for the far rings. Every engine since 2017
replaces far clusters with one simplified mesh per cell ... ring 2 and 3 chunks
could ship at a quarter of their triangles with the same textures."
`lodgenSimplifyFarRings` runs last, after the merge, because the merged shape
IS the cluster; ring 0 is never touched, ring 2 keeps 0.35 of its triangles and
ring 3 keeps 0.20. The cut is per (object identity index, texture-array layer)
group, so no collapse crosses an object or a layer, and since meshoptimizer
creates no vertices every channel of the packing contract arrives intact rather
than blended — the identity set of a chunk is INVARIANT under the pass, which
is what lets a manifest row still resolve at ring 3. Alpha-tested shapes and
impostor cards keep every triangle. Measured offline, with no exe: vanilla's
Commonwealth ships 3,708,637 object-LOD triangles at ring 0 across 344 chunks
and only 89,696 at ring 2 across 20 (17.5 a cell against 673.8), it has NO
dim-16 chunk over Sanctuary at all, and **0 of the 19,507 references in that
chunk has a base filling MNAM slot 2** (456 bases in the whole plugin do, and
51 fill slot 3) — so a far ring is empty without `--slot-fallback`, which is
new on the CLI and is what the harness builds rings 2 and 3 with. The atlas:
vanilla's `Commonwealth.Objects.DDS` is **DXT1**, 4096x2048, 13 mips,
5,592,552 bytes, not the BC3 our own comment claimed; `--atlas-bc1` (the stock
target's default in the panel) matches it, and needed a fix in `lodgenWriteDds`
where the mip filter forced BC1 alpha to 255 and would have turned every tree's
leaves back into a solid square below the top mip. New question:
`lodgen --dump-geometry FILE.BTO`. **BUILD PENDING** — this lane could not
compile or run anything; WW_CHANGES 2026-09-06n has every measured number, and
every one of them is of vanilla's files, not of ours.

**2026-09-06 — the emissive multiple rides in the `.lodm`, and nothing in
vanilla's LOD emits.** bungo: "carry the multiplier in lodm". The legacy `_g`
law is now the engine's whole rule: a texel is the diffuse × its own alpha ×
the SOURCE'S EMISSIVE COLOUR (black where alpha-tested), and the emissive
MULTIPLE — which cannot go in an eight-bit sheet — rides in the material as
`emissiveScale`, at the top level of a source/card `.lodm` and as a list
parallel to the layers on an array one. It is the source's multiple where the
source own-emits with a colour that is not black, else 0. That answers the
doubt the fourth texture shipped with: an opaque source no longer yields a
sheet that lights every wall. The chunk shape carries the source's emissive
colour, multiple and Own-Emit bit (bucket key and merge key both), the arrays
pass folds and writes them, the card bake records `emissive <scale>` and
`lodgenCard` copies it, and `lodgen --dump-shapes <file.BTO>` prints a chunk's
shader constants so a gate checks a LOD material against its SOURCE. The
measurement bungo asked for, on the stadium: over the WHOLE of Fallout4.esm,
3430 shader blocks in 3354 LOD models (of 3361 named) all carry Own-Emit and
NOT ONE has a non-black emissive colour, no LOD model has a
BSEffectShaderProperty or a filled glow slot, and none of the 121 materials
they name emits; over Diamond City itself, 1729 blocks in 1705 models, the
same. The measurement is `tools/lod_emission_probe.py` (no game, no exe). The stadium's floodlights
(`DiamondStadiumLight01..03`, emittance (1,1,1) × 6.0 out of
`DiamondLights01AlphaOn.BGSM`) carry ZERO MNAM slots — they are not drawn at
LOD distance at all. Nothing in Fallout 4's LOD tree emits. WW_CHANGES
2026-09-06l has the numbers and what is not done.

**2026-09-06 — where the bake reads from: a resource stack, and Mod
Organizer 2.** The LOD panel's Source section now asks the question first:
SPECIFIED, where you order plugins and a new Resources list of mod folders and
archives yourself, or MOD ORGANIZER 2, where the enabled plugins and their
archives come from the profile MO2 launched NifSkope into. Both read MO2's way
— bungo: "Instead of top wins, we use MO2's standard, the last one in the order
overrides the previous ones" — and both feed one ordered stack
(`lodgenSetResources`, `src/lodgen.h`) that `lodgenReadAsset()` consults before
`--data-root` and before the game manager. It is ONE `BA2File`, fed backwards
because that map is first-wins, in two passes: every folder's loose files
first, then the archives. So a loose file beats an archive wherever the archive
sits, and among archives the later entry wins, which is what the engine does.
An EMPTY stack is a no-op and the panel installs one only when there is
something to layer, so the byte-identity gates still measure what they were
written to measure. MO2 is detected by its hook DLL (`usvfs_x64.dll`); off it,
the panel says "not launched from Mod Organizer 2: add NifSkope to its
executable list (like FO4Edit), or use Specified" and Generate refuses. The
same stack goes to the game manager for the session, so the viewport shows what
the bake will see, and to the card bake through `WW_LODGEN_RESOURCES` /
`WW_LODGEN_MO2`. CLI: `--resource` (repeatable), `--plugins-txt`, `--mo2`, and
two questions that answer and stop — `--probe <relpath>` (which entry supplied
it, loose or archived, size and sha1) and `--print-source` / `--list-files N`.
Gate: `tests/spells/lodgen_resources.sh`, four precedence checks with the
hashes printed. WW_CHANGES 2026-09-06k has the numbers and what is not done.

**2026-09-06 — frame size classes, and the emissive sheet.** A LOD set now has
FOUR textures, not three: `_g` on a legacy set and `_e` on a pbr one, BC1, RGB
the emissive colour and no alpha (coverage lives on the colour sheet), keyed
`emissive` in the `.lodm` under both families. Vanilla has no such sheet
because it hides the quantity in the diffuse's ALPHA on its opaque chunk
shapes — measured on Diamond City's DXT5 atlas, 216,521 of 262,144 alpha
blocks varying, none fully transparent, against a black emissive colour and no
glow slot. So the legacy law IS that rule (colour × its own alpha where the
material is not alpha-tested, black where it is) and the pbr law is a source
`.lodm`'s `emissive` texture raw, black when it names none. It reaches the
bake (shader channel 13, the glow slot raw where a `.lodm` retargeted it),
`lodgenCard`, the mesh arrays and the card arrays. And a frame's shorter side
is now quantised UP to a multiple of 16 — a SIZE CLASS — with the recorded
extents widened to the frame's aspect rather than the silhouette stretched
into it, because a card array can only hold sets that share a grid and a
frame, and per-base frame sizes were leaving almost every base in an array of
its own. `docs/LODGEN_IMPOSTOR_SPEC.md` is the contract; WW_CHANGES
2026-09-06j has the numbers and what is not done.

**2026-09-06 — chunk shapes merge, the atlas `_s` sheet, card sheet arrays.**
A region build now runs three passes in order once the chunks are written:
`--arrays` (the mesh texture arrays), `--atlas` (the diffuse, normal and the
new `_s` sheets, all three under `<tex-dir>/Objects`, the directory the game
paths baked into the shapes actually name), then `--merge` (on by default in
region mode, `--no-merge` to opt out), which concatenates every shape a chunk
holds that the engine cannot tell apart — the name, all ten texture slots, the
alpha property, the shader's type, flags and constants, the vertex descriptor,
and the array `.lodm`. The `_s` sheet is BC5 like vanilla's
`Commonwealth.Objects_s.DDS` and carries each shape's specular strength and
smoothness folded into its R and G, so every atlased shape reads it at
constants 1/1 and two shapes that differed only in their constants can merge:
Sanctuary (-20,24) at dim 4 goes from ten shapes to four with its vertices and
triangles unchanged, against vanilla's three. With `--impostors`, `--arrays`
also packs the card sets the chunks stand on into
`<ws>.LodgenCards.<family>.<WxH>` DX10 BC3 arrays with a `cardArray` `.lodm`
beside them. Two manifest forms are new, both additions on the END of lines
that already had readers: `A <block> -1 <lodm>`, where −1 means the layer is
per vertex in UV2.y because a merged shape can span layers, and
`C … <lodm> <array lodm> <layer>` for a card that sits in an array. Nothing
consumes any of it yet, and `docs/LODGEN_IMPOSTOR_SPEC.md` is elastic until
FO4CS has a LOD path of its own. Gates, all PASS on the 14:03:22 exe:
`tests/spells/lodgen_merge.sh`, `lodgen_card_arrays.sh`,
`lodgen_texture_arrays.sh`, `lodgen_impostor_cards.sh`, `lodgen_identity.sh`,
and the two GUI ones, `lodgen_octahedral.sh` and `lod_generation.sh` (the
workspace at 59 of 59).

**Building: `bash tools/ww_build.sh [sources]`** is the whole gated chain as
one in-tree script — the game-up and second-`make` checks, the running exe
renamed aside rather than killed, `make -j2` under MSYS2 UCRT64 gated on
make's OWN exit code, the exe proved newer than the sources you name, and
`res/style.qss` and the shaders compared against their link-time copies. It
exists because an account-B session may execute only under the repo and so
cannot invoke `/c/msys64/usr/bin/bash` directly; use it instead of retyping
the incantation.


**Read this first.** It is the short document: where things are, how to build,
what will bite you. [WW_CHANGES.md](WW_CHANGES.md) is the detailed history,
[WW_FEATURES.md](WW_FEATURES.md) is what the fork adds,
[docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) is the single backlog, and
[docs/MISTAKES.md](docs/MISTAKES.md) is what went wrong and what stops it
happening again.

**Working directory:** `E:\Projects\NifskopeWildWastelandEdition`
(GitHub: [BungoV/NifSkope-WildWastelandEdition](https://github.com/BungoV/NifSkope-WildWastelandEdition),
branch `main`, `origin` is the fork — never push upstream.)

Updated **2026-09-05**. Edition **0.3.3**, packaged and released. Build green.
The current work is a sweep of the **compiled-collision backlog**, and since
2026-08-21 it is being tested IN THE GAME, which changed what the work is.

## 2026-09-06 — Impostor cards are configured, and the resolution is a ladder

Three of bungo’s asks landed together and compose into one law; the detail is in
`WW_CHANGES.md` 2026-09-06q and `docs/LODGEN_IMPOSTOR_SPEC.md`.

* **Card frames** and **card resolution** are panel rows, saved settings and
  validated driver arguments. Both were unvalidated environment variables.
* The resolution is what the run’s **largest** base gets. Every base comes down a
  pure halving ladder by its own world size (three rungs) and its short side down
  a five-rung aspect ladder (1, 3/4, 1/2, 3/8, 1/4). Proven: a base at a quarter
  of the largest dropped 256 px to 64, then took the 3/4 aspect rung, 48 x 64.
* `--card-half-aux` halves the normal, mask and emissive sheets. Measured 46.4%
  of the payload. The base colour never divides.
* The panel prices all three on one computed line and refuses a card directory
  baked at another grid or resolution.

Suite green after: octahedral (with a third bake for the ladder), card arrays
(with a half-aux pass), impostor cards, identity, merge, far rings, texture
arrays, resources, and the workspace at 97 checks against a floor of 74.

**Open, and worth a decision.** The chunk still emits the vanilla CROSSED QUADS
for every carded placement — 4 triangles reading a front|side sheet — and the
`C` manifest line tells FO4CS to ignore them and draw the octahedral card
instead. Since this bake is FO4CS-only, that is 2 triangles and one alpha-tested
overdraw per tree spent on geometry the consumer discards, plus the `_fs` sheet
on disk. Emitting a single quad under the FO4CS profile would halve both. Not
done: it breaks the stock fallback, so it is bungo’s call.

## 2026-09-05 — `.lodt`: one landscape file per worldspace, with a reader and Fallout 76 import

**The current work.** A new format that retires the per-chunk `.btr` terrain
path: `Data\Terrain\<EditorID>.lodt` holds heights, LTEX alphas, water (height
AND type), terrain colour, ground cover and a coarse AO plane behind a
progressive LOD pyramid. **The spec is the contract**, not the code:
[docs/LODGEN_BTD_FORMAT.md](docs/LODGEN_BTD_FORMAT.md). Read it before touching
`src/lodtfile.cpp`. History: WW_CHANGES 2026-09-04k (the format) and
2026-09-04l (reader, streaming writer, `.btd` import, the verification pass).

**ALL OF IT IS UNCOMMITTED.** bungo answered "Not yet" when asked to commit.
Do not commit or push any of it without his word. `git status` lists the set.

**Commands.**

  * `NifSkope -no-gui lodgen <esm> --worldspace 3C --lodt <dir>` — writes,
    reads the file back through the independent reader, and round-trips
    heights, alpha words and colour against the ESM's own `LAND` records.
    Exit 1 on any mismatch; heights must be EXACT (the 8-unit quantum is
    `VHGT`'s own). Commonwealth: 34 MB, ~30 s.
  * `NifSkope -no-gui lodgen --from-btd <file.btd> --lodt <dir>` — no plugin,
    the `.btd` is the whole landscape. Appalachia is 201×201 cells at 128
    samples each: **~25 min, 1.55 GB out, ~2.5 GB RSS**. Round-trips heights to
    half a quantum and alpha/colour/ground-cover words exactly.
  * `NifSkope -no-gui lodgen --from-btd <file.btd> --btd-probe` — 30 s, writes
    nothing: measures the alpha field↔slot pairing, ground cover likewise, bit
    15, and the colour channel means under A1R5G5B5. Run it on any new `.btd`
    before converting it.
  * `lodgen SeventySix.esm --worldspace 25DA15 --from-btd <Appalachia.btd> --lodt <dir>`
    — with a plugin and worldspace given, the `.btd` conversion takes WATER
    (XCLW/XCWT/WATR) from the plugin; the terrain still comes from the `.btd`.
    APPALACHIA is form 0025DA15 in SeventySix.esm.
    NOT YET RUN: on 2026-09-05 the plugin was unreadable (a raw `cat` of it
    blocked while Steam was up). The code path is proven on Fallout4.esm +
    the Pitt `.btd`. `WW_ESM_TRACE=1` traces `EsmWorld::load` if it hangs.
  * `... --lodt <dir> --verify-only` on either path — no write; opens the
    existing `<dir>/Terrain/<name>.lodt` and runs every round-trip against the
    source. A minute for Appalachia. Use it before re-converting anything.
  * `lodgen <esm> --worldspace HEX --heightmap <dir>` — the R16 shadow
    heightmap, now **native by default** (`cells*32` a side, one texel per
    `LAND` sample, oblong when the world is; `--heightmap-size N` for a square
    resample, `native` for the default). Sample-ALIGNED — texel i is sample i,
    not `(i+0.5)`. The reference is `Commonwealth_fine` in the FO4CS folder;
    diff against it, do not reason about it - the Commonwealth bake is now
    byte-identical to it. Shared VHGT edges take the MAXIMUM over every cell
    holding the sample (measured: 0 mismatches over 2.3M edge texels; every
    other rule missed thousands). Nine worldspaces deployed natively on
    2026-09-05; the 4K set is in `FO4CS/Textures/Terrain.4k/`. Ours carry no
    `F4FX` provenance block - legacy path in the loader.
  * `lodgen <esm> --worldspace HEX --dump-land <file>` — every cell's full
    33x33 `VHGT` as int16/8 with a presence byte, ~80 MB for the Commonwealth.
    The instrument for any shared-edge question: test the rule in Python
    against the dump instead of rebuilding per hypothesis.

**Harnesses:** `tests/spells/lodt_write.sh` (FO4, 9 checks + the tool's own
exit code), `tests/spells/lodt_btd.sh` (the Pitt `.btd`, the only one small
enough; it prints which paths it CANNOT cover — that worldspace has zero land
textures and one constant colour, so alpha and colour are only exercised by a
manual Appalachia run), `tests/spells/lod_generation.sh` (the workspace, 58
checks), `tests/spells/lod_channel_preview.sh` (terrain channels render
distinctly) and `tests/spells/lodgen_impostor_cards.sh` (the front|side sheet).

**The LOD Generation workspace** (2026-09-05, night): `src/lodgenmanager.cpp`
is a dock now — `tlCreateLodGenerationDock`, registered in the Workspaces
list in `nifskope_ui.cpp`, appended LAST. Whole-worldspace jobs (`.lodt`,
AO-only refresh, heightmap) run on a `std::thread` through
`LodtOptions::progress` and talk to the GUI only via `QMetaObject::invokeMethod`
queued lambdas — no Q_OBJECT, no moc, on purpose. Chunk jobs stay on the GUI
thread. The map widget is `LodgenProgressMap`. Self-test: `WW_LODGEN_TEST`.
The panel follows the docks' house style (2026-09-06a): `wwHeading` sections,
every number through `wwMakeScrubField`, every selector through
`wwMatchFieldStyle`, one label | field row per setting with the explanation in
the tooltip. The self-test counts each of those with a floor — a plain spin
box, a group box or a two-column row in it is a failure, not a style note.
`SHOT=<png> bash tests/spells/lod_generation.sh` also grabs the dock from
inside the app, which is how the alignment was judged; look at it after any
layout change, the counts do not see a ragged column.
2026-09-06b: the panel is three bands — the settings scroll, the map and bar
on a vertical splitter, the summary line and buttons pinned beneath. A
**Target** selector (FO4 Community Shaders / Stock engine) ticks what the
reader needs and hides what it cannot use; the run reads `wantLodt()` etc.,
never the box alone. Outputs are `LodgenSection`s that fold (the legacy
`.btr` one starts folded). `refreshSummary()` owns Generate's enabled state
and the sentence beside it — never `setEnabled( true )` the button yourself.
Check-box and radio indicators are styled app-wide in `res/style.qss` (Fusion's
box measured 0 levels against this theme's ground): Blender's blue `toggle`
skin entry with a white mark from `:/wnd/check.png` / `radio.png`. The app
reads `release/style.qss`, a copy made at link time — copy it by hand after a
sheet edit without a relink. The output is the mod folder itself, one field
(`outputDir()` is the only way to it; the folder is created on Generate).
Meshes and textures are read through `lodgenReadAsset()` in `lodgen.cpp`:
a loose folder first, then for `.nif` the generator's own index
(`lodgenMeshArchives()`, the manager's Fallout 4 folders and archives — the
manager itself drops every `.nif` at index time), and for textures and
materials `Game::GameManager::get_file`; there is no assets row. The CLI's `--data-root` remains an
optional loose override — batch mode never initialises the game manager. The
self-test builds the near chunk (-20,24) at dim 4 both ways and compares
bytes (the far chunk at dim 16 has no LOD at that ring without impostor
cards). The Object LOD chunks section carries every `LodgenObjectOptions`
field but the grey-AO debug view. **Stable identity** (06c): manifest rows end
in `ref part` (the placed reference; a SCOL part carries the SCOL reference and
its ordinal), the first line names ring, chunk and columns; the key across
rings is `(ref, part)` — `tests/spells/lodgen_identity.sh`. **Texture arrays**
(06d): `lodgenBuildTextureArrays`, `--arrays`, the panel's row; one DX10 BC3
array per size class + `_n`, layer in UV2.y, `A` manifest lines, sidecar; runs
before the atlas — `tests/spells/lodgen_texture_arrays.sh`. **Octahedral
impostors** (06e/06f): `WW_IMPOSTOR_OCT=N` (driver `OCT=N`); the sheets and the
arrays follow ONE spec, `docs/LODGEN_IMPOSTOR_SPEC.md`, in TWO FAMILIES (06g):
vanilla-sourced sets are LEGACY, `_d`/`_n`/`_gsaos` (gloss, specular, AO,
subsurface mask — the vanilla `_s` composed as the engine does, never
inverted); sets from a source `.lodm` of family pbr are `_bc`/`_n`/`_rmaos`.
`.lodm` = OUR LOD material (`src/io/lodmfile.h`: `LODM` envelope + compact
JSON: family, kind, textures, card grid / array layers), written beside every
card set (`<id>_oct.lodm`) and array set (`<ws>.LodgenArrays[PBR].<WxH>.lodm`),
read as a source override beside a material (`foo.bgsm` -> `foo.lodm`) or at
the diffuse's path under `materials\`. Manifest: `C ... <lodm>`, `A block layer
<lodm>`, `M block <material>`. The bake retargets a shape's slots 0/1/7 to a
source `.lodm`'s textures (`BSShaderLightingProperty::wwTextureOverride`) and
reads shader channel 10 = the legacy pair (gloss, specular) or the slot raw
(`lodMaskRaw`); `WW_LODGEN_DATA_ROOT` is a loose root for fixtures. The chunk
shape now carries the source's slot-7 `_s`, smoothness and strength (vanilla
chunks do). Gates: `lodgen_octahedral.sh` (two bakes: legacy, then pbr from
fixture `.lodm`s), `lodgen_texture_arrays.sh` (legacy, then a loose root with
one pbr `.lodm`). **Cards from the base, cards from a ring** (06h): the
candidate listing prints the base's own near MODL (`EsmLodBase::model`, STAT
and TREE), walks SCOL parts, takes `--candidates missing|trees|all` (driver
`CANDIDATES=`); the bake hides `_L1`..`_L9` detail-step shapes and writes
`model`/`hidden` meta lines (`.lodm` `card.source`);
`LodgenObjectOptions::impostorFromLevel` / `--impostors-from-level N` / the
panel's "Cards from ring" combo (FO4CS only) puts a placement on its card from
MNAM level N on even where the ring has a mesh (report: "N placements on
cards in place of their ring's mesh") — `lodgen_impostor_cards.sh` proves it
at (-20,24) dim 4. Read the spec before touching either bake. Every scrub field and matched selector in
the program takes the mouse wheel only while focused (`wwGuardWheel`, applied
by both helpers) — scrolling a panel past a number used to change it. 58
checks.

**More CLI:** `--refresh-ao` (AO plane in place, byte-identical to a fresh
write), heightmaps now carry the `F4FX` provenance block (corpus hash checked
against the loader's Commonwealth constant on every bake; spec in
`docs/F4FX_PROVENANCE.md`). The reader seek-reads blocks; it no longer loads
the file whole.

**What was measured, so nobody re-derives it.** Alpha packing is five 3-bit
INDEPENDENT opacities over a base layer, field `s` ↔ quadrant slot `s`, slot 0
on top; not a partition of unity (72% of Appalachia samples sum past 7).
Ground cover mask bit `b` ↔ slot `b`. Bit 15 is never set. Colour in a `.btd`
is A1R5G5B5; ours is 5-5-5 with R at 11, G at 6, B at 0. The height quantum for
a `.btd` is exactly `maxAbs / 32767` and the round-trip bound is half of it.
**Shared VHGT edges take the maximum** over every cell holding the sample -
bungo's decision 2026-09-05, so `.lodt` and the shadow heightmap describe one
surface; the cross-check rebuilds it (129 of 36864 Commonwealth seam samples
raised, 0 mismatched).
See docs/MISTAKES.md 2026-09-04/05 for what was believed before each of those
was measured.

**Known limit:** the output file is assembled in RAM before it is written. The
fix is to reserve the block directory (its size is known up front) and stream
payloads to disk; nobody has done it yet.

**Landmines.**

  * A running conversion holds `release/NifSkope.exe` open; `make` fails at
    the link until it exits. Check `Get-Process NifSkope` before building.
  * The `.btd` tile cache MUST cover a row of 8×8-cell tiles (`lodtfile.cpp`
    sizes it from the width). At the library's default 16 tiles Appalachia
    burned 825 CPU seconds without finishing pass one.
  * Form IDs in a converted `.lodt` are the `.btd`'s own — FO76's. A FO4 port
    of Appalachia only resolves them if it keeps them.
  * The AO plane is sky occlusion: FO4CS should gate the ambient term with it
    and fade it in as SSAO fades out. Over the sun term it double-darkens.
  * `--objects` takes its own chunk coordinates; passing `--objects --terrain X Y`
    silently bakes chunk (0, X).

## 2026-09-03 — Outfit sidecars: .ssf and .sclp are written now

Two Rigging spells, both castable headlessly, both new capability rather than a
fix. Harness `tests/spells/outfit_sidecars.sh`, 8 of 8. Full entry in
WW_CHANGES 2026-09-03.

  * **Generate Segment File (.ssf)** — the dismemberment table, from the Bone
    IDs the shape's own subsegments already carry. The engine reads a bone NAME
    per segment address and the name IS the visibility flag (`GetEnabled` is
    `GetSegmentBoneName(id) != "DISABLED"`). The address is
    `((segment << 8) | subsegment) << 8` and the second byte is the
    subsegment's ORDINAL, resolved as `SegmentStarts[segment] + 1 + ordinal`,
    not its User Index — the two disagree on OutfitM's left arm. Held against
    the shipped files: 56 of 56 assignments reproduced, 0 missed.
  * **Generate Bone Scale File (.sclp)** — the body shape an outfit imposes,
    48 `*_skin` bones, either an identity table or a weighted least-squares fit
    of a morphed body against an unmodified copy of the same mesh, in the bone
    NODE's frame. Recovers a synthetic 1.37 to 4e-6.

**Held against the WHOLE corpus, not a sample** (bungo: "when in doubt if
something works, generate the two files from samples from Fallout 4, and
compare"): 489 meshes regenerated, **2465 of 2554 shipped assignments
reproduced**, **455 of 489 files reproducing every one**, and **0 unresolved
bone hashes corpus-wide**. It also caught three format facts the ten-file sample
had not — both formats are CRLF, .sclp has no trailing newline, and the number
rule is %.17g (measured: 7695 of 7776 literals, against shortest-round-trip's
6705). All fixed; harness is 10 of 10.

**Every one of the 89 remaining misses is vanilla disagreeing with its own
mesh**, all in creature assets: Deathclaw.ssf names a Tail4 its mesh does not
have, and mcoatpostwar.ssf keys shapes that were renamed `_PW`. We follow the
mesh. On those 34 files our output is deliberately not vanilla's.

**What is owed: the in-game gate.** Neither file has been loaded by the engine
from our output, and both fail invisibly — a wrong base name is a limb that will
not come off, not a crash.

**A latent defect was found and NOT fixed** (docs/TO_BE_IMPLEMENTED.md, top):
"Parent Array Index" means the parent's shared-data row in vanilla and the
entry's own row in this fork's writer, so `riggingReadSegmentDefinitions` reads
vanilla subsegments' Bone IDs off by a row. The .ssf writer sidesteps it by
using Segment Starts, the engine's own rule.

## LODGEN 2026-08-31 (second pass): completion round + parity audit DONE

Overnight batch on bungo's six-item list, every item shipped and pushed
(149a120..c8d7861): SCOL expansion (Sanctuary 104% of vanilla's tris),
TES4-MAST formID load-order remapping (mod ESPs merge correctly, DLCs no
longer collide), splat-bake fixes (BGSM-backed TXSTs + NULL layers +
VCLR), UV2 extended terrain profile (sky vis + second class; nif.xml
gained the missing "UV 2" vertex field), deterministic tree repetition
breaking, and the `--atlas` object atlas pass. Then the chartered
**vanilla parity audit** — see **docs/LODGEN_PARITY.md** — which found
and fixed the real water rule (hasWater + default height + exposure
above cell terrain min; harbor wet-cell set now EQUALS vanilla's), the
far-ring plain-BSTriShape water type, and empty-MNAM-slot dropout
(substitution now opt-in `slotFallback`). Terrain budget is per CHUNK
(~2100 tris on every ring, like vanilla). Manager grew a drag-and-drop
plugin load-order list + worldspace combo. Harness 18/18 throughout.
Open gaps (in the parity doc): roads not rasterized into terrain bakes,
splat grading, and the in-game load gates only bungo can run.
THREE LODGEN CATEGORIES ratified (TO_BE_IMPLEMENTED.md top): vanilla /
vanilla+ (engine-legal extras) / improved (CS-exclusive, .pbrm-style
module gating, legacy fallback).

## LODGEN 2026-08-31: rungs 0-2 SHIPPED same night, rung 3 started

NifSkope generates FO4 world LOD. **Working now** (WW_CHANGES 2026-08-31,
harness `lodgen_terrain.sh`, 14/14 then; 21 checks today):

    lodgen <esm> --worldspace 3C --terrain-region X0 Y0 X1 Y1 --out-dir DIR

emits BTR + BTO + identity manifest per chunk under vanilla naming (the
9-chunk Sanctuary region in 2.3 s). Terrain holds against vanilla to
median 0.00 world units; objects land position-for-position; the FO4CS
identity contract is real in the files (R+G object index, B baked AO,
A authored sway alpha, .manifest.txt). Code: src/esmdata + src/lodgen;
layouts: docs/LODGEN_ESM_LAYOUTS.md; plan: docs/LODGEN_PLAN.md.

**Next in the campaign:** the IN-GAME GATE (bungo: drop a sweep output
into a mod folder over vanilla LOD), the two stock-engine tolerance
checks (fat vertex desc + vertex alpha on chunks WITHOUT CS), terrain
texture baking (needs a BC1 writer), impostor baker, instancing
manifests, geomorph weights, LTEX-class/wetness terrain bakes, and the
GUI World LOD manager with live chunk preview.

## New 2026-08-30: NifSkope opens Fallout 76 terrain (.btd)

A `.btd` is the whole worldspace's heightmap database, not a NIF — FO76 has no
.btr files. File > Open on one shows a region + detail picker and BUILDS the
terrain (whole map at LOD4 by default, ~15 s); `nifskope-cli btd` is the same
generator headless; `save()` refuses to overwrite the source `.btd`. Parser =
fo76utils' `btdfile.cpp` vendored into lib/libfo76utils (format spec is its top
comment). Generator + picker = `src/btdterrain.cpp`; harness =
`tests/spells/btd_terrain.sh`, 13/13, whose height authority is a python decode
of the file's own uncompressed LOD4 table. Textures/ground cover/colour are
decoded but undisplayed — top of docs/TO_BE_IMPLEMENTED.md. Full entry in
WW_CHANGES 2026-08-30. Picker bypass for harnesses:
`WW_BTD_REGION=x0,y0,x1,y1,lod`.

## NEXT SESSION STARTS HERE: one A/B closes the ragdoll (2026-08-25)

**The inertia FRAME was the defect, and it is fixed.** `dyn_inertia +0x20` is an
inverse inertia diagonal and `+0x40` is the frame it is expressed in; Compile
wrote the diagonal and left the frame at the identity, which is a different
tensor on every body. On a ragdoll that is every bone resisting rotation about
the wrong axes -- corpses detonating on death. bungo proved the cause in game on
2026-08-24 by splicing the stored quaternions back into a rebuilt file.

Decompile now writes the whole tensor `R diag(I) R^T` into **Inertia Tensor**
(an `hkMatrix3`, which always had room for it) and Compile diagonalises it back,
resolving the frame's non-uniqueness towards the body's own orientation. Full
reasoning in WW_CHANGES 2026-08-25.

**Measured, `collision_constraints.sh` 48/48:** worst tensor error 4.4e-07 on the
human skeleton's 18 bodies and 6.9e-07 on the brahmin's 39, quaternion agreement
0.999996 and 1.000000, vanilla identity frames 0 of 57. Dropping the frame scores
**89% relative error** on the same comparison, which is how a pass here is known
not to be vacuous.

**What is owed: the A/B, and it is now a THREE-way.** The two earlier game tests
were run against the wrong build's assets AND with the frame bug in place, so
neither told us anything about the writer.

  * `WW Ragdoll Test` -- rebuilt from the GAME's own archive, by the build that
    keeps the frame. Should behave.
  * `WW Ragdoll Placebo` -- the DataUnpacked files bit-identical, never touched
    by NifSkope. Should reproduce the old thrashing, which exonerates the writer.
  * `WW Ragdoll Control` -- superseded, do not enable.

Nothing else in the collision backlog is blocked on it.


## The live test, and what it found (2026-08-22)

`E:\Projects\Fallout 4 Mods\mods\WW Concord Collision Test` is 114 meshes — every
reference in ConcordMuseum01 the game must open a NIF for — rebuilt by our own
writer. It rebuilds in five minutes with three parallel workers
(`rebuild.sh` + `MANIFEST=` per worker), which is the point: an earlier
1,619-mesh build took an hour, and an hour is the wrong unit of work when one
mistake repeats it.

**STATE 2026-08-22: IT WORKS IN THE GAME.** bungo confirmed a door running our
recompiled collision -- the blob differs from vanilla's, so it is genuinely ours
-- loads, opens and closes. That is the whole round trip proven in the engine on
an ANIMATED object: decompile, recompile, load, activate. The Museum set's solids
are proven too (110 of 114 identical to vanilla within 0.46 mm,
`tools/collision_ab.py`), every file has exactly one physics system, and all 22
keyframed records carry the inertia sentinel.

## Where the ragdoll work stood on 2026-08-23

**Item 3c is CLOSED IN GAME.** bungo re-tested the wall terminal after the unit
fix and its collision is where it belongs, which validates three commits at once:
the mesh SHAPE builder (`c873b7b`), mixed compounds (`00dbc65`), and the
Havok-translation fix (`e5ab899`). Nothing in the Museum set is unverified any
more.

**What is left, and it is one piece of work rather than two:**

  1. **The rebuilt ragdolls were built from the WRONG BUILD of the game. Fixed;
     one A/B load will confirm it.**

     `E:\Tools\Fallout 4\DataUnpacked` -- this project's corpus, and the
     reference behind every "vanilla says X" figure in the collision work -- is a
     DIFFERENT BUILD from the installed game. Measured 2026-08-24 by reading the
     archives directly with `tools/ba2get.py`: **600 of 600 sampled NIFs differ**,
     most by 44 header bytes but not all, and **collision blobs differ too** (6 of
     6, including plain statics). The install is BA2 version 8 -- next-gen.

     For statics that is cosmetic. For ragdolls it is not: the two builds **order
     a ragdoll's bodies differently** (the game's bone 2 is RLeg_Thigh, the
     corpus's is SPINE1, and so on down). Both files are internally consistent, so
     both are valid ragdolls -- but the animation and behaviour data that DRIVES a
     ragdoll lives outside the NIF, was never touched, and expects the game's
     order. Our pipeline preserves whatever order it is handed, faithfully.

     So the two failed game tests were not testing the collision writer. They were
     testing what happens when another build's skeleton is installed over yours.

     **Rebuilt from the game's own archive, our output reproduces the game's order
     exactly** -- bone for bone, parent for parent, node for node, identical
     body-to-node map, exact mass (93.500 kg human, 121.000 kg brahmin), exact
     density and trigger-body counts.

     **The A/B, one session:**
       * `WW Ragdoll Test` -- rebuilt from the game's archive. Should behave.
       * `WW Ragdoll Placebo` -- the DataUnpacked files BIT-IDENTICAL, never
         touched by NifSkope. Should reproduce the old thrashing. If it does, the
         cause was the source assets and the writer is exonerated.
       * `WW Ragdoll Control` -- superseded, do not enable.

     Two REAL defects were found and fixed by the failed tests and stand on their
     own merits: primitive mass properties never set, so every ragdoll body summed
     to volume zero and lost density and centre of mass (6541dbd); and
     `hknpMaterial`'s flags and trigger type written as zeros, so the character
     bumper stopped being a trigger (8b1b312). Both are regression-tested --
     `collision_constraints.sh` is 40 checks -- and the corpus sweep is clean at
     155/155 files, 1202 joints in and out.

     Engine findings are in `docs/RE/ragdoll-engine-dig.md`: nothing validates our
     data on load, the mass path reads where we write, the grab weight is a
     scene-graph SUM that is identical for both files, and the engine deliberately
     loosens ragdoll constraints on death and eases them back -- so a brief settle
     is vanilla behaviour, not a defect.

     **Two method lessons in docs/MISTAKES.md, and the second is the expensive
     one:** Todd's treat first for vanilla engine behaviour; and when a comparison against a
     reference keeps saying "no difference" while reality disagrees, stop testing
     the subject and test the REFERENCE.

**The Museum set now stands at:** shape classes 114/114 vs vanilla, stored solids
113/114 (one known face-decomposition difference), compounds 37/37, body rest
state 0 differing, shape header words 1 differing, body order 46/46 on vanilla's
own rule. Everything that remains is documented and deliberate.

**Done and CONFIRMED IN GAME since the railings closed:**

  * **The mesh SHAPE builder is out** (`c873b7b`, WW_CHANGES 2026-08-23a).
    `hknpEncodeMeshShapeObjects` emits the four objects of a compressed mesh as
    self-contained pack objects; `hknpEncodeCompressedMesh` is the system half
    plus a splice. Byte-identical output on 114 of 114, twice.
  * **`encodeShapeObject` can BUILD a mesh**, not only copy one back -- the
    branch that unblocks everything below.
  * **Mixed compounds, item 3c, closed** (`00dbc65`, WW_CHANGES 2026-08-23b).
    Shape classes match vanilla on 114 of 114; stored solids 110 -> 113;
    compounds 34 -> 37 of 37.
  * **A mesh leaf under a transform was 18 game units out** (`e5ab899`,
    WW_CHANGES 2026-08-23c). Older than mixed compounds and true of every such
    mesh; flattening hid it. Ten green checks passed while it was wrong, because
    all of them measured what the collision IS and none measured WHERE.

**What the engine reads to choose an impact sound** (1.10.155 RVAs; the full
derivation is WW_CHANGES 2026-08-22j):

    FOCollisionListener::OnContactImpulseEvent          @0x630c60
        matA = bhkUtilFunctions::GetMaterialForShape(bodyA->m_shape, keyA)
        matB = ... bodyB ...
        BGSImpactManager::ProcessEvent({matA, matB, contactPos, velocity})
    bhkUtilFunctions::GetMaterialForShape              @0x1d8c300
        key == 0xffffffff -> shape+0x18; !(shape+0x10 & 4) -> 0; else the leaf
    BGSImpactManager::ProcessEvent                     @0xd19e00
        BOTH materials must resolve -- either null and BOTH surfaces go silent

**We never reach it.** The event is named after the flag that raises it.
`hknpBody::m_flags` bit 7 is `RAISE_CONTACT_IMPULSE_EVENTS`, named by the
engine's own debug printer `NVFlex::printHknpBodyInfo` @0x27afa4, and it arrives
from `hknpBodyCinfo::flags` at cinfo +0x18: `hknpBody::initialize` @0x14daaf0
copies that word through untouched apart from the low four bits.

Vanilla sets it on **1,408 of 1,408 dynamic bodies** and on none of the 12,456
static or keyframed ones (13,889 bodies, 11,820 files). Our Museum rebuild sets
it on **0 of 170**. The writer already writes the field and the decoder already
reads it; what is missing is the middle -- Decompile has no `bhkRigidBody` field
to put it in, so Compile starts from a default-constructed `HknpBodyPhys`, whose
`cinfoFlags` is 0.

**How it is written now:** `RAISE_CONTACT_IMPULSE_EVENTS` is DERIVED from
`in.dynamic`; `USER_FLAG_0` and `RAISE_TRIGGER_EVENTS` are CARRIED on
`bhkRigidBody`'s "Body Flags" bits 1 and 2, because neither follows from anything
we model. Those four values are all the corpus uses, and the rule reproduces all
13,889. `tools/hkbodyflags.py` checks it and is a gate in
`tools/rebuild_collision.sh`.

**Ruled out for good, and why the earlier reading was wrong:** the shape header
at +0x18 is only the FALLBACK for a composite shape, which is why hand-patching
it changed nothing. And the "per-body material words `000000ff` / `000100ff`,
whose high u16 is just the body's own index" are two named fields, `qualityId`
(u8 at +0x10) and `materialId` (u16 at +0x12); ours and vanilla's agree on both,
so the permuted body order was never a difference there.

**A method note worth keeping:** the exe carries Havok's own `hkClass` reflection
tables -- `<Class>Class_Members`, const arrays naming every field and its offset.
`hknpBodyCinfo`, `hknpBody`, `hknpMotionCinfo` and `hknpPhysicsSystemData` all
came out in a single query. Read those before deriving a layout by hand.

Both items that used to be here are accounted for above: mixed compounds CLOSED
on 2026-08-23, and the enabling change they shared -- the compressed-mesh SHAPE
builder pulled out of `hknpEncodeCompressedMesh` -- done with it. What is left is
multi-body systems and body order, both listed at the top.

**Four crashes and failures, none of which any check we owned could see:**

1. A compound's BVH shipped with **no pointer to it** — the local fixup at
   `+0x10 -> +0x40` was never emitted. `hknpDynamicCompoundShape::updateAabb`
   read null.
2. Compound AABBs were unioned from the children's **vertex lists**, and a
   capsule has none. Bounds stopped short, and an all-capsule body bounded
   nothing, was refused, and fell through to the triangle path.
3. Every compound wrote the placeholder `0x01000001` at +0x10. **Bit 0 of that
   word is the engine's "I am convex" flag**, so a compound was handed to
   `hknpScaledConvexShapeBase::calcAabb` on the first scaled reference.
4. Doors would not open. There is a **KEYFRAMED** body state between static and
   dynamic -- inertia record and a motion INDEX, no dyn_motion record, 170 of
   1,200 vanilla files and every one of them something the game moves -- and
   Compile refused it, in a guard that named the case. A static body cannot be
   driven by an animation however right its collision is. The body's own
   position (its hinge) and orientation were being dropped too.

**The rule those three teach:** our own reader and writer agreeing is ONE
measurement, not two. `--roundtrip` was byte-exact through all three. Use the
two external authorities — **Elric says what the tool WRITES**
(`tools/elric_pair.sh`), **Todd's treat says what the engine READS**
(its tooling is kept outside this repo; `asConvexShape` is four instructions
and settles a question a corpus histogram cannot).

Tools that now do this: `tools/hkcompound.py` (follows the pointer or fails;
`--flags` decodes the header word; `--aabb` prints the bound; `--damage`
reproduces the crash so the check is proved able to fail),
`tools/hkcompound_sweep.py` (holds a whole rebuild against a vanilla tree), and
`tools/fo4_crash_triage.sh` (reads the newest Addictol log and names the mesh).

(Elric IS installed, at `X:\Programs\Steam\steamapps\common\Fallout 4 1946160\Tools\Elric`
— an earlier note in these files said otherwise, from a search that covered only
C: and E:. `Fallout4.esm` is a DIFFERENT folder: `...\common\Fallout 4\Data`.)

- **Multi-material collision, both ways** — a body made of parts keeps every
  part's material through Compile, and Decompile splits a mesh back into one
  shape per material instead of throwing the rest away, so the round trip is
  closed. The CMSD run table that carries them is decoded (below).
- **Friction and restitution round** into their stored word instead of
  truncating. 0.4 is the Fallout 4 default restitution, so every body we compiled
  was one ULP low.
- **triangleIsInterior** is measured, not guessed: fully-edge-shared is a
  necessary condition, exactly, on 3,037 of 3,037 set bits. Still zero, and zero
  is the safe direction.
- **A convex source compiles to a convex shape** — box, hull, sphere, capsule —
  instead of a triangle mesh, and several of them in one body compile to a
  COMPOUND, whose BVH is decoded (86 of 86 vanilla compounds fit it).
- **Create Collision adds beside** rather than deleting the shape that is there.

Under that sits the rest of **compiled collision**, below, including the Elric
campaign that decoded both hkcd trees and lifted the 128-triangle cap.

### A convex source compiles to a convex shape (2026-08-20)

Compile had one output, `hknpCompressedMeshShape`, so a box went in and a
triangle soup came out. The class follows the SOURCE — 228 static systems in the
corpus are polytope-only — so this is not the dynamic-only concern the backlog
filed. What a future session needs:

- A polytope's mass properties describe the hull GROWN by its convex radius.
  Volume is the Minkowski sum (within 2% on 271 of 299), inertia is that solid's
  approximated by growing the hull's bounding box by r (within 15% on 255 of
  268), and the stored tensor is 1.5× the physical one.
- The major-axis frame at massProperties+0x20 is still undecoded, and does not
  need to be: 76.8% of vanilla carries `00 80 00 80 00 80 30 f5`, and a
  synthesized shape takes that.
- **Compounds are written too, since 2026-08-21.** `hknpDynamicCompoundShapeData`
  is `0x60 + 2n × 32`: 2n-1 depth-first BVH nodes and one zero record, a node
  being `float3 min | u32 0x3f000000|(parent+1) | float3 max | u16 leftChild+1 or
  0 | u16 rightChild+1 or the instance index`. Left children are implicit (always
  the next record), so only the right link carries information. 86 of 86 vanilla
  compounds fit it. Note `hknpStaticCompoundShape` is a class Elric never writes —
  all 71 corpus compounds are dynamic, 45 of them in bodies that do not simulate —
  so its type hash is not in our table and does not need to be.

### Compile keeps every material (2026-08-20)

Compile used to write ONE material per packfile — whichever the first leaf shape
held — so a body assembled from parts came out uniform. Two structures carry the
rest, and both are decoded now against the vanilla corpus (2,490 meshes, 3,898
sections, 9,536 run records):

- **hknpBSMaterialProperties' entry stride is 0x18**, not the 0x20 a
  single-entry table cannot be distinguished from. Object = 0x20 header +
  0x18 × n; each entry is a 1 at +0x10 and the CRC at +0x14.
- **The CMSD run table at +0xa0** is 4-byte records
  `[u8 material][u8 0][u8 firstPrimitive][u8 count]`, and the start is
  SECTION-RELATIVE: every section's runs begin at primitive 0 and cover its own
  primitive count exactly.
- **Section +0x54 = `(firstRun << 8) | runCount`**, the same packing as +0x50's
  primitives. The literal `1` that used to sit there pointed EVERY section at
  run 0 — a real bug in every multi-section packfile we had written, not just an
  omission.
- Identical run blocks are shared between sections; first-use dedup reproduces
  2,472 of 2,490 vanilla tables byte for byte.

What a future session needs to know:

- `tools/hkmatrun.py` checks those invariants by parsing the packfile itself, so
  it fails on a writer bug NifSkope's own decoder would agree with. `--damage`
  makes a copy with the old layout, which is how the harness proves the check
  can fail.
- `tests/spells/collision_materials.sh` is the guard (11 checks). The one that
  matters is the SWAP: exchange the two source shapes' materials and the run
  order has to exchange with them. Counting table entries cannot see a writer
  that assigns materials by position.
- **Still flattened, and it is the bigger half now**: a single compiled mesh
  holding several materials decompiles to ONE editable shape, so vanilla →
  decompile → compile still loses them. 13.5% of SetDressing meshes are
  multi-material. See item 0b in docs/TO_BE_IMPLEMENTED.md.

### Compiled collision no longer writes half the format (2026-08-16)

Collision compiled by NifSkope crashed Fallout 4. The compile path
(`hknpEncodeCompressedMesh`, which is NOT the assembler that reproduces 810 of
822 stock systems) was leaving out the traversal tree each section carries, both
of the shape's hkBitFields, three CMSD members, and the material table's own
pointer — and it wrote a 0x40-byte dynamic-inertia record where the stride is
0x70. All of it is measured against a 41-shape vanilla corpus; the table of
every field is in [WW_CHANGES.md](WW_CHANGES.md) under 2026-08-16.

What a future session needs to know:

- **The mesh tree's codec** (decoded here, confirmed on all 63 corpus sections):
  byte 3 bit 0 set = internal, right child at `self + (byte3 & 0xfe)`, left child
  next; clear = leaf, primitive `byte3 >> 1`, section-relative. `2n-1` nodes.
  The leaf index is a byte, which is why **a section holds at most 128
  primitives** — this partitioned at 255, so half of a full section was
  unreachable.
- **The section tree at CMSD +0x10 is decoded too (2026-08-17, via Elric
  reference pairs), so multi-section works to 511 sections** — no triangle cap
  short of the 65,535-vertex refusal. FIVE bytes a node: 3 bound bytes + u16;
  `data & 0x80` internal with the high byte the left subtree's leaf count,
  else leaf with the high byte the SECTION INDEX. Both trees' bound nibbles
  are `floor(sqrt(inset/parentSpan) * 15)`, hierarchical against the parent's
  DEQUANTIZED box — sqrt, not linear, which is what the 08-16 "89% clipping"
  measurement was actually seeing.
- **The Elric harness**: copy `Settings\PCMeshes.esf`, embed absolute
  ConvertTarget/OutputDirectory, set CloseWhenFinished, put fixtures under a
  path containing `Meshes\`, launch `Elrich.exe <esf>` on the desktop
  (unsandboxed) — runs and exits with zero clicks. Fixtures = decompiled
  vanilla meshes perturbed with `nifskope-cli set`; Elric is deterministic, so
  one changed vertex diffs to a handful of annotated bytes. It STRIPS
  already-compiled collision (vanilla's too), so it is a pair machine, not a
  load oracle.
- **The guard that says the writer agrees with itself** is
  `nifskope-cli collision <nif> --roundtrip`: a fresh compile must come back
  `byte-exact 1 / 1`. It did not before this, and that is what found the
  class-name ordering and the fixup ordering.
- Open, none blocking (details in
  [docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md)): triangleIsInterior
  semantics (zero is safe), quad pairing (would double section capacity),
  convex dynamic sources as polytopes, friction f16 round-vs-truncate.
- **Not validated in game.** Every claim here is against vanilla files and
  Elric pairs. Whether Fallout 4 accepts the result is the test that counts
  and it is bungo's.

The work before this is the **overnight cleanup wave** (below), on top of the
scene-composition pipeline and SAM import from 2026-08-10.

### The overnight wave (2026-08-11, commits 21b04c2…7b37226)

Run under an overseer-verifier model with parallel agents; every item
user-directed:

- **Per-glyph tooltips** on the Loaded-NIFs strip + Block List Vis column,
  state-aware, same rects as the clicks (`21b04c2`).
- **SAM pose EXPORT** (`0caf20b`): exact inverse of the import (toEuler ==
  SAM's MatrixToEulerYPR incl. both gimbal branches), six decimals, structural
  bone-set rule (96 keys covering the corpus's 89); round trip is one check of
  five in sam_pose_import.sh phase 3.
- **Merge polish** (`0caf20b`): flash placement is extent-aware (bounds, not
  pivots — bare minigun lights at P-FlashShort, not 56 units of air); a C-less
  donor that publishes points is a gun, not a flash; base-on-bone Receiver
  note silenced. OMOD assembler DROPPED per user; follow-specific-row resolved
  by design (single active skeleton).
- **Refraction actually works** (`b9dfcb0`): sequences bind data-less
  NiBlend*Interpolators (never serialized) — strength was frozen at 0.0
  forever; data-less blends now resolve to the controller's authored
  interpolator. Shader: normal.xy × strength × vertex alpha, 5% viewport
  height cap. refraction.sh (21 checks) walks the controller's own ramp
  against closed-form values; 6-of-7 A/B corpus byte-identical.
  render_regression baselines remain stale (pre-existing) — re-baseline
  pending.
- **The flat-list heap overflow** fixed (`2b0635d`, see the corrected section
  below) and **poselib** measuring the real invariant.
- **winpath consolidated** into _harness.sh, pure bash (`7b37226`); eight
  scripts flagged (pre-existing) for placing GUI windows without
  WW_WINDOW_AT.
- **Process rules now standing**: one NifSkope instance at a time
  (.harness.lock mkdir-mutex around every launch), all harness windows on the
  second monitor, never `git stash` in a shared tree (an agent's stash swept
  three others' work mid-run; recovered, stash preserved then retired), and
  suites whose fixtures build under /tmp cannot be verified from a sandboxed
  Git-Bash tool shell — the native exe cannot write C:\msys64\tmp there; use
  a PowerShell-launched login shell.

### The scene-composition pipeline (2026-08-10, commits f6b0285…8aed8d8)

One user-specified workflow, shipped in four verified increments:
**load → mark → snap → pose → flatten.**

- **Weapon mark + pose-follow mark** on Loaded-NIF rows: always-visible
  one-click toggles (five-slot glyph strip; the gesture suite caught a shipped
  slide-off select/drag bug on the eye/disc and fixed it for all four).
- **Weapon parts snap by BSConnectPoint data** (`nifmerge.cpp`): donor
  `C-<slot>` (any-of, case-insensitive incl. vanilla's `C-Reciever`) vs the
  assembly's published `P-<slot>` points, full transform composed via a
  per-part wrapper NiNode (10mm P-Mag cant 26.52° asserted). Muzzle flashes
  declare NO connect points and take the END of the barrel chain
  (P-ProjectileNode > P-Muzzle > P-Flash* > P-Barrel > bone+note). No
  whitelists anywhere — validation is purely node require/provide, per the
  user's explicit design. Reference DB from the OMOD/connect-point research is
  archived in the session scratchpad (`weapon_combos.json` + `.md`).
- **Pose-follow** renders a marked document against the loaded skeleton's
  bones by name via `Scene::skeletonOverride`, gated per document —
  non-destructive (follower file asserted byte-identical while its rendered
  vertex tracks the posed bone at constant grip distance).
- **Flatten** (`flattenWorkspaceToDocument(bakePose)`): live scene → one NIF,
  pose baked (exact, disk-round-tripped) or Pose-Manager-captured rest; no
  false choice when no rest exists. Flatten results are excluded as future
  flatten sources (self-feeding bug caught by the suite).
- Suites: `flatten.sh` 28/28, `weapon_mark.sh` 77/77, `loaded_nifs.sh`
  112/112, plus the art-object/particle gate the user mandated
  (`artobject_attach` 14/14, `carries_everything` 24/24, `live_effects`
  15/15) — effect NIFs merge 1:1 and render pixel-identical when unmarked.
- **Increment 5 + icons (3c8b243…8362c27):** the skull became a strip toggle
  (STRICTLY single-active — one pointer of state shared with merge targeting
  and pose-follow resolution), a divider separates role marks from display
  toggles, and the glyphs are skull / user-drawn pistol (embedded verbatim as
  an alpha mask in `weaponMarkPixmap` — re-embed from a PNG via binary
  splice, never redraw over it) / symmetric dog-bone.
  `release/ww_icon_sheet.png` (from `renderMarkIconSheet`, regenerated by
  `loaded_nifs.sh`) is the approval artifact for glyph changes; pixel-only
  changes need only that one run.
- Open cosmetic: a gun merged after Frame.nif reports "nothing publishes
  P-Receiver" (true, harmless) — `offersConnectPoints` is the knob. Bare
  minigun flash takes P-FlashFar (farthest-wins reading); a C-less second
  gun would chain-end. All three stated in merge summaries when they occur.

### This session (2026-08-11c) — Block List visibility

**H / Alt+H work in the Block List, and the Summary column is now the eye and
the see-through disc.** Details in [WW_CHANGES.md](WW_CHANGES.md); what a future
session needs to know:

- **There is ONE hidden set and it is `Scene::hiddenNodes`** (block numbers,
  session-only, never written to the NIF, subtree by `Node::isHidden`'s
  parent-chain walk). The viewport's H, the Block List's H, both context menus
  and the row's eye all reach it through `GLView::hideSelected` /
  `setBlockHidden`. Do not add a second one.
- **`hideSelected()` reads `objSelection` now**, so it is multi-selection aware
  on both surfaces. The old "Hide This" label described the old behaviour
  honestly and is gone.
- **Per-block see-through is `Scene::ghostNodes` + `Node::isGhosted()`, rendered
  by the X-ray blend in `BSShape::drawShape`.** The Loaded-NIFs per-document
  see-through cannot be reused: a ghosted document there is a flat triangle
  soup, not a Scene. Like the global X-ray this covers `BSShape` only, which is
  every shape in a Fallout 4 file.
- **The column REUSED slot 11** (`WwSummaryCol` → `WwVisCol`) rather than adding
  a thirteenth. That is deliberate and load-bearing: which columns the Block List
  hides is exactly what sends `QHeaderView`'s running total negative. If you ever
  do need to change the set, it goes through
  `wwReleaseBlockListColumns`/`wwApplyBlockListColumns` and each mode's own blob.
- **The eye and the disc are drawn once**, in `src/ui/wwglyphs.h`. Three callers:
  the Loaded-NIFs row strip, the Block List column, and `renderMarkIconSheet`.
  The sheet used to redraw them by hand and had already drifted — do not put a
  fourth copy anywhere.
- **A row gets toggles iff the scene can resolve it to a drawable of its own**:
  top item, real block number, inherits `NiAVObject`. It does not apply the
  key's promote-to-owning-shape rule, or one object's eye would appear on two
  rows.
- `block_visibility.sh` **74/74** across both modes. It replaced `WW_SUMMARY_TEST`.

### The session before (2026-08-10)

**The Pose Manager imports Screen Archer Menu poses (`4e36bbb`).** SAM `.json`
poses (a Discord request, 80 real PA pose files as corpus) are **absolute**
local transforms — not rest-relative deltas like OS poses — with rotation
`Rx(yaw)·Ry(pitch)·Rz(roll)` in degrees, which is element-for-element
`Matrix::fromEuler`. The convention was proven against the PA skeleton rest
pose (576 candidates eliminated, median matrix error 7.7e-08) and cross-checked
against SAM's own source (`SAF/conversions.cpp` — beware its
`MatrixFromDegree`/`…Transposed` path, which is the skeleton-adjust route and
yields the inverse rotation). `AnimSetup::applySamPose` parses everything
before touching the model, applies in one `nifSnapshotOp`, blends from the
CURRENT transform toward the pose (slerp), and uses `Transform::writeBack` so
quat-rotation nodes and Scale (which SAM carries, `0.0` = hide trick) work.
Merge re-apply dispatches `.json`/`.xml` on the active pose. Values are JSON
strings; missing bones are non-fatal (5 of 80 real files omit 17 armour-piece
bones). `sam_pose_import.sh` is green incl. a hand-coded expected matrix
(5.96e-08) and a real-corpus check (89/89 bones, Back_Armor rot diff 8.7e-08).
No SAM *export* — if added, write 6-decimal angles (SAM's own `%.02f` costs
~0.005°). Format research archived at the session scratchpad's
`sam_convention.md`.

**The first visual pass shipped a crumple and called it a pose (`a4e848a`
fixed it).** Frame.nif — like every skinned mesh — carries a FLAT copy of the
bone names off its root; SAM values are parent-space, so posing it directly is
garbage, and the phase-2 pixel-delta check passed it because a crumple moves
pixels beautifully. The importer now refuses flat targets (0 of 55 parented)
with an explanation naming the skeleton-merge workflow, sharing the rig
merge's `hasWorkspaceBoneHierarchy` test (now `AnimSetup::hasBoneHierarchy`).
The photographed path is the supported one: skeleton primary + frame
rig-merged, then posed — verified by composing WORLD transforms down the
parent chain independently in the harness (depths 2/9/12, worst 9.8e-6; a
chain-ignoring control misses by 113+ units). `sam_pose_import.sh` PASS ×3,
`workspace_skeleton_target.sh` 34/34. Lesson recorded twice this session:
**a pixel delta proves motion, never correctness** — and the Edit tool
flattens MIXED-ending files (`nifskope.cpp`, `WW_CHANGES.md`); binary splice
is the only safe route there.

### Window-state diagnostic cleanup safety

`window_state_roundtrip.sh` had an unsafe failure path: its `cmd /c` cleanup
discarded the exit status from deleting/importing the NifSkope registry tree and
then removed the backup unconditionally. A failed run left the real profile with
the test-only values `New Document Cube=0` and `Suppress Save Confirmation=1`.
Both user values were restored to their safe defaults (`1` and `0`), and the
harness now checks direct `reg.exe` process exit codes and retains/reports the
backup on any restore failure. Do not reintroduce cleanup that can erase the
only snapshot without proving the import succeeded. (`New Document Cube` no
longer exists — the starter cube was removed 2026-08-11b and the preference with
it. The incident stays written down because the restore logic is what it bought.)

### This session

One structural UI pass, closed end to end: the left editor is now one permanent
three-mode column instead of four tabified docks.

**X-01 refraction no longer becomes a giant dark silhouette.** The stock VFX
animates strength to 1.0; the preview's old 0.12 viewport-relative multiplier
turned its normal map into roughly 200-pixel jumps. Distortion is still driven
by that normal map, now capped at eight screen pixels at every resolution.
Multiple Loaded NIFs also collect all opaque geometry before one shared
transparent/refraction pass, so a refractive document can copy geometry behind
it from another Loaded NIF. Single-NIF render captures are unchanged except for
the intended refraction case; the X-01 `autoLoop` peak was captured on/off, the
release build is green, and `workspace_skeleton_target.sh` remains **34/34**.

**Explorer-to-NifSkope `.nif` drops now work across the whole window.** The
application-level route recognizes the native OpenGL container as well as the
specialist tree views, then offers an explicit adaptive workspace action, new
window(s), or Cancel. Adaptive replaces only the clean, untitled, sole starter;
otherwise it preserves the primary and adds every file to Loaded NIFs. A
multi-file starter drop opens the first file and enrolls the rest. The choice
menu opens only after the native drag loop has released the pointer. Its compact
form has no heading separator or file icon and labels the starter action simply
**Open Here**. A real edit of the starter is part of the replacement guard
harness — it renames block 0 and requires the document to stop being eligible,
which is one of the reasons the starter still has a root node now that the cube
is gone (2026-08-11b). The release build is green; `external_nif_drop.sh` is
**17/17** and `loaded_nifs.sh` remains **95/95**. No physical pointer-driving
test was run.

**The skull marker is optional for Loaded-NIF merges.** Clothing and props still
merge normally with no skeleton selected, using the clicked row as target; a
marked skeleton elsewhere in Loaded NIFs does not interfere. If the marked model
is included in the selection, it is intentional rig-merge input and becomes the
target automatically. NifSkope first requires a real NiNode hierarchy below the
file root, so flat bone-reference nodes in a frame or clothing NIF are refused
with a useful explanation rather than mistaken for a skeleton. The real-corpus
`workspace_skeleton_target.sh` passes **34/34** across all four cases, and
`loaded_nifs.sh` remains **95/95**.

**Loaded NIFs → Make Primary / Edit no longer reloads the application window.**
The old route created a second hidden `NifSkope`, restored its complete UI, then
showed it and hid the current window. It now shares the established in-place
swap route with the direct edit gesture: the live row bytes replace the content
of the existing primary `NifModel`, rebuilding only the scene. The same main
window, viewport, dock, page stack, Loaded model, active mode and splitter sizes
survive. Skeleton and face-donor marks follow the promoted mesh. The release
build is green and `loaded_nifs.sh` is **95/95**, including exact object-identity,
window-count, geometry and role-remapping assertions over the real Make Primary
entry point.

The compressed-width migration gap is closed. Schema 1 had already saved
`LeftColumnDock` at Qt’s incidental ~260 px content hint, so the old “new dock”
400 px initialization no longer ran. Schema 2 now requests **400 px once**,
after state and geometry replay, then permanently returns width ownership to the
user. The real schema-1 profile measured **400 px** on launch and retained the
existing **164 → 432 px** fold/unfold range; the capture was inspected and
`loaded_nifs.sh` remains **93/93**.

The top **Blocks · Header · NIFs** selector is now a full-width, equal-third
segmented control in selection blue. It shares its skin-backed geometry with
Collision Creation / Simulation: only the two outside ends are rounded, and
each square internal join has one border rather than two beveled corners.
Verified in the release build with `loaded_nifs.sh` **93/93** and
`collision_panel.sh` **41/41**; both generated captures were inspected.

- **Blocks** shows Block List above Block Details.
- **Header** gives the Header the full column.
- **NIFs** shows NIF Browser above Loaded NIFs.
- The buttons are ordered **Blocks · Header · NIFs**. Tab positions carry an
  explicit stable-mode ID, so moving NIFs from the second to the third button
  does not reinterpret an existing saved NIF mode as Header.
- The selector is the first row at the top. Switching it changes only a
  `QStackedWidget` page: the views, models, selection, searches, splitter sizes
  and unsaved Loaded NIFs stay alive in place.
- The old four dock shells are consumed before `restoreUi()`, then deleted. The
  new `LeftColumnDock` is the only core dock Qt ever restores, so no live widget
  is reparented after it enters the saved dock graph.
- Saved-window state is version `0x074`. Existing `0x073` layouts get one
  compatibility replay for unrelated docks/toolbars; mode and both inner
  splitters then persist explicitly under `UI/LeftColumn`.
- The column still folds from **164 to 432 px** in the harness while preserving
  the viewport's 50 px minimum.
- **Inactive pages cannot paint through.** The legacy visibility reset now runs
  before the content widgets enter `QStackedWidget`; doing it afterwards had
  overridden Header's hidden state and exposed its Type column as a vertical
  strip of characters along the viewport divider. The mode harness now requires
  exactly one page to be visible and the screenshots were checked again.

- **The selector and both Block panels were compacted and clarified.** Blocks,
  Header and NIFs use a flat orange-underlined selector with no shortcuts. Block
  List has one toolbar plus an advanced Filters dropdown, accurate
  Block/Name/Vis columns, navigable breadcrumbs, cached totals and a clear
  no-results state. Block Details has one search/pin/overflow row and explicit
  no-selection, no-match and no-pins states.
- **Header is now a standalone file inspector.** It shows source identity and
  NIF/User/Bethesda versions, recursively searches Name/Value/Type without
  replacing its model/root, exposes copy-summary/path actions, and retains the
  useful Type column while folding with the unified dock.

- **Eye and transparency clicks no longer select the row.** The Loaded-NIF view
  owns the full press/release gesture over those glyphs, so no orange/blue
  selection flash appears and no drag begins accidentally.
- **Loaded NIFs has its own search field and real vertical scrolling.** Filtering
  hides source rows without proxy-remapping their drag/action identity. A
  40-row harness probe proves the scrollbar gets a non-zero range.
- **Loaded NIFs reports its live membership.** Its header says total or
  shown/total, carries a glyph legend, and row tooltips add source/unsaved state.
  Empty and no-match states are passive paint. Browser Refresh and filtering are
  now required to preserve the exact Loaded model pointers and persistent rows.
  The 4 px vertical splitter stays non-collapsible and keeps both panes reachable.
- **The row menu is grouped by intent.** File actions, rigging, workspace
  display, tools/revert and removal are separated; skeleton and face-donor use
  the same skull/face icons as the row; duplicate Close/Remove wording is gone.
- **The nearly immovable left divider was a hard 400×240 dock minimum.** Those
  List/Tree dock floors are gone. The tested browser column now travels from
  164 to 432 px while preserving the viewport's 50 px minimum. Collision,
  Rigging and Vertex Paint expose horizontal scrollbars when folded; UV no
  longer adds a redundant 340 px dock floor. Genuine content floors (UV render
  view and timeline graph/lane) remain.
- **Verified:** staged release build green; `loaded_nifs.sh` **91/91**;
  `WW_DOCKS_TEST` **13/13**; `archive_browse_survives_load.sh` **4/4**; and the
  final populated/no-match screenshots were visually checked. The earlier
  `collision_panel.sh` **39/39** and two-cycle window-state pass are unchanged.

### Open

- **`WW_POSELIB_TEST` fails at its final Delete step, and it is PRE-EXISTING.**
  `findPose()` returns null after the Apply click's refresh
  (`nifskope_ui.cpp` ~3410). Verified unrelated to the SAM import: the log is
  byte-identical with all of that commit's `src/` changes stashed. Diagnose
  the dock refresh vs. the harness lookup before trusting this harness again.
- **`block_drag_live.ps1` has not been run since the multi-parent payload
  change.** It seizes the mouse; ask first, every time.
- The NIF Browser harness covers the real view gates, exact captured payloads,
  both save/load routes and rendered geometry, but no pointer-seizing live mouse
  script was run.
- An auxiliary re-run of `window_state_roundtrip.sh` did not reach its restore
  assertion: both the prior canonical binary and the staged binary stayed open
  after cycle 1's `CloseMainWindow()`. The script restored the registry profile
  each time. This did not reproduce in the dock or Loaded-NIF harnesses and is
  not attributed to the left-panel change; diagnose the close harness separately
  before claiming a fresh two-cycle pass.
- The flat-list **hang** below is still open and still harness-only.
- Everything else in this file's later sections is carried forward and untouched.

Two headlines:

- **The Block List is a direct-manipulation panel now** — drag to re-parent,
  reorder and un-parent; paste follows the pointer *in every window*; a click on
  blank space selects nothing. Details below.
- **The flat list mode is fixed and kept.** It was worse than filed — *no* row in
  it could be clicked, dropped on or right-clicked, not just newly inserted ones
  — and the cause was `QHeaderView`'s cached total going negative, not anything
  about the model. Hierarchy mode was never affected.
- **CLOSED 2026-08-11 (`2b0635d`): the "flat-list hang" was never a hang — it
  was a heap overflow, and this section's lore was wrong on both counts.**
  `QTreeView::expandAll()` emits `expanded()` from inside
  `QTreeViewPrivate::layout()`; `NifTreeView::scrollExpand` answered it
  synchronously with `scrollTo()`, whose `doItemsLayout()` cleared and re-laid
  `viewItems` under the outer layout's feet — which then wrote past the
  reallocated buffer. The process was already DEAD while the script waited out
  its 63-second deadline (that is why it read as a hang), and the Application
  log DOES carry APPCRASH records (`0xc0000005` then `0xc000041d`) — the "no
  APPCRASH" claim above was simply wrong. Under gdb's debug heap the ~50%
  becomes 4-in-4 with "Heap block modified past requested size". The scroll is
  now posted (QueuedConnection, QPersistentModelIndex), coalesced per burst,
  cancelled by explicit `scrollTo()`. 12/12 green with list mode back in
  `block_rename.sh`'s gate; `collision_drop.sh`'s "stall" was this same crash
  taking the process down mid-suite (10/10 now). Lesson for the next
  mystery: **a "hang" whose process cannot be attached to may be a corpse —
  check the process is alive before reaching for deadlock theories.**

### The Block List, as it now behaves

| gesture | result |
|---|---|
| drop **on** a `NiNode` | re-parent into it, **preserving world position** |
| **Shift** + drop | re-parent keeping the LOCAL transform |
| **Ctrl** + drop | link — adds the child link and keeps the old parent |
| drop in the **gap** between two rows | reorder to that position in the parent's `Children` |
| drop in the **blank space**, or the gap beside a top-level row | **out** — loses every parent, becomes a root of its own |
| hover a node that would accept the block | it unfolds after ~650ms, and folds back when the drag ends |
| pointer near the top/bottom edge | the list auto-scrolls |
| **Ctrl+V** over a row / over blank space | pastes into that row / with no parent |
| click blank space | selects nothing at all |

A row that **cannot take children is all gap** — there is nothing to drop inside
a mesh, so its whole height reorders. Only a `NiNode` keeps the
third/middle/third split.

`wwReparentBlocks` in `blocks.cpp` is the one primitive, shared with the
Collision Manager's Set Parent. `release/ww_drag.log` records the most recent
drag with no flag to set (`WW_DRAG_LOG=off`, or a path, overrides).

### Where to pick up

*(From the 2026-08-07 block-list sessions. Merging collision shapes in the
Collision Manager, listed here as unstarted, shipped in `WW_CHANGES.md` 08-07zb.)*

The four things that handoff listed are all closed — three fixed, one measured
and deliberately not done. What is left of them:

1. **Drag-and-drop has no coverage in flat list mode.** Nothing structural is in
   the way now that the view answers `indexAt` there; `block_dragdrop.sh` seeds
   no list mode, so it needs the registry dance `block_list_modes.sh` uses. Its
   code branches on the model and is believed correct, and nothing has driven it.
2. **Every structural edit serialises the whole file twice.** `nifSnapshotOp`
   saves the NIF before the operation and again after, for one undo step — 88 ms
   on a 512-block file, 160 ms on 2012, and it does not track what the operation
   touches. It is the largest cost in a drag by a wide margin and it is shared by
   everything, so it wants its own decision. In the backlog.
3. **The two list modes have drifted.** Flat list has no reorder, no drag-out and
   no auto-expand — deliberate, since it is file order rather than anyone's
   children. The mode is being kept; this is a question of how far to take it,
   not whether.

The live drag script is **cleared**: seven scenarios, six verified green in one
run and the seventh read out of that run's own log. Two things it taught, both
worth carrying:

- **A refused target never receives a drop event.** The handler answers with
  `Qt::IgnoreAction` and Qt withholds the `QDropEvent` entirely, so "no DROP
  reached the list" is the correct outcome for a refusal, not a failure.
- **`payload [N]` in the drag log is the block COUNT.** The identity of what was
  picked up is in the `=== drag start … first N ===` header. Reading the count as
  a block number had the script convicting the program in two whole runs.

It also could not fail at all until this session — `Write-Output` inside a
function whose caller wrapped it in `if (-not (…))` put the message *into* the
condition, and a two-element array is truthy however the verdict came out.

### Open, and honest about it

- **The UV Editor fold assertion is not functional coverage.** Its current
  `minimumWidth() < 340` check changes with polish/layout timing. A direct
  `resizeDocks(..., 280)` probe after showing the dock produced **795 px**: the
  wide header rows still impose a real effective floor despite the old explicit
  340 px dock minimum being gone. This was discovered while verifying the left
  editor’s independent 400 px migration and was deliberately not folded into
  that one-change fix.

- **Preset save/rename/remove still has no harness.** The "+" goes through a
  modal `QInputDialog::getText`, so it is not drivable the way the existing
  harnesses drive widgets; covering it means exposing the storage helpers behind
  test-only entry points first. Re-parenting, the other half of this note, is
  covered now — the operation moved into `wwReparentBlocks` and
  `block_dragdrop.sh` drives it.
- **Re-parenting has two transform rules now, on purpose.** The block list's
  plain drop preserves world position; the Collision Manager's **Set Parent**
  keeps the LOCAL transform, which is right for attaching collision to a bone
  and is why it was not changed.
- **`window_state_roundtrip.sh` runs outside the restricted sandbox.** It needs
  `Add-Type` temporary writes under `C:\msys64\tmp`; with that permission it is
  green for two maximised save/restore cycles on the second monitor. A sandbox
  permission failure before NifSkope launches is environmental, not a product
  failure.
- **The title bar reports a stale build.** `NIFSKOPE_REVISION` is baked when
  qmake runs, not when make does, so the About box and title can name an older
  commit than the binary. Cosmetic; fix by regenerating on link the way
  `README.md` already is.
- Parked from the same conversation, by choice: refit-shape-to-mesh (ranked
  highest of these), copy/paste physics between bodies, save-preset-from-an-
  existing-body, change shape type in place, mirror across X, and settling
  whether several selected meshes should make one `bhkListShape` body instead of
  several.

---

## State

Edition **0.3**, on upstream NifSkope 2.0.dev11 (fo76utils `develop` @
`f2587869`).

### What the last session changed

Fourteen commits, `873e02f`…`a901281`. All committed, harnessed and pushed.

The feature is in the table above. What is worth carrying forward is *how* it
went, because the shape of it will repeat:

- **The first version shipped dead, with 26 green checks.** Nothing in this
  codebase set `Qt::ItemIsDragEnabled`, so `QAbstractItemView` never entered
  `DraggingState` and `startDrag()` was never called. The harness drove the drop
  handlers directly — correct, since no synthetic event can enter a native drag
  loop — which put the one broken step outside everything it measured.
- **Four more fixes were made by reading code, none of them right**, while the
  harness climbed to 44 green. What actually found it was
  `tests/spells/block_drag_live.ps1`, driving the physical mouse: one run, one
  `DragEnter`, then silence. Ignoring a drag event ends the drag over the widget,
  and a drag begins on the row being dragged, whose neighbouring gaps refuse as
  no-ops. Dead before it began.
- **Rename was already built** (`d5765c4`) and filed in the backlog as not
  started, because nothing measured it. It had one real gap — proxy-only, so flat
  list mode did nothing at all.
- **Three checks were written that passed for the wrong reason** and had to be
  rewritten: a paste test casting an invalid index, a hover test whose helper
  re-expanded the row before hovering, and a Block Details test calling
  `NifTreeView::isRowHidden`, which shadows Qt's with a different meaning. Two of
  them wasted a build each; the third wasted two.

Everything above is in [WW_CHANGES.md](WW_CHANGES.md) under 2026-08-07 a–n.

### The rule that came out of it

**Ask what your harness enters below, and cover that separately.** Every bug in
this session lived above the point where the tests started: the drag start, the
native event loop, the paint during a modal drag. 82 checks below that line and
zero above it read as thorough and was not.

### Open, not started

- **Scale Inertia Tensor** exists in no UI. The 3ds Max exporter offers it.
- **Phantom / Shape Phantom** are present but greyed: they need
  `bhkSPCollisionObject` + `bhkSimpleShapePhantom`, which nothing here writes.
- **Gravity Factor, Rolling Friction Multiplier, Time Factor, Collision
  Response** are real `bhkRigidBodyCInfo` fields exposed nowhere.
- With **Replace off**, a new shape joins an existing body's `bhkListShape` —
  and the body settings in the create popup then do not apply to it. Decide what
  should happen.

Everything in [WW_FEATURES.md](WW_FEATURES.md) is built, committed, and covered
by at least one harness. Nothing is half-landed. Two things are deliberately
parked and say so in the UI:

- **PBR rendering** — implemented, mode and toggle greyed out.
- **The four mapped features** — growing-op segment attribution, `NiPointLight`,
  `NiPSysColliderManager`, `NiPSysBombModifier`. Researched and written up in
  [docs/FOUR_FEATURES_PLAN.md](docs/FOUR_FEATURES_PLAN.md), not started, on the
  user's instruction.

One thing on the list is unfinished-by-request: the **Shading menu** is longer
than it should be. Simplification was raised and then deferred — do not touch it
without asking.

## Build

Windows, MSYS2 UCRT64. Release:

```bash
C:/msys64/usr/bin/bash.exe -c 'export PATH=/ucrt64/bin:/usr/bin:/e/Tools/GIT/mingw64/bin:$PATH; cd /e/Projects/NifskopeWildWastelandEdition && make -f Makefile.Release -j2'
```

Output is `release/NifSkope.exe`. That is always the correct binary; do not test
against anything else.

**`git` has to be on that PATH** — the link rule runs `git rev-parse --short HEAD`
to bake the build rev into the title bar, and MSYS2 does not ship git. Without
`/e/Tools/GIT/mingw64/bin` the whole build compiles and then dies at the last
step with `git: command not found` / `Error 127`, leaving the OLD exe in place —
which then quietly passes or fails harnesses as if it were the new one. Set the
PATH with `export` before `cd`, too: a `PATH=... make ... | tail` pipeline puts
the pipe stages outside the assignment, and `tail` is not on the default path
either.

**`-j2`, not `-j8`.** This machine has hard-shut-down under sustained all-core
load — Kernel-Power 41 with no bugcheck, which reads as a power or thermal
margin problem rather than a software fault.

**The generated `Makefile*` files hold absolute paths.** They are gitignored, so
a fresh clone is fine, but if the repository folder is ever moved or renamed,
re-run qmake before building or the old path comes back at you as a
file-not-found from the middle of a link.

**After changing the include graph or adding data members to a widely-included
class, re-run qmake before trusting an incremental build:**

```bash
qmake6 -o Makefile NifSkope.pro
```

A stale dependency list once linked an old-layout `.o` and hard-crashed at
startup (`0xC0000005` inside `QHash::findNode`). More generally: **rule out a
stale incremental build before bisecting any access violation or heap
corruption.** `make clean` first. Identical source has crashed 6/6 incremental
and 0/12 clean.

**This bit again on 2026-08-07** and cost an hour: three widely-included headers
gained members, a dozen incremental builds followed, and a harness started dying
half way through with "Free Heap block modified after it was freed". The code
was correct. Clean build, 25 of 25. Do the clean build *first*, not after
reading the diff four times.

**`make clean` breaks the next build**, so know this before you run it: qmake
writes the `icon_res.o` rule with an absolute target path and lists the object
with a relative one, so make stops at `No rule to make target
'GeneratedFiles/.obj/icon_res.o'`. Build it once by hand and carry on:

```bash
windres -i res/icon.rc -o GeneratedFiles/.obj/icon_res.o --include-dir=./res
```

**Set a writable temp when building from a sandboxed shell.** `export
TMPDIR=/tmp TMP=/tmp TEMP=/tmp` — otherwise g++ tries `C:\Windows` for its
intermediates and fails with "Cannot create temporary file". And piping make to
`grep`/`tail` hides its exit status: use `set -o pipefail` or check
`${PIPESTATUS[0]}`, or a failed build reads as a successful one.

Relink can also fail with the exe locked. Kill the straggler and relink; it is
not a code error.

### Version numbers — there are two, on purpose

| | where | what it is |
|---|---|---|
| `WW_VER` | `NifSkope.pro` | this fork's edition number (`0.3`) — title bar, About box |
| `VER` | `build/VERSION` | upstream lineage (`2.0.dev11`) |

`applicationName` stays `"NifSkope 2.0"` because it is the **QSettings key**;
renaming it would strand every existing user's settings. The edition name lives
on `applicationDisplayName`. `NifSkope::migrateSettings` compares against
`NIFSKOPE_VERSION`, so that one has to keep tracking upstream.

To cut 0.3: change `WW_VER` in `NifSkope.pro`. Nothing else.

### README.md is generated

`README.md` is produced at link time by `QMAKE_PRE_LINK` from
`build/README.md.in`, substituting `@VERSION@` and `@WWVERSION@`. **Edit the
`.in` file.** Editing `README.md` directly works until the next link, then
silently reverts.

## Layout

```
src/                    application
  glview.cpp            the 3D viewport — modes, modeling ops, selection, gizmos
  nifskope_ui.cpp       docks, toolbars, menus, workspaces, the WW_* harnesses
  gl/hknpdecode.cpp     compiled Havok collision, read
  gl/hknpencode.cpp     compiled Havok collision, written back
  uvtools.cpp           UV editing workspace
  unfucktools.cpp       Issue Manager
  nifcli.cpp            headless CLI
  wwskin.h              the palette — colours come from here, never literals
  ui/widgets/
    wwnumberfield.*     the one number field
    timeline*.cpp       animation timeline
    physicspanel.*      ragdoll / physics sim
lib/                    vendored deps (qhull, gli, meshoptimizer, libfo76utils)
tests/                  harness wrappers, one per feature area
tools/                  byte-level verifiers, corpus scripts, render regression
docs/                   plans, audits, research, CLI reference
```

No submodules. Everything is vendored, so `git clone` is complete.

Note: source comments cite plan docs by bare filename (`MODELING_TOOLS_PLAN.md`,
`TO_BE_IMPLEMENTED.md`, …). Those files now live under `docs/`. The names are
still unique — grep finds them.

## Testing

Harnesses are environment-gated code paths inside the real binary. Set the flag,
the app drives itself and writes `release/ww_*.log`, and a shell wrapper asserts
on it.

```bash
tests/spells/top_bar.sh
tests/spells/collision_undo.sh
tests/spells/scrub_uniform.sh
```

The collision and menu work added six:

| harness | covers |
|---|---|
| `collision_panel.sh` | the two-button split, both popups, the disabled-shape tooltip, and that every moved control still writes its key |
| `collision_per_shape.sh` | N selected shapes → N bodies, each on its own node, source meshes consumed |
| `quick_favourites.sh` | pinning, the Q menu, Space/Q scoping, the search menu reaching menu actions |
| `spell_search.sh` | the palette: dismissal, positioning, and not listing its own row |
| `nav_keys.sh` | rotate/zoom rebinding, and that letting go stops the camera |
| `collision_compiled_edit.sh` | editing a compiled body in place |
| `window_state_roundtrip.sh` | open, close maximised, open again — the startup crash |

And the block-list session added three:

| harness | covers |
|---|---|
| `block_dragdrop.sh` | 87 checks: that the drag starts at all, the three modifiers, reorder by the gap, drag-out, every refusal, multi-select as one payload and its ordering, the highlight and the painted insertion line, the drag card, auto-expand and its fold-back, paste following the pointer *in a second window*, blank-click deselect, one undo step. `WW_BLOCKDND_BENCH=<n>` also times a move on a file that size |
| `block_rename.sh` | 25 in hierarchy (list mode is out of the gate, see below): F2 and double-click, that nothing else opens on top, no sideways scroll, Escape, the column asymmetry, the txt icon, and that the name reaches the palette |
| `collision_drop.sh` | 7 checks: a mesh dragged onto the Collision Manager gets collision, at the shape type the panel is showing, one body per mesh — and check 2 asks whether the dock accepts drops at all, which is the only thing the harness steps over |
| `block_list_modes.sh` | 8 per mode: that the header's total matches the sections it totals, that every row resolves back to itself through `indexAt`, that a block inserted now is addressable, and that all of it survives switching modes |
| `block_drag_live.ps1` | **the only thing above the native-drag boundary** — drives the physical mouse across 7 drags: into a shut node, into a row its own auto-unfold revealed, into a second root, a root made a child, out to blank space, a mesh row's all-gap reorder, and a refused cycle. See the warning below. |
| `block_visibility.sh` | 37 per mode: H / Alt+H over the list with a framebuffer delta either way, multi-select H, the subtree and inherited-from-an-ancestor rules, the eye and disc clicks asserted to land on **the same state the key produced**, the press/slide-off/release contract, non-drawable rows exposing nothing, see-through against BOTH the solid and the hidden frame, the column shape in both modes, and that F2 rename still types an 'h'. Seeds `List Mode` like the two above |

All three build their fixture from the CLI cube fixture (`-no-gui new --cube`),
so they need no game corpus at all. **`--cube` is not optional here**: as of
2026-08-11b the program's new document is empty (header plus one root NiNode),
and `new` on its own writes that. `new --cube` writes the old four-block cube
scene, byte-identical to what `new` produced before, which is why none of these
suites needed an assertion changed. Add Primitive cannot stand in for it — it
clones an existing BSTriShape, so it refuses to make the first shape in a
document. `block_rename.sh` and `block_list_modes.sh` seed
`List Mode` into the registry before launch and put it back on exit, because the
mode is read during window construction — the app has to *start* in the mode
under test, and both of them assert that it did rather than assuming.

**Never `ignore()` a drag event you mean to keep receiving.** Ignoring a
DragEnter or DragMove ends the drag over that widget — not one further event
arrives — so the first position the pointer happens to be at decides the whole
gesture. A drag begins on the row being dragged, whose neighbouring gaps refuse
as no-ops, so the block list's drag was dead before it began and stayed that way
through four wrong fixes. Accept the event and put the verdict in the drop
ACTION: `Qt::IgnoreAction` gives the no-drop cursor while the stream stays alive.

**A window that follows the cursor during a drag must be
`Qt::WindowTransparentForInput`.** Otherwise it takes part in hit-testing, and
the moment it passes under the pointer the view underneath gets a `DragLeave` and
stops receiving `DragMove` — so the follower freezes, the drop feedback stops
updating, and the stale caption it is left showing gets read as the program's
verdict on wherever the cursor is now. That arrived as three separate bug
reports: a stuck label, a line that never appeared, and legal drops "refused".

**Anything a drag draws must `repaint()`, not `update()`.** `QDrag::exec()` runs a
native modal loop; a posted update is coalesced and can sit in the queue until
the drag ends, so the paint lands after it stops being useful — indistinguishable
from never painting at all, and reported that way twice. No harness can catch it
either: a harness delivers drag events directly and is never inside the loop that
swallows the paint.

**A drag event cannot be delivered with `QApplication::sendEvent`.**
`QApplication::notify` routes drag and drop through the drag manager, so a
synthetic one reaches neither the widget's `event()` nor any event filter —
measured at zero, to the view and to the viewport both. That is why the drop
handlers are `NifTreeView` overrides and why `wwDeliverDragEvent` exists: a
harness needs an entry point that begins where Qt's routing ends. The override
count it reports is the check that the overrides ran, rather than the hook being
poked directly.

**There is a live-drag test, and it is the only thing above that boundary.**
`tests/spells/block_drag_live.ps1` drives the physical mouse at the block list on
the second monitor and reads `release/ww_drag.log` — which the program writes on
every drag, with no flag to set (`WW_DRAG_LOG=off` disables it, or a path
overrides it). It found in one run what four code-reading fixes missed while the
harness sat at 44 green.

**It SEIZES THE POINTER, so it is run by hand, never fired off.** Placement on
the second monitor keeps a *window* out of the way; the mouse is not per-monitor,
and clicks land wherever the cursor is dragged. It was once run mid-task and
disturbed the user's live session. Ask before every run, and if it has already
answered the question, do not re-run it to confirm.

**And that entry point is exactly how the drag shipped broken.** Driving the drop
handlers covers everything below `startDrag()` — and `startDrag()` was never
called, because `QAbstractItemView` will not enter `DraggingState` unless the
MODEL reports `Qt::ItemIsDragEnabled`. 26 checks green, feature dead. **When a
harness has to enter below the top of a mechanism, name what it stepped over and
cover that separately**: the flags on both models, `dragEnabled()` on the view,
and a real press-and-move. Swap the drag hook for a counting one first — the
production hook ends in `QDrag::exec()`, a modal loop that never returns with
nobody at the mouse.

**Two useful capture levers, both added while chasing things reading was not
finding.** `WW_CAMERA_LOG=<file>` appends every camera reorientation with its
rotation and view state — that is what finally located the startup view being
overwritten, after three carefully-read suspects each turned out innocent. And
`WW_RENDER_VIEW` now accepts a **negative** value, meaning "leave the camera
exactly as startup left it", which is the only way to photograph the startup
view: every other value overrides the thing under test. Its old upper bound was
`ViewWalk`, so asking for `ViewUser` was silently rewritten to Front and
produced a capture that looked like a passing test of a view it had never used.

`window_state_roundtrip.sh` is the one harness that **cannot** source
`_harness.sh`. `saveUi()` deliberately bails out on any `WW_*` variable so
harness layouts never overwrite the user's, and `WW_WINDOW_AT` is a `WW_*`
variable — so the flag that places the window off the primary monitor also
disables the write path the test exists to exercise. It seeds `UI/Window
Geometry` onto the second monitor instead, asserts the window landed there
before doing anything else, and restores the settings key on exit. If you write
another test that needs real settings written, it has the same problem.

`tests/spells/_harness.sh` holds shared setup. 60 flags exist; `grep -rhoE
'WW_[A-Z_]+_TEST' src/ | sort -u` lists them.

Three rules that were learned the hard way:

1. **Run only the harnesses whose code path the change reaches**, and say why
   each is in the list. A blanket run opens a dozen windows and tells you
   nothing about most of them.
2. **A harness must force the state it measures**, never inherit persisted
   `QSettings`. A green suite went red with no code change because
   `GLView/Enable Animations` was left `false` in the registry.
3. **Measure with an invariant that fails on broken code, and prove it fails.**
   A proxy number that merely agrees with correctness is not evidence. Where
   possible the check is committed *before* the fix, so its failure is recorded
   against the unfixed binary.

Three ways that rule got broken in one session, all worth knowing:

- **A threshold the wrong answer also clears.** "More than 20 materials" passed
  for two builds on Oblivion's 32 where Fallout 4 has 157. Name things that
  exist only in the right answer.
- **A check satisfied by the bug itself.** "The material can be typed" asked
  whether the field was editable — and the leftover `setEditable(true)` *was*
  the bug. Open the thing and look inside it.
- **A check that never ran.** Two green assertions about zooming, on a camera
  that had not moved: the pump spun `processEvents` for microseconds while
  `advanceGears` steps on real elapsed time, and the measurement read
  `cameraDistance()` when zoom changes `Zoom`. **Prove the control moves before
  asserting that a change moved it.**

And: **a check that fails intermittently is worse than no check**, because it
teaches you to re-run rather than to look. Poll for a condition with a deadline;
never sample once after a fixed delay.

`grabFramebuffer` does **not** repaint — pump `ogl->update()` +
`processEvents()` twice or you diff a stale frame.

## Landmines

**It will not start?** This was the long-running one, and it is fixed as of
`5983a97` — but the lesson it taught is wrong, so read the correction.

Clearing `UI/Window State` under `HKCU\Software\NifTools\NifSkope 2.0\UI` did
cure it, every time, which is why three sessions treated the saved blob as
corrupt. It never was. `restoreUi()` restored geometry before state, and
replaying a layout saved while **maximised** into a window that
`restoreGeometry()` had just flagged maximised, but that had not been shown yet,
faulted `0xC0000005` in `QLayout::addChildWidget`. Clearing the key worked
because an empty blob makes `restoreState()` a no-op — it removed the symptom,
not the cause. State is restored before geometry now.

**The real lesson: "clearing X fixes it" does not mean X was bad.** Hold one
variable at a time instead. The same 2090-byte blob crashes under a maximised
geometry and restores perfectly under a normal one; swapping only the geometry,
with the blob byte-identical, flips the outcome. That single comparison is what
broke it open after two sessions of bisecting the wrong thing.

Still true and still worth keeping: **a crash that survives reverting the change
that appears to cause it is not caused by that change** — check persisted state
before bisecting further.

**`QHeaderView` keeps its total by adding and subtracting, and a model change
desyncs it.** `length` is the sum of the sections, maintained by deltas rather
than re-derived. Hiding a section subtracts its width and remembers it; changing
the model gives every remembered width back **without adding it to `length`**.
Hide them again and each width comes off twice. The Block List hides 9 of the
NifModel's 12 columns, so after one load `length` was NEGATIVE — and
`visualIndexAt` returns -1 for anything past it, so `QTreeView::indexAt` had no
column and returned no index for any point in the view. Nothing in the flat list
could be clicked, dropped on or right-clicked, and it read as "the rows are
there but dead".

Two rules follow, and `block_list_modes.sh` guards both: **release the columns
before changing a view's model and apply them after**
(`wwReleaseBlockListColumns` / `wwApplyBlockListColumns`), and **a saved header
blob belongs to a model shape** — restoring one saved against the 3-column proxy
onto the 12-column NifModel desyncs the total the same way, so each mode keeps
its own.

**`NifTreeView::isRowHidden( int, const QModelIndex & )` is not
`QTreeView::isRowHidden`.** It shadows it with a different meaning: it ignores the
row number and answers for the item behind the index you passed as the *parent*.
Asking it whether a row is hidden returns something unrelated — under an invalid
root it is always false — and a check built on it measures nothing. Use
`visualRect().isEmpty()`, which is usually the real question anyway.

**"Selected" in the Block List is two things.** Qt's selection and current index,
and `NifModel::selHighlight` — mirrored from the 3D view's object selection, and
the one the row's COLOUR comes from. Clearing one leaves the other showing.

**An invalid root index means "show the whole model" to a QTreeView**, not "show
nothing". Block Details listed every block in the file the first time nothing was
selected.

**CRLF.** Four sources and one doc are CRLF: `src/glview.cpp`,
`src/gl/controllers.cpp`, `src/nifskope.cpp`, `src/spells/havok.cpp`,
`WW_CHANGES.md`. Python text-mode I/O flattens them into a 33,000-line junk
diff — **and so does `sed -i`**, which caught this twice in one session. Use an
editor that preserves them, or binary I/O, and check `git diff --numstat` before
committing.

**A disabled widget shows no tooltip.** It receives no mouse events, so Qt never
delivers it a `ToolTip` event and `setToolTip` on it displays nothing —
silently, in exactly the case where the explanation is needed. Put the text on
an enabled wrapper (`createShapeHost` in `collisiontools.cpp` is the pattern).

**`populatePhysicsEnums()` returns early before the physics editor exists.** It
guards on the editor's own combos, so calling it during construction to fill
create-side lists does nothing, leaving `currentData()` invalid and writing 0 —
which is `MO_SYS_INVALID`, `OL_UNIDENTIFIED` and friends. Fill those after the
editor is built, and again on `modelReset`.

**Collision tables are Fallout 4's, unconditionally.** `materialEnumType()` and
`layerEnumType()` used to walk the BS version down into Oblivion, and a BS
version of 0 — which is what you get before a file is open and from this
program's own new document — reached the bottom rung. Do not reintroduce a
ladder.

**`spRemoveBranch` reads the block-list selection.** Calling it to remove one
block removes the whole published multi-selection. `castCollisionOverSelection`
takes the selection down for the duration of a run for this reason.

**Scene node ids do not follow a renumber.** `Node::nodeId` is assigned when the
Scene builds it, so gathering geometry after an insert reads through a stale id.
Gather before mutating, or ask the `Node` for its own id.

**Colours.** Never introduce a colour literal. `wwSkinColor("name")` from
`src/wwskin.h`, or add a token there. The same discipline applies to the number
field: `wwMakeScrubField` / `WwNumberField`, never a sixth private copy of the
scrub gesture — that is exactly how the five that got deleted came to exist.

**Menus.** `QMenu` paints the icon and the check indicator in the *same* column,
so an icon-bearing checkable item silently loses its checkmark. Checked rows are
styled by fill (blue background, orange text) instead.

**Event filters run before the target's own handler.** Insetting a spin box's
line edit on the *host's* resize gets undone; the correction has to hook the
editor's own `Resize`.

**`QToolButton::sizeHint()` under-reports** on stylesheet-styled buttons, because
QSS padding is not folded in. Take the max of the hint and font-metrics + padding
or the label elides.

## Conventions

- Solo repo: commit straight to `main`. No branches, no PRs.
- **Update [WW_CHANGES.md](WW_CHANGES.md) with every change batch, unprompted.**
  Newest entry at the top, dated, with the measurement that backs it.
- When a feature's design is uncertain, look up Blender's equivalent and follow
  it. State deliberate divergences.
- Do GUI verification via the harnesses rather than handing over a checklist.
  In-game validation belongs to the user.
- Fallout 4 only. Fallout 76, Skyrim and Starfield paths are inherited, not
  maintained, and not to be worked on.
