/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENAGGREGATE_H
#define LODGENAGGREGATE_H

#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

// AGGREGATE RING-3 IMPOSTORS -- one card set per forested cell.

//! What the compositor needs of ONE tree's card set. The caller fills it from
//! the `LodgenCard` it already has, so this module never parses a sidecar and
//! the tree keeps exactly one reader of that file.
struct LodgenAggCard
{
	QString dir;            //!< the bake tree the PNGs live in
	quint32 formId = 0;
	int oct = 0;            //!< frames per side, N; the sheet is N x N frames
	int frameW = 0, frameH = 0;
	int gapX = 0, gapY = 0;
	float halfW = 0.0f, halfH = 0.0f;   //!< model units, spanning the WHOLE frame
	float center[3] = { 0.0f, 0.0f, 0.0f };
	float depthSpan = 0.0f;
	QVector<float> frameOff;            //!< 2 * oct^2, or empty for a set from before the law
	//! The coverage contract of the colour sheet; all three 0 = the older
	//! vintage, whose alpha is the raw fraction (docs/LODGEN_CARD_SHEETS.md 4).
	int covFloor = 0, covTest = 0, covBase = 0;
	bool ortho = false;     //!< card.projection == "ortho"; absent/persp is REFUSED by name
	bool pbr = false;       //!< the family, for the mask sheet's suffix
};

//! One tree standing in a cell, as the chunk builder already knows it.
struct LodgenAggTree
{
	quint32 srcIndex = 0;   //!< the caller's own index for this placement (what `covered` names)
	quint32 baseId = 0;
	float pos[3] = { 0.0f, 0.0f, 0.0f };    //!< world units
	float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   //!< the DRAWN rotation, row-major, world = R * local
	float scale = 1.0f;
	bool mirrorU = false;   //!< the repetition breaker's mirror, (treeHash >> 8) & 1
};

struct LodgenAggOptions
{
	int minTrees = 8;       //!< a cell is FORESTED at this many tree placements
	int tile = 64;          //!< the long side of an aggregate frame, in texels
	int views = 8;          //!< azimuths, one elevation band, at the horizon
	int auxDiv = 1;         //!< mirrors --card-half-aux; 1 = every sheet full size
};

//! One finished aggregate: the three sheets, and everything the `.lodm` and the
//! `.lodi` row need. Every number here is WRITTEN and MOVES.
struct LodgenAggSet
{
	int cellX = 0, cellY = 0;
	QImage colour;          //!< RGB + coverage in A
	QImage normal;          //!< R,G view normal, B height, A sway
	QImage mask;            //!< gloss/rough, spec/metal, AO, subsurface
	int views = 0, frameW = 0, frameH = 0, gapX = 0, gapY = 0, mips = 1;
	float half[2] = { 0.0f, 0.0f };
	float centre[3] = { 0.0f, 0.0f, 0.0f };
	float depthSpan = 0.0f;
	float boundRadius = 0.0f;
	QVector<float> frameOffset;     //!< 2 per view, in the view's own right/up
	std::vector<quint32> covered;   //!< the caller's srcIndex, ascending
	bool pbr = false;
	bool anyMirrored = false;
	//! WRITTEN AND MOVES (the three rules of 2026-09-04): the height channel's
	//! own readings, so a gate can see a taller forest read taller and a flat
	//! one read flat.
	int heightMin = 255, heightMax = 0;
	double heightMean = 0.0;
	float heightSpanUnits = 0.0f;   //!< (heightMax - heightMin)/255 * depthSpan
	int coveredTexels = 0;
	double coverageMean = 0.0;
};

struct LodgenAggStats
{
	int cellsSeen = 0;              //!< cells holding at least one tree
	int cellsForested = 0;          //!< cells at or above the threshold
	int cellsAggregated = 0;        //!< cells that produced a set
	int treesPhotographed = 0;
	int treesRefusedNoCard = 0;     //!< a tree whose base has no card set on disk
	int treesRefusedNotOrtho = 0;   //!< a card from before the orthographic camera
	int treesRefusedNoImage = 0;    //!< the sheet PNGs would not load
	int cellsRefusedAllTreesLost = 0;
	QStringList refusals;           //!< one line a reason, in words
};

//! Build one aggregate set per forested cell:
//! `trees` is every tree placement the caller wants considered, in any order, each with the DRAWN rotation and
//! the mirror the repetition breaker gives it bungo's *"with the same rotation and mirror the repetition
//! breaking would give them"*.
bool lodgenAggregateBuild( const QVector<LodgenAggTree> & trees,
	const QHash<quint32, LodgenAggCard> & cards, const LodgenAggOptions & opts,
	QVector<LodgenAggSet> * out, LodgenAggStats * stats, QString * error );

//! The game path stem of a cell's aggregate set, without the suffix:
//! `Data\FO4CSLOD\<ws>\Aggregate\<cellX>_<cellY>_agg` (lane LAYOUT1,
//! 2026-09-16: one root for every FO4CS-target output).
QString lodgenAggregateStem( const QString & worldspace, int cellX, int cellY );

//! The `.lodm` payload for one set, `kind: "aggregate"` (docs/LODGEN_LODM_FORMAT.md 3a).
QByteArray lodgenAggregateLodm( const QString & worldspace, const LodgenAggSet & set );

#endif // LODGENAGGREGATE_H
