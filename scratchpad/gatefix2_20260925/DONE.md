DONE -- GATEFIX2, 2026-09-25 04:2x. Branch gatefix2-20260925 (from b1cd5bc), worktree E:\Projects\NifskopeWWE-gatefix2.
Exe release/NifSkope.exe sha1 3a4d1e5dc7a7295885878271687b3486458600fc (= release/NifSkope.before_gatefix2.exe;
b1cd5bc unmodified, no source changed). Commits: bab6129 (the harness fix) and the scratchpad commit after it.

## 1. Skills loaded
ww-stale-gate-attribution (amended), nifskope-ww-worktree-build. nifskope-ww-lodgen was not needed (no bake flag
and no lodgen source touched).

## 2. What was built
No code changed. One build of b1cd5bc, from main's objects (main at 3489f4b, `make -n` 0 g++ lines; the three
NIFSKOPE_REVISION objects deleted first). Fixtures copied into this worktree with their mtimes (cp -a, gitignored):
showcase1_20260912/out/{look/mod,look/obj,lodl}, nativeview1_20260912/{resroot,work}.
- tests/spells/native_lighting.sh (bab6129): every window in its own `WW_SETTINGS_SCOPE=nativelighting`, wiped
  before each window and at exit, seeded with `Settings/Version=1`; optional `SEED_REG=<file.reg>` for red controls.

## 3. Gates, with the red runs
### Attribution: neither baseline moved; the harness read bungo's settings
- Red reproduced on 3a4d1e5d, old harness: 21 checks, 2 failures, legacy_btr_top/obl (nl_before.log). Top frame:
  93,893 pixels differ, mean luma 109.87 -> 132.89; the .BTR water shape is pure white instead of dark.
- Not an exe move. Under a COPY of his settings (reg export -> imported into a scope, his key never written):
  before_vt1 (09-16 11:54:47, 22,288,896 B -- the exe the baselines were measured on) and before_cellview4
  (23,625,728 B, the 09-19 21/0 build) give the SAME 93,893 / 33,933-pixel diffs as today's exe.
- Named by bisecting the settings copy: dropping `GLView/Display/Contributions` makes all four frames
  byte-identical. His Lighting-mode mask is `Contributions/2 = 0x00184b00` (default 0x03f84b80). Bit by bit: only
  0x80 (DoVertexColors, "Vertex Color") moves the picture; tint, vertex alpha and the rest do not.
  No landed change and no ruling moved these baselines; they are right as they stand.
- Second trap found on the way: an EMPTY scope is a first install (src/ui/settingsdialog.cpp: null
  Settings/Version -> save every pane's value). That writes Background 46,46,46 (src/ui/settingspane.cpp:440);
  the check's BG constant is the viewport skin colour, so every coverage mask breaks: 21 checks, 8 failures
  (nl_scoped1.log). Bisected to `Settings/Render/Colors` (bis_*.log). The seed fixes it.
### native_lighting.sh: RED -> GREEN
- Green: 21 checks, 0 failures, twice (nl_green1.log, nl_green2.log); scope key gone after exit.
- Red 1, his Contributions value seeded into the scope (SEED_REG=seed_contrib_only.reg): 21 checks, 2 failures,
  exactly the reported pair (nl_red_vertexcolour.log).
- Red 2, one flipped byte in a copy of legacy_btr_top.png: 21 checks, 1 failure (nl_red_badbase.log).
- Red 3, the old harness on the same exe: 21/2 (nl_before.log). Red 4, empty scope without the seed: 21/8.
### Kept green (exe 3a4d1e5d)
- lodgen_native: 32 checks, 0 failures (kg_lodgen_native.log).
- lodgen_native_baseline: 0 failures, PASS (kg_lodgen_native_baseline.log).
- lodgen_btofree: 30 checks, 0 failures (kg_lodgen_btofree.log), pin before_gatefix1 copied from main's release/.

## 4. Exe sha1 and commits
- 3a4d1e5dc7a7295885878271687b3486458600fc (release/NifSkope.exe == release/NifSkope.before_gatefix2.exe).
- bab6129 tests/spells/native_lighting: every window in its own seeded settings scope.
- scratchpad commit: this folder's text (DONE, DELIVERABLE_TEXT, progress, scripts). The logs (*.log are gitignored;
  they stay on disk in this folder), the .reg copies of his
  settings, the pictures and badbase/ stay untracked.

## 5. What the final bake needs
- Nothing from the generator: no defect was found. native_lighting needs the showcase1/nativeview1 fixtures
  (absent in a fresh worktree -> SKIP 77, never a pass).
- Possible product defect, NOT fixed (outside this lane): a fresh install saves Background 46,46,46, while the
  palette-v3 migration in src/nifskope_ui.cpp (~32709) removes that key so the viewport follows the skin colour.
  A new user's viewport is therefore off-skin until the palette migration runs again. Director's call.
- Eleven render spells still set no settings scope and so read bungo's settings: cell_open, cell_pick,
  impostor_draw, lod_channel_preview, lodgen_octahedral, lodi_v7, lodl_channels, lodl_open,
  refraction, render_shot, skeleton_overlay. Not changed here (not in the brief).
- For bungo, if he wants to know: his "Vertex Color" is unticked in the Lighting shading menu. That is his
  choice, not a bug; it only broke a gate that should never have read it.

## 6. Skill review
- ww-stale-gate-attribution: used; it fitted (its section 5 already said GUI gates read the profile). Amended
  with: test the baseline's own exe first; bisect a COPY of the settings imported into a scope; an empty scope is
  a first install, so seed Settings/Version; the list of unscoped render spells.
- nifskope-ww-worktree-build: worked as written.
- Missing skill wished for: none beyond the amendment. The .reg rewrite needs a Python FILE (heredocs lose the
  backslashes here) -- written into the amendment, not a separate page.
