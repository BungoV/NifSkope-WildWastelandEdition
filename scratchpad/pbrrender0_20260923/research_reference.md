# PBRRENDER0 research: the PBR Material Editor preview as reference law

Read 2026-09-23 22:28. Read-only. Abbreviations:
- ED = E:\Projects\Fo4CommunityShaders\PBRMaterialEditorQt\src\materialpreviewwidget.cpp (preview GLSL is inline raw strings; mesh fragment shader = lines 1603-2924)
- NS = E:\Projects\NifskopeWildWastelandEdition\res\shaders\pbrm_default.frag
- CS = E:\Projects\Fo4CommunityShaders\wt-spec1 (head b0d06b4 "PBRM v6 specular runtime", 2026-09-23 15:27). Note: wt-spec1 has no `features\` folder; its PBR shaders live in `res\Lighting\` (searched there, one folder).

## 1. BRDF

Editor (direct key light), ED:2633-2695:
- D: GGX/Trowbridge-Reitz, alpha = rough^2. ED:1784 `D_GGX`, ED:2645.
- G: SEPARABLE Schlick-Smith G1*G1 with k = (rough+1)^2/8 (the UE4 direct-light k), divided by 4 NoL NoV. ED:1785, ED:2645, ED:2652. NOT height-correlated.
- Roughness clamp: [0.035, 1] (ED:2345). NoV floored at .001, NoL/NoH/VoH at 0 (ED:2634).
- Fresnel: F = min( mix( Schlick(VoH, dielectricF0), F82(VoH, base, metalTint), metal ) * filmTint, 1 ). ED:2643.
  - Schlick: ED:1786.
  - F82 (OpenPBR conductor, Hoffman/Kutz form, mu_bar = 1/7): ED:1789. metalTint = specular colour (specularTint), 1 = plain Schlick on base.
- Multiscatter: Kulla-Conty style via BRDF LUT. E_ss = clamp(lut.x+lut.y, .001, 1); F_avg = f0 + (1-f0)/21; ms = 1 + F_avg*(1-E_ss)/E_ss; spec *= ms (direct AND env). ED:2646-2652, ED:2716.
  - LUT = 256x256 RGBA16F rendered once by a GLSL integration pass (ED:3764-3786, program m_brdfIntegrationProgram), indexed (NoV, rough).
- Diffuse: Lambert, diffuse = (1-F)*(1-metal)*base/PI (ED:2679). F is the per-light half-vector Fresnel (VoH), so the glossy/diffuse split on direct light is instantaneous Fresnel, not directional albedo.
  - EON (OpenPBR base_diffuse_roughness, Portsmouth-Kutz-Hill 2024) replaces Lambert when uDiffuseRoughness > 0: fss = rho/PI*AF*(1+dr*stinv) + multiscatter fms; still weighted (1-F). ED:2680-2690; albedo fit ED:1792.
- Direct sum: color = (diffuse+spec) * keyColor * keyIntensity * NoL * parallaxShadow * keyShadow (ED:2693-2695). No PI multiply on light (light is radiance-like; NoL * BRDF).
- Env (ambient) split, ED:2713-2717:
  - envDiffuse = (1 - Schlick(NoV, f0)) * (1-metal) * base * irradiance * 0.45 * uDiffuseScale   <- weight is Schlick at NoV, NOT (1 - E_spec); plus an unexplained 0.45 gain.
  - envSpec = prefiltered(R, rough*maxLod) * (f0*lut.x + lut.y*mix(1, metalTint, metal)) * filmTint * ms * specOcclusion * uSpecularScale
  - (envDiffuse + envSpec) * uEnvironmentIntensity * ao.

FO4CS wave 88 / lane SPECV6 (CS res\Lighting\truepbr_brdf.hlsli):
- D GGX (27-33), Vis = HEIGHT-CORRELATED Smith (35-45), Schlick F (47-52), no F82.
- Direct diffuse = Burley (Disney) * (1 - Schlick(F0, LdotH)) / PI * PI (engine light units) (54-84).
- Env BRDF = Lazarov analytic fit, no LUT (86-94).
- Multiscatter = Turquin/Filament: 1 + F0*(1/E - 1), E = max(A+B, .05) (96-106). Applied to direct spec too (145-156).
- GLOSSY-DIFFUSE SPLIT (OpenPBR spec line 671, f_gd ~= f_spec + (1 - E_spec(wo)) f_diffuse):
  E_spec = (F0*A + B) * multiscatterComp  (truepbr_brdf.hlsli:108-119, `TruePBR_IndirectSpecularAlbedo`);
  indirect diffuse *= 1 - saturate(E_spec)  (ambient_ibl_pass.hlsl:1176-1201, publish at 3936-3950);
  sun-pass sky ambient: `skyAmbient *= (1.0 - f4fxSpecularWeight) * f4fxDiffuseAO` (bsdf_light_sun.hlsl:3223).
  Gate: [Materials] bIndirectEnergySplit, ships ON (src\Materials\OpenPbrEnergy.h:42-59; read at src\Materials\TruePBRShimRuntime.cpp:5751-5756).
- VERDICT: the editor does NOT match wave 88's split. Editor env diffuse uses (1 - Schlick(NoV,f0)) and an extra *0.45; FO4CS uses (1 - E_spec) with E_spec = split-sum albedo x multiscatter. Editor direct = Lambert*(1-F(VoH)); FO4CS direct = Burley*(1-F(LdotH)) (VoH == LdotH, so the Fresnel weight agrees; the lobe shape does not). G differs (separable Schlick-k vs height-correlated). Multiscatter formula differs (Kulla-Conty F_avg vs Turquin F0). The OpenPbrEnergy.h comment (lines 30-33) itself says the editor preview "owes the same rule".

## 2. Inputs

- Upload: every map is RGBA8 UNORM (not an sRGB format), vertically flipped, mipmapped, Repeat (ED:3625-3634). Missing-map fallbacks: base white, normal (128,128,255), RMAOS = the constants (r,m,ao,specWeight) (ED:3639-3642).
- Base colour: pow(tex.rgb, 2.2) (NOT the piecewise sRGB curve), times pow(tint,2.2) * colorScale (ED:2256-2257). Constant fallback also pow 2.2. Base is NOT multiplied by vertex colour in the lit path (vColor only feeds unlit, gradient drivers, shelter, ED:2260/2471/2625).
- Normal map: XY = (s*255 - 128)/127 (128-centred, ED:1747 `unpackNormalXY`), times strength * globalNormalStrength; Z reconstructed sqrt(1 - xy.xy), stored B ignored (B may be height) (ED:2212). No green flip anywhere in the preview shader. Tangent frame: T Gram-Schmidt'd against the geometric normal, B = cross(N,T) * tangent.w; geometric normal flipped for back faces BEFORE the frame is built (ED:2163-2166). NIF meshes import tangents with w forced to 1 (previewmesh.cpp:389).
- Normal alpha: "Curvature" (signed, 0.5 neutral) / "Cone Map" (reserved, unused) / "None" (ED:538). Curvature feeds cavity spec occlusion (ED:2672-2678) and states.
- RMAOS: R roughness (clamped .035..1), G metallic, B AO, A = v6 specular weight (ED:2345-2363) or porosity when alphaCarries="Porosity" (weight then falls back to the constant, ED:532). Each channel has its own override-to-constant bool (ED:515-518).
- Opacity: base alpha unless overrideOpacity (ED:2258); composition mode 0 = opaque (opacity forced 1), 1 = alpha test vs uAlphaThreshold, 3 = premultiplied (ED:2336-2337, 2921).
- AO: ambient only. Multiplies the whole environment term (diffuse AND specular) and the fuzz ambient (ED:2717, 2769). Direct light never sees AO. Extra specular occlusion only from a bent-normal input (ED:2703-2712).
- Porosity: consumed ONLY by the surface-state system (wet/snow/etc.), ED:2419-2421; RMAOS.A when alphaCarries=Porosity, else constant, else saturate(rough*(1-metal)). No effect on a material with no active state. -> deferred.

## 3. PBRM v6 specular

Contract: E:\Projects\Fo4CommunityShaders\PBRMaterialEditorQt\docs\PBRM-v6-Specular.md s1-s4 (lines 9-95). Preview: ED:2348-2372.
- F0_ior = ((ior-1)/(ior+1))^2 (ED:2371; doc s2 line 33).
- specular weight = RMAOS.A (alphaCarries="Specular Weight", overrideSpecularWeight=false) else constant `specularWeight` (default 1) (ED:2363, 514-518).
- specular colour = tint over the IOR level, dielectric only: tint = pow(colour.rgb, 2.2) from the map, or the slot constant `color` when overrideColor (constants are packed into the special layer texel, ED:826-828). Slot disabled = tint 1.
- dielectricF0 = specWeight * F0_ior * tint (ED:2372). f0 = mix(dielectricF0, base, metal) (ED:2643). On METALS the same colour is the F82 edge tint (ED:2642-2643), and env uses lut.y*mix(1,tint,metal) (ED:2716).
- IOR: slot constant `ior` (default 1.5, read even when the slot is disabled, ED:528) unless overrideIor=false, then ior = specColour.A * iorMax (default 4.25) (ED:2369, 529-530). Map REPLACES scalar, never multiplies.
- Pattern B: ior, iorMax, overrideIor, color, overrideColor all live on the `primarySpecularColor` TEXTURE SLOT's values, not in a material settings group (ED:524-531, doc line 40).
- The v5 0.16 conductor clamp is gone (ED:2356-2359; doc s1 line 27).
- Secondary/tertiary layers: their RMAOS.A is a weight on the SAME material IOR, untinted: vec3(sf*iorF0) (ED:2381, 2389).
- v5 vs v6 for the same numbers:
  - v5: F0 = min(f0, 0.16), f0 = RMAOS.A or the constant (default 0.04); spec colour A was a tint coverage: tint = lerp(1, rgb, a).
  - v6: F0 = weight * ((ior-1)/(ior+1))^2 * tint.
  - Same stored texel means different things: RMAOS.A texel 10 (0.039) = F0 0.039 in v5, but weight 0.039 * 0.04 = F0 0.0016 in v6. A v5 constant f0 = 0.04 migrates to weight 1, ior 1.5 -> identical F0 0.04. f0 = 0.08 -> w 1, ior = (1+sqrt .08)/(1-sqrt .08) = 1.789 -> F0 0.08 (v5 would also give 0.08; above 0.16 v5 clamped, v6 does not) (doc s4 lines 75-79).
  - A NifSkope reader that keeps feeding RMAOS.A as F0 on a v6 file reads the WEIGHT as F0: weight 1 -> F0 1.0, a mirror.

## 4. Optional lobes: port first or deferred

- Emission -- CORE, PORT FIRST. Radiance = pow(colour,2.2) * mask(emissive.A) * (luminance / 100) (division on the CPU, ED:545; shader ED:2518-2521). Texture colour REPLACES the constant colour unless overrideColor (ED:2519). Added last, before tonemap (ED:2919), so 100 nits = linear 1.0 before exposure. Not multiplied by any glow scale. Doc: PBRM-v6-Specular.md line 155 (loader migrates old `intensity` x100).
- Specular colour / IOR (v6) -- CORE, PORT FIRST (item 3). Includes the F82 metal tint (one function, ED:1789).
- Multiscatter + split-sum env -- CORE, PORT FIRST (needs a BRDF LUT; 256x256 RG16F, one fullscreen pass, ED:3764-3786; or the Lazarov analytic fit FO4CS uses, which needs no texture).
- Tint masks -- PORT FIRST (cheap). Special layer 0, RGBA masks, overlap Normalize/Add/Priority RGBA; base *= (1 - sum(masks)) + sum(colour_i * mask_i) (ED:2310-2316). Note: tint colours are passed RAW sRGB 0..1, not pow-2.2 decoded (ED:5265-5267) -- an editor inconsistency to copy or fix knowingly.
- EON diffuse roughness -- PORT SECOND (self-contained, ~10 lines, ED:1792 + 2680-2690; off when diffuseRoughness = 0).
- Clearcoat (3 layers, IOR, tint^path, darkening, anisotropy, own normal) -- DEFERRED. Second GGX lobe + LUT + coat special layer + emission transmittance (ED:2822-2859, 2919).
- Fuzz/sheen (Zeltner LTC) -- DEFERRED. Needs the 32x32 table src/fuzzltc_table.h and the SurfaceEffects layer (ED:2756-2771, 2920).
- Retroreflection -- DEFERRED. Key-light-only pow(V.L) lobe (ED:2744-2751); under NifSkope's camera-attached light it would glow constantly.
- Thin film / iridescence -- DEFERRED. A cosine palette, not a physical interference model (ED:1942-1945, 2639); multiplies F.
- Six-way lightmaps (the "six-way RGB" inputs) -- DEFERRED. Particle-effect smoke relighting, only when uParticleActive and shader mode >= 10 (ED:2719-2738).
- Also not core: anisotropy, bent normal, detail normal, specular AA, cavity spec occlusion, surface states (porosity), subsurface, hair, eye, parallax, landscape macro, decals, layers 2/3.

## 5. Preview lighting environment (what a pixel-match needs)

Defaults (QSettings Preview/*, ED:6545-6558; member defaults materialpreviewwidget.h:568-570, 491-499):
- Camera: perspective 45 deg vertical FOV, near .02, far 100 (ED:4624); orbit view = translate(pan.x, pan.y, -3.2) * rotX(pitch 0) * rotY(yaw 0) (ED:3822). So the eye is at (0,0,3.2) looking at the origin, Y-up. Default sphere = unit radius (previewmesh.cpp:151-166, UV v = 1 - v, tangent (-sin t, 0, cos t, +1)). MSAA 4.
- Key light: directional. dir = (sin(yaw)cos(pitch), sin(pitch), cos(yaw)cos(pitch)) (ED:4629-4633), yaw -35, pitch 45 -> (-0.4056, 0.7071, 0.5792). Intensity 4. Colour = 5500 K via temperatureColor (ED:6515-6521) = QColor(255,237,222), passed as raw 0..1 (not linearized, ED:5221). Shadow map on, intensity .75 (ED:4635-4660).
  - When an EXR environment is loaded and "derive EXR lighting" is on (default true, ED:6546), effectiveKeyIntensity() returns 0 (materialpreviewwidget.h:191): the key is replaced by the environment.
- Environment: the user's cubemap (QSettings PreviewCubemaps/activePath; FO4 DDS mip 0 only, or EXR), else a procedural gradient + two lobes (ED:3701-3708). Directions: rotateY(envYaw) then swizzle (x, z, -y) -> the cube is treated as Z-up like Bethesda cubes (ED:1742).
  - Diffuse IBL: NO SH, NO irradiance cube. 32-sample golden-spiral cosine integral of the SOURCE cube at mip (maxLod-2), per pixel (ED:1763-1783). Then times 0.45 (ED:2715).
  - Specular IBL: GGX prefiltered cube, <=256 px, mip = rough * maxLod, 256 or 1024 samples per level (ED:3717-3757); below rough .08 it blends toward the unfiltered source at LOD 0 (ED:2698-2701).
  - BRDF LUT: 256^2 RGBA16F, (NoV, rough), generated by m_brdfIntegrationProgram (ED:3764-3786).
  - uEnvironmentIntensity = 1 * material iblIntensity (ED:5190).
- Exposure + tonemap: c *= exp2(EV), EV default 0; operator default 2 = Reinhard c/(c+1) (materialpreviewwidget.h:493, ED:6050); options ACES(Narkowicz) / AgX / None; then pow(1/2.2) (ED:1749-1762). Output to an 8-bit non-sRGB target.
- Post: bloom ON (.4) on the TONEMAPPED image, bright-pass luma .72-.95 (ED:2976-2997); SSAO ON (.5) multiplying the final colour (ED:3003-3013). Both change pixels: a pixel-match must disable them in the editor or reproduce them.
- To reproduce a sphere pixel in NifSkope: unit sphere at origin, eye (0,0,3.2) Y-up (NifSkope is Z-up: swap axes), 45 deg FOV, key dir above * colour (1,.929,.871) * 4, NO ambient term other than the IBL, the same cubemap prefiltered by the same GGX convolution (or accept a documented error), the 32-tap irradiance * 0.45, the same LUT, Reinhard + 1/2.2, bloom/SSAO/shadows/ground off on the editor side.

## 6. Refraction and the particle renderer -- DEFERRED

- Refraction = shader modes Glass (3), Water (4), and composition mode 6 (ED:2868-2906), plus Screen Distortion (special layer 14, ED:2908-2913) and interior mapping (ED:2874-2890). None of it refracts the SCENE: it refracts into the preview's prefiltered environment cube (`refract(-V,N,1/ior)` -> textureLod(uPrefilteredEnvironment ...)), then REPLACES the lit colour and forces opacity. Why not port: NifSkope's forward GL scene has no prefiltered cube and no scene-colour copy; a port would show the sky through a bottle in a room, i.e. a different, wrong picture, and FO4CS runtime refraction is a separate system. Deferred.
- The "particle renderer" = the Effect shader modes (UnlitEffect / SimpleLitEffect / PhysicallyLitEffect, ED:335-339) drawn on the preview MESH, not an emitter: flipbooks, polar UVs, distortion/advection/erosion modifier layers, palette ramp, random hue, fresnel/near fade, six-way lightmaps (ED:2108-2159, 2240-2254, 2266-2281, 2326-2335, 2719-2738), driven by a single age/seed from a UI slider (setParticleSimulation, ED:988-993). Why not port: NifSkope draws NIF particle systems through its own legacy effect path with no per-particle age/seed uniform, and a per-mesh age would animate every particle in lockstep. Deferred.
- Also deferred: the path-traced reference (ED:3066+), ground/liquid passes (ED:2047-2078).

## 7. Diff: NifSkope pbrm_default.frag (NS) vs editor (ED)

Same already: D GGX (NS:83-88 = ED:1784); separable G1 with k=(r+1)^2/8 (NS:90-93,193 = ED:1785,2645); Lambert (1-F)(1-metal)base/PI (NS:198 = ED:2679); F at VoH; AO ambient-only; Z reconstructed, no green flip.

Differences, one line each:
1. F0 model: NS dielectric = pbrF0 scalar or RMAOS.A as F0 (v5, NS:162-170,189); ED v6 = weight * ((ior-1)/(ior+1))^2 * specColour tint (ED:2363-2372). On a v6 file NS reads weight as F0 (weight 1 -> mirror). BIGGEST.
2. Base colour decode: NS none (bc.rgb * pbrBaseColor, NS:126); ED pow(2.2) on texture and constant, * pow(tint,2.2) * colorScale (ED:2256-2257). Unless NifSkope binds BaseMap as an sRGB GL format (not visible in scope), NS lights gamma-space albedo.
3. Tonemap/output: NS Hable with NifSkope's A.a/D.a scene exposure and sqrt (gamma 2.0) (NS:103-115); ED exp2(EV) + Reinhard + pow(1/2.2) (ED:1749-1762).
4. Env specular: NS raw cube at LOD rough*8, * envReflection * f0 * ao, no LUT (NS:208-214); ED GGX-prefiltered cube * (f0*A + B) * multiscatter * specOcc * envIntensity * ao (ED:2698-2717). NS loses the grazing B term and scales by the legacy BGSM envReflection.
5. Env diffuse: NS flat A.rgb * base * (1-metal) * ao (NS:206); ED 32-tap cosine irradiance of the cube * 0.45 * (1 - Schlick(NoV,f0)) * (1-metal) * base * envIntensity * ao (ED:2713-2717).
6. Multiscatter energy compensation: NS none (NS:14); ED Kulla-Conty 1 + F_avg(1-E)/E on direct and env spec (ED:2647-2652, 2716).
7. Metal Fresnel: NS Schlick(base) (NS:189-190); ED F82 with specular-colour edge tint (ED:1789, 2643) -- identical when the tint is white.
8. Normal unpack: NS rg*2-1 (NS:149, 128 -> +0.0039 tilt); ED (s*255-128)/127 (ED:1747).
9. Normal strength: NS pbrNormalStrength only; ED strength * globalNormalStrength (ED:2212).
10. Back faces: NS flips the final normal (NS:155-156); ED flips the geometric normal before building the TBN (ED:2163-2166).
11. Roughness floor: NS .02 (NS:172); ED .035 (ED:2345). NoV floor NS 1e-7, ED .001.
12. Vertex colour: NS base *= C.rgb always (NS:133); ED lit path ignores vertex colour.
13. Emission: NS pbrEmissiveColor * intensity * em.rgb * em.a * glowScaleSRGB, no decode, texture multiplies the constant (NS:217-224); ED pow(colour,2.2) (texture REPLACES constant) * mask * luminance/100 (ED:545, 2519-2521). Whether NifSkope's reader divides luminance by 100 is outside this read (src/io/pbrmfile.h, owed).
14. Direct light units: NS (diff+spec) * D.rgb * NoL (NS:200); ED * keyColor * keyIntensity(4) * NoL * shadow (ED:2693-2695).
15. Opacity: NS uses base alpha only with F_OPACITY and the NIF alpha property (NS:127-143); ED base alpha unless overrideOpacity, composition mode opaque/test/premult (ED:2258, 2336-2337).
16. Diffuse model: NS Lambert only; ED EON when diffuseRoughness > 0 (ED:2680-2690).
17. Tint masks, spec colour, coat, fuzz, film, retro, anisotropy, bent normal, detail normal, specular AA, cavity occlusion, shadows: absent in NS (see item 4).
18. Neither side matches FO4CS wave 88: FO4CS uses height-correlated Vis, Burley direct diffuse, Lazarov analytic DFG, Turquin multiscatter and indirect diffuse *= 1 - E_spec (item 1). The editor is the law for NifSkope; the editor itself still owes the (1 - E_spec) split.
