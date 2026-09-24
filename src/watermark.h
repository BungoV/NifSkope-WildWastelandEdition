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

/*! Lane WATER5 (hook-up H2): WaterStroke carries a record's trailing bytes.
 *  watercurves.cpp and waterwindow.cpp compile with or without this. */
#define WATERMARK_STROKE_EXTRA 1

/*! Lane WATER6: the solver consumes the window's per-point weights, a
 *  ONE-POINT curve, and an imported raster layer as the authority where
 *  painted (scratchpad/water5_20260910/CHANGE_NEEDED.md C1-C3). */
#define WATERMARK_RASTER_AUTHORITY 1

#include "watercurves.h"

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
	/* Lane WATER4's three: DyePin (7) is one point with a colour and a strength,
	 * whose plume runs DOWNSTREAM along the solved flow; DyeMouth (9) is a
	 * one-point mark on a river saying "this river's water tints the body it
	 * drains into" (the source is then the river's own id, so the consumer
	 * takes the river's colour); DyeKnob (8) is the document's one dye knob,
	 * the half-distance in world units, in `width`, at most one per file. A
	 * DyePin record carries 4 more bytes (RGBA) after its points; every other
	 * kind is the 20 + 8n of spec 3.7. */
	enum Kind { Stroke = 0, Pin = 1, Barrier = 2, Merge = 3, SourcePin = 4, OutletPin = 5,
		ZeroFlow = 6, DyePin = 7, DyeKnob = 8, DyeMouth = 9 };
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
	quint8 colour[4] = { 0, 0, 0, 0 };   //!< DyePin only: RGBA of the dye
	QVector<WaterStrokePoint> pts;
	/*! Lane WATER5 (hook-up H2): the bytes of the record AFTER its points (and
	 *  after a DyePin's colour), kept verbatim through open and save.  A curve
	 *  (kind 0/1) carries one float a point here, the per-point speed weight;
	 *  a raster layer (kind 10) carries its payload (watercurves.h).  Empty for
	 *  every kind that has none, so an old file re-encodes to its own bytes. */
	QByteArray extra;

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
	double solveSeconds = 0.0; //!< wall time of the potential solves, all bodies
	int dyeBodies = 0;         //!< bodies that received a plume
	qint64 dyeTexels = 0;      //!< texels with a dye weight > 0
	double strokeAgreement = 1.0; //!< mean cosine between the strokes' tangents and the solved flow under them
	QString note;              //!< the sentence the panel shows
};

/*! THE SOLVER CORE (lane WATER4): potential flow on a texel mask.
 *
 *  bungo, 2026-09-10: *"Could this maybe use a bit of some simulation
 *  though?"* -- after seeing the harmonic fill's held discs (*"it's like
 *  overlapping circles more like"*).  On a body's mask, `div( k grad phi ) =
 *  S`: `k` the conductance (the water depth, so flux prefers deep water), `S`
 *  the sources (+) and sinks (-), NO-FLUX at every bank face because the
 *  five-point stencil only ever reaches a neighbour inside the mask, and the
 *  velocity `u = -grad phi` from the face fluxes.  Continuity is then a
 *  property, not a rule: a channel that halves its width doubles its speed.
 *
 *  Solved by Jacobi-preconditioned conjugate gradient on the COMPACTED wet
 *  set (the bounding box of the Charles is 202,752 texels; the river is
 *  25,114).  The pure-Neumann system is singular and consistent once the
 *  sources balance, which `solve` enforces by subtracting the mean; a
 *  `dirichlet` set (cells held at 0) makes it regular and is how a WINDOW cut
 *  out of a sea gets an open far field.  Every method reports the number
 *  the harness gates on: iterations, the relative residual, the divergence.
 *
 *  Dye rides on it: `dye()` is the steady advection-decay `u . grad c =
 *  -|u| c / L` solved EXACTLY in one pass by visiting cells in descending
 *  potential -- every upwind neighbour has a higher potential, so it is
 *  already known -- with `L` the HALF-distance (weight 1/2 at L, 1/8 at 3L)
 *  and the chord through a cell along the flow as the distance a cell costs.
 *  `trace()` is Pollock's semi-analytic streamline on the same face fluxes,
 *  which cannot leave through a bank because a wall face's velocity is 0.
 */
struct WaterFlowGrid
{
	int w = 0, h = 0;
	int n = 0;                         //!< wet cells
	std::vector<int> idx;              //!< w*h -> cell index, or -1
	std::vector<int> cx, cy;           //!< cell -> texel
	std::vector<int> fi, fj;           //!< faces: cell i | cell j, the first nE are east faces
	std::vector<double> kf;            //!< face conductance, the harmonic mean
	int nE = 0;
	std::vector<double> diag;
	std::vector<int> comp;             //!< cell -> connected piece (faces only)
	int nComp = 0;

