#ifndef AIRZONE_H
#define AIRZONE_H

/* lane AIRZONE1 (bungo 10-29): HAND-PLACED VOLUMES, the files. "the work we did for volumes, may not be for naught ...
 * we still need some areas to have denser or different fog ... or different particles floating in the air ... we'll
 * transition that to hand placed volumes."
 *
 * One .recx file a cell (the shared standard, skill recx-record-extension-files; spec: RECX_SPEC_volumes):
 *   Data/Plugins/<Provider>/Cells/<EditorID or 8-digit local id>.recx
 *   { "entries": [ { "name": "...", "cell": "Plugin.ext|0x00XXXXXX", ["worldspace": "Plugin.ext|0x...", "grid": [x, y],]
 *                    "giMode": "rooms" | "all", "volumes": [ { shape, center, size, rotation, edge, priority,
 *                    "gi": {...}, "fog": {...}, "particles": {...} } ] } ] }
 * The entry's "cell" key decides the record (an exterior cell has a CELL record too; "worldspace" + "grid" are written
 * for people and checked). Read through recx.cpp (wwRecxFind, category Cells, key cell, block volumes as a list), so
 * the providers merge, the overrides are logged and the files reload when they change (500 ms).
 *
 * The math (shape, edge, priority, the fog and particle law, the GPU packing) is src/airzonemath.h; the GL side (the
 * row, the uniforms, the shader define) src/gl/cellairzone.h. This file is Qt Core only (the offline gate links it).
 *
 * GI (the `gi` block): the volumes replace today's GI shapes (lane PERF1's GiVolumeSet, the Space stage's or the room
 * grid stand-in) where a cell has them. giMode "rooms" (default): every room id the cell's volumes name drops today's
 * shapes of that room, every threshold pair drops today's thresholds of that pair; "all": every today's shape and
 * threshold whose centre lies in the cell is dropped. Kept shapes keep their places; a volume's `seq` puts it back at
 * its place (the converter writes today's order), so a converted cell gives the GI's list byte for byte. Applied when
 * the GI grid is built (cell open / reopen); fog and particles are live. */

#include "airzonemath.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

struct GiVolumeSet;

//! the CELL record a file entry claims
struct AirZoneCell
{
	bool interior = true;
	QString plugin;                 //!< the plugin that defines the CELL (file name with extension)
	quint32 localId = 0;            //!< its id inside that plugin
	QString edid;                   //!< its EditorID ("" none: most exterior cells)
	QString worldPlugin;            //!< exterior: the worldspace's plugin and id, and the grid
	quint32 worldLocalId = 0;
	QString worldEdid;
	int gx = 0, gy = 0;
	//! "Plugin.ext|0x00XXXXXX"
	QString key() const;
	//! the file name without .recx: the EditorID (unsafe characters as _), else the 8-digit local id
	QString fileStem() const;
	//! a person's name for it: "EDID" / "World 3,-2"
	QString label() const;
	bool sameRecord( const AirZoneCell & o ) const;
};

//! one cell's volumes as read
struct AirZoneCellVolumes
{
	AirZoneCell cell;
	std::vector<az::Volume> volumes;
	QString giMode = QStringLiteral( "rooms" );
	QString file, provider, rung;   //!< where they came from (empty: no file)
	QStringList warnings;           //!< refused or clamped values, one line each
	QStringList overridden;         //!< the losing providers' files (recx.cpp's merge)
	bool edited = false;            //!< the panel's unsaved copy stands in for the file
};

// ---- the format
//! one entry's volumes (and giMode); false + a warning when the entry holds none that can be read
bool airZoneParse( const QJsonObject & entry, std::vector<az::Volume> * out, QString * giMode, QStringList * warn );
//! one volume's JSON; numbers written so a float reads back bit for bit
QJsonObject airZoneVolumeJson( const az::Volume & v );
//! the entry for `cell`
QJsonObject airZoneEntryJson( const AirZoneCell & cell, const std::vector<az::Volume> & v, const QString & giMode );
/*! write `cell`'s entry into `path`: an existing file keeps its other entries and fields (the entry claiming the same
 *  record is replaced), a missing one is made with its folders. False + *err on failure. */
