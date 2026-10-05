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
 *   - the grass vertex shader is not in the shader pack: the blade is placed as
 *     (X0 + h0, Y0 + h1, h2) + V1 x + V2 y + V3 z, the model scaled by 1 + h13 (z only unless uniform),
 *     tinted by the shade, NO wind; the fade is fGrassStartFadeDistance 3500 + fGrassFadeRange 1000.
 *
 *  Row "Grass" in the Cell workspace; ships OFF; read at cell open (a switch reopens the cell). OFF adds
 *  nothing: no bucket, no uniform value but zero, so an OFF frame is the before-lane frame byte for byte.
 *  Pins (a harness without WW_CELL_GRASS is OFF whatever was saved):
 *   WW_CELL_GRASS=0|1            the row
 *   WW_CELL_GRASS_RED=seed       the jitter seed is off by one (the placement gate's red)
 *   WW_CELL_GRASS_DUMP=<file>    every blade: "B cx cy quadrant col row form j i h0..h15" (hex halves)
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
void wwCellGrassShape( const void * nif, int block );
void wwCellGrassNote( const void * nif, const QString & line );
QString wwCellGrassEcho( const void * nif );

//! Per draw (fo4_cell / pbrm_cell): the fade on a grass shape, zero on every other (a no-op without the uniform).
void wwCellGrassUniforms( Scene * scene, int block );

#endif
