#ifndef WW_ESPWRITE_H
#define WW_ESPWRITE_H

/*
 * Lane ESPWRITE1 (2026-11-02): the plugin writer for cell edits. Pure C++17 + QtCore: no GUI, no GL, no EsmWorld.
 *
 * It turns an EspWrite::Plan (masters, light flag, author, per-cell edits) into the bytes of a Fallout 4 override
 * plugin and commits them with QSaveFile. Every rule it follows was measured on Fallout4.esm and the shipped CC .esl
 * files (notes/espwrite-r/DESIGN.md section 2; skill fo4-cell-override-write); layouts follow xEdit's published
 * definitions (wbDefinitionsFO4.pas):
 *   - interior: GRUP 'CELL' 0 > GRUP 2 (i32 obj%10) > GRUP 3 (i32 (obj/10)%10) > CELL + GRUP 6 > [8] [9];
 *   - exterior: GRUP 'WRLD' 0 > WRLD + GRUP 1 > [persistent CELL + 6 > 8] then GRUP 4 (i16 y>>5, i16 x>>5) >
 *     GRUP 5 (i16 y>>3, i16 x>>3) > grid CELL + 6 > 9; every exterior persistent ref under the persistent cell;
 *   - blocks / sub-blocks sort by their label read as a u32, cells by form id (Fallout4.esm, every group);
 *   - TES4 = HEDR (1.0, records + groups, next id) CNAM MAST DATA... [ONAM] INTV INCC; ONAM = the master REFRs
 *     overridden inside temporary groups; INCC = interior CELL records;
 *   - ESL: flag 0x200, own ids 0x800..0xFFF.
 *
 * The Python reference (tools/espwrite_ref.py) builds the same plan independently; tests/espwrite gates the two
 * byte for byte, walks the result, round-trips it through EsmWorld and re-emits three CC files' cell trees.
 *
 * Red WW_ESPWRITE_RED (read on every call, live) = xy | temp | count | onam | yaw | renumber: the old/wrong rule each gate
 * must refuse (exterior labels x first; persistent refs into group 9; HEDR count + 1; ONAM left out; yaw sign
 * flipped; ESPWRITE2: append renumbers the kept refs; ESPWRITE3: editkeep | editflag, see changeRef).
 */

