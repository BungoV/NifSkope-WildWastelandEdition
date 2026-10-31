/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSPLAT_H
#define CELLSPLAT_H

#include <QString>

#include <vector>

#include "cellblockmap.h"
#include "cellground.h"

class EsmWorld;

/* The ground, blended: the LAND record's layers composited as the engine does, instead of the
 * per-quad mosaic of cellground.h. The VTXT opacities are SHARES of one sum,
 *     color = ( w0 * base + sum( w_i * layer_i ) ) * VCLR,  w0 = 1 - sum( w_i )
 * (bytes truncated, matching layers merged, a quadrant with no BTXT takes the engine default base,
 * only the first five layers drawn). The 33x33 land grid maps one-to-one onto the four 17x17
 * quadrant grids, so each quad corner already has a stored weight and the rasterizer's
 * interpolation IS the bilinear blend. Seams across quadrant center lines can exist in the data.
 * One quad per contributing layer, share in vertex alpha: the base pass is opaque, layers are
 * ADDED (SRC_ALPHA, ONE) and depth-tested GL_LEQUAL, so draw order does not matter.
 * Without WW_CELLSPLAT_LAYER_INDEX the layers composite in record order (`layersInRecordOrder`). */

//! A layer whose weight is below this at every corner of a quad contributes nothing.
constexpr float CELL_SPLAT_WEIGHT_MIN = 1.0f / 255.0f;

//! One corner of one splat quad. `w` is the layer's opacity AT THAT CORNER.
struct CellSplatVert
{
	float p[3] = { 0, 0, 0 };       //!< world units, MINUS the caller's origin
	float uv[2] = { 0, 0 };
	float rgb[3] = { 1, 1, 1 };     //!< VCLR, or neutral when the cell has none
	float w = 1.0f;                 //!< 1.0 on a base pass; the VTXT opacity on a layer
	float n[3] = { 0, 0, 1 };       //!< the vertex normal (the LAND's VNML), what the ground is lit with
};

//! One 128-unit quad of one pass, corners counter-clockwise from SW.
struct CellSplatQuad
{
	CellSplatVert v[4];
	float nrm[3] = { 0, 0, 1 };
	int bucket = 0;                 //!< into CellSplatBuild::buckets
};

/*! One texture drawn in one pass. Buckets are emitted in DRAW ORDER: every
 *  opaque base bucket first, then the layer buckets by ascending `order`. The
 *  caller must keep that order when it builds its shapes, because the
 *  transparent pass sorts by block number. */
struct CellSplatBucket
{
	quint32 ltex = 0;               //!< the LTEX form; 0 = no texture resolved
	QString diffuse;                //!< TX00, or the MNAM material when material-backed
	QString normal;                 //!< TX01
	QString spec;                   //!< TX07, the _s smoothness/spec map the game's Landscape shader reads (DUNESAND1)
	QString specWant;               //!< the same TX07 whatever the nospec red says, so a live set can put it back
	int order = 0;                  //!< the ATXT layer index; -1 = an opaque base pass
	bool blend = false;             //!< false = opaque base, true = alpha-over
	int quads = 0;
};

//! What the build MEASURED, so the legend carries its own denominators.
struct CellSplatBuild
{
	QVector<CellSplatBucket> buckets;
	std::vector<CellSplatQuad> quads;
	int cells = 0;                  //!< cells with a LAND record
	int cellsWithColour = 0;
	int quadsTotal = 0;             //!< 1024 per cell with LAND -- the mosaic's count
	int quadsEmitted = 0;           //!< base + layer passes actually written
	int quadsBase = 0;              //!< of those, opaque base passes
	int quadsBlended = 0;           //!< of those, alpha-over layer passes
	int quadsBare = 0;              //!< no base and no layer with any weight
	/* Kept apart from quadsBare so a broken LTEX resolver cannot pass as an unpainted cell. */
	int quadsBareNoTexture = 0;     //!< every pass named an LTEX that resolved to no texture
	int quadsDefaultBase = 0;       //!< quads whose quadrant has no BTXT: the engine default base
	int quadrantsDefaultBase = 0;   //!< quadrants with no BTXT
	int quadsBaseFallback = 0;      //!< a BTXT whose LTEX named no texture: default drawn instead
	int layersMerged = 0;           //!< slots summed into a matching layer or dropped into the base
	int layersDropped = 0;          //!< layers past the fifth (read into the base sum, not drawn)
	int layersFormZero = 0;         //!< ATXT layers naming form 0: the engine's default set
	int layersRead = 0;
	int ltexUnresolved = 0;
	int layersInRecordOrder = 0;    //!< layers composited WITHOUT a known paint order
	int bucketsSpec = 0;            //!< buckets that carry their LTEX's TX07 map (DUNESAND1)
	QStringList notes;
};

/*! The vertices `cellBuildSplat` would emit for this rectangle, without building it, so the
 *  caller's cap is checked before allocating. Returns -1 when the rectangle itself is bad. */
qint64 cellSplatCountVerts( EsmWorld & world, int x0, int y0, int x1, int y1 );

/*! Build the blended ground for the inclusive cell rectangle; `originX/Y` is subtracted from
 *  every position. False, with `*error`, only on a bad rectangle (no LAND is not an error). */
bool cellBuildSplat( EsmWorld & world, int x0, int y0, int x1, int y1,
	float originX, float originY, float tiling, CellSplatBuild & out, QString * error );

/*! The blended ground's shapes, by block, so the renderer can tell the shader `landSplat`
 *  (share = vertex alpha). Begin clears one document. */
void wwCellLandBegin( const void * nif );
void wwCellLandShape( const void * nif, int block );
// lane CELLSTREAM1: a streamed cell let go of this block (it is about to be reused)
void wwCellLandForget( const void * nif, int block );
//! lane LIVESET3: the document's blocks were renumbered (a live set removed or moved some)
void wwCellLandRemap( const void * nif, const WwBlockMap & m );
bool wwCellLandIs( const void * nif, int block );
//! lane LANDSPEC2: the cell document's worldspace (EDID; empty = interior) and scene origin, for the land-LOD band
void wwCellLandOrigin( const void * nif, const QString & ws, float ox, float oy );
bool wwCellLandOriginOf( const void * nif, QString * ws, float * ox, float * oy );
//! lane LANDSPEC4: the open's near block (centre cell, half width in cells), so the band window covers all of it
void wwCellLandBlock( const void * nif, int cx, int cy, int half );
bool wwCellLandBlockOf( const void * nif, int * cx, int * cy, int * half );
//! lane LANDSPEC6: a "Fixed Dimensions" world's WCTR cell (the land-LOD blend centre stays on it); false = a normal world
void wwCellLandFixed( const void * nif, bool fixed, int wx, int wy );
bool wwCellLandFixedOf( const void * nif, int * wx, int * wy );

//! lane DUNESAND1: WW_TERRBLEND_RED=nospec drops the ground's TX07 map (the old smoothness 1, mask 1 ground)
bool cellSplatSpecRed();

/*! lane DUNESAND2: every ground shape (blend and mosaic) with the TX07 its LTEX names, so a live
 *  WW_TERRBLEND_RED=nospec set swaps slot 7 in place. Cleared, forgotten and remapped with the wwCellLand* table. */
void wwCellGroundSpecShape( const void * nif, int block, const QString & spec );
QHash<int, QString> wwCellGroundSpecs( const void * nif );

//! The one-line summary for the scene notes, in the shape cellGroundLegend uses.
QString cellSplatLegend( const CellSplatBuild & b );

#endif // CELLSPLAT_H
