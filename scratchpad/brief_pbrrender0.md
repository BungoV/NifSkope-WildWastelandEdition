# Lane PBRRENDER0 -- NifSkope renders PBR materials: research + design (NO build)

Director brief, 2026-09-23 22:2x. Model: Opus 5.5. Folder: scratchpad/pbrrender0_<date>/ ; progress.md INCREMENTALLY.

## bungo's signal (2026-09-23 22:2x)
"Nifskope would need PBR to be rendered first, when a .pbrm materials is detected or a .json file that is linked to
our nif file, that points to our shader node"
- This is THE go for the long-held PBR renderer work (held since 2026-07-28).
- It comes BEFORE the card-PBR lane IMPOSTORPBRM1 (scratchpad/brief_impostorpbrm1.md), which now waits on this.
  The card bake captures what NifSkope renders, so PBR models must render as PBR first.

## Jobs (research + a design doc; no src edits, no build, no NifSkope launch)
1. **What NifSkope has today.**
   - docs/TO_BE_IMPLEMENTED.md: the PBR renderer item ("PBR draws empty frames" / §0), its current state.
   - The renderer and shaders under src/gl and res/shaders (search-lean: those folders only).
   - What draws when a FO4 mesh is PBR today? Measure from the code; say what is unknown.
2. **Detection: how a NIF is known to be PBR.** Find the FO4CS runtime's actual rules and quote file:line in
   ONE FO4CS worktree (wt-spec1: src/Materials/PBRM.cpp, TruePBRShimRuntime.cpp, and any .json linkage):
   - how a .pbrm is found for a mesh/material (path mapping from the BGSM/BGEM or the NIF shader property);
   - what the ".json file linked to our nif file, that points to our shader node" is: format, where it lives, how it
     names the shader node.
   - Also PBRMaterialEditorQt\docs\PBRM-v6.md for the .pbrm side.
   NifSkope must use the SAME rules the game uses, so the viewport and the game agree.
3. **The reference law.** The editor's preview renderer (PBRMaterialEditorQt/src/materialpreviewwidget.cpp + its
   GLSL) is the reference. Map it:
   - what ports first: BRDF, base colour / normal / RMAOS, v6 specular weight, colour and IOR (Pattern B), tint masks,
     emission (nits / 100), and the glossy-diffuse energy split (as FO4CS wave 88 does);
   - what is deferred: bungo warned the editor's custom refraction and particle renderer "may not work well at first
     or at all" in NifSkope's GL scene; the renderer merge comes LAST.
4. **The card bake.** Find where the impostor capture renders (src/, the lodgen card path). Design it so the SAME PBR
   path serves the viewport AND the bake, with no second renderer. Name what IMPOSTORPBRM1 then needs.
5. **The design doc:** docs/NIFSKOPE_PBR_RENDERER.md (new file; the only repo file you write). It holds:
   - detection rules;
   - the shader plan;
   - the uniforms and texture slots;
   - a staged lane plan (e.g. R1 detect + load + a debug overlay, R2 BRDF + v6 specular, R3 tint/emission,
     R4 the card bake uses it);
   - gates per stage: measured invariants that fail on broken code, such as a white-furnace pixel read, a v5 vs v6
     F0 read, or a sphere render compared against the editor's preview at a named pixel;
   - the open questions for bungo, each with a recommended answer and a one-line why.

## Rules
- Read CONSTITUTION.md and the HANDOFF top first.
- search-lean before ANY search. Never the NifSkope root or scratchpad; never the FO4CS root (use wt-spec1 only).
- Lane VTNORMAL1 is building in this tree now and DOCFIX1 is editing docs/LODGEN_*.md. Touch neither: no build, and
  no edit outside your new doc and your folder.
- Blender is the design reference when unsure (bungo's standing rule); state any divergence.
- Do not commit. Do not edit HANDOFF, WW_CHANGES or MISTAKES; put the text in DELIVERABLE_TEXT.md.
- The DONE marker's first word is DONE, PARTIAL or PENDING.
- Final report under 300 words, plain words: what renders today, how detection works, the stage plan, and the
  questions for bungo.

## RULING ADDED 22:3x (bungo; sent to the lane by message)
"Make sure you do not break legacy spec/gloss materials, however, you can upgrade those where needed, if the way they're lighted up does not align with how Fallout 4 works" -> legacy path unchanged for non-PBR meshes (pixel-identical gate per stage); audit NifSkope legacy spec/gloss vs vanilla FO4 lighting (Todd's treat/vanilla shaders first); a separate upgrade stage fixes only measured mismatches.

## RULING ADDED 22:3x (bungo; sent by message)
"There's particles too, so make sure you don't break their display specifically" -> particle systems + effect-shader meshes display exactly as today; a pixel-identical gate per stage on named vanilla particle/effect NIFs; the editor's custom particle renderer stays deferred to the merge.

## STANDING OK 22:4x (bungo; sent by message)
"You can run comparison tests when needed, to compare old and new shading of materials, bgsm bgem or embedded either" -> gates cover BGSM, BGEM and NIF-embedded materials; one reusable old-vs-new shading comparison harness (rung vs new exe, per-pixel diff + verdict line); implementation stages run it.

## ADDED 22:4x (bungo: "scene lighting in nifskope might need an upgrade"; sent by message)
Audit scene lighting (lights, ambient, env/cubemap, exposure/tonemap, sRGB) vs FO4 and the editor preview; propose an IBL + sun lighting stage; how it coexists with the pixel-identical legacy/particle gates (harness pins old lighting, or a scene mode with a UI row, default = bungo).

## ADDED 22:5x (bungo; sent by message)
"You can load a Fallout 4's scene, with the sky, clouds, sun and a ground plane and the cubemaps (or just the cubemap if pbrm), to test the display of materials in nifskope" -> FO4 lookdev test scene from vanilla assets (DataUnpacked, read-only): sky dome, clouds, sun, ground plane, cubemaps (legacy env maps; cubemap-only IBL for .pbrm); one loader for the harness (pinned), the viewport (option, default = bungo) and possibly the card bake.

## ADDED 22:5x (bungo; sent by message)
"Make it so you can load any weather from Fallout 4, so from any .esm or .esp you load" -> lookdev scene driven by a WTHR record picked from any loaded plugin (load order, overrides), built on the ESP record reader; time-of-day control; cubemap source named (not in WTHR); stage 1 may take sun + ambient + cubemap only.

## ADDED 22:5x (bungo; sent by message)
Shadows (vanilla cascaded + FO4CS screen-space), SSAO and SSGI match vanilla FO4 and FO4CS; each on/off with a UI row (defaults = bungo; masters-off rule); toggles-off keeps every pixel-identical gate; toggles-on measured invariants.
