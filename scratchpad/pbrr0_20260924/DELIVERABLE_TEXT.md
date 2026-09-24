# PBRR0 deliverable text (for the overseer to splice; this lane edits none of these files)

## HANDOFF text

PBRR0 (2026-09-24, lane PBRR0): the PBR renderer's rung-0 harness is built, NOT committed.
- Rung `release/before_pbrr0/` = exe 97d2b3a2 (VTNORMAL1) + shaders/ + runtime; it is the OLD arm.
- New exe `release/NifSkope.exe` 02:06:44 sha1 8485154d94d1d7909bb457810647f59ed8faa95c. No pixel change: every fixture is byte-identical to the rung (verdicts below).
- `tests/spells/pbr_shade_ab.sh` + `.py` + `_cases.txt` = s9's one old-vs-new harness. OLD three times per case (noise bar from a|b and a|c), NEW once; `--red shader|census` builds a sabotaged copy of NEW under `release/pbr_ab_red_<kind>/` and removes it.
- Pins added, all no-ops that only pin state: WW_PBRM_MODE, WW_PBRM_AUTOREPLACE (both still pass through the pbrmFeatureEnabled gate), WW_LIGHTING_MODE, WW_LOOKDEV*, WW_RENDER_SHADOWS/_CONTACT/_AO/_SSGI (echoed only), WW_RENDER_PARTICLES (shot hook; unset = the old forced-on).
- WW_PBRM_CENSUS=<absolute path>: header echoes every pin asked-and-served; one row per shape `shape kind prog material route path envelope refusal`, prog = the program actually bound. In R0 every row is route=legacy with "pbr display off: pbrmFeatureEnabled=false (R0, detection unchanged)" plus the resolver's own finding. Particle rows come from the shot hook at the grab (s8 forbids glparticles.cpp edits) and read prog="particles.prog" or "(not drawn: <why>)".
- The s9 particle fixtures draw NOTHING in this viewer, measured: MPSFireSmall01 / MPSSmokeFireMed01 are fed by BSPSysMultiTargetEmitterCtlr, which `src/gl/controllers.cpp:1232` (exact name match on NiPSysEmitterCtlr) never simulates, so 0 live particles at any time; AttachFXMist01's two systems draw but reach no pixel at t=0.5..6, any camera, flat or not. They are `@empty` cases (PASS-EMPTY, which guards nothing and FAILS if they ever draw). ShockHAndLeft.nif and CryoJet01.nif were added as the particle fixtures that do draw. Owed ruling for s9/s13: accept those two as the particle zero set, or pick others.

## WW_CHANGES text

**PBR renderer rung 0: the old-vs-new shading harness (lane PBRR0, 2026-09-24).** `tests/spells/pbr_shade_ab.sh` renders every fixture with the previous build (a frozen folder, `release/before_pbrr0`) three times and with the new build once, with every setting pinned the same in both, and says per fixture whether the new picture differs from the old by more than the old differs from itself. New test switches that only hold settings still: `WW_PBRM_MODE`, `WW_PBRM_AUTOREPLACE`, `WW_LIGHTING_MODE`, `WW_LOOKDEV*`, `WW_RENDER_PARTICLES`, `WW_RENDER_SHADOWS/_CONTACT/_AO/_SSGI`. `WW_PBRM_CENSUS=<file>` writes one line per drawn shape saying which material route served it and why PBR did not; today every shape says legacy, because PBR display is still switched off in code. No picture changes.

## MISTAKES text

- 2026-09-24 lane PBRR0: the harness's settings scope is WRITTEN by every run (game paths, view state), so on the first dry run the first run of a fresh scope framed 1280x826 and every later one 1280x824 -- the arm that ran first differed from the other by the machine's history, not the code. Fix: delete the scope key before EVERY run (`pbr_shade_ab.sh` run_one). Rule: a paired harness resets shared state per run, not per session.
- 2026-09-24 lane PBRR0: first wrote the particle census line INTO `glparticles.cpp`, which s8 of the PBR doc forbids for every R/L/W stage; caught on reading s8 while diagnosing the empty particle frames, reverted to HEAD, census moved to the shot hook. Rule: read the stage doc's "no edits to" list before choosing a hook site.
- 2026-09-24 lane PBRR0: the doc's three particle fixtures were chosen without rendering them here; two never emit (multi-target emitter unsimulated) and one reaches no pixel, so a zero gate on them passes on two blank frames. Rule: a fixture enters a pixel gate only after a render of it clears a content floor.

- 2026-09-24 lane PBRR0: the first shader red control scaled output by 0.9 and the BGEM glow plane stayed byte-identical -- its pixels are over-bright (>1.11) and clamp back to 1. A red control must be proven to reach EVERY case it is aimed at; the sabotage is now clamp(0,1) then *0.9, and the judge reports the aimed set by name.

## Skill review (CONSTITUTION 1a)

- New skill owed: `nifskope-ww-pbr-shade-ab` -- how to run the rung A/B harness (make the rung folder with shaders + runtime, OLD x3 / NEW x1, per-run scope reset, @empty marker, the two red controls and what each must show). Text is ready from this lane's progress.md; not written here because skills live outside the repo tree this lane owns -- overseer to create it.
- `nifskope-ww-render-shot`: add one line -- the MPS*/multi-target particle NIFs render empty in this viewer (BSPSysMultiTargetEmitterCtlr not simulated); use ShockHAndLeft.nif or CryoJet01.nif when a picture must contain particles.
