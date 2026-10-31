/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLPICK_H
#define CELLPICK_H

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

/* The pick table: the scene welds, so the builder records every placement with its world AABB and picking
 * is a ray against the table. Pure data (no widget, GL or NifModel). The pick is by box; ties go to the
 * smaller box so a bottle inside a building's box stays reachable. */

//! Lane NIFXCAL1: a placed light at a placement (its own ref, or one inside its box), as stored on the records.
struct CellPickLight
{
	quint32 ref = 0, base = 0;
	QString edid;
	int color[3] = { 255, 255, 255 };
	float fade = 1, radius = 0, fov = 90;	//!< base + the ref's XLIG / XRDS
	quint32 flags = 0;						//!< LIGH DATA flags
	bool box = false;						//!< linked to a light box
};

//! One drawn placement, and everything the panel shows for it.
struct CellPickEntry
{
	quint32 refForm = 0;
	quint32 baseForm = 0;
	quint32 baseType = 0;       //!< record type fourcc, little-endian as stored
	int scolPart = -1;          //!< >= 0 when this came out of a SCOL expansion
	QString baseEdid;
	QString model;              //!< the MODL actually loaded
	float pos[3] = { 0, 0, 0 }; //!< the REFR's own world position (not the part's)
	float rot[3] = { 0, 0, 0 }; //!< radians, as stored
	float scale = 1.0f;
	float drawPos[3] = { 0, 0, 0 };     //!< where this placement was actually welded
	float bmin[3] = { 0, 0, 0 };        //!< world AABB of the welded geometry
	float bmax[3] = { 0, 0, 0 };
	int cellX = 0, cellY = 0;
	bool persistent = false;
	bool disabled = false;      //!< initially disabled, or enable parent in the opposite state
	bool marker = false;
	quint32 layer = 0;          //!< XLYR
	QString layerEdid;
	quint32 enableParent = 0;   //!< XESP parent ref
	bool enableParentOpposite = false;
	bool hasLod = false;
	QString lodModels[4];       //!< MNAM slots 0..3
	bool precombined = false;   //!< the cell's XCRI names this ref
	quint32 group = 0;          //!< our `.lodi` GROUP id, when a bake was loaded
	int groupSize = 0;
	bool haveGroup = false;
	quint32 triangles = 0;      //!< triangles this placement contributed
	QVector<CellPickLight> lights;	//!< lane NIFXCAL1
};

//! A ray in world units.
struct CellRay
{
	float o[3] = { 0, 0, 0 };
	float d[3] = { 0, 0, 1 };
};

class CellPickTable
{
public:
	void clear();
	int size() const { return entries.size(); }
	const CellPickEntry & at( int i ) const { return entries.at( i ); }

	//! Append; the form index takes the FIRST entry of a ref (SCOL part 0).
	void append( const CellPickEntry & e );

	//! Lane NIFXCAL1: a light the panel reads out under entry i.
	void addLight( int i, const CellPickLight & l ) { entries[i].lights.append( l ); }
	//! A light's vanilla values in .nifx terms (lumens, kelvin, gel, type, cone, shadow, flicker, tier), read-only.
	static QString nifxReadout( const CellPickLight & l );

	//! Index of the first entry with this REFR form id, or -1.
	int indexOfForm( quint32 refForm ) const;

/*! Nearest box the ray enters, or -1. `candidates` receives how many boxes the ray entered at all; ties
 *  on entry distance go to the smaller box. */
	int pick( const CellRay & ray, int * candidates = nullptr, float * tOut = nullptr ) const;

	//! The flat Name | Value rows for one entry, in panel order.
	QVector<QPair<QString, QString>> rowsFor( int i ) const;

	//! `STAT`, `SCOL`, ... from the stored fourcc; "?" when it is not printable.
	static QString typeName( quint32 fourcc );
	//! `0x0001A2B3` -- one spelling of a form id, everywhere.
	static QString formName( quint32 form );

private:
	QVector<CellPickEntry> entries;
	QHash<quint32, int> byForm;
};

/*! The table of the open scene: the builder fills it, the panel and the harness read it. */
const CellPickTable & cellPickTable();
CellPickTable & cellPickTableMutable();

#endif // CELLPICK_H
