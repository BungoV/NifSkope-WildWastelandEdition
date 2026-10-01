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
 * "axis" (spots shine along -Z), "nodalc" (the interior ambient dropped). */

#include <QString>
#include <QVector>

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
	QString summary;                    //!< one census line
};

//! the cell view publishes the lighting of the document it just built (replacing that document's last)
void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting );
const WwCellLighting * wwCellLightsFor( const void * nif );

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

#endif
