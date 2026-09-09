# LANE TERRAINFIX -- the terrain pyramid defects, Far Harbor, water fields

Header: tree E:\Projects\NifskopeWildWastelandEdition, main, NO commits. Read
CONSTITUTION.md, HANDOFF.md top block (the 2026-09-09 blocks: the ease fix in
heightAt, the VT path's two defects, the pending gate), WW_CHANGES.md 2026-09-07
(up-in-green, nearest->bilinear), docs/LODGEN_TERRAIN_VT.md, docs/LODGEN_BTD_FORMAT.md,
scratchpad/lane_lattice_report.md section 3 (the quintic ease). Skills via the
Skill tool: `nifskope-ww-lodgen`, `nifskope-ww-build-verify`,
`ww-control-calibration` (for every "as good as vanilla" number),
`ww-artefact-localise`. Model Opus 5. Files you own: src/lodgen.cpp,
src/lodtfile.cpp, src/lodtfile.h, tests/spells/lodgen_terrain.sh,
tests/spells/lodt_write.sh, docs/LODGEN_TERRAIN_VT.md, docs/LODGEN_BTD_FORMAT.md,
WW_CHANGES.md, MISTAKES.md. No other file.

## The work
1. `lodgenBakeVtTile` (~src/lodgen.cpp:6104, the .lodv pyramid path): fix BOTH
   2026-09-07 defects the .btr path already has fixed -- nearest sampling ->
   bilinear THROUGH the quintic ease that heightAt now uses (share the code,
   one home), and up-in-BLUE -> up-in-GREEN. Gate: re-encode vanilla's own
   sheets through the VT writer and diff (the 2026-09-07 method), plus the
   grid-phase roughness statistic from lane LATTICE on one VT tile vs vanilla:
   controls per ww-control-calibration, numbers in the report.
2. Far Harbor `.lodt`: 62 of 20,207,616 texels differ from the DDS heightmap
   (FO4CS's four-worldspace check). Find which texels and why (a parent-world
   walk edge? a cell with two LAND records?); fix in the writer or document
   it as a DDS defect with the proof. Gate: the four-worldspace diff, run with
   `--verify-only` where it applies; Commonwealth stays byte-identical.
3. Water height and type in the `.lodt`: the planes exist (waterheight,
   watertype); check they carry what FO4CS's LODT1 list asked for (the cell's
   water height and the WATR form / type per cell, including worldspace default
   water where the cell has none). Fill any gap; document the plane semantics in
   LODGEN_BTD_FORMAT.md with the byte layout and a version bump of the header if
   the layout changes (bump = a new gate case in lodt_write.sh).
4. Run the PENDING gate tests/spells/lodgen_terrain.sh from the ease fix.

## Build rule
Write all code first. Before the FIRST build, check ONCE whether
scratchpad/images_20260909/DONE exists (a picture lane holds the exe). If it does
not exist, or Fallout4.exe is up (`tasklist | grep -i Fallout4; echo rc=$?` must
print rc=1), write the report and END BUILD PENDING with a paste-able resume in
scratchpad/terrainfix_20260909/PENDING.md. Never poll. Build only through the
build-verify skill; LF-only files, Python byte counts before and after;
numstat small.

## Report
scratchpad/lane_terrain_fix_report.md, incremental: 1. VT fix + numbers. 2. Far
Harbor cause + numbers. 3. Water planes. 4. Gates run / skipped and why, exe vs
source mtimes. 5. Mistakes (also MISTAKES.md). 6. Finished-work skill review.
Final message under 25 lines, numbers in a table, BUILD PENDING first if so.