#include <QByteArray>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace EspWrite
{

constexpr quint32 kRecCompressed = 0x00040000;
constexpr quint32 kTes4Localized = 0x00000080;
constexpr quint32 kTes4Light = 0x00000200;
constexpr quint32 kRefPersistent = 0x00000400;
constexpr quint32 kRefDisabled = 0x00000800;
constexpr quint32 kLightFirst = 0x800;
constexpr quint32 kLightLast = 0xFFF;
constexpr quint32 kBaseXMarker = 0x0000003B;
constexpr quint32 kBaseXMarkerHeading = 0x00000034;

//! WW_ESPWRITE_RED, read on every call (empty = no red)
QString red();

// ------------------------------------------------------------------ primitives
//! A subrecord: tag + u16 length + data; past 0xFFFF bytes an XXXX (u32 length) goes first and the u16 is 0.
QByteArray sub( const char * tag, const QByteArray & data );
QByteArray subU32( const char * tag, quint32 v );
QByteArray subZString( const char * tag, const QString & text );
//! The 8 bytes after a new record's form id: zero VCS, form version 131, zero.
QByteArray newRecordTail();
QByteArray record( const char * sig, quint32 flags, quint32 formID, const QByteArray & payload,
	const QByteArray & tail = newRecordTail() );
//! A GRUP: its size counts its own 24-byte header; the 8-byte stamp is written zero.
QByteArray group( quint32 label, quint32 type, const QByteArray & body );
quint32 label4( const char * fourcc );

struct Sub
{
	QByteArray tag;
	QByteArray data;
};
//! A raw record's payload, decompressed when the record carries 0x40000.
bool recordPayload( const QByteArray & rec, QByteArray & payload, QString * error );
//! The payload's subrecords (XXXX folded into the next one's length).
bool splitSubs( const QByteArray & payload, QVector<Sub> & out, QString * error );
QByteArray joinSubs( const QVector<Sub> & subs );
quint32 recordFlags( const QByteArray & rec );
quint32 recordForm( const QByteArray & rec );
QByteArray recordSig( const QByteArray & rec );
//! A copy of a raw record with header flag bits set / cleared (the payload is untouched, compressed or not).
QByteArray withFlags( const QByteArray & rec, quint32 set, quint32 clear = 0 );
/*! ESPWRITE3: an existing REFR / ACHR changed for an override. The payload is read decompressed and written plain;
 *  DATA takes `pos` / `rot` (radians) when given (nullptr keeps that half); `disable` sets 0x800; every other
 *  subrecord, the form id and the 8-byte header tail are kept. Reds: editkeep (DATA unchanged), editflag (no 0x800).
 *  ESPWRITE3B: `scale` sets XSCL (before XLOD / DATA; 1.0 removes it); red editscale leaves it. */
bool changeRef( const QByteArray & rec, const float * pos, const float * rot, const float * scale, bool disable,
	QByteArray & out, QString * error );
/*! ESPWRITE3B: a CELL with named XCLL fields set (cellLightFields(); colours as RRGGBB hex) and each field's Inherits
 *  bit cleared, so the cell's own value counts over its lighting template. A set LTMP adds a warning. Red
 *  xcllinherit leaves the Inherits bits. */
bool changeCellLight( const QByteArray & rec, const QVector<QPair<QString, QString>> & fields, QByteArray & out,
	QStringList * warnings, QString * error );
QStringList cellLightFields();
//! The value of one XCLL field (as changeCellLight takes it) and whether its Inherits bit is set (the readback).
bool cellLightRead( const QByteArray & xcll, const QString & field, QString & value, bool & inherited );
//! ESPWRITE3B: the refs a CELL's precombines (XCRI) and previs (XPRI) name, in the CELL's numbering.
bool precombinedRefs( const QByteArray & cellRec, QSet<quint32> & out, QString * error );

// ------------------------------------------------------------------ label rules
quint32 interiorBlockLabel( quint32 cellForm );
quint32 interiorSubLabel( quint32 cellForm );
quint32 exteriorBlockLabel( int x, int y );
quint32 exteriorSubLabel( int x, int y );
//! An exterior CELL's grid from XCLC.
bool cellGrid( const QByteArray & cellRec, int & x, int & y, QString * error );

// ------------------------------------------------------------------ pose
/*! The REFR rotation that makes a placement at `from` face `to`, as NifSkope WW draws a placement:
 *  fromEuler( -rx, -ry, -rz ) (niftypes.cpp, Rx Ry Rz), forward = +Y. Yaw only: rot = ( 0, 0, atan2( dx, dy ) ). */
void yawToward( const float from[3], const float to[3], float rot[3] );

// ------------------------------------------------------------------ tree (re-emit and the builder)
struct Node
{
	bool isGroup = false;
	quint32 type = 0;
	quint32 label = 0;
	QByteArray stamp;           //!< a parsed group's 8-byte stamp (written zero by emitNode)
	QByteArray raw;             //!< a record, verbatim
	QVector<Node> kids;
};
QByteArray emitNode( const Node & n, int * records = nullptr, int * groups = nullptr );
//! Parse [begin, end) of a plugin into nodes. Exact sizes or a named refusal.
bool parseNodes( const QByteArray & file, qsizetype begin, qsizetype end, QVector<Node> & out, QString * error );

// ------------------------------------------------------------------ a plugin's index (the copy source)
class PluginIndex
{
public:
	struct Group
	{
		quint32 type = 0, label = 0;
		qint64 offset = 0;
		int parent = -1;
	};
	bool open( const QString & path, QString * error );
	QString name() const { return fileName; }
	quint32 tes4Flags() const { return flags; }
	QStringList masters() const { return masterList; }
	bool has( quint32 formID ) const { return recs.contains( formID ); }
	QByteArray recordBytes( quint32 formID ) const;
	//! The groups that hold a record, innermost first (empty when absent).
	QVector<Group> chain( quint32 formID ) const;
	//! The CELL directly under world `world`'s type-1 group, 0 when none.
	quint32 persistentCell( quint32 world ) const { return pcells.value( world, 0 ); }
	const QByteArray & bytes() const { return data; }
	qint64 tes4End() const { return t4end; }

private:
	struct Rec
	{
		qint64 offset = 0;
		quint32 size = 0;
		int group = -1;
	};
	QString fileName;
	QByteArray data;
	quint32 flags = 0;
	qint64 t4end = 0;
	QStringList masterList;
	QVector<Group> groups;
	QHash<quint32, Rec> recs;
	QHash<quint32, quint32> pcells;
};

// ------------------------------------------------------------------ the plan
//! A copied record and where it came from (the plugin that WINS in the load order, plus its own masters).
struct Copied
{
	QByteArray raw;
	QString source;
	QStringList sourceMasters;
	bool sourceLocalized = false;
	QString full;               //!< the display name to inline when a localised FULL must leave (empty = drop it)
	bool ours = false;          //!< ESPWRITE2 append: already in the written file's numbering (no source check)
};

struct RefAdd
{
	quint32 base = 0;           //!< in the written file's numbering (high byte = master index)
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 }; //!< radians, as stored
	float scale = 1.0f;
	bool persistent = false;
	bool disabled = false;
	quint32 formOut = 0;        //!< filled by build()
	quint32 keepForm = 0;       //!< ESPWRITE2 append: the id this ref already has in the file (0 = a new id)
};

