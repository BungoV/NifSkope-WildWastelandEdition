/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLMODSOURCE_H
#define CELLMODSOURCE_H

#include <QString>
#include <QStringList>

/* ---------------------------------------------------------------------------
 * lane MODPICK1 -- where the Mods list comes from when no far LOD bake is shown
 * (bungo 10-12: "what if I have no mo2 installed? needs a fallback"). First hit
 * wins:
 *
 *   1. "mo2"     the MO2 profile set in the LOD panel (LodGeneration/mo2Profile);
 *   2. "mo2"     an MO2 instance found on this machine: every
 *                %LOCALAPPDATA%/ModOrganizer/<instance>/ModOrganizer.ini, and a
 *                portable MO2 beside the game (a ModOrganizer.ini in the game
 *                folder, one folder under it, or one beside it). Only Fallout 4
 *                instances count; with several, the one whose gamePath is
 *                NifSkope's configured Fallout 4 folder wins. Its
 *                selected_profile, mod_directory and profiles_directory (with
 *                %BASE_DIR%) are read; nothing is written;
 *   3. "data"    no MO2 (a plain Steam/GOG install, or Vortex, which deploys
 *                into Data): the game's Data folder (NifSkope's Fallout 4 path,
 *                else the registry's "Installed Path"). Plugins = Fallout4.esm,
 *                the DLC masters in the engine's order, Fallout4.ccc, then the
 *                `*` lines of %LOCALAPPDATA%/Fallout4/plugins.txt. Archives as the
 *                game loads them: the INI lists (sResourceIndexFileList,
 *                sResourceStartUpArchiveList, sResourceArchiveList,
 *                sResourceArchiveList2; Fallout4.ini then Fallout4Custom.ini under
 *                Documents/My Games/Fallout4), then each plugin's own
 *                "<plugin> - *.ba2" in load order; loose files beat archives;
 *   4. "masters" Data found but no plugins.txt: Fallout4.esm and the DLC in Data,
 *                and `note` says where it looked. No Data at all: an empty list
 *                whose `note` names every place looked.
 *
 * Every step: the masters and Fallout4.ccc's files present in Data load
 * whatever plugins.txt says (MO2 forces them on too and never writes them to
 * plugins.txt), so they are the first `alwaysLoads` rows: ticked and locked.
 * The one exception, every step: sTestFile1..10 in [General] of the MO2
 * profile's own Fallout4.ini/Fallout4Custom.ini (when it has them), else of
 * Documents/My Games/Fallout4's. The game then loads neither Fallout4.ccc nor
 * plugins.txt: the list is the masters plus the test files, all locked, and
 * the label says "test files only".
 *
 * WW_MODPICK_NOMO2=1 hides MO2 (steps 1 and 2) for the no-MO2 gate.
 * WW_MODPICK_MYGAMES=<folder> stands in for Documents/My Games/Fallout4 (a gate fixture).
 * WW_MODPICK_RED=nofallback: steps 3 and 4 refuse (the old "no MO2 profile" error).
 * WW_MODPICK_RED=noccc: alwaysLoads 0 (the Creation Club files as ordinary rows).
 * WW_MODPICK_RED=notestfile: sTestFile ignored.
 * ------------------------------------------------------------------------- */

struct CellModSource
{
	QString kind;           //!< "mo2" | "data" | "masters" | "" (nothing found)
	QString label;          //!< for the list's header: "the MO2 profile Default", "the game's Data folder (...)"
	QString from;           //!< the profile folder or the Data folder
	QStringList plugins;    //!< full paths, load order
	QStringList pluginFrom; //!< parallel: the mod folder's name, "Data" or "overwrite"
	int alwaysLoads = 0;    //!< the first N plugins (masters + Fallout4.ccc in Data, or masters + sTestFile)
	                        //!< load whatever plugins.txt says, under MO2 too: locked rows
	QStringList stack;      //!< the resource stack, lowest first (last wins), as lodgenSetResources takes it
	QString note;           //!< what was skipped, or where it looked
	QString error;          //!< why the list is empty
};

//! Resolve the source (read only; cheap enough to call on every refresh).
CellModSource cellModSource();

/*! The game's resource order for a Data folder and a plugin list: [Data, the INI archive lists, each plugin's
 *  own "<plugin> - *.ba2" in plugin order] -- explicit archives above the folder, so they win in that order. */
QStringList cellModDataStack( const QString & dataDir, const QStringList & plugins );

//! The archives named for one plugin, sitting beside it: "<stem> - *.ba2" (and "<stem>.ba2"), by name.
QStringList cellModPluginArchives( const QString & pluginPath );

#endif // CELLMODSOURCE_H
