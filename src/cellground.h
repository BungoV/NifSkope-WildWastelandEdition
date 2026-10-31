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

/* The cell view's ground, built from the LAND record as pure data (no NifModel, no GL): every 128-unit
 * quad takes the one layer with the highest opacity at its corner (no blending), else the quadrant's base
 * texture. Blending belongs to the terrain bake's splat compositor (docs/LODGEN_TERRAIN_VT.md). */

//! The engine's landscape texture repeat (fLandTextureTilingMult default), world units (docs/LODGEN_TERRAIN_VT.md).
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
	QString spec;                   //!< TX07 _s as bound (empty under WW_TERRBLEND_RED=nospec), lane DUNESAND2
	QString specWant;               //!< TX07 whatever the red says (a live set puts it back)
	int quads = 0;
};

//! What the build measured, for the notes line.
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
	int bucketsSpec = 0;            //!< buckets that carry their LTEX's TX07 map (DUNESAND2)
	/*! The two reasons a quad is bare, which add up to `quadsBare` exactly. `quadsUnpainted`: no BTXT and
	 *  every layer at zero opacity here. `quadsLtexNoTexture`: a real LTEX was chosen and named no texture. */
	int quadsUnpainted = 0;
	int quadsLtexNoTexture = 0;
	QStringList notes;
};

/*! Build the mosaic for the inclusive cell rectangle; `originX/Y` is subtracted from every position.
 *  False only on a bad rectangle: a cell with no LAND is not an error, and `cells` counts those that had one. */
bool cellBuildGround( EsmWorld & world, int x0, int y0, int x1, int y1,
	float originX, float originY, float tiling, CellGroundBuild & out, QString * error );

//! The one-line summary for the scene notes.
QString cellGroundLegend( const CellGroundBuild & b );

#endif // CELLGROUND_H
