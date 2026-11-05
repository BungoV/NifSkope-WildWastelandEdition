#ifndef PROBEGI_H
#define PROBEGI_H

//! THE BAKE RELIT BY THE CELL'S LIGHTS:
//! bungo 2026-10-01: "simulate the GI for me with lights ... based on the surfels and sky visibility
//! generated".

#include "features/gi/probeplace.h"
#include "features/lights/lightphys.h"
#include "features/gi/proberooms.h"
#include "features/gi/probesky.h"
#include "features/gi/probegifine.h"
#include "gl/celllights.h"
#include <memory>

#include <QString>
#include <QStringList>

#include <vector>

struct WwCellLighting;
struct AoDecalSet;	// src/cellaodecal.h
struct ProbeVolumes;	// src/features/gi/probevolumes.h

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
	int pad = 5;                        //!< edge voxels each side past the probes' reach (kPad); the shader fades x/y over them
	float radius = 0.0f;                //!< the probe blend radius
	int voxelsNear = 0, voxelsValid = 0;
	qint64 visRays = 0, visBlocked = 0;
	//! near voxels no probe was seen from at their centre (the centre inside a wall or behind it), filled from
	//! their eye instead -- the surface sample point nearest the centre: within the radius (voxelsEye), within
	//! twice it (voxelsFar); the mean of valid neighbours, two rings (voxelsGrown); still none (voxelsBare).
	int voxelsEye = 0, voxelsFar = 0, voxelsGrown = 0, voxelsBare = 0;
	/*! outdoor (room 0) slots still empty after the grow rings (past the probed rect, where no outdoor
	 *  probe stands), lit by the sky their eye sees (8 octants x 32 rays): slot 0 / slot 1, the mean sky share, and
	 *  "on" / "red noedge" / "recording: left empty" */
	int edgeSky0 = 0, edgeSky1 = 0;
	double edgeSkyShare = 0.0;
	QString edgeSkyNote;
	/*! the own-sky floor. An outdoor (room 0) slot the probes did fill still reads at least the sky its
	 *  eye sees, per axis (the grid blends only the probes of its room, and a voxel just inside a shop's open front
	 *  holds the dark interior probes while its eye sees the street's sky): slots looked at, slots raised, axes raised,
	 *  and "on" / "red noskyfloor" / "recording: left as blended" */
	int skyFloorSlots = 0, skyFloorRaised = 0, skyFloorAxes = 0;
	QString skyFloorNote;
	//! the edge/floor sky rays' tracer (bakert census: path, device, re-casts, GPU ms) and their ms
	QString edgeRaysNote;
	double msEdgeRays = 0;
	/*! the rooms. Each near voxel keeps two slots, the two rooms the surfaces
	 *  reading it stand in most; a slot gathers only the probes of its room, from the voxel's centre when the
	 *  centre is in that room, else from the slot's eye. grid / gridSky then hold slot 0 and grid2 / gridSky2
	 *  slot 1 (the same layout); slotRooms = probeRoomsPack( slot 0 room, slot 1 room ) per voxel (-1: none). Red
	 *  "noclamp": no rooms, one value a voxel (the blend before the lane). */
	bool roomsOn = false;
	ProbeRooms rooms;
	GiVolumeSet volumes;                //!< the GI volumes (the fitter's); empty: the rooms grid stand-in
	QString spaceRooms;                 //!< the Space relabel's census line (empty: not relabeled)
	std::vector<float> grid2, gridSky2, slotRooms;
	std::vector<int> probeRooms;        //!< per probe: its room, its second room (-1 none)
	int voxelsTwoRooms = 0, voxelsTwoBare = 0, voxelsCentreElsewhere = 0, probesRoomless = 0;
	/*! six slabs (+X -X +Y -Y +Z -Z), each dims[2] deep, x fastest: (r, g, b, valid) a voxel; rgb is
	 *  E x valid, so a filtered sample divides by its own valid (the renderer's 3D texture as is) */
	std::vector<float> grid;
	/*! with decal copies in the spec, the same grid (and grid2) built from the
	 *  probes with the copies taken out of their sky (E + dE, design section 4); empty without copies */
	std::vector<float> gridFree, gridFree2;
	int aoProbes = 0;                   //!< probes a copy's footprint reached
	double aoDsMean = 0.0;              //!< their mean dE (all axes and channels)
	QString aoGate;                     //!< WW_CELL_AODECAL_GATE's line
	std::vector<float> probeCube;       //!< per probe: pos[3] + 6 x rgb
	std::vector<float> surfelOut;       //!< per unique surfel: pos[3] nrm[3] albedo[3] B[3]
	// the Pass drop-down's data
	float surfelCell = 32.0f;           //!< the first file's surfel cell size
	std::vector<float> gridSky;         //!< the grid's layout, rgb = the blended open-sky share, a = valid
	std::vector<float> probeSky;        //!< per probe: the sky share on +X -X +Y -Y +Z -Z (mean of its 4 octants)
	std::vector<int> probeLinkStart;    //!< probe i links surfels probeLinks[start[i] .. start[i + 1])
	std::vector<int> probeLinks;        //!< surfelOut indices, one per resolved link
	double msLight = 0, msGather = 0, msGrid = 0;
	// the sky and the sun of a weather-lit exterior
	bool skyLit = false;                //!< the relight took them (the grid's sky then stands in for the weather's ambient)
	QString skyLabel;
	int surfelsSun = 0;                 //!< surfels the sun reaches
	int surfelsEmit = 0;                //!< surfels that glow (the .tbk emissive tail)
	qint64 sunRays = 0, sunBlocked = 0;
	int probesSky = 0;                  //!< probes that see any sky
	double skyVisMean = 0;              //!< over probes: the mean of the eight octants' sky share
	int probesTinted = 0;               //!< probes with a tinted octant (sky seen through glass)
	std::vector<float> probeSkyE;       //!< per probe: 6 x rgb, the sky's part of probeCube
	std::vector<float> surfelSun;       //!< per unique surfel: rgb, the sun's part of B
	// the passes (surfelOut keeps pass 1's B; probeCube and the grid are the last pass's)
	int passes = 1;                     //!< passes run (1 = one bounce)
	bool settled = true;                //!< the last pass changed no surfel by more than the bar
	int surfelsFed = 0, fedClosest = 0; //!< surfels that read a probe; of them, by the closest-probe fallback
	int surfelsRoomed = 0;              //!< surfels whose room the room boxes name
	qint64 feedRays = 0, feedBlocked = 0, feedOtherRoom = 0;
	double gain = 1.0;                  //!< sum of B, last pass over pass 1
	std::vector<double> passLog;        //!< per pass: largest change of a surfel's B, sum of B, largest B
	std::vector<float> surfelBounce;    //!< per unique surfel: rgb, the last pass's B
	std::vector<int> feedStart;         //!< surfel i reads feedProbe/feedWeight[start[i] .. start[i + 1])
	std::vector<int> feedProbe;
	std::vector<float> feedWeight;
	std::vector<int> surfelRoom;        //!< per unique surfel: its room, -1 = not known
	double msBounce = 0;
	//! the surfels grouped into bricks of spec.brickEdge units (4 m), one brick per cell and facing (the
	//! dominant axis of the normal, with its sign); a brick's light is the mean of its surfels' B; each probe
	//! keeps, per brick its links reach, six weights (sum of the links' omega x max(axis .
	bool bricksGather = false;          //!< the probes' cubes (and so the grid) come from the bricks
	int bricks = 0, brickFactors = 0, brickLinks = 0;
	float brickEdge = 0.0f;
	double brickRel = 0.0;              //!< sum |E_brick - E_link| / sum E_link, the last pass's B, every probe
	double brickP95 = 0.0;              //!< the per-probe 95th percentile of the same ratio (lit probes)
	double msBricks = 0;
	std::vector<qint32> brickKeys;      //!< per brick: cell x, y, z, facing (0..5 = +X -X +Y -Y +Z -Z), surfel count
	std::vector<float> brickLight;      //!< per brick: rgb, the mean of its surfels' last-pass B
	std::vector<float> brickCube;       //!< per probe: probeCube's layout, gathered from the bricks (last pass)
	std::vector<float> linkCube;        //!< per probe: the same, gathered from the links
	std::vector<float> surfelBrick;     //!< per unique surfel: rgb, its brick's mean of pass 1's B (the Pass view)
	/*! SKY2 FOGSLICE1 (the deck's air volume, D2 s55 / D9 s61): over the grid's box at 3/4 of the probe spacing, every
	 *  voxel the probes' mean irradiance (the grid's layout: six slabs, rgba, a = 1), for the air; airNote = its census */
	std::vector<float> air;
	float airOrigin[3] = { 0, 0, 0 };
	float airVoxel = 0.0f;
	int airDims[3] = { 0, 0, 0 };
	QString airNote;
	bool fineOn = false;                //!< spec.emissiveGrid asked for the fine boxes
	ProbeGiFine fine;                   //!< src/features/gi/probegifine.h
	//! Probe sharing: entries kept, segments tested, of them blocked, other-room refusals
	bool shareOn = false;
	qint64 shareEntries = 0, shareRays = 0, shareBlocked = 0, shareOtherRoom = 0;
	double shareNuMean = 0, shareMoved = 0, shareReach = 0, msShare = 0;
	std::vector<int> shareStart, shareProbe;   //!< per probe its kept neighbours (the dump's gi_share.bin)
	std::vector<float> shareG, shareNu;
	int sharePasses = 1;
	//! depth visibility: weights tested, of them cut (< 1), the mean factor over the tested, its build ms
	bool depthVisOn = false;
	qint64 depthVisTested = 0, depthVisCut = 0;
	double depthVisMean = 0, msDepthVis = 0;
};

