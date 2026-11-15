// Lane VKFAR2 (2026-11-14, row L8 slice 1): rain / snow on the present device (vkprecip.inc precipRun: a compute pass
// writes one record a particle, then instanced quads, no geometry stage) against GL's own cell_precip_gpu program
// (vert / geom / frag, the tree's sources) drawn as GL_POINTS with cellprecip.cpp's state, per sample + stencil, into the
// same 4x MSAA RGBA16F + D24S8 frame contents. fxview.cpp's helpers are reused (its main renamed).
// Cases (precipview_prep.py): snow near with an RGBA8 atlas, rain near with no texture (the round disc), snow far (the
// cell's 16-tap mean at the one-texel mip), unfogged; slice 2a: rain near and snow far again through the air
// (frag_jobs.py's volsky inputs, the volume in a shared target), each checked to change GL's own picture on >= 1000
// samples (fogOn 0 vs on); slice 3: cell_precip_look.prog (the drop unfogged, the snow through the air), each checked to
// differ from cell_precip_gpu.prog on >= 1000 samples. Unlit, the snow's normal-map split cannot change a sample (thin =
// thick, no key light): its pcTexN binding is bound but not judged until a lit fixture (slice 2b). A kept split case,
// snow near with no atlas (plain, unfogged), sits next to the open snow near row. Each run starts past particle 0
// (pcFirst) and is unlit. Every atlas and normal map is the app's kind (cellprecip.cpp loadDds): BC3 with every level,
// trilinear, CLAMP_TO_EDGE (rain_near_fog sRGB BC3); the rain cases read it too (rain_drop_look binds it, its look
// reads no texture). The rain atlas's real DXGI format was not on disk: BC3 and sRGB BC3 are the formats judged.
// GL's programs carry the loader's WW_CELL_FOGLEAK (on by default), the record pass too; each fogged case prints a
// fogleak check (GL without the define vs with it, reference only, not part of the verdict).
// Frame before the particles: a sky clear and a depth slab over the left half halfway across a gap between particle
// depths. After them a full-screen draw where stencil == 1 sets alpha 1, so the stencil the particles wrote is read too.
// Bar (farview's, set before the first run, never widened):
//   covered = samples whose colour differs from the frame before on either side (must be >= 1000);
//   flips = samples with any half past 2 ulp; EXACT = no half differs; BOUND = flips <= 0.05 % of covered, halves past
//   1 ulp outside flips <= 0.1 % of the covered halves, stencil differs on <= 0.05 % of covered samples. Else FAIL.
// Also: pcSeedS's bits as fxPack reads them (glGetUniformiv on a uint) equal the value set, or the gate fails.
// Reds (each must FAIL in at least one case): red 1 the cull radius 0, red 2 the strip's last two corners swapped,
// red 4 the near fade dropped, red 8 the air dropped (A = 1, B = 0), red 16 a look run on the plain pipelines, red 32
// the atlas and normal map dropped (the zero texture; must FAIL in EVERY atlas case but rain_drop_look). Last line `PRECIPVIEW GATE OK` or `PRECIPVIEW GATE FAILED`.
// Localisation (plain cases, printed only, never part of the verdict): GL's geometry stage is captured twice by transform
// feedback (gl_Position, gUv, gQ, gView and the flat outputs; any float differing between the two captures = GL not
// deterministic before raster), and the Vulkan run is repeated with its records read back (PrecipRun::recOut) to diff
// per particle against the capture: ulp buckets per field, and which flipped pixels lie under a differing particle.
// Reference (coordinator ruling 11-14: Vulkan's per-sample blend is right; NVIDIA GL's 4x blend at times writes a pixel's
// mean to all its samples, a driver quirk not reproduced): per particle, in draw order, GL's own program drawn with blending
// off into a 4x RGBA32F target (alpha -1 = not written; a particle's two triangles cover a sample once), then blended per
// sample by GL's own blend unit at 1x (a W*S x H RGBA16F target, one texel a sample, SRC_ALPHA / ONE_MINUS_SRC_ALPHA). The
// stencil is GL's 4x one (not blended). Every verdict and red is judged against it; GL's 4x frame is an info line.
// Reference checks (bars set before the first run, each must read 0 halves): R1 a constant full-screen blend at 1x vs 4x;
// R2 the same loop with the 1x blend off vs one GL 4x draw with blending off; R3 the reference vs GL's 4x frame on pixels
// no particle ever split (a capture with 1-3 samples written, or 4 unequal), and on GL's draws 2-5 there too (added after
// the first run, E1: GL's own run-to-run variation must lie on split pixels). Info: GL's merged pixels (GL one value, the
// reference split), how many hold the reference's mean. The repeat clause's GL side is the reference built twice (EXACT)
// and GL's 4x stencil drawn twice (EXACT); GL's 4x colour drawn twice is info (its merge varies run to run). Red 64 (harness only): Vulkan's 4 samples averaged per pixel
// (the reveal's samples kept); must FAIL in every near case.
// Lit (slice 2b, plan L8 (12); bars set before the first lit run, never widened): the sun alone (volTerms 1; the lamps, GI,
// ambient, CSM, the drift, the terrain air, the local sun law and an interior are refused by precipRun, GL draws them),
// through precip_lit_view_cs. Cases rain_near_lit (rain_near_disc's run, unfogged) and snow_far_lit (snow_far_fog's run
// through the air, the veil on, the air's column on the sun): cellOn 1, cellHasDir 1, a non-grey sun (1.6, 1.2, 0.75) off
// the view axis, the phase (volPhase) and its isotropic tint (volGi) set, pcTrans 0.5. The same verdict, reference
// checks, repeat clause and red 64 as the unlit cases. Each lit case must also show (else the gate fails): GL lit vs GL
// with cellHasDir 0 differs on >= 1000 covered samples; GL with pcTrans 0.5 vs 1.0 differs on >= 1000 (thin vs thick);
// no NaN / Inf half in GL's lit frame; every name the sun branch reads is found in GL's program (packed, not zero);
// rain_near_lit's captured gA (unfogged: the light) holds >= 100 distinct particles with unequal channels. Red 128 the
// light dropped (pcRed 4: lit = 1); must FAIL in every lit case.
// Fixture fix after the first lit run (11-15, the bar kept): rain_near_lit's sun gave N.to -0.13 on its view axis, so
// thick / thin = 0.99 and pcTrans 1.0 vs 0.5 moved 51 samples (the case did not test the mix); it takes its own sun
// (precipview_prep.py LIT_SUN, N.to 0.72). The sun's inputs are also checked packed and non-zero in the light block.
// Globals check (11-15, bar set before its first run): precipRun zeroes globalUniforms (b0) and so does this harness for
// GL, so a sun-path read of it would pass EXACT here and be wrong in the app. Each lit case captures GL's geometry stage
// (transform feedback, bit-exact run to run) with b0 zero and with b0 filled non-zero: 0 floats may differ.
// Pack-order check (11-15, bar set before its first run): after the stat line, a lit run with volTerms 2 (a refused term)
// read and asked as precipDraw does (precipLitRead, precipAdmits, then the packs) must be refused with 0 packs on refused
// runs. Red: the same run packed first and handed to precipRun must be refused with its 3 packs counted (> 0), or FAIL.
// Fixture move (11-15, the bar kept): the probe's refused switch is volPartRed 1 (refused for good), not volTerms 2 (the
// lamps, lifted below).
// Lamps (L8 term 2a, 11-15; bar set before its first run, never widened): cases rain_near_lamps (rain_near_lit's run,
// unfogged) and snow_far_lamps (snow_far_lit's run through the air), volTerms 3 (the sun + the lamps), a cellLights texture
// buffer this harness uploads (RGBA32F, 8 texels a light, celllights.cpp's layout) with cellLightCount 5: an omni, a
// spot, a hemisphere, a box and one out of reach; cellShadowOn 0, cellGoboOn 0, cellClusterOn 0 (the flat loop),
// cellLightPhys 0 (the game's curve, the app's default), cellRed 0. Every lit case sets these switches (the sun cases
// cellLightCount 0), so the case order cannot carry them. precipRun twins GL's data store (farTexelBuffer). The same
// verdict, reference checks, repeat clause, reds 64 / 128 and globals check as the lit cases. Each lamp case must also
// show (else the gate fails): GL with volTerms 3 vs 1 differs on >= 1000 covered samples (the lamps seen); GL with
// cellLightCount k vs k - 1 differs on >= 1000 for k = 1..4 (each shape seen) and on 0 for k = 5 (out of reach; fixture
// move (bar kept), VKFAR5: every half of every pixel GL drew alike in both draws; a pixel whose 4x merge state moved
// between the two draws is NVIDIA's blend, not the light: pv_R2 0 / 0, pv_A1 8 flips / 32 halves, the same 8 / 32 as
// GL's own draw-to-draw noise on the case in both runs; printed as info); every
// name the lamp branch reads is found in GL's program; cellLightCount packed non-zero. Red 256 the lights dropped (Vulkan
// binds the zero texel buffer); must FAIL in every lamp case. Still refused (precipLitWhy): the lamps' shadow cubes,
// gobos, the physical falloff, cellRed, a light count past GL's buffer (the cluster lists: lifted in term 2b below).
// Cluster lists (L8 term 2b, 11-15; bar set before its first run, never widened): case rain_near_cluster (rain_near_lamps'
// run and five lights, unfogged), volTerms 3, cellClusterOn 1 (the app's default whenever a cell has lights:
// cellclustergl.cpp wwCellClusterUniforms), globalUniforms (b0) zero but for projectionMatrix (the case's own projection;
// vpLightsBegin reads it), volRect (0, 0, 1 / W, 1 / H), a 15 x 9 x 4 grid of 64-pixel tiles on log depth over the run's
// particles and a cellCluster texture buffer (R32I, as cellclustergl.cpp): a third of the clusters walk every light
// (count -1), a third list all five, a third list 0, 2, 4 (the spot and the box left out). GL gets the same b0 bytes the
// Vulkan run gets (precipRun takes GL's bytes; the app hands it glcontext.cpp's last upload). The same verdict, reference
// checks, repeat clause, reds 64 / 128 / 256 as the lamp cases, and the lamp check's volTerms 3 vs 1 (the per-shape and
// out-of-reach checks stay with the flat-loop lamp cases: a list's own count overrides cellLightCount). The globals
// check is inverted for this case: b0 filled non-zero vs the case's own b0 must move GL's geometry stage (> 0 floats).
// The case must also show (else the gate fails): GL with cellClusterOn 0 differs on >= 1000 covered samples (the lists
// seen); GL with b0 all zero differs on >= 1000 (the projection read); every name the cluster branch reads is found in
// GL's program; cellClusterDims, cellClusterDepth, cellClusterLog, cellClusterBase and volRect packed non-zero. Red 512
// b0 dropped (Vulkan's globalUniforms zero) and red 1024 the lists dropped (the zero texel buffer at cellCluster): each
// must FAIL in the cluster case. Lifted with it: precipLitWhy's cluster refusal; precipAdmits refuses a cluster run with
// no b0 bytes, precipRun one whose list buffer is not R32I or shorter than cellClusterBase. Not covered here: the
// lights' radial falloff past (0, 1, 1) (every fixture light keeps it; the twin runs GL's own cellRadial).
// Vulkan's globals mirror (11-15, bar set before its first run): precipDraw hands every lit run the app's b0, so each
// lit case but the cluster one also runs Vulkan with the globals check's non-zero b0 as its bytes; it must be EXACT
// against the judged run (b0 read by nothing but the cluster), else the gate fails.
// Census (11-15, info, the L8 done line): the lit program set as the app's all-on defaults (exterior: the sun, volTerms
// 15, volCsm 1, airTerrOn 1, the drift on, vpSun's local law, the lamps' shadow cubes, gobos and cluster lists; interior:
// the same but no sun, no terrain air, no local law), every refusal precipLitWhys would fire printed. Done is 0 in both.
// Local sun (L8 term 3, 11-15; bar set before its first run, never widened): case rain_near_sunlocal (rain_near_lit's run,
// unfogged, volTerms 1) with vpSun (0.9, 1.3, 1.7, 1): the local law outdoors (celllights.cpp fxSunLocal, on by default),
// the sun's colour from vpSun, not cellDirColor (1.6, 1.2, 0.75). Every other lit case sets vpSun 0 (GL keeps a
// program's uniforms). The same verdict, reference checks, repeat clause, reds 64 / 128, the globals check and Vulkan's
// globals mirror as rain_near_lit. The case must also show (else the gate fails): GL with vpSun.w 0 differs on >= 1000
// covered samples (the law seen); vpSun found in GL's program; vpSun packed non-zero. Red 2048 the law dropped (Vulkan's
// packed vpSun.w 0, so cellDirColor lights it) must FAIL in the case. Lifted with it: precipLitWhy's local-sun refusal.
// Fixture move after the first run (11-15, the bar kept): vpSun (0.9, 1.3, 1.7) had cellDirColor's luminance (1.244 vs
// 1.252) and the air colours the sky terms by luminance (airEnvColour), so the law moved 8 samples; vpSun is now
// (0.5, 0.4, 0.3, 1), luminance 0.414.
// Terrain air (L8, 11-15; bar set before its first run, never widened): case snow_far_terrair (snow_far_lit's run, fogged,
// volTerms 1) with airTerrOn 1: the ground layer stands on the terrain grid (cell_volpart vpLight airGroundAt, on by default
// with the air: cellvolair.cpp 4219). airTerr is the air's own volAirTerr grid (the app binds its pass grid to both
// names), airTerrXf / airTerrFineXf its world rows, airTerrRef chosen before the run so the layer's anchor straddles the
// run's particles (the prep prints the share under it; want 0.2 - 0.8). The same verdict, reference checks, repeat
// clause, reds 64 / 128, the globals check and Vulkan's globals mirror as snow_far_lit. The case must also show (else the
// gate fails): GL with airTerrOn 0 differs on >= 1000 covered samples (the grid seen); airTerr, airTerrOn, airTerrXf,
// airTerrFineXf and airTerrRef found in GL's program; airTerrOn and airTerrXf packed non-zero. Red 4096 the grid dropped
// (Vulkan's airTerr the zero texture) must FAIL in the case. Lifted with it: precipLitWhy's terrain-air refusal; the
// app's rain / snow draw twins GL's airTerr (t17) into the lit set.
// Census roof line (11-15, info): a cell with a roof map turns pcRoofOn on (cellprecip.cpp gpuRoof), refused for every
// run, lit or not (precipAdmits, slice 3's); the census prints it beside the lit refusals and counts it.
// GI (L8 term 1, VKFAR5 11-15; bar set before its first run, never widened): the GI term (volTerms 4) reads the grid only
// when cellGiOn, and cell_lights compiles cellGiOn as the constant false unless the loader's WW_CELL_PRT (the old baked
// probe GI's red, WW_CELL_PRT_RED=1) is in the program; the default program and the lit twin hold no GI read. Case
// rain_near_gi (rain_near_lit's run, unfogged) with volTerms 5 (the sun + GI). The same verdict, reference checks, repeat
// clause, reds 64 / 128, the globals check and Vulkan's globals mirror as rain_near_lit. The case must also show (else
// the gate fails): none of cellGiOn, cellGi, cellGiSlots, cellRooms, cellGiAir, cellGiAirOn, cellDdgiOn, cellDdgiAtlas
// active in GL's default program; GL's captured geometry stage with volTerms 5 vs 1: 0 floats differ; the loader's RT GI
// variant (WW_CELL_RTDDGI, the ray-traced GI on) captured with volTerms 5 against the default program: 0 floats differ,
// and none of those names active in it; the WW_CELL_PRT variant with its GI fed (cellGiOn 1, cellGiAirOn 1, a filled
// cellGiAir) holds cellGiOn and cellGiAir and differs from GL's default frame on >= 1000 covered samples (the GI seen);
// precipAdmits refuses the WW_CELL_PRT variant's lit run (a program that reads a GI volume), 0 packs. Red 8192 that
// refusal's input dropped (precipLitRead's GI count forced 0, the run goes on to Vulkan): judged against the WW_CELL_PRT
// variant's frame it must FAIL with >= 1000 flips. Lifted with it: precipLitWhy's volTerms 4 refusal, replaced by the
// program refusal (a GL program with a GI read stays on GL).
// Known start state (VKFAR5 11-15, coordinator ruling; set before its first run, never widened): every GL program's loose
// uniforms are read right after it links (sampler units included in the snapshot's skip: those stay as set) and written
// back at the top of every bindJob, so each case starts from the linked state and sets what it needs on top; before,
// a program kept the uniforms an earlier case set and this case does not name (the fogged cases' air values on the
// unfogged lit cases). Every case reruns; a changed verdict is reported with its numbers, never absorbed. Red (a check
// on rain_near_lit, not a red bit): GL drawn with the restore skipped and a fogged case's uniforms left on the program
// must differ from rain_near_lit's judged GL frame on >= 1000 covered samples (the restore matters), else the gate fails.
// Ambient (L8 term 2, VKFAR5 11-15; bar set before its first run, never widened): the ambient term (volTerms 8) adds the
// weather's ambient, six axes of cellAmbient (pow( max( cellDalc rows . ( n, 1 ), 0 ), 2.2 ) x cellAmboScale) scaled by
// vpAmbKeep, the share the air's env history keeps (volEnvHist, t19 / lit binding 83, read only when volEnvGrid.w >= 0.5,
// through b0's projectionMatrix and volSlices; the app turns it on only with the air: cellvolair.cpp volEnvGrid). Two cases,
// volTerms 9 (the sun + the ambient), cellExtDalc 1 (the exterior's R7-1 law), cellDalc three non-grey rows with non-zero
// normal parts (the six axes differ): rain_near_amb (rain_near_lit's run, unfogged, volEnvGrid.w 0: keep 1) with
// cellAmboCount 2, an Ambient Only sphere and a box-linked one (cellAmboK.w 1, cellAmboBox rows) each over part of the
// run's particles, unequal scales; snow_far_amb (snow_far_lit's run, fogged) with volEnvGrid (16, 9, the air's slices,
// 1), a 16 x 9 x 6 slices RGBA16F volEnvHist (the app's hist format and strata) whose stratum 4 .a varies smoothly and
// whose stratum 5 holds .z exactly 1 on most texels (some 0.5: keep 1 there) and .y both negative and in 0..1, and b0
// zero but for projectionMatrix (the case's own; vpAmbKeep reads it). The prep prints the share of the keep case's
// in-view particles with keep < 1 (want 0.2 - 0.8). The same verdict, reference checks, repeat clause, reds 64 / 128 as
// the lit cases; the globals check and Vulkan's globals mirror as rain_near_lit for rain_near_amb, and inverted for
// snow_far_amb as for the cluster case (b0 filled non-zero vs the case's own must move GL's geometry stage, > 0 floats;
// no mirror run). Each ambient case must also show (else the gate fails): GL with volTerms 1 differs on >= 1000 covered
// samples (the ambient seen); cellDalc, cellExtDalc, cellAmboCount, cellAmbo, cellAmboK, cellAmboBox, volEnvGrid and
// volEnvHist found in GL's program; cellDalc and cellExtDalc packed non-zero. rain_near_amb also: GL with cellAmboCount 0
// differs on >= 1000 (the volumes seen; VKFAR5 at the lift, stricter, bar kept: also cellAmboCount 1 vs the case, the box
// seen alone, and cellAmboCount 0 vs 1, the sphere seen alone, each >= 1000; fixture move (bar kept): pv_A1's box held
// 0.032 of the in-view particles, so the prep grows its half extents, percentile 40 up by 10, until >= 0.15). snow_far_amb also: GL with volEnvGrid.w 0 differs on >= 1000 (the keep seen), GL
// with b0 all zero on >= 1000 (the projection read); volEnvGrid packed non-zero. Red 16384 the history dropped (Vulkan's
// volEnvHist the zero texture) must FAIL in snow_far_amb; red 32768 the ambient dropped (Vulkan's packed volTerms bit 8
// cleared) must FAIL in both; red 512 (b0 zero) must FAIL in snow_far_amb too. Lifted with it: precipLitWhy's volTerms 8
// refusal; precipAdmits (and precipRun's backstop) refuse an ambient run with the keep on (volEnvGrid.w >= 0.5) and no
// b0 bytes; the app's rain / snow draw twins GL's volEnvHist into the lit set's binding 83. Census after: exterior 5,
// interior 6 (the ambient line gone from both).
// The sun's shadow (L8 term 3, VKFAR6 11-15; bar set before its first run, never widened): volCsm 1 multiplies the sun by
// wwSunShadow = the cascades (csmMap t31, a D16 array read by compare taps) x the clouds' transmittance (ussCloudT,
// ussCloudMap t34), outdoors only. The app's defaults on this program: ussOn 0 (shadowset.cpp 2757: the tiers 0 and the
// cascades valid), ussCsmStable.w 1 (sunshadow.cpp 2363: the Off tier's stable bias, the normal offset from the geometry
// stage's pcDPx; the program has no volStage), ussCloudOn 1 (cloudshadow.cpp 108: the cloud shadow follows the clouds, at
// least Low). One case, rain_near_csm (rain_near_lit's run, unfogged, volTerms 1): volCsm 1, ussOn 0, csmForce -1, csmRed
// 0, csmProbe 0, ussCsmStable (1, 0, 0, 1) with ussCsmTexel and ussCsmBias (0.25 texel) per cascade, csmParams (1, a
// distance past the run, 256, a blend band), csmSplitOffset at the run's view-depth thirds, csmMat[3] orthographic along
// the case's sun (each cascade fitted to its share of the particles), csmInvRange 1 / each depth span; a 256 x 256 x 3 D16
// csmMap made by sharedTarget (type 2) as the app's, with csmTexParams' live state (linear, clamp to border 1, compare
// LEQUAL), each layer a different stripe pattern of occluder depths; ussCloudOn 1 with ussCloudFromView along the sun,
// ussCloudFar (0.6, 0.15, 0, 0) and a 64 x 64 RGBA16F ussCloudMap (linear, clamp to edge) whose start height, extinction and
// depth vary. The prep prints the in-view particles' shares in cascades 0, 1 and 2 (want each >= 0.1) and the share one
// hard tap shadows (want 0.2 - 0.8). The same verdict, reference checks, repeat clause, reds 64 / 128 as the lit cases; the
// globals check and Vulkan's globals mirror as rain_near_lit. The case must also show (else the gate fails): GL with
// volCsm 0 differs on >= 1000 covered samples (the sun's shadow seen); GL with csmForce 1 on >= 1000 (the map seen); GL
// with ussCloudOn 0 on >= 1000 (the clouds seen); csmMap, csmMat, csmInvRange, csmParams, csmSplitOffset, csmForce,
// ussCsmStable, ussCsmTexel, ussCsmBias, ussCloudOn, ussCloudMap, ussCloudFromView and ussCloudFar found in GL's program;
// csmMat, csmParams, csmSplitOffset, ussCsmBias, ussCloudFromView and ussCloudFar packed non-zero. GL with ussCsmStable.w
// 0 is printed (info). Reds, each must FAIL in rain_near_csm: 65536 the shadow map dropped (Vulkan's csmMap the zero D16
// array, all lit); 131072 the shadow dropped (the packed volCsm cleared); 262144 the clouds' map dropped (the zero
// texture); 524288 cascades 1 and 2 swapped (the packed csmMat[1] and csmMat[2]). Lifted with it: precipLitWhy's volCsm
// refusal; refused by name instead: ussOn (the shadow set's composition: a cascade / terrain / contact tier), csmProbe or
// csmRed non-zero; precipRun refuses a csmMap not shared with the present device (no per-run copy of the map); the app's
// rain / snow draw twins GL's csmMap (refused unless linear, clamp to border 1, compare LEQUAL) into the lit set's binding
// 95 with a compare sampler of that state and ussCloudMap into binding 98. The census sets ussOn 0 and ussCloudOn 1 (the
// lines above). Census after: exterior 4, interior 5 (the sun's shadow map line gone from both).
// The air's drift (L8 term 4, VKFAR6 11-15; bar set before its first run, never widened): volAirDrift.x > 0 makes the air
// lumpy where the particles are lit: the sun's column takes airTupSun to the power wwAirDriftSunK (the drift's mean along
// the sun) and each lamp's path exp( -airOd x dL ) (the drift at the puff and along the path), both reading volAirNoise
// (t30, a 3D R8 noise, linear, repeat; cellvolair.cpp vaNoiseLoad) at volAirDriftUv's two octaves, faded by volAirDrift.y
// / .z, centred on its mean .w. One case, snow_far_drift (snow_far_lamps' run, fogged, volTerms 3): volAirDrift (0.5,
// the in-view particles' median distance, 1 / their distance span, the noise's mean), volAirDriftUv two skewed octaves
// (about 2 and 5 noise tiles across the run), volAirDriftRed 0, a 32^3 R8 volAirNoise (linear, repeat) bound in every case
// (unread with the drift off). The same verdict, reference checks, repeat clause, reds 64 / 128 as the lit cases; the
// globals check and Vulkan's globals mirror as snow_far_lamps. The case must also show (else the gate fails): GL with
// volAirDrift.x 0 differs on >= 1000 covered samples (the drift seen); GL with volAirDriftUv all zero on >= 1000 (the
// noise's variation seen); volAirDrift, volAirDriftUv, volAirDriftRed and volAirNoise found in GL's program; volAirDrift
// and volAirDriftUv packed non-zero. GL with volAirDriftRed 1 (one sun tap) and 2 (the lamps' drift at the puff only) is
// printed (info). Reds, each must FAIL in snow_far_drift: 1048576 the noise dropped (Vulkan's volAirNoise the zero 3D
// texture); 2097152 the drift dropped (the packed volAirDrift.x 0); 4194304 the octaves swapped (the packed
// volAirDriftUv rows 0-2 with 3-5). Lifted with it: precipLitWhy's drift refusal; precipRun refuses a lit run with the
// drift on and no volAirNoise; the app's rain / snow draw twins GL's volAirNoise into the lit set's binding 94 with its
// own sampler (binding 158). Census after: exterior 3, interior 4 (the drift line gone from both).
// The lamps' shadow cubes (L8 term 5, VKFAR6 11-15; bar set before its first run, never widened): cellShadowOn 1 makes
// each casting lamp (t4.y > 0.5) dim the particles by cellShadowF: a hemisphere (t4.y 2) is 0 behind its plane; a slot
// (t4.x >= 0) takes 9 compare taps a texel apart (cellShadowTexel) into cellShadow (t6, GL's D16 cube array, linear,
// clamp to edge, compare LEQUAL, seamless), its ref (|P - pos + slope bias| - 1) / radius; cellShadowFadeOn blends each
// slot toward 1 by cellShadowFade[slot]; a casting lamp past the slots (t4.x -1) is unshadowed. One case,
// rain_near_shadow (rain_near_lamps' run, volTerms 3): cellShadowOn 1, cellShadowTexel 1 / 64, cellShadowFadeOn 1 with
// cellShadowFade 0.7 / 0.4 / 1.0 on slots 0 / 1 / 2; the omni in slot 0, the spot in slot 1, the hemisphere in slot 2
// (t4.y 2), the box casting past the slots (t4.x -1), the far omni not casting; a shared 64 x 64 x 16-cube D16 array
// (the app's slot count) made by sharedTarget as the app's (cube type), each used slot holding stripes of near
// occluders (depth 0.05) across the directions (else 1), so taps fall on stripe and face edges; GL with
// GL_TEXTURE_CUBE_MAP_SEAMLESS on, as the app. The prep prints each slot's share of in-view particles its stripes
// shadow (predicted before the run). The same verdict, reference checks, repeat clause, reds 64 / 128 as the lit cases;
// the globals check and Vulkan's globals mirror as rain_near_lamps. The case must also show (else the gate fails): GL
// with cellShadowOn 0 differs on >= 1000 covered samples (the cubes seen); GL with cellShadowFadeOn 0 on >= 1000 (the
// fade seen); cellShadow, cellShadowOn, cellShadowTexel, cellShadowFadeOn and cellShadowFade found in GL's program;
// cellShadowOn, cellShadowTexel and cellShadowFade packed non-zero. GL with cellShadowTexel 0 is printed (info). Reds,
// each must FAIL in rain_near_shadow: 8388608 the cubes dropped (Vulkan's cellShadow the zero D16 cube array);
// 16777216 the shadows off (the packed cellShadowOn 0); 33554432 the fade dropped (the packed cellShadowFadeOn 0).
// Lifted with it: precipLitWhy's shadow-cube refusal; makeShared / sharedTarget gain a D16 cube array (type 3), the
// app's lamp cubes (celllamps.cpp) become one while the view is on Vulkan; precipRun refuses a lit run with shadows on
// and cubes not shared with the present device (no per-run copy), a D16 cube array without linear filtering, and GL's
// ReSTIR variant (cellRestirOn in the program: its cubes are not read); the app's rain / snow draw twins GL's
// cellShadow (refused unless D16, linear, clamp to edge, compare LEQUAL) into the lit set's binding 70 with a compare
// sampler of that state (binding 134). Census after: exterior 2, interior 3 (the shadow-cube line gone from both).
// The lamps' gobos (L8 term 6, VKFAR6 11-15; bar set before its first run, never widened): cellGoboOn 1 tints each lamp
// by cellGobo: a spot by its pattern through t5 / t6 (local y, 1 + header; local z, tan( FOV / 2 )), any other shape by
// a dual paraboloid through the table at cellGoboTbl (1 + the light's record: local x + 1 + header, y, z), each pattern
// a header (w, h), its mean, then rows, bilinear by hand, all in cellLights (no texture: the whole store is copied, so
// the table and patterns travel with the lights). One case, rain_near_gobo (rain_near_lamps' run, volTerms 3):
// cellGoboOn 1, Rot 0, Flat 0, Gam 0, Map 0; the spot an 8 x 8 pattern, the omni and the hemisphere an 8 x 16 pattern
// (two stacked halves) each in its own frame, the box and the far omni none (table entry 0); cellLights grown to hold
// the table at texel 40, the records, the patterns (random unequal-channel texels 0.2 - 1.5). The prep prints each
// gobo light's in-reach particle count (predicted before the run). The same verdict, reference checks, repeat clause,
// reds 64 / 128 as the lit cases; the globals check and Vulkan's globals mirror as rain_near_lamps. The case must also
// show (else the gate fails): GL with cellGoboOn 0 differs on >= 1000 covered samples (the gobos seen); GL with
// cellGoboFlat 1 on >= 1000 (the patterns' texels seen, not just their means); GL with cellGoboRot 1 on >= 1000 (the
// uv seen); cellGoboOn, cellGoboRot, cellGoboFlat, cellGoboGam, cellGoboTbl and cellGoboMap found in GL's program;
// cellGoboOn and cellGoboTbl packed non-zero. GL with cellGoboMap 1 is printed (info). Reds, each must FAIL in
// rain_near_gobo: 67108864 the gobos off (the packed cellGoboOn 0); 134217728 the table dropped (the packed cellGoboTbl
// 0: the omni's and the hemisphere's patterns gone); 268435456 the uv flipped (the packed cellGoboRot 3). Lifted with
// it: precipLitWhy's gobo refusal. Census after: exterior 1, interior 2 (the gobo line gone from both).
// The roof map (L8 term 7, VKFAR6 11-15; bar set before its first run, never widened): pcRoofOn 1 drops a particle under
// the map's roof (cell_precip_field's pcRoofOpen: the texel floor( pcRoofCam.xy + rel.xy pcRoofInv ), off the map open,
// else open when the texel's z - pcRoofCam.z < rel.z), a cull: the particle set changes, no colour term. Three cases, one
// a record pass: rain_near_roof (plain, rain_near_disc's run), snow_look_roof (the look, snow_look_near's run; the drop
// look runs the same record pass, precip_look_view_cs), rain_near_roof_lit (lit, rain_near_lit's run, volTerms 1). Each
// pcRoofOn 1 with one fixture, R32F 24 x 16 (nx != ny), NEAREST, clamp to edge (cellprecip.cpp's state), the texel a
// sixteenth of the box side, the eye at texel (6.3, 11.7) so the map covers part of the box and the off-map branch is
// read (low x, high y); a quarter of the texels open (-1e30), the rest a roof at the eye's z + uniform( -L / 2, L / 2 ) (roofs above and
// below particles). The prep prints each case's in-view particles and how many its CPU particles put under a roof
// (predicted before the run). The same verdict (depth EXACT, colour inside the bar), reference checks and repeat clause
// as each case's run. The case must also show (else the gate fails): GL with pcRoofOn 0 differs on >= 1000 covered
// samples (the roof seen); pcRoof, pcRoofOn, pcRoofCam and pcRoofInv found in GL's program; pcRoofOn packed non-zero.
// Reds, each must FAIL in each roof case: 536870912 the map dropped (Vulkan's pcRoof the zero texture); 1073741824 the
// roof off (the packed pcRoofOn 0); 2147483648 the map's axes swapped (the packed pcRoofCam x and y swapped). A mismatch
// on a texel edge is a ruling or a Vulkan twin in GL's order, never a wider bar. Lifted with it: precipAdmits' roof
// refusal (every run); the lit record pass reads pcRoof at its own register (t47: t0 is cellLights there); a run with
// pcRoofOn and no pcRoof in GL's program is refused by name; the app's rain / snow draw twins GL's pcRoof (copied every
// run: the map changes with the cell). Census after: exterior 0, interior 1 (the roof line gone from both).
// Interiors (L8 term 8, VKFAR6 11-15; bar set before its first run, never widened): cellInterior 1 changes the particle
// law in cell_volpart (vpLight / vpLit): the local sun law off (vpLocal), the sun unshadowed (no volCsm read), the veil
// off, the ambient's switch cellHasDalc (not cellExtDalc), the ambient dropped under cellGiAmb 2, GI's replace by
// cellGiAmb 1, and a dark interior (cellHasDir 0) still lit (not cellFxLit's self-lit). Two cases, element 30, appended:
// rain_near_int (1: a dark interior, rain_near_amb's run with the lamps, volTerms 11: cellHasDir 0, the lamps' flat loop,
// the Ambient Only volumes; GI's bit is rain_near_gi's) and snow_far_int_sun (2: a sunlit interior, snow_far_lamps' fogged
// run, volTerms 11, the sun, the lamps, the volumes, the veil's switches on). Each: cellInterior 1, cellHasDalc 1, cellGiAmb 0 and
// cellExtDalc 1 (the ambient's fixture; unread inside). The same verdict (depth EXACT, colour inside the bar), reference
// checks, repeat clause and lamp / ambient checks as each case's run; rain_near_int's lit check "GL with cellHasDir 0" is
// printed, not judged (its sun is already off: the dark interior is the cellInterior check below). The case must also
// show (else the gate fails): GL with cellInterior 0 differs on >= 1000 covered samples (the interior's law seen); GL
// with cellHasDalc 0 on >= 1000 (the interior's ambient switch seen); GL with cellGiAmb 2 on >= 1000 (the ambient's GI
// rule seen); cellInterior, cellHasDalc, cellGiAmb and cellExtDalc found in GL's program; cellInterior and cellHasDalc
// packed non-zero. GL with cellExtDalc 0 and, fogged, volVeil 0 are printed (info: unread inside, want 0). Reds, each
// must FAIL in each interior case: 4294967296 the interior off (the packed cellInterior 0); 8589934592 the interior's
// ambient off (the packed cellHasDalc 0); 17179869184 the ambient's GI rule (the packed cellGiAmb 2). Lifted with it:
// precipLitWhys' interior refusal. Census after: exterior 0, interior 0 (the L8 done line).
// The route census (L8, VKFAR6 11-15, manager ruling: Vulkan covers both routes; bar set before its first run, never
// widened): the census above reads pPrecip (WW_CELL_FOGLEAK alone), a program the app never links. It also reads the
// loader's own programs (glcontext.cpp Shader::load, each define only where the expanded source names it): route default
// = WW_CELL_FOGLEAK + WW_FRAME_UBO + WW_CELL_AIRZONE (the shipped rows: WW/CellAirZones2 on, the frame block on);
// route all-on = default + WW_CELL_RTDDGI + WW_CELL_RTDDGI_CASCADES + WW_CELL_RESTIR (WW_ALL_ON: RTDDGI 1, preset ultra,
// 4 cascades). Each route, exterior and interior, sets the census' switches; all-on also its own values: ussOn 1 (the
// cascade / contact / terrain tiers ultra, shadowset.cpp 2754), cellLightPhys.x 1 (WW_CELL_LIGHT_PHYS 1, lightphys.h
// Physical), cellRestirOn 1 (rtddgicell.cpp restirUniforms), cellShadowOn 1 (WW_CELL_LAMPSHADOW ultra). Each prints its
// precipLitWhys refusals and a closed list: every active uniform outside a block and every active uniform block of GL's
// program that no kT_ table, sampler slot, precipLitRead name or the read block globalUniforms covers, one line each
// ("GL input the twin does not read"); each line counts in the route's total. The closed list on pPrecip must read 0
// (else the gate fails: the allow list is checked on the program every earlier term judged). Info, outside the done
// count: route all-on + WW_CELL_RESTIRGI (a row off by default, not in WW_ALL_ON; a ruling is asked). Red, must FAIL:
// the old census (pPrecip, refusals only, exterior + interior) against route default's total; it PASSES (the gate
// fails) when the old reading is not below it. Lifted with it (the product): precipLitWhys refuses a lit run whose
// cascades GL reads from a block (volCsm on outdoors, wwShadowFrame in GL's program: fxPack would pack zero cascades),
// so the app draws those runs on GL until the block is read. The old census lines stay byte-identical (verdict keys).
// Done when both routes read 0 / 0.
// The frame block (L8 route lift 1, VKFAR6 11-15; bar set before its first run, never widened): under WW_FRAME_UBO (the
// default route) GL reads csmMat[3], csmInvRange, csmForce, csmParams, csmSplitOffset, csmProbe and csmRed from the std140
// block wwShadowFrame (ww_sunshadow.glsl; sunshadow.cpp csmFrameUpload binds it at 2), not from loose uniforms. One case,
// appended, element 31: rain_near_csm_ubo (rain_near_csm's run and fixture, drawn and read through pRoute, the route default
// program; the gate uploads the case's cascade values into a block at binding 2, the layout of sunshadow.cpp's
// CsmFrameBlock). The same verdict (depth EXACT, colour inside the bar), reference checks, repeat clause and term 3's sun's
// shadow checks as rain_near_csm (GL with csmForce 1 writes the block's csmForce; a block member counts as found when the
// program holds wwShadowFrame). The case must also show (else the gate fails): GL with the block's bytes zero differs on
// >= 1000 covered samples (the block seen); wwShadowFrame found in GL's program and csmMat not a loose uniform there;
// precipLitRead read the block's bytes (csmBlock 1). Reds, each must FAIL in rain_near_csm_ubo: 34359738368 the block's
// bytes dropped (the packed cascade fields zero: the pack before the lift); 524288 (cascades 1 and 2 swapped) as in
// rain_near_csm. Lifted with it: precipLitRead reads the bound block's bytes (its binding, offset and size as GL holds
// them) and the lit pack takes the cascade fields from them; the block refusal stays for a block with no bytes read (no
// buffer bound, or a short one); the closed list counts wwShadowFrame as read. Census after: route default 2 / 2, route
// all-on 7 / 7 (the refusal and the block line gone outdoors, the block line indoors).
// The shadow set's composition (L8 route lift 2, VKFAR9 11-15; bar set before its first run, never widened): ussOn 1 (route
// all-on: the cascade, terrain and contact tiers ultra, shadowset.cpp 2754) makes wwSunShadowGeo return ussCompose = min(
// lerp( 1, the Vogel cascades, their fade ), the terrain tier, the contact mask ) in place of CSM1's blend. The terrain tier
// reads ussTerrMap (t32, RG16, linear, clamp to edge: the heights above which a point is lit / below which it is shadowed),
// the contact tier ussContactMap (t33, R8, linear, clamp to edge: the screen mask at the particle's window position). One
// case, appended, element 32: rain_near_uss (rain_near_csm_ubo's run, fixture, frame block and route program pRoute) with
// ussOn 1 and the app's ultra values: ussCsmOn 1, ussCsmFilter (1, 3.5, 44, 1) (preset 4's Vogel radius and taps, the
// receiver plane's bias), ussCsmWorld (1, 2.8, 10.5, 42), ussCsmPhase 17, ussCsmCellX / ussCsmCellY per cascade; ussTerrOn 1
// with a 128 x 128 RG16 ussTerrMap (the far tier alone: ussTerrSize (128, 0), ussTerrExtra 0, ussTerrSelf 0) whose heights
// straddle the in-view particles' world heights, ussTerrXform, ussTerrRange (the edge fade a circle round the eye) and
// ussWorldFromView from the case's view, ussTerrPen (1, the texel, tan lo, tan hi), ussTerrBias (1, sin elevation, 1, 0);
// ussContactOn 1 with a w x h R8 ussContactMap (soft blobs) and ussContactInvSize (1 / w, 1 / h); ussView 0, ussRed 0. The
// prep prints the in-view particles' shares under the hard terrain lookup < 1 and the contact mask < 1 (each want 0.2 -
// 0.8). The same verdict (depth EXACT, colour inside the bar), reference checks, repeat clause, sun's shadow and frame block
// checks as rain_near_csm_ubo. The case must also show (else the gate fails): GL with ussOn 0 differs on >= 1000 covered
// samples (the composition seen); GL with ussTerrOn 0 on >= 1000 (the terrain tier seen through the min); GL with
// ussContactOn 0 on >= 1000 (the contact tier seen through the min); ussOn, ussTerrOn, ussTerrMap, ussTerrXform,
// ussTerrRange, ussWorldFromView, ussContactOn, ussContactMap and ussContactInvSize found in GL's program; ussOn, ussTerrOn
// and ussContactOn packed non-zero. Reds, each must FAIL in rain_near_uss: 68719476736 the terrain map dropped (t32 the zero
// texture); 137438953472 the contact map dropped (t33 the zero texture); 274877906944 the composition off (the packed ussOn
// 0: CSM1's blend); 549755813888 the contact lookup's axes swapped (the packed ussContactInvSize x / y). Lifted with it:
// precipLitWhys' ussOn refusal and the packed block's backstop; refused by name instead: ussView or ussRed non-zero (a
// debug view, a red), a terrain / contact tier on with no texture on GL's unit; the app's rain / snow draw twins GL's
// ussTerrMap into binding 96 and ussContactMap into binding 97 (copied a run, GL's sampler state); the closed list counts
// both as sampler slots. Census after: route default 0 / 0, route all-on 4 / 4 (refusal 3 and both map lines gone); the
// closed list on pPrecip reads 0 (its clause passes). Amended before lift 2's first run (VKFAR10 11-15): route default 0 / 0
// leaves the old census red nothing to under-read (old 0 >= route 0 would read PASSED); with route default at 0 the red is the
// closed list itself: ussTerrMap dropped from the allow list, pRoute's closed list must read >= 1 (FAILS as it must). Above 0
// the old census red stays as it was.
// The lamps' shadows in GL's ReSTIR variant (L8 route lift 3, VKFAR11 11-15; bar set before its first run, never widened):
// the all-on route links cell_precip_gpu with WW_CELL_RESTIR, where cellShadowF gives every casting lamp (t4.y > 0.5,
// cellShadowOn 1; the hemisphere's plane test first) the pixel's traced ratio in place of its cube: cellRestirOn 1 reads
// cellRestir (t7, RG32F texel buffer: ratio, linear depth; bottom row first) at the particle centre's window position q,
// cellRestirSize (W, H) its size; q outside it -> 1; the particle's linear depth D (from cellRow) within 0.02 rr.y + 2 of
// rr.y -> rr.x, else 1 (no CELL_RESTIR_MISS_PIXEL in the particles' program); no cube is read (cellShadow compiled out). One
// case, appended, element 33: rain_near_restir (rain_near_shadow's run, lamps, slots and cubes, volTerms 3) drawn through
// pAllOn (the route all-on program) with cellRestirOn 1, cellRestirSize (w, h, 0, 0) of the gate's frame and a w x h RG32F
// cellRestir whose depth is one D0 for every texel (the prep picks the in-view particles' median depth and prints the share
// of in-view particles within its tolerance: want 0.1 - 0.8) and whose ratio is a smooth field in 0.1 - 0.9 (two low
// sines, no texel-sized steps). The same verdict (depth EXACT, colour inside the bar), reference checks, repeat clause, reds
// 64 / 128 as the lit cases; the globals check and Vulkan's globals mirror as rain_near_shadow. The case must also show
// (else the gate fails): GL with cellRestirOn 0 differs on >= 1000 covered samples (the ratio seen); cellRestirOn,
// cellRestir and cellRestirSize found in GL's program and cellShadow not (the cubes compiled out); cellRestirOn and
// cellRestirSize packed non-zero. Reds, each must FAIL in rain_near_restir: 1099511627776 the ratio dropped (Vulkan's
// cellRestir the zero texel buffer); 2199023255552 the ratio off (the packed cellRestirOn 0); 4398046511104 the size's axes
// swapped (the packed cellRestirSize x / y); 8796093022208 the block not repacked (the ReSTIR twin reads the plain lit
// block's bytes: every field from cellFarOn on 32 bytes early). Lifted with it: precipLitWhys' ReSTIR refusal; a ReSTIR
// run (cellRestirOn active in GL's program) takes a second lit twin, precip_lit_rt_view_cs (WW_CELL_RESTIR; its block is the
// lit block with cellRestirOn / cellRestirSize inserted at 4352, every later field 32 bytes on, checked against the
// reflect), its ratio GL's cellRestir copied a run into binding 71; refused by name instead: GL's ReSTIR variant with its
// cubes compiled in (cellShadow active: WW_CELL_RESTIR_CUBES_RED), cellRestirOn 1 with no RG32F buffer on GL's unit. The
// closed list counts cellRestir as a sampler slot and cellRestirSize as read. Census after: route default 0 / 0, route
// all-on 1 / 1 (refusal 1 and both input lines gone, the physical falloff left).
#define main fxview_main
#include "fxview.cpp"
#undef main
#include <array>

