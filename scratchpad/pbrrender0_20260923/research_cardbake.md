# PBRRENDER0 research -- the card / impostor bake and the shader it draws with

Read-only research, 2026-09-23. Anchors are file:line in the live tree at read time (other lanes are editing;
re-check the anchor text before quoting a number).

## 1. The capture path and the program it draws with

- The bake is the `WW_IMPOSTOR_BAKE=<outdir>` hook inside `NifSkope::createWindow`
  (src/nifskope_ui.cpp:1548), block starting src/nifskope_ui.cpp:22420. It loads the model into the ordinary
  NifSkope window, hides docks/grid/axes (22442-22449), forces an orthographic camera (22487-22488) and locks
  the clear colour (22494).
- Octahedral branch: `WW_IMPOSTOR_OCT=N` at src/nifskope_ui.cpp:22827. The function that renders the source
  mesh for one bake direction is the lambda `renderOff` at src/nifskope_ui.cpp:22980-23030: it resizes the
  GLView, pans the camera, binds a `QOpenGLFramebufferObject` (23004) and calls `gl->paintGL()` (23007), then
  `glReadPixels` (23012). Wrappers: `matteOff` (23033, colour + coverage via black/white passes) and
  `channelOff` (23063, one data channel). Per view it calls matteOff + channelOff 8, 9, 10, 11, 13
  (23461-23466). The AA=0 fallback photographs the window instead (`grabOnce` 22854, `matte` 22898,
  `channel` 22926; calls at 23450-23454).
- Channel selection is the global `wwLodChannelView` (src/gl/glproperty.cpp:39), set before each render
  (22902/23041 = 12, 22927/23064 = which).
- Program choice: `paintGL` draws the Scene, whose shapes call `Renderer::setupProgram`
  (src/gl/bsshape.cpp:300, src/gl/BSMesh.cpp:74, src/gl/glmesh.cpp:790). So YES it goes through
  `Renderer::setupProgram` (src/gl/renderer.cpp:158) -- but with `wwLodChannelView != 0` the PBRM verdict is
  forced false (renderer.cpp:191 `wantPbrm = mesh->bslsp && wwLodChannelView == 0 && ...`) and every FO4
  lighting-shader shape is sent BY NAME to `fo4_default.prog` (renderer.cpp:217-225). Every bake pass has a
  channel set, so the source geometry is ALWAYS drawn by fo4_default.prog / fo4_default.frag during the bake,
  never pbrm_default.prog, whatever pbrmMode() says.
- The captured channels are branches of fo4_default.frag's `lodChannelView` block
  (res/shaders/fo4_default.frag:275-364), all written after the alpha test (so leaf cards cut):
  - 12 = base colour: `baseMap.rgb * C.rgb` (vertex colour), unlit, no tone map (frag:328-333). Lighting is
    also switched off in the scene options for the matte (nifskope_ui.cpp:22901 / 23040).
  - 8 = geometric normal, view space, back-face flipped (frag:299-307) -- NOT the normal map.
  - 9 = window depth `gl_FragCoord.z` (frag:308-310) -> height.
  - 10 = material: a .lodm-retargeted shape's third texture RAW (`lodMaskRaw`), else the legacy pair
    R = gloss x spec map G, G = spec map R x spec strength, B = 1 (frag:311-327).
  - 11 = subsurface/leaf mask: tree-anim flag or alpha-test (frag:334-341).
  - 13 = emissive: a .lodm glow retarget raw, else vanilla glow rule baseMap.rgb*a*emissiveColour on opaque
    shapes (frag:342-364).
- Sheets written (nifskope_ui.cpp:23539-23611): `_oct_albedo.png` = colour + coverage alpha;
  `_oct_normal.png` = normal XY, height (ch 9) in B, sway in A (23585); third sheet `_oct_rmaos.png` or
  `_oct_gsaos.png` by family = ch10 R, G, ch10 B x height-neighbourhood AO, mask (ch 11) in A (23591-23594,
  name 23607); fourth `_oct_e.png` / `_oct_g.png` = ch 13 RGB opaque (23597-23599, 23610). AO is computed on
  the CPU from the height frame (23562-23579), sway from the silhouette (23580-23583). No specular colour, IOR,
  coat or fuzz is captured.

## 2. Offscreen, and the same draw as the viewport

- Default arm is offscreen: one FBO per view, GL_RGBA8, no MSAA, no sRGB, CombinedDepthStencil, at K x the
  frame's inner size (K = `WW_IMPOSTOR_AA`, default 4; 22975-22976, FBO 22998-23004), box-filtered down,
  coverage-weighted (comment 22933-22974). `WW_IMPOSTOR_AA=0` is the window-photograph fallback.
- It is NOT headless in the "separate renderer" sense: it borrows the live GLView's context
  (`pushGLContext` 22986), its camera fields (Dist/Pos/Rot) and calls the same `GLView::paintGL()` the window
  uses (23007) -- same Scene, same Shape draw, same Renderer. The window is still created and placed
  (headless placement helpers src/nifskope_ui.cpp:1375-1580). The only differences from the viewport are the
  channel global, lighting off for the matte, ortho camera, locked clear colour, grid/axes off.

