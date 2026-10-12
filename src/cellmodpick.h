/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLMODPICK_H
#define CELLMODPICK_H

#include <QString>
#include <QStringList>
#include <QVector>

/* ---------------------------------------------------------------------------
 * lane MODPICK1 -- which mods the cell view loads (bungo 10-12: "choose which
 * mods load"; the loaded cells keep the far LOD bake's mods, FARLOD3).
 *
 * The LIST is the shown far LOD bake's plugins (its `.lodb`, in the bake's
 * order) when the FO4CS far LOD is on and its record was read, else his MO2
 * profile's plugins (QSettings LodGeneration/mo2Profile, the LOD panel's own
 * field). A plugin gone from disk is listed, greyed, and never loads.
 *
 * The CHOICE is saved per worldspace: QSettings CellView/ModPick/<ws> holds the
 * file names (lower case) he unticked. No key = the picker was never touched,
 * and the cell loads exactly what it loaded before this lane (byte for byte the
 * same plugin list). A touched choice loads the list's present plugins minus
 * the unticked ones, in the list's order.
 *
 * ONE STACK: the far LOD is a bake and cannot drop a mod, so when the near
 * cells' plugins differ from the bake's present plugins the view says so
 * ("Mods differ from the far LOD bake ...", one line over the view, and a
 * "mods:" line in the notes). Near and far never differ silently.
 *
 * WW_MODPICK_RED=inert: the saved choice is read but not applied (the gate's
 * red control -- unticking then changes nothing).
 * ------------------------------------------------------------------------- */

struct CellModRow
{
	QString path;           //!< full path
	QString file;           //!< file name as on disk
	QString mod;            //!< the mod folder it comes from ("Data" = the game's own folder)
	bool present = false;   //!< the file is on disk
	bool on = true;         //!< ticked: loads (a gone file is never on)
};

struct CellModList
{
	QString ws;
	QString source;         //!< "bake" | "mo2" | "" (nothing to list)
	QString from;           //!< the .lodb or the MO2 profile folder
	QVector<CellModRow> rows;
	QString error;          //!< why the list is empty
};

/*! The picker's list for worldspace `ws`, ticks from the saved choice (untouched: ticked = what the last
 *  build of `ws` loaded). `lodb` = the shown bake's record (empty: no bake shown, the MO2 profile). */
CellModList cellModPickList( const QString & ws, const QString & lodb );

//! The bake record the last cell build of `ws` showed (empty: none), and the plugins it loaded (comma list).
QString cellModPickShownBake( const QString & ws );
QString cellModPickLoaded( const QString & ws );

//! The saved choice. Touched = the key exists; `off` = unticked file names, lower case.
bool cellModPickTouched( const QString & ws );
QStringList cellModPickOff( const QString & ws );
void cellModPickSave( const QString & ws, const QStringList & off );
//! Forget the choice: the next build loads what it loaded before the picker was touched.
void cellModPickReset( const QString & ws );

struct CellModApplied
{
	QString plugins;        //!< what the near cells load (comma list)
	QStringList resources;  //!< the MO2 profile's resource stack when the choice came from it (else empty: unchanged)
	QString line;           //!< the notes' "mods: ..." line
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
//! WW_MODPICK_TEST=<report>: the in-process GUI gate (cellmodpicktest.cpp).
void wwCellModPickHarness( NifSkope * skope );

#endif // CELLMODPICK_H
