/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_CELLLIGHTS_H
#define WW_CELLLIGHTS_H

/* CELL LIGHTS (lane PRTP3, 2026-10-01; docs/PRTP_PLAN.md 2, docs/PRTP2_LIGHT_MODEL.md).
 *
 * The cell view drawn with the cell's own lights: every placed LIGH (point and spot, the PRTP2
 * curve), and for an interior its XCLL / lighting template directional ambient (DALC) and
 * directional light, in place of the viewport's camera light. The cell view (src/cellview.cpp)
 * publishes one WwCellLighting per document it builds; the renderer swaps fo4_default for
 * fo4_cell.prog (the same shader with WW_CELLLIGHTS compiled in) on every draw of that document
 * while the Cell lights row is on. Every other document, and the row off, draw as before.
 *
 * Units: colors are linear (byte/255)^2.2 x FNAM fade, as PRTP2 section 0. The legacy FO4 program
 * works in sqrt-of-linear (its tonemap squares), so the cell branch sums in linear and writes the
 * square root.
 *
 * The spot axis is MEASURED, not assumed (scratchpad/prtp1_20260930/spot_axis.py over
 * Fallout4.esm): of 2,396 placed spots, local +X aims 1,177 down and 581 up, -Z 608 / 131, every
 * other axis under 6% down. +X it is; the PRTP4 capture is its refuter.
 *
 * Harness pins (read once): WW_CELL_LIT=0|1 (the row), WW_CELL_LIT_PROBE=<mode> (every cell-lit
 * fragment writes raw: 1 the placed lights' irradiance / 4, 2 the world position's high bytes,
 * 3 its low bytes (each over the 65536-unit box centred on the published centre), 4 the world
 * normal * 0.5 + 0.5), WW_CELL_LIT_RED=<red>: "linear" (the radial curve without its 2.2 power),
 * "axis" (spots shine along -Z), "nodalc" (the interior ambient dropped). Probe 5 (lane PRTPGI): the
 * bounce's irradiance E(N) / pi, raw. */

#include <QString>
#include <QVector>

#include <vector>

class Scene;

struct WwCellLight
{
	float pos[3] = { 0, 0, 0 };     //!< world
	float radius = 0.0f;
	float color[3] = { 0, 0, 0 };   //!< linear, fade folded in
	bool spot = false;
	float dir[3] = { 1, 0, 0 };     //!< world, the way a spot shines
	float cosOuter = 0.0f;          //!< cos(FOV / 2)
	float bias = 0.0f, scale = 1.0f, exponent = 2.0f;   //!< DATA Constant, Scalar, Exponent
	float cone = 1.0f;              //!< DATA Falloff Exponent (the spot edge)
	bool noSpecular = false;        //!< flag 0x8000
};

struct WwCellLighting
{
	bool interior = false;
	QVector<WwCellLight> lights;
	bool hasDalc = false;
	float dalc[6][3] = {};          //!< byte / 255 as stored (PRTP2 powers AFTER the affine sum), X+ X- Y+ Y- Z+ Z-
	bool hasDirectional = false;
	float dirColor[3] = { 0, 0, 0 };    //!< linear
	float dirTo[3] = { 0, 0, 1 };       //!< world, TO the light
	float center[3] = { 0, 0, 0 };      //!< the probe modes' position box centre
	/* lane IMGS1: the cell's imagespace (XCIM -> IMGS, wbDefinitionsFO4 layout), empty when the cell has none.
	 * hdr = HNAM: eye adapt speed, tonemap E, bloom threshold, bloom scale, exposure max, exposure min, sunlight
	 * scale, sky scale, middle gray; cine = CNAM saturation, brightness, contrast; tint = TNAM amount, r, g, b;
	 * lut = the TX00 3D LUT, 16x16x16 RGB8 (r fastest), empty when the strip was not found. */
	bool hasImageSpace = false;
	float isHdr[9] = { 3, 7, 0.6f, 0.5f, 0.15f, 0.15f, 1.8f, 1.5f, 3 };
	float isCine[3] = { 1, 1, 1 };
	float isTint[4] = { 0, 1, 1, 1 };
	QString isName, isLutPath;
	std::vector<unsigned char> isLut;
	QString summary;                    //!< one census line
};

//! the cell view publishes the lighting of the document it just built (replacing that document's last)
void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting );
const WwCellLighting * wwCellLightsFor( const void * nif );

