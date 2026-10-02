#ifndef PROBEGI_H
#define PROBEGI_H

/*! THE BAKE RELIT BY THE CELL'S LIGHTS (lane PRTPGI, 2026-10-01; docs/PRTP_PLAN.md 2i).
 *
 *  bungo 2026-10-01: "simulate the GI for me with lights ... based on the surfels and sky
 *  visibility generated". One bounce, from the bake's own `.tbk` files:
 *    1. every surfel is lit by the cell's placed lights (src/gl/celllights.h, the PRTP2 curves),
 *       each light behind a shadow ray through the bake's soup; the last `fixtureClear` units
 *       before the light are not tested (the lamp's own shade would block every ray)
 *       B = albedo (linear) x E, the surfel's outgoing light in the viewport's units
 *    2. every probe gathers its links: E_axis = sum B x solid angle x max(axis . dir, 0) on the six
 *       world axes (an ambient cube), the unlinked share renormalized over the surfaces (FO4CS's
 *       relight rule). Sky: an interior has none; a weather-lit exterior's sky and sun: src/probesky.h.
 *    3. a voxel grid over the surfaces: each voxel blends the probes within `radius` that it can
 *       SEE (a ray voxel -> probe through the soup), weight (1 - (d / radius)^2)^2. A probe behind a
 *       wall never lights the voxel: the guard against indoor/outdoor bleed.
 *  The renderer samples the grid (src/gl/celllights.cpp, unit 13) and adds albedo x E / pi. */

#include "probeplace.h"
#include "probesky.h"

#include <QString>

#include <vector>

struct WwCellLighting;

struct ProbeGiResult
{
	bool ok = false;
	QString error;
	int files = 0, probes = 0, surfels = 0, links = 0, linksUnresolved = 0;
	int surfelsLit = 0;                 //!< surfels at least one light reaches unblocked
	qint64 shadowRays = 0, shadowBlocked = 0;
	float origin[3] = { 0, 0, 0 };      //!< the grid's min corner (world)
	float voxel = 64.0f;
	int dims[3] = { 0, 0, 0 };
	float radius = 0.0f;                //!< the probe blend radius
	int voxelsNear = 0, voxelsValid = 0;
	qint64 visRays = 0, visBlocked = 0;
	/*! six slabs (+X -X +Y -Y +Z -Z), each dims[2] deep, x fastest: (r, g, b, valid) a voxel; rgb is
	 *  E x valid, so a filtered sample divides by its own valid (the renderer's 3D texture as is) */
	std::vector<float> grid;
	std::vector<float> probeCube;       //!< per probe: pos[3] + 6 x rgb
	std::vector<float> surfelOut;       //!< per unique surfel: pos[3] nrm[3] albedo[3] B[3]
	double msLight = 0, msGather = 0, msGrid = 0;
	// lane SKY1 (src/probesky.h): the sky and the sun of a weather-lit exterior
	bool sky = false;                   //!< the relight took them (the grid's sky then stands in for the weather's ambient)
	QString skyLabel;
	int surfelsSun = 0;                 //!< surfels the sun reaches
	qint64 sunRays = 0, sunBlocked = 0;
	int probesSky = 0;                  //!< probes that see any sky
	double skyVisMean = 0;              //!< over probes: the mean of the eight octants' sky share
	int probesTinted = 0;               //!< probes with a tinted octant (sky seen through glass)
	std::vector<float> probeSky;        //!< per probe: 6 x rgb, the sky's part of probeCube
	std::vector<float> surfelSun;       //!< per unique surfel: rgb, the sun's part of B
};

struct ProbeGiSpec
{
	float fixtureClear = 24.0f;     //!< units before the light a shadow ray does not test
	float voxelMin = 48.0f;         //!< the grid's voxel, raised until it fits maxVoxels
	int maxVoxels = 400000;
	float radiusScale = 2.0f;       //!< blend radius = this x the median probe-to-nearest-probe distance
	/*! deliberate defects for the gate's refuters: "noshadow" (no shadow rays), "novis" (the grid
	 *  ignores visibility), "flip" (links gathered from the opposite direction). Empty in real runs. */
	QString red;
	/*! lane SKY1: the weather light of an exterior (off = none: an interior, or a view the weather
	 *  does not light), and its refuters (WW_CELL_SKY_RED): "off" neither sky nor sun, "novis" every
	 *  octant sees the sky, "notint" the glass tint is ignored, "sunthrough" the sun has no shadow ray. */
	ProbeSkyLight sky;
	QString skyRed;
};

//! Relight the bake in `bakeDir` (its sector_*.tbk) with `lighting`, shadowed through `soup`.
bool probeGiRelight( const ProbeSoup & soup, const QString & bakeDir, const WwCellLighting & lighting,
	const ProbeGiSpec & spec, ProbeGiResult * out );
//! One census line.
QString probeGiCensusText( const ProbeGiResult & r );
//! The gate's dump: gi_surfels.bin, gi_probes.bin, gi_grid.bin in `dir` (layouts in probegi.cpp).
bool probeGiDump( const ProbeGiResult & r, const ProbeGiSpec & spec, const QString & dir, QString * err );

#endif // PROBEGI_H
