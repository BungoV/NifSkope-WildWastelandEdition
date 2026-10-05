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
 *  The renderer samples the grid (src/gl/celllights.cpp, unit 13) and adds albedo x E / pi.
 *
 *  Lane BOUNCE2 (2026-10-03, plan 2aj): more than one bounce, the still viewer's twin of the deck's "each surfel
 *  adds its probe's light from the previous frame". Between 2 and 3 the relight repeats: every surfel reads the
 *  probes it can see from its own point (within the blend radius, the step-3 weights, the n^2 blend of the six
 *  axes; none in the radius: the closest it can see within twice it), never a probe behind its wall nor, when
 *  its room is known (the .tbk v4 room boxes), one in another room; B = albedo x (E_direct + E_probes / pi);
 *  the probes gather again. Until the largest change is under a thousandth of the brightest surfel. */

#include "probeplace.h"
#include "proberooms.h"
#include "probesky.h"
#include "gl/celllights.h"
#include <memory>

#include <QString>
#include <QStringList>

#include <vector>

struct WwCellLighting;
struct AoDecalSet;	// lane AODECAL1 (src/cellaodecal.h)

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
	/*! lane ROOMCLAMP1: near voxels no probe was seen from at their centre (the centre inside a wall or behind
	 *  it), filled from their eye instead -- the surface sample point nearest the centre: within the radius
	 *  (voxelsEye), within twice it (voxelsFar); the mean of valid neighbours, two rings (voxelsGrown); still
	 *  none (voxelsBare). Red "noeye": the old hole. */
	int voxelsEye = 0, voxelsFar = 0, voxelsGrown = 0, voxelsBare = 0;
	/*! lane ROOMCLAMP1 (src/proberooms.h): the rooms. Each near voxel keeps two slots, the two rooms the surfaces
	 *  reading it stand in most; a slot gathers only the probes of its room, from the voxel's centre when the
	 *  centre is in that room, else from the slot's eye. grid / gridSky then hold slot 0 and grid2 / gridSky2
	 *  slot 1 (the same layout); slotRooms = probeRoomsPack( slot 0 room, slot 1 room ) per voxel (-1: none). Red
	 *  "noclamp": no rooms, one value a voxel (the blend before the lane). */
	bool roomsOn = false;
	ProbeRooms rooms;
	std::vector<float> grid2, gridSky2, slotRooms;
	std::vector<int> probeRooms;        //!< per probe: its room, its second room (-1 none)
	int voxelsTwoRooms = 0, voxelsTwoBare = 0, voxelsCentreElsewhere = 0, probesRoomless = 0;
	/*! six slabs (+X -X +Y -Y +Z -Z), each dims[2] deep, x fastest: (r, g, b, valid) a voxel; rgb is
	 *  E x valid, so a filtered sample divides by its own valid (the renderer's 3D texture as is) */
	std::vector<float> grid;
	/*! lane AODECAL1 (src/cellaodecal.h): with decal copies in the spec, the same grid (and grid2) built from the
	 *  probes with the copies taken out of their sky (E + dE, design section 4); empty without copies */
	std::vector<float> gridFree, gridFree2;
	int aoProbes = 0;                   //!< probes a copy's footprint reached
	double aoDsMean = 0.0;              //!< their mean dE (all axes and channels)
	QString aoGate;                     //!< WW_CELL_AODECAL_GATE's line
	std::vector<float> probeCube;       //!< per probe: pos[3] + 6 x rgb
	std::vector<float> surfelOut;       //!< per unique surfel: pos[3] nrm[3] albedo[3] B[3]
	// lane PROBEVIEW1: the Pass drop-down's data
	float surfelCell = 32.0f;           //!< the first file's surfel cell size
	std::vector<float> gridSky;         //!< the grid's layout, rgb = the blended open-sky share, a = valid
	std::vector<float> probeSky;        //!< per probe: the sky share on +X -X +Y -Y +Z -Z (mean of its 4 octants)
	std::vector<int> probeLinkStart;    //!< probe i links surfels probeLinks[start[i] .. start[i + 1])
	std::vector<int> probeLinks;        //!< surfelOut indices, one per resolved link
	double msLight = 0, msGather = 0, msGrid = 0;
	// lane SKY1 (src/probesky.h): the sky and the sun of a weather-lit exterior
	bool skyLit = false;                //!< the relight took them (the grid's sky then stands in for the weather's ambient)
	QString skyLabel;
	int surfelsSun = 0;                 //!< surfels the sun reaches
	int surfelsEmit = 0;                //!< lane EMISSIVEGI1: surfels that glow (the .tbk emissive tail)
	qint64 sunRays = 0, sunBlocked = 0;
	int probesSky = 0;                  //!< probes that see any sky
	double skyVisMean = 0;              //!< over probes: the mean of the eight octants' sky share
	int probesTinted = 0;               //!< probes with a tinted octant (sky seen through glass)
	std::vector<float> probeSkyE;       //!< per probe: 6 x rgb, the sky's part of probeCube
	std::vector<float> surfelSun;       //!< per unique surfel: rgb, the sun's part of B
	// lane BOUNCE2: the passes (surfelOut keeps pass 1's B; probeCube and the grid are the last pass's)
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
};

