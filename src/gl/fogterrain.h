#ifndef WW_FOGTERRAIN_H
#define WW_FOGTERRAIN_H

/* lane FOGTERRAIN1 (10-27): the ground fog layer stands on the terrain. The terrain-only heights the terrain shadow
 * already rasters (cell LAND + the far terrain LOD, src/gl/shadowset.cpp) are low-passed here into a coarse height
 * grid; the air pass (res/shaders/cell_airmed.glsl airGroundAt) raises the layer's anchor by the local ground's rise
 * over the fog frame's ground, so thickness and strength stay measured above the local ground.
 * Lane FOGTERRAIN2: the grid has its own job (shadowset.cpp fogTerrainFrame: no sun, no terrain-shadow tier), and the
 * surfaces' far tail, sky and sun (lookdev_fog.glsl wwAirGroundAt) and the particles (cell_volpart.glsl) stand on it too.
 * Lane FOGTERRAIN3: the layer stands on the water where the water is above the ground (the scene's water shapes,
 * rastered with the terrain); a fine level of 128 u texels around the camera for steep ground (layer 1 of the texture
 * array); the far tail and the sky read three samples along the ray (lookdev_fog.glsl wwAirGroundRay).
 * Red WW_FOG_TERRAIN_RED=absolute: the old absolute anchor everywhere; =seafloor: the water left out; =coarse: no fine
 * level. */

#include <cstdint>
#include <vector>

struct WwFogTerrGrid
{
	int nx = 0, ny = 0;
	double x0 = 0, y0 = 0, cell = 0;	// texel (i, j) centre at x0 + (i + 0.5) cell
	std::vector<float> H;	// world z, low-passed
	float hmin = 0.0f, hmax = 0.0f;
	std::uint64_t gen = 0;
	int shapes = 0;	// lane FOGTERRAIN2: the job's terrain shapes and its worker ms (telemetry)
	double ms = 0;
	// lane FOGTERRAIN3: the fine level around the camera (fnx 0: none), same texel count as the coarse one
	int fnx = 0;
	double fx0 = 0, fy0 = 0, fcell = 0;
	std::vector<float> F;
	int water = 0, wet = 0;	// water shapes rastered; raster texels the water raised (telemetry)
};

constexpr double kFogTerrCell = 512.0;	// the grid's texel (game units)
// the Gaussian's sigma (game units). 256 = half a texel: the 512 u box already removes the bumps, and 1024 kept a
// 300 u layer's hilltops 21 % thin (notes/fogterrain2/RESULTS.md, the sigma study)
constexpr double kFogTerrSigma = 256.0;
constexpr int kFogTerrRasterN = 2048;	// lane FOGTERRAIN2: the job's raster, 2048 texels of 256 u (524288 u), box 2 x 2
constexpr double kFogTerrRasterT = 256.0;
constexpr int kFogTerrIdleFrames = 120;	// the job retires (frees its grid) this many frames after the last request
// lane FOGTERRAIN3: the fine level, 1024 texels of 128 u (131072 u) rastered at 128 u. The NW mountains' 16-cell windows
// need it: at 512 u their 300 u layer read 0.44..1.51 of the flat density (notes/fogterrain3/RESULTS.md, the study)
constexpr int kFogTerrFineN = 1024;
constexpr double kFogTerrFineCell = 128.0;
constexpr double kFogTerrFineSigma = 128.0;

// the grid is flat at the frame's ground (within half a unit): the absolute law is exact there, so it is drawn
inline bool wwFogTerrainFlatAt( const WwFogTerrGrid & g, float ref )
{
	return g.hmax - ref <= 0.5f && ref - g.hmin <= 0.5f;
}

/* the low pass: box-average the heights (texels of size T, `noH` where no terrain) onto kFogTerrCell texels, then a
 * separable Gaussian of sigma `sigma` over the valid texels only (normalised); texels no valid height reaches take the
 * mean of the valid ones. A constant field comes back exactly constant. */
void wwFogTerrainLowpass( const std::vector<float> & H, int N, double T, double x0, double y0, float noH, double sigma,
	WwFogTerrGrid & out, double cell = kFogTerrCell );

// lane FOGTERRAIN3: the water raster `W` (noH where no water) raises the terrain raster `H` where it is higher (a sea or a
// lake: the layer stands on its surface, not on its floor; a texel with water and no terrain takes the water). Returns
// the texels raised.
int wwFogTerrainWaterMerge( std::vector<float> & H, const std::vector<float> & W, float noH );
// the fine level from its raster (texels of size T): the same low pass at kFogTerrFineCell / kFogTerrFineSigma into
// g.F, and g.hmin / g.hmax widened to cover it
void wwFogTerrainFine( const std::vector<float> & H, int N, double T, double x0, double y0, float noH, WwFogTerrGrid & g );

void wwFogTerrainPublish( WwFogTerrGrid && g );	// the worker's result (render thread)
const WwFogTerrGrid & wwFogTerrainGrid();	// the last published grid (gen 0: none)
bool wwFogTerrainWanted();	// the layer follows the terrain (live; red absolute: false)
enum class WwFogTerrRed { None, Absolute, Seafloor, Coarse };
WwFogTerrRed wwFogTerrainRed();	// WW_FOG_TERRAIN_RED (live): absolute | seafloor | coarse

void wwFogTerrainRequest();	// the air pass needs the grid this frame
bool wwFogTerrainRequested();	// once a frame (the job): asked within the last kFogTerrIdleFrames frames

// the job's frame decision, pure (notes/fogterrain2/gate): no sun and no terrain-shadow input, on purpose
struct WwFogTerrFrame
{
	bool requested = false;
	int shapes = 0;	// terrain shapes in the scene
	bool setChanged = false;	// the terrain set or the raster's window moved since the last raster
	bool busy = false;	// the worker runs
	bool haveGrid = false;	// a non-empty grid is published
};
enum class WwFogTerrAct { Idle, Retire, Empty, Raster };	// Retire: stop, publish empty, free; Empty: publish empty
WwFogTerrAct wwFogTerrainAct( const WwFogTerrFrame & f );

// the surfaces' rows (lookdev_fog.glsl wwAirGroundAt): uv = (dot(U.xyz, p) + U.w, dot(V.xyz, p) + V.w) at view point p,
// from the view's inverse rows (celllights.cpp cellRow: world = dot(row.xyz, p) + row.w) and the grid's airTerrXf
inline void wwFogTerrainViewRows( const float row0[4], const float row1[4], const float xf[4], float U[4], float V[4] )
{
	for ( int k = 0; k < 4; k++ ) {
		U[k] = row0[k] * xf[0];
		V[k] = row1[k] * xf[2];
	}
	U[3] += xf[1];
	V[3] += xf[3];
}

// a level's world -> uv rows (airTerrXf, airTerrFineXf: u = x X.x + X.y, v = y X.z + X.w); a missing fine level maps
// every point to uv -1, which the shaders' edge weight reads as 0 (the coarse level alone)
inline void wwFogTerrainXf( const WwFogTerrGrid & g, bool fine, float xf[4] )
{
	const int n = fine ? g.fnx : g.nx;
	const double c = fine ? g.fcell : g.cell, x0 = fine ? g.fx0 : g.x0, y0 = fine ? g.fy0 : g.y0;
	if ( n <= 1 || !( c > 0.0 ) ) {
		xf[0] = xf[2] = 0.0f;
		xf[1] = xf[3] = -1.0f;
		return;
	}
	const double s = 1.0 / ( c * n );
	xf[0] = float( s );
	xf[1] = float( -x0 * s );
	xf[2] = float( s );
	xf[3] = float( -y0 * s );
}

#endif
