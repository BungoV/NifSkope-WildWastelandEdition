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
 *    sigma0: the air hides as much at the fog's full distance dFull as the game's fog does there:
 *            1 - exp(-sigma0 x dFull) = min(max, 0.99)
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
 *  from 16 units to dFull.
 *    inject     per froxel: 4 equal sub-steps; each adds T x J x (1 - exp(-tau)) and multiplies T by exp(-tau) (exact
 *               for J constant over the sub-step, any density). The sun is evaluated per sub-step, the rest once a
 *               froxel. rgb = the slice's light from its near edge, a = the slice's transmittance.
 *    integrate  front to back: L_k = L_(k-1) + T_(k-1) x inj_k.rgb, T_k = T_(k-1) x inj_k.a.
 *    apply      a surface at distance d: colour x T(d) + L(d) (lookdev_fog.glsl wwFog), linear in log-depth.
 *  DEVIATION: no temporal history (NifSkope redraws on demand); the 4 stratified sub-steps stand in for it.
 *
 *  The fold "Volumetric Air" (Scene window): the row ships OFF (QSettings WW/VolAir); the pin WW_CELL_AIR=0|1 wins.
 *  Gates only: WW_CELL_AIR_RED=off (computed, not applied) | gioff | flat | noshadow | nolights | wrongmed | blendnear (with FOGBLEND=1) (comma
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

#endif
