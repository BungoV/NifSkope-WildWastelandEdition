/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSPLAT_H
#define CELLSPLAT_H

#include <QString>

#include <vector>

#include "cellground.h"

class EsmWorld;

/* ---------------------------------------------------------------------------
 * THE GROUND, BLENDED -- the same LAND record, composited instead of picked.
 *
 * `cellground.h` says outright what it does not do: "This does NOT blend.
 * Every 128-unit quad of the 32x32 grid takes the ONE layer with the highest
 * opacity at its own corner".  The result is a hard-edged mosaic -- correct
 * about WHAT is painted, wrong about what it LOOKS like, and visibly a grid of
 * 128-unit tiles from any height.  This file is the composite.
 *
 * ===========================================================================
 * WHAT THE ENGINE DOES, AND WHY IT FITS IN THE EXISTING VERTEX FORMAT
 * ===========================================================================
 *
 * A quadrant has one `BTXT` base texture and any number of `ATXT` layers.
 * Each layer carries a `VTXT` grid of 17x17 opacities and an `ATXT` LAYER
 * INDEX -- the int16 at payload offset 6 -- which is its SLOT.
 *
 * CORRECTED BY LANE TERRBLEND1 (2026-10-05), out of Todd's treat (1.10.155:
 * LoadVerticesIntoArrays, CreateGeometry, MergeMatchingTextures) and the
 * shipped Landscape pixel shader: the engine does NOT composite alpha-over.
 * The opacities are SHARES of one sum,
 *
 *     colour = ( w0 * base + sum( w_i * layer_i ) ) * VCLR,  w0 = 1 - sum( w_i )
 *
 * with each w a truncated byte, the base the engine's default set when the
 * quadrant has no BTXT, matching layers merged, and only the first five
 * layers drawn. Read as alpha-over (this file until TERRBLEND1) a quadrant
 * with no BTXT had nothing under its first layer, so that layer was promoted
 * to opaque per quad -- the hard squares -- and a quad with no layer either
 * was not drawn at all -- the sky-coloured holes. cellsplat.cpp has the
 * engine's rules line by line.
 *
 * The 17x17 grid is not a texture to be sampled: the cell's 33x33 land vertex
 * grid maps ONE-TO-ONE onto the four 17x17 quadrant grids, with the middle row
 * and column shared.  So every corner of every 128-unit quad already HAS a
 * stored opacity -- no interpolation is invented here.  Interpolating that
 * weight across the quad, which the rasteriser does for free, IS the engine's
 * bilinear blend.
 *
 * WHAT THAT DOES **NOT** BUY, corrected by lane CELLVIEW4B against the record
 * bytes of Sanctuary -20,7 (scratchpad/cellview4b_20260919/seam_probe.py).
 * This file used to end that paragraph with "because neighbours share their
 * corner values there is no seam between quads".  That is true INSIDE a
 * quadrant and false ACROSS one.  A quadrant's layer list is its own: two
 * quadrants need not carry the same textures at all, and where they do carry
 * the same LTEX the two stored grids are independent numbers that need not
 * agree on the line they share.  Measured on -20,7: across the BL|BR line the
 * same DriedGrass01 layer differs by at most 0.0353, but across the BR|TR line
 * the same texture differs by up to 0.7490.  A discontinuity along a cell's
 * centre lines is therefore something the RECORD can contain, and the viewer
 * neither creates nor can remove it.  The measurement that separates the two:
 * of the 64 quad pairs straddling -20,7's centre lines, 30 change their
 * dominant texture, and all 30 are pairs where one side is a quadrant with no
 * BTXT at all; of the 32 pairs where both sides have a BTXT, ZERO change.  The
 * quadrant arithmetic is clean; the seam is in the data.
 *
 * That is why this needs no new texture units. One quad per contributing
 * layer, the share in the vertex colour's ALPHA byte:
 *
 *   - ONE texture unit per shape, as now; the layering is in the geometry.
 *   - The base pass is OPAQUE (it writes the depth) and the shader scales it
 *     by its own share; every layer pass is ADDED (SRC_ALPHA, ONE) at its
 *     share. A sum does not care about draw order, and the rasteriser's
 *     linear interpolation of each share IS the engine's per-pixel weight --
 *     exact, where an alpha-over opacity a_i / (w0 + .. + a_i) interpolated
 *     per vertex is not (measured: up to 255 of 255 off, p99 79).
 *   - The shapes are registered (wwCellLandShape) so the renderer tells the
 *     shader `landSplat`: the share is the vertex alpha alone, never times the
 *     diffuse texture's alpha.
 *   - A translucent shape does not write depth, so the layers cannot occlude
 *     one another, and all of them depth-TEST against the opaque base with
 *     `GL_LEQUAL`.
 *
 * ===========================================================================
 * THE VERTEX BUDGET
 * ===========================================================================
 *
 * The mosaic emits 1024 quads = 4096 vertices per cell, whatever the paint.
 * A splat emits one quad per (quad, contributing layer) pair plus the base, so
 * the cost is the MEAN NUMBER OF LAYERS WITH NON-ZERO WEIGHT over the cell's
 * quads, not the worst case.  `CellSplatBuild::quadsEmitted` against
 * `quadsTotal` is that multiplier, measured per build and printed in the
 * legend, so it is never a guess.  `cellSplatMaxVerts()` is the same figure
 * turned into the refusal the caller needs: a build that would pass
 * `CELL_MAX_TOTAL_VERTS` (12,000,000, src/cellview.cpp ~44) must refuse before
 * it allocates, not after.
 *
 * ===========================================================================
 * WHAT IT REFUSES TO PRETEND
 * ===========================================================================
 *
 * * The LAYER INDEX is only honoured when `esmdata` reads it.  Until the
 *   hook-up lands, `WW_CELLSPLAT_LAYER_INDEX` is undefined and the layers
 *   composite in RECORD ORDER, which is the order they happen to sit in the
 *   file.  That is a guess and is counted as one -- `layersInRecordOrder` is
 *   non-zero exactly when the build did not know the paint order.
 * * A quadrant with no BTXT is NOT bare: the engine draws its default land
 *   texture set there (ESM_LTEX_ENGINE_DEFAULT, CommonwealthDefault01), and
 *   so does this. `quadsDefaultBase` counts it.
 * --------------------------------------------------------------------------- */

