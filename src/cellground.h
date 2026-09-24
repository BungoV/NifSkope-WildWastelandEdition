/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLGROUND_H
#define CELLGROUND_H

#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

class EsmWorld;

/* ---------------------------------------------------------------------------
 * THE GROUND, BUILT OUT OF THE LANDSCAPE RECORD.
 *
 * The cell view's first ground was the LAND heights with the VCLR vertex colour
 * on them and NO texture (src/cellview.h 2, "the base texture layers are NOT
 * sampled").  The heights were real from the first build -- VHGT is decoded in
 * `EsmWorld::land` -- but a relief surface with one flat grey on it and a
 * straight-down camera reads as a sheet of paper, and that is what the first
 * pictures showed.  This adds THE PAINT.
 *
 * ===========================================================================
 * WHAT IT DOES, AND WHAT IT REFUSES TO PRETEND
 * ===========================================================================
 *
 * A cell's paint is four quadrants (docs/LODGEN_ESM_LAYOUTS.md: 0 BL, 1 BR,
 * 2 TL, 3 TR).  Each has ONE base texture (`BTXT`) and any number of additional
 * layers (`ATXT` + `VTXT`), each layer carrying a 17x17 grid of opacities over
 * its quadrant.  The engine BLENDS them per pixel.
 *
 * This does NOT blend.  Every 128-unit quad of the 32x32 grid takes the ONE
 * layer with the highest opacity at its own corner, above
 * `CELL_GROUND_LAYER_MIN`, and the quadrant's base texture when no layer beats
 * that.  So the ground is a hard-edged mosaic of the real landscape textures,
 * not a composite: it answers "what is painted here" exactly and "what does
 * this look like in game" approximately, and the census line says so in those
 * words.  Blending needs the splat compositor the terrain bake owns
 * (docs/LODGEN_TERRAIN_VT.md), which is a bake, not a viewer.
 *
 * `T = 341.3333` world units is the ENGINE's landscape texture repeat, twelve
 * repeats across a cell, measured and written down in
 * docs/LODGEN_TERRAIN_VT.md (the `--land-tiling` paragraph).  It is not in the
 * data -- it is `fLandTextureTilingMult` -- so it is a constant here too, with
 * the same default the bake uses, and a caller may pass its own.
 *
 * ===========================================================================
 * WHY THIS IS A SEPARATE FILE
 * ===========================================================================
 *
 * It hands back PURE DATA -- quads with positions, UVs, normals, vertex colours
 * and a bucket index -- and never touches NifModel, Qt widgets or GL.  The cell
 * view welds those quads into its own buckets with its own vertex writer, so
 * there is no second geometry path; and a gate can ask for the mosaic of a cell
 * and count its buckets without a window, which is the only way the layer
 * choice is checkable at all.
 * --------------------------------------------------------------------------- */

//! The engine's landscape texture repeat, world units (docs/LODGEN_TERRAIN_VT.md).
constexpr float CELL_GROUND_TILING = 341.3333f;
//! A layer must reach this opacity to beat the quadrant's base texture.
constexpr float CELL_GROUND_LAYER_MIN = 0.5f;

struct CellGroundVert
{
	float p[3] = { 0, 0, 0 };       //!< world units, MINUS the caller's origin
	float uv[2] = { 0, 0 };
	float rgb[3] = { 1, 1, 1 };     //!< VCLR, or neutral when the cell has none
};

//! One 128-unit quad of the 32x32 grid, corners counter-clockwise from SW.
struct CellGroundQuad
{
	CellGroundVert v[4];
	float nrm[3] = { 0, 0, 1 };
	int bucket = 0;                 //!< into CellGroundBuild::buckets
};

//! One landscape texture the mosaic used.
struct CellGroundBucket
{
	quint32 ltex = 0;               //!< the LTEX form; 0 = no texture resolved
	QString diffuse;                //!< TX00, or the MNAM material when the TXST is material-backed
	QString normal;                 //!< TX01
	int quads = 0;
};

//! What the build MEASURED, so the notes line carries its own denominators.
struct CellGroundBuild
{
	QVector<CellGroundBucket> buckets;
	std::vector<CellGroundQuad> quads;
	int cells = 0;                  //!< cells with a LAND record
	int cellsWithColour = 0;        //!< of those, the ones carrying VCLR
	int quadsTotal = 0;
	int quadsTextured = 0;          //!< a diffuse actually resolved
	int quadsBare = 0;              //!< no LTEX, or an LTEX naming no texture
	int layersRead = 0;             //!< ATXT/VTXT layers over every quadrant
	int quadsFromLayer = 0;         //!< a layer beat the base texture here
	int ltexUnresolved = 0;         //!< LTEX forms that named no TXST texture
	/*! THE TWO REASONS A QUAD IS BARE, WHICH ADD UP TO `quadsBare` EXACTLY.
	 *
	 *  `quadsUnpainted`: the quadrant carries no BTXT and every layer is at
	 *  zero opacity at this quad's corner -- there is no paint in the plugin
	 *  here, and the Commonwealth WRLD names no default landscape texture to
	 *  inherit either (scratchpad/mountains_20260907/report_mountains.md R6).
	 *  `quadsLtexNoTexture`: a real LTEX form was chosen and it named no
	 *  texture.  The identity `quadsBare == quadsUnpainted +
	 *  quadsLtexNoTexture` is what the gate asserts, because it holds for any
	 *  cell in any worldspace and FAILS on the pre-2026-09-19 layer choice
	 *  (which let a null-form layer win and blank a quadrant that had a
	 *  perfectly good BTXT). An invariant, not a threshold. */
	int quadsUnpainted = 0;
	int quadsLtexNoTexture = 0;
	QStringList notes;
};

/*! Build the mosaic for the inclusive cell rectangle.  `originX/Y` is
 *  subtracted from every position, exactly as the cell view's own origin is.
 *  False, with `*error`, only on a bad rectangle -- a cell with no LAND is not
 *  an error, it is a cell with no LAND, and `cells` says how many there were. */
bool cellBuildGround( EsmWorld & world, int x0, int y0, int x1, int y1,
	float originX, float originY, float tiling, CellGroundBuild & out, QString * error );

//! The one-line summary for the scene notes.
QString cellGroundLegend( const CellGroundBuild & b );

#endif // CELLGROUND_H