struct RefOverride
{
	Copied rec;                 //!< the winning REFR, already changed (e.g. withFlags( raw, kRefDisabled ))
	bool persistent = false;    //!< the group its original sits in (8 = true)
};

struct CellEdit
{
	quint32 world = 0;          //!< 0 = interior
	Copied cell;                //!< the CELL being edited (an exterior grid cell, or the interior)
	Copied worldRec;            //!< exterior: the WRLD
	Copied persistentCell;      //!< exterior: the world's persistent cell (needed when any ref is persistent)
	QVector<RefAdd> adds;
	QVector<RefOverride> overrides;
	/* ESPWRITE3B espmerge: written even with no refs (a carried CELL with no children, or the WRLD alone when `cell`
	 * is empty). A carried override (`ours`) may be any record a cell's children hold (NAVM, LAND, PGRE...). */
	bool keep = false;
};

struct Plan
{
	QStringList masters;        //!< base file names, load order
	bool light = false;
	QString author = QStringLiteral( "NifSkope WW" );
	QString target;             //!< the file name we write (refused when it is a master)
	quint32 nextFloor = 0;      //!< ESPWRITE2 append: the old file's HEDR next id; new ids never go below it
	QVector<CellEdit> cells;
	/* ESPWRITE3B espmerge: the extended file's other top groups, verbatim and in its order (CELL / WRLD go in at
	 * Fallout4.esm's place); its TES4 SNAM (before the masters) and SCRN / TNAM (after ONAM); its INTV. */
	QVector<Node> keepTop;
	QByteArray tes4Snam, tes4Tail;
	quint32 intv = 1;
};
//! ESPWRITE3B: Fallout4.esm's top-group order (first occurrence; notes/espwrite2/tmp/toporder.py)
QStringList topGroupOrder();

struct Report
{
	int records = 0, groups = 0;
	int cells = 0, interiorCells = 0, newRefs = 0, overrides = 0, fullDropped = 0, fullInlined = 0;
	quint32 nextId = 0;
	qint64 bytes = 0;
	QVector<quint32> onam;
	QString line( const QString & file ) const;
};

bool build( Plan & plan, QByteArray & out, Report * report, QString * error );
//! build() then an atomic QSaveFile commit.
bool write( const QString & path, Plan & plan, Report * report, QString * error );

} // namespace EspWrite

#endif
