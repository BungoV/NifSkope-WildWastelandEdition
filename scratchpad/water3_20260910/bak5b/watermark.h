/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WATERMARK_H
#define WATERMARK_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

#include "lodtfile.h"

class QFile;

/*! @file watermark.h  Marking water direction by hand, on a `.lodl` version-3
 *  file, and re-deriving the planes from what was marked.
 *
 *  bungo, 2026-09-09: *"in nifskope, have the player mark the water direction
 *  in a smart way"*, *"lakes have no flow if they're not connected to rivers,
 *  then rivers end up at sea"*, *"different water colors for different bodies
 *  of water"* -- *"or at least an ID for them"*.
 *
 *  THE STROKES ARE THE SOURCE. The three planes in the file (body id, flow,
 *  shore) are DERIVED, and this class never edits a plane: it edits the stroke
 *  store and the body table, then re-derives. That is what makes a re-bake at
 *  another sample rate, or after a heightmap change, keep the user's work --
 *  the points are WORLD coordinates, not texels (spec_water.md 3.7).
 *
 *  A CONSEQUENCE WORTH KNOWING BEFORE READING THE CODE. With no strokes the
 *  flow plane is a pure function of the body-ID plane and the body table: the
 *  writer gives every texel of a body its body's mean direction, speed step 8,
 *  confidence 0. So re-deriving a file nobody has marked must reproduce the
 *  writer's own bytes exactly, and that is a GATE (`--water-mark-selftest`
 *  case "repack identity"), not an aspiration. It is also why the undo gate
 *  can be byte-identity rather than a tolerance.
 */

//! One point of a stroke, in WORLD units (not texels: see the file comment).
struct WaterStrokePoint
{
	float x = 0.0f;
	float y = 0.0f;
};

/*! One constraint the user drew. The wire layout is spec_water.md 3.7.
 *
 *  `kind` 0..3 are the spec's; 4, 5 and 6 are this lane's addition and are
 *  documented in the spec's 5.2 table. bungo asked for *"a source pin + outlet
 *  pin = a path"*, which is two ONE-POINT constraints that only mean anything
 *  as a pair, so they cannot be the spec's kind-1 pin (which carries its own
 *  direction); and *"lakes have no flow if they're not connected to rivers"* is
 *  kind 6, a one-point mark saying this body is still -- a STROKE and not a bit
 *  on the body record, because the store is the source and a lock that lived
 *  only in the table would be lost the next time the planes were re-derived
 *  from it. An older reader that does not know them skips them by stride,
 *  exactly as it skips a longer record. */
struct WaterStroke
{
	enum Kind { Stroke = 0, Pin = 1, Barrier = 2, Merge = 3, SourcePin = 4, OutletPin = 5,
		ZeroFlow = 6 };
	enum Flags {
		SetsSpeed     = 1u << 0,
		SetsDirection = 1u << 1,
		PinsBodyId    = 1u << 2,
		Disabled      = 1u << 3
	};
	quint16 body = 0;          //!< the body it was drawn on; 0 = resolve by position
	quint8 kind = Stroke;
	quint8 flags = SetsDirection;
	float speed = 0.0f;        //!< world units per second, when flags & SetsSpeed
	float width = 512.0f;      //!< world units, the influence radius
	QVector<WaterStrokePoint> pts;

	bool enabled() const { return !( flags & Disabled ); }
};

//! What one solve measured. Every field is printed by the panel and the harness.
struct WaterMarkSolve
{
	int strokes = 0;           //!< strokes that were applied
	int bodiesSolved = 0;      //!< bodies that carry at least one constraint
	int constrained = 0;       //!< plane texels held by a stroke
	int iterations = 0;        //!< the propagation's own iteration count
	double residual = 0.0;     //!< its last maximum change
	qint64 changedTexels = 0;  //!< flow words that differ from the automatic field
	qint64 outsideBody = 0;    //!< stroke points that fell outside their own body
	QString note;              //!< the sentence the panel shows
};

/*! An open `.lodl` version-3 file, its body table, its strokes, and the solve.
 *
 *  It does NOT hold a plane in memory. The body-ID plane is 75 MB at the
 *  Commonwealth's rate and the solve only ever needs the bounding box of a body
 *  somebody marked, so masks are pulled through `LodtFile` per body and the
 *  re-derivation walks the file tile by tile.
 */
class WaterMarkDoc
{
public:
	WaterMarkDoc();
	~WaterMarkDoc();