bool airZoneWriteEntry( const QString & path, const AirZoneCell & cell, const std::vector<az::Volume> & v,
	const QString & giMode, QString * err );
//! <dataRoot>/Plugins/<provider>/Cells/<stem>.recx
QString airZoneFilePath( const QString & dataRoot, const QString & provider, const AirZoneCell & cell );

/*! each cell's volumes through recx.cpp (category Cells, key cell, block volumes). red "novol" (WW_CELL_AIRZONE_RED):
 *  the files are ignored (every cell reads none). A cell with no entry is returned with no volumes. */
std::vector<AirZoneCellVolumes> airZoneLoad( const QStringList & roots, const QStringList & pluginFiles,
	const std::vector<AirZoneCell> & cells, const QString & red );

// ---- GI
//! merge the cells' gi volumes into today's set (rules above); "" and the set untouched when none has one
QString airZoneGiMerge( GiVolumeSet & set, const std::vector<AirZoneCellVolumes> & cells );
//! the converter: today's set as volumes of `cell` (an exterior cell: the shapes whose centre lies in it), seq = place
std::vector<az::Volume> airZoneFromGi( const GiVolumeSet & set, const AirZoneCell & cell );
//! FNV-1a 64 over the set's numbers in the GI's packing order (probegi.cpp giVolumesInto): the byte-identity census
QString airZoneGiHash( const GiVolumeSet & set );

// ---- a cell document's zones (the view's state; thread-safe: the build publishes, the frame reads)
/*! the cell build: the cells this document shows (interior: one; exterior: the loaded block), the load order and the
 *  game's Data (recx roots), and `on` (the Air zones row at the build) */
void wwAirZoneCellsPublish( const void * nif, const std::vector<AirZoneCell> & cells, const QStringList & pluginFiles,
	const QString & dataDir );
//! the document's volumes now (the files re-read when they changed, the panel's unsaved copy in place); version moves
//! on every change. Null when the document published no cells.
std::shared_ptr<const std::vector<AirZoneCellVolumes>> wwAirZoneFor( const void * nif, quint64 * version = nullptr );
//! the panel's working copy of cell `index` (live in the fog and particles; `clear` drops it: the file again)
void wwAirZoneEdit( const void * nif, int index, const std::vector<az::Volume> & v, const QString & giMode, bool clear = false );
//! save cell `index`'s working copy (or what is loaded) to its file, or a new one under dataRoot/Plugins/provider
bool wwAirZoneSave( const void * nif, int index, const QString & dataRoot, const QString & provider, QString * path, QString * err );
/*! the GI build (cellview.cpp, probesky.cpp relight): today's set is kept for the converter, then (on = the Air zones
 *  row) the cells' gi volumes merged in. Returns the notes line ("" when nothing was read; with WW_CELL_AIRZONE_GIHASH=1
 *  the hash of today's set). */
QString wwAirZoneGiApply( const void * nif, GiVolumeSet & set, bool on );
/*! the converter from the view: today's GI set (as the last build kept it) into each cell's file under
 *  dataRoot/Plugins/provider/Cells (other volumes in an existing file kept; its gi volumes replaced, giMode "all").
 *  Returns the files written; *err on a failure. */
QStringList wwAirZoneGiExport( const void * nif, const QString & dataRoot, const QString & provider, QString * err );
//! one census line for the notes / the panel
QString wwAirZoneCensus( const void * nif );
//! WW_CELL_AIRZONE_RED, the reader's tokens (novol: ignore the files; hardedge: the shader's weight 0/1)
QString wwAirZoneRed();

#endif // AIRZONE_H
