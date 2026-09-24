# Lane IMPOSTORLIGHT1 -- make the tree cards light like the model

Director brief, 2026-09-22 23:0x. Model: Opus 5.5. You own the BUILD + EXE slot (no other lane is live).
Folder: scratchpad/impostorlight1_20260922/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
He saw pics512/*_before_after.png from IMPOSTORFIN1 (cards at about half the model's brightness after
hookup_cardlight), asked "why is the lighting on the imposter wrong? Normals issue?", got the two candidate
faults below, and said: "Just fix it".

## Read first
CONSTITUTION.md; HANDOFF.md lines 1-12 (the IMPOSTORFIN1 LANDED PARTIAL line);
scratchpad/impostorfin1_20260922/FOR_BUNGO.md + DELIVERABLE_TEXT.md + DONE;
scratchpad/impostorlook1_20260919/DELIVERABLE_TEXT.md sections 2 and 5 (the transfer table: the old card was
DARKEST where the mesh is BRIGHTEST on all five subjects; the pipeline table fo4_default.vert:61,63 /
fo4_default.frag:238,538 vs impostor_oct.frag; frameNormalModel's never-negative z; the two-sided
abs(dot(normal, L))). Skills: nifskope-ww-build-verify, nifskope-ww-render-shot, ww-reference-card-diagnose,
ww-channel-view-refuter, ww-test-harness-add, nifskope-ww-lodgen.

## The two candidate faults (director's reading, NOT proven)
A. NORMALS. Two-sided lighting makes the side facing away from the light as bright as the lit side; and the
   inverted brightness (card dark where mesh bright) may be a flipped normal axis (frame X/Y sign, the
   octahedral frame basis, or the BC3 RG decode). 
B. COLOUR PIPELINE. hookup_cardlight gave the card the mesh's sqrt'd ambient*0.375 + sqrt'd diffuse +
   tonemap and it came out at ~0.5. Suspects: exposure that the mesh path has and the card does not, the
   albedo sheet's colour space (sRGB stored, linear read, or the reverse), ambient scale, AO applied twice.

## Jobs
1. DIAGNOSE FIRST, measured, no guessing. Same views, same headlight, current exe (bf6aa749):
   (a) a NORMALS view: the card's reconstructed world normal as colour beside the mesh's world normal as
       colour, per subject; per-channel sign agreement % and mean angle error over covered pixels. A flipped
       axis shows as a sign agreement near 0% on one channel.
   (b) a BRIGHTNESS LADDER: card vs mesh mean luma at each stage (albedo only, + ambient, + diffuse, + AO,
       + tonemap) so the stage that halves it is named with its line.
   Known-answer control first: a subject whose right answer you know (e.g. a baked flat quad or sphere).
2. FIX what the diagnosis names -- whatever it takes in the bake, the sheet decode or the shader -- so that:
   - card normals agree with the mesh (sign agreement and angle error reported, bar set BEFORE fixing);
   - card brightness / mesh brightness is 0.9..1.1 on all five subjects, and the brightness TRANSFER (card
     luma vs mesh luma across the pixel population) rises, not inverts;
   - a lit side that turns with the light: ONE-SIDED lighting for opaque surfaces (trunk, rock). Keep
     two-sided only where the source material itself is two-sided (read the flag from the near model's
     material; vanilla's own flag decides, not a guess) and say which subjects that is.
   If a fix needs a rebake, rebake. If it changes the sheet format, it needs a .lodm version bump so old
   sheets are refused, and you say so. Do NOT apply the `_n` channel swap (hookup_nswap.py): unruled.
   The transparency cut-off stays vanilla's 128/255.
3. Gates: impostor_draw.sh (row 5 was 0.4979 < 0.50; if it was dark pixels read as background, show that with
   numbers -- never lower a bar), lodgen_octahedral.sh, native_lighting.sh as control. Add gate rows that pin
   the brightness ratio and the normal sign agreement, and prove each new row FAILS on the bf6aa749 exe.
4. Pictures for bungo at cardRes 512, all five subjects: before (bf6aa749) | after, beside the mesh, plus a
   light-from-the-side view that shows the lit side turning. Headline two in FOR_BUNGO.md.

## Rules
Game must be down before any build (check Fallout4.exe). bungo's NifSkope window is open (pid 8728, running
release/NifSkope_inuse_8728.exe): never kill it; rename aside per the build skill. One harness NifSkope at a
time, second monitor, no focus steal. Take the rung release/NifSkope.before_impostorlight1.exe first.
Do not commit; do not edit HANDOFF.md, WW_CHANGES.md, MISTAKES.md -- put their text in DELIVERABLE_TEXT.md.
DONE marker first word DONE / PARTIAL / PENDING. Final report under 300 words: verdict, the named cause(s)
with lines, before/after numbers, gate numbers, picture paths, exe size/time/sha1.