namespace pv
{
using namespace fv;

const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";
const char * kRevealFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.0, 0.0, 0.0, 1.0 ); }\n";
// the reference's 1x pass: one texel a sample; 0 the blended frame, 1 the pixel split by this particle (blend MAX), 2 the
// last written output (blend off, check R2)
const char * kCapBlendFs = "#version 410 core\n"
	"uniform sampler2DMS cap; uniform int S;\n"
	"layout(location=0) out vec4 o; layout(location=1) out vec4 sp; layout(location=2) out vec4 last;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ), p = ivec2( q.x / S, q.y ); vec4 v = texelFetch( cap, p, q.x % S );\n"
	"  if ( v.a < 0.0 ) discard;\n"
	"  vec4 v0 = texelFetch( cap, p, 0 ); int w = 0; bool same = true;\n"
	"  for ( int k = 0; k < S; k++ ) { vec4 u = texelFetch( cap, p, k ); w += u.a < 0.0 ? 0 : 1; same = same && u == v0; }\n"
	"  o = v; sp = vec4( w < S || !same ? 1.0 : 0.0 ); last = v; }\n";
const char * kProbeFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.25, 0.5, 0.75, 0.4 ); }\n";

// float to half, round to nearest even (red 64's mean)
std::uint16_t f2h( float x )
{
	std::uint32_t b;
	std::memcpy( &b, &x, 4 );
	const std::uint16_t s = std::uint16_t( ( b >> 16 ) & 0x8000 );
	b &= 0x7FFFFFFFu;
	if ( b >= 0x7F800000u )
		return s | 0x7C00 | ( b > 0x7F800000u ? 0x200 : 0 );
	if ( b >= 0x477FF000u )
		return s | 0x7C00;
	if ( b < 0x38800000u ) {
		float v;
		std::memcpy( &v, &b, 4 );
		return s | std::uint16_t( std::nearbyint( v * 16777216.0f ) );
	}
	return s | std::uint16_t( ( b + 0xFFFu + ( ( b >> 13 ) & 1u ) - 0x38000000u ) >> 13 );
}

struct Verdict
{
	size_t covered = 0, flips = 0, past1 = 0, diff = 0, stencil = 0;
	size_t flipBy[3] = {};  // flips by their worst half: 3-8, 9-64, past 64 ulp
	size_t flipWhole = 0, flipPart = 0;  // flips in pixels whose every sample flipped (shading) / only some (coverage)
	int maxu = 0;
	bool ok() const { return covered >= 1000 && flips * 2000 <= covered && past1 * 1000 <= covered * 4 && stencil * 2000 <= covered; }
	const char * word() const { return covered < 1000 ? "FAIL" : diff == 0 && stencil == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

Verdict judge( const std::vector<std::uint16_t> & bg, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b,
	const std::vector<std::uint16_t> & ra, const std::vector<std::uint16_t> & rb )
{
	Verdict v;
	if ( a.size() != b.size() || a.size() != bg.size() || ra.size() != a.size() || rb.size() != a.size() || a.empty() )
		return v;
	std::vector<char> flipAt( a.size() / 4, 0 );  // kUnpackFs: a pixel's S = 4 samples sit side by side in a row
	for ( size_t s = 0; s < a.size() / 4; s++ ) {
		bool cov = false;
		int m = 0, p1 = 0, df = 0;
		for ( int c = 0; c < 4; c++ ) {
			const size_t i = s * 4 + size_t( c );
			cov = cov || a[i] != bg[i] || b[i] != bg[i];
			const int d = std::abs( ordered( a[i] ) - ordered( b[i] ) );
			m = std::max( m, d );
			p1 += d > 1;
			df += d != 0;
		}
		const bool sa = ra[s * 4 + 3] == 0x3C00, sb = rb[s * 4 + 3] == 0x3C00;
		cov = cov || sa || sb;
		if ( !cov )
			continue;
		v.covered++;
		v.diff += size_t( df );
		v.stencil += sa != sb;
		if ( m > 2 ) {
			v.flips++;
			v.flipBy[m <= 8 ? 0 : m <= 64 ? 1 : 2]++;
			flipAt[s] = 1;
		} else
			v.past1 += size_t( p1 );
		v.maxu = std::max( v.maxu, m > 2 ? 0 : m );
	}
	for ( size_t p = 0; p + 4 <= flipAt.size(); p += 4 ) {
		const int n = flipAt[p] + flipAt[p + 1] + flipAt[p + 2] + flipAt[p + 3];
		( n == 4 ? v.flipWhole : v.flipPart ) += size_t( n );
	}
	return v;
}

// ruling 11-14 (the near snow reds' localisation, harness only): GL's geometry-stage outputs by transform feedback
// (a copy of the program linked with these varyings; the shaders unchanged) against the Vulkan records read back
void( WINAPI * tfVaryings )( GLuint, GLsizei, const char * const *, GLenum ) = nullptr;
void( WINAPI * tfBegin )( GLenum ) = nullptr;
void( WINAPI * tfEnd )() = nullptr;
void( WINAPI * genQueries )( GLsizei, GLuint * ) = nullptr;
void( WINAPI * beginQuery )( GLenum, GLuint ) = nullptr;
void( WINAPI * endQuery )( GLenum ) = nullptr;
void( WINAPI * getQueryObjectuiv )( GLuint, GLenum, GLuint * ) = nullptr;
void( WINAPI * getBufferSubData )( GLenum, std::ptrdiff_t, std::ptrdiff_t, void * ) = nullptr;
const char * kTfVar[8] = { "gl_Position", "gUv", "gQ", "gView", "gCol", "gBox", "gA", "gB" };
const int kTfAt[9] = { 0, 4, 6, 8, 11, 15, 19, 22, 25 };  // each varying's first float in a captured vertex, 25 a vertex
const int kTfFloats = 25;
void( WINAPI * compressedTexImage2D )( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void * ) = nullptr;
void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
void( WINAPI * blendEquationi )( GLuint, GLenum ) = nullptr;
void( WINAPI * disablei )( GLenum, GLuint ) = nullptr;

// ruling 11-14 (step 1): the atlases as the app loads them (cellprecip.cpp loadDds): BC3 (fmt bc3, or bc3srgb), every level
// from the prep's numpy encoder (mips = the level count), BASE 0, MAX_LEVEL levels - 1, trilinear, CLAMP_TO_EDGE
bool uploadBc( const std::string & dir, TexIn & t )
{
	const GLenum ifmt = t.fmt == "bc3" ? 0x83F3 : t.fmt == "bc3srgb" ? 0x8C4F : 0;  // DXT5, SRGB_ALPHA DXT5
	if ( !ifmt || t.mips < 1 || t.target != GL_TEXTURE_2D ) {
		std::printf( "%s texture %s: format %s, %d levels\n", gTag, t.name.c_str(), t.fmt.c_str(), t.mips );
		return false;
	}
	const std::string data = readFile( dir + "/" + t.file );
	GLuint x = 0;
	glGenTextures( 1, &x );
	glBindTexture( GL_TEXTURE_2D, x );
	size_t at = 0;
	for ( int l = 0; l < t.mips; l++ ) {
		const int lw = std::max( t.w >> l, 1 ), lh = std::max( t.h >> l, 1 );
		const size_t bytes = size_t( std::max( 1, ( lw + 3 ) / 4 ) ) * size_t( std::max( 1, ( lh + 3 ) / 4 ) ) * 16;
		if ( at + bytes > data.size() ) {
			std::printf( "%s texture %s: short at level %d\n", gTag, t.name.c_str(), l );
			return false;
		}
		compressedTexImage2D( GL_TEXTURE_2D, l, ifmt, lw, lh, 0, GLsizei( bytes ), data.data() + at );
		at += bytes;
	}
	if ( at != data.size() ) {
		std::printf( "%s texture %s: bytes past its levels\n", gTag, t.name.c_str() );
		return false;
	}
	glTexParameteri( GL_TEXTURE_2D, 0x813C, 0 );  // GL_TEXTURE_BASE_LEVEL
	glTexParameteri( GL_TEXTURE_2D, fMaxLevel, t.mips - 1 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t.mips > 1 ? GLint( fMipLinear ) : GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint( fClamp ) );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint( fClamp ) );
	glBindTexture( GL_TEXTURE_2D, 0 );
	t.gl = x;
	return glGetError() == GL_NO_ERROR;
}
bool uploadAtlas( const std::string & dir, TexIn & t )
{
	return t.fmt.compare( 0, 3, "bc3" ) == 0 ? uploadBc( dir, t ) : upload( dir, t, 0 );
}

GLuint program3( const std::string & vs, const std::string & gs, const std::string & fs, const char * tag, bool tf = false )
{
	const GLuint a = shader( fVert, vs, tag ), b = shader( 0x8DD9, gs, tag ), c = shader( fFrag, fs, tag );  // GL_GEOMETRY_SHADER
	if ( !a || !b || !c )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.attachShader( p, c );
	if ( tf )
		tfVaryings( p, 8, kTfVar, 0x8C8C );  // GL_INTERLEAVED_ATTRIBS
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, fLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "%s link failed (%s): %.1500s\n", gTag, tag, log.data() );
		return 0;
	}
	return p;
}

