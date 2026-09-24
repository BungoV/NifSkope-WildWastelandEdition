# Lane PBRR1 -- PBR renderer stage R1: detect + load (BUILD lane)

Director brief, 2026-09-24 02:2x. Model: Opus 5.5. Folder: scratchpad/pbrr1_<date>/ ; progress.md INCREMENTALLY.

## Read first
scratchpad/brief_pbrchain.md (chain rules), docs/NIFSKOPE_PBR_RENDERER.md: s2 (detection), the stage table row R1,
and the RULINGS section at the end (it overrides the doc where they differ). PBRR0's lane text:
scratchpad/pbrr0_20260924/DELIVERABLE_TEXT.md (harness usage, fixtures, census format).

## Scope (the R1 row, with the rulings applied)
- v6 reader in src/io/pbrmfile.cpp: envelopes 4, 5 and 6, the F0 law by envelope (v6: min(weight x iorF0, cap) x tint,
  Pattern B sparse slot values; match PBRMaterialEditorQt/docs/PBRM-v6.md and the FO4CS wave 88 reader).
- ONE shared candidate function (viewport + lodgen) resolving, in order: material swap > .nifx (<nifstem>.nifx BESIDE
  the .nif only; node-keyed, case-insensitive; `material` entry) > (DIRECT .pbrm name in the shader property | same-name
  sibling of the BGSM/BGEM -- mutually exclusive by the property name) > FO76 BGSM > legacy. Direct links are
  SUPPORTED (bungo overruled the refusal).
- .nifx parser/writer with the `material` section; unknown sections and key order survive a round trip byte-identical.
- A texture load failure aborts the PBR binding (falls back to legacy, census names the reason).
- Enable the PBR path (the glproperty.cpp:1005 constant / greyed menu entries) BUT the DISPLAY DEFAULT stays Legacy
  until R3 passes (Q9): the user-facing mode defaults to Legacy; the harness forces PBR via WW_PBRM_MODE.
- WW_PBRM_CENSUS lines now report the real route (swap/nifx/direct/sibling/fo76/legacy), path, envelope, refusal.
- Debug overlay: a "PBR route" view tinting shapes by route (a row in the new Scene popup window is fine if you create
  it; otherwise a View-menu entry, and say which).

## Gates (the R1 row, gate (e) REPLACED by the ruling)
(a) coverage: the PBR duct fixture in PBR mode covers >= 99% of the pixels it covers in Legacy;
(b) route census == an independent Python resolver over the fixture set; swapping two precedence steps FAILS;
(c) v5 vs v6 F0: v6 weight 1 ior 1.5 -> 0.040, v5 f0 0.04 -> 0.040; the v6 file read through the v5 law -> 1.0 (red);
(d) .nifx round trip with an unknown section + odd key order is byte-identical;
(e) a direct-.pbrm NIF: census route reads `direct` and it renders PBR; swapping direct/nifx precedence FAILS (b);
(f) the standing zero set (pbr_shade_ab.sh) PASSES with the mode at its default (Legacy): nothing legacy moves.
Particles: use ShockHAndLeft + CryoJet01 as the visible-particle cases (fire/smoke/mist render blank; bungo's
acceptance is owed, keep them in the set).

## Also
- Write the skill PBRR0 recommended (its DELIVERABLE_TEXT.md "Skill review"): .claude/skills/<name>/SKILL.md for
  running pbr_shade_ab (arms, pins, noise bar, red control, verdict line); plus the note for the render-shot skill.
- Test PBR data in a loose test folder only (never the vanilla tree); make v5 + v6 twins, a .nifx case, a direct-link
  case, a swap case, an FO76 BGSM case.
