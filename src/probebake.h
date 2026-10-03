#ifndef PROBEBAKE_H
#define PROBEBAKE_H

/*! THE PRTP BAKE (lane PRTPBAKE, 2026-09-30; docs/PRTP_PLAN.md 2g).
 *
 *  Transport only, no light (the Division rule FO4CS kept: sun, sky and placed
 *  lights are evaluated live through the baked transport). For every placed
 *  probe, rays in a Fibonacci sphere (equal solid angle each) go through the
 *  same soup and the same BVH the placer used. A hit is a surfel sample
 *  (70-unit cells: position, normal, albedo means); a miss is sky (void in an interior: noSky). Per probe:
 *  sky fraction and mean / RMS hit distance per octant, and links to the
 *  surfel cells it saw, weighted by solid angle, the strongest `maxLinks` kept.
 *
 *  Output: FO4CS's `.tbk` v3 sector files, byte for byte its layout
 *  ('TBK1', 64-byte header, surfels 32 B, probes 144 B, links 12 B, one file
 *  per cell, sector_%+05d_%+05d.tbk), so its relight reads them unchanged.
 *  One divergence: a cell's file carries EVERY surfel its own probes link to,
 *  also those standing in a neighbour cell, so each cell bakes alone (the
 *  reader keeps the first copy of a duplicated surfel cell).
 *
 *  Thin walls: a 70-unit cell often holds both faces of a wall. The cell keeps
 *  one side (the one most rays saw, plus faces not opposed to it), and a probe
 *  never links a surfel turned away from it: that weight goes to unlinkedWeight,
 *  which FO4CS's relight leaves out of the gather domain. Linking it would light
 *  a room with the sunlit outside of its own wall.
 *
 *  Lane BAKE4 (2026-10-01): v4 (the default; spec.tbkVersion) keeps both sides of a
 *  thin wall in their own cell, tags every link with its side, the glass tint on its
 *  way and the door it crossed, and every probe with its sky tint and rooms. The v3
 *  writer above is unchanged byte for byte (tbkVersion 3). */

#include "probeplace.h"

#include <QString>
#include <QStringList>

struct ProbeBakeSpec
{
	int rays = 2048;            //!< per probe, Fibonacci sphere
	float surfelCell = 70.0f;   //!< FO4CS's surfel cell (header surfelCellSize)
	quint32 maxLinks = 1024;    //!< FO4CS's in-game bake keeps 256 (BakeConfig::maxLinksPerProbe); the
	                            //!< relight reads any count. 256 drops 0.04 of the sphere on busy probes
	                            //!< (the reference gate's irradiance error: 0.111 at 256, 0.059 at 1024)
	float sector = 4096.0f;     //!< a `.tbk` file's square (FO4CS: one exterior cell); the far map keeps it
	float rayMax = 131072.0f;   //!< a ray that meets nothing this far is sky
	int threads = 0;            //!< 0 = the machine's
	/*! An interior cell: no sky. A ray that meets nothing left through an opening
	 *  into the unloaded void, so its weight is unlinked (the relight renormalizes
	 *  over the surfaces), and every octant's sky visibility is 0. */
	bool noSky = false;
	/*! 2026-10-01: THE SECOND SIDE, inside v3. A cell seen from both sides of a thin
	 *  wall keeps one side; the other side's surfel goes into the EMPTY neighbour cell
	 *  on its own side (the reader keys a surfel by its position, so the position is
	 *  nudged across the boundary, under one cell). A probe the kept side faces away
	 *  from links that surfel instead of being refused. Off = the old refusal. */
	bool spill = true;
	/*! Lane BAKE4: the file version. 4 (default) = the v3 body plus a tail: a cell seen from
	 *  both sides of a thin wall keeps BOTH sides in itself (a back surfel; each ray links
	 *  the side whose face it hit), every link's side, glass tint and door, every probe's
	 *  sky tint per octant and rooms, and the room boxes. FO4CS's reader takes v3 only
	 *  (it refuses any other version and size), so 3 writes its file exactly (the second
	 *  side spilled next door, as above). */
	int tbkVersion = 4;
	/*! A deliberate defect for the gate's refuters: "octant" swaps the octant
	 *  bits (x negative -> bit 2), "normal" stores the triangle normal unflipped;
	 *  lane BAKE4 (v4): "oneside" drops the back surfels (their links refused, the
	 *  old rule), "rooms" writes no room ids or boxes, "glass" bakes as if no glass.
	 *  Empty in every real run. */
	QString red;
	/*! Lane CAPTURE1: where a surfel's albedo and normal come from (positions, sides, links, sky
	 *  and glass are the same in all three; only those two values differ):
	 *    "tri" (default)  one albedo per triangle (its centroid UV, coarse map), the face normal
	 *    "hit"            each ray hit's own point: UV -> texel at a mip from the ray's footprint,
	 *                     interpolated vertex color and vertex normal (needs soup.mat)
	 *    "cube"           every probe traces a cube of cubeFace^2 pixels a face (the deck's G-buffer
	 *                     capture); a surfel takes the mean albedo + normal of the pixels in its cell
	 *  albedoRed: "centroid" (hit reads the triangle's own tri value: equals tri), "nofilter" (the
	 *  cube also sees soup.cubeExtra). Empty in every real run. */
	QString albedoWay = QStringLiteral( "tri" );
	int cubeFace = 128;
	QString albedoRed;
	//! cube: these probes' six faces (rgb u8 linear, dist f32, facing f32, nrm f32x3 a pixel, the
	//! notes\probecap tracer's layout) are written to cubeDump
	QString cubeDump;
	std::vector<int> cubeDumpProbes;
	/*! lane ROOMCLAMP1: a probe whose rays meet the BACKS of one-sided faces on more than this share of the
	 *  sphere stands outside the shell: moved to the nearest clear point under it, or dropped (>= 1: off). */
	float backMax = 0.25f;
	QString backDump;   //!< the gate's list of every probe's share and fate (empty: none)
};

