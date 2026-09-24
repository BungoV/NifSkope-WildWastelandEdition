# Lane PBRR2A -- PBR renderer stage R2a: studio lighting + the Scene window (BUILD lane)

Director brief, 2026-09-24. Model: Opus 5.5. Folder: scratchpad/pbrr2a_<date>/ ; progress.md INCREMENTALLY.
Chain rules: scratchpad/brief_pbrchain.md. Skills: nifskope-ww-pbr-shade-ab (gates), the render-shot skill.

## Read first
docs/NIFSKOPE_PBR_RENDERER.md: the stage table row R2a (line ~740), the lighting/ambient sections it cites, and the
RULINGS at the end. HANDOFF.md: the RULED 00:5x 2026-09-24 line (full chain + the Scene WINDOW ruling, 01:2x/01:3x).
Lane text of PBRR1 (scratchpad/pbrr1_20260924/DELIVERABLE_TEXT.md) and LIGHTANGLES1 (lightangles1_20260924/): the
light angles are now remembered; they are saved on "Save Lighting" only and act only with Frontal Light off.

## Scope = the R2a row
Scene mode row (Legacy/Studio; Lookdev arrives in R2b); `SFCubeMapCache` for FO4 (Studio only); sun in linear units;
EV exposure; view transforms; shader sRGB encode; the PBR program's legacy-mode output transform.

## The Scene WINDOW (create it in this stage; bungo's ruling)
A NEW non-modal tool window "Scene", NOT a dock panel: separate, movable to any monitor, stays above NifSkope only,
opened/closed from a View menu entry AND a viewport toolbar button, remembers size and position (incl. the second
monitor). Flat Name|Value tree, skinVars palette only, label + control only (no descriptions/tooltips-as-help). Build
its section skeleton now: Mode / Weather / Sky / Ground / Fog / Effects; only rows this stage makes real are enabled
(Mode, exposure, view transform, light angles if they fit); later stages fill the rest. Every row applies live.
The PBR route debug view PBRR1 added: move it into the Scene window if it is a View-menu entry now (say which).
PBR display default stays Legacy (Q9) until R3.

## Gates = the R2a row
Legacy mode `zero` on everything (pbr_shade_ab zero set); uniform white cube L -> prefiltered mip readback = L and
irradiance = L (+-1/255); EV +1 doubles the pre-tonemap value (tonemap None); linear 0.5 grey -> 188 +-1 on screen;
sRGB-tagged and UNORM-tagged copies of one base texture render identical. Each with a red control that fails.
Scene window: a WW_* harness opens it, sets a row, confirms the live effect, closes/reopens and reads back its
geometry; with isolated settings.
Rung before_pbrr2a from the exe the previous lane leaves. DONE.md + DELIVERABLE_TEXT.md as in the chain rules.
