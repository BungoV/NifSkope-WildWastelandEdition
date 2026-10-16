/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLMODSOURCE_H
#define CELLMODSOURCE_H

#include <QString>
#include <QStringList>

/* Where the Mods list comes from when no far LOD bake is shown; first hit wins: the LOD panel's MO2 profile,
 * an MO2 instance found on this machine, the game's Data + plugins.txt, else just the masters in Data.
 * sTestFile1..10 in the game's ini replaces Fallout4.ccc and plugins.txt, as in the game. WW_MODPICK_NOMO2=1 hides MO2. */

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