	//! Build from a mask (1 = wet) and a conductance per texel (ignored where dry).
	void build( int width, int height, const std::vector<quint8> & wet,
		const std::vector<float> & k );
	/*! `A phi = b` with `b` > 0 a source.  `dirichlet` (may be empty) marks
	 *  cells held at 0.  False only when the system is empty. */
	bool solve( const std::vector<double> & b, const std::vector<quint8> & dirichlet,
		std::vector<double> & phi, int & iterations, double & residual,
		double tol = 1e-9, int cap = 20000 ) const;
	//! Flux on every face, positive from i to j.
	void faceFlux( const std::vector<double> & phi, std::vector<double> & F ) const;
	//! Cell velocity: each axis the mean of its two face velocities, a wall face 0.
	void velocity( const std::vector<double> & phi, std::vector<double> & ux,
		std::vector<double> & uy ) const;
	//! Net outflow per cell -- equals `b` where the solve converged.
	void divergence( const std::vector<double> & phi, std::vector<double> & div ) const;
	/*! The dye pass.  `held` >= 0 fixes a cell's weight (a source); < 0 is free.
	 *  `halfTexels` is L in texels.  `c` comes back in 0..1. */
	void dye( const std::vector<double> & phi, const std::vector<double> & held,
		double halfTexels, std::vector<double> & c ) const;
	/*! Pollock's streamline from texel-space (x, y); true when it enters a
	 *  `stop` cell (indexed like `idx`, over w*h) within `maxCells` crossings. */
	bool trace( const std::vector<double> & phi, double x, double y,
		const std::vector<quint8> & stop, int maxCells ) const;
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
	/*! P4's instrument: the mean direction of the flow plane AS IT SITS IN
	 *  THE FILE, over body `id`, in degrees, with the number of samples it
	 *  averaged. It reads the file's own flow plane at the file's own rate
	 *  through the body plane, so a re-derivation at a coarser rate is
	 *  measured the way a CONSUMER sees it and not the way the solver
	 *  remembers it -- the solve field lives at the body plane's rate and
	 *  does not move when the flow rate does.
	 *
	 *  `onlyMarked` counts only the samples that differ from the body's
	 *  AUTOMATIC word, which is the half a stroke actually moved. Over a
	 *  whole body the untouched majority pins the mean to that constant and
	 *  the number cannot move, which would make a gate written on it unable
	 *  to fail. */
	bool meanFileFlow( int id, bool onlyMarked, double & degrees, qint64 & samples,
		QString * error ) const;

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
	/*! bungo's dye: *"river flowing into an ocean and the river and the ocean
	 *  may have slightly different color"*.  A DyeMouth mark on `id` carries
	 *  its water past its mouth into the body it drains to, as a plume that
	 *  fades; `strength` 0..1 is the weight at the mouth. */
	void setBodyDyeMouth( int id, bool on, float strength );
	bool bodyDyeMouth( int id, float * strength = nullptr ) const;
	/*! The one dye knob: the half-distance in world units (weight 1/2 after
	 *  L, 1/8 after 3 L).  Stored as a DyeKnob mark when it is not the default. */
	double dyeHalfDistance() const;
	void setDyeHalfDistance( double worldUnits );
	static constexpr double kDyeHalfDistanceDefault = 8192.0;   //!< two cells
	//! True when the store carries any dye mark, i.e. when a dye plane is written.
	bool hasDye() const;
	/*! The dye word this document would write at a plane texel: bits 0..15 the
	 *  source (a body id, or 0x8000 | the dye pin's index), bits 16..23 the
	 *  weight.  0 where there is no dye. */
	quint32 dyeWordAt( int px, int py ) const;

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
	/*! A two-point segment entirely on DRY LAND, found in THIS file rather
	 *  than assumed -- the control for addStroke()'s refusal.  A worldspace
	 *  corner is not dry on the Commonwealth, it is open sea, so the point
	 *  has to be measured.  False when the file carries no dry texel with a
	 *  dry neighbour, which is itself worth saying out loud. */
	bool dryStroke( double & x0, double & y0, double & x1, double & y1 ) const;

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
	//! The dye plane, packed at the flow plane's rate, 4 bytes a sample.
	QByteArray packDyePlane( quint64 base, QString * error ) const;
	quint32 dyeWordOf( int px, int py, quint16 id ) const;
	//! One body's field: its window, mask, depth and constraints -> solved.
	bool solveBody( int id, WaterMarkSolve & st, QString * note );
	//! The plumes: every DyeMouth and DyePin, in the store's order.
	void solveDye( WaterMarkSolve & st );
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
	/*! Lane WATER6 (C3): the kind-10 raster layers, decoded ONCE from the
	 *  marks with lane WATER5's own decoder (never a second one) and asked
	 *  by flowWordOf BEFORE the solved field.  `rasterSrc` is the payload of
	 *  each cached layer: a read pass (sweep, packFlowPlane, flowWordAt)
	 *  compares it with the marks and rebuilds only when it differs, so a
	 *  layer that is added or removed between passes is seen and a fingerprint
	 *  cannot collide. */
	mutable QVector<WaterRasterLayer> rasterCache;
	mutable QVector<QByteArray> rasterSrc;
	void syncRasters() const;
	QHash<int, Field *> fields;      //!< body id -> solved field, empty until solve()
	Plane idPlane, flowPlane, shorePlane;
	Plane dyePlane;
	QByteArray originalTable;        //!< the table's bytes as read, for the repack gate
	/*! The body table as it was READ.  flowX, flowY, flowSource, confidence,
	 *  source, outlet and flag bit 1 are DERIVED from the strokes, so every
	 *  solve puts these back before deriving them again: without it they
	 *  accumulated and removing a stroke could not reproduce the file it
	 *  started from (gate P3, which failed by 1,021,405 bytes). */
	QVector<LodtWaterBody> tableAtOpen;
	mutable QFile * reader = nullptr;         //!< our own handle on the planes
	mutable QHash<quint64, QByteArray> idTiles;   //!< a tiny tile cache for picking
	mutable QList<quint64> idTileOrder;
	// the tail's own header fields, read at open
	quint64 oBody = 0, oName = 0, oId = 0, oFlow = 0, oShore = 0, oStroke = 0;
	quint64 oDye = 0;
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
