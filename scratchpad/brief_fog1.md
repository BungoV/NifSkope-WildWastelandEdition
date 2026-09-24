# Lane FOG1 -- PBR renderer: weather fog (distance power law + height blend, day/night) (BUILD lane)

Director brief, 2026-09-24 17:3x. Model: Opus 5.5 (account A, in-session agent). Folder: scratchpad/fog1_20260924/ ;
progress.md INCREMENTALLY. Chain rules: scratchpad/brief_pbrchain.md (DELIVERABLE_TEXT.md = exactly four "## " sections).
Skills: nifskope-ww-pbr-shade-ab (if present), nifskope-ww-build-verify, nifskope-ww-render-shot, ww-test-harness-add,
search-lean. PRECHECK: E: >= 2 GB free; Fallout4.exe not running. Another NifSkope may be open -- never kill it; use an
unused --port; every window on the second monitor (1920,0); one NifSkope instance of yours at a time.

## Read first
- scratchpad/pbrprep1_20260924/spec_fog.md IN FULL (+ its probes and fog_model.py). It is the law for this lane.
- PBRWX1 lane text (scratchpad/pbrwx1_20260924/DELIVERABLE_TEXT.md): the weather preview this builds on (sky colours
  in CIELab, sun arc, clouds, moon, hour row, Sky group in the Scene popup).
- docs/NIFSKOPE_PBR_RENDERER.md: the weather/sky row and RULINGS. HANDOFF.md top block.

## Rulings in force
- The sky dome is NOT fogged (matches the game; bungo). Fog applies to scene geometry only.
- Linear HDR only while Bloom/SSGI is on -- neither exists yet; fog is applied where the spec says the engine applies it,
  in the renderer's current space, and the lane text states which space that is.
- Night light follows the sun arc; the moon is visual only. Masters ship OFF: the Fog toggle ships off.

## Scope
1. FOG from the current WTHR: near/far/power/max, day and night, blended linearly on the spec's weight (climate times
   widened by fDaytimeColorExtension); the four NAM0 fog colours blended in CIELab over the time-of-day slots, times the
   per-slot NAM4 fog colour scale, then pow 2.2 -- exactly as spec_fog.md.
2. HEIGHT FOG: the two-plane height blend per the spec.
3. A Fog toggle in the Sky group of the Scene popup (settings persisted, harness-isolated); the hour row drives it live.

## Gates (each with a red control that fails)
- Fog factor at 3 distances x 2 hours vs fog_model.py, +-1/255 (red: wrong power or no day/night blend fails).
- Fog colour at mid-key, between keys and dusk vs the spec's CIELab blend x NAM4 scale, +-2/255.
- Height blend at two heights vs the spec model.
- Sky pixels identical with Fog on and off (the sky is not fogged).
- Fog OFF restores the before_fog1 picture byte-identically.
- Legacy zero set + R1/R2a/R2b/R3/R4 + PBRWX1 gates still PASS.
Rung before_fog1 from release/NifSkope.exe sha1 45108d83.

## Rules
No push, no edits to HANDOFF.md / WW_CHANGES.md / MISTAKES.md (you deliver the text, the director splices). Commit your
own source by explicit path list in small commits as each step compiles (never -a/-A, never git stash). Public repo:
call the engine symbol source "Todd's treat" and nothing else; no game data or binaries committed.
Line endings by Python byte count only; never open a file for writing in the same expression that reads it.

## Report
DONE.md + DELIVERABLE_TEXT.md in the lane folder, as the chain rules say, with images of fog on/off at dawn, noon, night.
Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote. A procedure you
re-derived from memory or worked out from first principles is a missing skill -- write it under
`E:\Projects\Claude\.claude\skills\<name>\SKILL.md` before you finish. Declining is allowed: name the procedure and
say why it will not recur.
