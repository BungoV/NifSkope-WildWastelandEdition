/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLREFS_H
#define CELLREFS_H

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

/* ---------------------------------------------------------------------------
 * THE CELL'S REFERENCE MODEL -- every placed reference the plugin has, drawn or
 * not (lane CELLWORK1, 2026-09-19).
 *
 * bungo, verbatim: "The goal for cell viewer, is to eventually edit and save the
 * cells, view all the technical placed objects, do everything creation kit does
 * with cell editing".
 *
 * ===========================================================================
 * WHY THIS IS NOT `cellPickTable()`
 * ===========================================================================
 *
 * `src/cellpick.h` is DRAW DATA: one entry per thing that was welded into the
 * scene, with the world box the ray tests against.  Four kinds of reference
 * never reach it at all --
 *
 *   - a light, a sound marker, a trigger box or any other record whose base
 *     carries no MODL: the builder counts it as "skipped, no model" and moves
 *     on;
 *   - an editor marker, while the Markers row is off;
 *   - an initially-disabled reference, while the Disabled row is off;
 *   - a deleted reference, and a reference with no base at all.
 *
 * -- and one kind reaches it MORE THAN ONCE: an SCOL expands into a row per
 * part, because each part is a separately pickable box.
 *
 * So a list built on the pick table is a list of what was DRAWN, and the
 * Creation Kit's cell view is a list of what EXISTS.  This table is the second
 * thing: one entry per REFR the reader returned, keyed by reference form id,
 * appended at the single funnel every REFR passes through
 * (`pushRefr` in src/cellview.cpp) BEFORE any decision about drawing is taken.
 * Its count is therefore the plugin's placed-reference count for the block, and
 * a gate can check that against an independent read of the same plugin.
 *
 * `fate` is why a reference is not on screen, in the builder's own words. It is
 * set on the way out of each of pushRefr's early returns, so it cannot drift
 * from the reason the census counts.
 *
 * ===========================================================================
 * THIS IS THE EDITOR'S MODEL, AND IT IS DELIBERATELY NOT A WIDGET
 * ===========================================================================
 *
 * Pure data -- no Qt widget, no GL, no NifModel -- exactly like the pick table,
 * for the same reason: a gate can build a cell, count its references and check
 * one by form id without a window and without a screenshot.  A later lane that
 * makes rows EDITABLE wants a model keyed by reference form id with an undo
 * stack over it (the animation workspace's shape), and this is the object that
 * model edits.  What does not exist anywhere in this tree yet is a WRITER: the
 * ESM/ESP side is read-only (src/esmdata.h has no save, no record emit, and no
 * byte writer), so editing and saving a cell is a plugin-writing lane before it
 * is a UI lane.
 * --------------------------------------------------------------------------- */

//! Why a placed reference is, or is not, on screen -- the builder's own reason.
enum class CellRefFate
{
	Drawn = 0,
	Deleted,        //!< the record is flagged deleted
	NoBase,         //!< no base object at all
	Disabled,       //!< initially disabled (or enable parent inverted), row off
	NoModel,        //!< the base carries no MODL: lights, sounds, primitives
	Marker          //!< an editor marker model, and the Markers row is off
};

/*! ONE LOADED CELL (lane CELLWORK1, bungo: "the viewed cells should include
 *  their names too if they have them" / "display a set amount of loaded cells
 *  near the camera, so not just one if you so choose").
 *
 *  The view has ALWAYS loaded a block (`CellSceneSpec::n`, odd: 1, 3, 5), so
 *  "more than one cell" is not new -- what was missing is that nothing said
 *  which cells those were or what each one carried.  This list is one entry per
 *  grid position in the block that the worldspace actually has a CELL record
 *  for, and the workspace's list carries a Cell column keyed to it.
 *
 *  It is deliberately a LIST OF CELLS and not a single cell with extra fields,
 *  so the camera-following streaming lane has the shape it needs already: rows
 *  come and go, nothing else changes. */
struct CellBlockEntry
{
	int cx = 0, cy = 0;
	quint32 cellForm = 0;
	/*! The CELL record's EDID, empty when it carries none.  The DISPLAY name
	 *  (FULL) is NOT here: Fallout4.esm is localised, so FULL is a four-byte
	 *  index into a string table this reader does not read.  An empty edid is a
	 *  cell with no editor id, which is ordinary; there is no field that means
	 *  "we could not be bothered to look". */
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
	/*! Count each cell's references and drawn references from the entries
	 *  themselves. Called once after the reads, so the per-cell numbers are a
	 *  WALK of the table the list shows and not a second set of counters that
	 *  could disagree with it. */
	void tallyCells();

private:
	QVector<CellRefEntry> entries;
	QHash<quint32, int> byForm;
	QVector<CellBlockEntry> cells;
	QHash<QPair<int, int>, int> byGrid;
};

/*! THE REFERENCE MODEL OF THE CELL THAT IS OPEN. The builder fills it; the
 *  workspace's list and the gates read it. One at a time, like the document. */
const CellRefTable & cellRefTable();
CellRefTable & cellRefTableMutable();

#endif // CELLREFS_H
