#ifndef PROBEVOLUMES_H
#define PROBEVOLUMES_H

/*! THE SPACE STAGE: ROOMS, THRESHOLDS AND THEIR SHAPES (lane VOLFIT1, 2026-10-19; recx draft VOLUMES.md sections 4-6).
 *
 *  A shared base for GI, fog, sound, navmesh and previs: every enclosed space of a cell is a room made of joined
 *  primitives (box, cylinder, wedge, capsule, sphere); everything else is the one outdoor volume ("outside"); rooms
 *  meet each other and the outside only at stored thresholds.
 *    1. geometry first: the ROOMCLAMP1 grid (proberooms.h) with its working grids kept: solid/glass, the closing by
 *       the exact distance transform (pinch = half a door width), labels, one-sided facing
 *    2. the outdoor air that is roofed AND floored gets a sky share (cosine-weighted, 32 rays) and a wall share (16
 *       level rays within wallReach) by a DDA through a 2x-coarse occupancy grid (no triangle is traced); a part of
 *       it with sky < skyMax and wall >= wallMin is a candidate; a connected candidate becomes a room when, as a
 *       whole, its open boundary share is under openMax and its seen-from-behind share under backMax
 *    3. an enclosed room seen mostly from behind (inside a rock) is no room; a room under minVolume folds into the
 *       neighbor it shares most faces with (sealed: dropped); a roofed room grows back up to growSteps cells to
 *       its wall line (the sky rule leaves its edge short of an opening)
 *    4. thresholds: the faces between two labels and the glass between them, clustered, each an oriented rectangle
 *       with its opening type (opening, door, window, portal) and evidence (door boxes, PortalMarkers, the placer's
 *       apertures); RoomMarkers are evidence for rooms
 *    5. shapes: in the room's footprint frame (minimal-area rectangle), a greedy cover by boxes (each grown while
 *       its next slab is under 3 % other air and under half solid) with wedges competing for roofs; then one
 *       cylinder / sphere / capsule / wedge competes for the whole room (fewer shapes win, boxes win ties).
 *       Measured on the grid: coverage, overshoot into other air, overshoot into solid
 *    6. hand fixes: a .recx room's own "shapes" replace the fitted ones (matched by its anchor)
 *  Refuters (ProbeVolumeSpec::red, WW_CELL_VOLUMES_RED / --red): nosky, nowall, noopen, noback, nofloor, nogrow,
 *  nofold, nomerge, onebox, boxonly, noglass, nowedge, noevidence, noids, norecx; the stage record: fpsoup, noskip; the GI map (probevolumesgi.h): nogimap, gione, spaceshapes, spaceleftover
 *  (probeVolumesCheck). */

#include "probeplace.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

