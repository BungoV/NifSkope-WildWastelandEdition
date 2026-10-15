/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODPFILE_H
#define LODPFILE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

/*! `.lodp` v1 -- LOD PATCHES (LOD PRTP, lane L1 LODP-PLACE, 2026-10-15).
 *
 *  Precomputed light for the DISTANT LOD: small surface patches (about one
 *  per 4-8 m) that sit ON the blocky LOD faces the far view draws -- the
 *  `.lodo` meshes the `.lodi` places, and the `.lodl` ground. L1 places them
 *  and writes their geometry; L2 fills the light words (horizon, openness,
 *  albedo, links) by tracing FULL detail. Plan: notes/lodprtp1/PLAN.md.
 *  Contract: docs/LODGEN_NATIVE_LODO_LODI.md section 13.
 *
 *  One file per SECTOR of 16 x 16 cells, `lodp_X_Y.lodp`, X = floor(x / 65536),
 *  written only when the sector holds at least one patch. Little-endian,
 *  fixed strides, every table at a fixed place after the one before it, every
 *  reserved byte zero and refused when it is not. CRC-32 is zlib's
 *  (lodvCrc32): `headerCrc32` over header bytes 0x10..0xFF, `indexCrc32` over
 *  every table in file order.
 *
 *  THE DRAW BINDING (how a far pixel finds its patch, no search):
 *   1. the pixel's `.lodi` instance -> the instance table row (sorted by
 *      `.lodi` index) -> `firstChart`;
 *   2. the triangle's chart id from the `.lodo` v8 CHART STREAM (one byte a
 *      cluster triangle, src/lodofile.h) -> chart row `firstChart + id`;
 *   3. the pixel's MESH-LOCAL position into the chart's frame -> slot
 *      (i, j, k) = floor(((p - origin) . U | V | N) / step), clamped ->
 *      `slots[firstSlot + (k * nv + j) * nu + i]` -> the patch;
 *   4. a slot that holds LODP_NONE (only a coarser level's triangle can land
 *      in one) takes the chart's `fallbackPatch`.
 *   Terrain patches need none of that: a land cell's first G x G patches are
 *   its terrain grid, row-major from the cell's SOUTH-WEST corner.
 *
 *  A chart is a set of a mesh's level-0 triangles whose face normals lie
 *  within 45 degrees of the chart's first (seed) triangle -- the plan's "never
 *  across a 45 degree crease" -- so a patch never folds round a corner. */

//! First four bytes, little-endian: `L`,`O`,`D`,`P`.
constexpr quint32 LODP_MAGIC = 0x50444F4CU;
constexpr quint32 LODP_VERSION = 1;
constexpr quint32 LODP_HEADER_BYTES = 256;
constexpr int LODP_SECTOR_CELLS = 16;
constexpr int LODP_SECTOR_CELL_COUNT = LODP_SECTOR_CELLS * LODP_SECTOR_CELLS;
constexpr float LODP_CELL_UNITS = 4096.0f;
constexpr float LODP_SECTOR_UNITS = LODP_CELL_UNITS * LODP_SECTOR_CELLS;   //!< 65,536
constexpr int LODP_AZIMUTHS = 16;
constexpr quint32 LODP_NONE = 0xFFFFFFFFU;
//! Patch position: i16 in steps of 1/2 unit, x/y from the cell's south-west corner, z from the cell's zBase.
constexpr float LODP_POS_STEP = 0.5f;
//! Patch area: f16 in units of this many square units (a 512 u patch is 64). 4096 so one tree patch
//! holds the largest authored LOD tree (10.8M u^2 of cards) at scale 5; 256 overflowed at scale 1.25.
constexpr float LODP_AREA_UNIT = 4096.0f;
//! The crease rule: a triangle joins a chart when its normal is within 45 degrees of the seed's.
constexpr double LODP_CREASE_COS = 0.70710678118654752440;
/*! An exact 45-degree bevel (common in authored LOD) lands within one rounding step of the
 *  threshold, so two correct implementations could split it differently. The join test is
 *  `dot >= LODP_CREASE_COS - LODP_CREASE_TIE`: an exact 45 always joins. */
constexpr double LODP_CREASE_TIE = 1e-9;