## 3. Does lodgen read or write .pbrm?

- READS only; nothing in src/ WRITES a .pbrm (every `.pbrm` hit in src/ is a lookup, parse or UI string).
- lodgen's one reader: `lodgenResolveMaterialMask` (src/lodgen.cpp:1822-1930; declared + documented
  src/lodgen.h:1245-1320). Arm 1 = a `.pbrm` (the material path itself, or the same-name sibling of a
  .bgsm/.bgem, or, when there is no material, the diffuse stem via `lodmSourceCandidate`), read through
  `lodgenReadAsset` and parsed by `pbrmParse` (io/pbrmfile.h). It takes ONLY: roughness/metallic constants,
  the RMAOS map path with the RmaosRoughness (R) / RmaosMetallic (G) feature bits, and the emissive map path
  (1872-1889). No base colour, normal, specular weight/colour, IOR, tint mask, coat or fuzz.
- Its one caller is the TERRAIN virtual-texture mask, per LTEX layer: `LodgenVtMaskCache::resolve`
  (src/lodgen.cpp:11286-11311, call at 11299; census words at 12927 / 13008). So the brief's phrase "the
  mesh-LOD mask path" (brief_impostorpbrm1.md:32) is really the far-TERRAIN layer mask. The lodgen.h:1246
  comment says it is shared with "the object path's gloss composition", but the only caller found is the
  terrain one; objects use `lodgenLegacyGloss` for the legacy pair.
- The card bake (nifskope_ui.cpp) never reads a .pbrm. It reads a SOURCE `.lodm` per shape
  (nifskope_ui.cpp:22598-22693, `lodmParse`) that retargets texture slots 0/1/2/7 via `wwTextureOverride`;
  family pbr only when every textured shape has a pbr .lodm (22691).
- Viewport side: `BSShaderLightingProperty::resolvePbrm` (src/gl/glproperty.cpp:1047-1086, called at 994)
  parses a .pbrm when the shader's Name ends `.pbrm`, or its same-name sibling when "Auto-replace BGSM/BGEM
  with .pbrm" is on (glview.cpp:3287, menu nifskope_ui.cpp:27398). Result lives on the shape as
  `pbrm` / `pbrmValid` and is consumed by `setupProgramPBRM`. That is what renderer.cpp:190 means: a
  terrain LOD shape whose material resolved a .pbrm this way would have taken pbrm_default.prog (and lost the
  channel preview) -- "baked" there means a .pbrm the user placed beside the terrain material, not one NifSkope
  produced. The shipped UI forces pbrmMode to Legacy and greys the two PBR modes out as "unfinished"
  (nifskope_ui.cpp:27363-27378; `pbrmFeatureEnabled` in glproperty.cpp gates the runtime so a stored setting
  cannot re-enable them), so today pbrm_default.prog is not reached in normal use at all.
- Also: nifcli `pbrm <file>` and the adoption probe (src/nifcli.cpp:299, 354-391) parse/print a .pbrm.

## 4. What brief_impostorpbrm1.md asks for, and what it needs from a NifSkope PBR renderer

Asks (scratchpad/brief_impostorpbrm1.md):
- bungo's question (line 7): do models with a .pbrm material bake correctly? Answer given: partly, untested.
- Card family pbr when every textured shape resolves a .pbrm (same lookup order as `lodgenResolveMaterialMask`,
  through the resource stack), sheets `_bc`, `_n`, `_rmaos` (roughness, metallic, AO, subsurface) and `_e`
  (emissive, luminance = intensity x100 nits), all "baked as data channels from the .pbrm's own
  textures/values in the same 4x offscreen bake" (lines 42-45). Mixed models need a stated rule (45-46).
- RULED additions (11-25): the card must carry the .pbrm's specular WEIGHT, specular COLOUR and IOR (PBRM v6
  + Pattern B: params on the texture input, overrideIor, sparse slot values) -- either a fourth sheet `_s`
  (colour x weight RGB, IOR in alpha) or folded into one per-pixel F0, with the BC precision cost and the
  dielectric-vs-metal loss stated. And the TINT MASK applied IN the bake so `_bc` holds the tinted colour,
  using the editor's law `tint = max(0, (1 - sum(m)) + cR*m.r + cG*m.g + cB*m.b + cA*m.a); base *= tint`
  (PBRMaterialEditorQt materialpreviewwidget.cpp:2314), checked against Warframe's rules.
- Coat / fuzz / specular colour / IOR: say which the LOD format carries and which it drops (46-47).
- Explicitly NOT the PBR renderer (34-35): "Data channels only." Implement in the bake only; FO4CS reader last.

What it needs from a NifSkope PBR path -- inputs and outputs, not lighting:
- INPUTS per shape: the parsed .pbrm (`PbrmMaterial` already on the shape via `resolvePbrm`, or via
  `lodgenResolveMaterialMask`'s lookup order), its texture slots resolved to GL textures (base colour, normal,
  RMAOS, emissive, tint mask, and the v6 specular weight/colour/IOR inputs), the constants/overrides when a
  slot is absent, and the UV transform.
