/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLREFS_H
#define CELLREFS_H

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

/* The cell's reference model: one entry per REFR the plugin has, drawn or not, keyed by form id and
 * appended in pushRefr (src/cellview.cpp) before any draw decision. Unlike cellPickTable() (what was
 * DRAWN), this is what EXISTS; `fate` says why a reference is not on screen. Pure data, no widgets or GL. */

//! Why a placed reference is, or is not, on screen -- the builder's own reason.
enum class CellRefFate
{
	Drawn = 0,
	Deleted,        //!< the record is flagged deleted
	NoBase,         //!< no base object at all
	Disabled,       //!< initially disabled (or enable parent inverted), row off
	NoModel,        //!< the base carries no MODL: lights, sounds, primitives
	Marker,         //!< an editor marker model, and the Markers row is off
	Projected,      //!< a decal, drawn on the surfaces under it (no shape of its own to pick)
	Refused         //!< placed content this view names and does not draw (the census says why)
};

/*! One loaded cell of the block (CellSceneSpec::n, odd): one entry per grid position the worldspace
 *  has a CELL record for. A list, so camera-following streaming only adds and removes rows. */
struct CellBlockEntry
{
	int cx = 0, cy = 0;
	quint32 cellForm = 0;
	/*! The CELL record's EDID, empty when it carries none. FULL is not here: Fallout4.esm is
	 *  localized, so FULL is a string-table index this reader does not resolve. */
	QString edid;
	int references = 0;     //!< reference-model entries from this grid position
	int drawn = 0;          //!< of those, the ones that reached the scene
};

//! `SanctuaryExt03 (-20,7)`, or `(-20,7)` for a cell with no editor id.
QString cellBlockLabel( const CellBlockEntry & c );

//! One placed reference, as the plugin has it.
struct CellRefEntry
{
	quint32 refForm = 0;
	quint32 baseForm = 0;
	quint32 baseType = 0;       //!< record type fourcc, little-endian as stored
	QString baseEdid;
	QString model;              //!< the base's MODL, empty for a modelless base
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 }; //!< radians, as stored
	float scale = 1.0f;
	int cellX = 0, cellY = 0;
	bool persistent = false;
	bool initiallyDisabled = false;
	quint32 layer = 0;
	quint32 enableParent = 0;
	bool enableParentOpposite = false;
	CellRefFate fate = CellRefFate::Drawn;
};

class CellRefTable
{
public:
	void clear();
	int size() const { return entries.size(); }
	const CellRefEntry & at( int i ) const { return entries.at( i ); }

	//! Append and return the index, so the caller can set the fate on its way out.
	int append( const CellRefEntry & e );
	//! Set the fate of an appended entry; out-of-range indices are ignored.
	void setFate( int i, CellRefFate fate );

	//! Index of the entry with this REFR form id, or -1.
	int indexOfForm( quint32 refForm ) const;

	//! How many entries carry this fate -- what the census counts, read back.
	int countOfFate( CellRefFate fate ) const;

	//! `drawn`, `no model`, ... -- one spelling of a fate, everywhere.
	static QString fateName( CellRefFate fate );

	// ---- the block: which cells are loaded right now
	void addCell( const CellBlockEntry & c );
	int cellCount() const { return cells.size(); }
	const CellBlockEntry & cellAt( int i ) const { return cells.at( i ); }
	//! The label for the cell at this grid position, or `(x,y)` if unknown.
	QString labelOfGrid( int cx, int cy ) const;
	/*! Count each cell's references and drawn references by walking the entries, so the per-cell
	 *  numbers cannot disagree with the list. Called once after the reads. */
	void tallyCells();

private:
	QVector<CellRefEntry> entries;
	QHash<quint32, int> byForm;
	QVector<CellBlockEntry> cells;
	QHash<QPair<int, int>, int> byGrid;
};

/*! The reference model of the open cell. The builder fills it; the workspace list and gates read it. */
const CellRefTable & cellRefTable();
CellRefTable & cellRefTableMutable();

#endif // CELLREFS_H
