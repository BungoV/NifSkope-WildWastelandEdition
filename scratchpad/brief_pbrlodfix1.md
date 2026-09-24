# Lane PBRLODFIX1 -- terrain PBR bake harness fails 5 of 14 (BUILD lane if a src fix is needed)

Director brief, 2026-09-24 03:5x. Model: Opus 5.5. Folder: scratchpad/pbrlodfix1_20260924/ ; progress.md INCREMENTALLY.
Chain rules: scratchpad/brief_pbrchain.md. Skills first: .claude/skills/nifskope-ww-lodgen/SKILL.md.

## The finding
tests/spells/lodgen_terrain_pbrm.sh (+ lodgen_terrain_pbrm_fixture.py) fails 5 of 14 checks, output IDENTICAL on
the PBRR0 rung (8485154d) and PBRR1 (ae101325) -> it predates tonight's PBR lanes. Current exe e5320fdd.

## Do
1. Run it on the current exe; list the 5 failing checks with their expected vs got.
2. Find WHEN it broke: bisect over the release/before_* rungs (newest first; VTNORMAL1 changed terrain normals and the
   msn folder resolution on 2026-09-23/24 -- prime suspect, rung release/before_vtnormal1 if it exists) and git log of
   src/lodgen*.cpp. A harness that went stale (its expectations no longer match an intended, ruled change) is fixed in
   the harness, with the ruling cited; a code regression is fixed in src. Say which, with evidence.
3. The harness must isolate settings (never inherit his profile's msnCache = his Upscaled Terrain Normals mod folder --
   if the failures come from the harness reading his profile, that IS the defect: force the state it measures).
4. Gates: the harness 14/14 PASS; a red control (revert your fix) fails the same checks; if src changed, the
   pbr_shade_ab zero set PASSES (skill nifskope-ww-pbr-shade-ab) and the lodgen harnesses the change reaches pass.
Rung before_pbrlodfix1 from e5320fdd if you build. DONE.md + DELIVERABLE_TEXT.md as in the chain rules.
