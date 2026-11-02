#ifndef PROBEFAR_H
#define PROBEFAR_H

//! THE FAR MAP:
//! bungo 2026-10-01: "the top down bakes from the division games for far areas ... so that GI works on areas
//! far from you".

#include "probeplace.h"

#include <QString>
#include <QStringList>

#include <vector>

struct ProbeFarSpec
{
	QString lodl;               //!< <world>.lodl; the colour sheets are found beside it
	QString lodi;               //!< <world>.lodi, optional: building boxes and the roofline
	int step = 8;               //!< .lodl samples a ground quad (32 a cell: 8 = 1024 units)
	float hoist = 512.0f;       //!< above the cell's roofline
	bool region = false;        //!< cells x0..x1, y0..y1 only (inclusive); else the whole file
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int sheetDim = 32;          //!< which `.VT.<dim>.lodt` level gives the colour (0 = no colour)
	float ringCells = 2.0f;     //!< ground kept this many cells past the region, so edge probes see ground
	/*! "shift": read the heights one cell off (the height gate must fail). Each must fail
	 *  the tree gate: "notrees" leaves the trees out, "treebox" puts a solid box a tree, "treeshift"
	 *  stands every tree one cell east, "canopy" leaves the roofline under the trees. */
	QString red;
	/*! cells hx0..hx1, hy0..hy1 (inclusive) are loaded at full detail by the caller; their
	 *  ground, boxes and trees stay out of the soup (an object goes by the cell of its middle). */
	bool hole = false;
	int hx0 = 0, hy0 = 0, hx1 = -1, hy1 = -1;
	//! ground vertex normals from the heights' central differences (soup VNM1); off = face normals
	bool normals = false;
};

struct ProbeFarResult
{
	int cells = 0, probes = 0;
	int quads = 0, waterQuads = 0, boxes = 0, boxCells = 0;
	int albedoQuads = 0;                //!< ground quads whose colour came from the sheet
	int normalQuads = 0;                //!< ground quads given vertex normals
	qint64 heightOutside = 0;           //!< samples outside their cell's stored lo..hi (must be 0)
	double hoistMean = 0;               //!< mean probe height over the ground under it
	float roofMax = 0;
	int trees = 0, treesPlaced = 0;     //!< trees in the soup; tree placements the LOD data has in the ground's cells
	qint64 treeTris = 0, treeFirst = 0; //!< their soup triangles, the first of them (they run to the end)
	double treeArea = 0, treeAreaFull = 0;   //!< soup area of the trees; the same triangles before shrinking
	int treeMaterials = 0, treeNoTexture = 0;   //!< tree materials met; those the LOD folder has no layer for
	int treeCells = 0;                  //!< cells whose roofline a canopy raised
	QString treeNote;                   //!< why there are none, when there are none
	QString sheet, error;
};

bool probeFarBuild( const ProbeFarSpec & spec, ProbeSoup & soup, std::vector<ProbePoint> & probes,
	ProbeFarResult * out );
QString probeFarCensusText( const ProbeFarResult & r );
/*! the far soup around an exterior bake. Everything the LOD files hold within `radius`
 *  cells of cell (cx, cy), minus the loaded block `half` cells around it, appended to `soup` (no probes). */
bool probeFarAppendRing( const QString & lodl, const QString & lodi, int cx, int cy, int half, int radius,
	ProbeSoup & soup, ProbeFarResult * out, bool normals = false );
int probeFarCli( const QStringList & args );

#endif
