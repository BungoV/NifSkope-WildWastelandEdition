/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLVOLAIR_H
#define CELLVOLAIR_H

#include <QString>

class Scene;

/*! VOLUMETRIC AIR (lane VOLAIR1, absorbs lane VOLFOG1's lit fog): ONE participating medium, "air", that both hides
 *  the scene (extinction) and glows with the light that is really there (in-scattering). While it is on, the game's
 *  distance fog is not drawn: the air IS the fog.
 *
 *  The medium: sigma_t(z) = sigma0 x rho(z), rho(z) = min(1, exp(-(z - zb) / H)), single-scatter albedo 1. Fitted to
 *  the fog records in force (the weather's outdoors, the cell's indoors), no hand dial:
 *    sigma0: least squares in optical depth against the game's fog over the street range (64 log-spaced
 *            distances, 500 to 5000 units, below the band): sigma0 = sum(d tau) / sum(d^2), tau = -ln(1 - alpha);
 *            never thinner than the far-end fit 1 - exp(-sigma0 x dFull) = min(max, 0.99), except where the
 *            record's curve ends inside the street range (its knee, ramp 0.75, under 5000 units: an interior's
 *            short steep curve), where the street fit alone holds (the red farfit keeps the far-end fit alone: the
 *            street-range air ~1/10 of the game's, halos and shafts too faint to see)
 *    height: the game's fog weight runs from 1 below its height band (the far pair, z0..z1) to K3.w above it; the
 *            air falls off over the same band: zb = z0, H = (z1 - z0) / -ln(K3.w); no band: uniform (H infinite).
 *  The optical depth of any straight segment is closed form (flat below zb, exponential above).
 *
 *  The light J the air scatters toward the eye (display units, pi x radiance, as the surfaces):
 *    1 the directional light (outdoors the weather's sun x its cascade shadow x the air's transmittance up to the
 *      scene's top; indoors the lighting template's) x the phase
 *    2 every placed light (colour x radial curve x cone x its shadow cube x the air's transmittance to it) x phase
 *    4 the probe GI (glowing effects -- neon -- are in it: they emit in the bake) read as L0 + L1 of the six-axis cube
 *    8 the ambient (sky outdoors, the cell's indoors) read as L0 + L1, on the share the GI grid does not cover
 *  The phase: the god-ray record in force (air Rayleigh + forward and back Henyey-Greenstein lobes, their colours
 *  normalised to a luminance of 1).
 *  The colour of the sky's share (1, 4, 8): the records' (owner's ruling 10-14) -- the game's own fog colour at the
 *  froxel's distance and height (near / far / high colours by hour) mixed toward the fog-sun colour as the game mixes
 *  it, at unit luminance; the physics gives its brightness and direction. The placed lights keep their own colours.
 *
 *  The passes (portable: same terms in FO4CS): a froxel volume, columns of 12 x 12 pixels x 64 slices exponential
 *  from 16 units to farD, where the air reaches min(max, 0.99) (tau_max / sigma0, at most dFull).
 *    inject     per froxel: 4 equal sub-steps; each adds T x J x (1 - exp(-tau)) and multiplies T by exp(-tau) (exact
 *               for J constant over the sub-step, any density). The sun is evaluated per sub-step, the rest once a
 *               froxel. rgb = the slice's light from its near edge, a = the slice's transmittance.
 *    integrate  front to back: L_k = L_(k-1) + T_(k-1) x inj_k.rgb, T_k = T_(k-1) x inj_k.a.
 *    apply      a surface at distance d: colour x T(d) + L(d) (lookdev_fog.glsl wwFog), linear in log-depth.
 *    far tail   past farD (mountains, the far LOD): closed form, no new number. The light per unit optical depth J
 *               of the last slices, held; T_tail = exp(-sigma0 x col(z0, z1) / |dz|) along the rest of the ray (flat:
 *               sigma0 x length), L += T(farD) x J x (1 - T_tail), J recoloured by the record colour's ratio there.
 *               Continuous at farD; a mountain top (thinner air above the band) hazes less than its foot.
 *  The surfaces' sun (FOGSUN1): the directional light on every lit surface outdoors x the air's transmittance up the
 *  sun ray, exp(-sigma0 x col(z, top) / max(mu, 0.02)), mu = the sun's height (lookdev_fog.glsl wwVolAirSunT).
 *  DEVIATION: no temporal history (NifSkope redraws on demand); the 4 stratified sub-steps stand in for it.
 *  Cost (coordinator 10-14), both on by default, the picture unchanged: clustered lights (per 8 x 8 columns x 8
 *  slices the lights whose sphere can reach one of its froxel points, ascending: the inject walks that list, not every
 *  light) and the conservative depth skip (stage 0: per column the farthest opaque ray distance from lane AO1's
 *  g-buffer; a froxel whose previous slice starts past the farthest of the column and its neighbours is never read
 *  by the apply and holds (0, 1)); no opaque pass that frame: no skip.
 *
 *  The fold "Volumetric Air" (Scene window): the row ships OFF (QSettings WW/VolAir); the pin WW_CELL_AIR=0|1 wins.
 *  Gates only: WW_CELL_AIR_RED=off (computed, not applied) | gioff | flat | noshadow | nolights | wrongmed | blendnear
 *  (with FOGBLEND=1) | farfit | physcol | nosunt (surfaces keep the full sun) | froxonly (no far tail) | nosky (the sky without the air) | fxfog (effects and particles keep the game's fog alone) | lightmid (placed lights taken once at the slice middle) | bluescatter (the records' coloured scatter weights: the air adds a blue hue) (comma
 *  list; also oldsun (the air lit by the fog-sun glare, no sky DALC), dropcluster (every cluster list loses its
 *  nearest light), nearskip (the skip behind the column's nearest surface)), WW_CELL_AIR_OPT=0..3 (1 clusters, 2 the
 *  depth skip; 0 the unoptimised path), WW_CELL_AIR_TERMS=<mask> (1 directional, 2 placed lights, 4 GI, 8 ambient), WW_CELL_AIR_PROBE=1 (the light
 *  term L alone, raw) | 2 (the transmittance T, grey), WW_CELL_AIR_DUMP=<file> (the volumes and the numbers),
 *  WW_CELL_AIR_TIME=1 (the pass's ms in the census).
 *  Test only, until bungo picks: WW_CELL_VOLAIR_FOGBLEND=1 (option B) draws the game's fog over the lit air with its
 *  own alpha, so it adds only past the fog's start distance; unset or 0 = option A (the real light only). */
