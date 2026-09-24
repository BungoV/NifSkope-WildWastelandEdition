# Lane IMPOSTORPBRM1 -- cards baked from models with a .pbrm material keep their PBR quantities

Director brief, 2026-09-23. Model: Opus 5.5. QUEUED behind IMPOSTORDEPTH1. Folder:
scratchpad/impostorpbrm1_<date>/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
"And currently, if I bake models that have PBRM material, the textures will get baked correctly?" Told:
partly and untested; offered the small fix (the card bake reuses the mesh-LOD path's .pbrm reader, no PBR
renderer); he said "Yes, queue".

## bungo's scope additions 2026-09-23 07:5x (RULED, not optional)
- "for PBRM, we need specular weight, specular color and specular ior input stuff from .pbrm rendered" -- the
  card MUST carry the .pbrm's specular weight, specular colour and IOR (PBRM v6 + Pattern B: feature params on the
  texture input, overrideIor, sparse slot values; contract Fo4CommunityShaders\PBRMaterialEditorQt\docs\PBRM-v6.md
  and docs/PBRM-v6-Specular.md). Design the home: e.g. a fourth sheet `_s` (specular colour x weight RGB, IOR in
  alpha on a stated mapping) OR folding weight x colour x IOR into one per-pixel F0 -- RECOMMEND one with its
  precision cost (BC formats) and what a dielectric vs a metal loses; the director relays before a format ships.
- "We also need tint masks, the four channels that tint the base color a certain way. Works same as in warframe"
  -- the .pbrm TintMask slot (mask texture R/G/B/A = four regions; colours colorR..colorA, default white; the
  editor's preview law, PBRMaterialEditorQt\src\materialpreviewwidget.cpp:2314:
  tint = max(0, (1 - sum(masks)) + colorR*m.r + colorG*m.g + colorB*m.b + colorA*m.a); baseColor *= tint)
  is applied IN THE BAKE so `_bc` holds the tinted colour. Check that law against Warframe's published TennoGen
  tint-mask rules and report any difference as a finding (do not change the editor). Say whether per-reference
  tint (colours varying per placed object) exists in FO4 data; if it does, the mask would have to ride the card
  instead -- report, do not build.

## What the director read (verify, do not trust)
- Card family (src/nifskope_ui.cpp ~22557-22647): pbr ONLY when every textured shape carries a pbr SOURCE
  `.lodm` (docs/LODGEN_IMPOSTOR_SPEC.md:170-176). A `.pbrm` alone does not make a card pbr; the third sheet
  is then legacy GSAOS from the vanilla material and the .pbrm's roughness / metallic / specular colour /
  IOR are ignored.
- The mesh-LOD mask path already reads a .pbrm (src/lodgen.cpp ~1829-1919, `LODGEN_MASK_PBRM`, arm 1:
  `.pbrm` beside the material, pbrmParse, RmaosRoughness / RmaosMetallic features).
- NifSkope's PBR viewport modes are forced to Legacy (nifskope_ui.cpp ~27318-27323) -- the PBR RENDERER is
  NOT this lane (standing: no NifSkope PBR renderer work until bungo says so). Data channels only.

## Jobs
1. Fixture: find or make a tree whose shapes carry v6 .pbrm materials (PBRMaterialEditorQt output; contract
   docs in Fo4CommunityShaders\PBRMaterialEditorQt, PBRM v6 + Pattern B: feature params on the texture input,
   overrideIor, sparse slot values). Say where it came from. Bake it on the current exe: what each sheet holds
   today, numbers per channel.
2. Design, written first: when every textured shape resolves a .pbrm (same lookup order as the mask path, via
   the resource stack), the card is family pbr: `_bc`, `_n`, `_rmaos` (roughness, metallic, AO, subsurface)
   baked as data channels from the .pbrm's own textures/values in the same 4x offscreen bake; `_e` from its
   emissive (luminance convention: intensity x100 nits, per the PBRM alignment). Mixed models (some shapes
   pbrm, some not): say the rule. Specular colour / IOR / coat / fuzz: which the LOD format can carry and
   which it drops, with the reason (docs/LODGEN_IMPOSTOR_SPEC.md "Ours, for LOD").
3. Implement in the bake only; the FO4CS reader is built last (contract text only). .lodm version bump only if
   meaning changes.
4. Gates: lodgen_octahedral.sh, impostor_draw.sh, native_lighting.sh control; a new row: a pbrm fixture bakes
   family pbr with roughness/metallic matching the source within a stated tolerance (fails on the rung).
5. Pictures: "3D model" | "Octahedral impostor" per channel (roughness, metallic) beside the source's own.

## Rules
CONSTITUTION.md first; skills nifskope-ww-lodgen, nifskope-ww-build-verify, nifskope-ww-render-shot,
ww-reference-card-diagnose, ww-test-harness-add. Game down before any build. bungo's NifSkope window: never
kill; rename aside. Rung first. One harness NifSkope at a time, second monitor. Do not commit; do not edit
HANDOFF/WW_CHANGES/MISTAKES -- text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING.
Final report under 300 words, plain words.