- CAPTURED CHANNELS (all unlit material values, per pixel, post alpha test):
  base colour x tint law x vertex colour -> `_bc` RGB (coverage from the matte);
  roughness, metallic, material AO -> `_rmaos` R, G, B (B then x the CPU height-AO, as today);
  emissive colour x intensity (with emissiveScale carrying the >1 multiple in the .lodm) -> `_e`;
  specular weight x colour and IOR (or a folded F0) -> the new home the lane must recommend;
  normal -> `_n` (today the GEOMETRIC normal only, frag:299-307; a .pbrm's normal map is not in the card).
- Which pbrm fields end up in the card: base colour (tinted), roughness, metallic, AO, emissive, and
  specular weight/colour/IOR (ruled). Coat, fuzz, sheen, retroreflection, film: nothing in the LOD format
  carries them (spec table docs/LODGEN_IMPOSTOR_SPEC.md:42-47; "not carried" list 619-625).
- Gap today: `pbrm_default.frag` has no specular weight/colour/IOR or tint-mask inputs at all (its uniforms are
  BaseMap/NormalMap/RmaosMap/EmissiveMap + pbrRoughness/Metallic/Ao/F0/Emissive*, res/shaders/pbrm_default.frag:19-39),
  and it has no `lodChannelView` branch (renderer.cpp:186-190). So neither NifSkope program can emit the
  ruled channels from a .pbrm today.

## 5. Design: one PBR path for the viewport and the bake

What the current bake does: it captures MATERIAL CHANNELS, not lit colour. Evidence: the colour pass is
channel 12, `baseMap.rgb * C.rgb`, "no lighting, no tone map" (res/shaders/fo4_default.frag:328-333), with
`Scene::DoLighting` also cleared for the matte (src/nifskope_ui.cpp:23040, 22901); the material pass is
channel 10, raw texture or the legacy gloss/spec pair (frag:311-327); normal and height are geometric
(frag:299-310). Nothing lit ever reaches a sheet; the consumer lights the card (comment nifskope_ui.cpp:22895-22897).
So the card bake is already independent of any BRDF. Keep that: it is the right design, and it means the
bake never needs the PBR LIGHTING to be finished -- only the PBR MATERIAL EVALUATION.

Recommendation (both ideas together, no second renderer):
1. Split the PBR program into two halves in one shader source: a shared "surface" function that turns a
   .pbrm into per-pixel material values (base colour with the tint-mask law and vertex colour, perturbed
   normal, roughness, metallic, AO, specular weight x colour, IOR or folded F0, emissive colour/intensity),
   and the lighting that consumes it. pbrm_default.frag lights the struct for the viewport; a
   `lodChannelView` branch in the SAME file writes one field of the struct flat for the bake (the way
   fo4_default.frag:275-364 already does for legacy). One evaluation, two outputs.
2. In `Renderer::setupProgram` (src/gl/renderer.cpp:185-225), change the verdict for the bake: a shape whose
   .pbrm resolved goes to pbrm_default.prog in channel mode too, instead of the current
   `wwLodChannelView == 0` term at 191 forcing it to fo4_default. Keep terrain preview channels 1-6 (vertex
   data views) on fo4_default by name, as 217-225 does, and route only the bake channels (8-13 and any new
   specular channel) to pbrm. Also: set `lodChannelView` on pbrm_default (today only fo4_default gets it,
   glproperty.cpp:736 / renderer.cpp:1270), and fix the stale-hint rule at 237 to the new verdict.
3. The verdict must not depend on the UI mode: the shipped UI forces Legacy and greys PBR out
   (nifskope_ui.cpp:27363-27378) and `pbrmFeatureEnabled` blocks the runtime. The bake needs its own "a .pbrm
   resolved for this shape" answer with the SAME lookup order as `lodgenResolveMaterialMask`
   (lodgen.cpp:1836-1863, including the diffuse-stem fallback that `resolvePbrm` lacks, glproperty.cpp:1057-1064).
   Best done by making resolvePbrm and the lodgen resolver share one candidate function.
4. Card family: pbr when every textured shape resolved a .pbrm OR a pbr .lodm (today only the .lodm counts,
   nifskope_ui.cpp:22691).

Cautions:
- The FBO is GL_RGBA8 (nifskope_ui.cpp:23000). Dielectric F0 0.02-0.08 gets only about 15 steps in 8-bit
  linear; IOR in an 8-bit alpha needs a stated mapping. Emissive intensity above 1 clamps, so the multiple
  must ride the .lodm (`emissiveScale`, as today, 22716-22754), with the x100-nits law applied there.
- The normal sheet is the GEOMETRIC normal (frag:299-307). A .pbrm normal map would add detail; changing that
  for legacy too would move every legacy sheet, so gate it to the pbr family or treat it as its own lane.
- Terrain's reason for the gate at renderer.cpp:186-190 (pbrm had no preview branch, every channel came out
  identical) goes away only once pbrm_default has the branch; until then the gate is correct.