long long ulpOf( float x, float y )
{
	std::int32_t i = 0, k = 0;
	std::memcpy( &i, &x, 4 );
	std::memcpy( &k, &y, 4 );
	const long long a = i < 0 ? -static_cast<long long>( i & 0x7FFFFFFF ) : i, b = k < 0 ? -static_cast<long long>( k & 0x7FFFFFFF ) : k;
	return std::llabs( a - b );
}

// precip_view_vs.hlsl spelled on the CPU from a record (c, axes, ext at floats 0 / 4 / 8): corner v's GL clip position
// (before the depth move), uv and view position; M = GL's pcProj as glGetUniformfv reads it (column i at 4 i = row i there)
void vsCorner( const float * r, int v, const float * M, const float * atlas, float pos[4], float uv[2], float view[3] )
{
	const float qx = ( v & 1 ) ? 1.0f : -1.0f, qy = ( v & 2 ) ? 1.0f : -1.0f;
	const float xh = qx * r[8], yh = qy * r[9];
	const float offY0 = r[6] * yh, offY1 = r[7] * yh;
	const float off0 = std::fma( r[4], xh, offY0 ), off1 = std::fma( r[5], xh, offY1 );
	view[0] = r[0] + off0;
	view[1] = r[1] + off1;
	view[2] = r[2] + 0.0f;
	uv[0] = std::fma( std::fma( qx, 0.5f, 0.5f ), atlas[0], r[10] );
	uv[1] = std::fma( std::fma( qy, 0.5f, 0.5f ), atlas[1], r[11] );
	for ( int i = 0; i < 4; i++ ) {
		const float c0 = view[1] * M[4 + i];
		const float c1 = std::fma( view[0], M[i], c0 );
		const float c2 = std::fma( view[2], M[8 + i], c1 );
		pos[i] = c2 + M[12 + i];
	}
}

// one case's particles: GL's captured triangles (6 vertices a particle, kTfFloats each) against the Vulkan records (28
// floats each: c axes ext col box a b; c.w 0 = culled), per field and bucket; then which flipped pixels lie under a
// particle that differs past 4 ulp anywhere (its GL quad's pixel box)
void particleDiff( const std::string & name, const std::vector<float> & gl, unsigned glPrims, const std::vector<std::uint8_t> & rec, int n,
	const float * M, const float * atlas, int W, int H, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & ref )
{
	std::vector<int> drawn;
	for ( int i = 0; i < n; i++ ) {
		float cw = 0.0f;
		std::memcpy( &cw, rec.data() + size_t( i ) * 112 + 12, 4 );
		if ( cw != 0.0f )
			drawn.push_back( i );
	}
	std::printf( "PRECIPVIEW %s particles: GL wrote %u triangles (%s 2 x the %zu records Vulkan drew of %d)\n", name.c_str(), glPrims,
		glPrims == 2 * drawn.size() ? "=" : "NOT", drawn.size(), n );
	if ( glPrims != 2 * drawn.size() )
		return;  // a cull difference: the finding itself (no pairing past it)
	const char * fname[7] = { "clip position", "uv", "view position", "gCol", "gBox", "gA", "gB" };
	size_t bucket[7][4] = {};  // exact / 1-4 / 5-64 / past 64 ulp, particles by their worst component
	long long fmax[7] = {};
	size_t glFlatSplit = 0, cornerBad = 0;
	std::vector<char> mark( size_t( W ) * H, 0 );
	size_t shown = 0, differing = 0;
	for ( size_t k = 0; k < drawn.size(); k++ ) {
		const float * r = reinterpret_cast<const float *>( rec.data() + size_t( drawn[k] ) * 112 );
		long long worst[7] = {};
		float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
		const float * first = &gl[k * 6 * kTfFloats];
		for ( int e = 0; e < 6; e++ ) {
			const float * g = first + e * kTfFloats;
			const float qx = g[kTfAt[2]], qy = g[kTfAt[2] + 1];
			if ( std::fabs( qx ) != 1.0f || std::fabs( qy ) != 1.0f ) {
				cornerBad++;
				continue;
			}
			const int v = ( qx > 0.0f ? 1 : 0 ) + ( qy > 0.0f ? 2 : 0 );
			float pos[4], uv[2], view[3];
			vsCorner( r, v, M, atlas, pos, uv, view );
			for ( int c = 0; c < 4; c++ )
				worst[0] = std::max( worst[0], ulpOf( g[kTfAt[0] + c], pos[c] ) );
			for ( int c = 0; c < 2; c++ )
				worst[1] = std::max( worst[1], ulpOf( g[kTfAt[1] + c], uv[c] ) );
			for ( int c = 0; c < 3; c++ )
				worst[2] = std::max( worst[2], ulpOf( g[kTfAt[3] + c], view[c] ) );
			for ( int c = 0; c < kTfAt[8] - kTfAt[4]; c++ )  // GL's flat outputs the same on every vertex (written once before the loop)
				glFlatSplit += std::memcmp( &g[kTfAt[4] + c], &first[kTfAt[4] + c], 4 ) != 0;
			if ( v >= 2 ) {  // a triangle's last vertex (the provoking one, GL's default): its flat values are the ones drawn
				for ( int c = 0; c < 4; c++ ) {
					worst[3] = std::max( worst[3], ulpOf( g[kTfAt[4] + c], r[12 + c] ) );
					worst[4] = std::max( worst[4], ulpOf( g[kTfAt[5] + c], r[16 + c] ) );
				}
				for ( int c = 0; c < 3; c++ ) {
					worst[5] = std::max( worst[5], ulpOf( g[kTfAt[6] + c], r[20 + c] ) );
					worst[6] = std::max( worst[6], ulpOf( g[kTfAt[7] + c], r[24 + c] ) );
				}
			}
			const float w = g[3];
			for ( int c = 0; c < 2 && w > 0.0f; c++ ) {
				const float s = ( g[c] / w * 0.5f + 0.5f ) * float( c ? H : W );
				lo[c] = std::min( lo[c], s );
				hi[c] = std::max( hi[c], s );
			}
		}
		bool diff = false;
		for ( int fi = 0; fi < 7; fi++ ) {
			bucket[fi][worst[fi] == 0 ? 0 : worst[fi] <= 4 ? 1 : worst[fi] <= 64 ? 2 : 3]++;
			fmax[fi] = std::max( fmax[fi], worst[fi] );
			diff = diff || worst[fi] > 4;
		}
		if ( !diff )
			continue;
		differing++;
		for ( int y = std::max( int( std::floor( lo[1] ) ), 0 ); y <= std::min( int( std::ceil( hi[1] ) ), H - 1 ); y++ )
			for ( int x = std::max( int( std::floor( lo[0] ) ), 0 ); x <= std::min( int( std::ceil( hi[0] ) ), W - 1 ); x++ )
				mark[size_t( y ) * W + x] = 1;
		if ( shown < 6 ) {
			shown++;
			std::printf( "PRECIPVIEW %s particle %d (GL primitive pair %zu) differs:", name.c_str(), drawn[k], k );
			for ( int fi = 0; fi < 7; fi++ )
				if ( worst[fi] > 4 )
					std::printf( " %s %lld ulp", fname[fi], worst[fi] );
			std::printf( " (GL gCol %.9g %.9g %.9g %.9g, Vulkan %.9g %.9g %.9g %.9g; GL gA %.9g, Vulkan %.9g)\n", first[kTfAt[4]], first[kTfAt[4] + 1],
				first[kTfAt[4] + 2], first[kTfAt[4] + 3], r[12], r[13], r[14], r[15], first[kTfAt[6]], r[20] );
		}
	}
	for ( int fi = 0; fi < 7; fi++ )
		std::printf( "PRECIPVIEW %s field %s: particles exact / 1-4 / 5-64 / past 64 ulp: %zu / %zu / %zu / %zu (max %lld)\n", name.c_str(), fname[fi],
			bucket[fi][0], bucket[fi][1], bucket[fi][2], bucket[fi][3], fmax[fi] );
	std::printf( "PRECIPVIEW %s GL flat outputs that differ between a particle's vertices: %zu floats; captured vertices off a corner: %zu\n",
		name.c_str(), glFlatSplit, cornerBad );
	// the flipped pixels (any sample's half past 2 ulp, as judge's flips) under / outside the differing particles' boxes
	size_t flipIn = 0, flipOut = 0, marked = 0;
	for ( size_t p = 0; p < mark.size(); p++ ) {
		marked += size_t( mark[p] );
		bool fl = false;
		for ( int s = 0; s < 4 && !fl; s++ )
			for ( int c = 0; c < 4 && !fl; c++ ) {
				const size_t i = ( p * 4 + size_t( s ) ) * 4 + size_t( c );
				fl = std::abs( ordered( a[i] ) - ordered( ref[i] ) ) > 2;
			}
		if ( fl )
			( mark[p] ? flipIn : flipOut )++;
	}
	std::printf( "PRECIPVIEW %s particles differing past 4 ulp: %zu of %zu; flipped pixels under their quads %zu, outside every one %zu "
		"(their quads cover %zu of %zu pixels)\n", name.c_str(), differing, drawn.size(), flipIn, flipOut, marked, mark.size() );
}

