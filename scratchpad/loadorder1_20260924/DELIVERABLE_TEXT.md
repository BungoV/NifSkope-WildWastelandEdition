## HANDOFF text
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

## WW_CHANGES text
- LOD generation reads a Mod Organizer 2 profile straight off disk. There is no need to launch NifSkope from MO2.
  - Command line: `lodgen --mo2-profile <profile folder>` (optionally `--mo2-mods <mods folder>`). It replaces the plugin list and the --resource lines.
  - Panel: Source > "Mod Organizer 2 profile", with Profile and Mods folder rows and a read-only Mod order list.
  - Plugins resolve to the mod folder that supplies them.
  - A plugin found nowhere is refused by name.
  - The bake record lists the resolved paths.
- `--plugins-txt` now keeps Fallout4.esm and the DLC masters, or refuses by name.

## MISTAKES text
- 2026-09-24 LOADORDER1: I added a third item to the LOD panel's Source box without searching the self-test for checks on that box. The structural check `source->count() == 2` failed on the first panel run and cost a rebuild. Rule: before adding an item to an existing selector, grep WW_LODGEN_TEST for the widget's objectName and update its structural count in the same patch.
- 2026-09-24 LOADORDER1: the harness compared the GUI list with `lodgen --mo2-profile <copied profile> --print-source` and got 0 plugins. A copied profile has no ModOrganizer.ini beside it, so Data could not be found. Rule: a copied or fixture profile always passes `--data-root`.

## Skill review
- Loaded: nifskope-ww-lodgen, nifskope-ww-build-verify, nifskope-ww-worktree-build, nifskope-ww-panel-style, mo2-mod-content-census, search-lean.
- Wished for: a list of the panel self-test's structural counts, the ones that break when a control is added.
- Written or extended:
  - mo2-mod-content-census: --mo2-profile, and the TestWorldspace FF form-ID trap.
  - nifskope-ww-lodgen: --mo2-profile, the panel's profile source, --data-root for a copied profile, the doubled FO4CSLOD path, and that the panel writes no .lodb.
