DONE -- lane ESMFIX1, 2026-09-24, worktree E:\Projects\NifskopeWWE-esmfix1, branch esmfix1-20260924 (from 541bbe5). Not pushed, not merged.

# 1. Skills loaded
- Loaded: nifskope-ww-worktree-build.
- Read and amended without loading them through the Skill tool: nifskope-ww-lodgen, mo2-mod-content-census.
- Not needed: nifskope-ww-build-verify, render-shot and test-harness-add. There was no GUI run and no harness. The gated build is this lane's build.sh, written from the worktree skill.

# 2. What was built
- The only file changed is lib/libfo76utils/src/esmfile.cpp.
- For plugins with a form version below 0xC0 (Oblivion through Fallout 4), a form-ID file index at or beyond the plugin's master count now maps to the plugin itself.
  - This is the game's rule and xEdit's. In xEdit, `TwbFile.FileFileIDtoLoadOrderFileID` sends FullSlot < MasterCount to that master and anything else to the file's own slot.
  - The same map serves record headers, GRUP labels and every field reference (mapFormID).
  - The raw "invalid form ID" refusal is off for these plugins.
  - Fallout 76 and Starfield (form version 0xC0 and up) keep the old map and the old refusal.
- Measured cause:
  - TestWorldspace.esp is a plain .esp: flags 0, form version 131, HEDR 1.0 / 923 records / next object 10416.
  - It has 1 master (Fallout4.esm).
  - All 483 records carry top byte FF: WRLD 1, CELL 211, LAND 131, ACHR 121, REFR 13, and one each of TXST, LTEX, CLMT, CONT, NPC_ and STAT.
  - The game loads these records as the plugin's own.
- The game ignores none of these records, so no skip-with-warning path was needed.
- No format, flag or default changed.

# 3. Gates (details in scratchpad/esmfix1_20260924/gate/GATES.txt)
- **G1 PASS** on his live Default profile (47 plugins).
  - The reader loads all 47 (`--list-worldspaces`, rc 0).
  - A one-chunk Sanctuary bake runs (rc 0). Its .lodb lists all 47, including testworldspace.esp.
  - census.py: TestWorldspace is the ONLY plugin with top bytes beyond its master count.
  - RED: the rung refuses both runs with "TestWorldspace.esp: invalid form ID", rc 1.
- **G2 PASS.** WRLD raw FF000F99 "TestDebugWorld" is predicted as 1C000F99 (plugin 28), and the exe reads back 1c000f99. That worldspace loads 210 cells.
  - RED: the record is absent on the rung.
- **G3 PASS.** A 9-chunk Sanctuary bake on the 17 vanilla masters gives 57 files. 56 are byte-identical to the rung's.
  - The .lodb differs only in its path, exe, time and timing lines.
  - Sensitivity red: on the same chunk, his live load order and the vanilla-only order give different files, so the byte compare does catch load-order changes.
  - Kept green: lodgen_loadorder.sh 24/0, lodgen_resources.sh 4/0.

# 4. Exe sha1 and commits
- release/NifSkope.exe, 21:55:01, MZ, sha1 ec5959c41a43ef5236cec491300aabe594301707.
- Rung: release/NifSkope.before_esmfix1.exe, sha1 b4f62763b254ea930c2f1df568cf2c816d9eb7da.
- Commits: ab12bf3 (the fix), then the report commit.

# 5. What the final bake needs from this lane
- Merge esmfix1-20260924 (one file, esmfile.cpp) together with LOADORDER1's branch. TestWorldspace.esp no longer needs to be unticked.
- Bake with `--mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default"`, as LOADORDER1 said.
- Stale text left for the director, because it is outside my file list: tests/spells/lodgen_loadorder.sh G5 still unticks TestWorldspace.esp in its profile copy (its checker counts raw FF IDs), and its comment still says the reader refuses the plugin. The gate still passes.

# 6. Skill review
- Loaded: nifskope-ww-worktree-build.
- Amended:
  - nifskope-ww-worktree-build: new section 5b, copy objects from a sibling worktree at your exact commit rather than from main.
  - nifskope-ww-lodgen: `--mo2-profile --list-worldspaces` is the check that every plugin loads, because `--print-source` opens no plugin. Also the form-ID rule.
  - mo2-mod-content-census: the TestWorldspace trap is marked fixed, with the rule and the probe scripts.
- Wished for: nothing that cost time.
- Declined to write: "resolve a plugin form ID like the game". It is a one-line rule, now kept in mo2-mod-content-census and the esmfile.cpp comment, so it will not need working out again.