//! A Bethesda previs marker read from the plugin (cell view): evidence only, never required.
struct ProbeVolumeMarker
{
	int type = 1;               //!< 1 RoomMarker, 2 PortalMarker
	float c[3] = { 0, 0, 0 };
	float ax[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	float half[3] = { 0, 0, 0 };
	quint32 ref = 0, from = 0, to = 0;
};

//! A placer opening (probeplace.h ProbePoint, class Aperture): evidence for thresholds.
struct ProbeVolumeAperture
{
	int type = 0;               //!< 1 Doorway, 2 Window, 3 Breach
	float pos[3] = { 0, 0, 0 };
	float width = 0, height = 0;
	quint32 doorRef = 0;
};

struct ProbeVolumeSpec
{
	float cell = 16.0f;
	int maxCells = 24000000;    //!< a 3x3 exterior block at about 27 units
	float pinch = 64.0f;        //!< the closing: half a door width (doors 128 wide close)
	float clipLo[3] = { 1.0f, 1.0f, 1.0f }, clipHi[3] = { 0.0f, 0.0f, 0.0f };   //!< as ProbeRoomSpec (x y only)
	float skyMax = 0.25f;       //!< THE look question: roofed air under this sky share may be indoors
	float wallMin = 0.5f;
	float openMax = 0.25f;
	float backMax = 0.5f;
	float wallReach = 1536.0f;
	float minVolume = 1.5e6f;   //!< about a 115-unit cube
	int growSteps = 6;
	//! lane VOLFIT1: roofed air sharing at least this share of its faces with a room is that room's (ROOMCLAMP1's
	//! watershed hands a room's floor layer by a doorway to the outdoor basin); a doorway between rooms is far below
	float mergeShare = 0.2f;
	float coverTarget = 0.92f;
	float slabOther = 0.03f;
	int maxShapes = 16;
	float borderBand = 0.05f;   //!< a room within this of skyMax / openMax is listed borderline
	QString red;
};

enum class ProbeShapeType : int { Box = 0, Cylinder = 1, Wedge = 2, Capsule = 3, Sphere = 4 };

struct ProbeVolumeShape
{
	ProbeShapeType type = ProbeShapeType::Box;
	float center[3] = { 0, 0, 0 };
	float size[3] = { 0, 0, 0 };
	float rotation[3] = { 0, 0, 0 };   //!< degrees: R = Rz Ry Rx (world axes, x first)
	bool contains( const float p[3] ) const;
	double volume() const;
};

struct ProbeVolumeRoom
{
	quint32 id = 0;             //!< per cell file, kept across bakes by the anchor
	QString cellKey;            //!< the cell whose file holds it (the anchor's)
	QString source;             //!< enclosed | roofed | sealed (+recx) | recx
	float anchor[3] = { 0, 0, 0 };
	std::vector<ProbeVolumeShape> shapes;
	bool handShapes = false;
	qint64 cells = 0;
	float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
	float sky = 0, wall = 0, openness = 0, back = 0;
	bool borderline = false;
	float coverage = 0, overshootAir = 0, overshootSolid = 0;
	QStringList evidence;
};

struct ProbeVolumeThreshold
{
	quint32 id = 0;
	int a = 0, b = 0;           //!< room indexes (1-based into rooms; 0 = outside)
	QString opening;            //!< opening | door | window | portal
	ProbeVolumeShape shape;     //!< a box: x along the opening, y across (the normal), z up the opening
	float normal[3] = { 0, 0, 0 };   //!< from a toward b
	float area = 0;
	QStringList evidence;
};

struct ProbeVolumes
{
	bool ok = false;
	QString error;
	float origin[3] = { 0, 0, 0 };
	float cell = 16.0f;
	int dims[3] = { 0, 0, 0 };
	std::vector<qint32> labels;  //!< per grid cell: -1 solid / void, 0 outside, k = rooms[k - 1]
	std::vector<ProbeVolumeRoom> rooms;
	std::vector<ProbeVolumeThreshold> thresholds;
	// census
	qint64 skyCells = 0, skyCoarse = 0, candidateCells = 0;
	int candidates = 0, roofedRooms = 0, rejectedOpen = 0, rejectedSky = 0, rejectedBack = 0, rejectedSmall = 0;
	int enclosed = 0, insideGeometry = 0, folded = 0, dropped = 0, grownCells = 0, tinyThresholds = 0;
	int merged = 0;             //!< roofed components merged into the room they mostly touch
	int handFixed = 0, idsKept = 0, idsNew = 0;
	double doorWidth = 0;       //!< the median door box's narrow side (the pinch's check)
	double msRooms = 0, msSky = 0, msClassify = 0, msThresholds = 0, msFit = 0, msTotal = 0;
	qint64 refineRays = 0;      //!< rays the fit's refine cast onto the triangles (lane VOLRAY1)
	qint64 fineRays = 0;        //!< rays of the fine lattice fit (lane VOLFIT2)
	int fineBoxes = 0;
	qint64 fineGrown = 0;       //!< fine points grown past the owned cells by the indoor rule
	qint64 finePruned = 0;      //!< fine boxes dropped (the others covered them within minGain)
	qint64 fineLost = 0;        //!< room points the check flood missed
	double msFine = 0;
	QString fineNote;           //!< lane VOLFIT3: the per-room picks (grid / moved), extras, faces moved, merges
	QString roomsLine;

