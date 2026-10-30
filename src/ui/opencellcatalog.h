/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef OPENCELLCATALOG_H
#define OPENCELLCATALOG_H

// File > Open Cell...: the plugin side (no widgets), so a unit test can drive it without a window.

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

struct OpenCellWorld
{
	quint32 form = 0;
	QString edid;
	QString name;   //!< FULL; a localized plugin's from its string tables, empty when they are not found
};

struct OpenCellCell
{
	quint32 form = 0;       //!< the reader's form (what a spec line names)
	quint32 shownForm = 0;  //!< lane CELLSPAWN1: the load-order FormID as the game and the CK show it
	QString edid;
	QString name;           //!< the winning record's FULL, as the CK shows it
	int x = 0, y = 0;
	int owner = -1;         //!< lane CELLSPAWN1: the plugin that adds the cell, an index into OpenCellCatalog::plugins
	int lastEdit = -1;      //!< lane CELLSPAWN1: the last plugin that edits it (the winning record's file)
};

//! Everything the dialog lists, read in one pass over the load order.
struct OpenCellCatalog
{
	QVector<OpenCellWorld> worlds;
	QHash<quint32, QVector<OpenCellCell>> cells;    //!< exterior cells by worldspace form, sorted by (y, x)
	QVector<OpenCellCell> interiors;                //!< sorted by editor ID
	QStringList noStrings;                          //!< localized plugins whose string tables were not found
	QStringList plugins;                            //!< the read load order's file names (owner / lastEdit index it)
};

//! lane CELLSPAWN1: the game's FormID for a reader form: a full plugin's top byte counts full plugins only.
//! `light[i]` says whether file i of the load order is a light plugin.
quint32 openCellShownForm( quint32 form, int ownerFile, const QVector<bool> & light );

//! How a catalog read finds a localized plugin's names.
struct OpenCellReadOptions
{
	QString lang;      //!< string-table language ("en"); empty = openCellStringsLanguage()
	QString dataDir;   //!< searched after the plugin's own folder (an MO2 mod's plugin, the game's archives)
};

//! Where plugin files are found: MO2 mods first (highest priority first), then the Data folder.
struct OpenCellSources
{
	QString dataDir;
	QStringList modDirs;
	QStringList resources;   //!< lane OPENCELLFIX1: an MO2 profile's resource folders, lowest first (empty: Data only)
	QString resolve( const QString & plugin ) const;   //!< full path, or empty when no source has it
};

//! A plugin's TES4 header: its masters in order, and whether its strings are localized.
bool openCellReadHeader( const QString & path, QStringList * masters, bool * localized, QString * error );

//! `plugins` in load order with every missing master inserted before its first dependent.
QStringList openCellWithMasters( const QStringList & plugins, const OpenCellSources & src,
	QStringList * missing );

//! The plugin files in a Data folder in a default load order: masters (.esm/.esl) first, Fallout4.esm leading.
QStringList openCellDataPlugins( const QString & dataDir );

//! A plugins.txt: the active (`*`) entries in order. Files without any `*` list every entry as active.
QStringList openCellParsePluginsTxt( const QString & text );

//! An MO2 modlist.txt: the enabled mods, highest priority first (the file's own order).
QStringList openCellParseModlist( const QString & text );

/*! A load-order file: its active plugins and, for an MO2 profile, the mod folders plugins come from.
 *  lane OPENCELLFIX1: led by the masters the game loads whatever the file says (Fallout4.esm, the DLC, Fallout4.ccc's
 *  files present in Data), which neither the game's nor MO2's plugins.txt lists. An MO2 profile goes through the LOD
 *  panel's reader (lodgenLoadOrderFromMo2): its Data folder comes from ModOrganizer.ini when `src.dataDir` holds no
 *  Fallout4.esm, and `src.resources` gets its folder stack. WW_OPENCELL_RED=masters: the old import (no masters). */
bool openCellImportLoadOrder( const QString & pluginsTxt, QStringList & plugins, OpenCellSources & src,
	QString * error );

//! Read worldspaces, their exterior cells and the interior cells out of `paths` (full paths, load order).
bool openCellReadCatalog( const QStringList & paths, OpenCellCatalog & out, QString * error,
	const OpenCellReadOptions & opt = OpenCellReadOptions() );

//! A string table, id -> text.
using OpenCellStrings = QHash<quint32, QString>;

//! One .STRINGS (`lengthPrefixed` false) or .DLSTRINGS/.ILSTRINGS (true) file; false when malformed.
bool openCellParseStrings( const QByteArray & bytes, bool lengthPrefixed, OpenCellStrings & out );

//! A localized plugin's tables: `Strings/<plugin>_<lang>.STRINGS`, `.DLSTRINGS`, `.ILSTRINGS`, loose or inside a
//! `<plugin> - *.ba2`, in the plugin's folder and then in `dataDir`. Loose beats archive, as in the game.
//! `found` is false when no table was found. WW_OPENCELL_STRINGS=0 (off) and WW_OPENCELL_RED=strings read nothing.
OpenCellStrings openCellLoadStrings( const QString & pluginPath, const QString & lang, const QString & dataDir,
	bool * found = nullptr );

//! lane OPENCELLFIX2: the string-table language the game uses: `[General] sLanguage` of Fallout4.ini, Fallout4Prefs.ini
//! and Fallout4Custom.ini (the MO2 profile's set when `profileDir` has one, else My Games), default "en".
//! Red twin REDTWIN=OPENCELL=lang (REDRETIRE1, tests/opencell's red build only): the old reader (the saved box value).
QString openCellStringsLanguage( const QString & profileDir = QString() );

//! The `.wwcell` spec line, in the form cellSpecFromLine() parses.
QString openCellExteriorLine( const QStringList & paths, const QString & world, int x, int y, int n );
QString openCellInteriorLine( const QStringList & paths, const QString & cell );

//! A file name for the spec: `<world>_<x>_<y>_<n>.wwcell` or `interior_<cell>.wwcell`.
QString openCellFileName( const QString & world, int x, int y, int n );
QString openCellInteriorFileName( const QString & cell );

//! Write `line` as the file's only line. Same codec as cellSpecFromFile() reads with.
bool openCellWriteSpec( const QString & path, const QString & line, QString * error );

//! Recent Cells: newest first, at most openCellRecentMax entries, kept in QSettings.
constexpr int openCellRecentMax = 10;
QStringList openCellRecent();
void openCellRecentAdd( const QString & path );
//! The folder a new spec goes to: beside the newest recent cell, else the app data folder.
QString openCellSpecDir();

#endif // OPENCELLCATALOG_H
