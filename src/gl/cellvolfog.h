/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLVOLFOG_H
#define CELLVOLFOG_H

#include <QString>

class Scene;

/*! Volumetric fog: light the fog scatters toward the eye, added on top of the game's distance fog. The opacity is the
 *  fog records' alpha along the ray; the light is the sun / directional light (shadowed), shaft-casting placed
 *  lights and probe GI; the phase is the GDRY medium. res/shaders/cell_volfog.frag fills a froxel volume that wwFog
 *  adds at each surface. Row "Volumetric Fog" ships OFF; WW_VOLFOG=0|1 pins it; WW_VOLFOG_RED, _TERMS=<mask>,
 *  _PROBE=1 and _DUMP=<file> are debug aids. */
void wwVolFogPass( Scene * scene, bool run );
//! the uniforms of the renderer's CURRENT program (a no-op without `volOn`); the sampler is bound always
void wwVolFogUniforms( Scene * scene );
//! the row (ships off); the pin WW_VOLFOG wins
bool wwVolFogOn();
void wwVolFogSetOn( bool on );
//! census echo: "volfog=on cols x rows x slices near far medium ... ms" or why none was drawn
QString wwVolFogSummary();

#endif
