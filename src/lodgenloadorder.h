#ifndef LODGENLOADORDER_H
#define LODGENLOADORDER_H

/* His MO2 load order, read off disk with no Mod Organizer running (lane
 * LOADORDER1, 2026-09-24).
 *
 * bungo, 2026-09-24: "use my assets from my MO2 mod list". `--mo2` works only
 * inside MO2's virtual file system; from a plain shell the plugins live in
 * their mod folders, not in Data, and nothing in Data says which archive beats
 * which. This reads the two files MO2 itself keeps in a profile:
 *
 *   modlist.txt   one line a mod; the TOP line is the HIGHEST priority;
 *                 `+` enabled, `-` disabled, `*` an unmanaged DLC/CC entry
 *   plugins.txt   the `*` lines are the enabled plugins, in load order; it
 *                 never lists Fallout4.esm, the DLC masters or the CC files
 *
 * and builds:
 *
 *   the PLUGIN LIST   Fallout4.esm, the DLC masters present in Data in the
 *                     engine's fixed order, the CC files of Fallout4.ccc
 *                     present in Data in that file's order, then every `*`
 *                     plugin of plugins.txt, each as a FULL PATH found by
 *                     searching overwrite, then the ENABLED mod folders from
 *                     the highest priority down, then Data. A plugin found
 *                     nowhere is a refusal that names it.
 *   the RESOURCE STACK  lowest first, last wins, as lodgenSetResources()
 *                     takes it: Data (its loose files and every archive in
 *                     it), then each enabled mod from the BOTTOM of modlist.txt
 *                     to the TOP (a folder entry is its loose tree and its own
 *                     archives, loose beating archives as the stack always
 *                     did), then MO2's overwrite folder, which MO2 puts above
 *                     every mod.
 *
 * A disabled mod contributes nothing: no plugin, no archive, no loose file.
 * Nothing here writes a byte to the MO2 folders. */

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

/*! The game's Data folder for the MO2 instance whose base directory is
 *  `instanceBase`: a portable instance's own ModOrganizer.ini, else the
 *  %LOCALAPPDATA%\ModOrganizer\<name>\ModOrganizer.ini whose base_directory is
 *  that folder. Empty when neither says. `from` names the ini read. */
QString lodgenMo2GameData( const QString & instanceBase, QString * from );

/*! Build the plugin list and the resource stack from a profile folder. Empty
 *  `modsDir` = <profile>/../../mods; empty `dataDir` = lodgenMo2GameData().
 *  False with `error` naming the refusal. */
bool lodgenLoadOrderFromMo2( const QString & profileDir, const QString & modsDir,
	const QString & dataDir, LodgenLoadOrder * out, QString * error );

/*! `--plugins-txt` keeping its masters (lane LOADORDER1): the masters present
 *  in `dataDir`, then `names` (plugins.txt order) resolved against `dataDir`,
 *  a name already among the masters skipped. A name not in `dataDir` is a
 *  refusal that names it and points at `--mo2-profile`. */
bool lodgenLoadOrderFromPluginsTxt( const QString & dataDir, const QStringList & names,
	QStringList * resolved, QString * error );

/*! The CLI's whole `--mo2-profile` step: build, print, hand back the stack
 *  (with `extraResources` stacked ABOVE his mods, the way --resource sits above
 *  --mo2's stack) and the comma list for `file`. False = refused, printed on
 *  `err`. */
bool lodgenApplyMo2Profile( const QString & profileDir, const QString & modsDir,
	const QString & dataRoot, const QStringList & extraResources, bool otherSource,
	QStringList * stack, QString * file, QTextStream & out, QTextStream & err );

#endif // LODGENLOADORDER_H
