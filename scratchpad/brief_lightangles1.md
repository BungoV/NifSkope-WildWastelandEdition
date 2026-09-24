# Lane LIGHTANGLES1 -- light angles not remembered across restarts (BUILD lane, small)

Director brief, 2026-09-24. Model: Opus 5.5. Folder: scratchpad/lightangles1_20260924/ ; progress.md INCREMENTALLY.
Chain rules: scratchpad/brief_pbrchain.md.

## The defect
src/ui/widgets/lightingwidget.cpp: the load reads "Lighting/Declination" and "Lighting/Planar Angle" (:78, :80) but
the save writes "Settings/Render/Lighting/Declination" / ".../Planar Angle" (:125, :126). The angles never come back
after a restart. Also check the load's `tmp % int(POS)`: whether the saved range (declination/planarAngle x POS/180)
can reach +-POS or beyond, where the modulo would fold a legal angle (e.g. 180 -> 0) -- fix if it can.

## Do
- Read and write ONE key family (the Settings/Render/Lighting/ one, matching the rest of the widget). If a value exists
  only under the old read key, it was never written by this code -- do not add a migration unless you find a writer.
- Search lean: src/ui/widgets/ first, then src/gl/ for other readers of declination/planarAngle (the R2a studio rig will
  build on these angles).

## Gates
(a) WW_* harness (isolated settings, second monitor): set declination + planar angle to non-zero values including the
    range ends, save, restart, read back: equal within one slider step. The same harness on the before_lightangles1
    rung FAILS (red control).
(b) pbr_shade_ab zero set PASSES at default settings (nothing else moves).
Rung before_lightangles1 from the exe PBRR1 leaves. DONE.md + DELIVERABLE_TEXT.md as in the chain rules.

## Also (small, from PBRR1)
- Add tests/fixtures/pbr_data/ to .gitignore (it holds vanilla + FO76 file copies; must never be committed). Measure .gitignore line endings with Python bytes first.