struct Job
{
	std::string name;
	int w = 0, h = 0, first = 0, n = 0, hasTex = 0, farT = 0, fog = 0, look = 0, lit = 0;  // (windows.h defines far)
	unsigned seed = 0;
	float zd = 0.5f;
	UniMap uni;
	std::vector<TexIn> tex;
	int atlasAt = -1, airAt[5] = { -1, -1, -1, -1, -1 }, normAt = -1, envAt = -1;  // into tex (envAt: L8 term 2's volEnvHist)
	int noiseAt = -1;  // into tex: L8 term 4's volAirNoise (3D R8)
	int csmN = 0, cloudN = 0;  // L8 term 3: shadow.txt's csmMap size (256 x 256 x 3, D16) and ussCloudMap size (RGBA16F)
	std::vector<std::uint16_t> csmData;
	std::vector<float> cloudData;
	int cubeN = 0;  // L8 term 5: cubes.txt's face edge (16 cubes, D16; cubes.bin N x N x 96 layer-faces)
	std::vector<std::uint16_t> cubeData;
	int roofNx = 0, roofNy = 0;  // L8 term 7: roof.txt's map size (R32F; roof.bin nx x ny floats, row-major from y 0)
	std::vector<float> roofData;
	int ussTN = 0, ussCW = 0, ussCH = 0;  // L8 route lift 2: uss.txt's ussTerrMap edge (RG16) and ussContactMap size (R8)
	std::vector<std::uint16_t> ussTerrData;
	std::vector<std::uint8_t> ussContactData;
	bool uss() const  // L8 route lift 2: the shadow set's composition on (ussOn 1, its maps from uss.txt)
	{
		const auto it = uni.find( "ussOn" );
		return it != uni.end() && !it->second.empty() && it->second[0] != 0.0 && ussTN > 0;
	}
	bool hasN() const { return look == 2 && hasTex; }
	bool frameUbo() const { return uni.count( "wwGateFrameUbo" ) != 0; }  // L8 route lift 1: drawn through pRoute, the cascades in wwShadowFrame
	struct Buf  // L8 term 2a: a texture buffer (buf.txt: name format texels file)
	{
		std::string name, fmt, file;
		int texels = 0;
		GLuint buf = 0, tex = 0;
	};
	std::vector<Buf> bufs;
	GLuint bufTex( const char * nm ) const
	{
		for ( const Buf & b : bufs )
			if ( b.name == nm )
				return b.tex;
		return 0;
	}
	bool lamps() const { return lit && !bufs.empty(); }
	bool localSun() const  // L8 term 3: vpSun.w on (the local law)
	{
		const auto it = uni.find( "vpSun" );
		return lit && it != uni.end() && it->second.size() == 4 && it->second[3] > 0.5;
	}
	bool gi() const  // L8 term 1 (VKFAR5): volTerms' GI bit (4)
	{
		const auto it = uni.find( "volTerms" );
		return lit && it != uni.end() && !it->second.empty() && ( int( it->second[0] ) & 4 ) != 0;
	}
	bool terrAir() const  // L8 terrain air: airTerrOn 1 (the ground layer on the grid)
	{
		const auto it = uni.find( "airTerrOn" );
		return lit && fog && it != uni.end() && !it->second.empty() && it->second[0] > 0.5;
	}
	std::vector<char> global;  // L8 term 2b: globalUniforms' bytes (global.bin; empty: zeros)
	bool cluster() const
	{
		for ( const Buf & b : bufs )
			if ( b.name == "cellCluster" )
				return lit != 0;
		return false;
	}
	bool restir() const  // L8 route lift 3: GL's ReSTIR variant (cellRestirOn 1, cellRestir from buf.txt), drawn through pAllOn
	{
		bool buf = false;
		for ( const Buf & b : bufs )
			buf = buf || b.name == "cellRestir";
		const auto it = uni.find( "cellRestirOn" );
		return lit && buf && it != uni.end() && !it->second.empty() && it->second[0] != 0.0;
	}
	bool amb() const  // L8 term 2 (VKFAR5): volTerms' ambient bit (8)
	{
		const auto it = uni.find( "volTerms" );
		return lit && it != uni.end() && !it->second.empty() && ( int( it->second[0] ) & 8 ) != 0;
	}
	bool envKeep() const  // L8 term 2: the ambient's keep on (volEnvGrid.w 1, the history bound)
	{
		const auto it = uni.find( "volEnvGrid" );
		return amb() && envAt >= 0 && it != uni.end() && it->second.size() == 4 && it->second[3] > 0.5;
	}
	bool readsB0() const { return cluster() || envKeep(); }  // GL's geometry stage reads b0's projection
	bool csm() const  // L8 term 3 (VKFAR6): the sun's cascades on (volCsm 1, a map from shadow.txt)
	{
		const auto it = uni.find( "volCsm" );
		return lit && csmN > 0 && it != uni.end() && !it->second.empty() && it->second[0] > 0.5;
	}
	bool drift() const  // L8 term 4 (VKFAR6): the air's drift on (volAirDrift.x > 0, the noise from tex.txt)
	{
		const auto it = uni.find( "volAirDrift" );
		return lit && fog && noiseAt >= 0 && it != uni.end() && it->second.size() == 4 && it->second[0] > 0.0f;
	}
	bool cube() const  // L8 term 5 (VKFAR6): the lamps' shadow cubes on (cellShadowOn 1, cubes from cubes.txt)
	{
		const auto it = uni.find( "cellShadowOn" );
		return lamps() && cubeN > 0 && it != uni.end() && !it->second.empty() && it->second[0] > 0.5 && !restir();
	}
	bool gobo() const  // L8 term 6 (VKFAR6): the lamps' gobos on (cellGoboOn 1, the patterns in cellLights)
	{
		const auto it = uni.find( "cellGoboOn" );
		return lamps() && it != uni.end() && !it->second.empty() && it->second[0] > 0.5;
	}
	bool roof() const  // L8 term 7 (VKFAR6): the roof map on (pcRoofOn 1, the map from roof.txt), any variant
	{
		const auto it = uni.find( "pcRoofOn" );
		return roofNx > 0 && it != uni.end() && !it->second.empty() && it->second[0] > 0.5;
	}
	bool interior() const  // L8 term 8 (VKFAR6): a lit interior (cellInterior 1)
	{
		const auto it = uni.find( "cellInterior" );
		return lit && it != uni.end() && !it->second.empty() && it->second[0] > 0.5;
	}
	bool darkInterior() const  // ... with no sun (cellHasDir 0): its lit check's cellHasDir 0 is printed, not judged
	{
		const auto it = uni.find( "cellHasDir" );
		return interior() && it != uni.end() && !it->second.empty() && it->second[0] < 0.5;
	}
};
const int kNormUnit = 8;  // slice 3: the look's pcTexN
const int kLightsUnit = 9, kClusterUnit = 10;  // L8 term 2a: the lamps' cellLights / cellCluster
const int kTerrUnit = 11;  // L8 terrain air: airTerr
const int kGiUnit = 12;  // L8 term 1: the WW_CELL_PRT variant's cellGiAir
const int kEnvUnit = 13;  // L8 term 2: the ambient's volEnvHist
const int kCsmUnit = 17, kCloudUnit = 18;  // L8 term 3: csmMap, ussCloudMap (15 / 16 are the read-back's)
const int kNoiseUnit = 19;  // L8 term 4: the drift's volAirNoise
const int kUssTerrUnit = 70, kUssContactUnit = 71;  // L8 route lift 2: ussTerrMap, ussContactMap (past every empty unit below)
const int kCubeUnit = 20;  // L8 term 5: the lamps' cellShadow (a D16 cube array)
const GLenum kTexBuffer = 0x8C2A;  // GL_TEXTURE_BUFFER
const int kRestirUnit = 72;  // L8 route lift 3: the ReSTIR ratio's cellRestir (past the shadow set's 70 / 71)
// slice 2a: the air's textures, in precipRun's tex[2..6] order, their GL units and fxTexOf types
const char * kAirTex[5] = { "atApTex", "volTex", "volTile", "volAirTerr", "volAirRainMap" };
const int kAirType[5] = { 1, 1, 0, 2, 0 };
const int kAirUnit = 1;  // units 1..5 (0 the atlas, 7 pcRoof)
}  // namespace pv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace pv;
	gTag = "PRECIPVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: precipview <prep dir>\nPRECIPVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "PRECIPVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	// the cases
	std::vector<Job> jobs;
	{
		std::ifstream in( dir + "/cases.txt" );
		std::string nm;
		while ( in >> nm ) {
			Job j;
			j.name = nm;
			std::ifstream jf( dir + "/" + nm + "/job.txt" );
			if ( !( jf >> j.w >> j.h >> j.first >> j.n >> j.hasTex >> j.seed >> j.farT >> j.zd >> j.fog >> j.look ) )
				return fatal( nm + "/job.txt" );
			if ( !( jf >> j.lit ) )  // slice 2b's field (absent in an older prep: unlit)
				j.lit = 0;
			j.uni = readUni( dir + "/" + nm + "/uni.txt" );
			{
				std::ifstream bf( dir + "/" + nm + "/buf.txt" );
				Job::Buf b;
				while ( bf >> b.name >> b.fmt >> b.texels >> b.file )
					j.bufs.push_back( b );
				std::ifstream gf( dir + "/" + nm + "/global.bin", std::ios::binary );
				if ( gf ) {
					j.global.assign( 336, 0 );
					if ( !gf.read( j.global.data(), 336 ) )
						return fatal( nm + "/global.bin" );
				}
			}
			if ( j.hasTex || j.fog ) {
				j.tex = readTex( dir + "/" + nm );
				for ( size_t k = 0; k < j.tex.size(); k++ ) {
					if ( j.tex[k].name == "pcTex" )
						j.atlasAt = int( k );
					if ( j.tex[k].name == "pcTexN" )
						j.normAt = int( k );
					if ( j.tex[k].name == "volEnvHist" && j.tex[k].type == 1 )
						j.envAt = int( k );
					if ( j.tex[k].name == "volAirNoise" && j.tex[k].type == 1 )
						j.noiseAt = int( k );
					for ( int a = 0; a < 5; a++ )
						if ( j.tex[k].name == kAirTex[a] && j.tex[k].type == kAirType[a] )
							j.airAt[a] = int( k );
				}
				if ( j.tex.size() != size_t( ( j.hasTex ? 1 : 0 ) + ( j.fog ? 5 : 0 ) + ( j.hasN() ? 1 : 0 ) + ( j.envAt >= 0 ? 1 : 0 ) + ( j.noiseAt >= 0 ? 1 : 0 ) ) || ( j.hasTex && j.atlasAt < 0 )
					|| ( j.hasN() && j.normAt < 0 ) )
					return fatal( nm + "/tex.txt" );
				for ( int a = 0; a < 5 && j.fog; a++ )
					if ( j.airAt[a] < 0 )
						return fatal( nm + "/tex.txt: " + kAirTex[a] );
			}
			{  // L8 term 3: the sun's cascades and the clouds' map (shadow.txt: csmMap size, ussCloudMap size)
				std::ifstream sf( dir + "/" + nm + "/shadow.txt" );
				if ( sf >> j.csmN >> j.cloudN ) {
					j.csmData.resize( size_t( j.csmN ) * size_t( j.csmN ) * 3 );
					j.cloudData.resize( size_t( j.cloudN ) * size_t( j.cloudN ) * 4 );
					std::ifstream cb( dir + "/" + nm + "/csm.bin", std::ios::binary ), lb( dir + "/" + nm + "/cloud.bin", std::ios::binary );
					if ( !cb.read( reinterpret_cast<char *>( j.csmData.data() ), std::streamsize( j.csmData.size() * 2 ) )
						|| !lb.read( reinterpret_cast<char *>( j.cloudData.data() ), std::streamsize( j.cloudData.size() * 4 ) ) )
						return fatal( nm + "/shadow.txt: csm.bin / cloud.bin" );
				}
			}
			{  // L8 route lift 2: the shadow set's maps (uss.txt: ussTerrMap edge, ussContactMap width and height)
				std::ifstream uf( dir + "/" + nm + "/uss.txt" );
				if ( uf >> j.ussTN >> j.ussCW >> j.ussCH ) {
					j.ussTerrData.resize( size_t( j.ussTN ) * size_t( j.ussTN ) * 2 );
					j.ussContactData.resize( size_t( j.ussCW ) * size_t( j.ussCH ) );
					std::ifstream tb( dir + "/" + nm + "/terr.bin", std::ios::binary ), ub( dir + "/" + nm + "/contact.bin", std::ios::binary );
					if ( !tb.read( reinterpret_cast<char *>( j.ussTerrData.data() ), std::streamsize( j.ussTerrData.size() * 2 ) )
						|| !ub.read( reinterpret_cast<char *>( j.ussContactData.data() ), std::streamsize( j.ussContactData.size() ) ) )
						return fatal( nm + "/uss.txt: terr.bin / contact.bin" );
				}
			}
			{  // L8 term 5: the lamps' shadow cubes (cubes.txt: the face edge; cubes.bin 16 cubes of 6 D16 faces)
				std::ifstream uf( dir + "/" + nm + "/cubes.txt" );
				if ( uf >> j.cubeN ) {
					j.cubeData.resize( size_t( j.cubeN ) * size_t( j.cubeN ) * 96 );
					std::ifstream ub( dir + "/" + nm + "/cubes.bin", std::ios::binary );
					if ( !ub.read( reinterpret_cast<char *>( j.cubeData.data() ), std::streamsize( j.cubeData.size() * 2 ) ) )
						return fatal( nm + "/cubes.txt: cubes.bin" );
				}
			}
			{  // L8 term 7: the roof map (roof.txt: nx ny; roof.bin R32F)
				std::ifstream rf( dir + "/" + nm + "/roof.txt" );
				if ( rf >> j.roofNx >> j.roofNy ) {
					j.roofData.resize( size_t( j.roofNx ) * size_t( j.roofNy ) );
					std::ifstream rb( dir + "/" + nm + "/roof.bin", std::ios::binary );
					if ( !rb.read( reinterpret_cast<char *>( j.roofData.data() ), std::streamsize( j.roofData.size() * 4 ) ) )
						return fatal( nm + "/roof.txt: roof.bin" );
				}
			}
			jobs.push_back( j );
		}
	}
	if ( jobs.empty() )
		return fatal( "cases.txt" );
	const int W = jobs[0].w, H = jobs[0].h, S = 4, WS = W * S;
	for ( const Job & j : jobs )
		if ( j.w != W || j.h != H )
			return fatal( "the cases' frame sizes differ" );

	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwprecipview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwprecipview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwprecipview", "precip view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf )
		return fatal( "no 4x MSAA pixel format" );
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, fmt, sizeof( got ), &got );
	SetPixelFormat( dc, fmt, &got );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	if ( !loadAll() || !load( genVertexArrays, "glGenVertexArrays" ) || !load( bindVertexArray, "glBindVertexArray" )
		|| !load( tfVaryings, "glTransformFeedbackVaryings" ) || !load( tfBegin, "glBeginTransformFeedback" )
		|| !load( tfEnd, "glEndTransformFeedback" ) || !load( genQueries, "glGenQueries" ) || !load( beginQuery, "glBeginQuery" )
		|| !load( endQuery, "glEndQuery" ) || !load( getQueryObjectuiv, "glGetQueryObjectuiv" ) || !load( getBufferSubData, "glGetBufferSubData" )
		|| !load( compressedTexImage2D, "glCompressedTexImage2D" ) || !load( clearBufferfv, "glClearBufferfv" )
		|| !load( blendEquationi, "glBlendEquationi" ) || !load( disablei, "glDisablei" ) )
		return fatal( "GL entry points" );
	std::printf( "PRECIPVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pSlab = program( kTriVs, kSlabFs, "slab" ),
		pReveal = program( kTriVs, kRevealFs, "reveal" ), pCapBlend = program( kTriVs, kCapBlendFs, "1x blend" ),
		pProbe = program( kTriVs, kProbeFs, "blend probe" );
	const GLuint pPrecip = program3( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.geom.glsl" ), readFile( dir + "/prog.frag.glsl" ),
		"cell_precip_gpu" );
	const GLuint pLook = program3( readFile( dir + "/prog_look.vert.glsl" ), readFile( dir + "/prog_look.geom.glsl" ),
		readFile( dir + "/prog_look.frag.glsl" ), "cell_precip_look" );
	// the localisation's copy of cell_precip_gpu.prog, its geometry-stage outputs captured (the plain cases)
	const GLuint pTf = program3( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.geom.glsl" ), readFile( dir + "/prog.frag.glsl" ),
		"cell_precip_gpu (captured)", true );
	// the fogleak check's reference (step 1, 11-14): both programs without the loader's WW_CELL_FOGLEAK, fogged cases only
	const GLuint pNoFl = program3( readFile( dir + "/prog_nofl.vert.glsl" ), readFile( dir + "/prog_nofl.geom.glsl" ),
		readFile( dir + "/prog_nofl.frag.glsl" ), "cell_precip_gpu (no FOGLEAK)" );
	const GLuint pLookNoFl = program3( readFile( dir + "/prog_look_nofl.vert.glsl" ), readFile( dir + "/prog_look_nofl.geom.glsl" ),
		readFile( dir + "/prog_look_nofl.frag.glsl" ), "cell_precip_look (no FOGLEAK)" );
	// L8 term 1 (VKFAR5): the loader's GI variants of the plain program (WW_CELL_PRT, the red; WW_CELL_RTDDGI, the RT GI on)
	const GLuint pPrt = program3( readFile( dir + "/prog_prt.vert.glsl" ), readFile( dir + "/prog_prt.geom.glsl" ),
		readFile( dir + "/prog_prt.frag.glsl" ), "cell_precip_gpu (WW_CELL_PRT)" );
	const GLuint pRt = program3( readFile( dir + "/prog_rtddgi.vert.glsl" ), readFile( dir + "/prog_rtddgi.geom.glsl" ),
		readFile( dir + "/prog_rtddgi.frag.glsl" ), "cell_precip_gpu (WW_CELL_RTDDGI)" );
	const GLuint pTfRt = program3( readFile( dir + "/prog_rtddgi.vert.glsl" ), readFile( dir + "/prog_rtddgi.geom.glsl" ),
		readFile( dir + "/prog_rtddgi.frag.glsl" ), "cell_precip_gpu (WW_CELL_RTDDGI, captured)", true );
	// (the RT GI variant's twin of the default: a program as fresh as it, so no earlier case's uniforms ride on one side only)
	const GLuint pTfNew = program3( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.geom.glsl" ),
		readFile( dir + "/prog.frag.glsl" ), "cell_precip_gpu (captured, fresh)", true );
	// the route census (header): the loader's own programs (default; all-on; all-on + WW_CELL_RESTIRGI, info)
	const GLuint pRoute = program3( readFile( dir + "/prog_route.vert.glsl" ), readFile( dir + "/prog_route.geom.glsl" ),
		readFile( dir + "/prog_route.frag.glsl" ), "cell_precip_gpu (route default)" );
	const GLuint pAllOn = program3( readFile( dir + "/prog_allon.vert.glsl" ), readFile( dir + "/prog_allon.geom.glsl" ),
		readFile( dir + "/prog_allon.frag.glsl" ), "cell_precip_gpu (route all-on)" );
	const GLuint pRgi = program3( readFile( dir + "/prog_rgi.vert.glsl" ), readFile( dir + "/prog_rgi.geom.glsl" ),
		readFile( dir + "/prog_rgi.frag.glsl" ), "cell_precip_gpu (route all-on + WW_CELL_RESTIRGI)" );
	if ( !pUnpack || !pSlab || !pReveal || !pCapBlend || !pProbe || !pPrecip || !pLook || !pTf || !pNoFl || !pLookNoFl || !pPrt || !pRt || !pTfRt || !pTfNew
		|| !pRoute || !pAllOn || !pRgi )
		return fatal( "GL programs" );
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl, pPrt, pRt, pTfRt, pTfNew, pRoute, pAllOn, pRgi } ) {  // every sampler its own empty unit from 20 (unit 0's default mixes sampler
		// types: GL_INVALID_OPERATION at the draw once the air's textures are bound); bindJob then sets the read ones (farcards.cpp's rule)
		f.useProgram( prog );
		GLint n = 0, next = 20;
		f.getProgramiv( prog, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( prog, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			const bool smp = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 && !( ty >= 0x8DC6 && ty <= 0x8DC8 ) )
				|| ( ty >= 0x900C && ty <= 0x900F ) || ( ty >= 0x9108 && ty <= 0x910D );
			if ( !smp )
				continue;
			std::vector<GLint> units( size_t( std::max( sz, 1 ) ) );
			for ( GLint & u : units )
				u = next++;
			if ( next > kUssTerrUnit )  // L8 route lift 2: the shadow set's units stay past every empty one
				return fatal( "the empty sampler units reach unit " + std::to_string( next - 1 ) );
			f.uniform1iv( f.getUniformLocation( prog, nm ), GLsizei( units.size() ), units.data() );
		}
	}
	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint refCol = msTex( W, H, fRgba16f, S ), refDs = msTex( W, H, fD24s8, S );
	const GLuint fbRef = fbo( fT2DMS, refCol, 0, refDs );
	const GLuint unp = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0, 0 );
	// the per-sample reference's targets: the capture (4x RGBA32F on refDs: the slab's depth) and the 1x frame's three
	const GLuint capCol = msTex( W, H, fRgba32f, S );
	const GLuint fbCap = fbo( fT2DMS, capCol, 0, refDs );
	GLuint bl[3] = {};
	for ( GLuint & t : bl )
		t = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbBl0 = fbo( GL_TEXTURE_2D, bl[0], 0, 0 );  // R1's probe: the blended frame alone
	GLuint fbBl = 0;
	f.genFramebuffers( 1, &fbBl );
	f.bindFramebuffer( fFb, fbBl );
	for ( int k = 0; k < 3; k++ )
		f.framebufferTexture2D( fFb, fCol0 + GLenum( k ), GL_TEXTURE_2D, bl[k], 0 );
	const GLenum bl3[3] = { fCol0, fCol0 + 1, fCol0 + 2 };
	f.drawBuffers( 3, bl3 );
	if ( f.checkFramebufferStatus( fFb ) != fComplete )
		fbBl = 0;
	if ( !fbFrame || !fbRef || !fbUnp || !fbCap || !fbBl0 || !fbBl )
		return fatal( "GL targets" );
	// uniforms.glsl's block (the geometry stage includes it; unlit and unfogged it reads none of it): zeros
	std::vector<char> zeros( 336, 0 );
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );
	f.bindBuffer( fUbo, ubo );
	f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), zeros.data(), fStatic );
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl, pPrt, pRt, pTfRt, pTfNew, pRoute, pAllOn, pRgi } ) {
		const GLuint bi = f.getUniformBlockIndex( prog, "globalUniforms" );
		if ( bi != 0xFFFFFFFFu )
			f.uniformBlockBinding( prog, bi, 0 );
		const GLuint sb = f.getUniformBlockIndex( prog, "wwShadowFrame" );  // L8 route lift 1: sunshadow.cpp binds it at 2
		if ( sb != 0xFFFFFFFFu )
			f.uniformBlockBinding( prog, sb, 2 );
	}
	// L8 route lift 1: the frame block (256 bytes, sunshadow.cpp CsmFrameBlock) at binding 2 as the app keeps it (before the
	// census); zeros until a frame-block case uploads its cascades
	std::vector<char> zeros256( 256, 0 );
	GLuint frameUboBuf = 0;
	f.genBuffers( 1, &frameUboBuf );
	f.bindBuffer( fUbo, frameUboBuf );
	f.bufferData( fUbo, std::ptrdiff_t( zeros256.size() ), zeros256.data(), fStatic );
	f.bindBufferBase( fUbo, 2, frameUboBuf );
	// known start state (header): every program's loose uniforms as linked, written back at the top of every bindJob
	struct UniSnap
	{
		GLint loc = -1;
		bool isInt = false, isUint = false;
		int comp = 0;
		GLint iv[16] = {};
		GLfloat fv[16] = {};
	};
	std::map<GLuint, std::vector<UniSnap>> linked;
	bool restoreOn = true;  // false only in the restore check (header)
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl, pPrt, pRt, pTfRt, pTfNew, pRoute, pAllOn, pRgi } ) {
		std::vector<UniSnap> & out = linked[prog];
		GLint n = 0;
		f.getProgramiv( prog, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( prog, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			const GLuint idx = GLuint( i );
			GLint blk = -1;
			f.getActiveUniformsiv( prog, 1, &idx, fBlockIndex, &blk );
			if ( blk >= 0 )
				continue;
			UniSnap s;
			switch ( ty ) {
			case 0x1406: s.comp = 1; break;
			case 0x8B50: s.comp = 2; break;
			case 0x8B51: s.comp = 3; break;
			case 0x8B52: s.comp = 4; break;
			case 0x8B5B: s.comp = 9; break;
			case 0x8B5C: s.comp = 16; break;
			case 0x1405: s.isInt = true; s.isUint = true; s.comp = 1; break;
			case 0x1404: case 0x8B56: s.isInt = true; s.comp = 1; break;
			case 0x8B53: case 0x8B57: s.isInt = true; s.comp = 2; break;
			case 0x8B54: case 0x8B58: s.isInt = true; s.comp = 3; break;
			case 0x8B55: case 0x8B59: s.isInt = true; s.comp = 4; break;
			default: continue;  // a sampler: its unit stays as set above
			}
			std::string base( nm );
			if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
				base.resize( base.size() - 3 );
			for ( GLint e = 0; e < std::max( sz, 1 ); e++ ) {
				const std::string el = sz > 1 ? base + "[" + std::to_string( e ) + "]" : std::string( nm );
				s.loc = f.getUniformLocation( prog, el.c_str() );
				if ( s.loc < 0 )
					continue;
				if ( s.isInt )
					f.getUniformiv( prog, s.loc, s.iv );
				else
					f.getUniformfv( prog, s.loc, s.fv );
				out.push_back( s );
			}
		}
	}
	auto restoreLinked = [&]( GLuint prog ) {
		const auto it = linked.find( prog );
		if ( it == linked.end() )
			return;
		for ( const UniSnap & s : it->second ) {
			if ( s.isUint )
				f.uniform1uiv( s.loc, 1, reinterpret_cast<const GLuint *>( s.iv ) );
			else if ( s.isInt )
				( s.comp == 1 ? f.uniform1iv : s.comp == 2 ? f.uniform2iv : s.comp == 3 ? f.uniform3iv : f.uniform4iv )( s.loc, 1, s.iv );
			else if ( s.comp == 9 )
				f.uniformMatrix3fv( s.loc, 1, GL_FALSE, s.fv );
			else if ( s.comp == 16 )
				f.uniformMatrix4fv( s.loc, 1, GL_FALSE, s.fv );
			else
				( s.comp == 1 ? f.uniform1fv : s.comp == 2 ? f.uniform2fv : s.comp == 3 ? f.uniform3fv : f.uniform4fv )( s.loc, 1, s.fv );
		}
	};
	GLuint vao = 0;
	genVertexArrays( 1, &vao );
	constexpr int kCases = 30;
	static char twinKeys[kCases], normKeys[kCases];  // one atlas twin a case (the atlases' sizes differ), one normal-map twin a case
	static char terrKeys[kCases];  // L8 terrain air: airTerr's twin a case
	static char envKeys[kCases];  // L8 term 2: volEnvHist's twin a case
	static char csmKeys[kCases], cloudKeys[kCases];  // L8 term 3: csmMap's shared target, ussCloudMap's twin a case
	static char noiseKeys[kCases];  // L8 term 4: volAirNoise's twin a case
	static char ussTerrKeys[kCases], ussContactKeys[kCases];  // L8 route lift 2: ussTerrMap's / ussContactMap's twin a case
	static char cubeKeys[kCases];  // L8 term 5: cellShadow's shared target a case
	static char roofKeys[kCases];  // L8 term 7: pcRoof's twin a case
	static char airKeys[kCases][5], volKeys[kCases];  // the air's twins a case, its volume's shared target a case
	if ( jobs.size() > size_t( kCases ) )
		return fatal( "more than " + std::to_string( kCases ) + " cases" );
	size_t jobAt = 0;

	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 15, fT2DMS, msColor );
		u1i( pUnpack, "src", 15 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		return out;
	};
	auto before = [&]( GLuint fb, float zd ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.3f, 0.45f, 0.7f, 0.5f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_SCISSOR_TEST );
		glScissor( 0, 0, W / 2, H );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		f.useProgram( pSlab );
		f.uniform1fv( f.getUniformLocation( pSlab, "zd" ), 1, &zd );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	auto reveal = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0 );
		glStencilFunc( GL_EQUAL, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		f.useProgram( pReveal );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
	};
	bool ok = true;
	struct RedSeen
	{
		const char * name;
		long long red;  // past bit 30 from L8 term 7 on
		bool seen;
	} reds[42] = { { "cull (radius 0)", 1, false }, { "corners (the strip's last two swapped)", 2, false }, { "near fade (dropped)", 4, false },
		{ "air (dropped)", 8, false }, { "look (dropped: the plain pipelines)", 16, false }, { "atlas (dropped: the zero texture)", 32, false },
		{ "light (dropped)", 128, false }, { "lamps' lights (dropped: the zero texel buffer)", 256, false },
		{ "cluster's globals (dropped: b0 zero)", 512, false }, { "cluster lists (dropped: the zero texel buffer)", 1024, false },
		{ "local sun (dropped: vpSun.w 0)", 2048, false }, { "terrain air (dropped: the zero airTerr)", 4096, false },
		{ "ambient's history (dropped: the zero volEnvHist)", 16384, false }, { "ambient (dropped: volTerms' bit 8)", 32768, false },
		{ "sun's shadow map (dropped: the zero D16 array)", 65536, false }, { "sun's shadow (dropped: volCsm 0)", 131072, false },
		{ "clouds' map (dropped: the zero texture)", 262144, false }, { "cascades 1 and 2 (swapped: csmMat)", 524288, false },
		{ "drift's noise (dropped: the zero volAirNoise)", 1048576, false }, { "drift (dropped: volAirDrift.x 0)", 2097152, false },
		{ "drift's octaves (swapped: volAirDriftUv)", 4194304, false }, { "lamps' cubes (dropped: the zero D16 cube array)", 8388608, false },
		{ "lamps' shadows (dropped: cellShadowOn 0)", 16777216, false }, { "lamps' shadow fade (dropped: cellShadowFadeOn 0)", 33554432, false },
		{ "lamps' gobos (dropped: cellGoboOn 0)", 67108864, false }, { "lamps' gobo table (dropped: cellGoboTbl 0)", 134217728, false },
		{ "lamps' gobo uv (flipped: cellGoboRot 3)", 268435456, false }, { "roof map (dropped: the zero pcRoof)", 536870912LL, false },
		{ "roof (off: pcRoofOn 0)", 1073741824LL, false }, { "roof map's axes (swapped: pcRoofCam x / y)", 2147483648LL, false },
		{ "interior (off: cellInterior 0)", 4294967296LL, false }, { "interior's ambient (off: cellHasDalc 0)", 8589934592LL, false },
		{ "ambient's GI rule (cellGiAmb 2)", 17179869184LL, false }, { "frame block (dropped: the cascade fields zero)", 34359738368LL, false },
		{ "terrain map (dropped: the zero texture)", 68719476736LL, false }, { "contact map (dropped: the zero texture)", 137438953472LL, false },
		{ "composition (off: the packed ussOn 0)", 274877906944LL, false }, { "contact lookup (axes swapped: ussContactInvSize)", 549755813888LL, false },
		{ "ReSTIR ratio (dropped: the zero texel buffer)", 1099511627776LL, false }, { "ReSTIR ratio (off: the packed cellRestirOn 0)", 2199023255552LL, false },
		{ "ReSTIR size (axes swapped: cellRestirSize)", 4398046511104LL, false }, { "ReSTIR block (not repacked)", 8796093022208LL, false } };
	using TexBufferFn = void( WINAPI * )( GLenum, GLenum, GLuint );
	const auto texBuffer = reinterpret_cast<TexBufferFn>( wglGetProcAddress( "glTexBuffer" ) );
	if ( !texBuffer )
		return fatal( "no glTexBuffer" );
	glEnable( 0x884F );  // GL_TEXTURE_CUBE_MAP_SEAMLESS, as the app (gltex.cpp; L8 term 5's cubes: Vulkan is always seamless)
	for ( Job & j : jobs ) {
		if ( j.hasTex && !uploadAtlas( dir + "/" + j.name, j.tex[size_t( j.atlasAt )] ) )
			return fatal( j.name + ": the atlas upload" );
		for ( Job::Buf & b : j.bufs ) {  // L8 term 2a: a texture buffer as celllights.cpp makes it (glTexBuffer on a GL buffer)
			const GLenum ifmt = b.fmt == "rgba32f" ? fRgba32f : b.fmt == "r32i" ? GLenum( 0x8235 ) : b.fmt == "rg32f" ? GLenum( 0x8230 ) : 0;
			const size_t bytes = size_t( b.texels ) * ( b.fmt == "r32i" ? 4 : b.fmt == "rg32f" ? 8 : 16 );  // (rg32f: L8 route lift 3)
			std::vector<char> data( bytes );
			std::ifstream bin( dir + "/" + j.name + "/" + b.file, std::ios::binary );
			if ( !ifmt || !bin.read( data.data(), std::streamsize( bytes ) ) )
				return fatal( j.name + ": the texture buffer " + b.name );
			f.genBuffers( 1, &b.buf );
			f.bindBuffer( kTexBuffer, b.buf );
			f.bufferData( kTexBuffer, std::ptrdiff_t( bytes ), data.data(), fStatic );
			f.bindBuffer( kTexBuffer, 0 );
			glGenTextures( 1, &b.tex );
			glBindTexture( kTexBuffer, b.tex );
			texBuffer( kTexBuffer, ifmt, b.buf );
			glBindTexture( kTexBuffer, 0 );
		}
		const GLuint atlas = j.hasTex ? j.tex[size_t( j.atlasAt )].gl : 0;
		if ( j.hasN() && !uploadAtlas( dir + "/" + j.name, j.tex[size_t( j.normAt )] ) )
			return fatal( j.name + ": the normal map upload" );
		const GLuint normMap = j.hasN() ? j.tex[size_t( j.normAt )].gl : 0;
		const GLuint pJob = j.look ? pLook : j.restir() ? pAllOn : j.frameUbo() ? pRoute : pPrecip;  // (L8 route lift 1: the route default program; lift 3: the all-on one)
		for ( int a = 0; a < 5 && j.fog; a++ ) {  // the volume in a shared target, as the app's
			TexIn & t = j.tex[size_t( j.airAt[a] )];
			GLuint into = 0;
			if ( a == 1 && !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &volKeys[jobAt] ), 1, t.w, t.h, t.d, fRgba16f, into, why ) )
				return fatal( j.name + ": the shared volume: " + why );
			if ( !upload( dir + "/" + j.name, t, into ) )
				return fatal( j.name + ": upload " + t.name );
		}
		if ( j.envAt >= 0 && !upload( dir + "/" + j.name, j.tex[size_t( j.envAt )], 0 ) )
			return fatal( j.name + ": upload volEnvHist" );
		const GLuint envTex = j.envAt >= 0 ? j.tex[size_t( j.envAt )].gl : 0;
		if ( j.noiseAt >= 0 && !upload( dir + "/" + j.name, j.tex[size_t( j.noiseAt )], 0 ) )
			return fatal( j.name + ": upload volAirNoise" );
		const GLuint noiseTex = j.noiseAt >= 0 ? j.tex[size_t( j.noiseAt )].gl : 0;
		GLuint csmTex = 0, cloudTex = 0;  // L8 term 3: the cascades as the app's (sharedTarget type 2, D16, csmTexParams' live state)
		if ( j.csmN > 0 ) {
			if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &csmKeys[jobAt] ), 2, j.csmN, j.csmN, 3, 0x81A5, csmTex, why ) )
				return fatal( j.name + ": the shared csmMap: " + why );
			const float one4[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			glBindTexture( fT2DA, csmTex );
			f.texSubImage3D( fT2DA, 0, 0, 0, 0, j.csmN, j.csmN, 3, 0x1902, 0x1403, j.csmData.data() );  // DEPTH_COMPONENT, UNSIGNED_SHORT
			glTexParameteri( fT2DA, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( fT2DA, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( fT2DA, GL_TEXTURE_WRAP_S, 0x812D );  // CLAMP_TO_BORDER
			glTexParameteri( fT2DA, GL_TEXTURE_WRAP_T, 0x812D );
			glTexParameterfv( fT2DA, GL_TEXTURE_BORDER_COLOR, one4 );
			glTexParameteri( fT2DA, 0x884C, 0x884E );  // COMPARE_MODE: COMPARE_REF_TO_TEXTURE
			glTexParameteri( fT2DA, 0x884D, GL_LEQUAL );
			glTexParameteri( fT2DA, 0x813D, 0 );  // MAX_LEVEL
			glBindTexture( fT2DA, 0 );
		}
		if ( j.cloudN > 0 )
			cloudTex = tex2d( j.cloudN, j.cloudN, fRgba16f, GL_RGBA, GL_FLOAT, j.cloudData.data(), true );
		GLuint ussTerrTex = 0, ussContactTex = 0;  // L8 route lift 2: the shadow set's maps as the app's (RG16 / R8, linear, clamp to edge)
		if ( j.ussTN > 0 ) {
			ussTerrTex = tex2d( j.ussTN, j.ussTN, 0x822C, fRg, 0x1403, j.ussTerrData.data(), true );  // RG16, UNSIGNED_SHORT
			ussContactTex = tex2d( j.ussCW, j.ussCH, fR8, fRed, GL_UNSIGNED_BYTE, j.ussContactData.data(), true );
		}
		GLuint cubeTex = 0;  // L8 term 5: the lamps' cubes as the app's (sharedTarget type 3, D16, shadowAlloc's state)
		if ( j.cubeN > 0 ) {
			if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &cubeKeys[jobAt] ), 3, j.cubeN, j.cubeN, 6 * 16, 0x81A5, cubeTex, why ) )  // BATCH1114: d is layers
				return fatal( j.name + ": the shared cellShadow: " + why );
			glBindTexture( 0x9009, cubeTex );
			f.texSubImage3D( 0x9009, 0, 0, 0, 0, j.cubeN, j.cubeN, 96, 0x1902, 0x1403, j.cubeData.data() );  // DEPTH_COMPONENT, UNSIGNED_SHORT
			glTexParameteri( 0x9009, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( 0x9009, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( 0x9009, GL_TEXTURE_WRAP_S, 0x812F );  // CLAMP_TO_EDGE
			glTexParameteri( 0x9009, GL_TEXTURE_WRAP_T, 0x812F );
			glTexParameteri( 0x9009, 0x8072, 0x812F );  // WRAP_R
			glTexParameteri( 0x9009, 0x884C, 0x884E );  // COMPARE_MODE: COMPARE_REF_TO_TEXTURE
			glTexParameteri( 0x9009, 0x884D, GL_LEQUAL );
			glTexParameteri( 0x9009, 0x813D, 0 );  // MAX_LEVEL
			glBindTexture( 0x9009, 0 );
		}
		GLuint roofTex = 0;  // L8 term 7: the roof map as the app's (R32F, NEAREST, clamp to edge; cellprecip.cpp)
		if ( j.roofNx > 0 )
			roofTex = tex2d( j.roofNx, j.roofNy, 0x822E, fRed, GL_FLOAT, j.roofData.data(), false );
		const void * uboOver = nullptr;  // the globals check's other b0 (bindJob uploads it in place of the case's)
		const void * vkGlobalOver = nullptr;  // the same bytes as Vulkan's b0 (the globals mirror)
		auto frameUpload = [&]( int how ) {  // L8 route lift 1: the block's bytes (std140, CsmFrameBlock): 0 the case's, 1 csmForce 1, 2 zero
			std::uint8_t blk[256] = {};
			if ( how != 2 ) {
				auto put = [&]( const char * nm, int at, int n, bool isInt ) {
					const auto it = j.uni.find( nm );
					for ( int k = 0; it != j.uni.end() && k < n && k < int( it->second.size() ); k++ ) {
						const std::int32_t vi = std::int32_t( it->second[size_t( k )] );
						const float vf = float( it->second[size_t( k )] );
						std::memcpy( blk + at + 4 * k, isInt ? static_cast<const void *>( &vi ) : static_cast<const void *>( &vf ), 4 );
					}
				};
				put( "csmMat", 0, 48, false );
				put( "csmInvRange", 192, 3, false );
				put( "csmForce", 204, 1, false );
				put( "csmParams", 208, 4, false );
				put( "csmSplitOffset", 224, 4, false );
				put( "csmProbe", 240, 1, true );
				put( "csmRed", 244, 1, true );
				if ( how == 1 ) {
					const float one = 1.0f;
					std::memcpy( blk + 204, &one, 4 );
				}
			}
			f.bindBuffer( fUbo, frameUboBuf );
			f.bufferData( fUbo, std::ptrdiff_t( sizeof( blk ) ), blk, fStatic );
			f.bindBufferBase( fUbo, 2, frameUboBuf );
		};
		auto bindJob = [&]( GLuint prog ) {
			f.useProgram( prog );
			if ( restoreOn )  // known start state (header)
				restoreLinked( prog );
			setUnis( prog, j.uni );
			bindUnit( 0, GL_TEXTURE_2D, atlas );
			u1i( prog, "pcTex", 0 );
			bindUnit( 7, GL_TEXTURE_2D, roofTex );  // L8 term 7 (none: read only with pcRoofOn 1)
			u1i( prog, "pcRoof", 7 );
			for ( int a = 0; a < 5; a++ ) {  // unfogged: nothing bound (fogOn 0, atAp off)
				const TexIn * t = j.fog ? &j.tex[size_t( j.airAt[a] )] : nullptr;
				bindUnit( kAirUnit + a, kAirType[a] == 1 ? fT3D : kAirType[a] == 2 ? fT2DA : GLenum( GL_TEXTURE_2D ), t ? t->gl : 0 );
				u1i( prog, kAirTex[a], kAirUnit + a );
			}
			// L8 terrain air: the air's grid at airTerr too (the app binds its pass grid to both; read only with airTerrOn)
			bindUnit( kTerrUnit, fT2DA, j.fog ? j.tex[size_t( j.airAt[3] )].gl : 0 );
			u1i( prog, "airTerr", kTerrUnit );
			bindUnit( kEnvUnit, fT3D, envTex );  // L8 term 2 (none: read only with volEnvGrid.w 1)
			u1i( prog, "volEnvHist", kEnvUnit );
			bindUnit( kCsmUnit, fT2DA, csmTex );  // L8 term 3 (none: read only with volCsm 1)
			u1i( prog, "csmMap", kCsmUnit );
			bindUnit( kCloudUnit, GL_TEXTURE_2D, cloudTex );
			u1i( prog, "ussCloudMap", kCloudUnit );
			bindUnit( kUssTerrUnit, GL_TEXTURE_2D, ussTerrTex );  // L8 route lift 2 (none: read only with ussOn 1)
			u1i( prog, "ussTerrMap", kUssTerrUnit );
			bindUnit( kUssContactUnit, GL_TEXTURE_2D, ussContactTex );
			u1i( prog, "ussContactMap", kUssContactUnit );
			bindUnit( kNoiseUnit, fT3D, noiseTex );  // L8 term 4 (none: read only with volAirDrift.x > 0)
			u1i( prog, "volAirNoise", kNoiseUnit );
			bindUnit( kCubeUnit, 0x9009, cubeTex );  // L8 term 5 (none: read only with cellShadowOn 1)
			u1i( prog, "cellShadow", kCubeUnit );
			if ( !j.fog ) {  // an unfogged case: the air fully off (a program keeps the last fogged case's uniforms)
				static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				u1i( prog, "fogOn", 0 );
				u1i( prog, "volOn", 0 );
				f.uniform4fv( f.getUniformLocation( prog, "atAp" ), 1, zero4 );
			}
			if ( !j.lit ) {  // an unlit case: GL's light chain off (a program keeps a lit case's uniforms)
				u1i( prog, "cellHasDir", 0 );
				u1i( prog, "cellInterior", 0 );
				u1i( prog, "airTerrOn", 0 );
			}
			bindUnit( kNormUnit, GL_TEXTURE_2D, normMap );
			u1i( prog, "pcTexN", kNormUnit );  // (not in the plain program: no location, nothing set)
			bindUnit( kLightsUnit, kTexBuffer, j.bufTex( "cellLights" ) );  // L8 term 2a (none: the zero buffer)
			u1i( prog, "cellLights", kLightsUnit );
			bindUnit( kClusterUnit, kTexBuffer, j.bufTex( "cellCluster" ) );
			u1i( prog, "cellCluster", kClusterUnit );
			bindUnit( kRestirUnit, kTexBuffer, j.bufTex( "cellRestir" ) );  // L8 route lift 3 (none: read only with cellRestirOn 1)
			u1i( prog, "cellRestir", kRestirUnit );
			f.useProgram( prog );
			f.bindBuffer( fUbo, ubo );  // L8 term 2b: the case's own b0 (zeros but the cluster case's projection)
			f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), uboOver ? uboOver : j.global.empty() ? zeros.data() : j.global.data(), fStatic );
			f.bindBufferBase( fUbo, 0, ubo );
			if ( j.frameUbo() )  // L8 route lift 1: the case's cascades into the frame block
				frameUpload( 0 );
		};
		// GL: cellprecip.cpp's state, its points
		std::vector<std::uint16_t> bg, refA, refB, refAn[4], refBn[4];
		std::vector<std::uint16_t> & refA2 = refAn[0], & refB2 = refBn[0];
		before( fbRef, j.zd );
		bg = readSamples( refCol );
		double glMs = 0.0;
		// pass 0 warms the driver (its first-use cost is not the draw's), pass 1 is timed and judged, passes 2-5 repeat it
		// (pass 2 is the repeat line's; passes 1-5 are the GL noise reference line's five draws)
		// (a fogged case's pass 6 draws with the air off: the air must change the picture, or the case does not test it)
		// (a look case's last pass draws cell_precip_gpu.prog instead: the look must change the picture too)
		// (a fogged case's last pass draws the same program without WW_CELL_FOGLEAK: the fogleak check, printed only)
		std::vector<std::uint16_t> refOff, refPlain, refNoFl, refRoof;  // (refRoof: L8 term 7's pass, pcRoofOn 0)
		std::vector<std::uint16_t> refPrt;  // L8 term 1: the WW_CELL_PRT variant's frame, its GI fed (the GI case)
		if ( const GLenum e = glGetError() )
			return fatal( j.name + ": a GL error before the reference draw (0x" + std::to_string( unsigned( e ) ) + ")" );
		const int passAir = j.fog ? 6 : -1, passPlain = j.look ? 6 + ( j.fog ? 1 : 0 ) : -1;
		const int passNoFl = j.fog ? 6 + 1 + ( j.look ? 1 : 0 ) : -1;
		const int passRoof = j.roof() ? 6 + ( j.fog ? 2 : 0 ) + ( j.look ? 1 : 0 ) : -1;  // L8 term 7: the roof off
		for ( int pass = 0; pass < 6 + ( j.fog ? 2 : 0 ) + ( j.look ? 1 : 0 ) + ( j.roof() ? 1 : 0 ); pass++ ) {
			before( fbRef, j.zd );
			bindJob( pass == passPlain ? pPrecip : pass == passNoFl ? ( j.look ? pLookNoFl : pNoFl ) : pJob );
			if ( pass == passAir ) {  // the air off: fogOn 0 alone leaves the volume and the aerial perspective on
				static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				u1i( pJob, "fogOn", 0 );
				u1i( pJob, "volOn", 0 );
				f.uniform4fv( f.getUniformLocation( pJob, "atAp" ), 1, zero4 );
			}
			if ( pass == passRoof )
				u1i( pJob, "pcRoofOn", 0 );
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_FALSE );
			glDisable( GL_CULL_FACE );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable( GL_STENCIL_TEST );
			glStencilMask( 0x03 );
			glStencilFunc( GL_ALWAYS, 1, 0xFF );
			glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			bindVertexArray( vao );
			const double g0 = nowMs();
			glDrawArrays( GL_POINTS, j.first, pass ? j.n : 1 );
			glFinish();
			if ( pass == 1 )
				glMs = nowMs() - g0;
			bindVertexArray( 0 );
			drawState0();
			glDepthFunc( GL_LESS );
			if ( pass == passAir )
				refOff = readSamples( refCol );
			else if ( pass == passPlain )
				refPlain = readSamples( refCol );
			else if ( pass == passNoFl )
				refNoFl = readSamples( refCol );
			else if ( pass == passRoof )
				refRoof = readSamples( refCol );
			else if ( pass ) {
				( pass == 1 ? refA : refAn[pass - 2] ) = readSamples( refCol );
				reveal( fbRef );
				( pass == 1 ? refB : refBn[pass - 2] ) = readSamples( refCol );
			}
		}
		if ( const GLenum e = glGetError() )
			return fatal( j.name + ": the GL reference draw (GL error 0x" + [&] { char s[16]; std::snprintf( s, sizeof( s ), "%04X", unsigned( e ) ); return std::string( s ); }() + ")" );
		std::printf( "PRECIPVIEW %s job: %dx%d x%d, run %d + %d, atlas %s, far %d, slab depth %.7f, GL draw %.2f ms\n", j.name.c_str(), W, H, S,
			j.first, j.n, j.hasTex ? ( j.tex[size_t( j.atlasAt )].fmt + ", every level, trilinear clamp" ).c_str() : "none", j.farT, double( j.zd ), glMs );
		if ( j.fog ) {  // the fixture's check, not the bar: samples the air changes past 2 ulp in GL's own picture
			const Verdict o = judge( bg, refOff, refA, refB, refB );
			std::printf( "PRECIPVIEW %s air check: GL with the air off differs from GL with the air on %zu of %zu covered samples (%s)\n", j.name.c_str(),
				o.flips, o.covered, o.flips >= 1000 ? "the air is seen" : "TOO FEW: the case does not test the air" );
			ok = ok && o.flips >= 1000;
			const Verdict n = judge( bg, refNoFl, refA, refB, refB );  // reference only: the define's reach in this fixture
			std::printf( "PRECIPVIEW %s fogleak check: GL without WW_CELL_FOGLEAK differs from GL with it %zu of %zu covered samples%s\n",
				j.name.c_str(), n.flips, n.covered, n.flips ? "" : " (0: this fixture does not reach the one-tail shape)" );
		}
		if ( j.look ) {  // the same: cell_precip_gpu.prog against cell_precip_look.prog on the same uniforms
			const Verdict o = judge( bg, refPlain, refA, refB, refB );
			std::printf( "PRECIPVIEW %s look check: GL's plain program differs from its look program %zu of %zu covered samples (%s)\n", j.name.c_str(),
				o.flips, o.covered, o.flips >= 1000 ? "the look is seen" : "TOO FEW: the case does not test the look" );
			ok = ok && o.flips >= 1000;
		}
		if ( j.roof() ) {  // L8 term 7's seen-checks (header): the roof seen, its names found
			const Verdict o = judge( bg, refRoof, refA, refB, refB );
			std::string umiss;
			for ( const char * nm : { "pcRoof", "pcRoofOn", "pcRoofCam", "pcRoofInv" } )
				if ( f.getUniformLocation( pJob, nm ) < 0 )
					umiss += std::string( umiss.empty() ? "" : ", " ) + nm;
			std::printf( "PRECIPVIEW %s roof checks: GL with pcRoofOn 0 differs %zu of %zu covered samples (want >= 1000); roof names missing "
				"from GL's program: %s\n", j.name.c_str(), o.flips, o.covered, umiss.empty() ? "none" : umiss.c_str() );
			ok = ok && o.flips >= 1000 && umiss.empty();
		}
		if ( j.lit ) {  // slice 2b's seen-checks (header): the light seen, thin vs thick seen, no NaN / Inf, the sun's names found
			auto drawGl = [&]( int mode ) {  // 1 cellHasDir 0, 2 pcTrans 1.0, 3 volTerms 1, 10 + k cellLightCount k; set back after
				before( fbRef, j.zd );
				bindJob( pJob );
				const float thick = 1.0f;
				if ( mode == 1 )
					u1i( pJob, "cellHasDir", 0 );
				else if ( mode == 2 )
					f.uniform1fv( f.getUniformLocation( pJob, "pcTrans" ), 1, &thick );
				else if ( mode == 3 )
					u1i( pJob, "volTerms", 1 );
				else if ( mode == 7 )  // L8 terrain air: the grid off (the absolute anchor)
					u1i( pJob, "airTerrOn", 0 );
				else if ( mode == 4 )  // L8 term 2b: the lists off (the flat loop)
					u1i( pJob, "cellClusterOn", 0 );
				else if ( mode == 5 ) {  // L8 term 2b: b0 all zero (no projection: vpLightsBegin walks every light)
					f.bindBuffer( fUbo, ubo );
					f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), zeros.data(), fStatic );
				} else if ( mode == 8 ) {  // L8 term 1: the WW_CELL_PRT variant, its GI fed (cellGiAir one bright constant)
					static GLuint giTex = 0;
					if ( !giTex ) {
						const float c[4] = { 0.9f, 0.7f, 0.5f, 1.0f };
						std::vector<float> d( 2 * 2 * 6 * 4 );
						for ( size_t i = 0; i < d.size(); i++ )
							d[i] = c[i % 4];
						glGenTextures( 1, &giTex );
						glBindTexture( fT3D, giTex );
						f.texImage3D( fT3D, 0, GLint( fRgba32f ), 2, 2, 6, 0, GL_RGBA, GL_FLOAT, d.data() );
						glTexParameteri( fT3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
						glTexParameteri( fT3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
						glBindTexture( fT3D, 0 );
					}
					bindJob( pPrt );
					bindUnit( kGiUnit, fT3D, giTex );
					u1i( pPrt, "cellGiAir", kGiUnit );
					u1i( pPrt, "cellGiOn", 1 );
					u1i( pPrt, "cellGiAirOn", 1 );
					const float org[3] = { 0.0f, 0.0f, 0.0f }, dims[3] = { 2.0f, 2.0f, 1.0f }, vox = 1000.0f;
					f.uniform3fv( f.getUniformLocation( pPrt, "cellGiAirOrigin" ), 1, org );
					f.uniform3fv( f.getUniformLocation( pPrt, "cellGiAirDims" ), 1, dims );
					f.uniform1fv( f.getUniformLocation( pPrt, "cellGiAirVoxel" ), 1, &vox );
				} else if ( mode == 20 ) {  // L8 term 2: the ambient's keep off (volEnvGrid.w 0)
					GLfloat g4[4] = {};
					f.getUniformfv( pJob, f.getUniformLocation( pJob, "volEnvGrid" ), g4 );
					g4[3] = 0.0f;
					f.uniform4fv( f.getUniformLocation( pJob, "volEnvGrid" ), 1, g4 );
				} else if ( mode == 21 ) {  // L8 term 2: no Ambient Only volumes
					u1i( pJob, "cellAmboCount", 0 );
				} else if ( mode == 22 ) {  // L8 term 2: the sphere alone (the box dropped)
					u1i( pJob, "cellAmboCount", 1 );
				} else if ( mode == 23 ) {  // L8 term 3: the sun's shadow off (volCsm 0)
					u1i( pJob, "volCsm", 0 );
				} else if ( mode == 24 ) {  // L8 term 3: the cascades forced lit (csmForce 1)
					const float one = 1.0f;
					f.uniform1fv( f.getUniformLocation( pJob, "csmForce" ), 1, &one );
					if ( j.frameUbo() )  // L8 route lift 1: a block member (glUniform is a no-op): the block's csmForce
						frameUpload( 1 );
				} else if ( mode == 25 ) {  // L8 term 3: the clouds off (ussCloudOn 0)
					u1i( pJob, "ussCloudOn", 0 );
				} else if ( mode == 26 ) {  // L8 term 3: the stable bias off (ussCsmStable.w 0, info)
					GLfloat s4[4] = {};
					f.getUniformfv( pJob, f.getUniformLocation( pJob, "ussCsmStable" ), s4 );
					s4[3] = 0.0f;
					f.uniform4fv( f.getUniformLocation( pJob, "ussCsmStable" ), 1, s4 );
				} else if ( mode == 27 ) {  // L8 term 4: the drift off (volAirDrift.x 0)
					GLfloat d4[4] = {};
					f.getUniformfv( pJob, f.getUniformLocation( pJob, "volAirDrift" ), d4 );
					d4[0] = 0.0f;
					f.uniform4fv( f.getUniformLocation( pJob, "volAirDrift" ), 1, d4 );
				} else if ( mode == 28 ) {  // L8 term 4: the noise read at one point (volAirDriftUv all zero)
					const GLfloat z24[24] = {};
					f.uniform4fv( f.getUniformLocation( pJob, "volAirDriftUv" ), 6, z24 );
				} else if ( mode == 29 || mode == 30 ) {  // L8 term 4: the drift's reds 1 (one sun tap) / 2 (the puff only), info
					u1i( pJob, "volAirDriftRed", mode - 28 );
				} else if ( mode == 31 ) {  // L8 term 5: the lamps' shadows off (cellShadowOn 0)
					u1i( pJob, "cellShadowOn", 0 );
				} else if ( mode == 32 ) {  // L8 term 5: the slots' fade off (cellShadowFadeOn 0)
					u1i( pJob, "cellShadowFadeOn", 0 );
				} else if ( mode == 33 ) {  // L8 term 5: the taps on one texel (cellShadowTexel 0), info
					const float zero = 0.0f;
					f.uniform1fv( f.getUniformLocation( pJob, "cellShadowTexel" ), 1, &zero );
				} else if ( mode == 34 ) {  // L8 term 6: the gobos off (cellGoboOn 0)
					u1i( pJob, "cellGoboOn", 0 );
				} else if ( mode == 35 ) {  // L8 term 6: each pattern's mean (cellGoboFlat 1)
					u1i( pJob, "cellGoboFlat", 1 );
				} else if ( mode == 36 ) {  // L8 term 6: u flipped (cellGoboRot 1)
					u1i( pJob, "cellGoboRot", 1 );
				} else if ( mode == 37 ) {  // L8 term 6: the paraboloid unmirrored (cellGoboMap 1), info
					u1i( pJob, "cellGoboMap", 1 );
				} else if ( mode == 38 ) {  // L8 term 8: the interior off (cellInterior 0)
					u1i( pJob, "cellInterior", 0 );
				} else if ( mode == 39 ) {  // L8 term 8: the interior's ambient off (cellHasDalc 0)
					u1i( pJob, "cellHasDalc", 0 );
				} else if ( mode == 40 ) {  // L8 term 8: the ambient under GI's rule (cellGiAmb 2)
					u1i( pJob, "cellGiAmb", 2 );
				} else if ( mode == 41 ) {  // L8 term 8: the exterior's ambient switch off (cellExtDalc 0), info
					u1i( pJob, "cellExtDalc", 0 );
				} else if ( mode == 42 ) {  // L8 term 8: the veil off (volVeil 0), info
					u1i( pJob, "volVeil", 0 );
				} else if ( mode == 9 ) {  // the case as it is (the restore check)
				} else if ( mode == 43 ) {  // L8 route lift 1: the frame block's bytes zero (the block seen)
					frameUpload( 2 );
				} else if ( mode == 44 ) {  // L8 route lift 2: the composition off (ussOn 0: CSM1's blend)
					u1i( pJob, "ussOn", 0 );
				} else if ( mode == 45 ) {  // L8 route lift 2: the terrain tier off (ussTerrOn 0)
					u1i( pJob, "ussTerrOn", 0 );
				} else if ( mode == 46 ) {  // L8 route lift 2: the contact tier off (ussContactOn 0)
					u1i( pJob, "ussContactOn", 0 );
				} else if ( mode == 47 ) {  // L8 route lift 3: the ReSTIR ratio off (cellRestirOn 0)
					u1i( pJob, "cellRestirOn", 0 );
				} else if ( mode == 6 ) {  // L8 term 3: the local law off (vpSun.w 0)
					const float off[4] = { 0.5f, 0.4f, 0.3f, 0.0f };
					f.uniform4fv( f.getUniformLocation( pJob, "vpSun" ), 1, off );
				} else
					u1i( pJob, "cellLightCount", mode - 10 );
				f.bindFramebuffer( fFb, fbRef );
				glViewport( 0, 0, W, H );
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				glDepthMask( GL_FALSE );
				glDisable( GL_CULL_FACE );
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				glEnable( GL_STENCIL_TEST );
				glStencilMask( 0x03 );
				glStencilFunc( GL_ALWAYS, 1, 0xFF );
				glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
				bindVertexArray( vao );
				glDrawArrays( GL_POINTS, j.first, j.n );
				bindVertexArray( 0 );
				drawState0();
				glDepthFunc( GL_LESS );
				std::vector<std::uint16_t> out = readSamples( refCol );
				bindJob( pJob );  // the case's own uniforms again
				return out;
			};
			const Verdict dark = judge( bg, drawGl( 1 ), refA, refB, refB );
			const Verdict thick = judge( bg, drawGl( 2 ), refA, refB, refB );
			size_t bad = 0;
			for ( std::uint16_t hv : refA )
				bad += ( hv & 0x7C00 ) == 0x7C00;
			const char * sunNames[] = { "cellOn", "cellHasDir", "cellInterior", "cellDirColor", "cellDirTo", "cellRow[0]", "volTerms",
				"volPhase[0]", "volGi", "pcTrans", "volVeil", "airMed", "airLayer", "airGround" };
			std::string missing;
			for ( int k = 0; k < int( std::size( sunNames ) ) - ( j.fog ? 0 : 4 ); k++ )
				if ( f.getUniformLocation( pJob, sunNames[k] ) < 0 )
					missing += std::string( missing.empty() ? "" : ", " ) + sunNames[k];
			std::printf( "PRECIPVIEW %s lit checks: GL with cellHasDir 0 differs %zu of %zu covered samples, pcTrans 1.0 vs 0.5 %zu (each want >= 1000); "
				"NaN / Inf halves in GL's lit frame %zu (want 0); sun-branch names missing from GL's program: %s\n", j.name.c_str(), dark.flips, dark.covered,
				thick.flips, bad, missing.empty() ? "none" : missing.c_str() );
			ok = ok && ( dark.flips >= 1000 || j.darkInterior() ) && thick.flips >= 1000 && bad == 0 && missing.empty();  // (L8 term 8: a dark interior's sun is off)
			if ( j.name == "rain_near_lit" ) {  // the restore check (header): a fogged case's uniforms left on, the restore skipped
				const Job * fogJob = nullptr;
				for ( const Job & o : jobs )
					if ( o.fog && o.lit && !fogJob )
						fogJob = &o;
				if ( fogJob ) {
					setUnis( pJob, fogJob->uni );
					restoreOn = false;
					const Verdict left = judge( bg, drawGl( 9 ), refA, refB, refB );
					restoreOn = true;
					bindJob( pJob );
					std::printf( "PRECIPVIEW %s restore check: GL with %s's uniforms left on and the restore skipped differs %zu of %zu covered "
						"samples (want >= 1000)\n", j.name.c_str(), fogJob->name.c_str(), left.flips, left.covered );
					ok = ok && left.flips >= 1000;
				} else {
					std::printf( "PRECIPVIEW %s restore check: no fogged lit case to take uniforms from\n", j.name.c_str() );
					ok = false;
				}
			}
			if ( j.localSun() ) {  // L8 term 3's seen-checks (header): the law seen, vpSun found
				const Verdict noLaw = judge( bg, drawGl( 6 ), refA, refB, refB );
				const bool found = f.getUniformLocation( pJob, "vpSun" ) >= 0;
				std::printf( "PRECIPVIEW %s local sun checks: GL with vpSun.w 0 differs %zu of %zu covered samples (want >= 1000); vpSun %s in GL's program\n",
					j.name.c_str(), noLaw.flips, noLaw.covered, found ? "found" : "MISSING" );
				ok = ok && noLaw.flips >= 1000 && found;
			}
			if ( j.csm() ) {  // L8 term 3's seen-checks (header): the shadow, the map and the clouds seen, their names found
				const Verdict noCsm = judge( bg, drawGl( 23 ), refA, refB, refB );
				const Verdict forced = judge( bg, drawGl( 24 ), refA, refB, refB );
				const Verdict noCloud = judge( bg, drawGl( 25 ), refA, refB, refB );
				const Verdict noStable = judge( bg, drawGl( 26 ), refA, refB, refB );
				const bool sfIn = f.getUniformBlockIndex( pJob, "wwShadowFrame" ) != 0xFFFFFFFFu;  // L8 route lift 1
				auto inBlock = []( const char * nm ) {
					for ( const char * b : { "csmMat", "csmInvRange", "csmForce", "csmParams", "csmSplitOffset", "csmProbe", "csmRed" } )
						if ( std::strcmp( nm, b ) == 0 )
							return true;
					return false;
				};
				std::string cmiss;
				for ( const char * nm : { "csmMap", "csmMat", "csmInvRange", "csmParams", "csmSplitOffset", "csmForce", "ussCsmStable", "ussCsmTexel",
						 "ussCsmBias", "ussCloudOn", "ussCloudMap", "ussCloudFromView", "ussCloudFar" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 && f.getUniformLocation( pJob, ( std::string( nm ) + "[0]" ).c_str() ) < 0
						&& !( sfIn && inBlock( nm ) ) )  // (a block member counts as found when the program holds wwShadowFrame)
						cmiss += std::string( cmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s sun's shadow checks: GL with volCsm 0 differs %zu of %zu covered samples, csmForce 1 %zu, ussCloudOn 0 %zu "
					"(each want >= 1000), ussCsmStable.w 0 %zu (info); shadow names missing from GL's program: %s\n", j.name.c_str(), noCsm.flips,
					noCsm.covered, forced.flips, noCloud.flips, noStable.flips, cmiss.empty() ? "none" : cmiss.c_str() );
				ok = ok && noCsm.flips >= 1000 && forced.flips >= 1000 && noCloud.flips >= 1000 && cmiss.empty();
				if ( j.frameUbo() ) {  // L8 route lift 1's seen-checks (header): the block seen, found, csmMat not loose, its bytes read
					const Verdict noBlk = judge( bg, drawGl( 43 ), refA, refB, refB );
					const bool loose = f.getUniformLocation( pJob, "csmMat" ) >= 0 || f.getUniformLocation( pJob, "csmMat[0]" ) >= 0;
					bindJob( pJob );
					WwVkPresent::PrecipRun tb;
					tb.lit = true;
					WwVkPresent::precipLitRead( &ga, pJob, tb );
					std::printf( "PRECIPVIEW %s frame block checks: GL with the block's bytes zero differs %zu of %zu covered samples (want >= 1000); "
						"wwShadowFrame %s in GL's program, csmMat %s; precipLitRead csmBlock %d (want 1)\n", j.name.c_str(), noBlk.flips,
						noBlk.covered, sfIn ? "found" : "MISSING", loose ? "LOOSE (a uniform)" : "not loose", tb.litIn.csmBlock );
					ok = ok && noBlk.flips >= 1000 && sfIn && !loose && tb.litIn.csmBlock == 1;
				}
				if ( j.uss() ) {  // L8 route lift 2's seen-checks (header): the composition and both tiers seen, their names found
					const Verdict noUss = judge( bg, drawGl( 44 ), refA, refB, refB );
					const Verdict noTerr = judge( bg, drawGl( 45 ), refA, refB, refB );
					const Verdict noCon = judge( bg, drawGl( 46 ), refA, refB, refB );
					std::string umiss;
					for ( const char * nm : { "ussOn", "ussTerrOn", "ussTerrMap", "ussTerrXform", "ussTerrRange", "ussWorldFromView", "ussContactOn",
							 "ussContactMap", "ussContactInvSize" } )
						if ( f.getUniformLocation( pJob, nm ) < 0 )
							umiss += std::string( umiss.empty() ? "" : ", " ) + nm;
					std::printf( "PRECIPVIEW %s composition checks: GL with ussOn 0 differs %zu of %zu covered samples, ussTerrOn 0 %zu, ussContactOn 0 %zu "
						"(each want >= 1000); composition names missing from GL's program: %s\n", j.name.c_str(), noUss.flips, noUss.covered,
						noTerr.flips, noCon.flips, umiss.empty() ? "none" : umiss.c_str() );
					ok = ok && noUss.flips >= 1000 && noTerr.flips >= 1000 && noCon.flips >= 1000 && umiss.empty();
				}
			}
			if ( j.drift() ) {  // L8 term 4's seen-checks (header): the drift and its noise seen, their names found
				const Verdict noDrift = judge( bg, drawGl( 27 ), refA, refB, refB );
				const Verdict flatNoise = judge( bg, drawGl( 28 ), refA, refB, refB );
				const Verdict oneSun = judge( bg, drawGl( 29 ), refA, refB, refB );
				const Verdict puffOnly = judge( bg, drawGl( 30 ), refA, refB, refB );
				std::string dmiss;
				for ( const char * nm : { "volAirDrift", "volAirDriftUv", "volAirDriftRed", "volAirNoise" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 && f.getUniformLocation( pJob, ( std::string( nm ) + "[0]" ).c_str() ) < 0 )
						dmiss += std::string( dmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s drift checks: GL with volAirDrift.x 0 differs %zu of %zu covered samples, volAirDriftUv zero %zu "
					"(each want >= 1000), volAirDriftRed 1 %zu, 2 %zu (info); drift names missing from GL's program: %s\n", j.name.c_str(),
					noDrift.flips, noDrift.covered, flatNoise.flips, oneSun.flips, puffOnly.flips, dmiss.empty() ? "none" : dmiss.c_str() );
				ok = ok && noDrift.flips >= 1000 && flatNoise.flips >= 1000 && dmiss.empty();
			}
			if ( j.gobo() ) {  // L8 term 6's seen-checks (header): the gobos, their texels and their uv seen, their names found
				const Verdict noGobo = judge( bg, drawGl( 34 ), refA, refB, refB );
				const Verdict flat = judge( bg, drawGl( 35 ), refA, refB, refB );
				const Verdict rot = judge( bg, drawGl( 36 ), refA, refB, refB );
				const Verdict map1 = judge( bg, drawGl( 37 ), refA, refB, refB );
				std::string umiss;
				for ( const char * nm : { "cellGoboOn", "cellGoboRot", "cellGoboFlat", "cellGoboGam", "cellGoboTbl", "cellGoboMap" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						umiss += std::string( umiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s gobo checks: GL with cellGoboOn 0 differs %zu of %zu covered samples, cellGoboFlat 1 %zu, "
					"cellGoboRot 1 %zu (each want >= 1000), cellGoboMap 1 %zu (info); gobo names missing from GL's program: %s\n", j.name.c_str(),
					noGobo.flips, noGobo.covered, flat.flips, rot.flips, map1.flips, umiss.empty() ? "none" : umiss.c_str() );
				ok = ok && noGobo.flips >= 1000 && flat.flips >= 1000 && rot.flips >= 1000 && umiss.empty();
			}
			if ( j.cube() ) {  // L8 term 5's seen-checks (header): the cubes and their fade seen, their names found
				const Verdict noShadow = judge( bg, drawGl( 31 ), refA, refB, refB );
				const Verdict noFade = judge( bg, drawGl( 32 ), refA, refB, refB );
				const Verdict oneTexel = judge( bg, drawGl( 33 ), refA, refB, refB );
				std::string umiss;
				for ( const char * nm : { "cellShadow", "cellShadowOn", "cellShadowTexel", "cellShadowFadeOn", "cellShadowFade" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 && f.getUniformLocation( pJob, ( std::string( nm ) + "[0]" ).c_str() ) < 0 )
						umiss += std::string( umiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s cube checks: GL with cellShadowOn 0 differs %zu of %zu covered samples, cellShadowFadeOn 0 %zu "
					"(each want >= 1000), cellShadowTexel 0 %zu (info); cube names missing from GL's program: %s\n", j.name.c_str(),
					noShadow.flips, noShadow.covered, noFade.flips, oneTexel.flips, umiss.empty() ? "none" : umiss.c_str() );
				ok = ok && noShadow.flips >= 1000 && noFade.flips >= 1000 && umiss.empty();
			}
			if ( j.restir() ) {  // L8 route lift 3's seen-checks (header): the ratio seen, its names found, the cubes compiled out
				const Verdict noRs = judge( bg, drawGl( 47 ), refA, refB, refB );
				std::string rmiss;
				for ( const char * nm : { "cellRestirOn", "cellRestir", "cellRestirSize" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						rmiss += std::string( rmiss.empty() ? "" : ", " ) + nm;
				const bool cubesIn = f.getUniformLocation( pJob, "cellShadow" ) >= 0 || f.getUniformLocation( pJob, "cellShadow[0]" ) >= 0;
				std::printf( "PRECIPVIEW %s ReSTIR checks: GL with cellRestirOn 0 differs %zu of %zu covered samples (want >= 1000); ReSTIR names "
					"missing from GL's program: %s; cellShadow in GL's program: %s (want no)\n", j.name.c_str(), noRs.flips, noRs.covered,
					rmiss.empty() ? "none" : rmiss.c_str(), cubesIn ? "yes" : "no" );
				ok = ok && noRs.flips >= 1000 && rmiss.empty() && !cubesIn;
			}
			if ( j.amb() ) {  // L8 term 2's seen-checks (header): the ambient seen, its names found, the volumes / the keep seen
				const Verdict noAmb = judge( bg, drawGl( 3 ), refA, refB, refB );
				std::string amiss;
				for ( const char * nm : { "cellDalc", "cellExtDalc", "cellAmboCount", "cellAmbo", "cellAmboK", "cellAmboBox", "volEnvGrid", "volEnvHist" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						amiss += std::string( amiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s ambient checks: GL with volTerms 1 differs %zu of %zu covered samples (want >= 1000); ambient names missing "
					"from GL's program: %s\n", j.name.c_str(), noAmb.flips, noAmb.covered, amiss.empty() ? "none" : amiss.c_str() );
				ok = ok && noAmb.flips >= 1000 && amiss.empty();
				if ( j.envKeep() ) {
					const Verdict noKeep = judge( bg, drawGl( 20 ), refA, refB, refB );
					const Verdict noB0 = judge( bg, drawGl( 5 ), refA, refB, refB );
					std::printf( "PRECIPVIEW %s ambient keep checks: GL with volEnvGrid.w 0 differs %zu of %zu covered samples, b0 all zero %zu (each want "
						">= 1000)\n", j.name.c_str(), noKeep.flips, noKeep.covered, noB0.flips );
					ok = ok && noKeep.flips >= 1000 && noB0.flips >= 1000;
				} else {
					const std::vector<std::uint16_t> none = drawGl( 21 ), sphere = drawGl( 22 );
					const Verdict noVol = judge( bg, none, refA, refB, refB );
					const Verdict noBox = judge( bg, sphere, refA, refB, refB );
					const Verdict noSph = judge( bg, none, sphere, refB, refB );
					std::printf( "PRECIPVIEW %s ambient volume checks: GL with cellAmboCount 0 differs %zu of %zu covered samples, cellAmboCount 1 (the box "
						"dropped) %zu, cellAmboCount 0 vs 1 (the sphere alone) %zu (each want >= 1000)\n", j.name.c_str(), noVol.flips, noVol.covered,
						noBox.flips, noSph.flips );
					ok = ok && noVol.flips >= 1000 && noBox.flips >= 1000 && noSph.flips >= 1000;
				}
			}
			if ( j.interior() ) {  // L8 term 8's seen-checks (header): the interior's law, its ambient switch and GI rule seen, the names
				const Verdict outside = judge( bg, drawGl( 38 ), refA, refB, refB );
				const Verdict noDalc = judge( bg, drawGl( 39 ), refA, refB, refB );
				const Verdict giAmb = judge( bg, drawGl( 40 ), refA, refB, refB );
				const Verdict noExt = judge( bg, drawGl( 41 ), refA, refB, refB );
				const std::string veil = j.fog ? std::to_string( judge( bg, drawGl( 42 ), refA, refB, refB ).flips ) : std::string( "-" );
				std::string imiss;
				for ( const char * nm : { "cellInterior", "cellHasDalc", "cellGiAmb", "cellExtDalc" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						imiss += std::string( imiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s interior checks: GL with cellInterior 0 differs %zu of %zu covered samples, cellHasDalc 0 %zu, cellGiAmb 2 %zu "
					"(each want >= 1000), cellExtDalc 0 %zu, volVeil 0 %s (info: unread inside, want 0); interior names missing from GL's program: %s\n",
					j.name.c_str(), outside.flips, outside.covered, noDalc.flips, giAmb.flips, noExt.flips, veil.c_str(), imiss.empty() ? "none" : imiss.c_str() );
				ok = ok && outside.flips >= 1000 && noDalc.flips >= 1000 && giAmb.flips >= 1000 && imiss.empty();
			}
			if ( j.terrAir() ) {  // L8 terrain air's seen-checks (header): the grid seen, its names found
				const Verdict flatGround = judge( bg, drawGl( 7 ), refA, refB, refB );
				std::string tmiss;
				for ( const char * nm : { "airTerr", "airTerrOn", "airTerrXf", "airTerrFineXf", "airTerrRef" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						tmiss += std::string( tmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s terrain air checks: GL with airTerrOn 0 differs %zu of %zu covered samples (want >= 1000); terrain names "
					"missing from GL's program: %s\n", j.name.c_str(), flatGround.flips, flatGround.covered, tmiss.empty() ? "none" : tmiss.c_str() );
				ok = ok && flatGround.flips >= 1000 && tmiss.empty();
			}
			if ( j.cluster() ) {  // L8 term 2b's seen-checks (header): the lists seen, b0's projection read, the names, volTerms 3 vs 1
				const Verdict noLamps = judge( bg, drawGl( 3 ), refA, refB, refB );
				const Verdict flat = judge( bg, drawGl( 4 ), refA, refB, refB );
				const Verdict noB0 = judge( bg, drawGl( 5 ), refA, refB, refB );
				std::string cmiss;
				for ( const char * nm : { "cellClusterOn", "cellCluster", "cellClusterDims", "cellClusterOrigin", "cellClusterDepth", "cellClusterLog",
						"cellClusterBase", "volRect" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						cmiss += std::string( cmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s cluster checks: GL with volTerms 1 differs %zu of %zu covered samples, cellClusterOn 0 %zu, b0 all zero %zu "
					"(each want >= 1000); cluster names missing from GL's program: %s\n", j.name.c_str(), noLamps.flips, noLamps.covered, flat.flips,
					noB0.flips, cmiss.empty() ? "none" : cmiss.c_str() );
				ok = ok && noLamps.flips >= 1000 && flat.flips >= 1000 && noB0.flips >= 1000 && cmiss.empty();
			} else if ( j.lamps() ) {  // L8 term 2a's seen-checks (header): the lamps seen, each shape seen, the far one unseen
				const Verdict noLamps = judge( bg, drawGl( 3 ), refA, refB, refB );
				std::vector<std::uint16_t> prev = drawGl( 10 );
				size_t shape[5] = {}, farDiff = 0, farAll = 0, farMoved = 0;
				for ( int k = 1; k <= 5; k++ ) {
					std::vector<std::uint16_t> cur = drawGl( 10 + k );
					const Verdict s = judge( bg, cur, prev, refB, refB );
					shape[k - 1] = s.flips;
					if ( k == 5 ) {  // out of reach (header): want 0 on every pixel GL drew alike; a pixel whose 4x merge moved between the draws is info
						farAll = s.diff;
						farDiff = cur.size() == prev.size() ? 0 : 1;
						for ( size_t p = 0; p * 16 < cur.size() && cur.size() == prev.size(); p++ ) {
							bool fc = true, fp = true;
							size_t d = 0;
							for ( size_t i = p * 16; i < p * 16 + 16; i++ ) {
								fc = fc && cur[i] == cur[p * 16 + i % 4];
								fp = fp && prev[i] == prev[p * 16 + i % 4];
								d += cur[i] != prev[i];
							}
							if ( fc != fp )
								farMoved += d != 0;
							else
								farDiff += d;
						}
					}
					prev.swap( cur );
				}
				std::string lmiss;
				for ( const char * nm : { "cellLightCount", "cellLights", "cellShadowOn", "cellGoboOn", "cellClusterOn", "cellLightPhys", "cellRed" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						lmiss += std::string( lmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s lamp checks: GL with volTerms 1 differs %zu of %zu covered samples (want >= 1000); light k vs k - 1: omni %zu, "
					"spot %zu, hemisphere %zu, box %zu (each want >= 1000), out of reach %zu halves on the pixels GL drew alike (want 0; info: every pixel "
					"%zu flips / %zu halves, %zu differing pixels whose 4x merge moved between the draws); lamp names missing from GL's program: %s\n",
					j.name.c_str(), noLamps.flips, noLamps.covered, shape[0], shape[1], shape[2], shape[3], farDiff, shape[4], farAll, farMoved,
					lmiss.empty() ? "none" : lmiss.c_str() );
				ok = ok && noLamps.flips >= 1000 && shape[0] >= 1000 && shape[1] >= 1000 && shape[2] >= 1000 && shape[3] >= 1000 && farDiff == 0
					&& lmiss.empty();
			}
			if ( j.gi() ) {  // L8 term 1's seen-check (header): the WW_CELL_PRT variant with its GI fed against GL's default frame
				refPrt = drawGl( 8 );
				const Verdict seen = judge( bg, refPrt, refA, refB, refB );
				std::string pmiss;
				for ( const char * nm : { "cellGiOn", "cellGiAir" } )
					if ( f.getUniformLocation( pPrt, nm ) < 0 )
						pmiss += std::string( pmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s GI seen: the WW_CELL_PRT variant with its GI fed differs from GL's default frame %zu of %zu covered samples "
					"(want >= 1000); its GI names missing: %s\n", j.name.c_str(), seen.flips, seen.covered, pmiss.empty() ? "none" : pmiss.c_str() );
				ok = ok && seen.flips >= 1000 && pmiss.empty();
			}
		}

		// the per-sample reference (header): GL's program per particle with blending off, blended per sample at 1x
		std::vector<std::uint16_t> refPS, refPSB;
		std::vector<char> mergedAt( size_t( W ) * H, 0 );
		size_t refRep = 0;  // halves the reference's second build differs in
		{
			const double r0 = nowMs();
			auto readBl = [&]( int k ) {
				std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
				f.bindFramebuffer( fFb, fbBl );
				glReadBuffer( fCol0 + GLenum( k ) );
				glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
				glReadBuffer( fCol0 );
				return out;
			};
			auto loadBg = [&]() {  // the frame before the particles into the blended frame and the last-written one
				for ( int k : { 0, 2 } ) {
					glBindTexture( GL_TEXTURE_2D, bl[k] );
					glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, WS, H, GL_RGBA, fHalf, bg.data() );
				}
				glBindTexture( GL_TEXTURE_2D, 0 );
			};
			auto halvesDiff = [&]( const std::vector<std::uint16_t> & x, const std::vector<std::uint16_t> & y ) {
				size_t n = x.size() == y.size() ? 0 : x.size() + 1;
				for ( size_t i = 0; i < std::min( x.size(), y.size() ); i++ )
					n += x[i] != y[i];
				return n;
			};
			auto probe = [&]() {
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				f.useProgram( pProbe );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
				drawState0();
			};
			// R1: one constant blended over the frame before, at 1x and at 4x
			loadBg();
			f.bindFramebuffer( fFb, fbBl0 );
			glViewport( 0, 0, WS, H );
			drawState0();
			probe();
			std::vector<std::uint16_t> p1( bg.size() );
			glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, p1.data() );
			before( fbRef, j.zd );
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			drawState0();
			probe();
			glDepthFunc( GL_LESS );
			const size_t r1 = halvesDiff( p1, readSamples( refCol ) );
			// R2's GL side: every particle in one 4x draw, blending off (the last written output a sample)
			before( fbRef, j.zd );
			bindJob( pJob );
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_FALSE );
			bindVertexArray( vao );
			glDrawArrays( GL_POINTS, j.first, j.n );
			bindVertexArray( 0 );
			drawState0();
			glDepthFunc( GL_LESS );
			const std::vector<std::uint16_t> off4 = readSamples( refCol );
			// the loop (refDs holds the slab from before(): the capture's depth test reads it, writes none); run twice, the
			// second build is the repeat clause's (GL's 4x frame varies run to run on the merged pixels)
			auto perSample = [&]() {
			loadBg();
			f.bindFramebuffer( fFb, fbBl );
			static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, none[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
			clearBufferfv( GL_COLOR, 1, zero4 );
			u1i( pCapBlend, "cap", 16 );
			u1i( pCapBlend, "S", S );
			bindUnit( 16, fT2DMS, capCol );
			bindJob( pJob );
			for ( int i = 0; i < j.n; i++ ) {
				f.bindFramebuffer( fFb, fbCap );
				glViewport( 0, 0, W, H );
				drawState0();
				clearBufferfv( GL_COLOR, 0, none );
				f.useProgram( pJob );
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				bindVertexArray( vao );
				glDrawArrays( GL_POINTS, j.first + i, 1 );
				bindVertexArray( 0 );
				f.bindFramebuffer( fFb, fbBl );
				glViewport( 0, 0, WS, H );
				drawState0();
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				blendEquationi( 1, 0x8008 );  // GL_MAX: the split flag
				disablei( GL_BLEND, 2 );
				f.useProgram( pCapBlend );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
			}
			blendEquationi( 1, 0x8006 );  // GL_FUNC_ADD
			drawState0();
			glDepthFunc( GL_LESS );
			bindUnit( 16, fT2DMS, 0 );
			};
			perSample();
			refPS = readBl( 0 );
			const std::vector<std::uint16_t> split = readBl( 1 ), last = readBl( 2 );
			const size_t r2 = halvesDiff( last, off4 );
			const double rMs = nowMs() - r0;
			perSample();
			refRep = halvesDiff( readBl( 0 ), refPS );
			// R3 on the never-split pixels; the merge record on the split ones (a pixel's 4 samples: (p * 4 + s) * 4 + c)
			// (R3 also on GL's draws 2-5: GL's run-to-run variation must sit on split pixels only)
			size_t r3 = 0, r3n = 0, nSplit = 0, merged = 0, mergedMean = 0, splitSame = 0, rep = 0, repFlat = 0;
			bool sizes = refA.size() == refPS.size();
			for ( int k = 0; k < 4; k++ )
				sizes = sizes && refAn[k].size() == refPS.size();
			for ( size_t p = 0; p < size_t( W ) * H && sizes; p++ ) {
				const size_t s0 = p * 4;
				bool sp = false;
				for ( size_t s = 0; s < 4; s++ )
					sp = sp || split[( s0 + s ) * 4] != 0;
				if ( !sp ) {
					for ( size_t i = s0 * 4; i < ( s0 + 4 ) * 4; i++ ) {
						r3 += refPS[i] != refA[i];
						for ( int k = 0; k < 4; k++ )
							r3n += refPS[i] != refAn[k][i];
					}
					continue;
				}
				nSplit++;
				{  // GL's draw 2 against draw 1 here (info): differs, and GL one value in either draw
					bool d = false, f1 = true, f2 = true;
					for ( size_t i = s0 * 4; i < ( s0 + 4 ) * 4; i++ ) {
						d = d || refA[i] != refA2[i];
						f1 = f1 && refA[i] == refA[s0 * 4 + i % 4];
						f2 = f2 && refA2[i] == refA2[s0 * 4 + i % 4];
					}
					rep += d;
					repFlat += d && ( f1 || f2 );
				}
				bool glFlat = true, refFlat = true, same = true;
				for ( size_t s = 1; s < 4; s++ )
					for ( size_t c = 0; c < 4; c++ ) {
						glFlat = glFlat && refA[( s0 + s ) * 4 + c] == refA[s0 * 4 + c];
						refFlat = refFlat && refPS[( s0 + s ) * 4 + c] == refPS[s0 * 4 + c];
					}
				for ( size_t i = s0 * 4; i < ( s0 + 4 ) * 4; i++ )
					same = same && refPS[i] == refA[i];
				splitSame += same;
				if ( glFlat && !refFlat ) {
					merged++;
					mergedAt[p] = 1;
					bool mean = true;
					for ( size_t c = 0; c < 4; c++ ) {
						float m = 0.0f;
						for ( size_t s = 0; s < 4; s++ )
							m += h2f( refPS[( s0 + s ) * 4 + c] );
						mean = mean && std::abs( ordered( f2h( m * 0.25f ) ) - ordered( refA[s0 * 4 + c] ) ) <= 1;
					}
					mergedMean += mean;
				}
			}
			const bool refOk = r1 == 0 && r2 == 0 && r3 == 0 && r3n == 0 && sizes;
			std::printf( "PRECIPVIEW %s reference: %d particles one by one in %.0f ms; R1 constant blend 1x vs 4x %zu halves differ, R2 blend off "
				"1x vs GL 4x %zu, R3 vs GL's 4x frame on %zu never-split pixels %zu, the same on GL's draws 2-5 %zu (each want 0): %s\n", j.name.c_str(),
				j.n, rMs, r1, r2, size_t( W ) * H - nSplit, r3, r3n, refOk ? "OK" : "FAILED" );
			std::printf( "PRECIPVIEW %s GL merged pixels (info, NVIDIA's 4x blend): of %zu split pixels GL holds one value on %zu where the reference "
				"is split (%zu of them the reference's mean within 1 ulp), %zu equal per sample, %zu other; GL draw 2 vs 1 differs on %zu split pixels, "
				"%zu of them one value in either draw\n", j.name.c_str(), nSplit, merged, mergedMean, splitSame, nSplit - merged - splitSame, rep, repFlat );
			ok = ok && refOk;
			// the stencil is GL's 4x one: its samples take GL's reveal, the rest the reference
			refPSB = refPS;
			for ( size_t s = 0; s < refB.size() / 4 && refB.size() == refPS.size(); s++ )
				if ( refB[s * 4] == 0 && refB[s * 4 + 1] == 0 && refB[s * 4 + 2] == 0 && refB[s * 4 + 3] == 0x3C00 )
					for ( size_t c = 0; c < 4; c++ )
						refPSB[s * 4 + c] = refB[s * 4 + c];
		}

		const GLuint pJobCase = pJob;
		GLuint vkProgOver = 0;  // L8 term 1: Vulkan's run read from another GL program (the WW_CELL_PRT variant)
		auto runVk = [&]( long long red, std::vector<std::uint16_t> & a, std::vector<std::uint16_t> & b, bool checkSeed, std::uint8_t * recOut = nullptr ) -> bool {
			const GLuint pJob = vkProgOver ? vkProgOver : pJobCase;
			before( fbFrame, j.zd );
			bindJob( pJob );
			f.bindFramebuffer( fFb, fbFrame );
			glViewport( 0, 0, W, H );
			WwVkPresent::PrecipRun t;
			t.w = W;
			t.h = H;
			glGetIntegerv( GL_VIEWPORT, t.vp );
			t.color = fc;
			t.first = j.first;
			t.n = j.n;
			t.roofOn = j.roof();  // precipDraw's (L8 term 7): pcRoofOn, then GL's pcRoof into t0 (the lit pass's t47)
			if ( t.roofOn ) {
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "pcRoof", 0, t.tex[0], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (pcRoof): %s\n", why.c_str() );
					return false;
				}
				t.tex[0].twin = reinterpret_cast<std::uintptr_t>( &roofKeys[jobAt] );
			}
			if ( !WwVkPresent::fxTexOf( &ga, pJob, "pcTex", 0, t.tex[1], why, true ) ) {
				std::printf( "PRECIPVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[1].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[jobAt] );
			t.fogged = j.fog != 0;
			t.look = j.look != 0;
			t.lit = j.lit != 0;
			t.global = vkGlobalOver ? vkGlobalOver : j.global.empty() ? nullptr : j.global.data();  // L8 term 2b: GL's b0 bytes (the app: glcontext.cpp's last upload)
			if ( t.lit )  // precipDraw's order: the switches from GL, the refusals, then the packs
				WwVkPresent::precipLitRead( &ga, pJob, t );
			if ( red & 8192 )  // red 8192 (L8 term 1): the GI program refusal's input dropped
				t.litIn.gi = 0;
			if ( !WwVkPresent::precipAdmits( t, why ) ) {
				std::printf( "PRECIPVIEW precipAdmits refused: %s\n", why.c_str() );
				return false;
			}
			if ( t.lit && ( t.litIn.terms & 2 ) && ( !WwVkPresent::precipTexBufOf( &ga, pJob, "cellLights", t.lights, why )
				|| !WwVkPresent::precipTexBufOf( &ga, pJob, "cellCluster", t.cluster, why ) ) ) {  // precipDraw's (L8 term 2a)
				std::printf( "PRECIPVIEW precipTexBufOf refused: %s\n", why.c_str() );
				return false;
			}
			if ( t.lit && ( t.litIn.terms & 2 ) && t.litIn.restir >= 0
				&& !WwVkPresent::precipTexBufOf( &ga, pJob, "cellRestir", t.restirBuf, why ) ) {  // precipDraw's (L8 route lift 3)
				std::printf( "PRECIPVIEW precipTexBufOf refused (cellRestir): %s\n", why.c_str() );
				return false;
			}
			for ( int a = 0; a < 5 && j.fog; a++ ) {
				if ( !WwVkPresent::fxTexOf( &ga, pJob, kAirTex[a], kAirType[a], t.tex[2 + a], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (%s): %s\n", kAirTex[a], why.c_str() );
					return false;
				}
				t.tex[2 + a].twin = reinterpret_cast<std::uintptr_t>( &airKeys[jobAt][a] );
			}
			if ( t.lit && t.litIn.airTerr > 0 ) {  // precipDraw's (L8 terrain air): GL's airTerr into the lit set's t17
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "airTerr", 2, t.tex[8], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (airTerr): %s\n", why.c_str() );
					return false;
				}
				t.tex[8].twin = reinterpret_cast<std::uintptr_t>( &terrKeys[jobAt] );
			}
			if ( t.lit && ( t.litIn.terms & 8 ) ) {  // precipDraw's (L8 term 2): GL's volEnvHist into the lit set's t19
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "volEnvHist", 1, t.tex[9], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (volEnvHist): %s\n", why.c_str() );
					return false;
				}
				t.tex[9].twin = reinterpret_cast<std::uintptr_t>( &envKeys[jobAt] );
			}
			if ( t.lit && t.litIn.csm > 0 && t.litIn.interior == 0 ) {  // precipDraw's (L8 term 3): csmMap into t31, ussCloudMap into t34
				if ( !WwVkPresent::precipCsmOf( &ga, pJob, t.tex[10], why ) ) {
					std::printf( "PRECIPVIEW precipCsmOf refused (csmMap): %s\n", why.c_str() );
					return false;
				}
				if ( t.litIn.cloud > 0 ) {
					if ( !WwVkPresent::fxTexOf( &ga, pJob, "ussCloudMap", 0, t.tex[11], why ) ) {
						std::printf( "PRECIPVIEW fxTexOf refused (ussCloudMap): %s\n", why.c_str() );
						return false;
					}
					t.tex[11].twin = reinterpret_cast<std::uintptr_t>( &cloudKeys[jobAt] );
				}
				if ( t.litIn.uss > 0 && t.litIn.ussTerr > 0 ) {  // precipDraw's (L8 route lift 2): ussTerrMap into t32
					if ( !WwVkPresent::fxTexOf( &ga, pJob, "ussTerrMap", 0, t.tex[14], why ) ) {
						std::printf( "PRECIPVIEW fxTexOf refused (ussTerrMap): %s\n", why.c_str() );
						return false;
					}
					t.tex[14].twin = reinterpret_cast<std::uintptr_t>( &ussTerrKeys[jobAt] );
				}
				if ( t.litIn.uss > 0 && t.litIn.ussContact > 0 ) {  // and ussContactMap into t33
					if ( !WwVkPresent::fxTexOf( &ga, pJob, "ussContactMap", 0, t.tex[15], why ) ) {
						std::printf( "PRECIPVIEW fxTexOf refused (ussContactMap): %s\n", why.c_str() );
						return false;
					}
					t.tex[15].twin = reinterpret_cast<std::uintptr_t>( &ussContactKeys[jobAt] );
				}
			}
			if ( t.lit && ( t.litIn.terms & 2 ) && t.litIn.shadow > 0 ) {  // precipDraw's (L8 term 5): cellShadow into t6
				if ( !WwVkPresent::precipCubeOf( &ga, pJob, t.tex[13], why ) ) {
					std::printf( "PRECIPVIEW precipCubeOf refused (cellShadow): %s\n", why.c_str() );
					return false;
				}
			}
			if ( t.lit && t.litIn.drift > 0.0f ) {  // precipDraw's (L8 term 4): volAirNoise into t30
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "volAirNoise", 1, t.tex[12], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (volAirNoise): %s\n", why.c_str() );
					return false;
				}
				t.tex[12].twin = reinterpret_cast<std::uintptr_t>( &noiseKeys[jobAt] );
			}
			WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), t.cs );
			WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_precipps, int( std::size( WwVkPresent::kT_precipps ) ), t.ps );
			t.packs = 2;
			if ( checkSeed && j.roof() ) {  // the roof's switch packed, not zero (the roof bar; kT_precipcs's head)
				const int at = precipAt( WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), "pcRoofOn" );
				bool any = false;
				for ( int b = 0; b < 4 && at >= 0; b++ )
					any = any || t.cs[at + b] != 0;
				std::printf( "PRECIPVIEW %s roof packed: pcRoofOn %s\n", j.name.c_str(), any ? "non-zero" : "zero or absent" );
				if ( !any )
					return false;
			}
			if ( j.look ) {  // slice 3: cell_precip_look.prog's run (its fields, pcTexN at tex[7])
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplookcs, int( std::size( WwVkPresent::kT_preciplookcs ) ), t.cs );
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplookps, int( std::size( WwVkPresent::kT_preciplookps ) ), t.ps );
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "pcTexN", 0, t.tex[7], why, true ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (pcTexN): %s\n", why.c_str() );
					return false;
				}
				t.tex[7].twin = reinterpret_cast<std::uintptr_t>( &normKeys[jobAt] );
			}
			if ( j.lit ) {  // slice 2b: the light chain's block, as precipDraw packs it
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), t.cs );
				WwVkPresent::precipFrameOver( t );  // precipDraw's (L8 route lift 1): the cascades from GL's frame block
				t.litBlock = true;
				if ( t.litIn.restir >= 0 ) {  // precipDraw's (L8 route lift 3): the ReSTIR variant's two inserted fields
					WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplitrtcs, int( std::size( WwVkPresent::kT_preciplitrtcs ) ), t.rt );
					if ( checkSeed && j.restir() ) {  // packed, not zero (the ReSTIR bar)
						bool on = false, size = false;
						for ( int b = 0; b < 4; b++ )
							on = on || t.rt[b] != 0;
						for ( int b = 16; b < 32; b++ )
							size = size || t.rt[b] != 0;
						std::printf( "PRECIPVIEW %s ReSTIR packed: cellRestirOn %s, cellRestirSize %s\n", j.name.c_str(), on ? "non-zero" : "zero or absent",
							size ? "non-zero" : "zero or absent" );
						if ( !on || !size )
							return false;
					}
				}
				if ( checkSeed ) {  // the sun's inputs packed, not zero (header)
					std::string zero;
					for ( const char * nm : { "cellDirColor", "cellDirTo", "cellRow", "volPhase", "volGi", "cellLightCount" } ) {
						if ( !j.lamps() && std::strcmp( nm, "cellLightCount" ) == 0 )
							continue;  // (the sun cases: 0)
						const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), nm );
						bool any = false;
						for ( int b = 0; b < ( std::strcmp( nm, "cellLightCount" ) == 0 ? 4 : 12 ) && at >= 0; b++ )
							any = any || t.cs[at + b] != 0;
						if ( !any )
							zero += std::string( zero.empty() ? "" : ", " ) + nm;
					}
					if ( j.cluster() ) {  // the grid and volRect packed, not zero (cluster bar)
						const struct { const char * nm; int bytes; } cf[5] = {
							{ "cellClusterDims", 16 }, { "cellClusterDepth", 16 }, { "cellClusterLog", 8 }, { "cellClusterBase", 4 }, { "volRect", 16 } };
						for ( const auto & f : cf ) {
							int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), f.nm );
							const unsigned char * src = t.cs;
							if ( at < 0 ) {
								at = precipAt( WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), f.nm );
							}
							bool any = false;
							for ( int b = 0; b < f.bytes && at >= 0; b++ )
								any = any || src[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + f.nm;
						}
					}
					if ( j.localSun() ) {  // vpSun packed, not zero (local sun bar)
						const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), "vpSun" );
						bool any = false;
						for ( int b = 0; b < 16 && at >= 0; b++ )
							any = any || t.cs[at + b] != 0;
						if ( !any )
							zero += std::string( zero.empty() ? "" : ", " ) + "vpSun";
					}
					if ( j.terrAir() ) {  // airTerrOn and airTerrXf packed, not zero (terrain air bar)
						for ( const auto & tf : { std::make_pair( "airTerrOn", 4 ), std::make_pair( "airTerrXf", 16 ) } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf.first );
							bool any = false;
							for ( int b = 0; b < tf.second && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf.first;
						}
					}
					if ( j.csm() ) {  // the cascades' and the clouds' fields packed, not zero (the sun's shadow bar)
						for ( const auto & tf : { std::make_pair( "csmMat", 192 ), std::make_pair( "csmParams", 16 ), std::make_pair( "csmSplitOffset", 16 ),
								 std::make_pair( "ussCsmBias", 12 ), std::make_pair( "ussCloudFromView", 64 ), std::make_pair( "ussCloudFar", 16 ) } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf.first );
							bool any = false;
							for ( int b = 0; b < tf.second && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf.first;
						}
					}
					if ( j.uss() ) {  // the composition's switches packed, not zero (the composition bar)
						for ( const char * tf : { "ussOn", "ussTerrOn", "ussContactOn" } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf );
							bool any = false;
							for ( int b = 0; b < 4 && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf;
						}
					}
					if ( j.gobo() ) {  // the gobos' fields packed, not zero (the gobo bar)
						for ( const char * tf : { "cellGoboOn", "cellGoboTbl" } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf );
							bool any = false;
							for ( int b = 0; b < 4 && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf;
						}
					}
					if ( j.cube() ) {  // the cubes' fields packed, not zero (the shadow-cube bar)
						for ( const auto & tf : { std::make_pair( "cellShadowOn", 4 ), std::make_pair( "cellShadowTexel", 4 ), std::make_pair( "cellShadowFade", 256 ) } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf.first );
							bool any = false;
							for ( int b = 0; b < tf.second && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf.first;
						}
					}
					if ( j.drift() ) {  // the drift's fields packed, not zero (the drift bar; kT_precipcs's head)
						for ( const auto & tf : { std::make_pair( "volAirDrift", 16 ), std::make_pair( "volAirDriftUv", 96 ) } ) {
							const int at = precipAt( WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), tf.first );
							bool any = false;
							for ( int b = 0; b < tf.second && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf.first;
						}
					}
					if ( j.amb() ) {  // cellDalc, cellExtDalc (and the keep's volEnvGrid) packed, not zero (ambient bar)
						for ( const auto & tf : { std::make_pair( "cellDalc", 48 ), std::make_pair( "cellExtDalc", 4 ), std::make_pair( "volEnvGrid", 16 ) } ) {
							if ( std::strcmp( tf.first, "volEnvGrid" ) == 0 && !j.envKeep() )
								continue;
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf.first );
							bool any = false;
							for ( int b = 0; b < tf.second && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf.first;
						}
					}
					if ( j.interior() )  // cellInterior and cellHasDalc packed, not zero (the interior bar)
						for ( const char * nm : { "cellInterior", "cellHasDalc" } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), nm );
							bool any = false;
							for ( int b = 0; b < 4 && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + nm;
						}
					std::printf( "PRECIPVIEW %s lit packed: zero or absent in the light block: %s\n", j.name.c_str(), zero.empty() ? "none" : zero.c_str() );
					if ( !zero.empty() )
						return false;
				}
			}
			if ( checkSeed ) {
				std::uint32_t seed = 0;
				std::memcpy( &seed, t.cs, 4 );  // pcSeedS at 0
				std::printf( "PRECIPVIEW %s pcSeedS: set %u, fxPack read %u (%s)\n", j.name.c_str(), j.seed, seed, seed == j.seed ? "same bits" : "DIFFERS" );
				if ( seed != j.seed )
					return false;
			}
			t.red = red & ~8192;
			t.recOut = recOut;
			f.bindFramebuffer( fFb, fbFrame );
			if ( !WwVkPresent::precipRun( t, why ) ) {
				std::printf( "PRECIPVIEW precipRun refused: %s\n", why.c_str() );
				return false;
			}
			a = readSamples( fc );
			reveal( fbFrame );
			b = readSamples( fc );
			return glGetError() == GL_NO_ERROR;
		};
		{
			std::vector<std::uint16_t> a, b;
			if ( !runVk( 0, a, b, true ) ) {
				std::printf( "PRECIPVIEW %s FAIL (the Vulkan run failed)\n", j.name.c_str() );
				ok = false;
			} else {
				const Verdict v = judge( bg, a, refPS, b, refPSB );
				std::printf( "PRECIPVIEW %s %s (%zu samples covered, %zu halves differ, %zu edge flips, %zu halves past 1 ulp outside them, "
					"max %d ulp outside them, stencil differs on %zu; flips by worst half 3-8 / 9-64 / past 64 ulp: %zu / %zu / %zu; "
					"in whole / partly flipped pixels: %zu / %zu)\n", j.name.c_str(), v.word(), v.covered, v.diff, v.flips, v.past1, v.maxu,
					v.stencil, v.flipBy[0], v.flipBy[1], v.flipBy[2], v.flipWhole, v.flipPart );
				ok = ok && v.ok();
				{  // info, never the bar: against GL's 4x frame, and how many of those flipped pixels are GL's merged ones
					const Verdict g4 = judge( bg, a, refA, b, refB );
					size_t fp = 0, fpm = 0;
					for ( size_t p = 0; p < mergedAt.size() && a.size() == refA.size(); p++ ) {
						int m = 0;
						for ( size_t i = p * 16; i < p * 16 + 16; i++ )
							m = std::max( m, std::abs( ordered( a[i] ) - ordered( refA[i] ) ) );
						fp += m > 2;
						fpm += m > 2 && mergedAt[p];
					}
					std::printf( "PRECIPVIEW %s against GL's 4x frame (info): %s (%zu flips, %zu past 1 ulp, stencil %zu); %zu flipped pixels, %zu of them "
						"GL's merged ones\n", j.name.c_str(), g4.word(), g4.flips, g4.past1, g4.stencil, fp, fpm );
				}
				{  // red 64 (harness only): Vulkan's 4 samples averaged per pixel, the reveal's samples kept
					std::vector<std::uint16_t> am( a.size() ), bm( b );
					for ( size_t p = 0; p < a.size() / 16; p++ )
						for ( size_t c = 0; c < 4; c++ ) {
							float m = 0.0f;
							for ( size_t s = 0; s < 4; s++ )
								m += h2f( a[( p * 4 + s ) * 4 + c] );
							for ( size_t s = 0; s < 4; s++ )
								am[( p * 4 + s ) * 4 + c] = f2h( m * 0.25f );
						}
					for ( size_t s = 0; s < b.size() / 4; s++ )
						if ( !( b[s * 4] == 0 && b[s * 4 + 1] == 0 && b[s * 4 + 2] == 0 && b[s * 4 + 3] == 0x3C00 ) )
							for ( size_t c = 0; c < 4; c++ )
								bm[s * 4 + c] = am[s * 4 + c];
					const Verdict r = judge( bg, am, refPS, bm, refPSB );
					const bool must = j.name.find( "_near" ) != std::string::npos;
					std::printf( "PRECIPVIEW %s red 64 (Vulkan's samples averaged): %s (%zu covered, %zu flips, %zu past 1 ulp, stencil %zu)%s\n", j.name.c_str(),
						r.word(), r.covered, r.flips, r.past1, r.stencil, must && r.ok() ? " PASSED in a near case: the gate cannot see it" : "" );
					ok = ok && !( must && r.ok() );
				}
				// repeatability: the judged reference against its second build, GL's stencil against its second draw, Vulkan
				// against a second run (each must be EXACT); GL's 4x colour against its second draw is info (the merge varies)
				std::vector<std::uint16_t> a2, b2;
				const Verdict g = judge( bg, refA2, refA, refB2, refB );
				const bool vkRan = runVk( 0, a2, b2, false );
				const Verdict w = vkRan ? judge( bg, a2, a, b2, b ) : Verdict();
				std::printf( "PRECIPVIEW %s repeat: reference vs its second build %zu halves differ, GL's stencil %zu, Vulkan vs Vulkan %s (%zu halves "
					"differ, %zu flips, stencil %zu); GL 4x vs GL 4x (info) %s (%zu halves differ, %zu flips)\n", j.name.c_str(), refRep, g.stencil,
					vkRan ? w.word() : "FAIL (run failed)", w.diff, w.flips, w.stencil, g.word(), g.diff, g.flips );
				ok = ok && refRep == 0 && g.stencil == 0 && vkRan && w.diff == 0 && w.stencil == 0;
				// reference only, never the bar: GL's own run-to-run noise, draws 2-5 each judged against draw 1
				std::size_t nFl[4], nPa[4], nDi[4], nFlMax = 0, nPaMax = 0;
				for ( int k = 0; k < 4; k++ ) {
					const Verdict n = judge( bg, refAn[k], refA, refBn[k], refB );
					nFl[k] = n.flips; nPa[k] = n.past1; nDi[k] = n.diff;
					nFlMax = std::max( nFlMax, n.flips ); nPaMax = std::max( nPaMax, n.past1 );
				}
				std::printf( "PRECIPVIEW %s GL noise (reference, not the bar; 5 draws, 2-5 vs 1): flips %zu %zu %zu %zu (max %zu = %.4f %% of covered), "
					"halves past 1 ulp %zu %zu %zu %zu (max %zu = %.4f %%), halves differ %zu %zu %zu %zu\n", j.name.c_str(), nFl[0], nFl[1], nFl[2],
					nFl[3], nFlMax, v.covered ? 100.0 * double( nFlMax ) / double( v.covered ) : 0.0, nPa[0], nPa[1], nPa[2], nPa[3], nPaMax,
					v.covered ? 100.0 * double( nPaMax ) / double( v.covered ) : 0.0, nDi[0], nDi[1], nDi[2], nDi[3] );
				// the localisation (reference, not the bar): GL's geometry stage captured twice (run to run the same?), then
				// per particle against the Vulkan records read back (a plain case: the look program is not captured)
				if ( !j.look ) {
					static GLuint tfBuf = 0, tfQ = 0;
					if ( !tfBuf ) {
						f.genBuffers( 1, &tfBuf );
						genQueries( 1, &tfQ );
					}
					const std::ptrdiff_t tfBytes = std::ptrdiff_t( j.n ) * 6 * kTfFloats * 4;
					auto capture = [&]( std::vector<float> & out, GLuint & prims, GLuint prog = 0, int terms = 0 ) {
						bindJob( prog ? prog : pTf );
						if ( terms )  // L8 term 1: volTerms other than the case's
							u1i( prog ? prog : pTf, "volTerms", terms );
						glEnable( 0x8C89 );  // GL_RASTERIZER_DISCARD
						f.bindBuffer( 0x8C8E, tfBuf );  // GL_TRANSFORM_FEEDBACK_BUFFER
						f.bufferData( 0x8C8E, tfBytes, nullptr, 0x88EA );  // GL_DYNAMIC_COPY
						f.bindBufferBase( 0x8C8E, 0, tfBuf );
						beginQuery( 0x8C88, tfQ );  // GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN
						tfBegin( GL_TRIANGLES );
						bindVertexArray( vao );
						glDrawArrays( GL_POINTS, j.first, j.n );
						tfEnd();
						endQuery( 0x8C88 );
						bindVertexArray( 0 );
						glDisable( 0x8C89 );
						prims = 0;
						getQueryObjectuiv( tfQ, 0x8866, &prims );  // GL_QUERY_RESULT
						out.assign( size_t( prims ) * 3 * kTfFloats, 0.0f );
						if ( !out.empty() )
							getBufferSubData( 0x8C8E, 0, std::ptrdiff_t( out.size() * 4 ), out.data() );
						f.bindBufferBase( 0x8C8E, 0, 0 );
					};
					std::vector<float> tf1, tf2;
					GLuint pr1 = 0, pr2 = 0;
					capture( tf1, pr1 );
					capture( tf2, pr2 );
					size_t tfDiff = 0, tfVar[8] = {};
					for ( size_t i = 0; i < std::min( tf1.size(), tf2.size() ); i++ )
						if ( std::memcmp( &tf1[i], &tf2[i], 4 ) != 0 ) {
							tfDiff++;
							const int at = int( i % size_t( kTfFloats ) );
							for ( int q = 0; q < 8; q++ )
								tfVar[q] += at >= kTfAt[q] && at < kTfAt[q + 1];
						}
					std::printf( "PRECIPVIEW %s GL geometry stage captured twice: %u / %u triangles, %zu floats differ (gl_Position %zu, gUv %zu, gQ %zu, "
						"gView %zu, gCol %zu, gBox %zu, gA %zu, gB %zu)%s\n", j.name.c_str(), pr1, pr2, tfDiff, tfVar[0], tfVar[1], tfVar[2], tfVar[3],
						tfVar[4], tfVar[5], tfVar[6], tfVar[7], glGetError() == GL_NO_ERROR ? "" : " (a GL error in the capture)" );
					if ( j.lit && !j.fog ) {  // unfogged, gA is the light: distinct values with unequal channels (header)
						std::vector<std::array<float, 3>> ga3;
						for ( size_t v0 = 0; v0 + size_t( kTfFloats ) <= tf1.size(); v0 += size_t( kTfFloats ) ) {
							const float * q = &tf1[v0 + size_t( kTfAt[6] )];
							if ( q[0] != q[1] || q[1] != q[2] )
								ga3.push_back( { q[0], q[1], q[2] } );
						}
						std::sort( ga3.begin(), ga3.end() );
						const size_t distinct = size_t( std::unique( ga3.begin(), ga3.end() ) - ga3.begin() );
						std::printf( "PRECIPVIEW %s lit gA: %zu distinct captured values with unequal channels (want >= 100)\n", j.name.c_str(), distinct );
						ok = ok && distinct >= 100;
					}
					if ( j.lit ) {  // the globals check (header): b0 filled non-zero moves GL's geometry stage only in the cluster case
						std::vector<float> g( 84 );
						for ( size_t k = 0; k < g.size(); k++ )
							g[k] = 0.37f + 0.11f * float( k % 13 ) - 0.05f * float( k % 7 );
						uboOver = g.data();  // (bindJob uploads b0 itself: the other bytes go through it)
						std::vector<float> tfG;
						GLuint prG = 0;
						capture( tfG, prG );
						uboOver = nullptr;
						f.bindBuffer( fUbo, ubo );
						f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), j.global.empty() ? zeros.data() : j.global.data(), fStatic );
						size_t gDiff = tfG.size() == tf1.size() ? 0 : std::max( tfG.size(), tf1.size() );
						for ( size_t i = 0; i < std::min( tfG.size(), tf1.size() ); i++ )
							gDiff += std::memcmp( &tfG[i], &tf1[i], 4 ) != 0;
						const bool blk = f.getUniformBlockIndex( pTf, "globalUniforms" ) != 0xFFFFFFFFu;
						std::printf( "PRECIPVIEW %s lit globals: b0 other vs the case's, %u / %u triangles, %zu floats differ (want %s; block %s in GL's program)\n",
							j.name.c_str(), prG, pr1, gDiff, j.readsB0() ? "> 0, the cluster / the ambient's keep reads b0" : "0", blk ? "active" : "not active" );
						ok = ok && ( j.readsB0() ? gDiff > 0 : gDiff == 0 ) && prG == pr1 && prG > 0;
						if ( !j.readsB0() ) {  // Vulkan's globals mirror (header): the app's b0 must not move a run without the lists
							std::vector<std::uint16_t> a4, b4;
							vkGlobalOver = g.data();
							const bool ran = runVk( 0, a4, b4, false );
							vkGlobalOver = nullptr;
							const char * word = ran ? judge( bg, a4, a, b4, b ).word() : "the run failed";
							std::printf( "PRECIPVIEW %s lit globals on Vulkan: b0 other vs none, against the judged run %s (want EXACT)\n", j.name.c_str(), word );
							ok = ok && ran && std::strcmp( word, "EXACT" ) == 0;
						}
					}
					if ( j.gi() ) {  // L8 term 1's checks (header): GI idle in the default program and the RT GI variant; WW_CELL_PRT refused
						std::string actDef, actRt;
						for ( const char * nm : { "cellGiOn", "cellGi", "cellGiSlots", "cellRooms", "cellGiAir", "cellGiAirOn", "cellDdgiOn", "cellDdgiAtlas" } ) {
							if ( f.getUniformLocation( pJob, nm ) >= 0 )
								actDef += std::string( actDef.empty() ? "" : ", " ) + nm;
							if ( f.getUniformLocation( pRt, nm ) >= 0 || f.getUniformLocation( pTfRt, nm ) >= 0 )
								actRt += std::string( actRt.empty() ? "" : ", " ) + nm;
						}
						std::vector<float> tfT1, tfRt, tfNew;
						GLuint prT1 = 0, prRt = 0, prNew = 0;
						capture( tfT1, prT1, 0, 1 );
						capture( tfRt, prRt, pTfRt );
						capture( tfNew, prNew, pTfNew );
						auto floatsDiff = [&]( const std::vector<float> & x, std::string & by, const std::vector<float> & ref ) {
							size_t d = x.size() == ref.size() ? 0 : std::max( x.size(), ref.size() ), var[8] = {}, ulp[8] = {};
							for ( size_t i = 0; i < std::min( x.size(), ref.size() ); i++ )
								if ( std::memcmp( &x[i], &ref[i], 4 ) != 0 ) {
									d++;
									std::int32_t ia, ib;
									std::memcpy( &ia, &x[i], 4 );
									std::memcpy( &ib, &ref[i], 4 );
									const size_t u = size_t( std::llabs( std::int64_t( ia ) - std::int64_t( ib ) ) );
									const int at = int( i % size_t( kTfFloats ) );
									for ( int q = 0; q < 8; q++ )
										if ( at >= kTfAt[q] && at < kTfAt[q + 1] ) {
											var[q]++;
											ulp[q] = std::max( ulp[q], u );
										}
								}
							const char * nm[8] = { "gl_Position", "gUv", "gQ", "gView", "gCol", "gBox", "gA", "gB" };
							for ( int q = 0; q < 8; q++ )
								if ( var[q] )
									by += " " + std::string( nm[q] ) + " " + std::to_string( var[q] ) + " (max " + std::to_string( ulp[q] ) + " ulp)";
							return d;
						};
						{  // (info) which loose uniforms differ between the gate's own program and a fresh one, both after the case's bindJob
							bindJob( pTfNew );
							bindJob( pTf );
							std::string stale;
							GLint n = 0;
							f.getProgramiv( pTf, fActiveUnis, &n );
							for ( GLint i = 0; i < n; i++ ) {
								char nm[256] = {};
								GLint sz = 0;
								GLenum ty = 0;
								f.getActiveUniform( pTf, GLuint( i ), 256, nullptr, &sz, &ty, nm );
								std::string base( nm );
								if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
									base.resize( base.size() - 3 );
								for ( GLint e = 0; e < std::max( sz, 1 ); e++ ) {
									const std::string el = sz > 1 ? base + "[" + std::to_string( e ) + "]" : std::string( nm );
									const GLint la = f.getUniformLocation( pTf, el.c_str() ), lb = f.getUniformLocation( pTfNew, el.c_str() );
									if ( la < 0 || lb < 0 )
										continue;
									float va[16] = {}, vb[16] = {};
									f.getUniformfv( pTf, la, va );
									f.getUniformfv( pTfNew, lb, vb );
									if ( std::memcmp( va, vb, sizeof( va ) ) != 0 )
										stale += " " + el + " (" + std::to_string( va[0] ) + " vs fresh " + std::to_string( vb[0] ) + ")";
								}
							}
							std::printf( "PRECIPVIEW %s GI checks (info): loose uniforms the gate's own program holds unlike a fresh one:%s\n", j.name.c_str(),
								stale.empty() ? " none" : stale.c_str() );
						}
						std::string byT1, byRt, byNew;
						const size_t dT1 = floatsDiff( tfT1, byT1, tf1 ), dRt = floatsDiff( tfRt, byRt, tfNew ), dNew = floatsDiff( tfNew, byNew, tf1 );
						std::printf( "PRECIPVIEW %s GI checks by field: volTerms 5 vs 1:%s; the RT GI variant vs a fresh default program:%s; (info) the fresh "
							"default vs the gate's own (uniforms earlier cases left on it): %zu floats,%s\n", j.name.c_str(), byT1.empty() ? " none" : byT1.c_str(),
							byRt.empty() ? " none" : byRt.c_str(), dNew, byNew.empty() ? " none" : byNew.c_str() );
						ok = ok && prNew == pr1;
						std::printf( "PRECIPVIEW %s GI checks: GI names active in GL's default program: %s; in the RT GI variant: %s (want none); captured "
							"volTerms 5 vs 1 %zu floats differ, the RT GI variant vs a fresh default program %zu (%u / %u / %u triangles; each want 0)\n", j.name.c_str(),
							actDef.empty() ? "none" : actDef.c_str(), actRt.empty() ? "none" : actRt.c_str(), dT1, dRt, pr1, prT1, prRt );
						ok = ok && actDef.empty() && actRt.empty() && dT1 == 0 && dRt == 0 && prT1 == pr1 && prRt == pr1 && pr1 > 0;
						std::vector<std::uint16_t> aP, bP, aR, bR;
						vkProgOver = pPrt;
						why.clear();
						const bool prtRan = runVk( 0, aP, bP, false );
						const std::string prtWhy = why;
						const bool redRan = runVk( 8192, aR, bR, false );
						vkProgOver = 0;
						const bool refused = !prtRan && prtWhy.find( "GI volume" ) != std::string::npos;
						const Verdict r = redRan && !refPrt.empty() ? judge( bg, aR, refPrt, bR, refB ) : Verdict();
						std::printf( "PRECIPVIEW %s GI program refusal: the WW_CELL_PRT variant's lit run %s%s (want refused, a GI volume named); red 8192 (the "
							"refusal's input dropped) against its frame: %s (%zu covered, %zu flips; want FAIL, >= 1000 flips)\n", j.name.c_str(),
							prtRan ? "RAN" : "refused: ", prtRan ? "" : prtWhy.c_str(), redRan ? r.word() : "the run failed", r.covered, r.flips );
						ok = ok && refused && redRan && !r.ok() && r.flips >= 1000;
					}
					std::vector<std::uint8_t> rec( size_t( j.n ) * 112 );
					std::vector<std::uint16_t> a3, b3;
					if ( !runVk( 0, a3, b3, false, rec.data() ) )
						std::printf( "PRECIPVIEW %s particles: the Vulkan run with the read-back failed\n", j.name.c_str() );
					else {
						std::printf( "PRECIPVIEW %s particles: the read-back run against the judged one %s\n", j.name.c_str(), judge( bg, a3, a, b3, b ).word() );
						bindJob( pJob );
						float M[16] = {}, atl[4] = {};
						f.getUniformfv( pJob, f.getUniformLocation( pJob, "pcProj" ), M );
						f.getUniformfv( pJob, f.getUniformLocation( pJob, "pcAtlas" ), atl );
						particleDiff( j.name, tf1, pr1, rec, j.n, M, atl, W, H, a, refPS );
					}
				}
			}
		}
		for ( RedSeen & r : reds ) {
			if ( ( r.red == 16 && !j.look ) || ( r.red == 32 && !j.hasTex ) || ( r.red == 128 && !j.lit ) || ( r.red == 256 && !j.lamps() )
				|| ( r.red == 512 && !j.readsB0() ) || ( r.red == 1024 && !j.cluster() ) || ( r.red == 2048 && !j.localSun() ) || ( r.red == 4096 && !j.terrAir() )
				|| ( r.red == 16384 && !j.envKeep() ) || ( r.red == 32768 && !j.amb() )
				|| ( ( r.red == 65536 || r.red == 131072 || r.red == 262144 || r.red == 524288 ) && !j.csm() )
				|| ( ( r.red == 1048576 || r.red == 2097152 || r.red == 4194304 ) && !j.drift() )
				|| ( ( r.red == 8388608 || r.red == 16777216 || r.red == 33554432 ) && !j.cube() )
				|| ( ( r.red == 67108864 || r.red == 134217728 || r.red == 268435456 ) && !j.gobo() )
				|| ( ( r.red == 536870912LL || r.red == 1073741824LL || r.red == 2147483648LL ) && !j.roof() )
				|| ( ( r.red == 4294967296LL || r.red == 8589934592LL || r.red == 17179869184LL ) && !j.interior() )
				|| ( r.red == 34359738368LL && !j.frameUbo() )
				|| ( ( r.red == 68719476736LL || r.red == 137438953472LL || r.red == 274877906944LL || r.red == 549755813888LL ) && !j.uss() )
				|| ( ( r.red == 1099511627776LL || r.red == 2199023255552LL || r.red == 4398046511104LL || r.red == 8796093022208LL ) && !j.restir() ) )
				continue;  // a plain run has no look to drop, a case with no atlas no atlas, an unlit one no light, no lamps no lights
			std::vector<std::uint16_t> a, b;
			if ( !runVk( r.red, a, b, false ) ) {
				std::printf( "PRECIPVIEW %s red %s: the run failed (counts as a FAIL)\n", j.name.c_str(), r.name );
				r.seen = true;
				continue;
			}
			const Verdict v = judge( bg, a, refPS, b, refPSB );
			std::printf( "PRECIPVIEW %s red %s: %s (%zu covered, %zu flips, %zu past 1 ulp, stencil differs on %zu)\n", j.name.c_str(), r.name,
				v.word(), v.covered, v.flips, v.past1, v.stencil );
			r.seen = r.seen || !v.ok();
			if ( r.red == 32 && v.ok() ) {  // every case that reads its atlas must see it dropped (the drop, pcLook 1, reads none)
				std::printf( "PRECIPVIEW %s red atlas %s\n", j.name.c_str(), j.look == 1 ? "PASSED: bound, not read (the drop's look reads no texture)"
					: "PASSED: the case does not see its atlas" );
				ok = ok && j.look == 1;
			}
			if ( r.red == 128 && v.ok() ) {  // every lit case must see its light dropped
				std::printf( "PRECIPVIEW %s red light PASSED: the case does not see its light\n", j.name.c_str() );
				ok = false;
			}
			if ( r.red == 256 && v.ok() ) {  // every lamp case must see its lights dropped
				std::printf( "PRECIPVIEW %s red lamps' lights PASSED: the case does not see its lights\n", j.name.c_str() );
				ok = false;
			}
			if ( ( r.red == 512 || r.red == 1024 || r.red == 2048 || r.red == 4096 || r.red == 16384 || r.red == 32768 || r.red == 65536
				|| r.red == 131072 || r.red == 262144 || r.red == 524288 || r.red == 1048576 || r.red == 2097152 || r.red == 4194304
				|| r.red == 8388608 || r.red == 16777216 || r.red == 33554432 || r.red == 67108864 || r.red == 134217728
				|| r.red == 268435456 || r.red == 536870912LL || r.red == 1073741824LL || r.red == 2147483648LL || r.red == 4294967296LL
				|| r.red == 8589934592LL || r.red == 17179869184LL || r.red == 34359738368LL
				|| r.red == 68719476736LL || r.red == 137438953472LL || r.red == 274877906944LL || r.red == 549755813888LL
				|| r.red == 1099511627776LL || r.red == 2199023255552LL || r.red == 4398046511104LL || r.red == 8796093022208LL ) && v.ok() ) {  // the cluster case must see its b0 and its lists dropped
				std::printf( "PRECIPVIEW %s red %s PASSED: the case does not see it\n", j.name.c_str(), r.name );
				ok = false;
			}
		}
		jobAt++;
	}
	for ( const RedSeen & r : reds )
		if ( !r.seen ) {
			std::printf( "PRECIPVIEW red %s PASSED in every case: the gate cannot see it\n", r.name );
			ok = false;
		}
	const WwVkPresent::FxStat st = WwVkPresent::takePrecipStat();
	std::printf( "PRECIPVIEW stat: %d runs, %d failed (%s), %d pipelines built in %.1f ms, %d input copies, drawing thread %.3f ms a run "
		"(max %.3f; includes the harness's hand-off wait, not a frame cost)\n", st.runs, st.fails, st.failWhy.c_str(), st.pipelines, st.buildMs,
		st.copies, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	{  // the pack-order check (header)
		f.useProgram( pPrecip );
		u1i( pPrecip, "cellHasDir", 1 );
		u1i( pPrecip, "cellInterior", 0 );
		u1i( pPrecip, "volTerms", 1 );
		u1i( pPrecip, "volPartRed", 1 );  // refused for good (header's fixture move)
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		auto probe = [&]( bool packFirst ) {
			WwVkPresent::PrecipRun t;
			t.w = W;
			t.h = H;
			glGetIntegerv( GL_VIEWPORT, t.vp );
			t.color = fc;
			t.first = jobs[0].first;
			t.n = jobs[0].n;
			t.lit = true;
			WwVkPresent::precipLitRead( &ga, pPrecip, t );
			bool drew = false;
			if ( packFirst || WwVkPresent::precipAdmits( t, why ) ) {
				WwVkPresent::fxPack( &ga, pPrecip, WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), t.cs );
				WwVkPresent::fxPack( &ga, pPrecip, WwVkPresent::kT_precipps, int( std::size( WwVkPresent::kT_precipps ) ), t.ps );
				WwVkPresent::fxPack( &ga, pPrecip, WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), t.cs );
				t.litBlock = true;
				t.packs = 3;
				drew = WwVkPresent::precipRun( t, why );
			}
			const WwVkPresent::FxStat s = WwVkPresent::takePrecipStat();
			std::printf( "PRECIPVIEW pack order (%s): %s, %d refused, %d packs on refused runs (%s)\n",
				packFirst ? "red: packed first" : "the app's: refusals first", drew ? "DREW" : "refused", s.fails, s.refusedPacks,
				s.failWhy.c_str() );
			return drew ? -1 : s.fails == 1 ? s.refusedPacks : -1;
		};
		const int asked = probe( false ), red = probe( true );
		if ( asked != 0 ) {
			std::printf( "PRECIPVIEW pack order FAILED: a refused run must pay 0 packs\n" );
			ok = false;
		}
		if ( red <= 0 ) {
			std::printf( "PRECIPVIEW red pack order PASSED: the count cannot see packs on a refused run\n" );
			ok = false;
		} else {
			std::printf( "PRECIPVIEW red pack order FAILS as it must (%d packs counted)\n", red );
		}
		u1i( pPrecip, "volPartRed", 0 );
	}
	{  // the census (header): the app's all-on lit defaults, every refusal that would fire (info; 0 is the L8 done line)
		const float drift[4] = { 0.5f, 0.0f, 1.0f, 0.5f }, sun[4] = { 1.0f, 0.9f, 0.8f, 1.0f }, zero4[4] = {};
		struct { const char * name; int interior; } where[2] = { { "exterior", 0 }, { "interior", 1 } };
		for ( const auto & w : where ) {
			f.useProgram( pPrecip );
			u1i( pPrecip, "cellHasDir", 1 - w.interior );          // celllights.cpp: the sun outdoors
			u1i( pPrecip, "cellInterior", w.interior );
			u1i( pPrecip, "volTerms", 15 );                         // cellvolair.cpp 4234: all four terms
			u1i( pPrecip, "volCsm", 1 );                            // cellvolair.cpp 4233: the cascades drawn
			u1i( pPrecip, "airTerrOn", 1 - w.interior );            // cellvolair.cpp 4219: the terrain air outdoors
			f.uniform4fv( f.getUniformLocation( pPrecip, "volAirDrift" ), 1, drift );  // cellvolair.cpp 3575: drift on
			f.uniform4fv( f.getUniformLocation( pPrecip, "vpSun" ), 1, w.interior ? zero4 : sun );  // celllights.cpp 2100
			u1i( pPrecip, "cellShadowOn", 1 );                      // celllights.cpp 1849
			u1i( pPrecip, "cellGoboOn", 1 );                        // celllights.cpp 1904
			u1i( pPrecip, "cellClusterOn", 1 );                     // cellclustergl.cpp 370
			u1i( pPrecip, "ussOn", 0 );                             // shadowset.cpp 2757: the tiers 0, the cascades valid
			u1i( pPrecip, "ussCloudOn", 1 );                        // cloudshadow.cpp 108: the cloud shadow follows the clouds
			WwVkPresent::PrecipRun t;
			t.lit = true;
			WwVkPresent::precipLitRead( &ga, pPrecip, t );
			const char * whys[24] = {};
			int n = WwVkPresent::precipLitWhys( t.litIn, whys, 24 );
			std::printf( "PRECIPVIEW census %s (the app's all-on lit defaults): %d refusals (0 is the L8 done line)\n", w.name, n );
			for ( int k = 0; k < n && k < 24; k++ )
				std::printf( "PRECIPVIEW census %s refusal %d: %s\n", w.name, k + 1, whys[k] );
		}
		u1i( pPrecip, "cellInterior", 0 );
		u1i( pPrecip, "volTerms", 1 );
		u1i( pPrecip, "volCsm", 0 );
		u1i( pPrecip, "airTerrOn", 0 );
		f.uniform4fv( f.getUniformLocation( pPrecip, "volAirDrift" ), 1, zero4 );
		f.uniform4fv( f.getUniformLocation( pPrecip, "vpSun" ), 1, zero4 );
		u1i( pPrecip, "cellShadowOn", 0 );
		u1i( pPrecip, "cellGoboOn", 0 );
		u1i( pPrecip, "cellClusterOn", 0 );
		u1i( pPrecip, "ussCloudOn", 0 );
	}
	{  // the route census (header): the loader's programs, the census' switches (+ all-on's own), the refusals + the closed list
		const float drift[4] = { 0.5f, 0.0f, 1.0f, 0.5f }, sun[4] = { 1.0f, 0.9f, 0.8f, 1.0f }, zero4[4] = {};
		const float phys[4] = { 1.0f, 0.0f, 0.0f, 0.0f };  // lightphys.h Physical (WW_CELL_LIGHT_PHYS 1)
		std::map<std::string, int> known = { { "globalUniforms", 1 } };
		auto addT = [&]( const WwVkPresent::FxField * t, int n ) {
			for ( int i = 0; i < n; i++ )
				known[t[i].name] = 1;
		};
		addT( WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ) );
		addT( WwVkPresent::kT_precipps, int( std::size( WwVkPresent::kT_precipps ) ) );
		addT( WwVkPresent::kT_preciplookcs, int( std::size( WwVkPresent::kT_preciplookcs ) ) );
		addT( WwVkPresent::kT_preciplookps, int( std::size( WwVkPresent::kT_preciplookps ) ) );
		addT( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ) );
		addT( WwVkPresent::kT_preciplitrtcs, int( std::size( WwVkPresent::kT_preciplitrtcs ) ) );  // L8 route lift 3
		for ( const char * nm : { "pcTex", "pcTexN", "pcRoof", "atApTex", "volTex", "volTile", "volAirTerr", "volAirRainMap", "airTerr",
				  "volEnvHist", "csmMap", "ussCloudMap", "cellShadow", "volAirNoise", "cellLights", "cellCluster", "ussTerrMap", "ussContactMap", "cellRestir",  // the sampler slots
				  "volTerms", "cellInterior", "volPartRed", "volCsm", "airTerrOn", "volAirDrift", "vpSun", "cellShadowOn", "cellGoboOn",
				  "cellGoboRot", "cellGoboFlat", "cellGoboGam", "cellGoboMap", "cellClusterOn", "cellRed", "cellLightPhys", "volEnvGrid",
				  "ussOn", "ussCloudOn", "csmProbe", "csmRed", "cellRestirOn",  // precipLitRead's switches
				  "cellGiOn", "cellGi", "cellGiSlots", "cellRooms", "cellGiAir", "cellGiAirOn", "cellDdgiOn", "cellDdgiAtlas" } )  // its GI names
			known[nm] = 1;
		// the closed list: every active loose uniform and every active block of prog that nothing above reads (one line each)
		auto closed = [&]( GLuint prog, const char * label, bool say ) {
			int n = 0;
			GLint nu = 0;
			f.getProgramiv( prog, fActiveUnis, &nu );
			const GLuint gu = f.getUniformBlockIndex( prog, "globalUniforms" ), sf = f.getUniformBlockIndex( prog, "wwShadowFrame" );
			std::map<GLint, int> blocks;
			for ( GLint i = 0; i < nu; i++ ) {
				char nm[256] = {};
				GLint sz = 0;
				GLenum ty = 0;
				f.getActiveUniform( prog, GLuint( i ), 256, nullptr, &sz, &ty, nm );
				const GLuint idx = GLuint( i );
				GLint blk = -1;
				f.getActiveUniformsiv( prog, 1, &idx, fBlockIndex, &blk );
				std::string s( nm );
				if ( s.find( '[' ) != std::string::npos )
					s.resize( s.find( '[' ) );
				if ( blk >= 0 ) {
					if ( GLuint( blk ) == gu || GLuint( blk ) == sf || blocks[blk]++ )  // (L8 route lift 1: wwShadowFrame read by precipLitRead)
						continue;
					n++;
					if ( say )
						std::printf( "PRECIPVIEW census %s GL input the twin does not read: the block %s (member %s)\n", label,
							GLuint( blk ) == sf ? "wwShadowFrame" : "(unnamed)", s.c_str() );
					continue;
				}
				if ( known.count( s ) )
					continue;
				n++;
				if ( say )
					std::printf( "PRECIPVIEW census %s GL input the twin does not read: %s\n", label, s.c_str() );
			}
			return n;
		};
		// one program, one place: the census' switches (allOn: + its own values); its refusals (+ the closed list)
		auto census = [&]( GLuint prog, int interior, bool allOn, const char * label, bool say, bool withClosed ) {
			f.useProgram( prog );
			u1i( prog, "cellHasDir", 1 - interior );
			u1i( prog, "cellInterior", interior );
			u1i( prog, "volTerms", 15 );
			u1i( prog, "volCsm", 1 );
			u1i( prog, "airTerrOn", 1 - interior );
			f.uniform4fv( f.getUniformLocation( prog, "volAirDrift" ), 1, drift );
			f.uniform4fv( f.getUniformLocation( prog, "vpSun" ), 1, interior ? zero4 : sun );
			u1i( prog, "cellShadowOn", 1 );
			u1i( prog, "cellGoboOn", 1 );
			u1i( prog, "cellClusterOn", 1 );
			u1i( prog, "ussOn", allOn ? 1 : 0 );  // all-on: the tiers ultra (shadowset.cpp 2754)
			u1i( prog, "ussCloudOn", 1 );
			if ( allOn ) {
				f.uniform4fv( f.getUniformLocation( prog, "cellLightPhys" ), 1, phys );
				u1i( prog, "cellRestirOn", 1 );  // rtddgicell.cpp restirUniforms: the ratio ready
			}
			WwVkPresent::PrecipRun t;
			t.lit = true;
			WwVkPresent::precipLitRead( &ga, prog, t );
			const char * whys[24] = {};
			const int n = WwVkPresent::precipLitWhys( t.litIn, whys, 24 );
			const int c = withClosed ? closed( prog, label, false ) : 0;
			if ( say ) {
				std::printf( "PRECIPVIEW census %s (the loader's program): %d refusals + %d GL inputs not read = %d (0 is the done line)\n",
					label, n, c, n + c );
				for ( int k = 0; k < n && k < 24; k++ )
					std::printf( "PRECIPVIEW census %s refusal %d: %s\n", label, k + 1, whys[k] );
				if ( c )
					closed( prog, label, true );
			}
			return n + c;
		};
		const int pClosed = closed( pPrecip, "pPrecip (the judged program)", true );
		std::printf( "PRECIPVIEW census closed list on pPrecip: %d (must read 0)\n", pClosed );
		if ( pClosed != 0 ) {
			std::printf( "PRECIPVIEW census closed list FAILED: pPrecip reads a GL input the allow list does not hold\n" );
			ok = false;
		}
		int route[2] = {}, allOnN[2] = {};
		const char * whereName[2] = { "exterior", "interior" };
		for ( int w = 0; w < 2; w++ ) {
			char lb[96];
			std::snprintf( lb, sizeof( lb ), "route default %s", whereName[w] );
			route[w] = census( pRoute, w, false, lb, true, true );
			std::snprintf( lb, sizeof( lb ), "route all-on %s", whereName[w] );
			allOnN[w] = census( pAllOn, w, true, lb, true, true );
			std::snprintf( lb, sizeof( lb ), "route all-on + WW_CELL_RESTIRGI %s (info, a ruling)", whereName[w] );
			census( pRgi, w, true, lb, true, true );
		}
		std::printf( "PRECIPVIEW census routes: default %d / %d, all-on %d / %d (exterior / interior; 0 / 0 on both is the done line)\n",
			route[0], route[1], allOnN[0], allOnN[1] );
		if ( route[0] + route[1] == 0 ) {  // L8 route lift 2 (header): nothing left for the old reading to under-read; the red is the
			// closed list itself: one sampler slot (ussTerrMap) dropped from the allow list must be seen on pRoute
			known.erase( "ussTerrMap" );
			const int dropped = closed( pRoute, "red allow list", false );
			known["ussTerrMap"] = 1;
			if ( dropped == 0 ) {
				std::printf( "PRECIPVIEW red allow list PASSED: ussTerrMap dropped from the allow list, pRoute's closed list reads 0\n" );
				ok = false;
			} else {
				std::printf( "PRECIPVIEW red allow list FAILS as it must (ussTerrMap dropped: pRoute's closed list reads %d)\n", dropped );
			}
		} else {
			const int old = census( pPrecip, 0, false, "old", false, false ) + census( pPrecip, 1, false, "old", false, false );
			if ( old >= route[0] + route[1] ) {
				std::printf( "PRECIPVIEW red old census PASSED: the old reading (%d) is not below route default's (%d)\n", old, route[0] + route[1] );
				ok = false;
			} else {
				std::printf( "PRECIPVIEW red old census FAILS as it must (the old reading %d, route default %d)\n", old, route[0] + route[1] );
			}
		}
		for ( const GLuint prog : { pPrecip, pRoute, pAllOn, pRgi } ) {  // pPrecip back to the census' reset
			f.useProgram( prog );
			for ( const char * nm : { "cellInterior", "volCsm", "airTerrOn", "cellShadowOn", "cellGoboOn", "cellClusterOn", "ussOn", "ussCloudOn", "cellRestirOn" } )
				u1i( prog, nm, 0 );
			u1i( prog, "volTerms", 1 );
			for ( const char * nm : { "volAirDrift", "vpSun", "cellLightPhys" } )
				f.uniform4fv( f.getUniformLocation( prog, nm ), 1, zero4 );
		}
	}
	for ( char & k : twinKeys )  // a BC atlas is a farTwin, an uncompressed one a shared target
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	for ( char & k : normKeys )
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	for ( int c = 0; c < kCases; c++ ) {
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &terrKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &envKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &csmKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &noiseKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &cubeKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &roofKeys[c] ) );
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &cloudKeys[c] ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &cloudKeys[c] ) );
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &ussTerrKeys[c] ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &ussTerrKeys[c] ) );
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &ussContactKeys[c] ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &ussContactKeys[c] ) );
		for ( char & k : airKeys[c] )
			WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	}
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "PRECIPVIEW GATE OK" : "PRECIPVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
