#ifndef WW_FOGTERRAIN_H
#define WW_FOGTERRAIN_H

/* lane FOGTERRAIN1 (10-27): the ground fog layer stands on the terrain. The terrain-only heights the terrain shadow
 * already rasters (cell LAND + the far terrain LOD, src/gl/shadowset.cpp) are low-passed here into a coarse height
 * grid; the air pass (res/shaders/cell_airmed.glsl airGroundAt) raises the layer's anchor by the local ground's rise
 * over the fog frame's ground, so thickness and strength stay measured above the local ground.
 * Red WW_FOG_TERRAIN_RED=absolute: the old absolute anchor. */

#include <cstdint>
#include <vector>

struct WwFogTerrGrid
{
	int nx = 0, ny = 0;
	double x0 = 0, y0 = 0, cell = 0;	// texel (i, j) centre at x0 + (i + 0.5) cell
	std::vector<float> H;	// world z, low-passed
	float hmin = 0.0f, hmax = 0.0f;
	std::uint64_t gen = 0;
};

constexpr double kFogTerrCell = 512.0;	// the grid's texel (game units)
constexpr double kFogTerrSigma = 1024.0;	// the Gaussian's sigma (game units; notes/fogterrain1/RESULTS.md: why)

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

#endif
