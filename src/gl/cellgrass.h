#ifndef CELLGRASS_H
#define CELLGRASS_H

#include <QString>
#include <QtGlobal>

#include <vector>

class EsmWorld;
class Scene;

/*! Lane GRASSMB1 (2026-10-05): THE GAME'S GRASS, placed the way the engine places it.
 *
 *  Ported op for op (float32, in the engine's order) from Todd's treat: AddCellGrass walks each LAND quadrant's
 *  8 x 8 patches (centre vertices 1, 3, .. 15; a patch is 256 units, its origin the centre vertex - 128);
 *  GetGrassParameters builds each patch's grass list (the base texture's GNAM grasses where the base shows,
 *  then every layer slot over 25/255 somewhere in the 3 x 3; at most 3 grasses a texture, 16 a patch; a 3 x 3
 *  density grid of density / 100, zeroed when its mean is under 0.005); CreateGrass lays an n x n lattice
 *  (n = min(256 / position range, 12)), keeps a point by its bilinear density against a draw, jitters it by the
 *  seeded table (Float0To1DistA: std::mt19937(1), seed = cellX * cellY), snaps it through a half float
 *  relative to its 12-cell block, reads the ground (the triangle's plane height and FACE normal), applies the
 *  water case and the slope window, then draws the shade, the yaw (fit to slope when flagged) and the height.
 *  Each blade is the engine's 16 half floats; the viewer welds the GRAS model at each blade into the cell's
 *  own buckets, drawn by the cell programs: one lighting and fog path, nothing forked.
 *
 *  Stated stand-ins (each a gap the exe cannot settle; the lane notes hold the trace):
 *   - the keep test draws the engine's GLOBAL generator (not reproducible); here one std::mt19937 per cell,
 *     seeded uint32(cellX) * 0x9E3779B1 ^ uint32(cellY), drawn in call order;
 *   - the cos table is filled at run time: cos(k 2 pi / 512); a layer byte is trunc(opacity * 255);
 *   - no water: the water height is -3.4e38; the ground colour is the nearest triangle corner's VCLR;
 *   - a blade that lands past its cell reads the neighbour's LAND; no quadrant culling (the viewer draws all);
 *   - the wind angle: the game adds the nearest of 4 BSRandom draws within +- the direction range / 2; here 0;
 *     one weather (no transition blend); the clock runs from the first grass frame, and an interactive window
 *     repaints while grass waves (never in a harness);
 *   - the bitangent is cross(normal, tangent), not the model's own; a model without vertex colours reads (1,1,1,1);
 *   - the colour times the shade passes through a byte (the game multiplies in the shader: a shade above 1 clips);
 *   - the per-blade phase offset rides only in the side mesh's float bitangents (cellmesh); with the side mesh
 *     off the document rows hold bytes and the offset is 0 (every blade of a type waves in phase);
 *   - the game lights grass deferred (its prepass's grass pixel output, then the deferred lights): not ported, the
 *     blades shade through fo4_cell / pbrm_cell (forward);
 *   - no actor benders (cb2[13..16]), and the motion blur's vectors are the camera's only (no grass motion).
 *
 *  The grass vertex shader IS the game's (Shaders011 entry 02183, the deferred prepass's grass technique 0x80):
 *  the blade is (X0 + h0, Y0 + h1, h2) + V1 x + V2 y + V3 z, the model scaled by 1 + h13 (z only unless uniform),
 *  coloured by the model's vertex colour times the shade; the wind (fo4_default.vert, uniforms from
 *  wwCellGrassUniforms): p = (phase - (h0 + h1) / 128) * frequency, w = ((sin(pi sin p) + sin(2 pi sin p)) 0.3 +
 *  cos(pi cos p) 0.2 + 1) (max - min) / 2 + min, times alpha^2 / 2, along (cos angle, sin angle, 0), added to the
 *  position and to the normal, tangent and bitangent (each renormalised). phase = timer / 600 * 2 pi * the GRAS
 *  wave period; min / max = fWindMin/MaxSpeed * 300 and the frequency from the weather's DATA (Sky::UpdateWind).
 *  The blade's (h0 + h1) / 128 rides in the welded bitangent's length (1024 + it), read back and renormalised by
 *  the vertex shader. The fade is fGrassStartFadeDistance 3500 + fGrassFadeRange 1000.
 *
 *  Row "Grass" in the Cell workspace; ships OFF; read at cell open (a switch reopens the cell). OFF adds
 *  nothing: no bucket, no uniform value but zero, so an OFF frame is the before-lane frame byte for byte.
 *  Pins (a harness without WW_CELL_GRASS is OFF whatever was saved):
 *   WW_CELL_GRASS=0|1            the row
 *   WW_CELL_GRASS_RED=seed       the jitter seed is off by one (the placement gate's red)
 *   WW_CELL_GRASS_DUMP=<file>    every blade: "B cx cy quadrant col row form j i h0..h15" (hex halves)
 *   WW_CELL_GRASS_WIND_T=<s>     the wind's clock fixed at s seconds (the previous frame 1/60 s before);
 *                                a harness run without it is fixed at 0 (never the wall clock)
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
void wwCellGrassShape( const void * nif, int block, float wavePeriod );
void wwCellGrassNote( const void * nif, const QString & line );
QString wwCellGrassEcho( const void * nif );

//! Per draw (fo4_cell / pbrm_cell): the fade on a grass shape, zero on every other (a no-op without the uniform).
void wwCellGrassUniforms( Scene * scene, int block );

//! The wind constants from the loaded weather (Sky::UpdateWind): wind = (angle, 0, 0, 0), wind2 = (min * 300,
//! max * 300, frequency, 1); false without a weather.
bool wwCellGrassWind( float wind[4], float wind2[4] );
//! The blade's phase offset, (h0 + h1) * 0.0078125: the weld carries it in the bitangent's length (1024 + it).
float wwCellGrassPhaseOffset( const WwGrassBlade & b );
//! One shape's phase at time t: ((t * 0.0016666667) * 6.2831802) * the GRAS wave period, float32.
float wwCellGrassPhase( double t, float wavePeriod );
//! After the cell view's frame: the wind's clock steps (prev = cur, cur = now) unless pinned.
void wwCellGrassFrameEnd();
//! True once after a frame that drew waving grass: an interactive window paints again (never in a harness).
bool wwCellGrassWantsRepaint();

#endif
