#ifndef CELLGRASS_H
#define CELLGRASS_H

#include "cellblockmap.h"

#include <QString>
#include <QtGlobal>

#include <vector>

class EsmWorld;
class Scene;

/*! The game's grass, placed the engine's way: per LAND quadrant 8 x 8 patches, each with its grass list and 3 x 3
 *  density grid, laid as a jittered lattice and welded into the cell's buckets (drawn by the cell programs).
 *  Stand-ins where the engine is not reproducible: a per-cell std::mt19937 keep test, no water, no wind angle
 *  jitter, forward lighting, no actor benders. Wind follows the game's grass vertex shader (fo4_default.vert).
 *  Row "Grass" ships OFF (read at cell open). WW_CELL_GRASS=0|1 pins it; WW_CELL_GRASS_RED=seed,
 *  WW_CELL_GRASS_DUMP=<file> and WW_CELL_GRASS_WIND_T=<s> (a fixed wind clock; harness runs default to 0) debug it.
 */

bool wwCellGrassOn();
void wwCellGrassSetOn( bool on );

//! One blade, the engine's own record: the 16 half floats CreateGrass writes, and where it came from.
struct WwGrassBlade
{
	int cx = 0, cy = 0, quadrant = 0, col = 0, row = 0;
	quint32 form = 0;
	int j = 0, i = 0;
	quint16 h[16] = {};
};

//! The blades of one exterior cell, in the engine's order. False when the cell has no LAND.
bool wwCellGrassPlace( const EsmWorld & world, int cx, int cy, std::vector<WwGrassBlade> & out );

/*! A blade's model transform as the stand-in vertex shader reads it: world = t + R * (p * s), R's columns
 *  V1 V2 V3 (row-major R[9]), s per axis. `shade` is the vertex tint. */
void wwCellGrassTransform( const WwGrassBlade & b, bool uniformScale, float t[3], float R[9], float s[3],
	float * shade );

//! The 12-cell block origin the blade's position halves are relative to.
void wwCellGrassBlockOrigin( int cx, int cy, float & x0, float & y0 );

//! Cell open: forget the last document's grass shapes; register one; the open's census line.
void wwCellGrassBegin( const void * nif );
//! lane CABLES1: `fade` false = the wind without the grass fade (a swaying cable)
void wwCellGrassShape( const void * nif, int block, float wavePeriod, bool fade = true );
// lane CELLSTREAM1: a streamed cell let go of this block (it is about to be reused)
void wwCellGrassForget( const void * nif, int block );
//! lane LIVESET3: the document's blocks were renumbered (a live set removed or moved some)
void wwCellGrassRemap( const void * nif, const WwBlockMap & m );
void wwCellGrassNote( const void * nif, const QString & line );
QString wwCellGrassEcho( const void * nif );

//! Per draw (fo4_cell / pbrm_cell): the fade on a grass shape, zero on every other (a no-op without the uniform).
void wwCellGrassUniforms( Scene * scene, int block );
//! lane BINDLESSMDI1: a grass shape (its own uniforms per draw)
bool wwCellGrassIs( Scene * scene, int block );

//! lane GPUDRIVEN1 step B: the shape waves in the wind (its vertices leave its bounds), so no culling bound holds it.
bool wwCellGrassIsShape( const void * nif, int block );

//! The wind constants from the loaded weather (the engine's wind update): wind = (angle, 0, 0, 0), wind2 = (min * 300,
//! max * 300, frequency, 1); false without a weather.
bool wwCellGrassWind( float wind[4], float wind2[4] );
//! lane FOGWIND1: the game's wind bearing from the weather's DATA direction byte, float32 in the engine's order (radians,
//! 0 = +X, counter-clockwise; offsetDeg = the random range offset, WEATHERBLEND3). Shared by the grass and the fog drift.
inline float wwGameWindAngle( quint8 dirByte, float offsetDeg = 0.0f )
{
	const float k255 = 1.0f / 255.0f;	// 0x3b808081
	const float dir = float( dirByte ) * k255 * 360.0f;
	const float twoPi = 6.28318548f;
	float angle = ( offsetDeg + dir ) * 0.0174532924f;
	if ( !( angle < twoPi ) )
		angle -= twoPi;
	else if ( angle < 0.0f )
		angle += twoPi;
	return angle;
}
//! lane FAR5: a swaying cable's (a no-fade block's) wind with its current phase in wind[3]; false for any other block
bool wwCellGrassCableWind( const void * nif, int block, float wind[4], float wind2[4] );
//! The blade's phase offset, (h0 + h1) * 0.0078125: the weld carries it in the bitangent's length (1024 + it).
float wwCellGrassPhaseOffset( const WwGrassBlade & b );
//! One shape's phase at time t: ((t * 0.0016666667) * 6.2831802) * the GRAS wave period, float32.
float wwCellGrassPhase( double t, float wavePeriod );
//! After the cell view's frame: the wind's clock steps (prev = cur, cur = now) unless pinned.
void wwCellGrassFrameEnd();
//! True once after a frame that drew waving grass: an interactive window paints again (never in a harness).
bool wwCellGrassWantsRepaint();

#endif
