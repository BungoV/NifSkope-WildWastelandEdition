#ifndef CELLFXLIT_H
#define CELLFXLIT_H

#include <QString>
#include <QVector>

#include <cstddef>

class Scene;
struct WwCellLight;

/*! Lane FXLIT1 (2026-10-02, docs/PRTP_PLAN.md "lit effects"): THE PLACED LIGHTS ON A LIT EFFECT.
 *
 *  An effect shape whose material sets the effect-lighting flag (a BGEM's, or Shader Flags 2 bit 30 of a
 *  property that names none) is not self-lit in the game: its pixel shader multiplies the colour by
 *      mix( 1, directional colour + sum over up to FOUR placed lights of colour x fade x falloff, lighting influence )
 *  falloff = pow( 1 - saturate( distance / radius )^2, 2.2 ) per PIXEL from its world position, times the spot
 *  cone for a spot; no N.L, no shadow, before the fog. The haze cards of Vault 111's cryo walkway are such
 *  shapes; drawn self-lit they turned the far door white.
 *
 *  The four lights belong to the placed MODEL (wwCellFxLitPickLights below holds the rule). The cell view welds,
 *  so in an interior it gives every lit effect shape a bucket per placement (src/cellview.cpp) and tells this
 *  table which model each written shape belongs to; the renderer asks it for the shape's lights per draw
 *  (src/gl/renderer.cpp, fo4_effectcell.prog). Part of the Cell lights row: no setting of its own. Exteriors
 *  too (lane SUNCELL1 final): the base term outdoors is the weather's sun x the imagespace Sunlight Scale
 *  (src/gl/celllights.cpp); no ambient, as the game's lit asm has none. No weather loaded = self-lit, as before.
 *  Red WW_CELL_FXLIT_EXT_RED=indoors keeps exteriors self-lit.
 *
 *  Pins (harness only):
 *    WW_CELL_FXLIT_PICK=strong   the four strongest lights at the bound's centre instead of the game's rule
 *    WW_CELL_FXLIT_RED=white     the lit effects drawn self-lit, as before this lane
 *                     =nofade    each light at its base's fade, the placement's fade offset left out
 *                     =all       every light of the cell instead of the model's four
 *                     =nopower   the falloff without its 2.2
 *    WW_CELL_FXLIT_DUMP=<file>   one line per lit model: serial, reference, bound, the lights picked
 *  Probes (WW_CELL_LIT_PROBE, tests/spells/cell_fx.sh stage L): in 70..74 the lit effects ALONE draw, opaque:
 *    70 the model's serial + 1 (24 bits), 71 / 72 the world position (as probes 2 / 3), 73 / 74 the multiplier
 *    / 4 as 16 bits, high and low byte. */
struct WwFxLitModel
{
	float center[3] = { 0, 0, 0 };  //!< the placed model's bounding sphere, world
	float radius = 0.0f;
	quint32 ref = 0;                //!< the placement's form
	QString model;
	int light[4] = { -1, -1, -1, -1 };          //!< indices into WwCellLighting::lights, -1 = none
	float scale[4] = { 1, 1, 1, 1 };            //!< 1, or the nofade red control's ratio
};

/*! WHICH LIGHTS A LIT EFFECT GETS -- the game's rule, as read from the game's code (lane FXLIT1's notes):
 *    - the list is the placed MODEL's (every shape of it shares one), never the card's or the pixel's;
 *    - a light is a candidate when it reaches the model's bounding sphere:
 *          distance( light, bound centre ) - bound radius < light radius;
 *    - bound radius UNDER 150 units: the candidates are sorted by (distance - bound radius) / light radius,
 *      LARGEST first -- the lights that reach the model least come first;
 *    - bound radius 150 or more: no sort. The order is the one the lights were added to the room in, which
 *      no record states; the plugin's record order stands in for it (INFERRED);
 *    - the FIRST FOUR of that list light the model.
 *  Not modelled: the room / portal visibility test the game also applies to a candidate, and lights that join
 *  the list after the model first showed (they are appended behind).
 *  `strongest` (WW_CELL_FXLIT_PICK=strong, harness only) picks instead the four candidates with the largest
 *  colour x falloff at the bound's centre. */
void wwCellFxLitPickLights( const QVector<WwCellLight> & lights, const float center[3], float radius,
	bool strongest, int out[4] );

//! A new scene for this document: its table is emptied.
void wwCellFxLitBegin( const void * nif );
//! One placed model with a lit effect shape; returns its serial.
int wwCellFxLitModel( const void * nif, const WwFxLitModel & m );
//! The written shape `block` belongs to model `serial`.
void wwCellFxLitShape( const void * nif, int block, int serial );
//! Red control data: light `index` of the published list at its base's fade is `ratio` x what was published.
void wwCellFxLitNoOffset( const void * nif, int index, float ratio );
//! After the document's lights are published: pick every model's lights (and write the dump).
void wwCellFxLitPick( const void * nif );
//! The model a shape belongs to, or null (not a lit effect of this document).
const WwFxLitModel * wwCellFxLitFor( const void * nif, int block, int * serial = nullptr );
//! Called with the effect program bound for `block`: the shape's lights (fxLit, fxLitScale, fxLitMode, fxLitId).
void wwCellFxLitUniforms( Scene * scene, int block );
//! True when this shape draws in the running probe pass (a lit effect, probe 70..74).
bool wwCellFxLitProbeShape( Scene * scene, int block );

//! Bounding sphere of `count` points (xyz, stride 3 floats): the box centre, the farthest point.
void wwCellFxLitSphere( const float * xyz, size_t count, float out[4] );
//! Grow sphere `a` (xyz, radius; radius < 0 = empty) to hold sphere `b`, as a scene node merges its children's.
void wwCellFxLitMerge( float a[4], const float b[4] );

#endif
