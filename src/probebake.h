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
 *  a room with the sunlit outside of its own wall. */

#include "probeplace.h"

#include <QString>
#include <QStringList>

struct ProbeBakeSpec
{
	int rays = 2048;            //!< per probe, Fibonacci sphere
	float surfelCell = 70.0f;   //!< FO4CS's surfel cell (header surfelCellSize)
	quint32 maxLinks = 256;     //!< FO4CS BakeConfig::maxLinksPerProbe
	float rayMax = 131072.0f;   //!< a ray that meets nothing this far is sky
	int threads = 0;            //!< 0 = the machine's
	/*! An interior cell: no sky. A ray that meets nothing left through an opening
	 *  into the unloaded void, so its weight is unlinked (the relight renormalizes
	 *  over the surfaces), and every octant's sky visibility is 0. */
	bool noSky = false;
	/*! A deliberate defect for the gate's refuters: "octant" swaps the octant
	 *  bits (x negative -> bit 2), "normal" stores the triangle normal unflipped.
	 *  Empty in every real run. */
	QString red;
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
	bool noSky = false;                     //!< the spec's: misses went to unlinked, not sky
	double voidMean = 0;                    //!< over probes, noSky only: the sphere share that met nothing
	int albedoKnown = 0;                    //!< 1 when the soup carried albedo, else every surfel is grey
	double msRays = 0, msWrite = 0;
	QStringList files;
	QString error;
};

//! Bake the probes into `outDir` (created). False on an unusable input or an unwritable folder.
bool probeBake( const ProbeSoup & soup, const std::vector<ProbePoint> & probes, const ProbeBakeSpec & spec,
	const QString & outDir, ProbeBakeResult * out );
//! The census as notes lines (cell view notes, CLI stdout).
QString probeBakeCensusText( const ProbeBakeResult & r );
//! `nifskope -no-gui probebake --soup <f> --rect minX,minY,maxX,maxY --out <dir>
//!  [--rays n] [--spacing s] [--red octant|normal] [--no-openings] [--no-rooms]`: place, then bake.
int probeBakeCli( const QStringList & args );

#endif // PROBEBAKE_H
