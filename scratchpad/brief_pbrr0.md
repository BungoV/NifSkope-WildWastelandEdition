# Lane PBRR0 -- PBR renderer stage R0: the old-vs-new shading harness (BUILD lane)

Director brief, 2026-09-23 23:1x. Model: Opus 5.5. Folder: scratchpad/pbrr0_<date>/ ; progress.md INCREMENTALLY.

## Chain rules
Read scratchpad/brief_pbrchain.md first (overnight order, the Scene popup window, gates, house rules). This lane is
its first stage.

## Source of truth
docs/NIFSKOPE_PBR_RENDERER.md: s9 (the harness design), the stage table row R0, and the RULINGS 23:0x section at
the end of the file (bungo's answers; they override the question table where they differ).

## Jobs
1. Build `tests/spells/pbr_shade_ab.sh`, `pbr_shade_ab.py` and `pbr_shade_ab_cases.txt` exactly as s9 says: OLD arm =
   a frozen rung folder (exe + shaders/), NEW arm = the built tree, all listed WW_* pins identical in both arms.
2. Add the new env switches as NO-OPS today that only pin state (WW_PBRM_MODE, WW_PBRM_AUTOREPLACE, WW_LIGHTING_MODE,
   WW_LOOKDEV*, WW_RENDER_PARTICLES, WW_RENDER_SHADOWS/_CONTACT/_AO/_SSGI). WW_RENDER_PARTICLES must default to
   today's behaviour (the shot hook forces showParticles on).
3. Add WW_PBRM_CENSUS: one line per shape = route (swap/nifx/direct/sibling/fo76/legacy), path, envelope, refusal
   reason. It echoes RESOLVED state. In R0 every shape reads legacy (PBR is off by the constant at
   glproperty.cpp:1005); say so in the line, do not change detection.
4. Gates (each must be proven to bite):
   - OLD-vs-OLD twice per case -> the noise bar, recorded per case;
   - `zero` on every s9 fixture (BGSM, BGEM, embedded lit, embedded effect, the 3 particle NIFs, 10mm pistol):
     NEW vs OLD PASS, because R0 changes no pixels;
   - red control: an altered shader copied into the NEW arm FAILS `zero`; census identical between arms.
5. Rung: copy the current release/ exe + shaders into rung folder `before_pbrr0` first (the OLD arm).

## Rules
- Read CONSTITUTION.md and the HANDOFF top first. search-lean before any search (src/, tests/spells/ only).
- You own the build slot in this shared tree; no other lane is building. Never git stash. Do not commit.
- ONE NifSkope instance at a time, second monitor (WW_WINDOW_AT 1920,0), `--port` unused; never kill bungo's window.
- No change to any pixel: legacy, particles and effect meshes stay identical (bungo's rulings 22:3x).
- Do not edit HANDOFF, WW_CHANGES or MISTAKES; put the text in DELIVERABLE_TEXT.md (sections: HANDOFF text,
  WW_CHANGES text, MISTAKES text).
- DONE marker's first word is DONE, PARTIAL or PENDING; include the release exe sha1.
- Final report under 300 words, plain words: verdict lines per fixture, the noise bars, the red-control proof.
