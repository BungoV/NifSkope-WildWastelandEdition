/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLVOLFOG_H
#define CELLVOLFOG_H

#include <QString>

class Scene;

/*! THE LIT MEDIUM (lane VOLFOG1, docs/cloud/VOLFOG1_DESIGN.md, docs/PRTP_PLAN.md "volumetric fog"): the light the
 *  fog scatters toward the eye, ADDED on top of the game's own distance fog (the game draws its fog and then its god
 *  rays over everything, DrawWorld::Render_PreUI 0x2857470).
 *  The density is the game's fog records' (his call): along a view ray the medium's opacity IS the fog alpha
 *  alpha(t) = wwFogEval(t, z(t)) (the weather's or, indoors, the cell's), so a slice's in-scatter is
 *  S x (alpha(end) - alpha(start)) -- the cloud design's exact slice integral with sigma_t read from the records.
 *  The light S (display units, pi x radiance): the directional light (outdoors the weather's sun -- at night the same
 *  light, the game has no moon light -- times its cascade shadow; indoors the lighting template's), the placed lights
 *  that emit shafts in the game (a shadow flag AND a GDRY: GenDynamic 0x320a50), and the probe GI read as L0 + L1.
 *  The phase is the god-ray record's medium (GDRY: Sky::GetGodraysSettings 0x64cb30): Rayleigh air, forward and back
 *  Henyey-Greenstein lobes, the three weighted by their scattering colours normalised to a luminance of 1; the GDRY
 *  intensity scales the result. Outdoors the weather's WGDR (two ToD slots), indoors CELL XGDR, else the lighting
 *  template's WGDR, else the engine's fallback medium.
 *  Here: wwVolFogPass renders res/shaders/cell_volfog.frag into a froxel volume (columns of 12 x 12 pixels, 64
 *  exponential slices from the fog's start to its full distance): stage 1 injects each slice (4 sub-steps), stage 2
 *  sums the slices front to back. lookdev_fog.glsl's wwFog adds the volume at a surface's distance.
 *  Haze cards (effect shaders) are left as they are: they fog themselves (EFX2) and never read the volume.
 *  DEVIATIONS: no temporal history (NifSkope redraws on demand); the game's own god-ray medium is a fixed density
 *  (1e-4 x the record's scattering per unit), here the fog records' (his call); the volume is added under an
 *  over-blended card rather than over it (exact for the additive beams).
 *  The row "Volumetric Fog" (Scene window, under Fog; ships OFF; QSettings WW/VolFog); the pin WW_VOLFOG=0|1 wins.
 *  Pins: WW_VOLFOG_RED=off (computed, not applied) | gioff | flat | noshadow | wrongsrc | sixreads (comma list;
 *  sixreads, lane VOLFOG1b: the GI cube read the VOLFOG1 way, six surface reads a froxel -- the cost's before),
 *  WW_VOLFOG_TERMS=<mask> (1 directional, 2 placed lights, 4 GI; the gates' stable subsets),
 *  WW_VOLFOG_PROBE=1 (the volume term alone, raw), WW_VOLFOG_DUMP=<file> (the volumes and the numbers). */
void wwVolFogPass( Scene * scene, bool run );
//! the uniforms of the renderer's CURRENT program (a no-op without `volOn`); the sampler is bound always
void wwVolFogUniforms( Scene * scene );
//! the row (ships off); the pin WW_VOLFOG wins
bool wwVolFogOn();
void wwVolFogSetOn( bool on );
//! census echo: "volfog=on cols x rows x slices near far medium ... ms" or why none was drawn
QString wwVolFogSummary();

#endif
