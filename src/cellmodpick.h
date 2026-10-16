/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLMODPICK_H
#define CELLMODPICK_H

#include <QString>
#include <QStringList>
#include <QVector>

/* Which mods the cell view loads, in which order: the shown far LOD bake's plugins, else cellmodsource.h's
 * source. The choice is saved per worldspace (QSettings CellView/ModPick, ModPickOrder, ModPickAdded); no key
 * = load exactly what was loaded before. WW_MODPICK_RED=inert|noorder|noadded ignores parts of the choice. */

struct CellModRow
{
	QString key;            //!< file name lower case; "dir:<path lower>" for a folder row
	QString path;           //!< full path (a folder row: the folder)
	QString file;           //!< file name as on disk (a folder row: the folder's name + "/")
	QString mod;            //!< the mod folder it comes from ("Data" = the game's own folder; "added: <folder>")
	bool present = false;   //!< on disk
	bool on = true;         //!< ticked: loads (a gone file is never on)
	bool locked = false;    //!< Fallout4.esm / a DLC master: stays at the top, always loads
	bool added = false;     //!< dropped in by hand
	bool folder = false;    //!< a folder holding no plugin: a resource root only
};

struct CellModList
{
	QString ws;
	QString source;         //!< "bake" | "mo2" | "data" | "masters" | "" (nothing to list)
	QString label;          //!< the header's "from ..." words
	QString from;           //!< the .lodb, the MO2 profile folder or the Data folder
	QVector<CellModRow> rows;
	QStringList naturalKeys;//!< the source order, then the added rows in the order they were added
	QStringList sourceStack;//!< the source's resource stack (non-bake sources)
	QString note;           //!< what the source skipped or where it looked
	QString error;          //!< why the list is empty
};

/*! The picker's list for worldspace `ws`, in the saved order, ticks from the saved choice (untouched: ticked =
 *  what the last build of `ws` loaded). `lodb` = the shown bake's record (empty: no bake shown). */
CellModList cellModPickList( const QString & ws, const QString & lodb );

//! The bake record the last cell build of `ws` showed (empty: none), and the plugins it loaded (comma list).
QString cellModPickShownBake( const QString & ws );
QString cellModPickLoaded( const QString & ws );

struct CellModChoice
{
	QStringList off;        //!< unticked row keys
	QStringList order;      //!< every row key in order; empty = the source order
	QStringList added;      //!< dropped paths, in list order
};

bool cellModPickTouched( const QString & ws );
CellModChoice cellModPickChoice( const QString & ws );
void cellModPickSave( const QString & ws, const CellModChoice & c );
//! Forget the choice (ticks, order, additions): the next build loads what it loaded before the picker was touched.
void cellModPickReset( const QString & ws );

// ---- rows, masters and drops (the widget's rules; pure functions)
QString cellModRowKey( const QString & path, bool folder );
//! Fallout4.esm and the DLC masters (the engine's fixed list): locked at the top
bool cellModLockedMaster( const QString & fileName );
//! The MAST entries of a plugin's header (file names; empty for a gone or unreadable file). Cached by path + size.
QStringList cellModPluginMasters( const QString & path );
//! Rows for dropped paths: a plugin file, or a folder (its plugins, .esm/.esl before .esp; a folder row when none).
QVector<CellModRow> cellModDropRows( const QStringList & paths, QString * skipped );
/*! A plugin row placed above one of its masters, or a master missing from the list: the refusal (empty = fine).
 *  Only problems `after` has and `before` did not are reported (a stale list never blocks a move). */
QString cellModOrderProblem( const QVector<CellModRow> & before, const QVector<CellModRow> & after );

struct CellModApplied
{
	QString plugins;        //!< what the near cells load (comma list)
	QStringList resources;  //!< the stack to set (empty: unchanged)
	QString line;           //!< the notes' "mods: ..." line(s)
	QString viewNote;       //!< empty unless the near cells' mods differ from the shown bake's
};

/*! At cell build, after the far LOD stack step: `plugins` = what the cell would load (the bake's after an
 *  adoption), `lodb` = the shown bake's record (empty: none), `bakePresent` = the bake's plugin paths still on
 *  disk. Applies the saved choice and records the build for the panel. */
CellModApplied cellModPickApply( const QString & ws, const QString & plugins, const QString & lodb,
	const QStringList & bakePresent );

//! The view's note for the document `doc` (a NifModel), set at build; empty = none.
QString cellModPickViewNote( const void * doc );
void cellModPickSetViewNote( const void * doc, const QString & note );

class NifSkope;
//! WW_MODPICK_TEST=<report>: the in-process GUI test (cellmodpicktest.cpp).
void wwCellModPickHarness( NifSkope * skope );

#endif // CELLMODPICK_H
