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
	QString name;   //!< FULL; empty when the plugin is localized (FULL is then a string-table index)
};

struct OpenCellCell
{
	quint32 form = 0;
	QString edid;
	QString name;
	int x = 0, y = 0;
};

//! Everything the dialog lists, read in one pass over the load order.
struct OpenCellCatalog
{
	QVector<OpenCellWorld> worlds;
	QHash<quint32, QVector<OpenCellCell>> cells;    //!< exterior cells by worldspace form, sorted by (y, x)
	QVector<OpenCellCell> interiors;                //!< sorted by editor ID
};

//! Where plugin files are found: MO2 mods first (highest priority first), then the Data folder.
struct OpenCellSources
{
	QString dataDir;
	QStringList modDirs;
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

//! A load-order file: its active plugins and, for an MO2 profile, the mod folders plugins come from.
bool openCellImportLoadOrder( const QString & pluginsTxt, QStringList & plugins, OpenCellSources & src,
	QString * error );

//! Read worldspaces, their exterior cells and the interior cells out of `paths` (full paths, load order).
bool openCellReadCatalog( const QStringList & paths, OpenCellCatalog & out, QString * error );

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
