# PBR overnight chain -- common rules for every stage lane (director, 2026-09-24 01:3x)

bungo's rulings (HANDOFF "RULED 00:5x", "ADDED 01:0x/01:1x", "UI RULED 01:2x" + the 01:3x window correction):
"PBR in nifskope, only stop when it fully works, I can have a basic scene with sky, sun, etc."

## Order (one build lane at a time in this shared tree)
PBRR0 harness -> R1 detect/load (all THREE routes: sibling, DIRECT .pbrm name, .nifx beside the .nif) ->
LIGHTANGLES1 (settings key mismatch lightingwidget.cpp:78 vs :125) -> R2a studio lighting -> R2b lookdev + W1 ->
R3 BRDF + v6 specular (PBR display flips ON when R3 passes) -> R4 tint + emission -> WEATHER PREVIEW (any WTHR from the
loaded plugins, sky colours, CLOUD layers, hour, the MOON with phase + moonlight at night) -> FOG (weather, vanilla
parity) -> CASCADED shadows (vanilla 3 cascades) -> CONTACT shadows (FO4CS Bend) -> SSAO (FO4CS GTAO) -> SSGI (FO4CS)
-> BLOOM (weather ImageSpace + FO4CS). Not tonight: R5 card bake, legacy L1, Volumetric Air.

## UI (ruled)
Everything lives in ONE new "Scene" POPUP WINDOW: non-modal, moves separately (any monitor), stays above NifSkope only,
opened from the View menu + a viewport toolbar button, remembers size/position. Flat Name|Value tree, skinVars palette,
label + control only (no descriptions). Sections: Mode (Legacy/Studio/Lookdev) / Weather (plugin, weather, hour) / Sky
(sky, clouds, sun, moon) / Ground (plane toggle) / Fog / Effects (cascaded shadows, contact shadows, SSAO, SSGI, bloom).
Every row live. The first stage that needs a row creates the window; later stages add rows to it.

## Gates (every stage)
- Standing zero set via tests/spells/pbr_shade_ab.sh: legacy BGSM/BGEM/embedded, the effect meshes and the 3 particle
  NIFs stay pixel-identical with the new feature OFF. Each new toggle OFF = pixel-identical; ON = a measured invariant
  from the design doc (docs/NIFSKOPE_PBR_RENDERER.md stage table + the RULINGS section at its end).
- Prove each new gate fails once on deliberately broken code (red control).
- A FAILED gate is fixed in the same lane and re-run; the chain does not stop on it.

## House rules
- Read CONSTITUTION.md, the HANDOFF top, the design doc's stage row + RULINGS. search-lean (src/, res/shaders/,
  tests/spells/ one folder per search).
- Rung first: copy release/ exe + shaders to a before_<lane> rung folder.
- ONE NifSkope instance, second monitor (WW_WINDOW_AT 1920,0), --port unused; never kill bungo's window.
- Never git stash; do not commit. Mixed line endings: binary splice, measure with Python byte counts.
- Vanilla data read-only: E:\Tools\Fallout 4\DataUnpacked\Data. Test PBR data in a loose test folder only.
- Do not edit HANDOFF, WW_CHANGES, MISTAKES: DELIVERABLE_TEXT.md (sections HANDOFF text / WW_CHANGES text / MISTAKES
  text). DONE.md first word DONE/PARTIAL/PENDING + release exe sha1. progress.md incrementally.
- Final report under 250 words, plain words, gate verdict lines.

## DELIVERABLE_TEXT.md layout (director splice)
Exactly four level-2 sections: ## HANDOFF text / ## WW_CHANGES text / ## MISTAKES text / ## Skill review. No other "## " line inside them (use ### or plain text).

## Public-repo wording (bungo 2026-09-24)
This repo is PUBLIC. In any tracked file or DELIVERABLE_TEXT.md, never name the engine symbol source, its folder, its tools or how it was obtained; call it Todd's treat. Call the source "Todd's treat" (bungo's stand-in name; its tooling = "the Todd's treat tooling"); keep the facts (function names, offsets). Engine dumps stay in gitignored scratch.
