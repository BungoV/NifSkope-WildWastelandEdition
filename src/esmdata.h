/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef ESMDATA_H
#define ESMDATA_H

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

#include <memory>

class ESMFile;

/* The record layer LODGEN reads a worldspace through (docs/LODGEN_PLAN.md,
 * rung 0). It answers exactly three questions per cell: what is the ground
 * (LAND heights), what stands on it (REFRs with their transforms), and which
 * of those have LOD models (STAT MNAM / TREE). Everything else in the ESM is
 * out of scope on purpose.
 *
 * The container parser is the vendored fo76utils ESMFile; layouts follow
 * wbDefinitionsFO4.pas. One trap this layer owns so callers cannot fall into
 * it: a worldspace's PERSISTENT cell also reports grid (0,0) — it holds
 * thousands of REFRs and no LAND. It is distinguished by its parent group
 * (directly under the world-children group, not under a subblock), kept
 * separate, and its REFRs are served alongside every cell's.
 */

//! One placed reference, already resolved far enough for LOD generation.
struct EsmRefr
{
	quint32 formID = 0;
	quint32 base = 0;
	quint32 baseType = 0;       //!< record type fourcc ("STAT", "TREE", ...)
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 }; //!< radians, Z-Y-X euler as stored
	float scale = 1.0f;         //!< XSCL, 1.0 when absent
	bool initiallyDisabled = false;
	bool deleted = false;
};

//! A base object's LOD model set (STAT MNAM rows / TREE model).
struct EsmLodBase
{
	quint32 formID = 0;
	quint32 type = 0;
	QString models[4];          //!< per-level LOD model paths, empty = none
	bool hasLod = false;
	// tree wind knobs (TREE CNAM / STAT DNAM), for the chunk manifest
	float trunkFlexibility = 0.0f;
	float branchFlexibility = 0.0f;
	float leafAmplitude = 0.0f;
	float leafFrequency = 0.0f;
};

//! One placement inside a static-collection part, in the SCOL's local space.
struct EsmScolPlacement
{
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 };  //!< radians, same euler convention as REFR
	float scale = 1.0f;
};

//! One part of a static collection: a source base and where its copies sit.
struct EsmScolPart
{
	quint32 base = 0;
	QVector<EsmScolPlacement> placements;
};

//! One additional splat layer on a cell quadrant: 17x17 opacities.
struct EsmLandLayer
{
	quint32 ltex = 0;
	float opacity[17][17];      //!< [row][col] over the quadrant, 0..1
};

//! One exterior cell's landscape: 33x33 heights in game units, world-placed.
struct EsmLand
{
	int cellX = 0, cellY = 0;
	bool valid = false;
	float heights[33][33];      //!< [row=y][col=x], SW origin, game units
	// splat data (docs/LODGEN_ESM_LAYOUTS.md): quadrants 0 BL, 1 BR, 2 TL, 3 TR
	quint32 baseTex[4] = { 0, 0, 0, 0 };    //!< BTXT LTEX per quadrant
	QVector<EsmLandLayer> layers[4];        //!< ATXT/VTXT layers, draw order
};

class EsmWorld
{
public:
	EsmWorld();
	~EsmWorld();

	//! Open an ESM (or comma-separated master list) and index one worldspace.
	bool load( const QString & esmPath, quint32 worldspaceFormID, QString * error );

	quint32 worldspace() const { return wsForm; }
	QString worldspaceEdid() const { return wsEdid; }

	float defaultLandHeight() const { return defLandH; }
	float defaultWaterHeight() const { return defWaterH; }

	//! Cell water: true when the cell has water, with its height resolved
	//! (XCLW when present and not the no-water sentinel, else the
	//! worldspace default).
	bool cellWater( int cx, int cy, float & height ) const;

	//! Grid extent of indexed exterior cells (inclusive).
	void cellBounds( int & minX, int & minY, int & maxX, int & maxY ) const;
	int cellCount() const { return cellIndex.size(); }
	bool hasCell( int cx, int cy ) const;

	//! LAND heights for one exterior cell. False when the cell or LAND is absent.
	bool land( int cx, int cy, EsmLand & out ) const;

	//! REFRs placed in one exterior cell (temporary + that cell's persistent).
	QVector<EsmRefr> refrs( int cx, int cy ) const;
	//! The worldspace's persistent-cell REFRs, grid-filtered by world position.
	QVector<EsmRefr> persistentRefrsIn( float minX, float minY, float maxX, float maxY ) const;

	//! LOD model info for a base object, cached. Never null.
	const EsmLodBase & lodBase( quint32 baseFormID ) const;

	//! A static collection's parts, cached; empty for non-SCOL bases.
	const QVector<EsmScolPart> & scolParts( quint32 formID ) const;

	//! An LTEX form's diffuse/normal texture paths (via TNAM -> TXST), cached.
	void ltexTextures( quint32 ltexForm, QString & diffuse, QString & normal ) const;

	//! All worldspaces in the file: formID -> EDID (static convenience).
	static QVector<QPair<quint32, QString>> listWorldspaces( const QString & esmPath, QString * error );

private:
	std::unique_ptr<ESMFile> esm;
	quint32 wsForm = 0;
	QString wsEdid;
	float defLandH = 0.0f;
	float defWaterH = 0.0f;
	struct CellEntry
	{
		quint32 cellForm = 0;
		quint32 childGroup = 0;   //!< the cell's type-6 group (0 if none)
	};
	QHash<QPair<int, int>, CellEntry> cellIndex;
	quint32 persistentCellGroup = 0;
	mutable QHash<quint32, EsmLodBase> lodBaseCache;
	mutable QHash<quint32, QVector<EsmScolPart>> scolCache;
	mutable QHash<quint32, QPair<QString, QString>> ltexCache;
	mutable QVector<EsmRefr> persistentCache;
	mutable bool persistentCacheBuilt = false;

	void indexWorldspace();
	QVector<EsmRefr> refrsInGroup( quint32 groupID ) const;
	const QVector<EsmRefr> & persistentRefrs() const;
};

#endif // ESMDATA_H
