#ifndef WW_FOGTERRAIN_H
#define WW_FOGTERRAIN_H

/* lane FOGTERRAIN1 (10-27): the ground fog layer stands on the terrain. The terrain-only heights the terrain shadow
 * already rasters (cell LAND + the far terrain LOD, src/gl/shadowset.cpp) are low-passed here into a coarse height
 * grid; the air pass (res/shaders/cell_airmed.glsl airGroundAt) raises the layer's anchor by the local ground's rise
 * over the fog frame's ground, so thickness and strength stay measured above the local ground.
 * Lane FOGTERRAIN2: the grid has its own job (shadowset.cpp fogTerrainFrame: no sun, no terrain-shadow tier), and the
 * surfaces' far tail, sky and sun (lookdev_fog.glsl wwAirGroundAt) and the particles (cell_volpart.glsl) stand on it too.
 * Red WW_FOG_TERRAIN_RED=absolute: the old absolute anchor everywhere. */

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
};

constexpr double kFogTerrCell = 512.0;	// the grid's texel (game units)
// the Gaussian's sigma (game units). 256 = half a texel: the 512 u box already removes the bumps, and 1024 kept a
// 300 u layer's hilltops 21 % thin (notes/fogterrain2/RESULTS.md, the sigma study)
constexpr double kFogTerrSigma = 256.0;
constexpr int kFogTerrRasterN = 2048;	// lane FOGTERRAIN2: the job's raster, 2048 texels of 256 u (524288 u), box 2 x 2
constexpr double kFogTerrRasterT = 256.0;
constexpr int kFogTerrIdleFrames = 120;	// the job retires (frees its grid) this many frames after the last request

// the grid is flat at the frame's ground (within half a unit): the absolute law is exact there, so it is drawn
inline bool wwFogTerrainFlatAt( const WwFogTerrGrid & g, float ref )
{
	return g.hmax - ref <= 0.5f && ref - g.hmin <= 0.5f;
}

/* the low pass: box-average the heights (texels of size T, `noH` where no terrain) onto kFogTerrCell texels, then a
 * separable Gaussian of sigma `sigma` over the valid texels only (normalised); texels no valid height reaches take the
 * mean of the valid ones. A constant field comes back exactly constant. */
void wwFogTerrainLowpass( const std::vector<float> & H, int N, double T, double x0, double y0, float noH, double sigma,
	WwFogTerrGrid & out );

void wwFogTerrainPublish( WwFogTerrGrid && g );	// the worker's result (render thread)
const WwFogTerrGrid & wwFogTerrainGrid();	// the last published grid (gen 0: none)
bool wwFogTerrainWanted();	// the layer follows the terrain (live; red absolute: false)

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

#endif