struct ProbeGiSpec
{
	float fixtureClear = 24.0f;     //!< units before the light a shadow ray does not test
	float voxelMin = 48.0f;         //!< the grid's voxel, raised until it fits maxVoxels
	int maxVoxels = 400000;
	float radiusScale = 2.0f;       //!< blend radius = this x the median probe-to-nearest-probe distance
	/*! deliberate defects for the gate's refuters: "noshadow" (no shadow rays), "novis" (the grid
	 *  ignores visibility), "flip" (links gathered from the opposite direction), "noeye" (lane ROOMCLAMP1:
	 *  a voxel blocked at its centre stays empty), "noclamp" (lane ROOMCLAMP1: no rooms). Empty in real runs. */
	QString red;
	ProbeRoomSpec rooms;            //!< lane ROOMCLAMP1: the room labels (its red: WW_CELL_ROOMS_RED)
	/*! lane SKY1: the weather light of an exterior (off = none: an interior, or a view the weather
	 *  does not light), and its refuters (WW_CELL_SKY_RED): "off" neither sky nor sun, "novis" every
	 *  octant sees the sky, "notint" the glass tint is ignored, "sunthrough" the sun has no shadow ray. */
	ProbeSkyLight sky;
	QString skyRed;
	/*! lane SKYINT1: an interior whose cell shows the sky (CELL DATA bit 7): its sky shares take the weather's
	 *  sky like an exterior's; the sun only with Use Sky Lighting + Sunlight Shadows (bits 8 and 11). */
	bool interiorSky = false;
	bool interiorSun = false;
	/*! lane BOUNCE2: 0 = repeat until settled (at most maxPasses), n = exactly n passes (1 = one bounce; the gates
	 *  that measure one bounce pin it, WW_CELL_GI_PASSES). Refuters in `red`: "rooms" (a surfel reads every probe
	 *  in the radius, walls and rooms ignored), "grow" (the feedback's albedo is 1.5: gain above one). */
	int passes = 0;
	int maxPasses = 64;
	double settle = 1e-3;
	//! lane FARVIEW1: the glowing surfels left dark (the placed-only relight the far light takes out of the GI)
	bool noGlow = false;
	/*! lane GPURELIGHT1 (src/proberelight.h): set, the relight also records its operators there (the pairs, links,
	 *  feed and blend lists, each entry's door crossings re-traced against the doors' real geometry) for the
	 *  relight without rays. Its own result is unchanged bit for bit. recordExtra: lights off at the start (a
	 *  switch's, a script's) whose pairs are recorded but which light nothing here; recordRef / recordGroup /
	 *  recordFlags: per light (lights, then recordExtra), the placed reference, its group key and its light flags
	 *  (FARVIEW1b 3.2 / 3.6), empty = 0. */
	struct ProbeRelightOps * record = nullptr;
	QVector<WwCellLight> recordExtra;
	std::vector<quint32> recordRef;
	std::vector<quint64> recordGroup;
	std::vector<quint16> recordFlags;
	//! lane AODECAL1: the placed decal copies (null: none; the result is then byte for byte as before)
	std::shared_ptr<const AoDecalSet> aoDecals;
};

//! Relight the bake in `bakeDir` (its sector_*.tbk) with `lighting`, shadowed through `soup`.
bool probeGiRelight( const ProbeSoup & soup, const QString & bakeDir, const WwCellLighting & lighting,
	const ProbeGiSpec & spec, ProbeGiResult * out );
//! lane ROOMCLAMP1: after gi.rgba / gi.sky took grid / gridSky, append slot 1 and hand the rooms over (none: no-op)
void probeGiRoomsInto( ProbeGiResult & r, struct WwCellGi & gi );
//! lane AODECAL1: when the relight built the copy-free grids, they take the grids' place (before probeGiRoomsInto)
void probeGiAoFreeSwap( ProbeGiResult & r );
//! One census line.
QString probeGiCensusText( const ProbeGiResult & r );
//! The gate's dump: gi_surfels.bin, gi_probes.bin, gi_grid.bin in `dir` (layouts in probegi.cpp).
bool probeGiDump( const ProbeGiResult & r, const ProbeGiSpec & spec, const QString & dir, QString * err );
/*! lane ROOMCLAMP1: `probegi` places, bakes and relights a dumped soup by the lights given (an interior), then
 *  dumps (the synthetic rooms gate, tests/spells/cell_rooms.sh) */
int probeGiCli( const QStringList & args );

#endif // PROBEGI_H
