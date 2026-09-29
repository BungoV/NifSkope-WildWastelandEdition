# TERRLIVE1 deliverable text (2026-09-29 08:13, rework 09:30, rule paint 12:05) -- lines for the overseer to splice

## HANDOFF
- TERRLIVE1 (branch terrlive1-20260929, not merged). Rework 2026-09-29 (coordinator 08:19 + correction):
  - Two terrain options: `--terrain-option hybrid|dynamic` and the panel's Terrain row. HYBRID is the default.
    FULL is ditched (CLI, panel, help); its byte gate is retired and its skill marked HISTORICAL.
  - Every bake writes projected decals (.lodd + .lodg, additive; no existing format touched). The .lodb records
    the option.
  - The blend to vanilla (law 2), for bungo's "square steps" and "dirt outline":
    - cause, measured: the old fill was painted per cell, so empty quadrants of edge cells and the first 2-4 km
      outside kept the engine-default ground (lum ~67 against vanilla's ~80), cut on cell/quadrant lines;
    - now: painted per quadrant; outside = vanilla's own dim-4 LOD diffuse, untouched; inside, ours rises over an
      8,192 u per-texel band (smoothstep); one constant (LODGEN_VT_FILL_BAND) for the bake and the live preview;
    - gate (edge/gate_law2.py): outside |ours - vanilla| 1.39 / 1.48 lum, outline dip 0.00 / 0.27 (north / west)
      -> PASS; the old stage bake reads 6.61 / 6.45 and 10.55 / 9.53 -> FAIL.
  - Whole map HYBRID: 913.8 MB (.lodt 749.5 MB + decals 164.3 MB), bake 1,332 s. 577 MB of the .lodt sits on
    tiles that are all vanilla now; dropping them needs a reader contract (bungo's call).
  - DYNAMIC: decals 164.3 MB shipped + vanilla's sheets read live (whole map 2,304 sheets, 805.7 MB, game files).
  - Preview total GPU ms (HYBRID / DYNAMIC): Boston 0.107 / 0.146, street 0.184 / 0.195, whole 0.405 / 0.534.
  - Crossover on ground wholly ours: oblique 30,720 u; street not measurable (too little wholly-ours ground past
    6 km). Preview fade unchanged at 8,192-12,288 u.
  - Owed:
    - bungo's eye on pics2/*.png
    - FO4CS readers
    - the "draw vanilla here" reader contract for the 577 MB of all-vanilla tiles
    - live-splat colour work (live 3-5 lum darker than baked deep inside; the dark blocky patches)
    - box culling at eye level
    - a black L-shaped line in the west close-up, present before and after (not chased)
    - 2 quadrants painted only by a NULL LTEX (bake 15,893 vs .lodl 15,891)
    - AO choice; TILING6 = `ltexFetch()` in terrainpreview.cpp; LTEX 000464c5 has no texture path

  - Optional rule paint outside (2026-09-29 10:00-12:05, bungo "option 2, make it optional"):
    - `--outside-paint vanilla|rule` + the panel row "Outside paint"; ships vanilla (= law 2, byte for byte: all 7
      whole-map files sha1-identical to the law-2 bake; the gate goes red on the rule bake).
    - On: a new file `<ws>.lodr` (2.6 MB, 512 u samples, 2 textures a sample from the worldspace's own 36 LTEX,
      chosen by slope, height and the best colour match to vanilla's dim-4 diffuse); HYBRID's far levels carry it,
      DYNAMIC draws it live through ltexFetch. `--rule-check` reads it back. The .lodb records `outside rule`.
    - Cost: +214 s HYBRID bake (+38 s DYNAMIC), GPU up to +0.06 ms whole map live, +0.11 ms on a low view over
      outside ground. Edge gate with the rule clauses PASS (dip 0.00 / 0.54, step 0.78 / 0.10); law-1 FAIL.
    - Drift from vanilla outside: mean 5.1 lum (p95 21); far hills lose some of vanilla's grey rock to brown/olive.
    - Owed: bungo's eye on pics3/*.png; FO4CS readers for the .lodr; whether the rule should pick rock more often.
## WW_CHANGES
- LOD terrain options (lane TERRLIVE1, branch terrlive1-20260929):
  - `--terrain-option hybrid|dynamic` and a Terrain row in the LOD panel. HYBRID (default) keeps the 64/128/256 u
    texture levels; near and mid distance are drawn live from the .lodl. DYNAMIC writes no terrain textures.
  - The painted ground now blends into vanilla's own LOD diffuse over 8 km inside its edge, per texel; outside
    it, vanilla's colour is left untouched. No more dark outline or square steps at the painted edge.
  - Every bake writes projected decals (.lodd/.lodg); `--decal-check` reads them back.
  - `--terrain-preview <spec.json>` renders and times the options offscreen, with the same blend to vanilla.

  - `--outside-paint vanilla|rule` and an "Outside paint" row in the LOD panel: optionally paint the ground outside
    our painted area with the worldspace's own landscape textures (chosen by slope, height and vanilla's colour),
    stored in a new `<ws>.lodr`; `--rule-check` reads it back. Off by default.
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
  06:01/05:59, 05:40/05:39, and 12:10/12:04 in this file's header). Corrected each time.
  - Rule: run `date` in the same command that writes the line.
- 2026-09-29 TERRLIVE1: a Bash heredoc turned the Python regex `\b` into a backspace. The gate's
  seconds mask then matched nothing and the gate went red for the wrong reason.
  - Fix: chr(92) plus `assert chr(8) not in source`.
- 2026-09-29 TERRLIVE1: the preview mapped an empty .lodl base slot (0xFFFF) to grey instead of the engine
  default land set, and the first live-splat pictures had big grey patches.
  - Caught from the picture, then measured: 20-28 fell to 6-10/255 per bin once fixed.
- 2026-09-29 TERRLIVE1: I read the ground outside our painted area as invented colour. It was vanilla's own
  dim-4 LOD diffuse (--vt-fill-vanilla, census line in every bake). bungo had to say it.
  - Lesson: read the bake's own census line for what fills an area before describing it.
- 2026-09-29 TERRLIVE1: `bash chain4.sh &` inside a foreground call kept running, and I launched a second
  background run; two whole-map bakes raced for one folder. The kill of my own duplicate was refused.
  - No harm: the duplicate failed at once and released the turn. Rule: background only via run_in_background,
    never `&` in a foreground call.
- 2026-09-29 TERRLIVE1: my first "outline dip" metric read 0.13 / 0.44 on the old bake, i.e. it did not see
  the outline it was written for. Redefined (sink below both ends, net of vanilla's) and proven: old 10.55 /
  9.53 FAIL, new 0.00 / 0.27 PASS.
- 2026-09-29 TERRLIVE1: the rule patch anchored on `auto sampleLtex`, which matches two lambdas. My sed fallback
  then moved the wrong one (the chunk writer's) and printed nothing. Caught by the move check, reverted that one
  file, re-anchored at `static bool lodgenBakeVtTile(`.
  - Rule: an anchor must be asserted unique, and a fallback edit must assert too; never a bare sed.
- 2026-09-29 TERRLIVE1: again a Bash heredoc changed Python escapes (`\n` became real newlines), so the nifcli
  anchors did not match (it failed loudly, no harm). Patch scripts now go through the Write tool with raw strings.
- 2026-09-29 TERRLIVE1: a waiter grepped build6.log for "rc=", which the build script prints to its task output,
  not the log; it would never have ended. Stopped it. Wait on the line the log itself writes.