void wwVolAirPass( Scene * scene, bool run );
//! the uniforms of the renderer's CURRENT program (a no-op without `volOn`); the sampler is bound always
void wwVolAirUniforms( Scene * scene );
/*! the sky through the air (bungo 10-14: the far tower stood out against a sky the air never touched). The sky is a
 *  surface at infinite distance: the froxels, then the far tail run to the end of the air (lookdev_fog.glsl
 *  wwVolAirSky). probes false: before the scene draw, into the linear frame (cell_volsky.prog under everything; the
 *  resolve lays the fog colour at infinity over the backdrop by 1 - T). probes true: after the scene draw, in a probe
 *  frame (WW_CELL_AIR_PROBE 1 / 2, WW_LOOKDEV_FOGPROBE 9 / 10) the raw probe on the far-plane pixels (the far gates' sky
 *  marker (1, 1, 1)). Gates only: WW_CELL_AIR_RED=nosky (the sky without the air, as before). */
void wwVolAirSkyDraw( Scene * scene, bool probes );
//! the row (ships off); the pin WW_CELL_AIR wins
bool wwVolAirOn();
void wwVolAirSetOn( bool on );
/*! lane FARGRID1: THE FAR GRID (the row "Far Air" under Volumetric Air: Off / Low / Medium / High / Ultra, ships Off,
 *  QSettings WW/VolAirFar; the pin WW_CELL_AIR_FAR=0..4 wins). Outdoors, the near grid then ends at 250 m (17,578 units)
 *  and a second, fixed grid (not tied to the screen) carries the SAME medium, phase, sun and sky from there to
 *  min(the preset end, the medium's farD); the closed-form tail as before past it. Its sun is shadowed by the cascades
 *  where they reach and the terrain's shadow-height map (TERRSHADOW1; the Terrain Shadows row), times the clouds' input
 *  (cell_volair.frag airCloudShadow, 1 until ATMORES1). No placed lights, no GI grid. Both grids share one volume (the
 *  far grid's layers after the near grid's), so the surfaces gain no sampler; Off draws exactly as before.
 *  Presets (cols x rows x slices, end, rebuild of an unchanged view every N frames): Low 64x36x16 4 km 32, Medium
 *  96x54x24 8 km 16, High 160x88x32 12 km 8, Ultra 240x132x48 16 km 4. Every input a weather owns comes from the
 *  records the near grid reads (fog distances and powers, sun and sky colours by hour, the god-ray phase).
 *  Reds (WW_CELL_AIR_RED): farnoterr (the far grid ignores the terrain), farseam (it starts 30 % past the near grid's
 *  end), farjitter (a per-frame slice offset). Echo: far=on(tier ... gpums ... volmb ...) in the air's census. */
int wwVolAirFarTier();
void wwVolAirSetFarTier( int tier );
/*! lane FOGDRIFT1: DRIFTING FOG (bungo 10-22, after RDR2). The near grid's density x a tileable 64^3 noise (two
 *  octaves, tiles 4096 and 1843 units) that the weather's wind (WTHR DATA speed, direction; range and turbulence wobble
 *  it) carries along; no wind = still lumps. The factor keeps the mean density (1 + a (n - mean) / span) and fades to 1
 *  from 2000 to 8000 units; the far grid and the tail stay smooth. Every in-scatter term (sun, sky, GI, the placed
 *  lights) scatters by the same noisy sub-step, so a lamp's glow brightens and dims as the lumps pass. Exteriors drift;
 *  an interior only when it shows the sky; a sealed one is drawn exactly as with the row off.
 *  The row "Fog Drift" under Volumetric Air ships OFF (QSettings WW/VolAirDrift; the pin WW_CELL_FOGDRIFT=0|1 wins).
 *  WW_CELL_FOGDRIFT_RED=still (no motion) | smooth (no noise); WW_CELL_FOGDRIFT_T=<s> pins the clock (shots' frames;
 *  a harness pins t = 0). Echo: drift=on(wind dir range turb speed t) in the air's census; the dump's "drift" line. */
bool wwVolAirDriftOn();
void wwVolAirSetDriftOn( bool on );
//! the view repaints while the fog drifts (never in a harness or with the clock pinned)
bool wwVolAirWantsRepaint();
//! census echo: "volair=on(...)" or why none was drawn
QString wwVolAirSummary();

#endif