struct ProbeGiSpec
{
	float fixtureClear = 24.0f;     //!< units before the light a shadow ray does not test
	float voxelMin = 48.0f;         //!< the grid's voxel, raised until it fits maxVoxels
	int maxVoxels = 400000;
	float radiusScale = 2.0f;       //!< blend radius = this x the median probe-to-nearest-probe distance
	/*! deliberate defects for the gate's refuters: "noshadow" (no shadow rays), "novis" (the grid
	 *  ignores visibility), "flip" (links gathered from the opposite direction), "noeye" (
	 *  a voxel blocked at its centre stays empty), "noclamp". Empty in real runs. */
	QString red;
	ProbeRoomSpec rooms;            //!< the room labels (its red: WW_CELL_ROOMS_RED)
	//! the Space stage's rooms (WW_CELL_VOLUMES_GI=1) relabel the room grid (probevolumesgi.h); null: ROOMCLAMP1's
	//! owned: the sky keeps copies of this spec past the load (probeSkyKeep), so no pointer into a load local
	std::shared_ptr<const ProbeVolumes> space;
	/*! the weather light of an exterior (off = none: an interior, or a view the weather
	 *  does not light), and its refuters (WW_CELL_SKY_RED): "off" neither sky nor sun, "novis" every
	 *  octant sees the sky, "notint" the glass tint is ignored, "sunthrough" the sun has no shadow ray. */
	ProbeSkyLight sky;
	QString skyRed;
	/*! an interior whose cell shows the sky (CELL DATA bit 7): its sky shares take the weather's
	 *  sky like an exterior's; the sun only with Use Sky Lighting + Sunlight Shadows (bits 8 and 11). */
	bool interiorSky = false;
	bool interiorSun = false;
	/*! 0 = repeat until settled (at most maxPasses), n = exactly n passes (1 = one bounce; the gates
	 *  that measure one bounce pin it, WW_CELL_GI_PASSES). Refuters in `red`: "rooms" (a surfel reads every probe
	 *  in the radius, walls and rooms ignored), "grow" (the feedback's albedo is 1.5: gain above one). */
	int passes = 0;
	int maxPasses = 64;
	double settle = 1e-3;
	//! the glowing surfels left dark (the placed-only relight the far light takes out of the GI)
	bool noGlow = false;
	//! the light physics rows' falloff and intensity curve (wwLightPhysParams); default = the game's
	wwlp::Params lightPhys;
	//! set, the relight also records its operators there (the pairs, links, feed and blend lists, each entry's
	//! door crossings re-traced against the doors' real geometry) for the relight without rays.
	struct ProbeRelightOps * record = nullptr;
	QVector<WwCellLight> recordExtra;
	std::vector<quint32> recordRef;
	std::vector<quint64> recordGroup;
	std::vector<quint16> recordFlags;
	//! the placed decal copies (null: none; the result is then byte for byte as before)
	std::shared_ptr<const AoDecalSet> aoDecals;
	//! the probes gather the deck's 4 m bricks instead of their links (WW_CELL_GI_BRICKS=1; off = the links,
	//! byte for byte as before). brickEdge: 4 m in game units (1 unit = 1.428 cm).
	bool bricks = false;
	float brickEdge = 280.0f;
	QString brickRed;
	//! the fine boxes round bright glow (WW_CELL_EMISSIVEGRID / the row); red WW_CELL_EMISSIVEGRID_RED
	bool emissiveGrid = false;
	QString emissiveGridRed;
	/*! Probe sharing (the Show row, WW_CELL_GI_SHARE; off = byte for byte as before): after
	 *  the last gather each probe takes nu x its clear same-room neighbours' light (proberelight.h probeShareNu).
	 *  shareRed (WW_CELL_GI_SHARE_RED): "nowall" skips the room and wall tests (the leak gate's red) */
	bool share = false;
	QString shareRed;
	//! Wide probe sharing: the share step runs this many times with nu kept (1 = one step, as before)
	int sharePasses = 1;
	/*! Probe depth visibility (DDGI, Majercik 2019): each probe keeps an 8x8 octahedral map of the mean and
	 *  mean square distance its links see; a point past the mean takes the Chebyshev bound (cubed, floor 0.05) on its
	 *  blend and feed weight. depthVisRed (WW_CELL_GI_DEPTHVIS_RED): "flat" every texel the probe's mean (no direction) */
	bool depthVis = false;
	QString depthVisRed;
	/*! the "Emittance in bounce light" state the bake took (-1 = no other state kept) and the one
	 *  wanted now; they differ = take emitalt_*.bin. emitStale (red WW_CELL_XEMI_GI_RED=stale) keeps the baked Le. */
	int emitBaked = -1, emitWant = -1;
	bool emitStale = false;
};

