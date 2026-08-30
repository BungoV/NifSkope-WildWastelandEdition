/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef BTDTERRAIN_H
#define BTDTERRAIN_H

#include <QString>

class NifModel;
class QWidget;

//! What a .btd file's header says about its worldspace, read without
//! decompressing anything. Cell coordinates are game cells (4096 units).
struct BtdWorldInfo
{
	int cellMinX = 0, cellMinY = 0, cellMaxX = 0, cellMaxY = 0;
	float heightMin = 0.0f, heightMax = 0.0f;
	int landTextureCount = 0;
	int groundCoverCount = 0;
};

//! One region choice: an inclusive cell rectangle and a detail level.
//! lod 0 samples every 32 units (the file's full resolution); each level
//! halves that, so lod 4 is one sample every 512 units.
struct BtdRegionSpec
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int lod = 4;
	bool valid = false;
};

//! Read the .btd header only. False (with *error set) on a file that is not
//! version 5/6 BTD.
bool btdReadWorldInfo( const QString & path, BtdWorldInfo & info, QString * error );

//! How many BSTriShapes and vertices a region would generate, for dialogs and
//! guards. Returns false if the region is outside the file or over budget,
//! with *error naming the limit.
bool btdEstimateRegion( const BtdWorldInfo & info, const BtdRegionSpec & spec,
	qint64 * shapes, qint64 * verts, QString * error );

/*! Build a Fallout 4 document whose geometry is the region's terrain.
 *
 * One BSTriShape per tile of cells (as many cells as keep a tile under the
 * 65,535-vertex format limit), heights from the file's heightmaps, normals
 * from finite differences on the same grid, seams closed by sampling one row
 * into the neighbouring cell. The model is rebuilt from scratch (createNew).
 */
bool nifCreateBtdTerrainScene( NifModel * nif, const QString & btdPath,
	const BtdRegionSpec & spec, QString * error );

//! The WW_BTD_REGION="x0,y0,x1,y1,lod" environment override, shared by the
//! dialog and the no-dialog open paths. False when the variable is unset or
//! malformed.
bool btdRegionFromEnv( BtdRegionSpec & spec );

//! Modal region/detail picker. Returns false if cancelled. When the
//! environment variable WW_BTD_REGION is set to "x0,y0,x1,y1,lod" the dialog
//! is skipped and that region is returned, which is how harnesses drive this.
bool btdQueryRegion( QWidget * parent, const QString & path,
	const BtdWorldInfo & info, BtdRegionSpec & spec );

//! The region a bare open uses when no dialog ran (command line, harness):
//! the whole worldspace at lod 4.
BtdRegionSpec btdDefaultRegion( const BtdWorldInfo & info );

#endif // BTDTERRAIN_H