/*! THE BOUNCE (lane PRTPGI, src/probegi.h): the bake relit by these lights, as a voxel grid of
 *  probe ambient cubes. Six slabs (+X -X +Y -Y +Z -Z) of dims[2] each, x fastest, (rgb x valid,
 *  valid) a voxel; the shader adds albedo x E(N) / pi, E blended over the three facing slabs by n^2. */
struct WwCellGi
{
	float origin[3] = { 0, 0, 0 };
	float voxel = 64.0f;
	int dims[3] = { 0, 0, 0 };
	std::vector<float> rgba;
	QString summary;
};
void wwCellGiPublish( const void * nif, const WwCellGi & gi );
const WwCellGi * wwCellGiFor( const void * nif );
//! the GI row (ships off); the pin WW_CELL_GI wins. Draws only while the Cell lights row is on.
bool wwCellGiOn();
void wwCellGiSetOn( bool on );

//! the Cell lights row (ships off); the pin WW_CELL_LIT wins
bool wwCellLightsOn();
void wwCellLightsSetOn( bool on );

//! this scene draws cell-lit: the row on, the scene's document published, a perspective non-picking draw
bool wwCellLightsWanted( Scene * scene );
//! the uniforms of the renderer's CURRENT program (a no-op for a program without `cellOn`)
void wwCellLightsUniforms( Scene * scene );
//! census echo: "celllit=on lights=N dalc=.. dir=.. probe=.. red=.."
QString wwCellLightsEcho( Scene * scene );
//! the red bits (1 linear, 2 axis, 4 nodalc); the cell view applies "axis" when it publishes
int wwCellLightsRed();

/*! THE IMAGESPACE (lane IMGS1): the game's own HDR -> display chain, transcribed from the shipped shaders
 *  (Fallout4 - Shaders.ba2, Shaders011.fxp; docs/PRTP_PLAN.md 2j):
 *    adapted  = the frame's mean luminance (0.2125, 0.7154, 0.0721), Inf/NaN read as 0 (the downsample
 *               chain and its adaptation step; a still view is the converged value)
 *    exposure = clamp( middle gray / (adapted + 0.001), exposure min, exposure max )
 *    x        = exposure * hdr;  Hable (A .15, B .5, C .1, D .2, E = tonemap E, F .3), W 11.2, x2 in
 *    grade    = mix(luma, c, saturation) -> mix(.., luma * tint, amount) -> contrast * (brightness * c -
 *               adapted) + adapted
 *    display  = LUT( pow(grade, 1 / 2.2) * 15/16 + 1/32 )
 *  bloom    = added to hdr before the exposure (lane BLOOM1, wwCellImageSpaceSetBloom below)
 *  The measure: the frame drawn raw (probe 6) into a float target at a quarter size, read back. Row ships
 *  off; the pin WW_CELL_IS wins. WW_CELL_IS_DUMP=<file> writes the measure (full size) and the numbers;
 *  WW_CELL_IS_RED=nolut|noexp|nograde|nobloom its refuters. */
bool wwCellImageSpaceOn();
void wwCellImageSpaceSetOn( bool on );
//! the cell view draws through the imagespace: cell-lit, the row on, the document's cell has one
bool wwCellImageSpaceWanted( Scene * scene );
//! the measure pass's switch: while true every cell-lit fragment writes its raw linear colour
void wwCellImageSpaceMeasuring( bool on );
//! true inside the measure pass: the renderer masks every program that is not cell-lit (effects, sky, debug
//! draw nothing there -- the game's adapted value is the lit surfaces' light, ours have no linear output)
bool wwCellImageSpaceIsMeasuring();
//! the measured mean luminance of the last measure (negative before any)
void wwCellImageSpaceSetAdapted( Scene * scene, float lum, int pixels );
/*! lane BLOOM1: the bloom from the measure's float RGBA (w x h, step 4 when it is full size: box-averaged to a
 *  quarter first), HNAM bloom scale * max(0, c - bloom threshold), blurred 15 taps vertical then horizontal;
 *  the shader adds it (bilinear, a quarter of the view) to the HDR before the exposure */
void wwCellImageSpaceSetBloom( Scene * scene, const float * rgba, int w, int h, int step );
QString wwCellImageSpaceEcho( Scene * scene );

#endif
