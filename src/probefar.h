#ifndef PROBEFAR_H
#define PROBEFAR_H

/*! THE FAR MAP (lane PRTPFAR, 2026-10-01; docs/PRTP_PLAN.md 2h).
 *
 *  bungo 2026-10-01: "the top down bakes from the division games for far areas ... so that GI works on
 *  areas far from you". The Division's distant shading (slide 57) and FO4CS's charter for its far tier:
 *  one sky-lit probe hoisted above each cell's roofline, baked for the WHOLE worldspace, so a place
 *  with no near probes loaded still has measured bounce light.
 *
 *  Built from the worldspace's own LOD files, never from loaded cells:
 *  - the ground: the `.lodl` heightfield every `step` samples, albedo from the colour sheet of the
 *    `.VT.*.lodt` beside it (the terrain's own albedo, grass tint folded in), sRGB decoded to linear;
 *  - water: the cell's water plane where the cell has water above its lowest ground;
 *  - buildings: the `.lodi` occluder boxes (boxes fitted INSIDE each placed object's LOD mesh),
 *    which also set the roofline the probe is hoisted over.
 *  The probes then go through the same bake as the near ones (src/probebake.cpp) into `.tbk` v3 files,
 *  one per cell, surfel cell 512. */

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
	QString red;                //!< "shift": read the heights one cell off (the height gate must fail)
};

struct ProbeFarResult
{
	int cells = 0, probes = 0;
	int quads = 0, waterQuads = 0, boxes = 0, boxCells = 0;
	int albedoQuads = 0;                //!< ground quads whose colour came from the sheet
	qint64 heightOutside = 0;           //!< samples outside their cell's stored lo..hi (must be 0)
	double hoistMean = 0;               //!< mean probe height over the ground under it
	float roofMax = 0;
	QString sheet, error;
};

bool probeFarBuild( const ProbeFarSpec & spec, ProbeSoup & soup, std::vector<ProbePoint> & probes,
	ProbeFarResult * out );
QString probeFarCensusText( const ProbeFarResult & r );
int probeFarCli( const QStringList & args );

#endif