//! A layer whose weight is below this at every corner of a quad contributes nothing.
constexpr float CELL_SPLAT_WEIGHT_MIN = 1.0f / 255.0f;

//! One corner of one splat quad. `w` is the layer's opacity AT THAT CORNER.
struct CellSplatVert
{
	float p[3] = { 0, 0, 0 };       //!< world units, MINUS the caller's origin
	float uv[2] = { 0, 0 };
	float rgb[3] = { 1, 1, 1 };     //!< VCLR, or neutral when the cell has none
	float w = 1.0f;                 //!< 1.0 on a base pass; the VTXT opacity on a layer
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
	/* THE TWO REASONS A QUAD IS BARE, KEPT APART (lane CELLVIEW4B). The builder
	 * has both in hand and was folding them into one number, and a counter
	 * whose halves cannot be told apart cannot fail on a broken LTEX reader:
	 * a resolver that returned nothing for every texture in the game would
	 * report the same `quadsBare` as a genuinely unpainted cell. */
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
	QStringList notes;
};

/*! The vertices `cellBuildSplat` would emit for this rectangle, WITHOUT
 *  building it -- the caller's refusal cap is checked before the allocation,
 *  never after. Returns -1 when the rectangle itself is bad. */
qint64 cellSplatCountVerts( EsmWorld & world, int x0, int y0, int x1, int y1 );

/*! Build the blended ground for the inclusive cell rectangle. `originX/Y` is
 *  subtracted from every position, exactly as `cellBuildGround`'s is.
 *  False, with `*error`, only on a bad rectangle -- a cell with no LAND is not
 *  an error, it is a cell with no LAND. */
bool cellBuildSplat( EsmWorld & world, int x0, int y0, int x1, int y1,
	float originX, float originY, float tiling, CellSplatBuild & out, QString * error );

/*! lane TERRBLEND1: the blended ground's shapes, by block, so the renderer can
 *  tell the shader `landSplat` (the share is the vertex alpha; an opaque base
 *  is scaled by it, a layer is added at it). Begin clears one document. */
void wwCellLandBegin( const void * nif );
void wwCellLandShape( const void * nif, int block );
bool wwCellLandIs( const void * nif, int block );

//! The one-line summary for the scene notes, in the shape cellGroundLegend uses.
QString cellSplatLegend( const CellSplatBuild & b );

#endif // CELLSPLAT_H
