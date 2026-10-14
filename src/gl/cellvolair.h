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
 *            never thinner than the far-end fit 1 - exp(-sigma0 x dFull) = min(max, 0.99) (the red farfit keeps
 *            that fit alone: the street-range air ~1/10 of the game's, halos and shafts too faint to see)
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
 *
 *  The passes (portable: same terms in FO4CS): a froxel volume, columns of 12 x 12 pixels x 64 slices exponential
 *  from 16 units to farD, where the air reaches min(max, 0.99) (tau_max / sigma0, at most dFull).
 *    inject     per froxel: 4 equal sub-steps; each adds T x J x (1 - exp(-tau)) and multiplies T by exp(-tau) (exact
 *               for J constant over the sub-step, any density). The sun is evaluated per sub-step, the rest once a
 *               froxel. rgb = the slice's light from its near edge, a = the slice's transmittance.
 *    integrate  front to back: L_k = L_(k-1) + T_(k-1) x inj_k.rgb, T_k = T_(k-1) x inj_k.a.
 *    apply      a surface at distance d: colour x T(d) + L(d) (lookdev_fog.glsl wwFog), linear in log-depth.
 *  DEVIATION: no temporal history (NifSkope redraws on demand); the 4 stratified sub-steps stand in for it.
 *
 *  The fold "Volumetric Air" (Scene window): the row ships OFF (QSettings WW/VolAir); the pin WW_CELL_AIR=0|1 wins.
 *  Gates only: WW_CELL_AIR_RED=off (computed, not applied) | gioff | flat | noshadow | nolights | wrongmed | blendnear (with FOGBLEND=1) | farfit (comma
 *  list), WW_CELL_AIR_TERMS=<mask> (1 directional, 2 placed lights, 4 GI, 8 ambient), WW_CELL_AIR_PROBE=1 (the light
 *  term L alone, raw) | 2 (the transmittance T, grey), WW_CELL_AIR_DUMP=<file> (the volumes and the numbers),
 *  WW_CELL_AIR_TIME=1 (the pass's ms in the census).
 *  Test only, until bungo picks: WW_CELL_VOLAIR_FOGBLEND=1 (option B) draws the game's fog over the lit air with its
 *  own alpha, so it adds only past the fog's start distance; unset or 0 = option A (the real light only). */
void wwVolAirPass( Scene * scene, bool run );
//! the uniforms of the renderer's CURRENT program (a no-op without `volOn`); the sampler is bound always
void wwVolAirUniforms( Scene * scene );
//! the row (ships off); the pin WW_CELL_AIR wins
bool wwVolAirOn();
void wwVolAirSetOn( bool on );
//! census echo: "volair=on(...)" or why none was drawn
QString wwVolAirSummary();

/*! VOLUMETRIC PARTICLES (lane VOLPART1): a particle system the cell lights (steam, smoke, fog puffs) is a little
 *  piece of the air. In particles_cell.prog (res/shaders/particles.frag, the law written out there):
 *    lit    DIRECTLY per pixel at the sprite's own point from the very list the air reads (the directional light x its
 *           cascade shadow, every placed light x its cone x its shadow cube x the air's transmittance to it, the GI
 *           and the ambient as six-axis cubes): thin = the air's phase (per unit opacity the air's own J), thick = a
 *           wrapped diffuse at the puff's normal (its normal map turned by the sprite's rotation, else a sphere);
 *           alpha as thickness: single-scatter share 1 - a, wrap 1 - a. Never lit from the integrated volume.
 *    seen   through the INTEGRATED froxel volume between it and the eye (colour x T + L, as every surface), so steam
 *           and air fade alike (the game's fog as before while the air is off)
 *    soft   a ball round the sprite's plane against the opaque pass's depth: no hard line where it meets a surface
 *  The row "Volumetric Particles" (Scene window) ships OFF (QSettings WW/VolPart); the pin WW_CELL_VOLPART=0|1 wins.
 *  Off, the particles draw exactly as before. Gates only: WW_CELL_VOLPART_RED=flat | nothick | nosoft | nolights |
 *  white | nofog | noshadow (comma list), WW_CELL_VOLPART_DUMP=<file> (each lit system's per-particle probe, appended:
 *  glparticles.cpp), WW_CELL_VOLPART_HIDE=1 (no particle drawn: the gates' plate). */
bool wwVolPartOn();
void wwVolPartSetOn( bool on );
int wwVolPartRed();
//! the uniforms of the renderer's CURRENT program (particles_cell.prog); true while on
bool wwVolPartUniforms( Scene * scene );
QString wwVolPartSummary();
//! gates only: counts the air's pass calls (one a frame); the particle probe keeps the last frame's alone
int wwVolAirFrameSerial();

#endif