//! Defaults (section 1: 4-8 m; L1's census picked 512 u = 7.3 m, terrain 8 x 8 a cell).
constexpr float LODP_PATCH_UNITS_DEFAULT = 512.0f;
constexpr int LODP_TERRAIN_GRID_DEFAULT = 8;

//! Chart ids in the `.lodo` chart stream: 0..LODP_CHART_MAX are charts, then the two specials.
constexpr quint8 LODP_CHART_MAX = 0xFC;           //!< at most 253 charts a mesh, refused above
constexpr quint8 LODP_CHART_FOLIAGE = 0xFE;       //!< an alpha-tested tree material: the instance's tree patch
constexpr quint8 LODP_CHART_NONE = 0xFF;          //!< degenerate, or past the cluster's triangleCount

enum LodpHeaderFlags
{
	/*! The charts were checked, triangle for triangle, against the `.lodo`'s
	 *  v8 chart stream and agreed. Clear = the `.lodo` carried no stream (a
	 *  census run): the file is placed right but nothing on the draw side can
	 *  bind to it. */
	LODP_FLAG_BOUND = 1
};
constexpr quint32 LODP_FLAGS_KNOWN = LODP_FLAG_BOUND;

enum LodpCellFlags
{
	LODP_CELL_TERRAIN = 1,      //!< the cell's first G x G patches are its terrain grid
	LODP_CELL_QUEST_LAYER = 2   //!< PROPOSED P1 (needs bungo's yes): refused in v1
};
constexpr quint16 LODP_CELL_FLAGS_KNOWN = LODP_CELL_TERRAIN;

enum LodpPatchKind { LODP_KIND_TERRAIN = 0, LODP_KIND_OBJECT = 1, LODP_KIND_WATER = 2, LODP_KIND_TREE = 3 };

enum LodpPatchFlags
{
	LODP_PATCH_BURIED = 1,      //!< L2: every sample point failed to reach open air
	LODP_PATCH_SUBMERGED = 2    //!< terrain below its cell's water plane (kept, flagged: section 1 gives water itself no patches)
};
constexpr quint8 LODP_PATCH_FLAGS_KNOWN = LODP_PATCH_BURIED | LODP_PATCH_SUBMERGED;

enum LodpChartKind { LODP_CHART_KIND_SURFACE = 0, LODP_CHART_KIND_TREE = 1 };

#pragma pack( push, 1 )

//! Cell row, 32 bytes, 256 a sector, row-major from the sector's south-west cell: index = ly * 16 + lx.
struct LodpCell
{
	quint32 firstPatch;
	quint32 patchCount;
	quint32 firstChart;
	quint32 chartCount;
	quint64 inputHash;      //!< L6's incremental key (the full-detail 5 x 5 around the cell); 0 in L1
	float zBase;            //!< the z every patch of this cell is stored from
	quint16 flags;          //!< LodpCellFlags
	quint16 reserved;       //!< 0
};

//! Instance row, 16 bytes, sorted by `lodiIndex` ascending: one row a `.lodi` placement that owns charts.
struct LodpInstance
{
	quint32 lodiIndex;      //!< index into the `.lodi` instance table
	quint32 firstChart;     //!< sector-local chart index
	quint16 chartCount;     //!< the mesh's chart count, plus one for a tree patch
	quint16 reserved;       //!< 0
	quint32 lodoMesh;       //!< the `.lodo` mesh the charts were cut from (the base's first present rep)
};

//! Chart row, 64 bytes: one chart of one placement, its slot grid in the MESH's own frame.
struct LodpChart
{
	quint32 firstSlot;      //!< into the slot table
	quint16 nu, nv, nw;     //!< slot grid; nu * nv * nw slots
	quint8 meshChart;       //!< the id the `.lodo` chart stream carries (LODP_CHART_FOLIAGE for a tree)
	quint8 kind;            //!< LodpChartKind
	float step;             //!< slot edge in MESH units = patch units / the placement's scale
	float origin[3];        //!< mesh-local
	float axisU[3];         //!< unit, mesh-local; V = N x U
	float axisN[3];         //!< unit, the chart's area-weighted normal
	quint32 instance;       //!< sector-local instance row
	quint32 fallbackPatch;  //!< the chart's largest patch, for a slot that holds none
	quint32 reserved;       //!< 0
};