struct ProbeBakeResult
{
	int probes = 0, sectors = 0, surfels = 0, links = 0;
	qint64 rays = 0, hits = 0, misses = 0;
	double skyMean = 0, unlinkedMean = 0;   //!< over probes: sky fraction of 4 pi; dropped link weight
	int probesCapped = 0;                   //!< probes that saw more surfel cells than maxLinks
	int twoSided = 0;                       //!< surfel cells seen from opposite sides (a thin wall): one side kept
	qint64 linksTurned = 0;                 //!< probe -> cell links refused: the cell's surfel faces away
	double turnedMean = 0;                  //!< over probes: the sphere share those refusals unlinked
	int spilled = 0;                        //!< second sides housed in an empty neighbour cell
	qint64 linksSpilled = 0;                //!< links that reached a second side instead of a refusal
	bool noSky = false;                     //!< the spec's: misses went to unlinked, not sky
	double voidMean = 0;                    //!< over probes, noSky only: the sphere share that met nothing
	int albedoKnown = 0;                    //!< 1 when the soup carried albedo, else every surfel is grey
	// lane CAPTURE1: the albedo way's census
	QString albedoWay;
	qint64 albSamples = 0;                  //!< hit: ray hits read at their point; cube: pixels that landed on a surfel
	qint64 albDropped = 0;                  //!< cube: pixels on a cell with no surfel (or sky)
	qint64 albExtra = 0;                    //!< cube red nofilter: pixels on a left-out shape
	int albSurfels = 0, albKept = 0;        //!< surfels the way rewrote; kept their tri value (no sample)
	int albLeaned = 0;                      //!< normals leaned toward the face normal to face every probe linking them
	double msAlbedo = 0;
	// lane BAKE4
	int version = 3;                        //!< the `.tbk` version written
	int backSurfels = 0, backWritten = 0;   //!< v4: second sides kept in their own cell; written (summed over files)
	qint64 linksBack = 0, linksDoor = 0, linksTinted = 0;   //!< v4 links: to a back surfel, through a door, through glass
	int doors = 0, glassTris = 0;           //!< the soup's door boxes and glass triangles
	double glassMean = 0;                   //!< over probes: the sphere share seen through glass
	int probesRoomed = 0, boxesWritten = 0; //!< probes standing in an enclosed room; room boxes written (summed over files)
	// lane EMISSIVEGI1: glowing surfaces
	int emitTris = 0, emitSurfels = 0, emitDropped = 0;   //!< the soup's glowing triangles; surfels written with Le; lost to v3
	qint64 emitHits = 0;                    //!< pass-1 hits on a glowing triangle
	double emitLeSum[3] = { 0, 0, 0 };      //!< the written surfels' Le, summed
	double msRays = 0, msWrite = 0;
	// lane ROOMCLAMP1: probes outside the shell
	bool backRule = false;
	float backMax = 0.25f;
	int backMoved = 0, backDropped = 0;
	double backShareMax = 0;
	std::vector<int> backHist;              //!< probes per 0.05 of back-face share (20 bins), before the rule
	QStringList files;
	QString error;
};

//! Bake the probes into `outDir` (created). False on an unusable input or an unwritable folder.
//! `roomBoxes` (the placer's) go into the v4 files whose probes name their rooms.
bool probeBake( const ProbeSoup & soup, const std::vector<ProbePoint> & probes, const ProbeBakeSpec & spec,
	const QString & outDir, ProbeBakeResult * out, const std::vector<ProbeRoomBox> * roomBoxes = nullptr );
//! The census as notes lines (cell view notes, CLI stdout).
QString probeBakeCensusText( const ProbeBakeResult & r );
//! `nifskope -no-gui probebake --soup <f> --rect minX,minY,maxX,maxY --out <dir>
//!  [--rays n] [--spacing s] [--red octant|normal] [--no-openings] [--no-rooms]`: place, then bake.
int probeBakeCli( const QStringList & args );

#endif // PROBEBAKE_H
