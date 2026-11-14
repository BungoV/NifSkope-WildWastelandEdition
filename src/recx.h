#ifndef WW_RECX_H
#define WW_RECX_H

/* Record extension files (.recx): extra data for a record that lives in a plugin, without editing the plugin.
 * The format is shared with the game-side readers (docs/RECX.md):
 *
 *   <root>/Plugins/<Provider>/<Category>/<EditorID or 8-digit local id>.recx
 *   { "entries": [ { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473", "groundFog": { ... } } ] }
 *
 * Provider = the mod that ships the data, by its plain name (no extension). The folder and file names are for people:
 * an entry claims a record by its record key (named after the kind: "weather" for Weathers), "Plugin.ext|0xLOCALID"
 * (the id inside its plugin, 8 hex digits) or a bare EditorID, and/or by "editorId": exact or a family wildcard
 * ("CommonwealthClear*"). Every provider's files are read.
 *
 * Merge, block by block: for one block ("groundFog") the claims rank form > exact EditorID > wildcard (the caller's own
 * derivation from the record comes after all of them); at the same rank the provider later in the merge order wins
 * that block. Merge order: a provider named like a loaded plugin (CORE -> CORE.esm/.esp/.esl) takes that plugin's
 * load-order place; a provider with no plugin (FO4CS) comes after all plugins, in name order. Every overridden claim is
 * reported; two claims at one rank from the same provider are a conflict (the first file by path, then the first entry,
 * wins). Files are re-read when they change.
 *
 * Roots (where Plugins/ is looked for): WW_RECX_ROOT alone (tests); else the folder of every loaded plugin, last loaded
 * first (a mod manager's mod folders), the game's Data, and the files this program ships (<exe>/Data, else
 * <exe>/../res/Data). The same Provider/Category/file under two roots is one file: the first root's (as a mod manager
 * would show it).
 */

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct WwRecxHit
{
	bool ok = false;
	QJsonObject block;	// the winning claim's block
	QJsonObject entry;	// the whole entry it came from
	QString file;		// its file
	QString provider;	// its provider folder
	QString rung;		// "form", "editorId", "wildcard"; empty without a match
	QStringList overridden;	// the losing claims, "<file> (<rung>)", winner first in the merge order's words
	bool conflict = false;	// a loser from the winner's own provider at the winner's rank
};

//! the roots, first wins (above); pluginFiles = the loaded plugins' full paths, in load order
QStringList wwRecxRoots( const QStringList & pluginFiles, const QString & gameData );

/*! lane WEATHERRECX1: the mod folders of the mod manager's profile, lowest first (MO2's resource stack). A mod with no
 *  plugin (Fusion Shaders - Assets) is a root when it holds Plugins/. Set once by the application (recxmodroots.cpp);
 *  unset (the offline gates) = only WW_RECX_MODS. */
using WwRecxModRootsFn = QStringList ( * )();
void wwRecxSetModRoots( WwRecxModRootsFn fn );

/* lane WEATHERRECX1: the graphics provider (the Fusion Shaders data mod, MO2 mods\Fusion Shaders - Assets): every
 * graphics .recx NifSkope writes goes under Plugins/<this>/; the old name "FO4CS" is still read for one release */
inline QString wwRecxProvider()
{
	return QStringLiteral( "Fusion Shaders - Assets" );
}
inline QString wwRecxOldProvider()
{
	return QStringLiteral( "FO4CS" );
}

/*! lane WEATHERRECX1: the Fusion Shaders data mod's folder (its Textures and Plugins), empty when there is none:
 *  WW_ASSETS_MOD=<dir>, else <MO2>/mods/<wwRecxProvider()> beside the profile NifSkope reads, enabled or not.
 *  Defined in recxmodroots.cpp (the application only; the offline gates never call it). */
QString wwAssetsModRoot();

/*! The winning `block` ("groundFog") for one record of `category` ("Weathers"), its record key `key` ("weather"):
 *  the plugin that defines the record (file name with extension), its id inside that plugin, its EditorID.
 *  pluginFiles (load order) set the providers' merge order. Re-scans the roots when a file changed (at most every
 *  half second); logs every override once on stderr. */
WwRecxHit wwRecxFind( const QStringList & roots, const QStringList & pluginFiles, const QString & category,
	const QString & key, const QString & plugin, quint32 localId, const QString & edid, const QString & block,
	bool caseFree = false, bool arrayBlock = false );
//! lane AIRZONE1: arrayBlock true = the block is a JSON list (the Cells category's "volumes": [...]); the hit's `block`
//! is then empty and the caller reads `entry[block]`. False (every other caller) = objects only, as before.
//! lane WX1: `block` spelled any case (the spec's rule); the exact spelling first (Qt keeps no file order for the rest)
QJsonValue wwRecxBlockValue( const QJsonObject & entry, const QString & block, bool caseFree );

/*! lane LIGHTS5: re-scan `category` (at most every half second) and return a count that moves whenever its files were
 *  re-read; `files` = how many there are. A caller that walks many records asks again only when it moved. */
quint64 wwRecxScan( const QStringList & roots, const QString & category, int * files = nullptr );

//! the files read and the parse errors of the last scan of `category`, one line each (the census)
QString wwRecxScanSummary( const QString & category );

/* lane WEATHERRECX2 (spec COMMON "override"): what an entry replaces of its record.
 *   absent or false: augment (the file adds to the record); true: the file is the whole record (it may stand alone);
 *   a list of function names (the category's list, spec): only those replace the record's, the rest augments.
 * A name not in `valid` is dropped with a line in `dropped` ("override "<n>" dropped (not-function: <valid>)"). */
struct WwRecxOverride
{
	bool set = false;	// the key is there (false counts: an explicit augment)
	bool all = false;	// true
	QStringList funcs;	// the list's valid names, in the spelling of `valid`
	QStringList dropped;
	//! the function (or a part of it: "clouds.layers" under "clouds") is replaced by the file
	bool covers( const QString & func ) const;
	//! "override", "override[fog,clouds.layers]" or "augment"
	QString word() const;
};
WwRecxOverride wwRecxOverride( const QJsonObject & entry, const QStringList & valid );

/* lane WEATHERRECX2 (spec COMMON "base"): an entry's "base" (a part file name or a list of them) merged under its own
 * keys. A name is looked for beside the file, then in <category folder>/Parts/<name>.recx. A part file is
 * { "version": "1.8", "part": { <blocks>, "base": ... } }; later parts win over earlier ones, the entry over all;
 * objects merge key by key. A cycle or a missing part is dropped with a line in `log`. Done for every entry on read. */
QJsonObject wwRecxExpandEntry( const QJsonObject & entry, const QString & filePath, const QString & catDir, QStringList & log );

#endif
