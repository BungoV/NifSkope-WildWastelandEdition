/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODPPLACE_H
#define LODPPLACE_H

#include "lodpfile.h"
#include "lodofile.h"

#include <functional>
#include <map>
#include <utility>
#include <vector>

/*! LOD PRTP step 1 (lane L1): cut every `.lodo` mesh into CHARTS and place
 *  `.lodp` patches on the faces the far view draws. QtCore only, so the
 *  standalone writer gate links it alone. PLAN.md sections 3-4,
 *  docs/LODGEN_NATIVE_LODO_LODI.md section 13.
 *
 *  THE CHART RULE (one implementation, used by lodgen to write the `.lodo`
 *  chart stream and by the placer; tests/spells/lodp_decode.py is its
 *  independent twin). Over a mesh's LEVEL-0 triangles in cluster order, the
 *  face normal of each from its DEQUANTISED positions in double precision
 *  (lo + q / 65535 * extent):
 *   * a triangle whose cross product is no longer than 1e-6 is degenerate:
 *     LODP_CHART_NONE;
 *   * a triangle of an alpha-tested TREE material (LODO_MAT_TREE with
 *     alphaThreshold != 0: the leaf cards) is LODP_CHART_FOLIAGE;
 *   * otherwise the first triangle not yet in a chart SEEDS a new chart, and
 *     every triangle not yet in one whose normal n has n . n_seed >= cos 45
 *     joins it.
 *   A coarser level's triangle takes the chart whose area-weighted normal is
 *   closest (largest dot, first on a tie); foliage and degenerate as above.
 *
 *  THE SLOT RULE. A chart's frame is N = its area-weighted normal, U =
 *  cross(up, N) normalised with up = Z unless |N.z| >= 0.9 (then X), V = N x U,
 *  origin = the minimum of (p.U, p.V, p.N) over the chart's vertices. Slots
 *  are cubes of edge `step` = patch units / placement scale, ceil(max (p -
 *  origin) . axis / step) of them along each axis (at least one); a point
 *  goes to slot clamp(floor(d / step), 0, n - 1), so the last slot is open at
 *  its far side and a vertex exactly on a boundary opens no empty row. Each triangle is
 *  clipped against the slot boxes in 3D; a slot whose clipped area is more
 *  than 1e-6 step^2 holds a patch. The patch stands at the area-weighted
 *  centroid of its clipped pieces, moved onto the nearest piece when the
 *  centroid is not on one, with the pieces' area-weighted normal and their
 *  area. */

//! One mesh's charts.
struct LodpMeshCharts
{
	int charts = 0;                     //!< surface charts, ids 0..charts-1
	bool foliage = false;               //!< has LODP_CHART_FOLIAGE triangles at level 0
	//! One byte a cluster triangle over the mesh's whole cluster range (every level), 16 a cluster.
	std::vector<quint8> triChart;
	std::vector<double> normal;         //!< 3 a chart, area-weighted, unit
};

/*! Charts of `mesh`. False + `error` when the mesh has more than
 *  LODP_CHART_MAX + 1 charts. */
bool lodpMeshCharts( const LodoLibrary & lib, quint32 mesh, LodpMeshCharts * out, QString * error );

/*! The `.lodo` v8 chart stream for a whole library: `clusters x 16` bytes,
 *  plus each mesh's surface chart count. */
bool lodpChartStream( const LodoLibrary & lib, std::vector<quint8> * stream, std::vector<quint8> * meshCharts, QString * error );

//! One drawn placement, as the CLI decodes it from the `.lodi`.
struct LodpPlacementIn
{
	quint32 lodiIndex = 0;
	quint32 mesh = 0;           //!< the `.lodo` mesh drawn (the base's first present rep)
	float pos[3] = { 0, 0, 0 };
	float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };  //!< row-major, world = R * local
	float scale = 1.0f;
};

//! The ground, as the CLI reads it from the `.lodl` (synthetic in the fixture).
struct LodpTerrainIn
{
	int minX = 0, minY = 0, maxX = -1, maxY = -1;  //!< cell bounds, inclusive
	int samplesPerCell = 0;
	//! Height at a full-rate global sample: gx = (cellX - minX) * samplesPerCell + local.
	std::function<float( int gx, int gy )> height;
	//! Cell flags: land, and the water plane when there is one.
	std::function<void( int cx, int cy, bool * land, bool * water, float * waterH )> cell;
};

struct LodpPlaceOptions
{
	float patchUnits = LODP_PATCH_UNITS_DEFAULT;
	int terrainGrid = LODP_TERRAIN_GRID_DEFAULT;
	quint64 worldspaceHash = 0;
	quint64 lodInputHash = 0;
	quint64 lodoIdentity = 0;
	bool bound = false;         //!< set LODP_FLAG_BOUND (the caller checked the stream)
	//! Red controls for the gates only (never a user switch): 1 = push every object patch 8 u along its normal.
	int red = 0;
};

struct LodpPlaceStats
{
	quint64 placements = 0, placedObjects = 0, meshesCut = 0, meshScaleKeys = 0;
	quint64 charts = 0, treeCharts = 0, slotIds = 0;
	quint64 objectPatches = 0, treePatches = 0, terrainPatches = 0, submerged = 0, landCells = 0;
	quint64 centroidMoved = 0;          //!< patches whose centroid was off the faces and moved onto them
	quint64 maxPatchesPerCell = 0, cellsOverCap = 0;   //!< cap 800 (PLAN.md section 3)
	quint64 sectors = 0, bytes = 0;
	double maxReach = 0.0;              //!< farthest patch from its placement's cell, units
};

/*! Places every patch and returns the sector files in memory, keyed by
 *  (sectorX, sectorY). `terrain` may be null (objects only). */
bool lodpPlace( const LodoLibrary & lib, const std::vector<LodpPlacementIn> & placements,
	const LodpTerrainIn * terrain, const LodpPlaceOptions & opt,
	std::map<std::pair<int, int>, LodpSector> * sectors, LodpPlaceStats * stats, QString * error );

/*! Decodes one `.lodp` patch's world position (x, y, z) from its cell. */
void lodpPatchWorld( const LodpSector & s, int cellIndex, const LodpPatch & p, double xyz[3] );

/*! The known-answer fixture (src/lodpfixture.cpp): writes `Synthetic.lodo`
 *  (v8, with its chart stream), the sector files, `placements.tsv` and
 *  `Synthetic.lodp.expect.txt` into `dir`. `red` is passed to the placer
 *  (LodpPlaceOptions::red) so the gates can write their red controls. */
bool lodpFixtureWrite( const QString & dir, int red, QStringList * report, QString * error );
//! The fixture's ground: 64 + 3 (gx mod 37) + 2 (gy mod 23).
float lodpFixtureHeight( int gx, int gy );

#endif