//! Patch row, 64 bytes (PLAN.md section 3).
struct LodpPatch
{
	qint16 pos[3];          //!< x, y from the cell's south-west corner, z from zBase; LODP_POS_STEP units
	quint16 normal;         //!< octahedral 8:8, low byte x
	quint8 albedo[3];       //!< L2
	quint8 flags;           //!< LodpPatchFlags
	quint16 area;           //!< f16, LODP_AREA_UNIT square units: the surface area the patch stands for
	quint8 sampleCount;     //!< L2
	quint8 kind;            //!< LodpPatchKind
	quint8 horizon[LODP_AZIMUTHS];      //!< L2
	quint8 openness[LODP_AZIMUTHS];     //!< L2
	quint32 firstLink;      //!< L2
	quint16 linkCount;      //!< L2
	quint16 push;           //!< L2, census
	quint32 lodSource;      //!< object/tree: the sector-local chart; terrain: j * G + i
	quint32 reserved;       //!< 0
};

//! Link row, 8 bytes (L2 writes them; L1 writes none).
struct LodpLink
{
	quint32 target;
	quint16 formFactor;     //!< f16
	quint16 flags;
};

#pragma pack( pop )

static_assert( sizeof( LodpCell ) == 32, "LodpCell is 32 bytes" );
static_assert( sizeof( LodpInstance ) == 16, "LodpInstance is 16 bytes" );
static_assert( sizeof( LodpChart ) == 64, "LodpChart is 64 bytes" );
static_assert( sizeof( LodpPatch ) == 64, "LodpPatch is 64 bytes" );
static_assert( sizeof( LodpLink ) == 8, "LodpLink is 8 bytes" );

//! One sector file in memory: what the writer takes and the reader gives.
struct LodpSector
{
	quint32 flags = 0;              //!< LodpHeaderFlags
	quint64 worldspaceHash = 0;     //!< lodoFnv1a64 of the worldspace EDID's UTF-8
	quint64 lodInputHash = 0;       //!< the LOD bytes it was placed on (lodpInputHash)
	quint64 lodoIdentity = 0;       //!< the `.lodo` it pairs with (its lodoIdentity)
	quint64 optionsHash = 0;        //!< lodpOptionsHash
	qint32 sectorX = 0, sectorY = 0;
	float patchUnits = LODP_PATCH_UNITS_DEFAULT;
	quint8 terrainGrid = LODP_TERRAIN_GRID_DEFAULT;
	std::vector<LodpCell> cells;    //!< exactly 256
	std::vector<LodpInstance> instances;
	std::vector<LodpChart> charts;
	std::vector<quint32> slotIds;
	std::vector<LodpPatch> patches;
	std::vector<LodpLink> links;
};

//! The header as decoded, for describe and the gates.
struct LodpHeader
{
	quint32 version = 0;
	quint32 flags = 0;
	quint32 headerCrc32 = 0;
	quint32 indexCrc32 = 0;
	quint64 fileBytes = 0;
};

/* ---- the packing contract, shared by the writer, the reader and the placer ---- */

//! Unit normal -> octahedral 8:8 (low byte x).
quint16 lodpPackOct8( const float n[3] );
void lodpUnpackOct8( quint16 packed, float n[3] );
//! IEEE half, round to nearest even; the reader's inverse.
quint16 lodpHalfFromFloat( float f );
float lodpFloatFromHalf( quint16 h );
//! floor(v / 65536) and the cell's index inside its sector.
int lodpSectorOf( int cell );
int lodpCellIndex( int cellX, int cellY, int sectorX, int sectorY );
//! `lodp_X_Y.lodp`
QString lodpFileName( int sectorX, int sectorY );
//! The options a bake was placed with, hashed (patch units, terrain grid, crease, version).
quint64 lodpOptionsHash( float patchUnits, int terrainGrid );

/* ---- file I/O ---- */

/*! Writes one sector file. Refuses (false + `error`) a table that breaks a
 *  rule the reader enforces, so a file it writes is one the reader accepts. */
bool lodpWrite( const QString & path, const LodpSector & s, LodpHeader * headerOut, QString * error );
/*! Reads one sector file and checks every rule of the contract: magic, version
 *  (only 1), both CRCs, every reserved byte, the table layout, every index in
 *  range, every count consistent, slot targets inside their chart's cell. */
bool lodpRead( const QString & path, LodpHeader * header, LodpSector * s, QString * error );
QStringList lodpDescribe( const LodpHeader & h, const LodpSector & s );

#endif
