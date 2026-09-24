# PBRRENDER0: text for the overseer to splice (lane did not edit these files)

## HANDOFF (lane entry)

PBRRENDER0 (2026-09-23 22:25 to 23:0x): research and design only. No src edit, no build, no launch, no commit.

The deliverable is the new, uncommitted `docs/NIFSKOPE_PBR_RENDERER.md`.

**Headline findings:**
- A FO4 `.pbrm` never renders today, for three reasons:
  - the gate `pbrmFeatureEnabled = false` (`glproperty.cpp:1005`);
  - `pbrmfile.cpp:149` refuses envelope v6, and every current editor file is v6;
  - detection differs from the game.
- TBI §0 "empty frames" has a strong candidate cause, already fixed but never tested. Before 53fe028 (2026-08-03,
  `renderer.cpp:910` `mesh->setUniforms`), the PBR path never uploaded `modelViewMatrix` or `normalMatrix`, so every
  vertex collapsed. R1's first gate (coverage ≥ 99% of Legacy) decides it.
- **Detection = the game's rules.**
  - Order: swap (`_d.dds`) > `.nifx` `material` link (new, proposed) > same-name sibling (always on) > FO76 BGSM >
    legacy.
  - A direct `.pbrm` name is refused, as `PBRM.cpp:1368-1371` does.
  - `.nifx` gets an additive `material` section keyed by geometry node name, and stays version 1.
- **Stages:**
  - R0 harness → R1 detect/load/flip → R2a Studio lighting → R2b lookdev + weather W1 → R3 BRDF + v6 specular +
    energy split → R4 tint/emission.
  - Then R5 card bake (= IMPOSTORPBRM1's renderer half), S1-S4 shadows/contact/AO/SSGI, and W2-W5 weather.
  - L1 (legacy back to the vanilla Blinn-Phong law) runs when no R stage is mid-flight.
  - X1 is the `.nifx` editor; X2 (geometry `.nifx`) is an owed ruling.
- **18 questions for bungo in §14.** The top five: Q1 refuse direct links; Q2 sibling default ON; Q3 precedence;
  Q6 FO4CS energy split; Q9 PBR display default.
- **Owed outside this repo:**
  - FO4CS: a `.nifx` load seam, a `material` section parser and a precedence slot. Readers come last; not a blocker.
  - PBR Material Editor doc fixes: PBRM-v6.md:440-441 (direct-link sentence), :22 (envelope row), :384-385 (bits
    26/27 vs the runtime's 30/31).
  - Editor: write the `.nifx` `material` section; owes the `(1 - E_spec)` split.
- **Bug found, out of scope:** the lighting widget saves the light angles under one QSettings key and reads them
  under another (`lightingwidget.cpp:125-126` vs `:78-81`).
- **IMPOSTORPBRM1** needs R1 plus the material half of R3/R4. It does not need lighting.

## WW_CHANGES

### NifSkope PBR renderer: design and staged plan (lane PBRRENDER0, 2026-09-23)

- New `docs/NIFSKOPE_PBR_RENDERER.md` covers:
  - what renders today;
  - detection as the game does it, plus a proposed `.nifx` `material` section;
  - the shader plan, with the editor preview as the law and the FO4CS wave-88 energy split;
  - uniforms and texture slots;
  - a Studio lighting mode;
  - the FO4 lookdev stage and a WTHR weather picker from any loaded plugin;
  - a legacy spec/gloss audit against vanilla bytecode;
  - the particle constraint;
  - one old-vs-new shading harness;
  - shadows, contact shadows, AO and SSGI as toggles;
  - the card bake on the same PBR path;
  - later `.nifx` editing and geometry;
  - the stage plan with gates, and 18 questions.
- Nothing built. It supersedes the unbuilt "Material resolution" note in `RENDERER_MATCH_PLAN.md` §2.

## MISTAKES (root MISTAKES.md)

- **2026-09-23 PBRRENDER0: timestamps typed from feel.**
  - What happened: progress entries were stamped 22:3x-22:5x while the clock read 22:28.
  - Fix: corrected after reading `date`. Every later stamp came from `date` in the same turn.
  - Rule already exists (read the clock). Coordinator message stamps also ran ahead of the real clock (22:5x
    labels arrived at 22:44-22:54). Stamp from `date`, never from a message label.
- **2026-09-23 PBRRENDER0: TBI §0 went stale without anyone noticing.**
  - `TO_BE_IMPLEMENTED.md` §0 cites the gate at `glproperty.cpp:996`; it is now `:1005`.
  - It still reads as an open mystery, but 53fe028 (2026-08-03) added the missing `setUniforms` call on the PBR
    path, which is a strong candidate fix. Nobody linked the two because the gate was never flipped.
  - Rule: when a commit fixes a class of bug (here, per-program uniforms never uploaded), grep the TBI for open items
    of that class and note the candidate there.
- **2026-09-23 PBRRENDER0: the legacy "match the editor" work moved away from vanilla.**
  - `RENDERER_MATCH_PLAN.md` §1 (07-26) swapped `fo4_default.frag` from the vanilla-like `exp2(10g+1)` Phong with
    F0 0.2 to the editor's GGX.
  - The vanilla bytecode audit shows the old law was the game's.
  - Not a code error at the time (the goal then was "match the editor"). Rule: a legacy shading change cites the
    vanilla bytecode law it moves toward, not only the editor.

## Skill review

- **Candidate skill `pbr-shade-ab`:** the old-vs-new shading A/B procedure. It covers:
  - the frozen rung folder (exe + shaders);
  - the pinned `WW_*` set, including `WW_SETTINGS_SCOPE`;
  - the OLD-vs-OLD noise bar;
  - the zero/change/value gate kinds;
  - a red control per gate kind;
  - the verdict line format.

  Write it when R0 builds the harness, because the procedure is repeatable across every R/L/S/W stage.
- **Candidate skill `fo4-wthr-read`:**
  - NAM0 row/ToD layout and the form-version row counts;
  - DALC and IMSP→IMGS;
  - CLMT TNAM units and the ToD blend;
  - the independent decoder `scratchpad/pbrrender0_20260923/wthr_probe.py` as the gate oracle.

  Add it to `E:\Tools\AISkills` as well (standing rule).
- The vanilla shader disassembly route (`fxp_dis.py` over Shaders011.fxp with d3dcompiler_47) is worth one line in
  the existing FO4 shader RE skill, if one exists.
