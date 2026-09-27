# Night 2026-09-27 -- rules shared by every map-fix lane (read in full before starting)

## Context
bungo (asleep, gave the night to the overseer): "we fix all the maps that are broken, overnight" / "after you're
done with fixing bakes, merge it, then render the bakes for me, this time fixed" / "and the water maps will finally
bake on the water? hopefully" / "I leave you to do it all overnight".
The map review that found the faults: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\audit1\DONE.md
(section 2 = his points with evidence, section 3 = the full table, section 4 = the ranked faults). Read your rows.
The 79 current pictures: ...\scratchpad\maps1\full\NN_<key>.png; how they were made: ...\scratchpad\maps1\DONE.md.
Offline drawing with the viewer's camera: skill ww-lod-offline-picture (E:\Projects\Claude\.claude\skills).

All lanes branch from night-20260927 @ 8f58e7db = NifSkope main 422881d4 + AO2 (tower weld, see-through decal
casters) + GPU1 (GPU bake option, default on). The overseer merges the lanes into night-20260927, bakes the whole
Commonwealth once, renders every map, then merges into main. So: keep your change to your own area, and say in your
report every file and every format field you touched.

## Rules
- Read E:\Projects\ClaudeNifskope CONSTITUTION and the NifSkope HANDOFF top block (E:\Projects\NifskopeWildWastelandEdition\HANDOFF.md) first.
- Skills: nifskope-ww-lodgen, nifskope-ww-build-verify, nifskope-ww-render-shot, ww-module-off-is-identical,
  ww-channel-view-refuter, ww-legend-matches-picture, search-lean, ww-lod-offline-picture (repo tree <worktree>\.claude\skills
  and live tree E:\Projects\Claude\.claude\skills).
- Game gate before EVERY build, bake and render:
  `if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi`
- ONE build on the machine at a time: before `make`, check (Get-CimInstance Win32_Process) that no make/g++/cc1plus
  runs; if one does, wait; never kill anything you did not start. Other lanes build tonight too.
- ONE headless NifSkope at a time: E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\turn.sh acquire <lane> /
  release <lane> (read it first: every verb but `release` acquires). Pick an unused port per run, never a fixed base.
- The antivirus kills fresh exes run from a worktree: copy the built exe + DLLs to a run folder under YOUR worktree's
  scratchpad and run from there. If every launch dies in ~20 s with no log, wait 10 min and retry (seen 03:25-04:05).
- Any GUI on the second monitor (x=1920,0); never take focus; never kill his NifSkope or MO2; never kill by image name.
- Bakes: the Boston box (-8 -12 3 -1) for every test. No whole-map bake (the overseer runs the one whole bake).
  Delete your own bake output and sheet caches when done; E: disk is shared.
- Commit early and small, by explicit path only, never -a/-A, never git stash. No push, no merge, no install.
- Never write NifSkope main, its ledgers, anything under E:\Projects\Fallout 4 Mods\, FO4CS, or any CORE file.
- Public repo: no game data, textures, binaries, PDB names, pictures or bake output in git.
- Patch source and scripts through the Write/Edit tools only, never a heredoc or python -c.
- A tool or permission refusal: stop that step, record it, do not route around it.
- A format change: bump the version the way the doc's version history does, keep the reader refusing unknown bits,
  update the doc section, and list the change for the FO4CS reader in your DELIVERABLE_TEXT (FO4CS is not touched
  tonight; its reader is updated later by standing order).
- Every fix is ON by default when it passes its gates (no fix-only toggles). A switch to turn it off for the
  byte-identical gate is fine on the command line / env only.
- Deliverable text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES) into scratchpad\<lane>_20260927\DELIVERABLE_TEXT.md.

## Pictures
Each picture ONE file, FULL SIZE, a plain-words title burned into a 60 px bar at the top. NO side-by-side sheets,
no contact sheets (bungo: "I can't judge anything if you cram those small previews into that sheet you made").
Before and after = two files with the same camera (the maps1 Boston camera: view 8 ortho as in maps1\shot.sh).

## Report
Incremental (a crash must not lose work), scratchpad\<lane>_20260927\DONE.md in your worktree:
1. Skills loaded. 2. What was wrong (with the audit's number). 3. What you changed, file by file, and any format change.
4. Gates with numbers. 5. Commits. 6. Pictures (paths). 7. What is still not right.
8. Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote. A procedure worked
out from first principles is a missing skill -- write it under .claude\skills\<name>\SKILL.md in your worktree, or say
why it will not recur.
Final message: short, plain words, no invented names: what changed, the gate numbers, the picture paths.