//! Relight the bake in `bakeDir` (its sector_*.tbk) with `lighting`, shadowed through `soup`.
bool probeGiRelight( const ProbeSoup & soup, const QString & bakeDir, const WwCellLighting & lighting,
	const ProbeGiSpec & spec, ProbeGiResult * out );
//! after gi.rgba / gi.sky took grid / gridSky, append slot 1 and hand the rooms over (none: no-op)
void probeGiRoomsInto( ProbeGiResult & r, struct WwCellGi & gi );
//! when the relight built the copy-free grids, they take the grids' place (before probeGiRoomsInto)
void probeGiAoFreeSwap( ProbeGiResult & r );
/*! the Space stage's ROOMCLAMP1 grid offered to the relight's memo (kept grids left out); a relight
 *  over the same soup with the same room spec (clip, cell) takes it instead of building it again. Off: WW_GI_ROOMSHARE=0. */
void probeGiRoomsOffer( const ProbeSoup & soup, const ProbeRoomSpec & spec, ProbeRooms & rooms );
//! One census line.
QString probeGiCensusText( const ProbeGiResult & r );
//! The gate's dump: gi_surfels.bin, gi_probes.bin, gi_grid.bin in `dir` (layouts in probegi.cpp).
bool probeGiDump( const ProbeGiResult & r, const ProbeGiSpec & spec, const QString & dir, QString * err );
//! gi_lights.txt in `dir`, the placed lights as the relight lit them (the path-traced reference's input).
bool probeGiDumpLights( const WwCellLighting & lighting, const ProbeGiSpec & spec, const QString & dir, QString * err );
/*! `probegi` places, bakes and relights a dumped soup by the lights given (an interior), then
 *  dumps (the synthetic rooms gate, tests/features/gi/cell_rooms.sh) */
int probeGiCli( const QStringList & args );

#endif // PROBEGI_H
