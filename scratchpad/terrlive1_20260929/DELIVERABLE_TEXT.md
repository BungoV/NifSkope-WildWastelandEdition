# TERRLIVE1 deliverable text (2026-09-29 08:13) -- lines for the overseer to splice

## HANDOFF
- TERRLIVE1 (branch terrlive1-20260929, not merged).
  - Three terrain options:
    - `--terrain-option full|hybrid|dynamic`, and the same in the panel (label + control). HYBRID is the default.
    - The .lodb records the option.
    - Every bake also writes projected decals (.lodd + .lodg, additive; no existing format touched).
  - FULL is byte-identical to today's bake on Boston: 232/232 files, 213 sha1-equal, 19 equal after named
    environment masks. The sabotage runs go red.
  - HYBRID writes no VT.2/VT.4:
    - the shipped recipe saves 62.9 MB on Boston (12%) and no time
    - the pyramid-only recipe saves 36.7 s (4.3%)
    - the whole-map .lodt is 0.75 GB against the staged FULL's 15.2 GB
  - Preview (`lodgen --terrain-preview`), total GPU ms:
    - Boston: 0.065 / 0.097 / 0.129
    - street: 0.042 / 0.176 / 0.187
    - whole map: 0.394 / 0.403 / 0.564
  - Crossover (baked 64 u level within 4/255 of FULL):
    - street view: from 6,144 u; fade set to 8,192-12,288
    - oblique view: 34,816 u (plateau 4-5.4)
  - Owed:
    - bungo's eye on pics/*.png
    - live-splat colour work (tint/erosion/fill; the live splat is 7-16/255 from FULL)
    - box culling at eye level
    - dynamic's missing .btr chunk sheets
    - AO choice: (a) a 32 u map is 4-7/255 from the 16 u reference, at 1.6 MB for Boston and 403 MB for the
      whole map
    - FO4CS readers
    - TILING6 = `ltexFetch()` in terrainpreview.cpp
    - LTEX 000464c5 has no texture path

## WW_CHANGES
- LOD terrain options (lane TERRLIVE1, branch terrlive1-20260929):
  - `--terrain-option full|hybrid|dynamic` and a Terrain row in the LOD panel.
  - HYBRID drops the two finest texture levels (16/32 u) and keeps 64/128/256 u; near and mid distance are
    drawn live from the .lodl.
  - DYNAMIC writes no terrain textures.
  - Every bake writes projected decals (.lodd/.lodg).
  - `--decal-check` reads them back.
  - `--terrain-preview <spec.json>` renders and times the three options offscreen.

## MISTAKES
- 2026-09-29 TERRLIVE1: an ad-hoc preview run with a relative spec path failed, and its turn was not
  released: held idle 07:52-08:02, blocking IDENT2.
  - Fix: every launch goes through a wrapper that releases on every exit (pv.sh).
  - The preview now resolves spec paths against the spec.
- 2026-09-29 TERRLIVE1: build 1's HYBRID still wrote VT.2/VT.4 whenever the .btr chunk sheets were on, i.e.
  in the shipped recipe. It saved nothing, and I found that only from the byte table.
  - Fix: stage those levels without writing them (build 2).
  - Lesson: check an option's saving in the recipe that ships, not the bare one.
- 2026-09-29 TERRLIVE1: timestamps typed ahead of the clock three times in DONE.md (05:17/05:16,
  06:01/05:59, 05:40/05:39). Corrected each time.
  - Rule: run `date` in the same command that writes the line.
- 2026-09-29 TERRLIVE1: a Bash heredoc turned the Python regex `\b` into a backspace. The gate's
  seconds mask then matched nothing and the gate went red for the wrong reason.
  - Fix: chr(92) plus `assert chr(8) not in source`.
- 2026-09-29 TERRLIVE1: the preview mapped an empty .lodl base slot (0xFFFF) to grey instead of the engine
  default land set, and the first live-splat pictures had big grey patches.
  - Caught from the picture, then measured: 20-28 fell to 6-10/255 per bin once fixed.
