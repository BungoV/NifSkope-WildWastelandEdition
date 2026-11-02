#ifndef LODGENLOADORDER_H
#define LODGENLOADORDER_H

// His MO2 load order, read off disk with no Mod Organizer running.

#include <QString>
#include <QStringList>

class QTextStream;

struct LodgenLoadOrder
{
	QString profileDir;
	QString modsDir;
	QString dataDir;
	QString dataFrom;            //!< where dataDir came from: "--data-root" or the ModOrganizer.ini path
	QString overwriteDir;        //!< empty when the instance has none
	QStringList plugins;         //!< full paths, load order
	QStringList pluginFrom;      //!< parallel to plugins: "data", "overwrite" or the mod folder's name
	int masters = 0;             //!< how many of plugins came from Data implicitly (Fallout4.esm, DLC, CC)
	QStringList stack;           //!< resource stack, lowest priority first (last wins)
	QStringList stackFrom;       //!< parallel to stack: "data", "overwrite" or "mod <modlist line>"
	int modsEnabled = 0;
	int modsDisabled = 0;
	int separators = 0;
	QStringList modsMissing;     //!< `+` lines whose folder is not on disk (MO2 shows them missing too)
	QStringList archivesNoPlugin;//!< enabled mods' archives no enabled plugin names (the game would not load them)
};

/*! Fallout4.esm, the DLC masters and the Creation Club files, in the engine's
 *  order, as full paths, for the ones present in `dataDir`. Fallout4.esm first
 *  when present; the caller refuses a load order without it. */
QStringList lodgenLoadOrderMasters( const QString & dataDir );

//! The game's Data folder for the MO2 instance whose base directory is `instanceBase`: a portable instance's
//! own ModOrganizer.ini, else the %LOCALAPPDATA%\ModOrganizer\<name>\ModOrganizer.ini whose base_directory is
//! that folder.
QString lodgenMo2GameData( const QString & instanceBase, QString * from );

/*! Build the plugin list and the resource stack from a profile folder. Empty
 *  `modsDir` = <profile>/../../mods; empty `dataDir` = lodgenMo2GameData().
 *  False with `error` naming the refusal. */
bool lodgenLoadOrderFromMo2( const QString & profileDir, const QString & modsDir,
	const QString & dataDir, LodgenLoadOrder * out, QString * error );

//! `--plugins-txt` keeping its masters: the masters present in `dataDir`, then `names` (plugins.txt order)
//! resolved against `dataDir`, a name already among the masters skipped.
bool lodgenLoadOrderFromPluginsTxt( const QString & dataDir, const QStringList & names,
	QStringList * resolved, QString * error );

//! The CLI's whole `--mo2-profile` step: build, print, hand back the stack (with `extraResources` stacked ABOVE
//! his mods, the way --resource sits above mo2's stack) and the comma list for `file`.
bool lodgenApplyMo2Profile( const QString & profileDir, const QString & modsDir,
	const QString & dataRoot, const QStringList & extraResources, bool otherSource,
	QStringList * stack, QString * file, QTextStream & out, QTextStream & err );

#endif // LODGENLOADORDER_H
