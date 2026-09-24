# Lane IMPOSTORWIND1 -- card sway baked from the tree model's own wind weights

Director brief, 2026-09-23. Model: Opus 5.5. QUEUED behind IMPOSTORSHRUB1 (build slot); may run its offline
research part (jobs 1-2) beside another lane since it builds nothing until job 3.
Folder: scratchpad/impostorwind1_<date>/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
Told that a card's sway weight today is synthetic (docs/LODGEN_IMPOSTOR_SPEC.md:317: h^2 x (0.35 + 0.65 r),
from the picture, no leaf/branch distinction) and offered the upgrade, he said: "sway from the tree's model's
own wind weights would be neat".

## Jobs
1. RESEARCH, vanilla first (standing rule: vanilla research = Todd's treat FIRST, quote RVAs per build;
   the Todd's treat tooling (kept outside this repo)): what a vanilla FO4 tree carries for wind --
   vertex colour channels, the shader's tree-anim flag, BSTreeNode bone lists, anything else -- and how the
   engine's tree shader turns it into motion (which channel = branch bend, which = leaf flutter, their
   ranges). Confirm on the vanilla corpus (E:\Tools\Fallout 4\DataUnpacked\Data): value histograms per
   channel over every tree near model, and 3 named trees where the channels visibly separate trunk, branch
   and leaf cards.
2. DESIGN, written before code: bake the model's wind weights into the card (rendered as a data channel in
   the same offscreen 4x bake, coverage-weighted, like normal/height). Where the channels go: today `_n`
   alpha = synthetic sway, blue = height; a second sway value (leaf flutter) needs a home -- options with
   BC3 precision costs, including the still-unruled height<->sway swap (scratchpad/impostorfin1_20260922/
   hookup_nswap.py). Needs a .lodm version bump so an old reader refuses. Trees with no wind data keep the
   synthetic law. RECOMMEND one layout; the director relays it to bungo before job 3 ships a format change.
3. After the layout is ruled: implement in the bake, the NifSkope card drawer (sway preview animated, a
   clock), spec docs; the FO4CS reader is built last -- contract text only.
4. Gates + pictures: sway sheet as greyscale beside the model's wind channels as colour; a GIF of the card
   swaying beside the 3D model swaying (labelled "3D model" / "Octahedral impostor"), same wind.

## Rules
CONSTITUTION.md first; skills nifskope-ww-lodgen, nifskope-ww-render-shot, nifskope-ww-build-verify,
ww-reference-card-diagnose, ww-test-harness-add. Game down before any build. bungo's NifSkope window: never
kill; rename aside. One harness NifSkope at a time, second monitor. Do not commit; do not edit
HANDOFF/WW_CHANGES/MISTAKES -- text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING.
Final report under 300 words, plain words.
