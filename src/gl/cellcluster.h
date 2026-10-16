/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_CELLCLUSTER_H
#define WW_CELLCLUSTER_H

/* Clustered light lists for the cell's lit shaders: screen tiles x logarithmic view-depth slices, the slicing of
 * the Skyrim Community Shaders Light Limit Fix (github.com/doodlum/skyrim-community-shaders, features/Light
 * Limit Fix: clustered shading, Olsson et al. 2012). Pure C++ so the unit test can drive it without GL.
 * Each cluster lists, in ascending light order, every light that can touch it; a pixel's lighting sum over
 * its list therefore equals the flat loop's bit for bit. A cluster past the cap lists nothing and is marked
 * for the flat loop (count -1), never truncated. */

#include <cstdint>
#include <vector>

//! A light in VIEW space (the camera at the origin looking down -z), as the culling sees it.
struct WwClusterLight
{
	double pos[3] = {};
	double radius = 0.0;
	int shape = 0;          //!< 0 omni (and box: its volume lies inside the radius), 1 spot, 2 hemisphere
	double dir[3] = {};     //!< spot: the cone axis; hemisphere: the half space's normal (lit where (P - pos).dir >= 0)
	double cosOuter = -1.0; //!< spot: cos of the half angle; the light is zero at or outside it
};

struct WwClusterParams
{
	int tilePx = 80;
	int slices = 24;
	int maxPerCluster = 512;
	bool redMidSlice = false;   //!< the refuter: each slice culled as its middle depth only (lights at its edges get dropped)
};

struct WwClusterGrid
{
	int tilesX = 0, tilesY = 0, slices = 0, tilePx = 0;
	int vp[4] = {};
	float logNear = 0.0f, sliceScale = 0.0f;    //!< slice = floor((log(depth) - logNear) * sliceScale)
	std::vector<std::int32_t> cells;            //!< per cluster: (offset into index, count); count -1 = walk every light
	std::vector<std::int32_t> index;
	int refs = 0, overflowed = 0, maxCount = 0, culledOffscreen = 0;
	double buildMs = 0.0;

	int clusterCount() const { return tilesX * tilesY * slices; }
	//! The shader's rule: the cluster of a fragment at window pixel (fx, fy) with view depth `depth`.
	int clusterOf( float fx, float fy, float depth ) const;
};

/*! Build the lists for one camera: `proj` is the projection matrix column-major (m[col * 4 + row]), `vp` the GL
 *  viewport. False (grid left empty) when the projection is not a plain perspective or orthographic one. */
bool wwClusterBuild( const float proj[16], const int vp[4], const std::vector<WwClusterLight> & lights,
	const WwClusterParams & params, WwClusterGrid & out );

#endif // WW_CELLCLUSTER_H