	/*! Open a version-3 file. Refuses BY NAME on a version 1 or 2 file, on one
	 *  with no body table, and on one whose stroke store it cannot decode --
	 *  never silently opens an empty document. */
	bool open( const QString & path, QString * error );
	bool isOpen() const { return opened; }
	QString path() const { return filePath; }
	//! The reader, for a caller that wants the header as it is ON DISK.
	const LodtFile * file() const { return lodl; }
	//! Samples per cell of the flow plane; see setFlowRate().
	int flowSamples() const { return int( flowRate ); }
	/*! Re-derive the flow plane at another rate. The strokes are WORLD
	 *  coordinates, so this is the property gate P4 tests: mark at 32, write at
	 *  8, and the same body still points the same way. Refuses a rate that does
	 *  not divide the file's own samples per cell. */
	bool setFlowRate( int samplesPerCell, QString * error );

	// ---- the body table ---------------------------------------------------
	int bodyCount() const { return table.size(); }
	//! Body `id` is 1-based, as the plane stores it. False outside 1..count.
	bool body( int id, LodtWaterBody & out ) const;
	QString bodyName( int id ) const;
	//! The WATR forms this file interned, plus the worldspace default, in order.
	QVector<quint32> waterForms() const;

	void setBodyName( int id, const QString & name );
	//! -1 = automatic (the classifier's own class); 0 sea, 1 river, 2 lake.
	void setBodyClass( int id, int cls );
	//! `on` false clears the override (alpha 0), which is what the file means by "none".
	void setBodyColour( int id, quint8 r, quint8 g, quint8 b, bool on );
	void setBodyForm( int id, quint32 form );
	/*! bungo's rule, as a per-body switch: a lake that is not connected to a
	 *  river has NO flow. Locked bodies solve to zero and ignore their form's
	 *  NAM0 -- which is the one arm that would otherwise give a still lake a
	 *  velocity, because vanilla stores that per FORM. */
	void setBodyLockZero( int id, bool on );
	bool bodyLockZero( int id ) const;

	// ---- geometry ---------------------------------------------------------
	//! World units per texel of the body-ID plane.
	double worldPerTexel() const;
	//! World bounds of the whole worldspace, in world units.
	void worldBounds( double & x0, double & y0, double & x1, double & y1 ) const;
	//! Body id at a world position; 0 = dry (or outside the worldspace).
	quint16 bodyAtWorld( double wx, double wy ) const;
	//! Plane texel of a world position (row 0 SOUTH, as everything in this file).
	void worldToTexel( double wx, double wy, int & px, int & py ) const;
	void texelToWorld( int px, int py, double & wx, double & wy ) const;

	// ---- the strokes ------------------------------------------------------
	const QVector<WaterStroke> & strokes() const { return marks; }
	/*! Add one stroke. Returns false WITH A SENTENCE when it does not name a
	 *  body -- a stroke on dry land is refused, not stored, because the store
	 *  is the source of the planes and a constraint that constrains nothing is
	 *  a silent no-op the next reader would have to explain. */
	bool addStroke( const WaterStroke & s, QString * message );
	void removeStroke( int index );
	void clearStrokes();
	//! Strokes whose `body` field names `id`.
	QVector<int> strokesOfBody( int id ) const;

	// ---- the solve and the save -------------------------------------------
	/*! Re-derive the flow field from the strokes. Nothing is written; the
	 *  result lives in this object until save(). */
	bool solve( WaterMarkSolve * out, QString * error );
	/*! Write the file: the same bytes up to the body table, then the table,
	 *  the name blob, the stroke store and the three planes re-packed. The
	 *  body-ID and shore planes are copied VERBATIM (rebased), because nothing
	 *  a marking tool does can move a body's shape -- barriers and merges do,
	 *  and those are refused until the classifier can be re-run. */
	bool save( QString * error );
	//! True when the document differs from what is on disk.
	bool isModified() const { return dirty; }

	/*! The flow word this document would write at a plane texel -- the solved
	 *  field where a body was marked, the automatic constant everywhere else.
	 *  This is the function the re-derivation and the harness both read, so
	 *  the picture and the file can never disagree. */
	quint16 flowWordAt( int px, int py ) const;

	//! One line per body, for the panel's list.
	QString describeBody( int id ) const;