	size_t index( int x, int y, int z ) const
	{
		return ( size_t( z ) * size_t( dims[1] ) + size_t( y ) ) * size_t( dims[0] ) + size_t( x );
	}
	//! the grid's label at p (-2 outside the grid)
	int gridAt( const float p[3] ) const;
	//! the room by shapes (what a runtime reads): the smallest room whose shapes hold p; 0 = outside
	int roomAt( const float p[3] ) const;
};

bool probeVolumesBuild( const ProbeSoup & soup, const std::vector<ProbeVolumeMarker> & markers,
	const std::vector<ProbeVolumeAperture> & apertures, const ProbeVolumeSpec & spec, ProbeVolumes * out );
//! The rooms' cell keys: an exterior room goes to the cell holding its anchor ("X_Y"); an interior's to `key`.
void probeVolumesAssignCells( ProbeVolumes * v, bool interior, const QString & key );
//! Hand fixes: each room of the .recx entry named `name` with an anchor and "shapes" replaces the fitted shapes
//! of the room holding its anchor (or adds a room). Returns the log lines.
QStringList probeVolumesApplyRecx( ProbeVolumes * v, const QString & recxPath, const QString & name );
//! Keeps room ids: each room of a previous file (same cell) gives its id to the room now holding its anchor.
QStringList probeVolumesKeepIds( ProbeVolumes * v, const QString & cellKey, const ProbeVolumes & prev );
QString probeVolumesCensusText( const ProbeVolumes & v );

//! One cell's file `<key>.space` (JSON): the rooms whose cellKey is `key` and the thresholds touching them.
QByteArray probeVolumesJson( const ProbeVolumes & v, const QString & key, const QString & world, bool interior );
//! Reads a .space file (rooms with shapes, thresholds; no grid). For PERF1 and the id keep.
bool probeVolumesRead( const QString & path, ProbeVolumes * out, QString * error );

/*! The stage record `<key>.space.manifest` (BAKEINCR1's style, folded later into `<key>.bakemanifest` by BAKEUI1):
 *    stage 3 space
 *    identity <sha1 of the stage version + spec>
 *    input soup <sha1>        the triangles, glass, door boxes, masks, two-sided flags
 *    input markers <sha1>
 *    input apertures <sha1>
 *    input recx <sha1 | none>
 *    output <file> <sha1>
 *  The check stops at the first reason: never built, unreadable, identity, input <name>, output <file>. */
struct ProbeVolumesInputs
{
	QByteArray identity, soup, markers, apertures, recx;
};
ProbeVolumesInputs probeVolumesInputs( const ProbeSoup & soup, const std::vector<ProbeVolumeMarker> & markers,
	const std::vector<ProbeVolumeAperture> & apertures, const ProbeVolumeSpec & spec, const QString & recxPath );
//! "" = clean (the stage may be skipped), else the first dirty reason
QString probeVolumesCheck( const QString & dir, const QString & key, const ProbeVolumesInputs & in, const QString & red );
bool probeVolumesWriteCell( const QString & dir, const QString & key, const QByteArray & json, const ProbeVolumesInputs & in,
	QString * error );

//! The cell folder: `<root>/<World>/<X>_<Y>` or `<root>/Interiors/<EDID>`.
QString probeVolumesDir( const QString & root, const QString & world, bool interior, const QString & key );

//! A marker list file (one per line: type ref from to c[3] ax[9] half[3]); the cell view writes it beside a soup dump.
bool probeVolumesMarkersWrite( const QString & path, const std::vector<ProbeVolumeMarker> & m );
bool probeVolumesMarkersRead( const QString & path, std::vector<ProbeVolumeMarker> * m );

//! One Space stage run over one or more cells (the CLI and the cell view share it).
struct ProbeVolumesStageSpec
{
	QString root, world;        //!< bake root; worldspace EDID ("" + !interior + flat = `<root>/<key>`)
	bool interior = false;
	bool flat = false;          //!< files straight under root (tests)
	bool byAnchor = false;      //!< exterior block: each room goes to its anchor's X_Y cell
	ProbeVolumeSpec spec;
	QString recxFile, recxName; //!< recxName "" = each cell's own record
	bool grid = false;          //!< also write `<key>.spacegrid`
	QStringList keys;           //!< the cells written
};
/*! Checks every cell's record; all clean = "space: skipped (clean)" and `out` read back from the files; else rebuilds,
 *  applies the recx, keeps every cell's ids, writes. Returns the log lines; *ok false on a build or write error. */
QString probeVolumesStage( const ProbeSoup & soup, const std::vector<ProbeVolumeMarker> & markers,
	const std::vector<ProbeVolumeAperture> & apertures, const ProbeVolumesStageSpec & st, ProbeVolumes * out, bool * ok );

/*! lane ROOMFIT9: the in-cell stage's other inputs (apertures, clip, keys, world, red, recx) as `<soup>.spacein` beside
 *  the soup dump; `volumes --like-cell <file>` reads them back, so the CLI rebuilds the in-cell files exactly. */
bool probeVolumesStageInWrite( const QString & path, const std::vector<ProbeVolumeAperture> & aps, const ProbeVolumesStageSpec & st );
bool probeVolumesStageInRead( const QString & path, std::vector<ProbeVolumeAperture> * aps, ProbeVolumesStageSpec * st );

/*! `nifskope -no-gui volumes --soup <psp> --out <dir> [--key K] [--world W] [--interior] [--markers f] [--recx f]
 *  [--rect x0,y0,x1,y1 (clip, and the placer's apertures as evidence)] [--skymax s] [--red r] [--grid] [--view-dump f]`:
 *  builds, writes `<dir>/<key>.space` + `.space.manifest` (+ `<key>.spacegrid` with --grid: float origin[3], cell;
 *  int dims[3]; int32 label per cell), or prints "space: skipped (clean)" when the record matches.
 *  `--gi-map f`: the rooms mapped onto a GI room grid of the same soup (probevolumesgi.h), written as a TSV.
 *  `--like-cell <soup>.spacein`: the in-cell stage's own clip, keys, apertures and red (markers default `<soup>.markers`). */
int probeVolumesCli( const QStringList & args );

#endif // PROBEVOLUMES_H
