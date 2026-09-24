/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLPICK_H
#define CELLPICK_H

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

/* ---------------------------------------------------------------------------
 * THE PICK TABLE -- what the cell view knows about each thing it drew.
 *
 * The scene welds (src/cellview.h 1), so the geometry itself cannot answer
 * "which ref is this".  The builder therefore WRITES DOWN every placement as it
 * welds it, with its world AABB, and picking is a ray against that table.  Two
 * consequences worth stating rather than discovering:
 *
 *   - the pick is by BOX, not by triangle.  Nearest box entered along the ray
 *     wins.  A small ref inside a big ref's box (a bottle on a shelf inside a
 *     building's box) is reachable only because ties are broken by the SMALLER
 *     box: without that rule every click in a downtown cell would hit the same
 *     building.  `pick()` returns the candidates it considered, so the panel can
 *     say "3 under the cursor" instead of pretending there was one.
 *   - the table is pure data.  No Qt widget, no GL, no NifModel: a harness can
 *     build a cell, ask for a formID, and check a world position, without a
 *     window and without a screenshot.  That is what makes the gate honest.
 * --------------------------------------------------------------------------- */

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

	//! Index of the first entry with this REFR form id, or -1.
	int indexOfForm( quint32 refForm ) const;

	/*! Nearest box the ray enters, or -1. `candidates`, when given, receives how
	 *  many boxes the ray entered at all -- the number the panel reports so a
	 *  pick never implies it was unambiguous. Ties on entry distance are broken
	 *  by the SMALLER box (see the header comment). */
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

/*! THE TABLE OF THE SCENE THAT IS OPEN. The builder fills it; the panel and the
 *  harness read it. One at a time, like the document itself. */
const CellPickTable & cellPickTable();
CellPickTable & cellPickTableMutable();

#endif // CELLPICK_H