	/*! THE TWO IDENTITY GATES, exposed because a twin of the writer's code is
	 *  only safe while something proves the two agree.
	 *
	 *  `tableRepackMatches` re-encodes the body table and compares it to the
	 *  bytes read at open; `flowRepackMatches` re-derives the whole flow plane
	 *  and compares it to the plane the WRITER put in the file. On an unmarked
	 *  document both must be exact, and that is what makes "undo is
	 *  byte-identical" a property rather than a hope. */
	bool tableRepackMatches() const;
	bool flowRepackMatches( qint64 * differingBytes, QString * error ) const;

	/*! Walk every texel of the BODY plane once, tile by tile, handing back the
	 *  body id, the automatic flow word and this document's word. The harness's
	 *  isolation gate is this sweep -- a whole-plane measurement, not a claim
	 *  that only the marked body's bounding box could have moved. */
	bool sweep( const std::function<void( int px, int py, quint16 id,
		quint16 wordAuto, quint16 wordNow )> & cb, QString * error ) const;

private:
	struct Field;           //!< a solved body's own little grid
	struct Plane            //!< one tiled zlib plane container, as it sits on disk
	{
		int tilesX = 0, tilesY = 0, tileEdge = 0, bps = 0;
		quint64 headAt = 0, dirAt = 0, dataAt = 0, bytes = 0;
		QByteArray dir;
		bool ok = false;
	};
	bool readPlane( QFile & f, quint64 at, int bps, Plane & p, QString * error ) const;
	//! One tile, inflated; a uniform tile answers its repeated sample instead.
	bool tileOf( QFile & f, const Plane & p, int tx, int ty,
		QByteArray & raw, quint32 & uniform, bool & isUniform ) const;
	quint16 idAtTexel( int px, int py ) const;
	quint16 automaticWord( quint16 id ) const;
	//! flowWordAt() once the id is already known -- the sweep and the packer have it.
	quint16 flowWordOf( int px, int py, quint16 id ) const;
	//! The flow plane, re-derived, as the bytes that would sit at `base`.
	QByteArray packFlowPlane( quint64 base, QString * error ) const;
	//! Re-read the ZeroFlow marks into `lockZero` (the store is the source).
	void syncLocks();
	bool readTail( QString * error );
	bool decodeStrokes( const QByteArray & raw, QString * error );
	QByteArray encodeStrokes() const;
	QByteArray encodeTable() const;
	QByteArray encodeNames() const;
	bool copyRange( QFile & in, QFile & out, quint64 at, quint64 bytes, QString * error ) const;

	/*! Held by POINTER, and destroyed before the file is replaced: on Windows a
	 *  rename over a file somebody still has open fails, and the save path
	 *  renames the original aside. */
	LodtFile * lodl = nullptr;
	QString filePath;
	bool opened = false;
	bool dirty = false;
	QVector<LodtWaterBody> table;
	QVector<QString> names;
	QVector<quint8> lockZero;
	QVector<WaterStroke> marks;
	QHash<int, Field *> fields;      //!< body id -> solved field, empty until solve()
	Plane idPlane, flowPlane, shorePlane;
	QByteArray originalTable;        //!< the table's bytes as read, for the repack gate
	mutable QFile * reader = nullptr;         //!< our own handle on the planes
	mutable QHash<quint64, QByteArray> idTiles;   //!< a tiny tile cache for picking
	mutable QList<quint64> idTileOrder;
	// the tail's own header fields, read at open
	quint64 oBody = 0, oName = 0, oId = 0, oFlow = 0, oShore = 0, oStroke = 0;
	quint32 nameLen = 0, strokeLen = 0, bodyStride = 0;
	quint32 idRate = 0, flowRate = 0, shoreRate = 0, shoreQuantum = 32, flowEnc = 0;
	quint32 sect = 0;
};

/*! The tool's own known-answer control and its gates, printed as a report.
 *
 *  `lodl <file.lodl> --water-mark-selftest` runs it on a real file. It is the
 *  headless half of `WW_WATER_MARK_TEST`; the panel half is in
 *  watermarkpanel.cpp and counts the house-style rules.
 *
 *  The cases, each with the floor that stops an empty implementation passing:
 *    repack identity  a file nobody marked re-derives to the same bytes
 *    isolation        a stroke on body B changes ONLY body B's texels
 *    floor            and changes at least 60% of B's own
 *    refuter          the same stroke on a NEIGHBOUR leaves B alone (shown RED
 *                     for B first, so the isolation check is seen to be able
 *                     to fail)
 *    dry refusal      a stroke on dry land is refused in words and stored nowhere
 *    round trip       strokes written, read back, re-written: byte-identical
 *    undo             remove the stroke, re-derive: byte-identical to the start
 */
bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error );

#endif // WATERMARK_H
