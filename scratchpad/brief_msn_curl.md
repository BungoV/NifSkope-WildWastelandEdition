# LANE MSN -- is vanilla's terrain `_msn` detail integrable?

Header: tree E:\Projects\NifskopeWildWastelandEdition, branch main, uncommitted
tree ("Not yet" from bungo: NO commits). Read CONSTITUTION.md first, then the top
block of HANDOFF.md ("THE OPEN THREAD"), then WW_CHANGES.md entry 2026-09-07.
Model: Opus 5. Files you may write: only under scratchpad/mountains_20260907/
and your report. Never touch src/. Never run a build. Check Fallout4.exe is not
running before executing release/NifSkope.exe (tasklist); if it is up, stop and
say so in the report.

bungo's words on the thread: "Still blurry, vanilla is superior in quality."

## The work
1. Find the curl-free test that was written and never run. It is somewhere in
   scratchpad/mountains_20260907/ (msn_compare.py builds the vanilla/ours
   `_msn` pair; look for the script that computes d(nx/nz)/dy - d(ny/nz)/dx on
   the high-frequency residual). If it does not exist as a runnable script,
   write it there as msn_curl.py. Inputs regenerate from Fallout4.esm and the
   scripts beside them; the vanilla sheets come from the FO4 corpus
   (E:\Tools\Fallout 4\DataUnpacked\Data, never a mod folder).
2. Controls FIRST, before the vanilla verdict:
   - Positive control: OUR OWN bilinear `_msn` (derived from the VHGT heightfield,
     so integrable by construction). Its curl residual is the floor.
   - Negative control: a field that cannot be integrable, e.g. the vanilla
     residual with its x and y components swapped, or a rotated copy. Its curl
     residual is the ceiling.
   - Report both numbers. If the test does not separate them, the test is
     broken and the vanilla verdict is not to be reported.
3. Then vanilla's high-frequency residual (vanilla `_msn` minus the same sheet
   low-passed at our detail scale), on at least three Commonwealth far tiles at
   the same size and mip as before, one of them the tile from the 6.6-8.2x
   measurement. Place each between floor and ceiling.
4. Verdict, in plain words, with the refuter: integrable (came from a finer
   heightfield we do not ship -> reuse vanilla's sheets where terrain is
   unchanged) or not integrable (composited landscape material normal maps ->
   reproducible). If the answer is "mixed", say what fraction and by what rule.
5. If NOT integrable and time allows: one probe of which LTEX normal maps it
   could be, by correlating the residual against the material normal textures
   of the layers painted on that tile (lodgen --dump-layers). Optional.

## Gates
- Controls separated by at least 5x (ceiling/floor). Numbers in the report.
- Every number has the tile, size and mip beside it.
- All scripts saved under scratchpad/mountains_20260907/ and named in the report.

## Rules
- No src/ edits, no builds, no commits, no writes outside the two paths above.
- Skills to invoke: `nifskope-ww-lodgen` (the CLI and its traps, --dump-land /
  --dump-layers), `nif` only if a mesh is touched. Read them before choosing
  commands.

## Report
Write scratchpad/lane_msn_report.md incrementally, section by section:
1. Where the test was found / what was written. 2. Controls with numbers.
3. Vanilla per tile. 4. Verdict + refuter. 5. Mistakes (or "none").
6. Finished-work skill review: skills used, procedures re-derived that should
   be a skill, ones written, or the reason for declining.
Final message to the director: under 30 lines, plain language, numbers in a
table.
