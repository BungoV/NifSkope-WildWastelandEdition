DONE -- lane LOADORDER1 (LOD-A), 2026-09-24, worktree E:\Projects\NifskopeWWE-loadorder1, branch loadorder1-20260924. Not pushed, not merged.

# 1. Skills loaded
nifskope-ww-lodgen, nifskope-ww-build-verify, nifskope-ww-worktree-build, nifskope-ww-panel-style, mo2-mod-content-census, search-lean.

# 2. What was built
- `lodgen --mo2-profile <profile> [--mo2-mods <dir>]` (src/lodgenloadorder.{h,cpp}, nifcli.cpp). It reads modlist.txt and plugins.txt off disk, with no Mod Organizer and no usvfs.
  - Plugins come out as FULL PATHS: Fallout4.esm, then the DLC/CC masters, then the `*` lines. Each plugin is looked for in overwrite, then the enabled mods top-down, then Data. A plugin found nowhere is refused by name.
  - The resource stack is Data, then the enabled mods bottom-up, then overwrite.
  - The .lodb bake record carries the resolved paths.
  - `--print-source` prints the plugin list and the stack.
- `--plugins-txt` keeps the masters, or refuses by name.
- The panel (src/lodgenmanager.cpp) has a third Source choice, "Mod Organizer 2 profile".
  - It adds Profile and Mods folder rows, and a read-only "Mod order" list. The plugin list shows the resolved full paths.
  - The status line reads "N plugins (M from mod folders), E mods enabled, D disabled, files from <Data>".
  - A bad profile is refused by name and Generate refuses with it.
  - All of it is in house style.
- The docs name both switches: LODGEN_BAKE_RECORD.md and the LODGEN_LEDGER_FORMAT.md skip list.
- Two spells:
  - tests/spells/lodgen_loadorder.sh (G1-G5) plus its checker, which re-derives the load order on its own.
  - tests/spells/lodgen_panel_mo2.sh, which drives the self-test leg WW_LODGEN_MO2DISK.

# 3. Gates (numbers; every red shown on the rung)
- **G1-G5 on the final exe: 24 checks, 0 failures** (gate_final.txt).
  - G1: the list and stack equal the independent re-derivation, path for path.
  - G2/G3: probes; a swapped modlist flips the winner.
  - G4: no disabled mod is in the stack.
  - G5: a bake record with 46 plugins, BNS Trees.esp and TrueGrass.esp by mod path.
  - Reds: the rung refuses `--mo2-profile`; on --plugins-txt the rung gives `error opening input file "HUDFramework.esm"`.
- **Panel: 4 checks, 0 failures** (gate_panel.txt).
  - The MO2DISK leg is 14/0.
  - The whole WW_LODGEN_TEST suite is 142/0.
  - The GUI plugin list equals the CLI list, path for path (46).
  - RED: the rung has 0 of the floor of 14 (its suite is 128/0).
  - The GUI bake took 3.8 s and wrote 19 files, 0 .BTO/.BTR, with no scratch folder left.
- Kept green:
  - lodgen_resources: 4/0.
  - lodgen_bakerec (f): PASS against this lane's rung, 12 identical. It FAILS against the old before_bakerec1 rung; that drift is pre-existing.
  - lodgen_defaults: 31/1. The (d) C-line floor fails the same way on the rung, so it is pre-existing.
- The screenshot gate/panel_panel_mo2.png was viewed.

# 4. Exe sha1 and commits
- Final: release/NifSkope.exe, 21:38:51, MZ, **5ff25b1bcfdcc88c4ecd1401df2ea0bfa204ecb4**.
- Earlier builds: 4b9a5977 (CLI), c7a96c2f (first panel build).
- Rung: dadd253ade075582ed1610e5677c476ec0c07a4c.
- Commits: 27e5dfc, 91f6fce, 5ab1b5c, d050be2, 45dc72f.

# 5. What the final bake needs
- **BLOCKER:** his live profile does not bake. `TestWorldspace.esp: invalid form ID`: mod "AnotherOne's Test World" has 483 of 484 records under top byte FF, with 1 master. libfo76utils esmfile.cpp:309 refuses them before the master remap. Two ways past it:
  - (a) untick TestWorldspace.esp in MO2, or
  - (b) a follow-up lane fixes esmfile.cpp: an index beyond the masters = the plugin itself.
  Refuter for (b): an xEdit check of that plugin.
- Panel settings:
  - Source = "Mod Organizer 2 profile".
  - Profile = E:/Projects/Fallout 4 Mods/profiles/Default. This is prefilled.
  - Target = FO4CS.
  - Output = E:\Projects\Fallout 4 Mods\mods\FO4CSLOD.
- On disk that gives mods\FO4CSLOD\FO4CSLOD\Commonwealth, which the game sees as Data\FO4CSLOD\Commonwealth. The doubled name is correct, not a bug.
- The FO4CS target writes only the FO4CS set, so R2 holds with no new control.
- The panel writes no .lodb. For the record, bake from the CLI with `--mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default"` in place of the plugin list and the --resource lines.

# 6. Skill review
- Loaded: the six above.
- Wished for: a skill that names the panel self-test's STRUCTURAL counts that break when a control is added. The source-count check cost a rebuild.
- Written or extended:
  - mo2-mod-content-census: --mo2-profile, and the TestWorldspace FF form-ID trap.
  - nifskope-ww-lodgen: the --mo2-profile line, the panel's third source, `--data-root` for a copied profile, the doubled FO4CSLOD path, and that the panel writes no .lodb.
